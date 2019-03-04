/**********************************************************
Name: GS_FILTR.CPP 

Module description: Filtro in sessione di lavoro su dati estratti,
                    trasferimento di valori da tabella secondaria,
                    filtro tematico, sensori
            
Author:  Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS

              
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
#include <math.h>
#include <ctype.h>       /*  per isdigit() */
#include <fcntl.h>
#include <string.h>      /*  per strcat() strcmp()  */

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>
#include "adsdlg.h"   
#include <dbents.h>
#include <dbpl.h>
#include "acedCmdNF.h" // per acedCommandS e acedCmdS

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_opcod.h"

#include "gs_resbf.h"     // gestione resbuf
#include "gs_utily.h" 
#include "gs_list.h"      // gestione liste C++ 
#include "gs_selst.h"     // gestione gruppi di selezione
#include "gs_ase.h" 
#include "gs_class.h" 
#include "gs_sec.h"
#include "gs_init.h" 
#include "gs_user.h"
#include "gs_graph.h" 
#include "gs_lisp.h"
#include "gs_area.h"      // per GS_CURRENT_WRK_SESSION
#include "gs_query.h"
#include "gs_evid.h"      // evidenziazione oggetti grafici
#include "gs_topo.h" 
#include "gs_dbref.h" 
#include "gs_filtr.h" 
#include "gs_thm.h" 

#include "d2hMap.h" // doc to help


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>
   #include <time.h>
#endif


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/

// operazioni della DCL "main_filter" e "main_filter4leg" e "SensorRulesBuilder"
#define LOCATION_SEL       DLGOK + 1
#define OBJ_SEL            DLGOK + 2
#define SENSOR_CLS_SEL     DLGOK + 3
#define DETECTED_CLS_SEL   DLGOK + 4
#define RANGE_SEL          DLGOK + 10                       
#define LEGEND_SEL         DLGOK + 11                    
#define PNT_LEG_SEL        DLGOK + 12                       
#define LOAD_SEL           DLGOK + 13     

// tipi di forme spaziali di selezione
#define WINDOW_DEF           DLGOK + 1
#define CIRCLE_SEL           DLGOK + 2
#define CIRCLE_DEF           DLGOK + 3
#define POLYGON_SEL          DLGOK + 4
#define POLYGON_DEF          DLGOK + 5
#define FENCE_SEL            DLGOK + 6
#define FENCE_DEF            DLGOK + 7
#define BUFFERFENCE_SEL      DLGOK + 8
#define BUFFERFENCE_DEF      DLGOK + 9
#define SHOW                 DLGOK + 10
#define FENCEBUTSTARTEND_SEL DLGOK + 11
#define FENCEBUTSTARTEND_DEF DLGOK + 12

// tools da utilizzare con il filtro o da sensori
enum FilterSensorToolEnum
{
   GSNoneFilterSensorTool        = 0,
   GSHighlightFilterSensorTool   = 1,
   GSExportFilterSensorTool      = 2,
   GSStatisticsFilterSensorTool  = 3,
   GSBrowseFilterSensorTool      = 4,
   GSDetectedEntUpdateSensorTool = 5,
   GSMultipleExportSensorTool    = 6
};

#define HIGHLIGHT       1
#define FILTER_EXPORT   2
#define STATISTICS      3
#define GRAPHBROWSE     4
#define DETECTED_UPDATE 5
#define SENSOR_EXPORT   6

static TCHAR *OPERATOR_LIST[] = {_T("="), _T(">"), _T("<"), _T("<>"), _T(">="), _T("<=")};
// vedi anche GS_SEC.CPP
static TCHAR *PARTIAL_AGGR_FUN_LIST[] = {_T("Avg"), _T("Count"), _T("Count *"), _T("Count distinct"), _T("Max"), _T("Min"), _T("Sum")};

static TCHAR *AGGR_FUN_LIST[] = {_T("Avg"), _T("Count"), _T("Count *"), _T("Count distinct"), _T("Max"), _T("Min"), _T("StDev"), _T("Sum"), _T("Var")};

// struttura usata per scambiare dati nella dcl "SpatialSel"
struct Common_Dcl_SpatialSel_Struct
{
   bool      Inverse;
   C_STRING  Boundary;
   C_STRING  SelType;
   C_RB_LIST CoordList;
};

// struttura usata per scambiare dati nella dcl "main_filter"
struct Common_Dcl_main_filter_Struct
{
   bool             LocationUse;
   C_STRING         SpatialSel;
   bool             LastFilterResultUse;
   int              Logical_op;
   int              Tools;
   C_CLASS_SQL_LIST sql_list;
   int              ClsSQLSel;
   int              SecSQLSel;
};

// struttura usata per scambiare dati nella dcl "SensorRulesBuilder"
struct Common_Dcl_main_sensor_Struct
{
   C_CLASS          *pSensorCls;    // Classe sensore
   C_LINK_SET       SensorLS;       // Link set dei sensori (oggetti grafici o lista di codici)
   C_RECT           SensorWindow;   // Finestra di selezione per griglie
   C_CLASS          *pDetectedCls;  // Classe da rilevare
   C_STRING         SpatialSel;     // Regole di rilevamento spaziale
   int              NearestSpatialSel; // Numero di entità più vicine al sensore (spazialmente)
   int              NearestTopoSel;    // Numero di entità più vicine al sensore (topologicamente)
   double           NearestTopoMaxCost; // Numero di entità più vicine al sensore (topologicamente)
   C_STRING         TopoRulesFile;     // File con le regole per la visita topologica
   C_RB_LIST        DetectionSQLRules; // regole di rilevamento SQL
   C_STRING         Aggr_Fun;      // Funzione di aggregazione SQL che genera i valori per aggiornare
                                   // le entità sensori (caso Tools=GSStatisticsFilterSensorTool) 
   C_STRING         Lisp_Fun;      // Funzione GEOLisp che genera i valori per aggiornare 
                                   // le entità rilevate (caso Tools=GSDetectedEntUpdateSensorTool) 
   C_STRING         DetectedAttrib; // Nome attributo da cui leggere i valori per le statistiche
                                    // (caso Tools=GSStatisticsFilterSensorTool)
                                    // Nome attributo in cui scrivere i valori delle entità rilevate
                                    // (caso Tools=GSDetectedEntUpdateSensorTool) 
   C_STRING         SensorAttrib;   // Nome attributo delle entità sensori da cui leggere valori
                                    // (caso Tools=GSStatisticsFilterSensorTool)
   C_STRING         SensorAttribTypeCalc; // Funzione di calcolo ("somma a", "sostituisci a", "sottrai a")
                                          // (caso Tools=GSStatisticsFilterSensorTool e 
                                          //  Tools=GSDetectedEntUpdateSensorTool)
   int              UseSensorSpatialRules; // Usa le regole di rilevazione spaziale
   int              UseNearestSpatialSel;  // Usa solo i più vicini al sensore (spazialmente)
   int              UseNearestTopoRules;   // Usa solo i più vicini al sensore (topologicamente)
   int              UseSensorSQLRules;     // Usa le regole di rilevazione SQL
   int              DuplicateDetectedObjs; // flag di ricerca di elementi già trovati da altri sensori
   int              Tools; // GSHighlightFilterSensorTool
                           // GSExportFilterSensorTool
                           // GSStatisticsFilterSensorTool
                           // GSBrowseFilterSensorTool
                           // GSDetectedEntUpdateSensorTool
                           // GSNoneFilterSensorTool
   C_CLASS_SQL_LIST sql_list;
};

// struttura usata per scambiare dati nella dcl "sql_bldqry"
struct Common_Dcl_sql_bldqry_Struct
{
   int       prj;
   int       cls;
   int       sub;
   int       sec;
   C_STRING  TableRef;
   C_DBCONNECTION *pConn;
   C_RB_LIST Structure;
   C_STRING  SQLCond;
   int       UseSQLMAPSyntax;
};

// struttura usata per scambiare dati nella dcl "sql_range_legend" 
struct Common_Dcl_legend_Struct
{
	int         dcl_file;         // identificativo del file di dcl
	int         current_line;     // linea corrente nella list-box dei range
	int         prj;					// codice progetto
	int         cls;					// codice classe
	int         sub;					// codice sottoclasse
	int         type_interv;	   // tipologia di intervallo
	int         n_interv;			// numero di intervalli impostati
	int         int_build;        // tipologia di costruzione intervalli 
	int         FlagLeg;          // flag che indica se creare la legenda oppure no
   int         ins_leg;          // modalità punto di inserimento legenda
	long        FlagFas;				// valore del flag di fas impostabile
	long        FlagThm;				// valore del flag del tematismo impostato
	C_STRING    thm;					// tematismo selezionato per l'evidenziazione
	C_STRING    attrib_name;      // attributo selezionato per il filtro
	C_STRING    attrib_type;      // tipologia attributo selezionato per il filtro
   C_STR_LIST  thm_list;         // lista tematismi modificabili classe scelta
	C_RB_LIST   current_values;	// valore corrente del range di impostazione
   C_RB_LIST   range_values;     // lista dei range del filtro
	C_RB_LIST   thm_sel[8];       // vettore di puntatori a C_RB_LIST per i range di ogni thm
	C_RB_LIST   Legend_param;		// parametri della legenda
   C_STRING    Boundary;
   C_STRING    SelType;
   C_RB_LIST   CoordList;
   bool        Inverse;
   int         AddObjs2SaveSet;
};

struct Common_Dcl_legend_values_Struct
{
	int         passo;			// passo ripetitivo nella lista intervalli
	int         posiz;         //	posizione del range di valori nella 
	                           // list-box degli intervalli
	int         prj;				// codice progetto
	int         cls;				// codice classe
	int         sub;				// codice sottoclasse
	C_STRING    attrib;        // attributo scelto dall'utente 
	C_STRING    attrib_type;   // tipologia attributo selezionato per il filtro
   C_RB_LIST   temp_values;   // lista dei range del filtro
};

// common struct per la funzione gsc_dd_sel_multipleVal (eventualmente da inserire in altro file)
struct Common_Val_Sel
{
	C_STR_LIST StrValList;
	C_STR_LIST SelValList;
};


//----------------------------------------------------------------------------//
// Classe C_QRY_SEC per la memorizzazione di istruzioni compilate per una tabella secondaria
// La classe contiene:
// 1) Codice tabella secondaria
// 2) Istruzione SQL compilata per lettura dati TEMP
// 3) Istruzione SQL compilata per lettura dati OLD
//----------------------------------------------------------------------------//
class C_QRY_SEC : public C_INT
{
   friend class C_QRY_SEC_LIST;

   protected :  
      enum SecondaryTableTypesEnum type; // tipo di secondaria
      C_STRING key_pri;      // per secondaria esterna
      int      operat;       // posizione nell'array OPERATOR_LIST
      resbuf   value;        // valore di riferimento (se numerico o stringa)

   public :
      CAsiSession *pTempSession;
      CAsiExecStm *pTempStm;
      CAsiCsr     *pTempCsr;
      CAsiData    *pTempParam;

      CAsiSession *pOldSession;
      CAsiExecStm *pOldStm;
      CAsiCsr     *pOldCsr;
      CAsiData    *pOldParam;

      C_PREPARED_CMD pCmdIsInTemp;

      C_QRY_SEC(void)
      {
         pTempSession  = pOldSession = NULL;
         pTempStm      = pOldStm     = NULL;
         pTempCsr      = pOldCsr     = NULL;
         pTempParam    = pOldParam   = NULL;
         operat        = -1;
         value.restype = RTNONE;
         return;
      }
      virtual ~C_QRY_SEC()
      {
         gsc_ASITermStm(&pTempStm); gsc_ASITermStm(&pOldStm);
         gsc_ASITermSession(&pTempSession); gsc_ASITermSession(&pOldSession);
         if (pTempCsr) delete pTempCsr; if (pOldCsr)  delete pOldCsr;
         if (pTempParam) delete pTempParam; if (pOldParam)  delete pOldParam;
         if (value.restype == RTSTR && value.resval.rstring)
            free(value.resval.rstring);
      }

      int    initialize(int cls, int sub, int sec, TCHAR *what = NULL, TCHAR *SQLWhere = NULL);
      int    get_sec()      { return get_key(); }
      int    get_operat()   { return operat; }
      resbuf *get_value()   { return &value; }
      int    get_type()     { return type; }
      TCHAR  *get_key_pri() { return key_pri.get_name(); }
};


class C_QRY_SEC_LIST : public C_INT_LIST
{
   public :
      C_QRY_SEC_LIST() : C_INT_LIST() {}
      virtual ~C_QRY_SEC_LIST() {} // chiama ~C_INT_LIST

      int initialize (int cls, int sub, C_SEC_SQL_LIST *pSecSQLList = NULL);
};


//----------------------------------------------------------------------------//
// Classe C_QRY_CLS per la memorizzazione di istruzioni compilate per una classe GEOsim
// La classe contiene:
// 1) Codice classe
// 2) Codice sotto-classe
// 3) Istruzione SQL compilata per lettura dati TEMP
// 4) Istruzione SQL compilata per lettura dati OLD
// 5) Istruzione SQL compilata per lettura relazioni TEMP da codice compl.
// 6) Istruzione SQL compilata per lettura relazioni OLD da codice compl.
//----------------------------------------------------------------------------//
class C_QRY_CLS : public C_INT_INT
{
   friend class C_QRY_CLS_LIST;

   protected :  
      C_QRY_SEC_LIST SecQryList;

   public :
      CAsiSession *pTempSession;
      CAsiExecStm *pTempStm;
      CAsiCsr     *pTempCsr;
      CAsiData    *pTempParam;

      CAsiSession *pOldSession;
      CAsiExecStm *pOldStm;
      CAsiCsr     *pOldCsr;
      CAsiData    *pOldParam;

      C_PREPARED_CMD pCmdIsInTemp;

      // inizializzati solo per gruppi
      C_PREPARED_CMD from_temprel_where_key;
      C_PREPARED_CMD from_oldrel_where_key;

      C_QRY_CLS()
      {
         pTempSession = pOldSession = NULL;
         pTempStm     = pOldStm     = NULL;
         pTempCsr     = pOldCsr     = NULL;
         pTempParam   = pOldParam   = NULL;
         return;
      }

      virtual ~C_QRY_CLS()
      {
         gsc_ASITermStm(&pTempStm); gsc_ASITermStm(&pOldStm);
         gsc_ASITermSession(&pTempSession); gsc_ASITermSession(&pOldSession);
         if (pTempCsr) delete pTempCsr; if (pOldCsr)  delete pOldCsr;
         if (pTempParam) delete pTempParam; if (pOldParam)  delete pOldParam;
      }

      int initialize(int cls, int sub, TCHAR *SQLWhere = NULL, C_SEC_SQL_LIST *pSecSQLList = NULL);
      int initializeGridRange(int cls, int sub, TCHAR *SQLWhere = NULL, C_SEC_SQL_LIST *pSecSQLList = NULL);

      int get_cls() { return get_key(); }
      int get_sub() { return get_type(); }
      C_QRY_SEC_LIST *ptr_qry_sec_list() { return &SecQryList; }
};


class C_QRY_CLS_LIST : public C_INT_INT_LIST
{
   public :
      C_QRY_CLS_LIST() : C_INT_INT_LIST() {}
      virtual ~C_QRY_CLS_LIST() {} // chiama ~C_INT_INT_LIST
      
      int initialize(C_CLASS_SQL_LIST &SQLCondList);
};


int gsc_setTileDclSpatialSel(ads_hdlg dcl_id, Common_Dcl_SpatialSel_Struct *CommonStruct);
int gsc_get_filtered_entities(int CounterToVideo);

// finestra "SpatialSel"
static void CALLB dcl_SpatialSel_Boundary(ads_callback_packet *dcl);
static void CALLB dcl_SpatialSel_SelType(ads_callback_packet *dcl);
static void CALLB dcl_SpatialSel_Define(ads_callback_packet *dcl);
static void CALLB dcl_SpatialSel_Select(ads_callback_packet *dcl);
static void CALLB dcl_SpatialSel_Show(ads_callback_packet *dcl);

int gsc_arc_point(ads_point pt1, ads_point pt2, ads_real bulge, 
                  int num_vertex, resbuf **out);

int gsc_selObjsWindow(C_RB_LIST &CoordList, int type, ads_name ss, int ExactMode = GS_BAD,
                      int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                      int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);
int gsc_selObjsCircle(C_RB_LIST &CoordList, int type, ads_name ss,
                      int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                      int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);
int gsc_selObjsPolygon(C_RB_LIST &CoordList, int type, ads_name ss, int ExactMode = GS_BAD,
                      int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                      int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);
int gsc_selObjsAll(ads_name ss, int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);

// finestra "main_filter"
static void CALLB dcl_main_filter_Logical_op(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_LocationUse(ads_callback_packet *dcl);
static void CALLB dcl_SpatialSel(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_LastFilterResultUse(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_ClsQryList(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_SecQryList(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_ClassQrySlid(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_SecQrySlid(ads_callback_packet *dcl);
static void CALLB dcl_main_filter_SecAggr(ads_callback_packet *dcl);
static int gsc_setTileDclmain_filter(ads_hdlg dcl_id, 
                                     Common_Dcl_main_filter_Struct *CommonStruct);
static int gsc_setTileClsQryListDclmain_filter(ads_hdlg dcl_id, 
                                               Common_Dcl_main_filter_Struct *CommonStruct,
                                               int OffSet = 0);
static int gsc_setTileSecQryListDclmain_filter(ads_hdlg dcl_id, 
                                               Common_Dcl_main_filter_Struct *CommonStruct,
                                               int OffSet = 0);

// finestra "sql_bldqry"
static void CALLB dcl_bldqry_operator(ads_callback_packet *dcl);
static void dcl_sql_bldqry_edit(ads_hdlg hdlg, const TCHAR *str);

static int gsc_SQLCond_Load(FILE *file, C_STRING &SQLCond, bool Unicode = false);
int gsc_splitSQLCond(C_STRING &SQLCond, C_STRING &LPName, C_STRING &Cond);
int gsc_splitSQLAggrCond(C_STRING &SQLAggrCond, C_STRING &AggrFun, C_STRING &AttribName,
                         C_STRING &Operator, C_STRING &Value);


// FUNZIONI DI GESTIONE GS_VERIFIED
int gsc_prepare_data_from_GS_VERIFIED(C_PREPARED_CMD &pCmd);
_RecordsetPtr gsc_prepare_data_from_GS_VERIFIED(void);
int gsc_prepare_data_from_GS_VERIFIED(_CommandPtr &pCmd);

int gsc_in_GS_VERIFIED(C_PREPARED_CMD &pCmd, int Cls, long Key,
                       _RecordsetPtr *pRs = NULL, int *IsRsCloseable = NULL);
int gsc_getClsFrom_GS_VERIFIED(int cls, _RecordsetPtr &pRs);
int gsc_canc_GS_VERIFIED(C_PREPARED_CMD &pCmd, int Cls, long Key);
int gsc_prepare_insert_GS_VERIFIED(_RecordsetPtr &pInsRs, C_RB_LIST &ColValues);
int gsc_correct_GS_VERIFIED(C_CLASS *pCls, C_CLASS_SQL_LIST &SQLCondList,
                            TCHAR *DestAttr = NULL, int CounterToVideo = GS_GOOD);
int gsc_gs_verified_to_LS(int cls, C_LINK_SET &ResultLS);
int gsc_DelAllGS_VERIFIED(void);
int gsc_DelSS_in_GS_VERIFIED(C_SELSET &ss);

int gsc_select_not_empty(_CommandPtr &pCmd, long key);

// FUNZIONI DI APPOGGIO ALL'ESECUZIONE DEL FILTRO
static int gsc_db_prepare_for_filter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLWhere,
                                     int CounterToVideo = GS_GOOD);
static int gsc_exe_SQL_filter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLWhere, C_LINK_SET *pLinkSet,
                              C_LINK_SET &ResultLS, TCHAR *DestAttr = NULL,
                              int CounterToVideo = GS_GOOD);
static int gsc_exe_SQL_SimplexFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                     C_SELSET *pSelSet, C_LINK_SET &ResultLS, TCHAR *DestAttr = NULL,
                                     int CounterToVideo = GS_GOOD);
static int gsc_exe_SQL_GroupFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                   C_LINK_SET *pLinkSet, C_LINK_SET &ResultLS, TCHAR *DestAttr = NULL,
                                   int CounterToVideo = GS_GOOD);
static int gsc_exe_SQL_GridFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                  C_LONG_BTREE *pKeyList, C_LINK_SET &ResultLS, TCHAR *DestAttr = NULL,
                                  int CounterToVideo = GS_GOOD);
static int gsc_exe_SQL_TabSecFilter(C_CLASS *pCls, C_LINK_SET &ResultLS,
                                    int CounterToVideo = GS_BAD);
static int gsc_exe_SQL_TabSecFilter(C_CLASS *pCls, C_LONG_LIST &IDList, int RemoveFromVERIF,
                                    int CounterToVideo = GS_BAD);
static int gsc_RefuseSQLAggr(presbuf ReadValue, int Operator, presbuf RefValue);
static int gsc_do_recovery_child(C_CLASS *pMother, C_CLASS *pChildClass, C_LINK_SET *pLinkSet, 
                                 C_CLASS_SQL_LIST &SQLWhere, int CounterToVideo = GS_BAD);
int gsc_get_data(C_QRY_CLS *pClsQry, long key, presbuf pData = NULL, const TCHAR *ColName = NULL, int *source = NULL);
int gsc_get_data(C_QRY_CLS *pClsQry, presbuf key, presbuf pData = NULL, const TCHAR *ColName = NULL, int *source = NULL);
int gsc_get_data(C_QRY_SEC *pSecQry, int cls, int sub, long key,
                 presbuf pData = NULL, int *source = NULL);
int gsc_get_data(C_QRY_SEC *pSecQry, int cls, int sub, presbuf key,
                 presbuf pData = NULL, int *source = NULL);
int gsc_get_alldata(C_QRY_SEC *pSecQry, int cls, int sub, long key, C_RB_LIST &ColValues, int *source = NULL);
int gsc_get_alldata(C_QRY_SEC *pSecQry, int cls, int sub, presbuf key, C_RB_LIST &ColValues, int *source = NULL);

// FUNZIONI DI APPOGGIO ALL'ESECUZIONE DEL TRASFERIMENTO DATI DA SECONDARIA
static int gsc_SecValueTransfer(C_CLASS *pCls, C_LINK_SET &ResultLS,
                                const TCHAR *DestAttr);
static int gsc_SecValueTransferGroup(C_CLASS *pCls, C_LONG_LIST &IDList,
                                     const TCHAR *DestAttr);

// FUNZIONI DEL FILTRO PER LEGENDA 
static void CALLB gs_can_dlg(ads_callback_packet *dcl);

static void CALLB gs_acp_larg(ads_callback_packet *dcl);
static void CALLB gs_radiolarg(ads_callback_packet *dcl);
static void CALLB gs_editlarg(ads_callback_packet *dcl);

static void CALLB gs_acp_htext(ads_callback_packet *dcl);
static void CALLB gs_radiohtext(ads_callback_packet *dcl);
static void CALLB gs_edithtext(ads_callback_packet *dcl);

static void CALLB gs_acp_scala(ads_callback_packet *dcl);
static void CALLB gs_radioscala(ads_callback_packet *dcl);
static void CALLB gs_editscala(ads_callback_packet *dcl);

static int gsc_sel_larg(double *current);
static int gsc_sel_htext(double *current);
static int gsc_sel_scala(double *current);
static int gsc_sel_color(C_STRING &value, C_COLOR *cur_val, ads_hdlg hdlg);
static int gsc_validvalue(presbuf p, TCHAR *attrib,int *num_msg);
static int gsc_scel_value(int prj, int cls, int sub, C_STRING &AttrName, 
								  TCHAR *attrib_type, C_STRING *value);
static int gsc_selpoint(ads_point *point);

static void CALLB dcl_main_filt4leg_Listattrib(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_SelThm(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Discret(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Continuos(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Auto(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Manual(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_SelVal(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_add_object2saveset(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Define(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Load(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Save(ads_callback_packet *dcl);
static void CALLB dcl_main_filt4leg_Help(ads_callback_packet *dcl);

static void CALLB dcl_ImpostRange_Listrange(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Add(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Ins(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Edit(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Del(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Slide(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Leg(ads_callback_packet *dcl);
static void CALLB dcl_ImpostRange_Help(ads_callback_packet *dcl);

static void CALLB dcl_ImpostValue_Editvalue(ads_callback_packet *dcl);
static void CALLB dcl_ImpostValue_Select(ads_callback_packet *dcl);
static void CALLB dcl_ImpostValue_Editrange(ads_callback_packet *dcl);
static void CALLB dcl_ImpostValue_Help(ads_callback_packet *dcl);

static void CALLB dcl_ImpostLegend_Create(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Onlayer(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Layer(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Ante(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Post(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_XPoint(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_YPoint(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Select(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_DimX(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_DimY(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Simboffset(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Dim(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Liststili(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Etichoffset(ads_callback_packet *dcl);
static void CALLB dcl_ImpostLegend_Help(ads_callback_packet *dcl);

// funzioni da inserire in altri files (?) 
int gsc_validcolor(C_STRING *value, int *valint, int flag = FALSE); // da inserire in gs_utily
int gsc_validcolor(C_STRING &value, C_COLOR &out, int flag = FALSE);
int gsc_evid4thm(C_CLASS *p_class, C_SELSET &selset, C_FAS *EvidFas, long flag_set, int k);

// funzioni da inserire in altri files (?) per la selezione multipla da 'lista valori'
int gsc_dd_sel_multipleVal(C_CLASS *pCls, C_STRING &AttrName, C_STR_LIST *result);
int gsc_dd_sel_multipleVal(C_STRING &Title, _RecordsetPtr &pRs, C_STR_LIST *result);

static int gsc_scel_multiple_value(int prj, int cls, int sub, C_STRING &AttrName, C_STR_LIST *values);
static void CALLB dcl_AttrMultipleValues_Help(ads_callback_packet *dcl);
static void CALLB dcl_AttrMultipleValues_accept(ads_callback_packet *dcl);
static void CALLB dcl_AttrMultipleValues_ValuesList(ads_callback_packet *dcl);

// funzioni per il filtro per legenda 
int gsc_ThemaFilterFromFile(TCHAR *path_file, int cls, int sub);
int gsc_LoadParamFromFile(Common_Dcl_legend_Struct *CommonStruct, TCHAR *path, C_STRING *SelSpace);
int gsc_ddLoadParam(Common_Dcl_legend_Struct *CommonStruct, ads_hdlg hdlg, C_STRING *SelSpace);
int gsc_ddSelectRange(Common_Dcl_legend_Struct *CommonStruct);
int gsc_ddLegendImpost(Common_Dcl_legend_Struct *CommonStruct);
int gsc_ReadOldRangeValues(C_RB_LIST *current_values, C_RB_LIST *temp_list, 
									C_CLASS *pCls, TCHAR *attrib, TCHAR *attrib_type,int passo,
									int posiz, ads_hdlg hdlg, TCHAR *dcl_tile);
int gsc_ModListRange(C_RB_LIST &current_values, C_RB_LIST *temp_list,
                     int passo, int posiz, TCHAR *attrib = NULL, int *num_msg = FALSE);
int gsc_InsListRange(C_RB_LIST &current_values, C_RB_LIST *temp_list, 
							int passo, int posiz, TCHAR *attrib = NULL, int *num_msg = FALSE);
int gsc_AddListRange(C_RB_LIST *current_values, C_RB_LIST *temp_list,
                     TCHAR *attrib = NULL, int *num_msg = FALSE);
int gsc_ImpostNewRangeValues(C_STR_LIST *lista, TCHAR *thm, TCHAR *attrib, TCHAR *tipo_attr,
									  C_RB_LIST *temp_list, ads_hdlg dcl_id, TCHAR *dcl_tile, C_CLASS *pCls);
int gsc_ImpostDefRangeValues(C_STR_LIST *sel_val, C_STR_LIST *lista, TCHAR *thm, 
									  TCHAR *attrib, C_RB_LIST *temp_list, C_CLASS *p_cls);
int gsc_RefreshValues(C_RB_LIST *temp_list, TCHAR *attrib, TCHAR *attrib_type,
							 C_CLASS *pCls, ads_hdlg dcl_id, TCHAR *dcl_tile);
int gsc_RefreshRanges(C_RB_LIST *current_values, C_CLASS *pCls, TCHAR *attrib, 
							 TCHAR *attrib_type,  int valori, ads_hdlg dcl_id, 
							 TCHAR *dcl_tile, int interv = TRUE, int shift = FALSE);
int gsc_SaveLegParam(C_RB_LIST &leg_param, int impleg, TCHAR *layer, ads_point ins_pnt, 
                     double dimx, double dimy, double simboffset, double dim, 
                     TCHAR *style, double etichoffset);
int gsc_filt4thm(Common_Dcl_legend_Struct *CommonStruct4Thm, 
					  Common_Dcl_main_filter_Struct *CommonStructCondition, C_LINK_SET *pLinkSet);
int gsc_build_legend(Common_Dcl_legend_Struct *CommonStruct4Leg);


// funzioni per il filtro a sensori
int gsc_SaveSensorConfig(Common_Dcl_main_sensor_Struct *pStruct, C_STRING &Path,
                         const TCHAR *Section);
int gsc_LoadSensorConfig(Common_Dcl_main_sensor_Struct *pStruct, C_STRING &Path,
                         const TCHAR *Section);
int gsc_getSpatialRules(presbuf DetectionSpatialRules, int *Mode, double *dimX, double *dimY);
int gsc_selClsObjsOnSpatialRules(C_LINK_SET &SingleSensorLS, 
                                 const TCHAR *Boundary, int Mode, bool Inverse,
                                 double dimX, double dimY, C_CLASS *pDetectedCls,
                                 C_LINK_SET &EntDetectedLS);
int gsc_selClsObjsOnTopoRules(long NodeSubSensorId,
                              C_TOPOLOGY &Topo,
                              int ClosestQty,
                              C_SUB *pNodeDetectedSub,
                              C_LINK_SET &EntDetectedLS);
int gsc_setStatisticsFilterSensor(C_CLASS *pSensorCls, long Key, C_SELSET &EntSS, 
                                  C_PREPARED_CMD *pSensorTempCmd, C_RB_LIST &ColValues,
                                  C_LINK_SET &DetectedLS, 
                                  C_STRING &TempTableRef,
                                  presbuf pRbDestAttribValue,
                                  const TCHAR *pAggrFun, 
                                  const TCHAR *pDestAttrib, const TCHAR *pDestAttribTypeCalc,
                                  bool DestAttribIsnumeric);
int gsc_sensorGrid(C_CGRID *pSensorCls, C_LINK_SET &SensorLS, C_CLASS *pDetectedCls,
                   presbuf DetectionSpatialRules = NULL, bool Inverse = false,
                   presbuf DetectionSQLRules = NULL, int NearestSpatialSel = 0,
                   C_CLASS_SQL_LIST *pDetectedSQLCondList = NULL, C_LINK_SET *pDetectedLS = NULL,
                   const TCHAR *pAggrFun = NULL, const TCHAR *pLispFun = NULL,
                   int Tools = NONE, const TCHAR *pDestAttrib = NULL, 
                   const TCHAR *pDestAttribTypeCalc = NULL, int DuplicateObjs = GS_BAD, 
                   C_LINK_SET *pRefused = NULL, int CounterToVideo = GS_BAD);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


static C_QRY_CLS_LIST QRY_CLS_LIST; // lista di istruzioni compilate
C_LINK_SET GS_LSFILTER;             // LinkSet ad uso esclusivo del filtro

// per selezione di un cerchio via ads_draggen
static C_RB_LIST LAST_POINTS, DRAGGEN_POINTS;
static ads_point CENTER;
static double    RAY;


//-----------------------------------------------------------------------//
//////////////////  C_QRY_SEC       INIZIO    /////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_QRY_SEC::initialize <internal> */
/*+                                                                       
  Inizializzazione dei membri della C_QRY_SEC.
  Parametri:
  int cls,      Codice classe
  int sub,      Codice sottoclasse
  int sec;      Codice tabella secondaria
  TCHAR *what;  cosa cercare (nil oppure "*" oppure "AVG(CAMPO)>5"...); default = NULL
  TCHAR *SQLWhere; condizione di filtro (WHERE...); default = NULL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non va se la tebella secondaria contiene un attributo che
  si chiama CLASS_ID o SUB_CL_ID o KEY_PRI o KEY_ATTRIB o STATUS.
-*/  
/*********************************************************/
int C_QRY_SEC::initialize (int cls, int sub, int sec, TCHAR *what, TCHAR *SQLWhere)
{
   C_SECONDARY    *pSec;
   C_STRING       SQLAggrCond, statement, StrValue, TableRef, UDL, UDLPrefix, KeyAttribCorrected;
   C_INFO_SEC     *pInfo;
   C_DBCONNECTION *pConn;
   C_ATTRIB       *pAttrib;
   _ParameterPtr  pParam;
   C_CLASS        *pCls;

   SQLAggrCond = _T("SEC.*");

   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), cls, sub, sec)) == NULL)
      return GS_BAD;
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL)
      return GS_BAD;

   UDLPrefix = _T("PRJ");
   UDLPrefix += pCls->ptr_id()->pPrj->get_key();
   UDLPrefix += _T("CLS");
   UDLPrefix += pCls->ptr_id()->code;
   UDLPrefix += _T("SUB");
   UDLPrefix += pCls->ptr_id()->sub_code;
   UDLPrefix += _T("SEC");
   UDLPrefix += pSec->code;

   pInfo = pSec->ptr_info();

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   if (what && wcslen(what) > 0)
   {
      C_STRING AggrFun, AttribName, Operator, Value;

      SQLAggrCond = what;
      
      if (gsc_splitSQLAggrCond(SQLAggrCond, AggrFun, AttribName, Operator, StrValue) == GS_GOOD)
      {  // SQLAggrCond definisce una funzione di aggregazione
         if (gsc_fullSQLAggrCond(AggrFun, AttribName, Operator, StrValue, TRUE, SQLAggrCond) == GS_BAD)
            return GS_BAD;
      }
      else // SQLAggrCond definisce un attributo e non una funzione di aggregazione
      {
         AttribName = SQLAggrCond;
         // Correggo eventualmente la sintassi del nome del campo con i doppi apici (") per MAP
         gsc_AdjSyntaxMAPFormat(SQLAggrCond);
      }

      if (Operator == _T("=")) operat = 0;
      else
      if (Operator == _T(">")) operat = 1;
      else
      if (Operator == _T("<")) operat = 2;
      else
      if (Operator == _T("<>")) operat = 3;
      else
      if (Operator == _T(">=")) operat = 4;
      else
      if (Operator == _T("<=")) operat = 5;
      else
          operat = -1;

      if (StrValue.len() > 0)
      {
         // se incomincia con : è considerato un parametro
         if (StrValue.get_name() && StrValue.get_name()[0] == _T(':') &&
             wcslen(StrValue.get_name()) > 1)
         {
            TCHAR    Msg[TILE_STR_LIMIT];
            int      Result;
            C_STRING dummy;

            swprintf(Msg, TILE_STR_LIMIT, gsc_msg(651), StrValue.get_name() + 1); // "Inserire valore parametro <%s>: "
            if ((Result = gsc_ddinput(Msg, StrValue)) != GS_GOOD) return Result;
         }

         if (gsc_is_numeric(StrValue.get_name()) == GS_GOOD)
         {
            value.restype      = RTREAL;
   	      value.resval.rreal = _wtof(StrValue.get_name());
         }
         else
         {
            C_ATTRIB *pAttrib;

            if ((pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(AttribName.get_name(), FALSE)) == NULL)
               return GS_BAD;

            if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

            value.restype = RTSTR;          
            value.resval.rstring = gsc_tostring(StrValue.get_name());
         }
      }
   }
   else
      operat = -1;

   // se si tratta di una secondaria di GEOsim
   if (pSec->get_type() == GSInternalSecondaryTable)
   {
      C_STRING LinkTableRef;

      type    = GSInternalSecondaryTable;
      key_pri = GS_EMPTYSTR;

      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(pCls->ptr_info()->key_attrib.get_name(), FALSE);

      if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
      if (pSec->getTempTableRef(TableRef, GS_BAD) == GS_BAD) return GS_BAD;
      if (pAttrib->init_TempADOType(pCls->ptr_info()->getDBConnection(OLD),
                                    pCls->ptr_info()->getDBConnection(TEMP)) == NULL)
         return GS_BAD;

      if (pConn->ExistTable(TableRef) == GS_GOOD)
      {
         // preparo istruzione per lettura da TEMP
         statement = _T("SELECT ");
         statement += SQLAggrCond;
         statement += _T(" FROM ");
         if (gsc_Table2MAPFormat(pConn, TableRef, UDL) == GS_BAD) return GS_BAD;
         statement += UDL;
         statement += _T(" SEC, ");

         if (pSec->getTempLnkTableRef(TableRef) == GS_BAD) return GS_BAD;
         if (pSec->create_tab_link(TRUE, TEMP) == GS_BAD) return GS_BAD;
         if (gsc_Table2MAPFormat(pConn, TableRef, UDL) == GS_BAD) return GS_BAD;
         statement += UDL;
         statement += _T(" LINK WHERE LINK.KEY_ATTRIB=SEC.");
         statement += KeyAttribCorrected; // nome attributo chiave con la sintassi già corretta
         statement += _T(" AND LINK.CLASS_ID=");
         statement += cls;
         statement += _T(" AND LINK.SUB_CL_ID=");
         statement += sub;
         statement += _T(" AND LINK.STATUS<>");
         statement += ERASED;
         statement += _T(" AND LINK.STATUS<>");
         statement += (ERASED | NOSAVE);
         statement += _T(" AND LINK.KEY_PRI=:VAL");
   
         if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
         {
            statement += _T(" AND (");
            statement += SQLWhere;
            statement += _T(")");
         }

         // Preparo se necessario il file UDL
         UDL = _T("PRJ");
         UDL += pCls->ptr_id()->pPrj->get_key();
         UDL += _T("CLS");
         UDL += pCls->ptr_id()->code;
         UDL += _T("SUB");
         UDL += pCls->ptr_id()->sub_code;
         UDL += _T("SEC");
         UDL += pSec->code;
         UDL += _T("TEMP");

         if (gsc_SetACADUDLFile(UDL.get_name(), pSec->ptr_info()->getDBConnection(TEMP),
                                TableRef.get_name()) == GS_BAD)
            return GS_BAD;
         if ((pTempSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
            return GS_BAD;
         if (gsc_ASIPrepareSql(pTempSession, statement.get_name(), UDL.get_name(),
                               &pTempStm, &pTempCsr, &pTempParam,
                               NULL, NULL, GS_BAD) == GS_BAD) // senza visualizzare msg di errore
         {
            // Purtroppo se la condizione di fltro ricade su campi di tipo date
            // la compilazione x ACCESS non funziona perchè access non ha il tipo date
            // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
            // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
            C_STRING AccessStm;

            AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(statement);
            AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);       

            if (gsc_ASIPrepareSql(pTempSession, statement.get_name(), UDL.get_name(),
                                  &pTempStm, &pTempCsr, &pTempParam) == GS_BAD)

               return GS_BAD;
         }

         // preparo istruzione per verifica che esistano secondarie in TEMP
         if (pSec->prepare_reldata_where_keyPri(pCmdIsInTemp, TEMP) == GS_BAD) return GS_BAD;
      }

      // preparo istruzione per lettura da OLD
      if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

      if (pAttrib->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == NULL)
         return GS_BAD;

      statement = _T("SELECT ");
      statement += SQLAggrCond;
      statement += _T(" FROM ");
      if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, UDL) == GS_BAD) return GS_BAD;
      statement += UDL;
      statement += _T(" SEC, ");

      if (pSec->getOldLnkTableRef(TableRef) == GS_BAD) return GS_BAD;
      if (gsc_Table2MAPFormat(pConn, TableRef, UDL) == GS_BAD) return GS_BAD;
      statement += UDL;
      statement += _T(" LINK WHERE LINK.KEY_ATTRIB=SEC.");
      statement += KeyAttribCorrected; // nome attributo chiave con la sintassi già corretta
      statement += _T(" AND LINK.CLASS_ID=");
      statement += cls;
      statement += _T(" AND LINK.SUB_CL_ID=");
      statement += sub;
      statement += _T(" AND LINK.KEY_PRI=:VAL");
   
      if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
      {
         statement += _T(" AND (");
         statement += SQLWhere;
         statement += _T(")");
      }

      // Preparo se necessario il file UDL
      UDL = UDLPrefix;
      UDL += _T("OLD");
      if (gsc_SetACADUDLFile(UDL.get_name(), pSec->ptr_info()->getDBConnection(OLD),
                             pSec->ptr_info()->OldTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      if ((pOldSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
         return GS_BAD;
      if (gsc_ASIPrepareSql(pOldSession, statement.get_name(), UDL.get_name(),
                            &pOldStm, &pOldCsr, &pOldParam) == GS_BAD)
         return GS_BAD;
   } // fine caso di secondaria GEOsim
   else
   { // se si tratta di secondaria esterna
      type = GSExternalSecondaryTable;
      key_pri = pInfo->key_pri;

      if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

      pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(pInfo->key_attrib.get_name(), FALSE);

      if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;

      // preparo istruzione per lettura
      statement = _T("SELECT ");
      statement += SQLAggrCond;
      statement += _T(" FROM ");

      if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, UDL) == GS_BAD) return GS_BAD;
      statement += UDL;
      statement += _T(" SEC WHERE ");      
      statement += KeyAttribCorrected; // nome attributo chiave con la sintassi già corretta
      statement += _T("=:VAL");
   
      if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
      {
         statement += _T(" AND (");
         statement += SQLWhere;
         statement += _T(")");
      }

      // Preparo se necessario il file UDL
      UDL = UDLPrefix;
      if (gsc_SetACADUDLFile(UDL.get_name(), pSec->ptr_info()->getDBConnection(OLD),
                             pSec->ptr_info()->OldTableRef.get_name()) == GS_BAD)
         return GS_BAD;
      if ((pOldSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
         return GS_BAD;
      if (gsc_ASIPrepareSql(pOldSession, statement.get_name(), UDL.get_name(),
                            &pOldStm, &pOldCsr, &pOldParam) == GS_BAD)
         return GS_BAD;
   }
   
   set_key(sec);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_QRY_SEC         FINE    /////////////////////////////
//////////////////  C_QRY_SEC_LIST    INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_QRY_SEC_LIST::initialize             <internal> */
/*+                                                                       
  Inizializzazione della C_QRY_SEC_LIST
  Parametri:
  int cls;                     codice classe
  int sub;                     codice sottoclasse
  C_SEC_SQL_LIST *pSecSQLList; Lista delle condizioni SQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_QRY_SEC_LIST::initialize(int cls, int sub, C_SEC_SQL_LIST *pSecSQLList)
{
   C_SEC_SQL *pSQLSecCond;
   C_QRY_SEC *pQryCSec;

   remove_all();
   if (!pSecSQLList) return GS_GOOD;

   pSQLSecCond = (C_SEC_SQL *) pSecSQLList->get_head();
   while (pSQLSecCond)
   {
      if ((pSQLSecCond->get_grp() && wcslen(pSQLSecCond->get_grp()) > 0) ||
          (pSQLSecCond->get_sql() && wcslen(pSQLSecCond->get_sql()) > 0))
      { // se ci sono delle condizioni impostate
         if ((pQryCSec = new C_QRY_SEC) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
         add_tail(pQryCSec);
         if (pQryCSec->initialize(cls, sub, pSQLSecCond->get_sec(),
                                  pSQLSecCond->get_grp(), pSQLSecCond->get_sql()) == GS_BAD) 
            { remove_all(); return GS_BAD; }
      }
      pSQLSecCond = (C_SEC_SQL *) pSQLSecCond->get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_QRY_SEC_LIST    FINE    /////////////////////////////
//////////////////  C_QRY_CLS         INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_QRY_CLS::initialize                  <internal> */
/*+                                                                       
  Costruttore della C_QRY_CLS
  Parametri:
  int cls;     codice classe
  int sub;     codice sottoclasse;
  TCHAR *SQLWhere; condizione di filtro (WHERE...); (default = NULL)
  C_SEC_SQL_LIST *pSecSQLList; Lista delle condizioni per le tabelle 
                               secondarie (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_QRY_CLS::initialize(int cls, int sub, TCHAR *SQLWhere, C_SEC_SQL_LIST *pSecSQLList)
{
   C_CLASS        *pCls;
   C_STRING       UDL, statement, Postfix, TempTableRef, Catalog, Schema, Table;
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   Postfix  = _T(" WHERE ");
   // Correggo la sintassi del nome del campo per SQL MAP
   UDL = pCls->ptr_info()->key_attrib;
   gsc_AdjSyntaxMAPFormat(UDL);
   Postfix += UDL;
   UDL.clear();
   Postfix += _T("=:VAL");
   if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
   {
      Postfix += _T(" AND (");
      Postfix += SQLWhere;
      Postfix += _T(")");
   }

   // preparo istruzione per lettura da OLD
   statement = _T("SELECT * FROM ");
   if (gsc_Table2MAPFormat(pCls->ptr_info()->getDBConnection(OLD),
                           pCls->ptr_info()->OldTableRef, UDL) == GS_BAD) return GS_BAD;
   statement += UDL;
   statement += Postfix;

	if (!pCls->ptr_info()) return GS_BAD;

   if (pCls->getLPNameOld(UDL) == GS_BAD) return GS_BAD;
   if (gsc_SetACADUDLFile(UDL.get_name(), pCls->ptr_info()->getDBConnection(OLD),
                          pCls->ptr_info()->OldTableRef.get_name()) == GS_BAD)
      return GS_BAD;

   if ((pOldSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
      return GS_BAD;
   if (gsc_ASIPrepareSql(pOldSession, statement.get_name(), UDL.get_name(),
                         &pOldStm, &pOldCsr, &pOldParam) == GS_BAD)
      return GS_BAD;

   // ricavo connessione OLE-DB per tabella TEMP
   if ((pConn = pCls->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (pCls->getTempTableRef(TempTableRef, GS_BAD) == GS_BAD) return GS_BAD; // senza creare la tabella
   // se esiste la tabella temporanea e i dati sono stati modificati
   if (pConn->ExistTable(TempTableRef) == GS_GOOD && 
       pCls->isDataModified() == GS_GOOD)
   {
      // preparo istruzione per lettura da TEMP
      statement = _T("SELECT * FROM ");
      if (gsc_Table2MAPFormat(pConn, TempTableRef, UDL) == GS_BAD) return GS_BAD;
      statement += UDL;
      statement += Postfix;

	   // se ha db e grafica
      if (pCls->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
		if (gsc_SetACADUDLFile(UDL.get_name(), pCls->ptr_info()->getDBConnection(TEMP),
									  TempTableRef.get_name()) == GS_BAD)
			return GS_BAD;

      if ((pTempSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
         return GS_BAD;
      if (gsc_ASIPrepareSql(pTempSession, statement.get_name(), UDL.get_name(),
                            &pTempStm, &pTempCsr, &pTempParam,
                            NULL, NULL, GS_BAD) == GS_BAD) // senza visualizzare msg di errore
      {
         // Purtroppo se la condizione di fltro ricade su campi di tipo date
         // la compilazione x ACCESS non funziona perchè access non ha il tipo date
         // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
         // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
         C_STRING AccessStm;

         AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(statement);
         AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);       
         if (gsc_ASIPrepareSql(pTempSession, AccessStm.get_name(), UDL.get_name(),
                               &pTempStm, &pTempCsr, &pTempParam) == GS_BAD)
            return GS_BAD;
      }

      // Istruzione per verificare la presenza nel temp
      if (pCls->prepare_data(pCmdIsInTemp, TEMP) == GS_BAD) return GS_BAD;
   }

   // se gruppo
   if (pCls->ptr_group_list())
      // preparo le istruzioni di lettura delle relazioni TEMP legate ad un gruppo
      if (pCls->prepare_reldata_where_key(from_temprel_where_key, TEMP) == GS_BAD ||
          pCls->prepare_reldata_where_key(from_oldrel_where_key, OLD) == GS_BAD)
         return GS_BAD;

   if (SecQryList.initialize(cls, sub, pSecSQLList) == GS_BAD) return GS_BAD;
   set_key(cls);
   set_type(sub);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_QRY_CLS::initializeGridRange         <internal> */
/*+                                                                       
  Costruttore della C_QRY_CLS per una classe griglia in cui si 
  devono cercare valori all'interno di un intervallo.
  Parametri:
  int cls;     codice classe
  int sub;     codice sottoclasse;
  TCHAR *SQLWhere; condizione di filtro (WHERE...); default = NULL
  C_SEC_SQL_LIST *pSecSQLList; Lista delle condizioni per le tabelle
                               secondarie (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_QRY_CLS::initializeGridRange(int cls, int sub, TCHAR *SQLWhere, C_SEC_SQL_LIST *pSecSQLList)
{
   C_CLASS        *pCls;
   C_STRING       UDL, statement, Postfix, TempTableRef, Catalog, Schema, Table, KeyAttribCorrected;
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   // ricavo connessione OLE-DB per tabella TEMP
   if ((pConn = pCls->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (pCls->getTempTableRef(TempTableRef, GS_BAD) == GS_BAD) return GS_BAD; // senza creare la tabella

   // preparo istruzione per lettura da TEMP
   statement = _T("SELECT * FROM ");
   if (gsc_Table2MAPFormat(pConn, TempTableRef, UDL) == GS_BAD) return GS_BAD;
   statement += UDL;

   statement += _T(" WHERE ");
   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pCls->ptr_info()->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);
   statement += KeyAttribCorrected;
   statement += _T(">=:VAL_MIN AND ");
   statement += KeyAttribCorrected;
   statement += _T("<=:VAL_MAX");
   if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
   {
      statement += _T(" AND (");
      statement += SQLWhere;
      statement += _T(")");
   }

	// se ha db e grafica
   if (pCls->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
	if (gsc_SetACADUDLFile(UDL.get_name(), pCls->ptr_info()->getDBConnection(TEMP),
                          TempTableRef.get_name()) == GS_BAD)
		return GS_BAD;

   if ((pTempSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
      return GS_BAD;
   if (gsc_ASIPrepareSql(pTempSession, statement.get_name(), UDL.get_name(),
                         &pTempStm, &pTempCsr, &pTempParam,
                         NULL, NULL, GS_BAD) == GS_BAD) // senza visualizzare msg di errore
   {
      // Purtroppo se la condizione di fltro ricade su campi di tipo date
      // la compilazione x ACCESS non funziona perchè access non ha il tipo date
      // ma solo timestamp (CAMPO = DATE '2013-03-06' non andrebbe).
      // Allora dove trovo DATE '...' lo sostituisco con TIMESTAMP '... 00:00:00'
      C_STRING AccessStm;

      AccessStm = gsc_AdjSyntaxDateToTimestampMAPFormat(statement);
      AccessStm = gsc_AdjSyntaxTimeToTimestampMAPFormat(AccessStm);       

      if (gsc_ASIPrepareSql(pTempSession, AccessStm.get_name(), UDL.get_name(),
                            &pTempStm, &pTempCsr, &pTempParam) == GS_BAD)
         return GS_BAD;
   }

   if (SecQryList.initialize(cls, sub, pSecSQLList) == GS_BAD) return GS_BAD;
   set_key(cls);
   set_type(sub);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_QRY_CLS         FINE    /////////////////////////////
//////////////////  C_QRY_CLS_LIST    INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_QRY_CLS_LIST::initialize             <internal> */
/*+                                                                       
  Inizializzazione della C_QRY_CLS_LIST
  Parametri:
  C_CLASS_SQL_LIST &SQLCondList; Lista delle condizioni SQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_QRY_CLS_LIST::initialize(C_CLASS_SQL_LIST &SQLCondList)
{
   C_CLASS_SQL *pSQLClsCond = (C_CLASS_SQL *) SQLCondList.get_head();
   C_QRY_CLS   *pQryCls;

   remove_all();
   while (pSQLClsCond)
   {
      if ((pQryCls = new C_QRY_CLS) == NULL) 
         { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
      add_tail(pQryCls);
      if (pQryCls->initialize(pSQLClsCond->get_cls(), pSQLClsCond->get_sub(),
                              pSQLClsCond->get_sql(), pSQLClsCond->ptr_sec_sql_list()) == GS_BAD) 
         { remove_all(); return GS_BAD; }

      pSQLClsCond = (C_CLASS_SQL *) pSQLClsCond->get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_QRY_CLS_LIST    FINE    /////////////////////////////
//////////////////  C_SEC_SQL         INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


C_SEC_SQL::C_SEC_SQL() {}
C_SEC_SQL::~C_SEC_SQL() {} // chiama ~C_2STR_INT

int   C_SEC_SQL::get_sec() { return get_type(); }
int   C_SEC_SQL::set_sec(int sec) { return set_type(sec); } 
TCHAR *C_SEC_SQL::get_grp() { return get_name(); }
int   C_SEC_SQL::set_grp(const TCHAR *grp) { return set_name(grp); } 
TCHAR *C_SEC_SQL::get_sql() { return get_name2(); }
int   C_SEC_SQL::set_sql(const TCHAR *sql) { return set_name2(sql); }


/*****************************************************************************/
/*.doc C_SEC_SQL::save                                                     */
/*+
  Questa funzione salva su file la condizione SQL della secondaria.
  Parametri:
  int cls;           codice classe
  int sub;           codice sotto-classe
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL::save(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_STRING Prefix, EntryName;

   Prefix = _T("PRJ");
   Prefix += GS_CURRENT_WRK_SESSION->get_PrjId();
   Prefix += _T("CLS");
   Prefix += cls;
   Prefix += _T("SUB");
   Prefix += sub;
   Prefix += _T("SEC");
   Prefix += get_sec();

   EntryName = Prefix;
   EntryName += _T("SQL");
   if (ProfileSections.set_entry(sez, EntryName.get_name(), get_sql()) != GS_GOOD)
      return GS_BAD;

   EntryName = Prefix;
   EntryName += _T("GRP");
   if (ProfileSections.set_entry(sez, EntryName.get_name(), get_grp()) != GS_GOOD)
      return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_SEC_SQL::load                                                       */
/*+
  Questa funzione carica da file la condizione SQL per la secondaria.
  Parametri:
  int cls;           codice classe
  int sub;           codice sotto-classe
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL::load(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   C_STRING           Prefix, EntryName;
   
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
      return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   Prefix = _T("PRJ");
   Prefix += GS_CURRENT_WRK_SESSION->get_PrjId();
   Prefix += _T("CLS");
   Prefix += cls;
   Prefix += _T("SUB");
   Prefix += sub;
   Prefix += _T("SEC");
   Prefix += get_sec();

   EntryName = Prefix;
   EntryName += _T("SQL");
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())))
      set_sql(pProfileEntry->get_name2());
   else
      set_sql(GS_EMPTYSTR);

   EntryName = Prefix;
   EntryName += _T("GRP");
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())))
      set_grp(pProfileEntry->get_name2());
   else
      set_grp(GS_EMPTYSTR);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_SEC_SQL::load                                                       */
/*+
  Apre tutti i riferimenti alla tabella secondaria (temporanea e old).
  Parametri:
  C_CLASS *pCls;  puntatore alla classe madre

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL::prepare_for_filter(C_CLASS *pCls)
{
   C_SECONDARY *pSec;
   C_STRING    CorrectedWhere, LPNName, MapTableRef;

   if ((pSec = (C_SECONDARY *) pCls->find_sec(get_sec())) == NULL) return GS_BAD;
   CorrectedWhere = get_sql();

   if (CorrectedWhere.len() > 0)
   {
      // Preparo se necessario il file UDL
      LPNName = _T("PRJ");
      LPNName += pCls->ptr_id()->pPrj->get_key();
      LPNName += _T("CLS");
      LPNName += pCls->ptr_id()->code;
      LPNName += _T("SUB");
      LPNName += pCls->ptr_id()->sub_code;
      LPNName += _T("SEC");
      LPNName += pSec->code;
      LPNName += _T("OLD");
      if (gsc_SetACADUDLFile(LPNName.get_name(), pSec->ptr_info()->getDBConnection(OLD),
                             pSec->ptr_info()->OldTableRef.get_name()) == GS_BAD)
         return GS_BAD;

      if (gsc_Table2MAPFormat(pSec->ptr_info()->getDBConnection(OLD),
                              pSec->ptr_info()->OldTableRef, MapTableRef) == GS_BAD)
         return GS_BAD;
      // richiede eventuali parametri tramite finestra
      if (gsc_ReadASIParams(LPNName.get_name(), MapTableRef.get_name(), 
                            CorrectedWhere, GSWindowInteraction) == GS_BAD)
         return GS_BAD;
      set_sql(CorrectedWhere.get_name());
   }
   else
      set_sql(GS_EMPTYSTR);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_SEC_SQL         FINE    /////////////////////////////
//////////////////  C_SEC_SQL_LIST    INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


C_SEC_SQL_LIST::C_SEC_SQL_LIST() {}
C_SEC_SQL_LIST::~C_SEC_SQL_LIST() {} // chiama ~C_2STR_INT_LIST


/*****************************************************************************/
/*.doc C_SEC_SQL_LIST::initialize                                          */
/*+
  Questa funzione inizializza una lista di tabelle secondarie che
  descrivono l'albero genealogico della classe madre passata come parametro.
  Parametri:
  int cls;     codice classe
  int sub;     codice sotto-classe

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL_LIST::initialize(int cls, int sub)
{
   C_SINTH_SEC_TAB_LIST SinthSecList;
   C_SINTH_SEC_TAB      *pSinthSec;
   C_SEC_SQL            *pSecSQL;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   remove_all();
   
   // leggo la lista delle tabelle secondarie
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->getSinthClsSecondaryTabList(cls, sub, SinthSecList) == GS_BAD) return GS_BAD;

   pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_head();
   while (pSinthSec)
	{
	   if ((pSecSQL = new C_SEC_SQL) == NULL)
         { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
	   add_tail(pSecSQL);
	   pSecSQL->set_sec(pSinthSec->get_key());
   	pSecSQL->set_grp(_T(""));
   	pSecSQL->set_sql(_T(""));

      pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_next();
	}

   return GS_GOOD;
}

/*****************************************************************************/
/*.doc C_SEC_SQL_LIST::copy                                                */
/*+
  Questa funzione copia la lista in un altro oggetto C_SEC_SQL_LIST
  Parametri:
  C_SEC_SQL_LIST &out;  lista risultato della copia

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL_LIST::copy(C_SEC_SQL_LIST &out)
{
   C_SEC_SQL *pSecSQL, *pSecSQLOut;

   out.remove_all();
   
	pSecSQL = (C_SEC_SQL *) get_head();
   while (pSecSQL)
   {
      if ((pSecSQLOut = new C_SEC_SQL()) == NULL)
         { out.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
	   out.add_tail(pSecSQLOut);
	   pSecSQLOut->set_sec(pSecSQL->get_sec());
	   pSecSQLOut->set_grp(pSecSQL->get_grp());
	   pSecSQLOut->set_sql(pSecSQL->get_sql());

      pSecSQL = (C_SEC_SQL *) get_next();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_SEC_SQL_LIST::from_rb                                             */
/*+
  Questa funzione inizializza una lista di condizioni SQL per le 
  tabelle secondarie tramite una lista di resbuf.
  Parametri:
  resbuf *list;   lista di resbuf descritta di seguito

  ((<sec> <aggr> <sql>) ...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL_LIST::from_rb(resbuf *list)
{
   presbuf   p_sec, p_rb;
   int       i = 0, sec;
   C_SEC_SQL *pSecSQL;
   C_STRING  aggr, sql;

   while ((p_sec = gsc_nth(i++, list)) != NULL)
   {
      // codice secondaria
      if ((p_rb = gsc_nth(0, p_sec)) == NULL || p_rb->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      sec = p_rb->resval.rint;
      // condizione di aggregazione sql
      if ((p_rb = gsc_nth(1, p_sec)) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      switch (p_rb->restype)
      {
         case RTSTR:
            aggr = p_rb->resval.rstring;
            break;
         case RTNONE: case RTNIL:
            aggr = GS_EMPTYSTR;
            break;
         default:
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      }
      // condizione sql
      if ((p_rb = gsc_nth(2, p_sec)) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      switch (p_rb->restype)
      {
         case RTSTR:
            sql = p_rb->resval.rstring;
            break;
         case RTNONE: case RTNIL:
            sql = GS_EMPTYSTR;
            break;
         default:
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      }

      if ((pSecSQL = (C_SEC_SQL *) search_type(sec)) == NULL) return GS_BAD;
   	pSecSQL->set_grp(aggr.get_name());
   	pSecSQL->set_sql(sql.get_name());
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_SEC_SQL_LIST::save                                                  */
/*+
  Questa funzione salva su file la lista delle condizioni SQL per le secondarie.
  Scrive una entry chiamata ID_SEC_SQL_LIST_CLS<cls>SUB<sub> = <sec1>,<sec2>.
  Parametri:
  int cls;           Codice classe
  int sub;           Codice sottoclasse
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL_LIST::save(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_SEC_SQL *pSecSQL;
   C_STRING  IDList, EntryName;
   bool      First = true;

   pSecSQL = (C_SEC_SQL *) get_head();
   while (pSecSQL)
   {
      if (pSecSQL->save(cls, sub, ProfileSections, sez) == GS_BAD) return GS_BAD;

      if (First) First = false;
      else IDList += _T(",");

      IDList += pSecSQL->get_sec();

      pSecSQL = (C_SEC_SQL *) get_next();
   }

   EntryName = _T("ID_SEC_SQL_LIST_CLS");
   EntryName += cls;
   EntryName += _T("SUB");
   EntryName += sub;

   return ProfileSections.set_entry(sez, EntryName.get_name(), IDList.get_name());
}


/*****************************************************************************/
/*.doc C_SEC_SQL_LIST::load                                                  */
/*+
  Questa funzione carica da file la lista delle condizioni SQL per le secondarie.
  Legge una entry chaimata ID_SEC_SQL_LIST_CLS_<cls>_SUB<sub> = <sec1>,<sec2>.
  Parametri:
  int cls;           Codice classe
  int sub;           Codice sottoclasse
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SEC_SQL_LIST::load(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_B2STR    *pProfileEntry;
   C_SEC_SQL  *pSecSQL;
   C_STRING   Buffer, EntryName;
   C_INT_LIST IDList;
   C_INT      *pID;

   remove_all();

   EntryName = _T("ID_SEC_SQL_LIST_CLS");
   EntryName += cls;
   EntryName += _T("SUB");
   EntryName += sub;
   if ((pProfileEntry = ProfileSections.get_entry(sez, EntryName.get_name())) == NULL) return GS_GOOD;
   if (IDList.from_str(pProfileEntry->get_name2()) != GS_GOOD) return GS_GOOD;

   pID = (C_INT *) IDList.get_head();
   while (pID)
   {
      if ((pSecSQL = new C_SEC_SQL()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }       
      pSecSQL->set_sec(pID->get_key());
      if (pSecSQL->load(cls, sub, ProfileSections, sez) == GS_BAD) { delete pSecSQL; return GS_BAD; }
      add_tail(pSecSQL);

      pID = (C_INT *) IDList.get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_SEC_SQL_LIST    FINE    /////////////////////////////
//////////////////  C_CLASS_SQL       INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


C_CLASS_SQL::C_CLASS_SQL() {}
C_CLASS_SQL::~C_CLASS_SQL() {}  //  chiama ~C_INT_INT_STR

int   C_CLASS_SQL::get_cls() { return get_key(); }
int   C_CLASS_SQL::set_cls(int cls) { return set_key(cls); } 
int   C_CLASS_SQL::get_sub() { return get_type(); }
int   C_CLASS_SQL::set_sub(int sub) { return set_type(sub); } 
TCHAR* C_CLASS_SQL::get_sql() { return get_name(); }
int   C_CLASS_SQL::set_sql(const TCHAR *sql) { return set_name(sql); }
C_SEC_SQL_LIST* C_CLASS_SQL::ptr_sec_sql_list() { return &sec_sql_list; }


/*****************************************************************************/
/*.doc C_CLASS_SQL::save                                                     */
/*+
  Questa funzione salva su file la condizione SQL per della classe.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL::save(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_STRING EntryName;

   EntryName = _T("PRJ");
   EntryName += GS_CURRENT_WRK_SESSION->get_PrjId();
   EntryName += _T("CLS");
   EntryName += get_cls();
   EntryName += _T("SUB");
   EntryName += get_sub();
   EntryName += _T("SQL");

   if (ProfileSections.set_entry(sez, EntryName.get_name(), get_sql()) != GS_GOOD)
      return GS_BAD;

   if (ptr_sec_sql_list()->save(get_cls(), get_sub(), ProfileSections, sez) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_CLASS_SQL::load                                                     */
/*+
  Questa funzione carica da file la condizione SQL per la classe.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_B2STR  *pProfileEntry;
   C_STRING EntryName, EntryValue;

   EntryName = _T("PRJ");
   EntryName += GS_CURRENT_WRK_SESSION->get_PrjId();
   EntryName += _T("CLS");
   EntryName += get_cls();
   EntryName += _T("SUB");
   EntryName += get_sub();
   EntryName += _T("SQL");
   if ((pProfileEntry = ProfileSections.get_entry(sez, EntryName.get_name())) == NULL) return GS_GOOD;
   set_sql(pProfileEntry->get_name2());

   if (ptr_sec_sql_list()->load(get_cls(), get_sub(), ProfileSections, sez) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_CLASS_SQL       FINE    /////////////////////////////
//////////////////  C_CLASS_SQL_LIST  INIZIO  /////////////////////////////
//-----------------------------------------------------------------------//


C_CLASS_SQL_LIST::C_CLASS_SQL_LIST() {}
C_CLASS_SQL_LIST::~C_CLASS_SQL_LIST() {} // chiama ~C_INT_INT_STR_LIST


/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::from_rb                                             */
/*+
  Questa funzione inizializza una lista di condizioni SQL per classi e 
  tabelle secondarie che descrivono l'albero genealogico della classe madre 
  passata come parametro.
  Parametri:
  resbuf *list;   lista di resbuf descritta di seguito

  ((<cls> <sub> <sql> [((<sec> <aggr> <sql>) ...)]) ...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL_LIST::from_rb(resbuf *list)
{
   presbuf        p_cls, p_rb;
   int            i = 0, cls, sub;
   C_CLASS_SQL    *pClsSQL;
   C_SEC_SQL_LIST *p_sec_sql_list;
   C_STRING       sql;

   while ((p_cls = gsc_nth(i++, list)) != NULL)
   {
      // codice classe
      if ((p_rb = gsc_nth(0, p_cls)) == NULL || p_rb->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      cls = p_rb->resval.rint;
      // codice sottoclasse
      if ((p_rb = gsc_nth(1, p_cls)) == NULL || p_rb->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      sub = p_rb->resval.rint;
      // condizione sql
      if ((p_rb = gsc_nth(2, p_cls)) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      switch (p_rb->restype)
      {
         case RTSTR:
            sql = p_rb->resval.rstring;
            break;
         case RTNONE: case RTNIL:
            sql = GS_EMPTYSTR;
            break;
         default:
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      }

      if ((pClsSQL = (C_CLASS_SQL *) search(cls, sub)) == NULL) return GS_BAD;
      pClsSQL->set_sql(sql.get_name());

      // lista condizioni sql per secondarie (opzionale)
      if ((p_rb = gsc_nth(3, p_cls)) != NULL)
      {
         p_sec_sql_list = (C_SEC_SQL_LIST *) pClsSQL->ptr_sec_sql_list();
         if (p_sec_sql_list->from_rb(p_rb) == GS_BAD) return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::hasCondition                                        */
/*+
  Questa funzione retituisce true se la lista contiene almeno una condizione SQL.
-*/  
/*****************************************************************************/
bool C_CLASS_SQL_LIST::hasCondition(void)
{
   C_CLASS_SQL *pClsSQL = (C_CLASS_SQL *) get_head();
   C_SEC_SQL   *pSecSQL;

   while (pClsSQL)
   {
      if (gsc_strlen(pClsSQL->get_sql()) > 0) return true;
      pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->get_head();
      while (pSecSQL)
      {
         if (gsc_strlen(pSecSQL->get_sql()) > 0) return true;
         pSecSQL = (C_SEC_SQL *) pSecSQL->get_next();
      }
      pClsSQL = (C_CLASS_SQL *) pClsSQL->get_next();
   }

   return false;
}


/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::initialize                                          */
/*+
  Questa funzione inizializza una lista di classi e tabelle secondarie che
  descrivono l'albero genealogico della classe madre passata come parametro.
  Parametri:
  C_CLASS *pMotherCls;  classe madre
  bool FromFile;        Flag, se = TRUE prova a cercare le condizioni SQL da file
                        (default = FALSE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL_LIST::initialize(C_CLASS *pMotherCls, bool FromFile)
{
   C_ID        *ptr_id = pMotherCls->ptr_id();
   C_CLASS_SQL *pClsSQL;
   C_INT_LIST  *group_list = NULL;
   C_INT       *punt;
   C_CLASS     *pCls;
   C_STRING    Path;
   C_PROFILE_SECTION_BTREE ProfileSections;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   remove_all();
	if ((pClsSQL = new C_CLASS_SQL()) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
	add_tail(pClsSQL);
	pClsSQL->set_cls(ptr_id->code);
	pClsSQL->set_sub(ptr_id->sub_code);
	pClsSQL->set_sql(GS_EMPTYSTR);

   if (FromFile)
   {
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;

      if (gsc_path_exist(Path) == GS_GOOD)
         if (gsc_read_profile(Path, ProfileSections) == GS_GOOD)
            // provo a cercare la condizione SQL da file
            pClsSQL->load(ProfileSections, _T("LAST_FILTERED_CLASSES"));
   }

   if (pClsSQL->sec_sql_list.initialize(ptr_id->code, ptr_id->sub_code) == GS_BAD)
      { remove_all(); return GS_BAD; }

   if (ptr_id->category == CAT_GROUP)
      group_list = (C_INT_LIST *) pMotherCls->ptr_group_list();
   
   if (group_list)
   {
 	   punt = (C_INT *) group_list->get_head();
	   while (punt)
	   {
		   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(punt->get_key(), 0)) == NULL)
            { remove_all(); return GS_BAD; }
         ptr_id = pCls->ptr_id();

	      if ((pClsSQL = new C_CLASS_SQL) == NULL)
            { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
	      add_tail(pClsSQL);
	      pClsSQL->set_cls(ptr_id->code);
	      pClsSQL->set_sub(ptr_id->sub_code);
      	pClsSQL->set_sql(GS_EMPTYSTR);

         if (FromFile) // provo a cercare la condizione SQl da file
            pClsSQL->load(ProfileSections, _T("LAST_FILTERED_CLASSES"));

         if (pClsSQL->sec_sql_list.initialize(ptr_id->code, ptr_id->sub_code) == GS_BAD)
            { remove_all(); return GS_BAD; }

	      punt = (C_INT *) punt->get_next();
		}
   }

   return GS_GOOD;
}

/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::copy                                                */
/*+
  Questa funzione copia la lista in un altro oggetto C_CLASS_SQL_LIST
  Parametri:
  C_CLASS_SQL_LIST &out;  lista risultato della copia

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL_LIST::copy(C_CLASS_SQL_LIST &out)
{
   C_CLASS_SQL *pClsSQL, *pClsSQLOut;

   out.remove_all();
   
	pClsSQL = (C_CLASS_SQL *) get_head();
   while (pClsSQL)
   {
      if ((pClsSQLOut = new C_CLASS_SQL()) == NULL)
         { out.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
	   out.add_tail(pClsSQLOut);
	   pClsSQLOut->set_cls(pClsSQL->get_cls());
	   pClsSQLOut->set_sub(pClsSQL->get_sub());
	   pClsSQLOut->set_sql(pClsSQL->get_sql());
      
      if (pClsSQL->sec_sql_list.copy(*(pClsSQLOut->ptr_sec_sql_list())) == GS_BAD) 
         { out.remove_all(); return GS_BAD; }

      pClsSQL = (C_CLASS_SQL *) get_next();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::save                                                */
/*+
  Questa funzione salva su file la lista delle condizioni SQL per le classi.
  Scrive una entry chiamata ID_LIST = <cls>,<sub> da interpretare a coppie di
  valori.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL_LIST::save(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_CLASS_SQL *pClsSQL;
   C_STRING    IDList;
   bool        First = true;

   pClsSQL = (C_CLASS_SQL *) get_head();
   while (pClsSQL)
   {
      if (pClsSQL->save(ProfileSections, sez) == GS_BAD) return GS_BAD;

      if (First) First = false;
      else IDList += _T(",");

      IDList += pClsSQL->get_cls();
      IDList += _T(',');
      IDList += pClsSQL->get_sub();

      pClsSQL = (C_CLASS_SQL *) get_next();
   }

   return ProfileSections.set_entry(sez, _T("ID_CLS_SQL_LIST"), IDList.get_name());
}


/*****************************************************************************/
/*.doc C_CLASS_SQL_LIST::load                                                */
/*+
  Questa funzione carica da file la lista delle condizioni SQL per le classi.
  Legge una entry chaimata ID_LIST = <cls>,<sub> da interpretare a coppie di
  valori.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo in memoria
  const TCHAR *sez;                         Nome della sezione nel file di profilo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS_SQL_LIST::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_CLASS_SQL    *pClsSQL;
   C_STRING       Buffer;
   C_INT_INT_LIST IDList;
   C_INT_INT      *pID;
   C_B2STR        *pProfileEntry;

   remove_all();
   if ((pProfileEntry = ProfileSections.get_entry(sez, _T("ID_CLS_SQL_LIST"))) == NULL) return GS_GOOD;
   if (IDList.from_str(pProfileEntry->get_name2()) != GS_GOOD) return GS_GOOD;

   pID = (C_INT_INT *) IDList.get_head();
   while (pID)
   {
      if ((pClsSQL = new C_CLASS_SQL()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }       
      pClsSQL->set_cls(pID->get_key());
      pClsSQL->set_sub(pID->get_type());
      if (pClsSQL->load(ProfileSections, sez) == GS_BAD) { delete pClsSQL; return GS_BAD; }
      add_tail(pClsSQL);

      pID = (C_INT_INT *) IDList.get_next();
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_CLASS_SQL_LIST  FINE    /////////////////////////////
//-----------------------------------------------------------------------//


/*****************************************************************************/
/*.doc gsc_trad_SelType                                                      */
/*+
  Questa comando effettua una traduzione del tipo di selezione. 
  Parametri:
  C_STRING &SelType;
  C_STRING &Trad;       risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_trad_SelType(C_STRING &SelType, C_STRING &Trad)
{
   // Traduzioni per SelType (case insensitive)
   if (gsc_strcmp(SelType.get_name(), _T("crossing"), FALSE) == 0)
   {
      Trad = gsc_msg(575); // "INTERSEZIONE"
   }
   else
   if (gsc_strcmp(SelType.get_name(), _T("inside"), FALSE) == 0)
   {
      Trad = gsc_msg(576); // "INTERNO"
   }
   else
      Trad = GS_EMPTYSTR;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_trad_boundary                                                     */
/*+
  Questa comando effettua una traduzione della forma spaziale di selezione. 
  Parametri:
  C_STRING &SelType;
  C_STRING &Trad;       risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_trad_Boundary(C_STRING &Boundary, C_STRING &Trad)
{
   // Traduzioni per SelType (case insensitive)
   if (gsc_strcmp(Boundary.get_name(), ALL_SPATIAL_COND, FALSE) == 0) // tutto
   {
      Trad = gsc_msg(580); // "TUTTO"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      Trad = gsc_msg(578); // "CERCHIO"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   {
      Trad = gsc_msg(577); // "POLIGONO"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), FENCE_SPATIAL_COND, FALSE) == 0)  // fence
   {
      Trad = gsc_msg(581); // "INTERCETTA"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
   {
      Trad = gsc_msg(582); // "INTERSEZIONE ESTREMI ESCLUSI"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      Trad = gsc_msg(443); // "BUFFER INTERCETTA"
   }
   else
   if (gsc_strcmp(Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0) // finestra
   {
      Trad = gsc_msg(579); // "FINESTRA"
   }
   else
      Trad = GS_EMPTYSTR;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setTileDclmain_filter                                             */
/*+
  Questa funzione setta tutti i controlli della DCL "main_filter" in modo 
  appropriato secondo il tipo di selezione.
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_main_filter_Struct *CommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileDclmain_filter(ads_hdlg dcl_id, 
                                     Common_Dcl_main_filter_Struct *CommonStruct)
{
   C_STRING    Boundary, SelType, LocSummary, Trad, JoinOp;
   C_RB_LIST   CoordList;
   C_CLASS_SQL *pClsSQL;
   C_CLASS     *pCls = NULL;
   bool        Inverse;

   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.get_head()))
   	pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub());

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(CommonStruct->SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }

   if (Inverse)
      LocSummary = gsc_msg(316); // "Inverso di: "

   // Traduzioni per SelType
   if (gsc_trad_SelType(SelType, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   LocSummary += _T(" ");
   // Traduzioni per Boundary
   if (gsc_trad_Boundary(Boundary, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   ads_set_tile(dcl_id, _T("Location_Descr"), LocSummary.get_name());

   // Location use
   if (CommonStruct->LocationUse)
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("1"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_ENABLE); 
   }
   else
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("0"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_DISABLE); 
   }

   if (pCls->get_category() != CAT_GRID)
   {
      // logical operator
      switch (CommonStruct->Logical_op)
      {
         case UNION:
            ads_set_tile(dcl_id, _T("Logical_op"), _T("Union"));
            break;
         case SUBTRACT_A_B:
            ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_Location_SSfilter"));
            break;
         case SUBTRACT_B_A:
            ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_SSfilter_Location"));
            break;
         case INTERSECT:
            ads_set_tile(dcl_id, _T("Logical_op"), _T("Intersect"));
            break;
      }

      // LastFilterResult use
      if (CommonStruct->LastFilterResultUse)
         ads_set_tile(dcl_id, _T("SSFilterUse"), _T("1"));
      else
         ads_set_tile(dcl_id, _T("SSFilterUse"), _T("0"));
   }
   else // Se griglia
   {
      ads_mode_tile(dcl_id, _T("Logical_op"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("SSFilterUse"), MODE_DISABLE); 
   }

   // controllo su operatori logici (abilitato solo se è stata scelta una area 
   // tramite "Location" ed è stato scelto anche l'uso di LastFilterResult)
   if (CommonStruct->LocationUse && Boundary.len() > 0 && CommonStruct->LastFilterResultUse)
      ads_mode_tile(dcl_id, _T("Logical_op"), MODE_ENABLE); 
   else
      ads_mode_tile(dcl_id, _T("Logical_op"), MODE_DISABLE); 

   // tools
   switch (CommonStruct->Tools)
   {
      case GSNoneFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("None"));
         break;
      case GSHighlightFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Highlight"));
         break;
      case GSExportFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Export"));
         break;
      case GSStatisticsFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Statistics"));
         break;
      case GSBrowseFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("GraphBrowse"));
         break;
      case GSDetectedEntUpdateSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("UpdDetected"));
         break;
      case GSMultipleExportSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("SensorExport"));
         break;
   }

   // Condizioni SQL Classi
   if (gsc_setTileClsQryListDclmain_filter(dcl_id, CommonStruct) == GS_BAD)
      return GS_BAD;

   // Condizioni SQL Secondarie
   if (gsc_setTileSecQryListDclmain_filter(dcl_id, CommonStruct) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setTileClsQryListDclmain_filter                                  */
/*+
  Questa funzione setta la lista il controllo "ClsQryList" nella finestra "main_filter"
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_main_filter_Struct *CommonStruct;
  int                           OffSet; (default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileClsQryListDclmain_filter(ads_hdlg dcl_id, 
                                               Common_Dcl_main_filter_Struct *CommonStruct,
                                               int OffSet)
{
   C_CLASS_SQL *pClsSQL;
   C_CLASS     *pCls;
   C_STRING    str, strCond;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   ads_start_list(dcl_id, _T("ClsQryList"), LIST_NEW, 0);
   pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.get_head();
   while (pClsSQL)
   {
   	if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub())) == NULL)
         return GS_BAD;
      str = pCls->ptr_id()->name;
      str += _T("\t:\t");

      strCond = pClsSQL->get_sql();
      if (strCond.len() >= (size_t) OffSet)
         str += (strCond.get_name() + OffSet);
      gsc_add_list(str);
      pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.get_next();
   }
   ads_end_list();
   
   str = CommonStruct->ClsSQLSel;
   ads_set_tile(dcl_id, _T("ClsQryList"), str.get_name());
   
   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setTileSecQryListDclmain_filter                                  */
/*+
  Questa funzione setta la lista il controllo "SecQryList" nella finestra "main_filter"
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_main_filter_Struct *CommonStruct;
  int                           OffSet; (default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileSecQryListDclmain_filter(ads_hdlg dcl_id, 
                                               Common_Dcl_main_filter_Struct *CommonStruct,
                                               int OffSet)
{
   C_SINTH_SEC_TAB_LIST SinthSecList;
   C_SINTH_SEC_TAB      *pSinthSec;
   C_CLASS              *pCls;
   C_SEC_SQL            *pSecSQL;
   C_CLASS_SQL          *pClsSQL;
   C_STRING             str, strCond;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.getptr_at(CommonStruct->ClsSQLSel + 1)) == NULL)
      return GS_BAD;

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub())) == NULL)
      return GS_BAD;

   // leggo la lista delle tabelle secondarie
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->getSinthClsSecondaryTabList(pClsSQL->get_cls(), pClsSQL->get_sub(), SinthSecList) == GS_BAD) return GS_BAD;

   ads_start_list(dcl_id, _T("SecQryList"), LIST_NEW, 0);

   pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_head();
   while (pSinthSec)
   {
      if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(pSinthSec->get_key())) != NULL)
      {
         str = pSinthSec->get_name();
         str += _T("\t:\t");
         strCond = pSecSQL->get_grp();
         for (size_t i = str.len()+ strCond.len(); i < 50; i++)
            strCond += _T(" ");

         strCond += pSecSQL->get_sql();
         if (strCond.len() >= (size_t) OffSet)
            str += (strCond.get_name() + OffSet);

         gsc_add_list(str);
      }

      pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_next();
   }
   ads_end_list();

   str = CommonStruct->SecSQLSel;
   ads_set_tile(dcl_id, _T("SecQryList"), str.get_name());

   if (pCls->get_category() == CAT_GRID)
   {
      ads_mode_tile(dcl_id, _T("SecSQL"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("SecAggr"), MODE_DISABLE); 
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "Location"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, LOCATION_SEL);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "Logical_op"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_Logical_op(ads_callback_packet *dcl)
{
   TCHAR val[TILE_STR_LIMIT];

   if (ads_get_tile(dcl->dialog, _T("Logical_op"), val, TILE_STR_LIMIT) == RTNORM)
   {
      // case insensitive
      if (gsc_strcmp(val, _T("Union")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Logical_op = UNION;
      else
      if (gsc_strcmp(val, _T("Subtr_Location_SSfilter")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Logical_op = SUBTRACT_A_B;
      else
      if (gsc_strcmp(val, _T("Subtr_SSfilter_Location")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Logical_op = SUBTRACT_B_A;
      else
      if (gsc_strcmp(val, _T("Intersect")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Logical_op = INTERSECT;
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "LastFilterResultUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_LastFilterResultUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("SSFilterUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->LastFilterResultUse = true;
   else
      ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->LastFilterResultUse = false;

   gsc_setTileDclmain_filter(dcl->dialog, (Common_Dcl_main_filter_Struct*)dcl->client_data);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "LocationUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_LocationUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("LocationUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->LocationUse = true;
   else
      ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->LocationUse = false;

   gsc_setTileDclmain_filter(dcl->dialog, (Common_Dcl_main_filter_Struct*)dcl->client_data);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "Tools"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_Tools(ads_callback_packet *dcl)
{
   TCHAR val[TILE_STR_LIMIT];

   if (ads_get_tile(dcl->dialog, _T("Tools"), val, TILE_STR_LIMIT) == RTNORM)
   {
      if (gsc_strcmp(val, _T("Highlight")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Tools = GSHighlightFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("Export")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Tools = GSExportFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("Statistics")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Tools = GSStatisticsFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("GraphBrowse")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Tools = GSBrowseFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("None")) == 0)
         ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->Tools = GSNoneFilterSensorTool;
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "ClassSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_ClassSQL(ads_callback_packet *dcl)
{
   Common_Dcl_main_filter_Struct *CommonStruct;
   C_CLASS_SQL *pClsSQL;
   C_CLASS     *pCls;
   C_INFO      *pInfo;
   C_STRING    SQLCond, str, CompleteName;

   if (!GS_CURRENT_WRK_SESSION) return;

   CommonStruct = (Common_Dcl_main_filter_Struct*)(dcl->client_data);

   pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.getptr_at(CommonStruct->ClsSQLSel + 1);
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub())) == NULL)
      return;
   if ((pInfo = pCls->ptr_info()) == NULL) return;
   SQLCond = pClsSQL->get_sql();
   
   str = gsc_msg(230); //  "Classe: "
   pCls->get_CompleteName(CompleteName);
   str += CompleteName;

   if (gsc_ddBuildQry(str.get_name(), pInfo->getDBConnection(OLD), pInfo->OldTableRef, SQLCond,
                      GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code, pClsSQL->get_sub()) != GS_GOOD) return;
   if (pClsSQL->set_sql(SQLCond.get_name()) == GS_BAD) return;
   gsc_setTileClsQryListDclmain_filter(dcl->dialog, 
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "ClsQryLis"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_ClsQryList(ads_callback_packet *dcl)
{
   ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->ClsSQLSel = _wtoi(dcl->value);
   ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->SecSQLSel = 0;
   gsc_setTileSecQryListDclmain_filter(dcl->dialog,
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data));
   
   if (dcl->reason == CBR_DOUBLE_CLICK)
      dcl_main_filter_ClassSQL(dcl);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "SecSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_SecSQL(ads_callback_packet *dcl)
{
   Common_Dcl_main_filter_Struct *CommonStruct;
   C_CLASS_SQL *pClsSQL;
   C_CLASS     *pCls;
   C_SEC_SQL   *pSecSQL;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pInfo;
   C_STRING    SQLCond, str;

   if (!GS_CURRENT_WRK_SESSION) return;

   CommonStruct = (Common_Dcl_main_filter_Struct*)(dcl->client_data);

   pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.getptr_at(CommonStruct->ClsSQLSel + 1);
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub())) == NULL)
      return;
   if (pCls->ptr_info() == NULL) return;
   pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->getptr_at(CommonStruct->SecSQLSel + 1);
   if (!pSecSQL) return;
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code,
                            pCls->ptr_id()->sub_code, pSecSQL->get_sec())) == NULL)
      return;

   if ((pInfo = pSec->ptr_info()) == NULL) return;
   SQLCond = pSecSQL->get_sql();
   
   str = gsc_msg(309); // "Tabella secondaria: "
   str += pSec->get_name();

   if (gsc_ddBuildQry(str.get_name(), pInfo->getDBConnection(OLD), pInfo->OldTableRef,
                      SQLCond, GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code,
                      pCls->ptr_id()->sub_code, pSecSQL->get_sec()) != GS_GOOD) return;
   if (pSecSQL->set_sql(SQLCond.get_name()) == GS_BAD) return;
   gsc_setTileSecQryListDclmain_filter(dcl->dialog, 
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "SecAggr"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_SecAggr(ads_callback_packet *dcl)
{
   Common_Dcl_main_filter_Struct *CommonStruct;
   C_CLASS_SQL *pClsSQL;
   C_CLASS     *pCls;
   C_SEC_SQL   *pSecSQL;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pInfo;
   C_STRING    SQLAggr, str;

   if (!GS_CURRENT_WRK_SESSION) return;

   CommonStruct = (Common_Dcl_main_filter_Struct*)(dcl->client_data);

   pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.getptr_at(CommonStruct->ClsSQLSel + 1);
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSQL->get_cls(), pClsSQL->get_sub())) == NULL)
      return;
   if (pCls->ptr_info() == NULL) return;
   pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->getptr_at(CommonStruct->SecSQLSel + 1);
   if (!pSecSQL) return;
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code,
                            pCls->ptr_id()->sub_code, pSecSQL->get_sec())) == NULL)
      return;

   if ((pInfo = pSec->ptr_info()) == NULL) return;
   SQLAggr = pSecSQL->get_grp();
   
   str = gsc_msg(309); // "Tabella secondaria: "
   str += pSec->get_name();

   if (gsc_ddBuildAggrQry(str.get_name(), pInfo->getDBConnection(OLD), pInfo->OldTableRef,
                          SQLAggr, GS_CURRENT_WRK_SESSION->get_PrjId(), pCls->ptr_id()->code,
                          pCls->ptr_id()->sub_code, pSecSQL->get_sec()) != GS_GOOD) return;
   if (pSecSQL->set_grp(SQLAggr.get_name()) == GS_BAD) return;
   gsc_setTileSecQryListDclmain_filter(dcl->dialog, 
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "SecQryList"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_SecQryList(ads_callback_packet *dcl)
{
   ((Common_Dcl_main_filter_Struct*)(dcl->client_data))->SecSQLSel = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
      dcl_main_filter_SecSQL(dcl);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "ClassQrySlid"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_ClassQrySlid(ads_callback_packet *dcl)
{
   gsc_setTileClsQryListDclmain_filter(dcl->dialog,
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data),
                                       _wtoi(dcl->value));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter" su controllo "SecQrySlid"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filter_SecQrySlid(ads_callback_packet *dcl)
{
   gsc_setTileSecQryListDclmain_filter(dcl->dialog,
                                       (Common_Dcl_main_filter_Struct*)(dcl->client_data),
                                       _wtoi(dcl->value));
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_main_filter_help(ads_callback_packet *dcl)
   { gsc_help(IDH_ANALISIDELLABANCADATILAFUNZIONEDIFILTRO); } 


/*****************************************************************************/
/*.doc gsddfilter                                                            */
/*+
  Questo comando effettua una interrogazione della banca dati caricata nella
  sessione di lavoro corrente con criteri spaziali e SQL.
-*/  
/*****************************************************************************/
void gsddfilter(void)
{
   C_CLASS    *pCls;
   int        ret = GS_GOOD, status = DLGOK, dcl_file, Sub;
   C_SELSET   PickSet;
   C_LINK_SET ResultLS;
   bool       UseLS;
   ads_hdlg   dcl_id;
   C_STRING   path;   
   Common_Dcl_main_filter_Struct CommonStruct;
   C_CLS_PUNT_LIST               ClassList;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opFilterEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   if (gsc_get_pick_first(PickSet) == GS_GOOD && PickSet.length() > 0)
      // Ricavo la lista delle classi nel gruppo di selezione
      if (gsc_get_class_list(PickSet, ClassList, &Sub) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   PickSet.clear();

   if (ClassList.get_count() == 1)
   {
      pCls = (C_CLASS *) ((C_CLS_PUNT *) ClassList.get_head())->get_class();
      if (Sub > 0)
         pCls = (C_CLASS *) pCls->ptr_sub_list()->search_key(Sub);
   }
   else
      // seleziona la classe o sottoclasse
      if ((ret = gsc_ddselect_class(NONE, &pCls, FALSE, &ClassList)) != GS_GOOD)
         if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
         else return GEOsimAppl::CMDLIST.CancelCmd();

   // prima inizializzazione
   CommonStruct.SpatialSel      = _T("\"\" \"\" \"\" \"Location\" (\"all\") \"\"");
   CommonStruct.LocationUse     = true;
   CommonStruct.LastFilterResultUse = false;
   CommonStruct.Logical_op      = UNION;
   CommonStruct.ClsSQLSel       = 0;
   CommonStruct.SecSQLSel       = 0;

   // Carica il valore del flag "tool"
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_B2STR                 *pProfileEntry;

   path = GS_CURRENT_WRK_SESSION->get_dir();
   path += _T('\\');
   path += GS_INI_FILE;

   CommonStruct.Tools = GSHighlightFilterSensorTool;
   if (gsc_path_exist(path) == GS_GOOD)
      if (gsc_read_profile(path, ProfileSections) == GS_GOOD)
         if ((pProfileEntry = ProfileSections.get_entry(_T("LAST_FILTER_OPTION"), _T("TOOL"))))
            CommonStruct.Tools =_wtoi(pProfileEntry->get_name2());

   if (CommonStruct.sql_list.initialize(pCls, TRUE) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GEOsimAppl::CMDLIST.ErrorCmd();

   while (1)
   {
      ads_new_dialog(_T("main_filter"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      gsc_setTileDclmain_filter(dcl_id, &CommonStruct);

      ads_action_tile(dcl_id, _T("Location"), (CLIENTFUNC) dcl_SpatialSel);
      ads_action_tile(dcl_id, _T("LocationUse"), (CLIENTFUNC) dcl_main_filter_LocationUse);
      ads_client_data_tile(dcl_id, _T("LocationUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Logical_op"), (CLIENTFUNC) dcl_main_filter_Logical_op);
      ads_client_data_tile(dcl_id, _T("Logical_op"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SSFilterUse"), (CLIENTFUNC) dcl_main_filter_LastFilterResultUse);
      ads_client_data_tile(dcl_id, _T("SSFilterUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Tools"), (CLIENTFUNC) dcl_main_filter_Tools);
      ads_client_data_tile(dcl_id, _T("Tools"), &CommonStruct);
      ads_action_tile(dcl_id, _T("ClsQryList"), (CLIENTFUNC) dcl_main_filter_ClsQryList);
      ads_client_data_tile(dcl_id, _T("ClsQryList"), &CommonStruct);
      ads_action_tile(dcl_id, _T("ClassSQL"), (CLIENTFUNC) dcl_main_filter_ClassSQL);
      ads_client_data_tile(dcl_id, _T("ClassSQL"), &CommonStruct);
      ads_action_tile(dcl_id, _T("ClassQrySlid"), (CLIENTFUNC) dcl_main_filter_ClassQrySlid);
      ads_client_data_tile(dcl_id, _T("ClassQrySlid"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecQryList"), (CLIENTFUNC) dcl_main_filter_SecQryList);
      ads_client_data_tile(dcl_id, _T("SecQryList"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecSQL"), (CLIENTFUNC) dcl_main_filter_SecSQL);
      ads_client_data_tile(dcl_id, _T("SecSQL"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecAggr"), (CLIENTFUNC) dcl_main_filter_SecAggr);
      ads_client_data_tile(dcl_id, _T("SecAggr"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecQrySlid"), (CLIENTFUNC) dcl_main_filter_SecQrySlid);
      ads_client_data_tile(dcl_id, _T("SecQrySlid"), &CommonStruct);
      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_main_filter_help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case LOCATION_SEL:  // definizione di area di selezione
         {
            gsc_ddSelectArea(CommonStruct.SpatialSel);
            break;
         }
	   }
   }
   ads_unload_dialog(dcl_file);

   if (ret != GS_GOOD)
      if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      else return GEOsimAppl::CMDLIST.CancelCmd();

   // ricavo il LINK_SET da usare per il filtro
   ret = gsc_get_LS_for_filter(pCls, CommonStruct.LocationUse,
                               CommonStruct.SpatialSel,
                               CommonStruct.LastFilterResultUse, 
                               CommonStruct.Logical_op, 
                               ResultLS);

   // se c'è qualcosa che non va
   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // se devono essere considerate tutte le entità della classe che sono state estratte
   if (ret == GS_CAN) UseLS = false;
   else UseLS = true;

   path = GS_CURRENT_WRK_SESSION->get_dir();
   path += _T('\\');
   path += GS_INI_FILE;

   bool Unicode = false;

   ProfileSections.remove_all();
   if (gsc_path_exist(path) == GS_GOOD)
      gsc_read_profile(path, ProfileSections, &Unicode);
   // Salva il valore del flag "tool"
   ProfileSections.set_entry(_T("LAST_FILTER_OPTION"), _T("TOOL"), CommonStruct.Tools);
   CommonStruct.sql_list.save(ProfileSections, _T("LAST_FILTERED_CLASSES"));
   gsc_write_profile(path, ProfileSections, Unicode);

   // Se la classe è griglia e si è scelto di evidenziare
   if (pCls->get_category() == CAT_GRID)
   {
      C_STRING  JoinOp, Boundary, SelType;
      C_RB_LIST CoordList;

      if (UseLS && ResultLS.ptr_KeyList()->get_count() == 0)
      {
         acutPrintf(gsc_msg(375), 0); // "\n%ld entità filtrate."
         GS_LSFILTER.clear();
         GS_LSFILTER.cls = pCls->ptr_id()->code;
         GS_LSFILTER.sub = pCls->ptr_id()->sub_code;
         gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro
         return GEOsimAppl::CMDLIST.EndCmd();
      }

      if (gsc_do_sql_filter(pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                            CommonStruct.sql_list, (UseLS) ? &ResultLS : NULL) == GS_BAD)
         return GEOsimAppl::CMDLIST.ErrorCmd();
      
      if (GS_LSFILTER.ptr_KeyList()->get_count() == 0)
      {
         acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
         return GEOsimAppl::CMDLIST.EndCmd();
      }
   }
   else
   {
      if (UseLS && ResultLS.ptr_SelSet()->length() == 0)
      {
         acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
         GS_LSFILTER.clear();
         GS_LSFILTER.cls = pCls->ptr_id()->code;
         GS_LSFILTER.sub = pCls->ptr_id()->sub_code;
         gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro
         return GEOsimAppl::CMDLIST.EndCmd();
      }

      if (gsc_do_sql_filter(pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                            CommonStruct.sql_list, (UseLS) ? &ResultLS : NULL) == GS_BAD)
         return GEOsimAppl::CMDLIST.ErrorCmd();
      
      if (GS_LSFILTER.ptr_SelSet()->length() == 0)
      {
         acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
         return GEOsimAppl::CMDLIST.EndCmd();
      }
   }

   acutPrintf(gsc_msg(0)); // "\nE' stato impostato il gruppo di selezione <ssfilter>."

   switch (CommonStruct.Tools)
   {
      case GSHighlightFilterSensorTool:
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDFILTEREVID\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      case GSExportFilterSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDFILTEREXPORT\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSStatisticsFilterSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDFILTERSTATISTICS\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSBrowseFilterSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }

         if (fwprintf(file, _T("GSFILTERBROWSE\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }

         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSNoneFilterSensorTool:
         break;
      default:
         break;
   }
 
   return GEOsimAppl::CMDLIST.EndCmd();
}


/*****************************************************************************/
/*.doc gsc_splitSpatialSel                                                  */
/*+
  Questa funzione effettua la scomposizione di una condizione spaziale in formato
  ADE, estraendo il tipo di selezione ("all", "window", "circle", "polygon", 
  "fence", bufferfence"),
  il modo di selezione ("crossing", "inside") e la lista delle coordinate.
  Parametri:
  C_STRING &SpatialSel;   Condizione spaziale secondo il
                          formato usato da ADE (vedi manuale) ad esempio:
                          ("AND" "(" "not" "Location" ("window" "crossing" (x1 y1 z1) (x2 y2 z2)) ")")
                          ("AND" "(" "not" "Location" ("bufferfence" "crossing" offset (x1 y1 z1) (x2 y2 z2)) ")")
                          ("AND" "(" "not" "Location" ("fence" (x1 y1 z1) (x2 y2 z2)) ")")
  C_STRING &JoinOp;        Operatore di unione con altre qry ("and", "or")
  bool     *Inverse;       True se impostata opzione not
  C_STRING &Boundary;      Tipo di selezione ("all", "window", "circle",
                           "polygon", "fence", bufferfence", "point")
  C_STRING &SelType;       Modo di selezione ("crossing", "inside")
  C_RB_LIST &CoordList;    Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_SplitSpatialSel(C_STR *pSpatialSel, C_STRING &JoinOp, bool *Inverse,
                        C_STRING &Boundary, C_STRING &SelType, C_RB_LIST &CoordList)
{
   C_STRING SpatialSel(pSpatialSel->get_name());
   
   return gsc_SplitSpatialSel(SpatialSel, JoinOp, Inverse, Boundary,
                              SelType, CoordList);
}
int gsc_SplitSpatialSel(C_STRING &SpatialSel, C_STRING &JoinOp, bool *Inverse,
                        C_STRING &Boundary, C_STRING &SelType, C_RB_LIST &CoordList)
{
   TCHAR     *p, *pCond = SpatialSel.get_name();
   int       Start, End;
   C_STRING  StrNum, dummy;
   double    value;
   ads_point point, FirstPt;
   bool      PolylineSel = false, PolylineClosed = false, isFirst = true;

   JoinOp.clear();
   *Inverse = false;
   Boundary.clear();
   SelType.clear();
   CoordList.remove_all();

   if (SpatialSel.len() == 0) return GS_GOOD;

   // cerco l'operatore AND o OR
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   if (End > Start)
      if (JoinOp.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
   pCond = pCond + End + 1;

   // non considero eventuali gruppi
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   pCond = pCond + End + 1;

   // cerco l'operatore di not
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   if (End > Start)
      if (dummy.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
   *Inverse = (dummy.comp("not", 0) == 0) ? true : false;
   pCond = pCond + End + 1;

   // non considero la parola chiave "Location"
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   pCond = pCond + End + 1;

   // cerco il tipo di selezione
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   if (Boundary.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
   
   pCond = pCond + End + 1;
   // case insensitive
   if (gsc_strcmp(Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0 ||       // window
       gsc_strcmp(Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0 ||       // cerchio
       gsc_strcmp(Boundary.get_name(), BUFFERFENCE_SPATIAL_COND, FALSE) == 0 ||  // bufferfence
       gsc_strcmp(Boundary.get_name(), POLYLINE_SPATIAL_COND, FALSE) == 0 ||     // polyline
       gsc_strcmp(Boundary.get_name(), POLYGON_SPATIAL_COND, FALSE) == 0)        // polygon

   {
      // Se si tratta di polyline cerco il tipo di selezione
      if (gsc_strcmp(Boundary.get_name(), POLYLINE_SPATIAL_COND, FALSE) == 0) // polyline
      {
         PolylineSel = true;

         if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
         End = Start = (int) (p - pCond) + 1;
         while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
         if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
         
         if (Boundary.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
         pCond = pCond + End + 1;
      }

      // cerco il modo di selezione
      if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      End = Start = (int) (p - pCond) + 1;
      while (pCond[End] != _T('\0') && pCond[End] != _T('"')) End++;
      if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      
      if (SelType.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;

      // case insensitive
      if (gsc_strcmp(SelType.get_name(), _T("crossing"), FALSE) != 0 &&
          gsc_strcmp(SelType.get_name(), _T("inside"), FALSE) != 0)
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      pCond = pCond + End + 1;

      // Se la selezione era di tipo "polyline"
      if (PolylineSel)
      {  // Flag polilinea aperta (1) o chiusa (0)
         while (*pCond != _T('\0') && *pCond == _T(' ')) pCond++;
         if (*pCond == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
         PolylineClosed = (*pCond == _T('0')) ? true : false;
         // Asse delle Z (lo salto)
         if ((pCond = wcschr(pCond, _T(')'))) == NULL) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
         pCond++;
      }
   }
   else
      // case insensitive
      if (gsc_strcmp(Boundary.get_name(), FENCE_SPATIAL_COND, FALSE) != 0 && // fence
          gsc_strcmp(Boundary.get_name(), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) != 0 && // fencebutstartend
          gsc_strcmp(Boundary.get_name(), POINT_SPATIAL_COND, FALSE) != 0 && // punto
          gsc_strcmp(Boundary.get_name(), ALL_SPATIAL_COND, FALSE) != 0)     // tutto
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

   // cerco la lista delle coordinate
   Start = 0;
   while (pCond[Start] == _T(' ') || pCond[Start] == _T('\n')) Start++;

   while (pCond[Start] != _T('\0'))
   {
      if (pCond[Start] == _T(')')) break; // esco
      else
      if (pCond[Start] == _T('\"'))
      {
         Start++;
         continue;
      }
      else
      if (pCond[Start] == _T('(')) // coordinate di un punto
      {
         // coordinata X
         End = ++Start;
         while (pCond[End] != _T('\0') && pCond[End] != _T(' ') && pCond[End] != _T(')'))
            End++;
         if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

         if (StrNum.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
         point[X] = _wtof(StrNum.get_name());
         // coordinata Y
         Start = End;
         End   = ++Start;
         while (pCond[End] != _T('\0') && pCond[End] != _T(' ') && pCond[End] != _T(')'))
            End++;
         if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

         if (StrNum.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
         point[Y] = _wtof(StrNum.get_name());
         // coordinata Z
         Start = End;
         End   = ++Start;
         while (pCond[End] != _T('\0') && pCond[End] != _T(' ') && pCond[End] != _T(')'))
            End++;
         if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

         if (Start == End) // il valore della Z non era presente
            point[Z] = 0;
         else
         {
            if (StrNum.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
            point[Z] = _wtof(StrNum.get_name());
         }

         if ((CoordList += acutBuildList(RTPOINT, point, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

         if (isFirst) // memorizzo primo punto
         {
            isFirst = false;
            ads_point_set(point, FirstPt);
         }
      }
      else // valore numerico (es. una distanza)
      {
         End = Start + 1;
         while (pCond[End] != _T('\0') && pCond[End] != _T(' ') && pCond[End] != _T('(') && pCond[End] != _T(')'))
            End++;
         if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

         if (StrNum.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
         value = _wtof(StrNum.get_name());

         if ((CoordList += acutBuildList(RTREAL, value, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }       
      }

      pCond = pCond + End + 1;
      Start = 0;
      while (pCond[Start] == _T(' ') || pCond[Start] == _T('\n')) Start++;
   }

   if (PolylineClosed)
      if ((CoordList += acutBuildList(RTREAL, 0.0, RTPOINT, FirstPt, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }       

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_fullSpatialSel                                                    */
/*+
  Questa funzione effettua la composizione di una condizione spaziale in formato
  ADE, dal tipo di selezione ("all", "window", "circle", "polygon",
  "fence", bufferfence"),
  il modo di selezione ("crossing", "inside") e la lista delle coordinate.
  Parametri:
  C_STRING  &JoinOp;       input
  bool      Inverse;       input
  C_STRING  &Boundary;     input
  C_STRING  &SelType;      input
  C_RB_LIST &CoordList;    input
  C_STRING  &SpatialSel;   output, condizione spaziale secondo il
                           formato usato da ADE (vedi manuale) ad esempio:
                           "and" "(" "not" "Location" ("window" "crossing" (x1 y1 z1) (x2 y2 z2)) ")"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_fullSpatialSel(C_STRING &JoinOp, bool Inverse,
                       C_STRING &Boundary, C_STRING &SelType, C_RB_LIST &CoordList,
                       C_STRING &SpatialSel)
{
   TCHAR   *StrNum;
   presbuf p;

   // operatore di join
   SpatialSel = _T("\"");
   SpatialSel += JoinOp;
   SpatialSel += _T("\"");

   // gruppo
   SpatialSel += _T(" \"\"");

   // inverso
   SpatialSel += _T(" \"");
   if (Inverse) SpatialSel += _T("NOT");
   
   SpatialSel += _T("\" \"Location\" (\"");

   // tipo di selezione
   SpatialSel += Boundary.get_name();
   SpatialSel += _T("\"");
   
   // da tenere presente che FENCEBUTSTARTEND_SPATIAL_COND non esiste in ADE
   if (gsc_strcmp(Boundary.get_name(), FENCE_SPATIAL_COND, FALSE) != 0 && // fence
       gsc_strcmp(Boundary.get_name(), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) != 0 && // fencebutstartend
       gsc_strcmp(Boundary.get_name(), ALL_SPATIAL_COND, FALSE) != 0)     // tutto
   {
      // cerco il modo di selezione
      SpatialSel += _T(" \"");
      SpatialSel += SelType.get_name();
      SpatialSel += _T("\"");
   }

   // cerco la lista delle coordinate
   p = CoordList.get_head();
   while (p)
   {
      SpatialSel += _T(" ");

      if (p->restype == RTPOINT || p->restype == RT3DPOINT)
         SpatialSel += _T("(");

      if ((StrNum = gsc_rb2str(p)) == NULL) return GS_BAD;
      gsc_strsep(StrNum, _T(' '), _T(',')); // sostituisco eventuali ',' con ' ' 

      SpatialSel += StrNum;
      free(StrNum);

      if (p->restype == RTPOINT || p->restype == RT3DPOINT)
         SpatialSel += _T(")");

      p = CoordList.get_next();
   }

   // gruppo
   SpatialSel += _T(")");
   SpatialSel += _T(" \"\"");

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setTileDclSpatialSel                                             */
/*+
  Questa funzione setta tutti i controlli della DCL "SpatialSel" in modo 
  appropriato secondo il tipo di selezione ("all", "window", "circle", 
  "polygon", "fence", "bufferfence"),
  il modo di selezione ("crossing", "inside") e la lista delle coordinate.
  Parametri:
  ads_hdlg  dcl_id;     
  Common_Dcl_SpatialSel_Struct *CommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_setTileDclSpatialSel(ads_hdlg dcl_id, Common_Dcl_SpatialSel_Struct *CommonStruct)
{
   presbuf p;

   // tipo di selezione
   if ((CommonStruct->Boundary).len() == 0)
   {
      ads_callback_packet TmpDCL;

      TmpDCL.dialog      = dcl_id;
      TmpDCL.value       = gsc_tostring(ALL_SPATIAL_COND); // tutto
      TmpDCL.client_data = CommonStruct;
      dcl_SpatialSel_Boundary(&TmpDCL);
      free(TmpDCL.value);

      return GS_GOOD;
   }

   ads_mode_tile(dcl_id, _T("SelType"), MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("Inverse"), MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("Define"),  MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("Select"),  MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("Save"),    MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("Load"),    MODE_ENABLE); 
   ads_mode_tile(dcl_id, _T("accept"),  MODE_ENABLE); 
   if (gsc_strcmp((CommonStruct->Boundary).get_name(), ALL_SPATIAL_COND, FALSE) == 0) // tutto
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("all"));
      ads_mode_tile(dcl_id, _T("SelType"), MODE_DISABLE); 
      ads_set_tile(dcl_id, _T("Inverse"), _T("0"));
      ads_mode_tile(dcl_id, _T("Inverse"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("Define"),  MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("Select"),  MODE_DISABLE); 
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("circle"));
      // modo di selezione
      if (gsc_strcmp((CommonStruct->SelType).get_name(), _T("inside"), FALSE) == 0)
         ads_set_tile(dcl_id, _T("SelType"), _T("inside"));
      else
         ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));

      if ((CommonStruct->CoordList).GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"),  MODE_DISABLE);
      }
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("polygon"));
      // modo di selezione
      if (gsc_strcmp((CommonStruct->SelType).get_name(), _T("inside"), FALSE) == 0)
         ads_set_tile(dcl_id, _T("SelType"), _T("inside"));
      else
         ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));

      if ((CommonStruct->CoordList).GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE);
      }
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), FENCE_SPATIAL_COND, FALSE) == 0) // fence
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("fence"));
      // modo di selezione
      ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));
      ads_mode_tile(dcl_id, _T("inside"),  MODE_DISABLE); 

      if (CommonStruct->CoordList.GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"),  MODE_DISABLE);
      }
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("fencebutstartend"));
      // modo di selezione
      ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));
      ads_mode_tile(dcl_id, _T("inside"),  MODE_DISABLE); 

      if (CommonStruct->CoordList.GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"),  MODE_DISABLE);
      }
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("bufferfence"));
      // modo di selezione
      if (gsc_strcmp((CommonStruct->SelType).get_name(), _T("inside"), FALSE) == 0)
         ads_set_tile(dcl_id, _T("SelType"), _T("inside"));
      else
         ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));

      if ((CommonStruct->CoordList).GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"),  MODE_DISABLE);
      }
   }
   else
   if (gsc_strcmp((CommonStruct->Boundary.get_name()), WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      ads_set_tile(dcl_id, _T("Boundary"), _T("window"));
      // modo di selezione
      if (gsc_strcmp((CommonStruct->SelType).get_name(), _T("inside"), FALSE) == 0)
         ads_set_tile(dcl_id, _T("SelType"), _T("inside"));
      else
         ads_set_tile(dcl_id, _T("SelType"), _T("crossing"));
      ads_mode_tile(dcl_id, _T("Select"), MODE_DISABLE); 

      if (CommonStruct->CoordList.GetCount() == 0) // se non ci sono coordinate
      {
         ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE);
      }
   }
   else
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

   // Inverso della selezione spaziale
   ads_set_tile(dcl_id, _T("Inverse"), (CommonStruct->Inverse) ? _T("1") : _T("0"));

   // lista di coordinate
   ads_start_list(dcl_id, _T("Coord"), LIST_NEW, 0);

   p = (CommonStruct->CoordList).get_head();
   if (p)
   {
      C_STRING StrNum;

      ads_mode_tile(dcl_id, _T("Show"), MODE_ENABLE);
      if (gsc_strcmp((CommonStruct->Boundary).get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
      {  // centro del cerchio
         TCHAR *dummy;

         if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;
         StrNum += _T(", ");
         StrNum += gsc_msg(88); // "raggio"
         StrNum += _T(" ");
         // valore del raggio
         p = (CommonStruct->CoordList).get_next();
         if (!p) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
         if ((dummy = gsc_rb2str(p, 50)) == NULL) return GS_BAD;
         StrNum += dummy;
         free(dummy);
         gsc_add_list(StrNum);
      }
      else
      if (gsc_strcmp((CommonStruct->Boundary).get_name(), FENCE_SPATIAL_COND, FALSE) == 0 || // fence
          gsc_strcmp((CommonStruct->Boundary).get_name(), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend

      {
         double bulge;

         // polilinea aperta o chiusa
         if ((p = p->rbnext))
            p = p->rbnext; // vettore normale asse Z

         // lista di punti
         while (p)
         {
            if (gsc_rb2Dbl(p, &bulge) == GS_GOOD)
            {
               p = p->rbnext;
               if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;
               StrNum += _T(", ");
               StrNum += gsc_msg(453); // prominenza
               StrNum += _T(" ");
               StrNum += bulge;
            }
            else
               if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;

            gsc_add_list(StrNum);
            p = p->rbnext;
         }
      }
      else
      if (gsc_strcmp((CommonStruct->Boundary).get_name(), BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
      {
         TCHAR  *dummy;
         double bulge;

         StrNum = gsc_msg(483); // "larghezza"
         StrNum += _T(" ");
         if ((dummy = gsc_rb2str(p, 50)) == NULL) return GS_BAD;
         StrNum += dummy;
         free(dummy);
         gsc_add_list(StrNum);

         // offset
         if ((p = p->rbnext))
            if ((p = p->rbnext)) // polilinea aperta o chiusa
               p = p->rbnext; // vettore normale asse Z

         // lista di punti
         while (p)
         {
            if (gsc_rb2Dbl(p, &bulge) == GS_GOOD)
            {
               p = p->rbnext;
               if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;
               StrNum += _T(", ");
               StrNum += gsc_msg(453); // prominenza
               StrNum += _T(" ");
               StrNum += bulge;
            }
            else
               if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;

            gsc_add_list(StrNum);
            p = p->rbnext;
         }
      }
      else // lista di punti
         while (p)
         {
            if (StrNum.paste(gsc_rb2str(p, 50)) == NULL) return GS_BAD;
            gsc_add_list(StrNum);
            p = (CommonStruct->CoordList).get_next();
         }
   }
   else
      ads_mode_tile(dcl_id, _T("Show"), MODE_DISABLE);
   
   ads_end_list();

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Boundary"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Boundary(ads_callback_packet *dcl)
{
   if (gsc_strcmp(dcl->value,
                  ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Boundary.get_name(),
                  FALSE) != 0)
   {
      // imposto il nuovo valore
      ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Boundary.set_name(dcl->value);
      
      if (((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType.len() == 0)
         // imposto il tipo di selezione
         ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType.set_name(_T("crossing"));
      
      // cancello la lista delle coordinate
      ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->CoordList.remove_all();
      
      // setto i controlli
      gsc_setTileDclSpatialSel(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "SelType"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_SelType(ads_callback_packet *dcl)
{
   if (gsc_strcmp(dcl->value,
                  ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType.get_name(),
                  FALSE) != 0)
   {
      // imposto il nuovo valore
      ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType.set_name(dcl->value);
      // setto i controlli
      gsc_setTileDclSpatialSel(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Inverse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Inverse(ads_callback_packet *dcl)
{
   // imposto il nuovo valore
   ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Inverse = (gsc_strcmp(dcl->value, _T("0")) == 0) ? FALSE : TRUE;
   // setto i controlli
   gsc_setTileDclSpatialSel(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Define"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Define(ads_callback_packet *dcl)
{
   TCHAR buffer[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Boundary"), buffer, TILE_STR_LIMIT); 
   // case insensitive
   if (gsc_strcmp(buffer, WINDOW_SPATIAL_COND, FALSE) == 0) // window
      ads_done_dialog(dcl->dialog, WINDOW_DEF);         
   else
   if (gsc_strcmp(buffer, CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
      ads_done_dialog(dcl->dialog, CIRCLE_DEF);         
   else
   if (gsc_strcmp(buffer, POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
      ads_done_dialog(dcl->dialog, POLYGON_DEF);         
   else
   if (gsc_strcmp(buffer, FENCE_SPATIAL_COND, FALSE) == 0) // fence
      ads_done_dialog(dcl->dialog, FENCE_DEF);         
   else
   if (gsc_strcmp(buffer, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
      ads_done_dialog(dcl->dialog, FENCEBUTSTARTEND_DEF);         
   else
   if (gsc_strcmp(buffer, BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
      ads_done_dialog(dcl->dialog, BUFFERFENCE_DEF);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Select"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Select(ads_callback_packet *dcl)
{
   TCHAR buffer[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Boundary"), buffer, TILE_STR_LIMIT); 
   // case insensitive
   if (gsc_strcmp(buffer, CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
      ads_done_dialog(dcl->dialog, CIRCLE_SEL);         
   else
   if (gsc_strcmp(buffer, POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
      ads_done_dialog(dcl->dialog, POLYGON_SEL);         
   else
   if (gsc_strcmp(buffer, FENCE_SPATIAL_COND, FALSE) == 0) // fence
      ads_done_dialog(dcl->dialog, FENCE_SEL);         
   else
   if (gsc_strcmp(buffer, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
      ads_done_dialog(dcl->dialog, FENCEBUTSTARTEND_SEL);         
   else
   if (gsc_strcmp(buffer, BUFFERFENCE_SPATIAL_COND, FALSE) == 0)  // bufferfence
      ads_done_dialog(dcl->dialog, BUFFERFENCE_SEL);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Show"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Show(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, SHOW);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Load"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Load(ads_callback_packet *dcl)
{
	presbuf  QueryDir = NULL;
   C_STRING FileName, LastSpatialQryFile;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSpatialQryFile"), LastSpatialQryFile) == GS_BAD ||
       gsc_dir_exist(LastSpatialQryFile) == GS_BAD)
   {
      // il direttorio di default per le QRY è una variabile d'ambiente di ADE
      if ((QueryDir = ade_prefgetval(_T("QueryFileDirectory"))) == NULL ||
          QueryDir->restype != RTSTR)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSVarNotDef)); // "*Errore* Variabile non definita"
         if (QueryDir) acutRelRb(QueryDir);
         return;
      }
   }
   else
   {
      if (gsc_dir_from_path(LastSpatialQryFile, LastSpatialQryFile) == GS_BAD) return;
      if ((QueryDir = acutBuildList(RTSTR, LastSpatialQryFile.get_name(), 0)) == NULL) return;
   }

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), QueryDir->resval.rstring, _T("qry"), UI_LOADFILE_FLAGS, FileName) == RTNORM)
   {
      FILE       *file = NULL;
      C_STR_LIST SpatialSelList;
      C_STRING   SpatialSel, JoinOp, Boundary, SelType;
      C_RB_LIST  CoordList;
      bool       Inverse, IsUnicode;

      // Apertura file NON in modo UNICODE (i file .QRY sono ancora in ANSI)
      if ((file = gsc_fopen(FileName, _T("r"), MORETESTS, &IsUnicode)) == NULL)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSOpenFile)); // "*Errore* Apertura file fallita"
         acutRelRb(QueryDir);
         return;
      }
   
      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastSpatialQryFile"), FileName);

      do
      {
         if (gsc_SpatialSel_Load(file, SpatialSelList) == GS_BAD || 
             SpatialSelList.get_count() == 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(GS_ERR_COD));
            break;
         }
         if (gsc_fclose(file) == GS_BAD)
         {
            file = NULL;
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
            break;
         }
         file = NULL;
         // scompongo solo la prima condizione impostata
         if (gsc_SplitSpatialSel((C_STR *) SpatialSelList.get_head(), 
                                 JoinOp, &Inverse, Boundary,
                                 SelType, CoordList) == GS_BAD)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
            break;
         }
         // imposto i nuovi valori
         ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Boundary = Boundary;
         ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType  = SelType;
         ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Inverse  = Inverse;
         ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->CoordList << CoordList.get_head();
         CoordList.ReleaseAllAtDistruction(GS_BAD);

         // setto i controlli
         gsc_setTileDclSpatialSel(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)dcl->client_data);
      }
      while (0);

      if (file)
         if (gsc_fclose(file) == GS_BAD)
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }

   acutRelRb(QueryDir);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" su controllo "Save"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel_Save(ads_callback_packet *dcl)
{
	presbuf  QueryDir = NULL;
   C_STRING FileName, DefaultFileName, LastSpatialQryFile;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSpatialQryFile"), LastSpatialQryFile) == GS_BAD ||
       gsc_dir_exist(LastSpatialQryFile) == GS_BAD)
   {
      // il direttorio di default per le QRY è una variabile d'ambiente di ADE
      if ((QueryDir = ade_prefgetval(_T("QueryFileDirectory"))) == NULL ||
          QueryDir->restype != RTSTR)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSVarNotDef)); // "*Errore* Variabile non definita"
         if (QueryDir) acutRelRb(QueryDir);
         return;
      }
   }
   else
   {
      if (gsc_dir_from_path(LastSpatialQryFile, LastSpatialQryFile) == GS_BAD) return;
      if ((QueryDir = acutBuildList(RTSTR, LastSpatialQryFile.get_name(), 0)) == NULL) return;
   }

   // "Posizione"
   if (gsc_get_tmp_filename(QueryDir->resval.rstring, gsc_msg(654), _T(".qry"), DefaultFileName) == GS_BAD)
      { acutRelRb(QueryDir); return; }
   acutRelRb(QueryDir);

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), DefaultFileName, _T("qry"), UI_SAVEFILE_FLAGS, FileName) == RTNORM)
   {
      TCHAR    Prefix[] = _T("(setq ade_cmddia_before_qry (getvar \"cmddia\"))\n(setvar \"cmddia\" 0)\n(ade_qryclear)\n");
      TCHAR    Postfix[] = _T("(setvar \"cmddia\" ade_cmddia_before_qry)\n");
      C_STRING JoinOp, SpatialSel, InverseStr; // JoinOp non usato
      FILE     *file;
      C_STRING ext;

      gsc_splitpath(FileName, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // aggiungo l'estensione "QRY"
         FileName += _T(".QRY");

      // compongo la condizione impostata
      if (gsc_fullSpatialSel(JoinOp,
                             ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Inverse,
                             ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Boundary,
                             ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->SelType,
                             ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->CoordList,
                             SpatialSel) == GS_BAD)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
         return;
      }

      // Apertura file NON in modo UNICODE (i file .QRY sono ancora in ANSI)
      if ((file = gsc_fopen(FileName, _T("w+"))) == NULL)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidPath)); // "*Errore* Path non valida: controllare il percorso e/o le macchine in rete"
         return;
      }

      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastSpatialQryFile"), FileName);

      do
      {
         if (fwprintf(file, Prefix) < 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
         
         if (gsc_SpatialSel_Save(file, SpatialSel) == GS_BAD)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }

         if (fwprintf(file, Postfix) < 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
      }
      while (0);

      if (gsc_fclose(file) == GS_BAD)
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SpatialSel_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Selezionespaziale); } 


/*****************************************************************************/
/*.doc gsc_ddSelectArea                                                      */
/*+
  Questa funzione effettua una selezione di un'area con interfaccia a finestra.
  Parametri:
  C_STRING &SpatialSel;   Condizione spaziale secondo il
                          formato usato da ADE (vedi manuale)
                          ("AND "(" "not" "window" "crossing" (x1 y1 z1) (x2 y2 z2))

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddSelectArea(C_STRING &SpatialSel)
{
   int       ret = GS_GOOD, status = DLGOK, dcl_file;
   C_STRING  buffer, JoinOp;   
   ads_hdlg  dcl_id;
   Common_Dcl_SpatialSel_Struct CommonStruct;

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &CommonStruct.Inverse,
                           CommonStruct.Boundary, CommonStruct.SelType,
                           CommonStruct.CoordList) == GS_BAD)
      return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   buffer = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(buffer, &dcl_file) == RTERROR) return GS_BAD;
    
   while (1)
   {
      ads_new_dialog(_T("SpatialSel"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      ads_action_tile(dcl_id, _T("Boundary"), (CLIENTFUNC) dcl_SpatialSel_Boundary);
      ads_client_data_tile(dcl_id, _T("Boundary"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SelType"),  (CLIENTFUNC) dcl_SpatialSel_SelType);
      ads_client_data_tile(dcl_id, _T("SelType"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Inverse"),  (CLIENTFUNC) dcl_SpatialSel_Inverse);
      ads_client_data_tile(dcl_id, _T("Inverse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Define"),   (CLIENTFUNC) dcl_SpatialSel_Define);
      ads_action_tile(dcl_id, _T("Select"),   (CLIENTFUNC) dcl_SpatialSel_Select);
      ads_action_tile(dcl_id, _T("Show"),     (CLIENTFUNC) dcl_SpatialSel_Show);
      ads_action_tile(dcl_id, _T("Load"),     (CLIENTFUNC) dcl_SpatialSel_Load);
      ads_client_data_tile(dcl_id, _T("Load"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Save"),     (CLIENTFUNC) dcl_SpatialSel_Save);
      ads_client_data_tile(dcl_id, _T("Save"), &CommonStruct);
      ads_action_tile(dcl_id, _T("help"),     (CLIENTFUNC) dcl_SpatialSel_help);

      // setto i controlli
      if (gsc_setTileDclSpatialSel(dcl_id, &CommonStruct) == GS_BAD)
         { ret = GS_BAD; break; }

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  // compongo la condizione impostata
         C_STRING JoinOp; // non usato

         if (gsc_fullSpatialSel(JoinOp, CommonStruct.Inverse,
                                CommonStruct.Boundary, CommonStruct.SelType,
                                CommonStruct.CoordList, SpatialSel) == GS_BAD)
             { ret = GS_BAD; break; }
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case WINDOW_DEF:  // definizione di una finestra
            gsc_define_window(CommonStruct.CoordList);
            break; 
         case CIRCLE_SEL:  // selezione di un cerchio esistente
            gsc_select_circle(CommonStruct.CoordList);
            break; 
         case CIRCLE_DEF:  // definizione di un cerchio
            gsc_define_circle(CommonStruct.CoordList);
            break; 
         case POLYGON_SEL: // selezione di un poligono esistente
            gsc_select_polygon(CommonStruct.CoordList);
            break; 
         case POLYGON_DEF: // definizione di un poligono
            gsc_define_polygon(CommonStruct.CoordList);
            break; 
         case FENCE_SEL:   // selezione di una fence esistente
         case FENCEBUTSTARTEND_SEL:
            gsc_select_fence(CommonStruct.CoordList);
            break; 
         case FENCE_DEF:   // definizione di una fence
         case FENCEBUTSTARTEND_DEF:
            gsc_define_fence(CommonStruct.CoordList);
            break; 
         case BUFFERFENCE_SEL:   // selezione di un bufferfence esistente
            gsc_select_bufferfence(CommonStruct.CoordList);
            break; 
         case BUFFERFENCE_DEF:   // definizione di un bufferfence
            gsc_define_bufferfence(CommonStruct.CoordList);
            break; 
         case SHOW:        // mostra la selezione corrente
            gsc_showSpatialSel(CommonStruct.Boundary.get_name(), CommonStruct.CoordList, GS_GOOD);
            break; 
	   }
   }
   ads_unload_dialog(dcl_file);

   return ret;
}

/*****************************************************************************/
/*.doc gsc_showSpatialSel                                                     */
/*+
  Questa funzione effettua la selezione di una finestra.
  Parametri:
  const TCHAR *Boundary;  Forma della sessione
  C_RB_LIST &CoordList;   Lista delle coordinate
  int WaitForUser;        Flag, se = GS_GOOD si attende che l'utente prema <invio>
                          (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_showSpatialSel(const TCHAR *Boundary, C_RB_LIST &CoordList, int WaitForUser)
{
   presbuf prb;
   TCHAR str[133];
   
   // case insensitive
   if (gsc_strcmp(Boundary, WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      ads_point pt1, pt2, pt3, pt4;

      // disegno il rettangolo
      if ((prb = CoordList.get_head()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, pt1);
      if ((prb = CoordList.get_next()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, pt3);
      pt2[X] = pt3[X]; pt2[Y] = pt1[Y];
      pt4[X] = pt1[X]; pt4[Y] = pt3[Y];
      
      if (acedGrDraw(pt1, pt2, 1, 1) != RTNORM || acedGrDraw(pt2, pt3, 1, 1) != RTNORM ||
          acedGrDraw(pt3, pt4, 1, 1) != RTNORM || acedGrDraw(pt4, pt1, 1, 1) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   }
   else
   if (gsc_strcmp(Boundary, CIRCLE_SPATIAL_COND, FALSE) == 0)  // cerchio
   {
      ads_point center;
      presbuf   prb;

      if ((prb = CoordList.get_head()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, center);
      if ((prb = CoordList.get_next()) == NULL || prb->restype != RTREAL)
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

      // disegna cerchio
      if (gsc_circle_point(center, prb->resval.rreal, 100, &prb) == GS_GOOD)
      {
         presbuf Tail = prb;
         
         while (Tail->rbnext) Tail = Tail->rbnext;
         Tail->rbnext = acutBuildList(RTPOINT, prb->resval.rpoint, 0);
         gsc_grdraw(prb, 1, 1);
         acutRelRb(prb);
      }
   }
   else
   if (gsc_strcmp(Boundary, POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   {
      ads_point pnt;
      presbuf   prb;

      if ((prb = CoordList.get_head()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, pnt);

      // disegno il poligono
      gsc_grdraw(CoordList.get_head(), 1, 1);
      // chiudo poligono
      acedGrDraw(pnt, CoordList.get_tail()->resval.rpoint, 1, 1);
   }
   else
   if (gsc_strcmp(Boundary, FENCE_SPATIAL_COND, FALSE) == 0 || // fence
       gsc_strcmp(Boundary, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
   {
      presbuf prb = CoordList.get_head();

      if (!prb) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      // verifico se la polilinea è chiusa
      bool Closed = (prb->resval.rint == 1) ? true : false;
      if (!(prb = prb->rbnext)){ GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      // salto il vettore normale asse Z
      if (!(prb = prb->rbnext)){ GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

      // disegno la polilinea fence
      gsc_grdraw(prb, 1, 1, Closed);
   }
   else
   if (gsc_strcmp(Boundary, BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      presbuf prb = CoordList.get_head();

      if (!prb) { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      // Offset
      if (!(prb = prb->rbnext)){ GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      // verifico se la polilinea è chiusa
      bool Closed = (prb->resval.rint == 1) ? true : false;
      if (!(prb = prb->rbnext)){ GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      // salto il vettore normale asse Z
      if (!(prb = prb->rbnext)){ GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }

      // disegno la polilinea bufferfence
      gsc_grdraw(prb, 1, 1, Closed);
   }

   if (WaitForUser == GS_GOOD) 
      acedGetString(TRUE, gsc_msg(87), str); // "\nPremere <Invio>"

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_IsWindowContainedInCurrentScreen                                                     */
/*+
  Questa funzione verifica se la finestra passata come parametro
  è contenuta nel video corrente. In caso contrario le coordinate dei punti
  verranno modificate per ottenere una finestra atta a contenere il 
  video corrente + la finestra passata.
  Parametri:
  ads_point Corner1;     angolo della finestra
  ads_point Corner2;     angolo della finestra

  Restituisce GS_GOOD se la finestra è già contenuta dello video corrente
  altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_IsWindowContainedInCurrentScreen(ads_point Corner1, ads_point Corner2)
{
   int       IsContained = GS_GOOD;
   ads_point pt1, pt2;
   double    Coord;

   // Se la finestra non è completamente a video devo fare uno zoom per includerla
   gsc_getCurrentScreenCoordinate(pt1, pt2);

   Coord = min(Corner1[X], Corner2[X]);
   if (Coord < pt1[X]) { IsContained = GS_BAD; Corner1[X] = Coord; }
   else Corner1[X] = pt1[X];

   Coord = min(Corner1[Y], Corner2[Y]);
   if (Coord < pt1[Y]) { IsContained = GS_BAD; Corner1[Y] = Coord; }
   else Corner1[Y] = pt1[Y];

   Coord = max(Corner1[X], Corner2[X]);
   if (Coord > pt2[X]) { IsContained = GS_BAD; Corner2[X] = Coord; }
   else Corner2[X] = pt2[X];

   Coord = max(Corner1[Y], Corner2[Y]);
   if (Coord > pt2[Y]) { IsContained = GS_BAD; Corner2[Y] = Coord; }
   else Corner2[Y] = pt2[Y];

   return IsContained;
}
int gsc_IsWindowContainedInCurrentScreen(presbuf CoordList, bool Closed,
                                         ads_point WinPt1, ads_point WinPt2,
                                         double OffSet = 0.0,
                                         ads_point ObjWinPt1 = NULL, ads_point ObjWinPt2 = NULL)
{
   presbuf      p = CoordList;
   double       Bulge;
   int          i = 0;
   AcDb2dVertex Vertex;
   AcGePoint2d  dummyPt;
   AcDbPolyline dummyPline;

   // Creo una polilinea 2D
   while (p)
   {
      if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
      else if (!(p = p->rbnext)) return GS_BAD;

      AcGePoint2d_set_from_ads_point(p->resval.rpoint, dummyPt);
      dummyPline.addVertexAt(i++, dummyPt, Bulge);

      p = p->rbnext;
   }  

   if (Closed) dummyPline.setClosed(Adesk::kTrue);

   if (gsc_get_ent_window(&dummyPline, WinPt1, WinPt2) == GS_BAD) return GS_BAD;
   WinPt1[X] -= OffSet; WinPt1[Y] -= OffSet;
   WinPt2[X] += OffSet; WinPt2[Y] += OffSet;

   // Coordinate dell'oggetto
   if (ObjWinPt1) ads_point_set(WinPt1, ObjWinPt1);
   if (ObjWinPt2) ads_point_set(WinPt2, ObjWinPt2);

   return gsc_IsWindowContainedInCurrentScreen(WinPt1, WinPt2);
}


/*****************************************************************************/
/*.doc gsc_define_window                                                     */
/*+
  Questa funzione effettua la definizione di una nuova finestra.
  Parametri:
  C_RB_LIST &CoordList; Lista delle coordinate
  oppure
  C_RECT &Rectangle;    Rettangolo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_define_window(C_RB_LIST &CoordList)
{
   ads_point pt1, pt2, pnt;
   int       ret;
   C_STRING  FirstMsg, SecondMsg;
   
   FirstMsg  = gsc_msg(349); // "\nPrimo angolo: "
   SecondMsg = gsc_msg(350); // "\nSecondo angolo: "

   // finestra già definita
   if (CoordList.GetCount() > 0)
   {
      TCHAR   *str = NULL;
      presbuf prb;

      acutPrintf(gsc_msg(91)); // "\nModificare la finestra:"

      if ((prb = CoordList.get_head()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, pt1);
      if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
      FirstMsg += _T("<");
      FirstMsg += str;
      FirstMsg += _T(">: ");
      free(str);
      
      if ((prb = CoordList.get_next()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, pt2);
      if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
      SecondMsg += _T("<");
      SecondMsg += str;
      SecondMsg += _T(">: ");
      free(str);
   }

   if ((ret = acedGetPoint(NULL, FirstMsg.get_name(), pnt)) == RTERROR)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;
   if (ret != RTNONE) ads_point_set(pnt, pt1);

   if ((ret = ads_getcorner(pt1, SecondMsg.get_name(), pnt)) == RTERROR)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;
   if (ret != RTNONE) ads_point_set(pnt, pt2);

   // pt1 deve essere il punto in basso a sin
   if (pt1[X] > pt2[X]) 
      { pnt[X] = pt1[X]; pt1[X] = pt2[X]; pt2[X] = pnt[X]; }
   if (pt1[Y] > pt2[Y]) 
      { pnt[Y] = pt1[Y]; pt1[Y] = pt2[Y]; pt2[Y] = pnt[Y]; }

   // disegno il rettangolo
   ads_point_set(pt1, pnt);
   pnt[X] = pt2[X];
   if (acedGrDraw(pt1, pnt, 1, 1) != RTNORM || acedGrDraw(pnt, pt2, 1, 1) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pnt[X] = pt1[X];
   pnt[Y] = pt2[Y];
   if (acedGrDraw(pt2, pnt, 1, 1) != RTNORM || acedGrDraw(pnt, pt1, 1, 1) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if ((CoordList << acutBuildList(RTPOINT, pt1, RTPOINT, pt2, 0)) == NULL)
      return GS_BAD;

   return GS_GOOD;
}
int gsc_define_window(C_RECT &Rectangle)
{
   C_RB_LIST CoordList;
   
   if (gsc_define_window(CoordList) == GS_BAD) return GS_BAD;
   gsc_rb2Pt(CoordList.getptr_at(1), Rectangle.BottomLeft.point);
   gsc_rb2Pt(CoordList.getptr_at(2), Rectangle.TopRight.point);
   
   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_selObjsWindow                                                     */
/*+
  Questa funzione effettua la selezione degli oggetti in una finestra.
  punto.
  Parametri:
  C_RB_LIST &CoordList;    Lista delle coordinate
  int type;                flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  ads_name ss;             Gruppo di selezione (non allocato)
  int ExactMode;           Controllo su coordinate di ogni oggetto (default = GS_BAD)
  int CheckOn2D;           Controllo oggetti senza usare la Z (default = GS_GOOD) 
                           usato solo se ExactMode = GS_GOOD
  int OnlyInsPt;           Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
                           usato solo se ExactMode = GS_GOOD
  int Cls;                 Per cercare solo oggetti di una certa classe (default = 0)
  int Sub;                 Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int ObjType;             Tipo di oggetti grafici (default = GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsWindow(C_RB_LIST &CoordList, int type, C_SELSET &ss, int ExactMode,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   presbuf   prb1, prb2;
   int       ToZoom = FALSE;
   ads_point pt1, pt2;

   if ((prb1 = CoordList.get_head()) == NULL ||
       (prb1->restype != RTPOINT && prb1->restype != RT3DPOINT))
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   ads_point_set(prb1->resval.rpoint, pt1);

   if ((prb2 = CoordList.get_next()) == NULL ||
       (prb2->restype != RTPOINT && prb2->restype != RT3DPOINT))
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   ads_point_set(prb2->resval.rpoint, pt2);

   // Se la finestra non è completamente a video devo fare uno zoom per includerla
   if (gsc_IsWindowContainedInCurrentScreen(pt1, pt2) == GS_BAD)
      gsc_zoom(pt1, pt2);

   if (gsc_ssget((type == INSIDE) ? _T("_W") : _T("_C"),
                  prb1->resval.rpoint, prb2->resval.rpoint, NULL, ss) != RTNORM)
      return GS_CAN;

   if (Cls > 0) ss.intersectClsCode(Cls, Sub, ObjType);
   else ss.intersectType(ObjType);

   if (ExactMode == GS_GOOD)
   {
      // poichè ssget si basa sulla mappa video (la precisione dipende dal livello
      // di zoom corrente) può ritornare oggetti che non sono effettivamente
      // corrispondenti al tipo di selezione ma sono molto vicini
      
      // Includo tutti i testi e i blocchi che sono con punto di inserimento
      // interno all'area
      C_SELSET PunctualSS;
      if (gsc_getNodesInWindow(prb1->resval.rpoint, prb2->resval.rpoint,
                               PunctualSS, CheckOn2D) == GS_BAD) return GS_BAD;
      if (Cls > 0) PunctualSS.intersectClsCode(Cls, Sub, ObjType);
      else PunctualSS.intersectType(ObjType);

      ss.add_selset(PunctualSS);

      if (Cls > 0) ss.intersectClsCode(Cls, Sub, ObjType); // roby da togliere
      else ss.intersectType(ObjType);

      AcGePoint3dArray Vertices;

      Vertices.setLogicalLength(4);
      Vertices[0].set(prb1->resval.rpoint[X], prb1->resval.rpoint[Y], // X, Y
                           (CheckOn2D == GS_GOOD) ? 0.0 : prb1->resval.rpoint[Z]); // Z
      Vertices[1].set(prb2->resval.rpoint[X], prb1->resval.rpoint[Y], // X, Y
                           (CheckOn2D == GS_GOOD) ? 0.0 : prb1->resval.rpoint[Z]); // Z
      Vertices[2].set(prb2->resval.rpoint[X], prb2->resval.rpoint[Y], // X, Y
                           (CheckOn2D == GS_GOOD) ? 0.0 : prb1->resval.rpoint[Z]); // Z
      Vertices[3].set(prb1->resval.rpoint[X], prb2->resval.rpoint[Y], // X, Y
                           (CheckOn2D == GS_GOOD) ? 0.0 : prb1->resval.rpoint[Z]); // Z

      AcDb3dPolyline dummyPline(AcDb::k3dSimplePoly, Vertices, Adesk::kTrue); // closed

      if (ss.SpatialInternalEnt(&dummyPline, CheckOn2D, OnlyInsPt, type) == GS_BAD)
         { gsc_ReleaseSubEnts(&dummyPline); return GS_BAD; }

      // roby 2017
      if (type == INSIDE) // devo verificare che tutte le componenti geometriche delle entità siano completamente interne
      {
         ads_name ent;
         C_SELSET entSelSet;
         int      i = 0, n, j;
         C_CLASS  *pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub);

         while (ss.entname(i++, ent) == GS_GOOD)
            if (pCls->get_SelSet(ent, entSelSet, ObjType) == GS_GOOD)
            {
               n = entSelSet.length();
               // rimuovo i riempimenti
               j = 0;
               while (entSelSet.entname(j++, ent) == GS_GOOD)
                  if (gsc_ishatch(ent) == GS_GOOD)
                     { entSelSet.subtract_ent(ent); j--; }

               if (entSelSet.SpatialInternalEnt(&dummyPline, CheckOn2D, OnlyInsPt, type) == GS_GOOD)
                  if (n > entSelSet.length())
                     { ss.subtract(entSelSet); i--; } // tolgo l'entità da SelSet
            }
      }

      gsc_ReleaseSubEnts(&dummyPline);
   }
   
   return GS_GOOD;
}
int gsc_selObjsWindow(C_RB_LIST &CoordList, int type, ads_name ss, int ExactMode,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   C_SELSET InternalSS;
   int      Result = gsc_selObjsWindow(CoordList, type, InternalSS, ExactMode, CheckOn2D, OnlyInsPt,
                                       Cls, Sub, ObjType);

   InternalSS.get_selection(ss);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return Result;
}


/*****************************************************************************/
/*.doc gsc_select_polygon                                                     */
/*+
  Questa funzione effettua la selezione di un poligono esistente.
  Parametri:
  C_RB_LIST &CoordList;   Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_select_polygon(C_RB_LIST &CoordList)
{
   C_POINT_LIST PntList;
   ads_point    pnt;
   ads_name     ent;
   int          ret;

   do
   {
      acutPrintf(gsc_msg(96)); // "\nSeleziona oggetto: "
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      while ((ret = acedEntSel(GS_EMPTYSTR, ent, pnt)) == RTERROR);
      if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ret == RTCAN) return GS_CAN;

      // se è una polilinea o lwpolilinea con vertici validi
      if (PntList.add_vertex_point(ent, GS_GOOD) == GS_GOOD &&
          PntList.get_count() > 0) break;

      acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
   }
   while (1);

   ads_point_set(((C_POINT * )PntList.get_head())->point, pnt); // primo punto
   if ((CoordList << PntList.to_rb()) == NULL) return GS_BAD;

   // prima cancello il poligono (per baco acad di refresh video)
   gsc_grdraw(CoordList.get_head(), -1, 1);
   acedGrDraw(pnt, CoordList.get_tail()->resval.rpoint, -1, 1);

   // disegno il poligono
   gsc_grdraw(CoordList.get_head(), 1, 1);
   // chiudo poligono
   acedGrDraw(pnt, CoordList.get_tail()->resval.rpoint, 1, 1);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_define_polygon                                                     */
/*+
  Questa funzione effettua la definizione di un nuovo poligono.
  Parametri:
  C_RB_LIST &CoordList;   Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_define_polygon(C_RB_LIST &CoordList)
{
   ads_point pt1, pt2, pnt;
   C_RB_LIST points;
   presbuf   prb;
   int       ret;
      
   // poligono già definito
   if ((prb = CoordList.get_head()) != NULL)
   {
      C_STRING Msg;
      TCHAR    *str = NULL;
      int      stop = FALSE;

      acutPrintf(gsc_msg(646)); // "\nModificare il poligono:"

      // primo punto
      while (prb)
      {
         Msg = gsc_msg(647); // "Primo punto o C per cancellare: "
         if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
         Msg += _T("<");
         Msg += str;
         Msg += _T(">: ");
         free(str);

         acedInitGet(0, gsc_msg(649)); // "C"
         if ((ret = acedGetPoint(NULL, Msg.get_name(), pt1)) == RTERROR)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         switch (ret)
         {
            case RTCAN: return GS_CAN;
            case RTNORM:
               stop = TRUE;
               break;
            case RTNONE:
               ads_point_set(prb->resval.rpoint, pt1);
               stop = TRUE;
               break;
         }
         if (stop) break;

         prb = CoordList.get_next();
      }
      if ((points << acutBuildList(RTPOINT, pt1, 0)) == NULL) return GS_BAD;
      ads_point_set(pt1, pnt); // primo punto

      // punti successivi
      while ((prb = CoordList.get_next()) != NULL)
      {
         Msg = gsc_msg(648); // "\nPunto seguente o C per cancellare: "
         if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
         Msg += _T("<");
         Msg += str;
         Msg += _T(">: ");
         free(str);

         acedInitGet(0, gsc_msg(649)); // "C"
         if ((ret = acedGetPoint(pt1, Msg.get_name(), pt2)) == RTERROR)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         switch (ret)
         {
            case RTCAN: return GS_CAN;
            case RTNONE:
               ads_point_set(prb->resval.rpoint, pt2);
            case RTNORM:
               if ((points += acutBuildList(RTPOINT, pt2, 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               acedGrDraw(pt1, pt2, -1, 1);
               ads_point_set(pt2, pt1);
               break;
         }
      }
   }
   else
   {
      if ((ret = acedGetPoint(NULL, gsc_msg(351), pt1)) == RTERROR) // "\nPrimo punto: "
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ret == RTCAN) return GS_CAN;
      if ((points << acutBuildList(RTPOINT, pt1, 0)) == NULL) return GS_BAD;
      ads_point_set(pt1, pnt); // primo punto
   }

   while ((ret = acedGetPoint(pt1, gsc_msg(352), pt2)) == RTNORM) // "\nPunto seguente: "
   {
      if ((points += acutBuildList(RTPOINT, pt2, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      acedGrDraw(pt1, pt2, -1, 1);
      ads_point_set(pt2, pt1);
   }

   // chiudo poligono
   if (acedGrDraw(pnt, points.get_tail()->resval.rpoint, -1, 1) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (ret == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;

   CoordList << &points;
   points.remove_all_without_dealloc();

   // disegno il poligono
   gsc_grdraw(CoordList.get_head(), 1, 1);
   // chiudo poligono
   acedGrDraw(pnt, CoordList.get_tail()->resval.rpoint, 1, 1);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_EraseConsecutiveDuplicatedPts
/*+
  Questa funzione cancella un punto qualora il suo precedente abbia le 
  stesse coordinate 2D.
  Ciò viene fatto perchè le funzioni di autocad di ricerca oggetti tramite
  ssget non funzionano se trovano nella lista dei punti due punti coincidenti
  uno consecutivo all'altro.
  Parametri:
  C_RB_LIST &CoordList;   Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
void gsc_EraseConsecutiveDuplicatedPts(C_RB_LIST &CoordList)
{
   ads_point pt1;
   presbuf   pRb;
   bool      Changed = false, toCopy;
   C_RB_LIST NoDuplCoordList;

   // Cerco primo punto
   pRb = CoordList.get_head();
   while (pRb)
   {
      NoDuplCoordList += gsc_copybuf(pRb);
      if (pRb->restype == RTPOINT || pRb->restype == RT3DPOINT) break;
      pRb = pRb->rbnext;
   }

   if (!pRb) return;

   ads_point_set(pRb->resval.rpoint, pt1);
   // Cerco punti successivi
   pRb = pRb->rbnext;
   while (pRb)
   {
      toCopy = true;

      if (pRb->restype == RTPOINT || pRb->restype == RT3DPOINT)
      {
         if (gsc_2Dpoint_equal(pt1, pRb->resval.rpoint))
         {
            Changed = true;
            toCopy = false;
         }
         ads_point_set(pRb->resval.rpoint, pt1);
      }

      if (toCopy)
         NoDuplCoordList += gsc_copybuf(pRb);
      pRb = pRb->rbnext;
   }

   if (Changed) NoDuplCoordList.copy(CoordList);
}


/*****************************************************************************/
/*.doc gsc_selObjsPolygon                                                    */
/*+
  Questa funzione effettua la selezione degli oggetti in una poligono.
  Parametri:
  C_RB_LIST &CoordList;    Lista delle coordinate
  int type;                flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  ads_name ss;             Gruppo di selezione (non allocato)
  int ExactMode;           Controllo su coordinate di ogni oggetto eseguito solo
                           se type = INSIDE (default = GS_BAD)
  int CheckOn2D;           Controllo oggetti senza usare la Z (default = GS_GOOD) 
                           usato solo se ExactMode = GS_GOOD
  int OnlyInsPt;           Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
                           usato solo se ExactMode = GS_GOOD
  int Cls;                 Per cercare solo oggetti di una certa classe (default = 0)
  int Sub;                 Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int ObjType;             Tipo di oggetti grafici (default = GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsPolygon(C_RB_LIST &CoordList, int type, C_SELSET &ss, int ExactMode,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   ads_point pt1, pt2, WinPt1, WinPt2;

   // Elimino punti consecutivi uguali
   gsc_EraseConsecutiveDuplicatedPts(CoordList);

   // Se il poligono non è completamente a video devo fare uno zoom per includerlo
   if (gsc_IsWindowContainedInCurrentScreen(CoordList.get_head(), true, 
                                            WinPt1, WinPt2, 0.0, pt1, pt2) == GS_BAD)
      gsc_zoom(WinPt1, WinPt2);

   // La selezione _WP ritorna esclusivamente gli oggetti completamente interni
   // al poligono e non quelli che intersecano anche in un solo punto il poligono stesso
   // in questo modo, ad esempio, tutte le polilinee che seppure interne al poligono
   // ma con il vertice iniziale o finale sul poligono vengono scartate da questo tipo
   // di selezione. Quindi faccio sempre la selezione _CP e poi scarto gli oggetti esterni.
   //if (gsc_ssget((type == INSIDE) ? "_WP" : "_CP", CoordList.get_head(), NULL, NULL, ss) != RTNORM)
   if (gsc_ssget(_T("_CP"), CoordList.get_head(), NULL, NULL, ss) != RTNORM)
      return GS_CAN;

   if (Cls > 0) ss.intersectClsCode(Cls, Sub, ObjType);
   else ss.intersectType(ObjType);

   if (ExactMode == GS_GOOD) //  && type == INSIDE
   {
      // poichè ssget si basa sulla mappa video (la precisione dipende dal livello
      // di zoom corrente) può ritornare oggetti che non sono effettivamente
      // corrispondenti al tipo di selezione ma sono molto vicini
      AcGePoint3dArray Vertices;
      presbuf          p;
      int              i = 0;
      
      // Includo tutti i testi e i blocchi che sono con punto di inserimento
      // interno all'area
      C_SELSET PunctualSS;

      if (gsc_getNodesInWindow(pt1, pt2, PunctualSS, CheckOn2D) == GS_BAD) return GS_BAD;
      if (Cls > 0) PunctualSS.intersectClsCode(Cls, Sub, ObjType);
      else PunctualSS.intersectType(ObjType);

      ss.add_selset(PunctualSS);

      Vertices.setLogicalLength(CoordList.GetCount());
      p = CoordList.get_head();
      while (p)
      {
         Vertices[i++].set(p->resval.rpoint[X], p->resval.rpoint[Y], // X, Y
                           (CheckOn2D == GS_GOOD) ? 0.0 : p->resval.rpoint[Z]); // Z
         p = CoordList.get_next();
      }  

      AcDb3dPolyline dummyPline(AcDb::k3dSimplePoly, Vertices, Adesk::kTrue); // closed
      if (ss.SpatialInternalEnt(&dummyPline, CheckOn2D, OnlyInsPt, type) == GS_BAD)
         { gsc_ReleaseSubEnts(&dummyPline); return GS_BAD; }

      // roby 2017
      if (type == INSIDE) // devo verificare che tutte le componenti geometriche delle entità siano completamente interne
      {
         ads_name ent;
         C_SELSET entSelSet;
         int      i = 0, n, j;
         C_CLASS  *pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub);

         while (ss.entname(i++, ent) == GS_GOOD)
            if (pCls->get_SelSet(ent, entSelSet, ObjType) == GS_GOOD)
            {
               n = entSelSet.length();
               // rimuovo i riempimenti
               j = 0;
               while (entSelSet.entname(j++, ent) == GS_GOOD)
                  if (gsc_ishatch(ent) == GS_GOOD)
                     { entSelSet.subtract_ent(ent); j--; }

               if (entSelSet.SpatialInternalEnt(&dummyPline, CheckOn2D, OnlyInsPt, type) == GS_GOOD)
                  if (n > entSelSet.length())
                     { ss.subtract(entSelSet); i--; } // tolgo l'entità da SelSet
            }
      }

      gsc_ReleaseSubEnts(&dummyPline);
   }

   return GS_GOOD;
}
int gsc_selObjsPolygon(C_RB_LIST &CoordList, int type, ads_name ss, int ExactMode,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   C_SELSET InternalSS;
   int      Result = gsc_selObjsPolygon(CoordList, type, InternalSS, ExactMode, CheckOn2D, OnlyInsPt,
                                        Cls, Sub, ObjType);

   InternalSS.get_selection(ss);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return Result;
}


/*****************************************************************************/
/*.doc gsc_draggen_circle                                                    */
/*+
  Questa funzione effettua il draggen di un cerchio.
  Parametri:
  ads_point pt;   punto attuale
  ads_matrix mt;  matrice di traslazione

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*****************************************************************************/
int gsc_draggen_circle(ads_point pt, ads_matrix mt)
{
   presbuf   prb;

   LAST_POINTS << &DRAGGEN_POINTS;
   DRAGGEN_POINTS.remove_all_without_dealloc();

   gsc_mat_ident(mt);

   RAY = gsc_dist(CENTER, pt);
   if (gsc_circle_point(CENTER, RAY, 100, &prb) == GS_BAD) 
   {
      gsc_grdraw(LAST_POINTS.get_head(), -1, 1);
      return RTERROR;
   }
   DRAGGEN_POINTS.remove_all_without_dealloc();

   DRAGGEN_POINTS << prb;
   DRAGGEN_POINTS += acutBuildList(RTPOINT, prb->resval.rpoint, 0);

   gsc_grdraw(LAST_POINTS.get_head(), -1, 1);
   gsc_grdraw(DRAGGEN_POINTS.get_head(), -1, 1);

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_select_circle                                                     */
/*+
  Questa funzione effettua la selezione di una cerchio esistente.
  Parametri:
  C_RB_LIST &CoordList;   Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_select_circle(C_RB_LIST &CoordList)
{
   C_RB_LIST  DescrEnt;
   ads_point  pnt;
   ads_name   ent;
   int        ret;
   presbuf    prb;

   do
   {
      acutPrintf(gsc_msg(96)); // "\nSeleziona oggetto: "
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      while ((ret = acedEntSel(GS_EMPTYSTR, ent, pnt)) == RTERROR);
      if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ret == RTCAN) return GS_CAN;

      if ((DescrEnt << acdbEntGet(ent)) == NULL)
         { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

      if ((prb = DescrEnt.SearchType(0)) == NULL || prb->resval.rstring == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

      // se è cerchio
      if (gsc_strcmp(prb->resval.rstring, CIRCLE_SPATIAL_COND, FALSE) == 0) break;  // cerchio

      acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
   }
   while (1);

   // centro
   if ((prb = DescrEnt.SearchType(10)) == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   ads_point_set(prb->resval.rpoint, CENTER);
   // raggio
   if ((prb = DescrEnt.SearchType(40)) == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   RAY = prb->resval.rreal;

   if ((CoordList << acutBuildList(RTPOINT, CENTER, RTREAL, RAY, 0)) == NULL) return GS_BAD;

   // disegna cerchio
   if (gsc_circle_point(CENTER, RAY, 100, &prb) == GS_GOOD)
   {
      // prima cancello il cerchio (per baco acad di refresh video)
      gsc_grdraw(prb, -1, 1);

      presbuf Tail = prb;
      
      while (Tail->rbnext) Tail = Tail->rbnext;
      Tail->rbnext = acutBuildList(RTPOINT, prb->resval.rpoint, 0);
      
      gsc_grdraw(prb, 1, 1);
      acutRelRb(prb);
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_select_circle                                                     */
/*+
  Questa funzione effettua la definizione di un nuovo cerchio.
  Parametri:
  C_RB_LIST &CoordList;   Lista delle coordinate

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_define_circle(C_RB_LIST &CoordList)
{
   C_RB_LIST DescrEnt;
   ads_point pnt;
   double    PrevRay = 0;
   ads_name  ent, SelSet;
   int       ret;
   C_STRING  CenterMsg, RayMsg;
   
   CenterMsg = gsc_msg(353); // "\nCentro: "
   RayMsg    = gsc_msg(265); // "\nRaggio: "

   // cerchio già definito
   if (CoordList.GetCount() > 0)
   {
      TCHAR   *str = NULL;
      presbuf prb;

      acutPrintf(gsc_msg(90)); // "\nModificare il cerchio:"
      if ((prb = CoordList.get_head()) == NULL ||
          (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      ads_point_set(prb->resval.rpoint, CENTER);

      if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
      CenterMsg += _T("<");
      CenterMsg += str;
      CenterMsg += _T(">: ");
      free(str);

      if ((prb = CoordList.get_next()) == NULL || prb->restype != RTREAL)
         { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
      PrevRay = RAY = prb->resval.rreal;

      if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
      RayMsg += _T("<");
      RayMsg += str;
      RayMsg += _T(">: ");
      free(str);
   }

   if ((ret = acedGetPoint(NULL, CenterMsg.get_name(), pnt)) == RTERROR)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;
   if (ret != RTNONE) ads_point_set(pnt, CENTER);

   // Serve solo per creare un gruppo di selezione non vuoto.
   // E' un trucco per utilizzare il draggen
   if ((DescrEnt << acutBuildList(RTDXF0, _T("POINT"), 10, CENTER, 0)) == NULL)
      return GS_BAD;
   if (ads_entmake(DescrEnt.get_head()) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (acdbEntLast(ent) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (acedSSAdd(ent, NULL, SelSet) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; gsc_EraseEnt(ent); return GS_BAD; }

   DRAGGEN_POINTS.remove_all();
   LAST_POINTS.remove_all();

   acedInitGet(RSG_OTHER, GS_EMPTYSTR);
   ret = ads_draggen(SelSet, RayMsg.get_name(), 0, gsc_draggen_circle, pnt);
   
   gsc_EraseEnt(ent);
   ads_ssfree(SelSet);
   LAST_POINTS.remove_all();

   switch (ret)
   {
      case RTNONE:
         if (PrevRay > 0) RAY = PrevRay;
         break;
      case RTNORM:
         break;
      case RTCAN:
         DRAGGEN_POINTS.remove_all();
         return GS_CAN;
      case RTSTR:
      {
         TCHAR   str[133];
         presbuf prb;
         
         RAY = 0;
         while (RAY <= 0)
         {
            if ((ret = acedGetInput(str)) == RTERROR)
               { DRAGGEN_POINTS.remove_all(); return GS_BAD; }
            if (ret == RTCAN) { DRAGGEN_POINTS.remove_all(); return GS_CAN; }
            RAY = _wtof(str);
         }
         if (gsc_circle_point(CENTER, RAY, 100, &prb) == GS_BAD)
            { DRAGGEN_POINTS.remove_all(); return GS_BAD; }
         DRAGGEN_POINTS << prb;
         DRAGGEN_POINTS += acutBuildList(RTPOINT, prb->resval.rpoint, 0);
         break;
      }
      default:
         { DRAGGEN_POINTS.remove_all(); return GS_BAD; }
   }

   if ((CoordList << acutBuildList(RTPOINT, CENTER, RTREAL, RAY, 0)) == NULL)
      return GS_BAD;

   // disegna cerchio
   gsc_grdraw(DRAGGEN_POINTS.get_head(), 1, 1);
   DRAGGEN_POINTS.remove_all();

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_selObjsCircle                                                     */
/*+
  Questa funzione effettua la selezione degli oggetti in un cerchio.
  Parametri:
  C_RB_LIST &CoordList;    lista di resbuf: 1 elemento = centro, 2 elemento = raggio
  int type;                flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  ads_name ss;             Gruppo di selezione (non allocato)
  int CheckOn2D;           Controllo oggetti senza usare la Z (default = GS_GOOD) 
  int OnlyInsPt;           Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
  int      Cls;            Per cercare solo oggetti di una certa classe (default = 0)
  int      Sub;            Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int      ObjType;        Tipi di oggetti da cercare (default GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsCircle(C_RB_LIST &CoordList, int type, C_SELSET &ss,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   ads_point center;
   double    radius;
   presbuf   prb;
   ads_point pt1, pt2, WinPt1, WinPt2;

   if ((prb = CoordList.get_head()) == NULL ||
       (prb->restype != RTPOINT && prb->restype != RT3DPOINT))
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   ads_point_set(prb->resval.rpoint, center);
   if ((prb = CoordList.get_next()) == NULL || prb->restype != RTREAL)
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   radius = prb->resval.rreal;

   pt1[X] = center[X] - radius; pt1[Y] = center[Y] - radius; pt1[Z] = center[Z];
   pt2[X] = center[X] + radius; pt2[Y] = center[Y] + radius; pt2[Z] = center[Z];

   ads_point_set(pt1, WinPt1);
   ads_point_set(pt2, WinPt2);

   // Se il cerchio non è completamente a video devo fare uno zoom per includerlo
   if (gsc_IsWindowContainedInCurrentScreen(WinPt1, WinPt2) == GS_BAD)
      gsc_zoom(WinPt1, WinPt2);

   // Elimina il primo punto perche' uguale all'ultimo altrimenti gsc_ssget fallisce.
   if (gsc_ssget((type == INSIDE) ? _T("_W") : _T("_C"), pt1, pt2, NULL, ss) != RTNORM)
      return GS_CAN;
   
   if (Cls > 0) ss.intersectClsCode(Cls, Sub, ObjType);
   else ss.intersectType(ObjType);

   // poichè ssget si basa sulla mappa video (la precisione dipende dal livello
   // di zoom corrente) può ritornare oggetti che non sono effettivamente
   // corrispondenti al tipo di selezione ma sono molto vicini
   
   // Includo tutti i testi e i blocchi che sono con punto di inserimento
   // interno all'area
   C_SELSET PunctualSS;
   if (gsc_getNodesInWindow(pt1, pt2, PunctualSS, CheckOn2D) == GS_BAD) return GS_BAD;
   if (Cls > 0) PunctualSS.intersectClsCode(Cls, Sub, ObjType);
   else PunctualSS.intersectType(ObjType);

   ss.add_selset(PunctualSS);
   
   AcGePoint3d cntr;

   cntr.set(center[X], center[Y], // X, Y
            (CheckOn2D == GS_GOOD) ? 0.0 : center[Z]); // Z

   AcDbCircle dummyCircle(cntr, AcGeVector3d(0.0, 0.0, 1.0), radius);
   if (ss.SpatialInternalEnt(&dummyCircle, CheckOn2D, OnlyInsPt, type) == GS_BAD)
      return GS_BAD;

   // roby 2017
   if (type == INSIDE) // devo verificare che tutte le componenti geometriche delle entità siano completamente interne
   {
      ads_name ent;
      C_SELSET entSelSet;
      int      i = 0, n, j;
      C_CLASS  *pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub);

      while (ss.entname(i++, ent) == GS_GOOD)
         if (pCls->get_SelSet(ent, entSelSet, ObjType) == GS_GOOD)
         {
            n = entSelSet.length();
            // rimuovo i riempimenti
            j = 0;
            while (entSelSet.entname(j++, ent) == GS_GOOD)
               if (gsc_ishatch(ent) == GS_GOOD)
                  { entSelSet.subtract_ent(ent); j--; }

            if (entSelSet.SpatialInternalEnt(&dummyCircle, CheckOn2D, OnlyInsPt, type) == GS_GOOD)
               if (n > entSelSet.length())
                  { ss.subtract(entSelSet); i--; } // tolgo l'entità da SelSet
         }
   }

   
   return GS_GOOD;
}
int gsc_selObjsCircle(C_RB_LIST &CoordList, int type, ads_name ss,
                      int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{
   C_SELSET InternalSS;
   int      Result = gsc_selObjsCircle(CoordList, type, InternalSS, CheckOn2D, OnlyInsPt,
                                       Cls, Sub, ObjType);

   InternalSS.get_selection(ss);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return Result;
}


///////////////////////////////////////////////////////////////////////////////
//                FINE     FUNZIONI       CIRCLE                             //
//                INIZIO   FUNZIONI       FENCE                              //
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_select_fence                                                      */
/*+
  Questa funzione effettua la selezione di una fence esistente (oggetti 
  intersecanti una polyline).
  Parametri: 
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<flag aperta o chiusa> <piano> <bulge1> <punto1> <bulge2> <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_select_fence(C_RB_LIST &CoordList)
{
   C_POINT_LIST PntList;
   ads_point    pnt;
   ads_name     ent;
   int          ret;
   bool         Closed;
   C_RB_LIST    VertexList;

   do
   {
      acutPrintf(gsc_msg(96)); // "\nSeleziona oggetto: "
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      while ((ret = acedEntSel(GS_EMPTYSTR, ent, pnt)) == RTERROR);
      if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ret == RTCAN) return GS_CAN;

      // se è una polilinea o lwpolilinea con vertici validi
      if (PntList.add_vertex_point(ent, GS_GOOD) == GS_GOOD &&
          PntList.get_count() > 0) break;

      acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
   }
   while (1);

   pnt[X] = pnt[Y] = 0.0; pnt[Z] = 1;
   Closed = (gsc_isClosedPline(ent) == GS_GOOD) ? true : false;
   if (CoordList << acutBuildList(RTSHORT, (Closed) ? 1 : 0,
                                  RT3DPOINT, pnt, 0) == NULL)
      return GS_BAD;

   // scrivo anche i bulge
   if ((VertexList << PntList.to_rb(true)) == NULL) return GS_BAD;

   // prima cancello la polilinea (per baco acad di refresh video)
   gsc_grdraw(VertexList.get_head(), -1, 1, Closed);

   // disegno la polilinea fence
   gsc_grdraw(VertexList.get_head(), 1, 1, Closed);

   CoordList += VertexList;
   VertexList.remove_all_without_dealloc();

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_define_fence                                                      */
/*+
  Questa funzione effettua la definizione di una nuova fence.
  Parametri:
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<flag aperta o chiusa> <piano> [<bulge1>] <punto1> [<bulge2>] <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_define_fence(C_RB_LIST &CoordList)
{
   ads_point pt1, pt2;
   C_RB_LIST points;
   presbuf   prb;
   int       ret;

   // poligono già definito
   if ((prb = CoordList.get_head()) != NULL)
   {
      C_STRING Msg;
      TCHAR    *str = NULL;
      int      stop = FALSE;

      acutPrintf(gsc_msg(650)); // "\nModificare l'intercettazione:"
      
      if (!(prb = prb->rbnext)) return GS_BAD ; // salto flag aperta o chiusa
      if (!(prb = prb->rbnext)) return GS_BAD ; // salto piano

      pt1[X] = pt1[Y] = 0; pt1[Z] = 1;
      if ((points << acutBuildList(RTSHORT, 0, RT3DPOINT, pt1, 0)) == NULL)
         return GS_BAD;

      // primo punto
      while (prb)
      {
         if (prb->restype != RTPOINT && prb->restype != RT3DPOINT)
            if (!(prb = prb->rbnext)) return GS_BAD ; // salto il bulge

         Msg = gsc_msg(647); // "Primo punto o C per cancellare: "
         if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
         Msg += _T("<");
         Msg += str;
         Msg += _T(">: ");
         free(str);

         acedInitGet(0, gsc_msg(649)); // "C"
         if ((ret = acedGetPoint(prb->resval.rpoint, Msg.get_name(), pt1)) == RTERROR)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         switch (ret)
         {
            case RTCAN: return GS_CAN;
            case RTNORM:
               stop = TRUE;
               break;
            case RTNONE:
               ads_point_set(prb->resval.rpoint, pt1);
               stop = TRUE;
               break;
         }
         if (stop) break;

         prb = prb->rbnext;
      }
      if ((points += acutBuildList(RTPOINT, pt1, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      // punti successivi
      while ((prb = prb->rbnext) != NULL)
      {
         if (prb->restype != RTPOINT && prb->restype != RT3DPOINT)
            if (!(prb = prb->rbnext)) return GS_BAD ; // salto il bulge

         Msg = gsc_msg(648); // "\nPunto seguente o C per cancellare: "
         if ((str = gsc_rb2str(prb)) == NULL) return GS_BAD;
         Msg += _T("<");
         Msg += str;
         Msg += _T(">: ");
         free(str);

         acedInitGet(0, gsc_msg(649)); // "C"
         if ((ret = acedGetPoint(pt1, Msg.get_name(), pt2)) == RTERROR)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         switch (ret)
         {
            case RTCAN: return GS_CAN;
            case RTNONE:
               ads_point_set(prb->resval.rpoint, pt2);
            case RTNORM:
               if ((points += acutBuildList(RTPOINT, pt2, 0)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               acedGrDraw(pt1, pt2, -1, 1);
               ads_point_set(pt2, pt1);
               break;
         }
      }
   }
   else
   {
      // polilinea aperta e vettore normale asse Z
      pt1[X] = pt1[Y] = 0; pt1[Z] = 1;
      if ((points << acutBuildList(RTSHORT, 0, RT3DPOINT, pt1, 0)) == NULL)
         return GS_BAD;
      if ((ret = acedGetPoint(NULL, gsc_msg(351), pt1)) == RTERROR) // "\nPrimo punto: "
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ret == RTCAN) return GS_CAN;
      if ((points += acutBuildList(RTPOINT, pt1, 0)) == NULL) // bulge e punto
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   }
   
   while ((ret = acedGetPoint(pt1, gsc_msg(352), pt2)) == RTNORM) // "\nPunto seguente: "
   {
      if ((points += acutBuildList(RTPOINT, pt2, 0)) == NULL) // bulge e punto
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      acedGrDraw(pt1, pt2, -1, 1);
      ads_point_set(pt2, pt1);
   }
   
   if (ret == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;

   CoordList << &points;
   points.remove_all_without_dealloc();

   // disegno il poligono   
   if ((prb = CoordList.get_head()) && (prb = prb->rbnext) && (prb = prb->rbnext))
      gsc_grdraw(prb, 1, 1);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_selObjsFence                                                      */
/*+
  Questa funzione effettua la selezione degli oggetti in una fence.
  Parametri:
  C_RB_LIST &CoordList; Lista delle coordinate 
                        (<flag aperta o chiusa> <piano> <bulge1> <punto1> <bulge2> <punto2> ...)
                        <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                        <piano> vettore normale che identifica l'asse Z (0 0 1)
  ads_name ss;          Gruppo di selezione (non allocato)
  int OnlyInsPt;        Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
                        usato solo se ExactMode = GS_GOOD
  int Cls;              Per cercare solo oggetti di una certa classe (default = 0)
  int Sub;              Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int ObjType;          Tipo di oggetti grafici (default = GRAPHICAL)
  bool IncludeFirstLast; Nel controllo considera anche se l'intersezione è 
                         esattamente il punto iniziale o finale della linea
                         (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsFence(C_RB_LIST &CoordList, C_SELSET &ss, int CheckOn2D, int OnlyInsPt,
                     int Cls, int Sub, int ObjType, bool IncludeFirstLast)
{
   ads_point pt1, pt2, WinPt1, WinPt2;
   int       Closed;
   C_SELSET  PunctualSS;
   presbuf   p, pStart;
   int       nVertex = 1, i = 0;
   bool      _3D = false;
   double    Bulge;

   if (!(p = CoordList.get_head())) return GS_BAD;
   if (gsc_rb2Int(p, &Closed) == GS_BAD) return GS_BAD; // leggo flag aperta/chiusa

   if (!(p = p->rbnext)) return GS_BAD;
   if (!(p = p->rbnext)) return GS_BAD; // salto il vettore normale dell'asse Z
   pStart = p;

   // Se la finestra non è completamente a video devo fare uno zoom per includerla
   if (gsc_IsWindowContainedInCurrentScreen(p, (Closed == 1) ? true : false,
                                            WinPt1, WinPt2, 0.0, pt1, pt2) == GS_BAD)
      gsc_zoom(WinPt1, WinPt2);

   if (gsc_ssget(_T("_C"), pt1, pt2, NULL, ss) != RTNORM) return GS_CAN;

   if (Cls > 0) ss.intersectClsCode(Cls, Sub, ObjType);
   else ss.intersectType(ObjType);

   // Includo tutti i testi e i blocchi che sono con punto di inserimento
   // interno all'area
   if (gsc_getNodesInWindow(pt1, pt2, PunctualSS, CheckOn2D) == GS_BAD) return GS_BAD;
   if (Cls > 0) PunctualSS.intersectClsCode(Cls, Sub, ObjType);
   else PunctualSS.intersectType(ObjType);

   ss.add_selset(PunctualSS);

   // Conto i vertici e verifico se ci sono Z diverse
   if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
   else if (!(p = p->rbnext)) return GS_BAD;

   if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
   ads_point_set(p->resval.rpoint, pt1);

   while ((p = p->rbnext))
   {
      if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
      else if (!(p = p->rbnext)) return GS_BAD;

      if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
      ads_point_set(p->resval.rpoint, pt2);

      if (pt1[Z] != pt2[Z]) _3D = true;

      ads_point_set(pt2, pt1);
      nVertex++;
   }

      p = pStart;
      if (_3D)
      {
         AcGePoint3dArray Vertices;
         Vertices.setLogicalLength(nVertex);

         while (p)
         {
            if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
            else if (!(p = p->rbnext)) return GS_BAD;

            Vertices[i++].set(p->resval.rpoint[X], p->resval.rpoint[Y], // X, Y
                              (CheckOn2D == GS_GOOD) ? 0.0 : p->resval.rpoint[Z]); // Z
            p = p->rbnext;
         }  

         AcDb3dPolyline dummyPline(AcDb::k3dSimplePoly, Vertices,
                                   (Closed == 1) ? Adesk::kTrue : Adesk::kFalse); // closed
         if (ss.SpatialIntersectEnt(&dummyPline, CheckOn2D, OnlyInsPt, GS_BAD,
                                    IncludeFirstLast) == GS_BAD)
            { gsc_ReleaseSubEnts(&dummyPline); return GS_BAD; }
         gsc_ReleaseSubEnts(&dummyPline);
      }
      else
      {
         AcDb2dVertex Vertex;
         AcGePoint2d  dummyPt;
         AcDbPolyline dummyPline;
         double       Elev;

         while (p)
         {
            if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
            else if (!(p = p->rbnext)) return GS_BAD;

            AcGePoint2d_set_from_ads_point(p->resval.rpoint, dummyPt);
            Elev = p->resval.rpoint[Z];
            dummyPline.addVertexAt(i++, dummyPt, Bulge);

            p = p->rbnext;
         }  

         if (Closed == 1) dummyPline.setClosed(Adesk::kTrue);
         dummyPline.setElevation((CheckOn2D == GS_GOOD) ? 0.0 : Elev);
         if (ss.SpatialIntersectEnt(&dummyPline, CheckOn2D, OnlyInsPt, GS_BAD,
                                    IncludeFirstLast) == GS_BAD)
            return GS_BAD;
      }
   
   return GS_GOOD;
}
int gsc_selObjsFence(C_RB_LIST &CoordList, ads_name ss, int CheckOn2D = GS_GOOD,
                     int OnlyInsPt = GS_GOOD, int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL,
                     bool IncludeFirstLast = true)
{
   C_SELSET InternalSS;
   int      Result = gsc_selObjsFence(CoordList, InternalSS, CheckOn2D, OnlyInsPt,
                                      Cls, Sub, ObjType, IncludeFirstLast);

   InternalSS.get_selection(ss);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return Result;
}


///////////////////////////////////////////////////////////////////////////////
//                FINE     FUNZIONI       FENCE                              //
//                INIZIO   FUNZIONI       BUFFER-FENCE                       //
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_select_bufferfence                                                */
/*+
  Questa funzione effettua la selezione di una fence esistente (oggetti 
  intersecanti una polyline) e di una distanza.
  Parametri: 
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<offset> <flag aperta-chiusa> <piano> [<bulge1>] <punto1> [<bulge2>] <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_select_bufferfence(C_RB_LIST &CoordList)
{
   int      ret;
   double   OffSet = 0.0;
   presbuf  p;
   C_STRING Msg;

   if ((ret = gsc_select_fence(CoordList)) != GS_GOOD) return ret;
   
   Msg = gsc_msg(74); // "\nLarghezza buffer <"
   Msg += OffSet;
   Msg += _T(">: ");
   acedInitGet(RSG_OTHER, NULL);
   ret = acedGetDist(NULL, Msg.get_name(), &OffSet);
   if (ret != RTNORM && ret != RTNONE)
      { gsc_grdraw(CoordList.get_head(), -1, 1); CoordList.remove_all(); return GS_CAN; }

   if ((p = acutBuildList(RTREAL, OffSet, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   CoordList.link_head(p);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_define_bufferfence                                                */
/*+
  Questa funzione effettua la definizione di una nuova fence.
  Parametri:
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<offset> <flag aperta-chiusa> <piano> [<bulge1>] <punto1> [<bulge2>] <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_define_bufferfence(C_RB_LIST &CoordList)
{
   double    OffSet = 0.0;
   int       ret;
   presbuf   p;
   C_RB_LIST _CoordList;
   C_STRING  Msg;

   if ((p = CoordList.get_head()))
   {
      gsc_rb2Dbl(p, &OffSet);
      CoordList.copy(_CoordList);
      _CoordList.remove_head(); // elimino offset
   }
   if ((ret = gsc_define_fence(_CoordList)) != GS_GOOD) return ret;

   Msg = gsc_msg(74); // "\nLarghezza buffer <"
   Msg += OffSet;
   Msg += _T(">: ");
   acedInitGet(RSG_OTHER, NULL);
   ret = acedGetDist(NULL, Msg.get_name(), &OffSet);
   if (ret != RTNORM && ret != RTNONE)
      { gsc_grdraw(_CoordList.get_head(), -1, 1); CoordList.remove_all(); return GS_CAN; }

   _CoordList.copy(CoordList);
   if ((p = acutBuildList(RTREAL, OffSet, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }  
   CoordList.link_head(p);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_addObjsBufferFence                                  <internal>    */
/*+
  Questa funzione è di ausilio alla gsc_selObjsBufferFence e aggiunge
  gli oggetti che sono entro un buffer di un segmento rettilineo o un arco.
  Parametri:
  C_SELSET &ss;      Gruppo di oggetti
  ads_point pt1;     Primo punto del segmento
  ads_point pt2;     Secondo punto del segmento
  double Bulge;      tangente di 1/4 angolo interno
  double OffSet;     Distanza
  int type;          flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  int Cls;
  int Sub;
  int CheckOn2D;     Controllo oggetti senza usare la Z (default = GS_GOOD) 
  int OnlyInsPt;     Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
  C_SELSET &ResultSS; Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_addObjsBufferFence(C_SELSET &ss, ads_point pt1, ads_point pt2, double Bulge,
                           double OffSet, int type, int Cls, int Sub, int CheckOn2D, int OnlyInsPt,
                           C_SELSET &ResultSS)
{
   AcGePoint3d  Vertex, NextVertex;
   AcDbPolyline *pExtBuffer, *pIntBuffer;
   int          res = GS_GOOD;
   C_SELSET     PartialSS, SubtractSS;
   
   Vertex.set(pt1[X], pt1[Y], pt1[Z]);
   NextVertex.set(pt2[X], pt2[Y], pt2[Z]);

   // calcolo l'area di offset
   do
   {
      if (Bulge == 0) // segmento rettilineo
      {
         res = gsc_getBufferOnLine(Vertex, NextVertex, OffSet, &pExtBuffer);
         pIntBuffer = NULL;
      }
      else // arco
      {
         if (Bulge < 0) // senso orario
            res = gsc_getBufferOnArc(NextVertex, Vertex, -1 * Bulge, OffSet,
                                     &pExtBuffer, &pIntBuffer);
         else // senso antiorario
            res = gsc_getBufferOnArc(Vertex, NextVertex, Bulge, OffSet,
                                     &pExtBuffer, &pIntBuffer);
      }
      if (res != GS_GOOD) break;

      // verifico gli oggetti per l'area di offset
      ss.copy(PartialSS);
      if ((res = PartialSS.SpatialInternalEnt(pExtBuffer, CheckOn2D, OnlyInsPt, type)) == GS_BAD)
         break;

      if (pIntBuffer)
      {
         PartialSS.copy(SubtractSS);
         if ((res = SubtractSS.SpatialInternalEnt(pIntBuffer, CheckOn2D, OnlyInsPt, INSIDE)) == GS_BAD)
            break;
         PartialSS.subtract(SubtractSS);
      }

      ResultSS.add_selset(PartialSS);
   }
   while (0);

   // roby 2017
   if (type == INSIDE) // devo verificare che tutte le componenti geometriche delle entità siano completamente interne
   {
      ads_name ent;
      C_SELSET entSelSet;
      int      i = 0, n, j;
      C_CLASS  *pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub);

      while (ResultSS.entname(i++, ent) == GS_GOOD)
         if (pCls->get_SelSet(ent, entSelSet, GRAPHICAL) == GS_GOOD)
         {
            n = entSelSet.length();
            // rimuovo i riempimenti
            j = 0;
            while (entSelSet.entname(j++, ent) == GS_GOOD)
               if (gsc_ishatch(ent) == GS_GOOD)
                  { entSelSet.subtract_ent(ent); j--; }

            if (entSelSet.SpatialInternalEnt(pExtBuffer, CheckOn2D, OnlyInsPt, type) == GS_GOOD)
            {
               if (pIntBuffer)
               {
                  entSelSet.copy(SubtractSS);
                  if ((res = SubtractSS.SpatialInternalEnt(pIntBuffer, CheckOn2D, OnlyInsPt, INSIDE)) == GS_BAD)
                     break;
                  entSelSet.subtract(SubtractSS);
               }
               
               if (n > entSelSet.length())
                  { ResultSS.subtract(entSelSet); i--; } // tolgo l'entità da SelSet
            }
         }
   }

   pExtBuffer->close();
   delete pExtBuffer;
   if (pIntBuffer)
   {
      pIntBuffer->close();
      delete pIntBuffer;
   }
   
   return res;
}


/*****************************************************************************/
/*.doc gsc_selObjsBufferFence                                                */
/*+
  Questa funzione effettua la selezione degli oggetti in un buffer fence.
  Parametri:
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<offset> <flag aperta o chiusa> <piano> <bulge1> <punto1> <bulge2> <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)
  int type;                Flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  ads_name ss;             Gruppo di selezione (non allocato)
  int CheckOn2D;           Controllo oggetti senza usare la Z (default = GS_GOOD) 
  int OnlyInsPt;           Per i blocchi usare solo il punto di inserimento (default = GS_GOOD)
  int Cls;                 Per cercare solo oggetti di una certa classe (default = 0)
  int Sub;                 Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int ObjType;             Tipo di oggetti grafici (default = GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsBufferFence(C_RB_LIST &CoordList, int type, C_SELSET &ss,
                           int CheckOn2D, int OnlyInsPt, int Cls, int Sub, int ObjType)
{  
   ads_point pt1, pt2, ptStart, WinPt1, WinPt2;
   presbuf   p;
   int       res = GS_GOOD, Closed;
   C_SELSET  ResultSS, PunctualSS;
   double    Bulge1, Bulge2, OffSet;

   if (!(p = CoordList.get_head())) return GS_BAD;
   if (gsc_rb2Dbl(p, &OffSet) == GS_BAD) return GS_BAD; // leggo offset
   if (!(p = p->rbnext)) return GS_BAD;
   if (gsc_rb2Int(p, &Closed) == GS_BAD) return GS_BAD; // leggo flag aperta/chiusa

   if (!(p = p->rbnext)) return GS_BAD;
   if (!(p = p->rbnext)) return GS_BAD; // salto il vettore normale dell'asse Z

   ss.clear();
   // Se la finestra non è completamente a video devo fare uno zoom per includerla
   if (gsc_IsWindowContainedInCurrentScreen(p, (Closed == 1) ? true : false,
                                            WinPt1, WinPt2, OffSet, pt1, pt2) == GS_BAD)
      gsc_zoom(WinPt1, WinPt2);

   if (gsc_ssget((type == INSIDE) ? _T("_W") : _T("_C"), pt1, pt2, NULL, ResultSS) != RTNORM)
      return GS_CAN;

   // Includo tutti i testi e i blocchi che sono con punto di inserimento
   // interno all'area
   if (gsc_getNodesInWindow(pt1, pt2, PunctualSS, CheckOn2D) == GS_BAD) return GS_BAD;
   ResultSS.add_selset(PunctualSS);

   if (Cls > 0)
      ResultSS.intersectClsCode(Cls, Sub, ObjType);
   else
      ResultSS.intersectType(ObjType);

   // Se ResultSS è vuoto posso uscire senza fare ulteriori controlli
   if (ResultSS.length() <= 0) return GS_GOOD;

   // Per ogni tratto della polilinea verifico gli oggetti
   if (gsc_rb2Dbl(p, &Bulge1) == GS_BAD) Bulge1 = 0.0; // potrebbe non esserci
   else if (!(p = p->rbnext)) return GS_BAD;

   if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
   ads_point_set(p->resval.rpoint, pt1);
   if (Closed == 1) ads_point_set(p->resval.rpoint, ptStart);

   while ((p = p->rbnext))
   {
      if (gsc_rb2Dbl(p, &Bulge2) == GS_BAD) Bulge2 = 0.0; // potrebbe non esserci
      else if (!(p = p->rbnext)) return GS_BAD;

      if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
      ads_point_set(p->resval.rpoint, pt2);

      if ((res = gsc_addObjsBufferFence(ResultSS, pt1, pt2, Bulge1,
                                        OffSet, type, Cls, Sub, CheckOn2D, OnlyInsPt,
                                        ss)) != GS_GOOD) break;

      ads_point_set(pt2, pt1);
      Bulge1 = Bulge2;
   }

   if (Closed == 1) // Se polilinea chiusa
   {
      if ((res = gsc_addObjsBufferFence(ResultSS, pt1, ptStart, Bulge1,
                                        OffSet, type, Cls, Sub, CheckOn2D, OnlyInsPt,
                                        ss)) != GS_GOOD) return GS_BAD;
   }
   
   return GS_GOOD;
}
int gsc_selObjsBufferFence(C_RB_LIST &CoordList, int type, ads_name ss,
                           int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                           int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL)
{
   C_SELSET InternalSS;
   int      Result = gsc_selObjsBufferFence(CoordList, type, InternalSS, CheckOn2D, OnlyInsPt,
                                            Cls, Sub, ObjType);

   InternalSS.get_selection(ss);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return Result;
}


///////////////////////////////////////////////////////////////////////////////
//                FINE     FUNZIONI       BUFFER-FENCE                       //
//                INIZIO   FUNZIONI       ALL                                //
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_selObjsAll                                                        */
/*+
  Questa funzione effettua la selezione di tutt gli oggetti.
  Parametri:
  ads_name ss;            Gruppo di selezione (non allocato)
  int Cls;                Per cercare solo oggetti di una certa classe (default = 0)
  int Sub;                Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int ObjType;            Tipo di oggetti grafici (default = GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsAll(ads_name ss, int Cls, int Sub, int ObjType)
{
   C_SELSET dummy;

   if (gsc_ssget(_T("_X"), NULL, NULL, NULL, dummy) != RTNORM) return GS_CAN;  
   dummy.ReleaseAllAtDistruction(GS_BAD);
   
   if (Cls > 0) dummy.intersectClsCode(Cls, Sub, ObjType);
   else dummy.intersectType(ObjType);

   dummy.get_selection(ss);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_selObjsArea                                                       */
/*+
  Questa funzione effettua la selezione degli oggetti con i criteri previsti
  dalla selezione spaziale.
  Parametri:
  C_STRING &SpatialSel;    Descrizione query spaziale
  ads_name ss;             Gruppo di selezione (non allocato)
  int      Cls;            Per cercare solo oggetti di una certa classe (default = 0)
  int      Sub;            Per cercare solo oggetti di una certa sottoclasse (default = 0)
  int      ObjType;        Tipi di oggetti da cercare (default GRAPHICAL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selObjsArea(C_STRING &SpatialSel, ads_name ss,
                    int Cls, int Sub, int ObjType)
{
   C_STRING  JoinOp, Boundary, SelType;
   C_RB_LIST CoordList;
   bool      Inverse;
   int       Type;

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
      return GS_BAD;

   // case insensitive
   Type = (SelType.comp(_T("inside"), FALSE) == 0) ? INSIDE : CROSSING;

   // case insensitive
   if (Boundary.comp(ALL_SPATIAL_COND, FALSE) == 0) // tutto
   {
      if (gsc_selObjsAll(ss, Cls, Sub, ObjType) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      if (gsc_selObjsCircle(CoordList, Type, ss, GS_GOOD, GS_GOOD, Cls, Sub, ObjType) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(FENCE_SPATIAL_COND, FALSE) == 0) // fence
   {
      if (gsc_selObjsFence(CoordList, ss, GS_GOOD, GS_GOOD, Cls, Sub, ObjType, true) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // "fencebutstartend"
   {  // senza includere punto iniziale e finale della linea
      if (gsc_selObjsFence(CoordList, ss, GS_GOOD, GS_GOOD, Cls, Sub, ObjType, false) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      if (gsc_selObjsBufferFence(CoordList, Type, ss, GS_GOOD, GS_GOOD, Cls, Sub, ObjType) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   {
      //                     CoordList,type , ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
      if (gsc_selObjsPolygon(CoordList, Type, ss, GS_GOOD, GS_GOOD, GS_GOOD,Cls, Sub, ObjType) == GS_BAD) return GS_BAD;
   }
   else
   if (Boundary.comp(WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      //                CoordList,type,ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
      if (gsc_selObjsWindow(CoordList, Type, ss, GS_GOOD, GS_GOOD, GS_GOOD, Cls, Sub, ObjType) == GS_BAD) return GS_BAD;
   }
   else return GS_BAD;

   if (Inverse)
   {
      C_SELSET Global;

      gsc_ssget(_T("_X"), NULL, NULL, NULL, Global);
      
      if (Cls > 0) Global.intersectClsCode(Cls, Sub, ObjType);
      else Global.intersectType(ObjType);

      Global.subtract(ss);
      acedSSFree(ss);
      Global.get_selection(ss);
      Global.ReleaseAllAtDistruction(GS_BAD);
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_selGridKeyListArea                                     <external> */
/*+
  Questa funzione effettua la selezione delle celle di una classe griglia 
  con i criteri previsti dalla selezione spaziale.
  Parametri:
  C_STRING &SpatialSel;    Descrizione query spaziale
  C_LONG_BTREE &KeyList;   Lista di celle
  int      Cls;            Codice classe
  int      Sub;            Codice sottoclasse

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selGridKeyListArea(C_STRING &SpatialSel, C_LONG_BTREE &KeyList, 
                           int Cls, int Sub)
{
   C_CLASS      *pCls;
   C_GRID       *pInfoGrid;
   C_STRING     JoinOp, Boundary, SelType;
   C_RB_LIST    CoordList;
   bool         Inverse;
   int          Mode;
   C_LONG_BTREE *p, Partial;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub)) == NULL) return GS_BAD;
   if (pCls->get_category() != CAT_GRID) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   pInfoGrid = (C_GRID *) pCls->ptr_grid();

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
      return GS_BAD;

   // case insensitive
   Mode = (SelType.comp(_T("inside"), FALSE) == 0) ? INSIDE : CROSSING;

   if (Inverse)
      p = &Partial;
   else
      p = &KeyList;

   // case insensitive
   if (Boundary.comp(ALL_SPATIAL_COND, FALSE) == 0) // tutto
   {
      C_RECT Rect;
      // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
      pInfoGrid->getExtension(Rect, true);
      if (pInfoGrid->getKeyListInWindow(Rect, CROSSING, *p) == GS_BAD) return GS_BAD;
   }
   else
   if (Boundary.comp(CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      if (pInfoGrid->getKeyListInCircle(CoordList, Mode, *p) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(FENCE_SPATIAL_COND, FALSE) == 0) // fence
   {
      if (pInfoGrid->getKeyListFence(CoordList, *p) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      if (pInfoGrid->getKeyListBufferFence(CoordList, Mode, *p) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   {
      if (pInfoGrid->getKeyListInPolygon(CoordList, Mode, *p) == GS_BAD)
         return GS_BAD;
   }
   else
   if (Boundary.comp(WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      if (pInfoGrid->getKeyListInWindow(CoordList, Mode, *p) == GS_BAD)
         return GS_BAD;
   }
   else return GS_BAD;

   if (Inverse)
   {
      C_RECT Rect;
      // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
      pInfoGrid->getExtension(Rect, true);
      if (pInfoGrid->getKeyListInWindow(Rect, CROSSING, KeyList) == GS_BAD) return GS_BAD;

      KeyList.subtract(*p);
   }

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////
/*.doc (new 2) gsc_redraw <extern> */
/*+
-------------------------------------------------------------
-------------------------------------------------------------
-*/
int gsc_redraw(ads_name ss,int mode)
{
   long i,len;
   ads_name ent;

   if (!ss) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (ads_sslength(ss,&len) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   for (i = 0; i < len; i++)
   {
      if (acedSSName(ss, i, ent) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (ads_redraw(ent, mode) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   }

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////
/*.doc (new 2) gsc_grdraw <extern> */
/*+
-------------------------------------------------------------
-------------------------------------------------------------
-*/
int gsc_grdraw(resbuf *points, int color, int mode, bool Closed)
{
   resbuf    *rb;
   ads_point pnt, FirstPt;
   bool      First = true;
   double    Bulge1, Bulge2;

   if (!(rb = points)) return GS_BAD;

   if (gsc_rb2Dbl(rb, &Bulge1) == GS_BAD) Bulge1 = 0.0; // potrebbe non esserci
   else if (!(rb = rb->rbnext)) return GS_BAD;

   ads_point_set(rb->resval.rpoint, pnt);
   if (Closed) ads_point_set(rb->resval.rpoint, FirstPt);
   if (!(rb = rb->rbnext)) return GS_BAD;

   while (rb)
   {
      if (gsc_rb2Dbl(rb, &Bulge2) == GS_BAD) Bulge2 = 0.0; // potrebbe non esserci
      else if (!(rb = rb->rbnext)) return GS_BAD;

      if (Bulge1 == 0) // segmento retto
      {
         if (acedGrDraw(pnt, rb->resval.rpoint, color, mode) != RTNORM)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      }
      else // arco
      {
         resbuf *out;

         gsc_arc_point(pnt, rb->resval.rpoint, Bulge1, 100, &out);
         gsc_grdraw(out, color, mode);
         acutRelRb(out);
      }

      ads_point_set(rb->resval.rpoint, pnt);
      Bulge1 = Bulge2;
      rb     = rb->rbnext;
   }
   
   if (Closed)
   {
      if (Bulge1 == 0) // segmento retto
      {
         if (acedGrDraw(pnt, FirstPt, color, mode) != RTNORM)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      }
      else // arco
      {
         resbuf *out;

         gsc_arc_point(pnt, FirstPt, Bulge1, 100, &out);
         gsc_grdraw(out, color, mode);
         acutRelRb(out);
      }

   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_circle_point                                                      */
/*+
  Questa funzione restituisce una lista di punti rappresentante la divisione 
  di un cerchio in tanti segmenti.
  Parametri:
  ads_point center;     centro del cerchio
  ads_real raggio;      raggio
  int num_vertex;       in quanti punti si vuole dividere il cerchio
  resbuf **out;         lista di punti

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_circle_point(ads_point center, ads_real raggio, int num_vertex, resbuf **out)
{
   C_RB_LIST list;
   presbuf   rb;
   ads_point pnt;
   ads_real  ang;
   int       i;

   pnt[0]=center[0]+raggio;
   pnt[1]=center[1];
   pnt[2]=center[2];
   if ((list << acutBuildList(RTPOINT, pnt, 0)) == NULL) return GS_BAD;
   rb = list.get_head();

   for (i = num_vertex; i > 0; i--)
   {
      ang = (2 * PI) / num_vertex * i;
      pnt[0]=center[0]+cos(ang)*raggio;
      pnt[1]=center[1]+sin(ang)*raggio;
      pnt[2]=center[2];
      if ((rb->rbnext=acutBuildList(RTPOINT,pnt,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      rb=rb->rbnext;
   }
   if (out!=NULL) { *out=list.get_head(); list.ReleaseAllAtDistruction(GS_BAD); } 

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_arc_point                                                      */
/*+
  Questa funzione restituisce una lista di punti rappresentante la divisione 
  di un arco in tanti segmenti.
  Parametri:
  ads_point pt1;        punto iniziale
  ads_point pt2;        punto finale
  ads_real bulge;       curvatura
  int num_vertex;       in quanti punti si vuole dividere l'arco
  resbuf **out;         lista di punti

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_arc_point(ads_point pt1, ads_point pt2, ads_real bulge, int num_vertex, 
                  resbuf **out)
{
   C_RB_LIST list;
   presbuf   rb;
   ads_point ptInit, ptFinal, Center, pnt;
   double    ArcAngle, Radius, Angle, FinalAngle, Step;

   ArcAngle = fabs(atan(bulge) * 4);
   if (bulge < 0)
   {
      ads_point_set(pt1, ptFinal);
      ads_point_set(pt2, ptInit);
   }
   else
   {
      ads_point_set(pt1, ptInit);
      ads_point_set(pt2, ptFinal);
   }

   gsc_getArcRayCenter(ptInit, ptFinal, ArcAngle, &Radius, Center);

   Angle      = acutAngle(Center, ptInit);
   FinalAngle = acutAngle(Center, ptFinal);
   Step       = ArcAngle / (num_vertex - 1);
   if (FinalAngle < Angle) FinalAngle += (2 * PI);

   while (Angle <= FinalAngle)
   {
      acutPolar(Center, Angle, Radius, pnt);
      if ((rb = acutBuildList(RTPOINT, pnt, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      list += rb;

      Angle += Step;
   }
   
   *out = list.get_head();
   list.ReleaseAllAtDistruction(GS_BAD);

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "listattr"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_listattr(ads_callback_packet *dcl)
{
   int                          pos;
   Common_Dcl_sql_bldqry_Struct *CommonStruct;
   presbuf                      Structure;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);
   Structure = CommonStruct->Structure.get_head();

   pos = _wtoi(dcl->value);

   if (dcl->reason == CBR_DOUBLE_CLICK)
   {
      C_STRING AttrName(" "), FieldName;

      FieldName = gsc_nth(0, gsc_nth(pos, Structure))->resval.rstring;
      if (CommonStruct->UseSQLMAPSyntax == GS_GOOD)
      {  // Correggo la sintassi del nome del campo per SQL MAP
         gsc_AdjSyntaxMAPFormat(FieldName);
      }
      else
         // Correggo la sintassi del nome del campo con i delimitatori propri della connessione OLE-DB
         if (gsc_AdjSyntax(FieldName, CommonStruct->pConn->get_InitQuotedIdentifier(),
                           CommonStruct->pConn->get_FinalQuotedIdentifier(),
                           CommonStruct->pConn->get_StrCaseFullTableRef()) == GS_BAD) return;

      AttrName += FieldName;

      dcl_sql_bldqry_edit(dcl->dialog, AttrName.get_name());
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "AttrValues"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_AttrValues(ads_callback_packet *dcl)
{
   Common_Dcl_sql_bldqry_Struct *CommonStruct;
   TCHAR                        strPos[10];
   presbuf                      Structure;
   C_STRING                     AttrName, result, Title;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);
   ads_get_tile(dcl->dialog, _T("listattr"), strPos, 10); 
   Structure = CommonStruct->Structure.get_head();
   AttrName = gsc_nth(0, gsc_nth(_wtoi(strPos), Structure))->resval.rstring;

   if (CommonStruct->prj > 0 && CommonStruct->cls > 0)
   {
      C_ATTRIB       *pAttrib;
      C_STRING       dummy;
      C_DBCONNECTION *pConn;

      if (CommonStruct->sec == 0)
      {
         C_CLASS *pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub);

         if (!pCls || !pCls->ptr_info()) return;
			Title = gsc_msg(230); //  "Classe: "
         pCls->get_CompleteName(dummy);
			Title += dummy;

			Title += _T(" - ");
			Title += AttrName.get_name();

         if (gsc_dd_sel_uniqueVal(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub, 0,
                                  AttrName, result, Title) != GS_GOOD) return;

         if ((pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
            return;
         pConn = pCls->ptr_info()->getDBConnection(OLD);
      }
      else // tabella secondaria
      {
         C_SECONDARY *pSec = gsc_find_sec(CommonStruct->prj, CommonStruct->cls,
                                          CommonStruct->sub, CommonStruct->sec);

         if (!pSec) return;
			Title = gsc_msg(309); //  "Tabella secondaria: "
			Title += pSec->get_name();
			Title += _T(" - ");
			Title += AttrName.get_name();
			Title += _T(" - ");

         if (gsc_dd_sel_uniqueVal(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub, CommonStruct->sec,
                                  AttrName, result, Title) != GS_GOOD) return;

         if ((pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
            return;
         pConn = pSec->ptr_info()->getDBConnection(OLD);
      }

      pAttrib->init_ADOType(pConn);

      if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
      {
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(result) == GS_BAD) return;
      }
      else if (gsc_DBIsDate(pAttrib->ADOType) == GS_GOOD)
      {
         // Correggo la stringa "date" secondo la sintassi SQL
         C_PROVIDER_TYPE *p;
         p = pConn->ptr_DataTypeList()->search_Type(adDBDate, FALSE);
         if (!p)
            // se non c'è il campo data (es ACCESS) provo con adDate
            p = pConn->ptr_DataTypeList()->search_Type(adDate, FALSE);

         if (pConn->Str2SqlSyntax(result, p) == GS_BAD)
            return;
         result = gsc_AdjSyntaxDateToTimestampMAPFormat(result);
      }
      else if (gsc_DBIsTimestamp(pAttrib->ADOType) == GS_GOOD)
      {
         // Correggo la stringa "time-stamp" secondo la sintassi SQL 
         C_PROVIDER_TYPE *p;
         p = pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE);

         if (pConn->Str2SqlSyntax(result, p) == GS_BAD)
            return;
         result = gsc_AdjSyntaxDateToTimestampMAPFormat(result);
      }
      else if (gsc_DBIsTime(pAttrib->ADOType) == GS_GOOD)
      {
         // Correggo la stringa "date" secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(result, pConn->ptr_DataTypeList()->search_Type(adDBTime, FALSE)) == GS_BAD)
            return;
         result = gsc_AdjSyntaxTimeToTimestampMAPFormat(result);
      }
   }
   else
      if (gsc_dd_sel_uniqueVal(CommonStruct->pConn, CommonStruct->TableRef, AttrName,
                               result, Title) != GS_GOOD) return;

   if (result.len() > 0)
      dcl_sql_bldqry_edit(dcl->dialog, result.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "SuggestValue"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_SuggestValue(ads_callback_packet *dcl)
{
   Common_Dcl_sql_bldqry_Struct *CommonStruct;
   TCHAR                        strPos[10];
   presbuf                      Structure;
   C_STRING                     AttrName, result, dummy;
   C_ATTRIB                     *pAttrib;
   C_DBCONNECTION               *pConn;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);
   ads_get_tile(dcl->dialog, _T("listattr"), strPos, 10); 
   Structure = CommonStruct->Structure.get_head();
   AttrName = gsc_nth(0, gsc_nth(_wtoi(strPos), Structure))->resval.rstring;

   if (CommonStruct->prj == 0 || CommonStruct->cls == 0) return;

   if (CommonStruct->sec == 0)
   {
      C_CLASS *pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub);

      if (!pCls || !pCls->ptr_info()) return;
      if ((pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
         return;
      pConn = pCls->ptr_info()->getDBConnection(OLD);
   }
   else // tabella secondaria
   {
      C_SECONDARY *pSec = gsc_find_sec(CommonStruct->prj, CommonStruct->cls,
                                       CommonStruct->sub, CommonStruct->sec);
      if ((pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
         return;
      pConn = pSec->ptr_info()->getDBConnection(OLD);
   }

   pAttrib->init_ADOType(pConn);

   if (gsc_ddSuggestDBValue(result, pAttrib->ADOType) == GS_BAD ||
       result.len() == 0) return;

   // Aggiungo gli apici a inizio\fine
   if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
   {
      // Correggo la stringa secondo la sintassi SQL 
      if (pConn->Str2SqlSyntax(result) == GS_BAD) return;
   }
   else
   // Data in formato MAP SQL yyyy-mm-dd
   if (gsc_DBIsDate(pAttrib->ADOType) == GS_GOOD)
   {
      // Correggo la stringa "date" secondo la sintassi SQL 
      if (pConn->Str2SqlSyntax(result, pConn->ptr_DataTypeList()->search_Type(adDBDate, FALSE)) == GS_BAD)
         return;
   }
   else
   // Data in formato MAP SQL yyyy-mm-dd hh:mm:ss
   if (gsc_DBIsTimestamp(pAttrib->ADOType) == GS_GOOD)
   {
      // Correggo la stringa "time-stamp" secondo la sintassi SQL 
      if (pConn->Str2SqlSyntax(result, pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
         return;
   }
   else
   // Tempo in formato MAP SQL hh:mm:ss
   if (gsc_DBIsTime(pAttrib->ADOType) == GS_GOOD)
   {
      // Correggo la stringa "date" secondo la sintassi SQL 
      if (pConn->Str2SqlSyntax(result, pConn->ptr_DataTypeList()->search_Type(adDBTime, FALSE)) == GS_BAD)
         return;
   }
   else
   // converto gli attributi numerici con lo standard (123.456 e NON con le impostazioni di window)
   if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD)
   {
      double n;
      if (gsc_conv_Number(result.get_name(), &n) == GS_BAD) return;
      result = gsc_tostring(n);
   }

   if (result.len() > 0)
      dcl_sql_bldqry_edit(dcl->dialog, result.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "MODULE"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_module(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" MODULE("));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "ABSOLUTE"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_absolute(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" ABSOLUTE("));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "mat_op"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_mat_op(ads_callback_packet *dcl)
{
   TCHAR op[10], ToAppend[10] = GS_EMPTYSTR;

   ads_get_tile(dcl->dialog, _T("mat_op"), op, 10); 

   switch (_wtoi(op))
   {
      case 0:
         wcscpy(ToAppend, _T(" + ")); break;
      case 1:
         wcscpy(ToAppend, _T(" - ")); break;
      case 2:
         wcscpy(ToAppend, _T(" * ")); break;
      case 3:
         wcscpy(ToAppend, _T(" / ")); break;
      default: return;
   }   
   
   dcl_sql_bldqry_edit(dcl->dialog, ToAppend);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "AND"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_and(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" AND "));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "OR"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_or(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" OR "));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "NOT"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_not(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" NOT "));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Inizia per"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_StartWith(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" LIKE '<valore>%'"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Termina per"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_EndWith(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" LIKE '%<valore>'"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Contenente"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_Containing(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" LIKE '%<valore>%'"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "BETWEEN"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_between(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" BETWEEN <limite1> AND <limite2>"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "IN"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_in(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" IN(<valore1>, <valore2> ...)"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "IS NULL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_is_null(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" IS NULL"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "IS NOT NULL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_is_not_null(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" IS NOT NULL"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "OPEN"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_open(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(" ("));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "CLOSE"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_close(ads_callback_packet *dcl)
{
   dcl_sql_bldqry_edit(dcl->dialog, _T(")"));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Operator"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_operator(ads_callback_packet *dcl)
{
   TCHAR op[10], ToAppend[10] = GS_EMPTYSTR;

   ads_get_tile(dcl->dialog, _T("Operator"), op, 10); 

   switch (_wtoi(op))
   {
      case 0:
         wcscpy(ToAppend, _T(" = ")); break;
      case 1:
         wcscpy(ToAppend, _T(" > ")); break;
      case 2:
         wcscpy(ToAppend, _T(" < ")); break;
      case 3:
         wcscpy(ToAppend, _T(" <> ")); break;
      case 4:                  
         wcscpy(ToAppend, _T(" >= ")); break;
      case 5:
         wcscpy(ToAppend, _T(" <= ")); break;
      default: return;
   }   
   
   dcl_sql_bldqry_edit(dcl->dialog, ToAppend);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "DATE"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_date(ads_callback_packet *dcl)
{
   C_STRING Msg(" TIMESTAMP'<");

   Msg += gsc_msg(296); // "aaaa-mm-gg"
   Msg += _T("> 00:00:00'");
   dcl_sql_bldqry_edit(dcl->dialog, Msg.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "DATETIME"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_datetime(ads_callback_packet *dcl)
{
   C_STRING Msg(" TIMESTAMP'<");

   Msg += gsc_msg(296); // "aaaa-mm-gg"
   Msg += _T(" ");
   Msg += gsc_msg(300); // "hh:mm:ss"
   Msg += _T(">'");
   dcl_sql_bldqry_edit(dcl->dialog, Msg.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "TIME"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_time(ads_callback_packet *dcl)
{
   C_STRING Msg(" TIME'<");

   Msg += gsc_msg(300); // "hh:mm:ss"
   Msg += _T(">'");
   dcl_sql_bldqry_edit(dcl->dialog, Msg.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "CLEAR"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_clear(ads_callback_packet *dcl)
{
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   ads_set_tile(dcl->dialog, _T("editsql"), GS_EMPTYSTR); 
   ads_mode_tile(dcl->dialog, _T("editsql"), MODE_SETFOCUS); 
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Load"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_Load(ads_callback_packet *dcl)
{
	C_PROJECT *pPrj;
   C_CLASS   *pCls;
   C_STRING  FileName, QueryDir, LastSQLQryFile;
   Common_Dcl_sql_bldqry_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSQLQryFile"), LastSQLQryFile) == GS_BAD ||
       gsc_dir_exist(LastSQLQryFile) == GS_BAD)
   {
      // il direttorio di default per le QRY SQL è il direttorio dei DWG
      if (CommonStruct->prj > 0 && (pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(CommonStruct->prj)))
      {
         if (CommonStruct->cls > 0 && 
             (pCls = pPrj->find_class(CommonStruct->cls, CommonStruct->sub)) &&
              pCls->ptr_GphInfo() &&
              pCls->ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
            QueryDir = ((C_DWG_INFO *) pCls->ptr_GphInfo())->dir_dwg;
      }

      if (QueryDir.get_name() == NULL)
         if (GS_CURRENT_WRK_SESSION)
            // il direttorio di default per le QRY SQL è il direttorio dei progetto attuale
            QueryDir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
   }
   else
      if (gsc_dir_from_path(LastSQLQryFile, QueryDir) == GS_BAD) return;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), QueryDir, _T("qry"), UI_LOADFILE_FLAGS, FileName) == RTNORM)
   {
      FILE     *file = NULL;
      C_STRING SQLCond, LPName, SQL;
      bool     Unicode;

      if ((file = gsc_fopen(FileName, _T("r"), MORETESTS, &Unicode)) == NULL)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSOpenFile)); // "*Errore* Apertura file fallita"
         return;
      }
   
      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastSQLQryFile"), FileName);

      do
      {
         if (gsc_SQLCond_Load(file, SQLCond, Unicode) == GS_BAD)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(GS_ERR_COD));
            break;
         }
         if (gsc_fclose(file) == GS_BAD)
         {
            file = NULL;
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
            break;
         }
         file = NULL;
         // scompongo la condizione impostata
         if (gsc_splitSQLCond(SQLCond, LPName, SQL) == GS_BAD)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
            break;
         }
         
         ads_set_tile(dcl->dialog, _T("editsql"), SQL.get_name()); // elimino la '('
         ads_mode_tile(dcl->dialog, _T("editsql"), MODE_SETFOCUS); 
      }
      while (0);

      if (file)
         if (gsc_fclose(file) == GS_BAD)
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "Save"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_Save(ads_callback_packet *dcl)
{
	C_PROJECT *pPrj;
   C_CLASS   *pCls;
   C_STRING  FileName, DefaultFileName, QueryDir, LastSQLQryFile;
   Common_Dcl_sql_bldqry_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSQLQryFile"), LastSQLQryFile) == GS_BAD ||
       gsc_dir_exist(LastSQLQryFile) == GS_BAD)
   {
      // il direttorio di default per le QRY SQL è il direttorio dei DWG
      if (CommonStruct->prj > 0 && (pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(CommonStruct->prj)))
      {
         if (CommonStruct->cls > 0 && 
             (pCls = pPrj->find_class(CommonStruct->cls, CommonStruct->sub)) &&
              pCls->ptr_GphInfo() &&
              pCls->ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
            QueryDir = ((C_DWG_INFO *) pCls->ptr_GphInfo())->dir_dwg;
      }

      if (QueryDir.get_name() == NULL)
         if (GS_CURRENT_WRK_SESSION)
            // il direttorio di default per le QRY SQL è il direttorio dei progetto attuale
            QueryDir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
   }
   else
      if (gsc_dir_from_path(LastSQLQryFile, QueryDir) == GS_BAD) return;

   // "SQL"
   if (gsc_get_tmp_filename(QueryDir.get_name(), gsc_msg(655), _T(".qry"), DefaultFileName) == GS_BAD)
      return;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), DefaultFileName, _T("qry"), UI_SAVEFILE_FLAGS, FileName) == RTNORM)
   {
      TCHAR    Prefix1[] = _T("(setq ade_cmddia_before_qry (getvar \"cmddia\"))\n(setvar \"cmddia\" 0)\n(ade_qryclear)\n(ade_qrydefine '(\"\" \"\" \"\" \"SQL\" (\"");
      TCHAR    Postfix[] = _T(") \"\"))\n(setvar \"cmddia\" ade_cmddia_before_qry)");
      TCHAR    SQLCond[MAX_LEN_SQL_STM], *p_chr;
      FILE     *file;
      C_STRING ext, LPName;

      LPName = _T("\" ");

      gsc_splitpath(FileName, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // aggiungo l'estensione "QRY"
         FileName += _T(".QRY");

      // Apertura file NON in modo UNICODE (i file .QRY sono ancora in ANSI)
      if ((file = gsc_fopen(FileName, _T("w+"))) == NULL)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidPath)); // "*Errore* Path non valida: controllare il percorso e/o le macchine in rete"
         return;
      }

      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastSQLQryFile"), FileName);

      do
      {
         if (fwprintf(file, Prefix1) < 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
         if (CommonStruct->prj > 0 && CommonStruct->cls > 0 && CommonStruct->sec == 0)
         { // si tratta di una classe di GEOsim
            C_CLASS   *pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub);

            if (!pCls) break;
            // Ricavo il LinkPathName della tabella OLD
            if (pCls->getLPNameOld(LPName) == GS_GOOD) LPName += _T("\" ");
         }
         // LPN
         if (fwprintf(file, LPName.get_name()) < 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
         ads_get_tile(dcl->dialog, _T("editsql"), SQLCond, MAX_LEN_SQL_STM);

         if (fputwc(_T('"'), file) == WEOF) break; // antepongo un doppio apice "
         // scrivo la stringa senza formattazione (es. "%%" -> "%%" e non "%")
         p_chr = SQLCond; 
		   while (p_chr[0] != _T('\0'))
         {
            // per i caratteri speciali antepongo un "\" backslash
            if (p_chr[0] == _T('"') || p_chr[0] == _T('\\'))
               if (fputwc(_T('\\'), file) == WEOF) break;
            if (fputwc(p_chr[0], file) == WEOF) break;

            p_chr++;
         }
         if (fputwc('"', file) == WEOF) break; // aggiungo un doppio apice "

         if (p_chr[0] != _T('\0'))
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
         if (fwprintf(file, Postfix) < 0)
         {
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"
            break;            
         }
      }
      while (0);

      if (gsc_fclose(file) == GS_BAD)
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_bldqry" su controllo "OK"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_bldqry_accept_ok(ads_callback_packet *dcl)
{
   TCHAR    *err, val[MAX_LEN_SQL_STM];
   int      prj = 0, cls = 0;
   C_STRING SQLTest; 
   Common_Dcl_sql_bldqry_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);

   ads_set_tile(dcl->dialog, _T("error"), gsc_msg(360)); // "Attendere..."
   ads_get_tile(dcl->dialog, _T("editsql"), val, MAX_LEN_SQL_STM);

   if (CommonStruct->sec == 0)
   { // classe di GEOsim
      prj = CommonStruct->prj;
      cls = CommonStruct->cls;
   }
   SQLTest = _T("SELECT * FROM ");
   SQLTest += CommonStruct->TableRef;
   SQLTest += _T(" WHERE ");
   SQLTest += val;
   if ((err = CommonStruct->pConn->CheckSql(SQLTest.get_name())) != NULL)
   {  // ci sono errori
      ads_set_tile(dcl->dialog, _T("error"), err);
      free(err);
      return;
   }
      
   CommonStruct->SQLCond = val;
   ads_done_dialog(dcl->dialog, DLGOK);
} 


/*****************************************************************************/
/*.doc dcl_sql_bldqry_edit                                                   */
/*+
  Questa funzione aggiunge una stringa alla condizione SQL della finestra
  "sql_bldqry".
  Parametri:
  ads_hdlg hdlg;     handle della finestra
  const TCHAR *str;   stringa da aggungere
-*/  
/*****************************************************************************/
static void dcl_sql_bldqry_edit(ads_hdlg dcl_id, const TCHAR *str)
{
   TCHAR val[MAX_LEN_SQL_STM];

   ads_set_tile(dcl_id, _T("error"), GS_EMPTYSTR);
   if (!str) return;
   ads_get_tile(dcl_id, _T("editsql"), val, MAX_LEN_SQL_STM);
   if (wcslen(val) + wcslen(str) + 1 >= MAX_LEN_SQL_STM)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSStringTooLong)); // "*Errore* Stringa troppo lunga"
      return;
   }
   wcscat(val, str);
   ads_set_tile(dcl_id, _T("editsql"), val); 
   ads_mode_tile(dcl_id, _T("editsql"), MODE_SETFOCUS); 
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_bldqry_help(ads_callback_packet *dcl)
   { gsc_help(IDH_ImpostazionedellecondizioniSQL); } 


/*****************************************************************************/
/*.doc gsc_ddBuildQry                                                        */
/*+
  Questa funzione crea e modifica una condizione SQL di una classe o una secondaria
  con interfaccia a finestra.
  Parametri:
  const TCHAR *title;      Titolo finestra
  C_DBCONNECTION *pConn;   Connessione OLE-DB
  C_STRING &TableRef;
  C_STRING &SQLCond;       condizione SQL (dopo il WHERE ...)
  int prj;                 Codice progetto (solo per classi GEOsim) (default = 0)
  int cls;                 Codice classe (solo per classi GEOsim)   (default = 0)
  int sub;                 Codice sottoclasse (solo per classi GEOsim) (default = 0)
  int sec;                 Codice tabella secondaria (default = 0)
  int UseSQLMAPSyntax;     Se = GS_GOOD verrà utilizzata la sintassi SQL propria 
                           di AutoCAD MAP altrimenti verranno utilizzati
                           quelli della connessione OLE-DB (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddBuildQry(const TCHAR *title, C_DBCONNECTION *pConn, C_STRING &TableRef,
                   C_STRING &SQLCond, int prj, int cls, int sub, int sec,
                   int UseSQLMAPSyntax)
{
   C_STRING      path;
   ads_hdlg      dcl_id;
   int           status, dcl_file, i = 0;
   Common_Dcl_sql_bldqry_Struct CommonStruct;
   presbuf       p;

   CommonStruct.prj      = prj;
   CommonStruct.cls      = cls;
   CommonStruct.sub      = sub;
   CommonStruct.sec      = sec;
   if (gsc_Table2MAPFormat(pConn, TableRef, path) == GS_BAD) return GS_BAD;
   CommonStruct.TableRef = path;
   CommonStruct.pConn    = pConn;
   CommonStruct.SQLCond  = SQLCond;
   CommonStruct.UseSQLMAPSyntax = UseSQLMAPSyntax;

   // leggo struttura
   if ((CommonStruct.Structure << pConn->ReadStruct(TableRef.get_name())) == NULL)
      return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("sql_bldqry"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (!dcl_id)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;
   }
     
   // RIEMPIE LA LIST-BOX DEGLI ATTRIBUTI SELEZIONABILI
   ads_start_list(dcl_id, _T("listattr"), LIST_NEW, 0);
   while ((p = CommonStruct.Structure.nth(i++)) != NULL)
      gsc_add_list(gsc_nth(0, p)->resval.rstring);
   ads_end_list();

   if (title) ads_set_tile(dcl_id, _T("TITOLO"), title);
   ads_set_tile(dcl_id, _T("listattr"), _T("0"));

   if (SQLCond.len() > 0)
      ads_set_tile(dcl_id, _T("editsql"), SQLCond.get_name()); 
      
   ads_action_tile(dcl_id , _T("listattr"), (CLIENTFUNC) dcl_bldqry_listattr);
   ads_client_data_tile(dcl_id, _T("listattr"), &CommonStruct);
   ads_action_tile(dcl_id , _T("AttrValues"), (CLIENTFUNC) dcl_bldqry_AttrValues);
   ads_client_data_tile(dcl_id, _T("AttrValues"), &CommonStruct);
   ads_action_tile(dcl_id, _T("SuggestValue"), (CLIENTFUNC) dcl_bldqry_SuggestValue);
   ads_client_data_tile(dcl_id, _T("SuggestValue"), &CommonStruct);
   
   ads_action_tile(dcl_id , _T("and"), (CLIENTFUNC) dcl_bldqry_and);
   ads_action_tile(dcl_id , _T("or"), (CLIENTFUNC) dcl_bldqry_or);
   ads_action_tile(dcl_id , _T("not"), (CLIENTFUNC) dcl_bldqry_not);
   
   ads_action_tile(dcl_id , _T("module"), (CLIENTFUNC) dcl_bldqry_module);
   ads_action_tile(dcl_id , _T("absolute"), (CLIENTFUNC) dcl_bldqry_absolute);
   ads_action_tile(dcl_id , _T("mat_op"), (CLIENTFUNC) dcl_bldqry_mat_op);

   ads_action_tile(dcl_id , _T("date"), (CLIENTFUNC) dcl_bldqry_date);
   ads_action_tile(dcl_id , _T("datetime"), (CLIENTFUNC) dcl_bldqry_datetime);
   ads_action_tile(dcl_id , _T("time"), (CLIENTFUNC) dcl_bldqry_time);

   ads_action_tile(dcl_id , _T("StartWith"), (CLIENTFUNC) dcl_bldqry_StartWith);
   ads_action_tile(dcl_id , _T("EndWith"), (CLIENTFUNC) dcl_bldqry_EndWith);
   ads_action_tile(dcl_id , _T("Containing"), (CLIENTFUNC) dcl_bldqry_Containing);

   ads_action_tile(dcl_id , _T("between"), (CLIENTFUNC) dcl_bldqry_between);
   ads_action_tile(dcl_id , _T("in"), (CLIENTFUNC) dcl_bldqry_in);
   ads_action_tile(dcl_id , _T("is_null"), (CLIENTFUNC) dcl_bldqry_is_null);
   ads_action_tile(dcl_id , _T("is_not_null"), (CLIENTFUNC) dcl_bldqry_is_not_null);
   ads_action_tile(dcl_id , _T("open_op"), (CLIENTFUNC) dcl_bldqry_open);
   ads_action_tile(dcl_id , _T("close_op"), (CLIENTFUNC) dcl_bldqry_close);
   ads_action_tile(dcl_id , _T("Operator"), (CLIENTFUNC) dcl_bldqry_operator);

   ads_action_tile(dcl_id , _T("clear"), (CLIENTFUNC) dcl_bldqry_clear);

   ads_action_tile(dcl_id , _T("Load"), (CLIENTFUNC) dcl_bldqry_Load);
   ads_client_data_tile(dcl_id, _T("Load"), &CommonStruct);
   ads_action_tile(dcl_id , _T("Save"), (CLIENTFUNC) dcl_bldqry_Save);
   ads_client_data_tile(dcl_id, _T("Save"), &CommonStruct);

   ads_action_tile(dcl_id , _T("accept"), (CLIENTFUNC) dcl_bldqry_accept_ok);
   ads_client_data_tile(dcl_id, _T("accept"), &CommonStruct);
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_bldqry_help);

   // LANCIA LA DIALOG-BOX
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);
   
   if (status == DLGCANCEL) return GS_CAN;

   SQLCond = CommonStruct.SQLCond;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setTileDclaggr_bldqry                                            */
/*+
  Questa funzione setta tutti i controlli della DCL "aggr_bldqry" in modo 
  appropriato.
  Parametri:
  ads_hdlg  dcl_id;     
  C_STRING &SQLAggrCond;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_setTileDclaggr_bldqry(ads_hdlg dcl_id, Common_Dcl_sql_bldqry_Struct *CommonStruct)
{
   C_STRING AggrFun, AttribName, Operator, Value, dummy;
   presbuf  p, Structure = CommonStruct->Structure.get_head();
   int      i;
                                  
   if (CommonStruct->SQLCond.len() > 0)
      // scompongo la condizione impostata
      if (gsc_splitSQLAggrCond(CommonStruct->SQLCond, AggrFun, AttribName, Operator, Value) == GS_BAD)
         return GS_BAD;

   // list box delle funzioni di aggregazione
   ads_start_list(dcl_id, _T("aggrSQL"), LIST_NEW, 0);
   for (i = 0; i < ELEMENTS(PARTIAL_AGGR_FUN_LIST); i++)
   {
      gsc_add_list(PARTIAL_AGGR_FUN_LIST[i]);
      if (gsc_strcmp(AggrFun.get_name(), PARTIAL_AGGR_FUN_LIST[i], FALSE) == 0)
         dummy = i;
   }   
   ads_end_list();
   if (dummy.len() > 0) ads_set_tile(dcl_id , _T("aggrSQL"), dummy.get_name());
   dummy.clear();

   // list box degli attributi
   i = 0;
   ads_start_list(dcl_id, _T("attrib"), LIST_NEW, 0);
   while ((p = gsc_nth(i, Structure)) != NULL)
   {
      if ((p = gsc_nth(0, p)) == NULL) break;
      gsc_add_list(p->resval.rstring);
      if (gsc_strcmp(AttribName.get_name(), p->resval.rstring, FALSE) == 0)
         dummy = i;
      i++;
   }   
   ads_end_list();
   if (dummy.len() > 0) ads_set_tile(dcl_id , _T("attrib"), dummy.get_name());
   dummy.clear();

   if (gsc_strcmp(AggrFun.get_name(), _T("Count *"), FALSE) == 0)
      ads_mode_tile(dcl_id, _T("attrib"), MODE_DISABLE);
   else
      ads_mode_tile(dcl_id, _T("attrib"), MODE_ENABLE);

   // list box degli operatori
   i = 0;
   ads_start_list(dcl_id, _T("Operator"), LIST_NEW, 0);
   for (i = 0; i < ELEMENTS(OPERATOR_LIST); i++)
   {
      gsc_add_list(OPERATOR_LIST[i]);
      if (gsc_strcmp(Operator.get_name(), OPERATOR_LIST[i], FALSE) == 0)
         dummy = i;
   }   
   ads_end_list();
   if (dummy.len() > 0) ads_set_tile(dcl_id , _T("Operator"), dummy.get_name());
   dummy.clear();

   // edit box valore
   ads_set_tile(dcl_id , _T("value"), Value.get_name());

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "aggr_bldqry" su controllo "aggrSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_aggr_bldqry_aggrSQL(ads_callback_packet *dcl)
{
   if (wcschr(PARTIAL_AGGR_FUN_LIST[_wtoi(dcl->value)], _T('*')))
      ads_mode_tile(dcl->dialog, _T("attrib"), MODE_DISABLE);
   else
      ads_mode_tile(dcl->dialog, _T("attrib"), MODE_ENABLE);
} 
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "aggr_bldqry" su controllo "clear"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_aggr_bldqry_clear(ads_callback_packet *dcl)
{
   Common_Dcl_sql_bldqry_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);

   ((Common_Dcl_sql_bldqry_Struct*)(dcl->client_data))->SQLCond.clear();
   ads_done_dialog(dcl->dialog, DLGOK);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "sql_aggr_bldqry" su controllo "AttrValues"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_aggr_bldqry_AttrValues(ads_callback_packet *dcl)
{
   Common_Dcl_sql_bldqry_Struct *CommonStruct;
   TCHAR                        strPos[10];
   presbuf                      Structure;
   C_STRING                     result, AttrName, Title;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);
   ads_get_tile(dcl->dialog, _T("attrib"), strPos, 10); 
   Structure = CommonStruct->Structure.get_head();
   AttrName = gsc_nth(0, gsc_nth(_wtoi(strPos), Structure))->resval.rstring;

   if (CommonStruct->prj > 0 && CommonStruct->cls > 0)
   {
      C_ATTRIB *pAttrib;
      C_STRING dummy;

      if (CommonStruct->sec == 0)
      {
         C_CLASS *pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub);

         if (!pCls || !pCls->ptr_info()) return;
			Title = gsc_msg(230); //  "Classe: "
         pCls->get_CompleteName(dummy);
			Title += dummy;

			Title += _T(" - ");
			Title += AttrName.get_name();
         if (gsc_dd_sel_uniqueVal(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub, 0,
                                  AttrName, result, Title) != GS_GOOD) return;
         if ((pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
            return;
         pAttrib->init_ADOType(pCls->ptr_info()->getDBConnection(OLD));
      }
      else // tabella secondaria
      {
         C_SECONDARY *pSec = gsc_find_sec(CommonStruct->prj, CommonStruct->cls,
                                          CommonStruct->sub, CommonStruct->sec);

			if (!pSec) return;
			Title = gsc_msg(309); //  "Tabella secondaria: "
			Title += pSec->get_name();
			Title += _T(" - ");
			Title += AttrName.get_name();
			Title += _T(" - ");
         if (gsc_dd_sel_uniqueVal(CommonStruct->prj, CommonStruct->cls, CommonStruct->sub, CommonStruct->sec,
                                  AttrName, result, Title) != GS_GOOD) return;

         if ((pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
            return;
         pAttrib->init_ADOType(pSec->ptr_info()->getDBConnection(OLD));
      }

      if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
      {  // Aggiungo apici perchè è di tipo carattere
         dummy = _T("'");
         dummy += result;
         dummy += _T("'");
         result = dummy;
      }
   }
   else
      if (gsc_dd_sel_uniqueVal(CommonStruct->pConn, CommonStruct->TableRef, AttrName,
                               result, Title) != GS_GOOD) return;

   if (result.len() > 0)
      ads_set_tile(dcl->dialog, _T("value"), result.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "aggr_bldqry" su controllo "accept_ok"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_aggr_bldqry_accept_ok(ads_callback_packet *dcl)
{
   TCHAR    *err, val[TILE_STR_LIMIT];
   C_STRING AggrFun, AttribName, Operator, Value, SQLAggrCond, statement;
   presbuf  p;
   Common_Dcl_sql_bldqry_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_sql_bldqry_Struct*)(dcl->client_data);

   ads_set_tile(dcl->dialog, _T("error"), gsc_msg(360)); // "Attendere..."
   
   ads_get_tile(dcl->dialog, _T("aggrSQL"), val, TILE_STR_LIMIT);
   AggrFun = PARTIAL_AGGR_FUN_LIST[_wtoi(val)];
   
   ads_get_tile(dcl->dialog, _T("attrib"), val, TILE_STR_LIMIT);
   p = CommonStruct->Structure.nth(_wtoi(val));
   AttribName = gsc_nth(0, p)->resval.rstring;

   ads_get_tile(dcl->dialog, _T("Operator"), val, TILE_STR_LIMIT);
   Operator = OPERATOR_LIST[_wtoi(val)];
   
   ads_get_tile(dcl->dialog, _T("value"), val, TILE_STR_LIMIT);
   if (wcslen(val) == 0) 
   {  // ci sono errori
      ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidSqlStatement));
      return;
   }
   Value = val;

   if (gsc_fullSQLAggrCond(AggrFun, AttribName, Operator, Value, TRUE, SQLAggrCond) == GS_BAD)
      return;

   statement = _T("SELECT ");
   statement += SQLAggrCond;
   statement += _T(" FROM ");
   statement += CommonStruct->TableRef;

   if ((err = CommonStruct->pConn->CheckSql(statement.get_name())) != NULL)
   {  // ci sono errori
      ads_set_tile(dcl->dialog, _T("error"), err);
      free(err);
      return;
   }

   if (gsc_fullSQLAggrCond(AggrFun, AttribName, Operator, Value, FALSE, SQLAggrCond) == GS_BAD)
      return;

   ((Common_Dcl_sql_bldqry_Struct*)(dcl->client_data))->SQLCond = SQLAggrCond;
   ads_done_dialog(dcl->dialog, DLGOK);
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_aggr_bldqry_help(ads_callback_packet *dcl)
   { gsc_help(IDH_ImpostazionedellecondizioniSQL); } 


/*****************************************************************************/
/*.doc gsc_ddBuildAggrQry                                                    */
/*+
  Questa funzione crea e modifica una condizione di aggregazione SQL di una classe
  o una secondaria con interfaccia a finestra.
  Parametri:
  const TCHAR *title;    Titolo finestra
  C_DBCONNECTION *pConn; Connessione OLE-DB
  C_STRING &TableRef;
  C_STRING &SQLAggrCond; Condizione SQL con funzioni di aggregazione (es. SUM(lungh)> 10 ...)
  int prj;               Codice progetto (solo per classi GEOsim) (default = 0)
  int cls;               Codice classe (solo per classi GEOsim)   (default = 0)
  int sub;               Codice sottoclasse (solo per classi GEOsim) (default = 0)
  int sec;               Codice tabella secondaria (default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddBuildAggrQry(const TCHAR *title, C_DBCONNECTION *pConn, C_STRING &TableRef,
                       C_STRING &SQLAggrCond, int prj, int cls, int sub, int sec)
{
   C_STRING   path;
   ads_hdlg   dcl_id;
   int        status, dcl_file;
   C_RB_LIST  Structure;
   Common_Dcl_sql_bldqry_Struct CommonStruct;

   CommonStruct.prj      = prj;
   CommonStruct.cls      = cls;
   CommonStruct.sub      = sub;
   CommonStruct.sec      = sec;
   CommonStruct.TableRef = TableRef;
   CommonStruct.pConn    = pConn;
   CommonStruct.SQLCond  = SQLAggrCond;

   // leggo struttura
   if ((CommonStruct.Structure << pConn->ReadStruct(TableRef.get_name())) == NULL)
      return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("aggr_bldqry"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (!dcl_id)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;
   }

   ads_set_tile(dcl_id, _T("TITOLO"), title);

   gsc_setTileDclaggr_bldqry(dcl_id, &CommonStruct);
   
   ads_action_tile(dcl_id , _T("aggrSQL"), (CLIENTFUNC) dcl_aggr_bldqry_aggrSQL);
   ads_action_tile(dcl_id , _T("AttrValues"), (CLIENTFUNC) dcl_aggr_bldqry_AttrValues);
   ads_client_data_tile(dcl_id, _T("AttrValues"), &CommonStruct);
   ads_action_tile(dcl_id , _T("accept"), (CLIENTFUNC) dcl_aggr_bldqry_accept_ok);
   ads_client_data_tile(dcl_id, _T("accept"), &CommonStruct);
   ads_action_tile(dcl_id , _T("clear"), (CLIENTFUNC) dcl_aggr_bldqry_clear);
   ads_client_data_tile(dcl_id, _T("clear"), &CommonStruct);
   ads_action_tile(dcl_id, _T("help"),   (CLIENTFUNC) dcl_aggr_bldqry_help);

   // LANCIA LA DIALOG-BOX
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);
   
   if (status == DLGCANCEL) return GS_CAN;

   SQLAggrCond = CommonStruct.SQLCond;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsddfilterevid <external> */
/*+
   Comando con uso di interfaccia DCL per l'evidenziazione degli oggetti
   grafici che sono associati ai record della tabella a cui è stato applicato
   l'ultimo filtro.
-*/  
/*********************************************************/
void gsddfilterevid(void)
{
   C_CLASS  *pCls;
   int 	   ret;
   long     LenSS;

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(GS_LSFILTER.cls, GS_LSFILTER.sub)) == NULL)
      { acutPrintf(gsc_msg(117), 0); return; } // "\n%ld oggetti grafici filtrati."

   if (pCls->get_category() == CAT_GRID) // Se è griglia
   {
      if ((LenSS = GS_LSFILTER.ptr_KeyList()->get_count()) == 0)
         { acutPrintf(gsc_msg(117), 0); return; } // "\n%ld oggetti grafici filtrati."
   }
   else
      if ((LenSS = GS_LSFILTER.ptr_SelSet()->length()) == 0)
         { acutPrintf(gsc_msg(117), 0); return; } // "\n%ld oggetti grafici filtrati."

   acutPrintf(gsc_msg(117), LenSS); // "\n%ld oggetti grafici filtrati."

   GEOsimAppl::CMDLIST.StartCmd();

   gsc_startTransaction();

   if (pCls->get_category() == CAT_GRID) // Se griglia
      ret = gsc_ddGridEvid((C_CGRID *) pCls, *(GS_LSFILTER.ptr_KeyList()));
   else
      ret = gsc_ddevid(pCls, *(GS_LSFILTER.ptr_SelSet()));

   if (ret != GS_GOOD)
   {
      gsc_abortTransaction();
      if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      else return GEOsimAppl::CMDLIST.CancelCmd();
   }

   gsc_endTransaction();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_filterPrepareStatistics             <external> */
/*+
  Funzione LISP, prepara una tabella temporanea per eseguire il calcolo
  delle statistiche del risultato del filtro.

  Ritorna una stringa indicante il riferimento completo alla tabella temporanea in 
  caso di successo altrimenti nil.
-*/  
/*********************************************************/
int gs_filterPrepareStatistics(void)
{
   C_STRING TempTabName;
   acedRetNil();
   
   if (gsc_filterPrepareStatistics(TempTabName, GS_LSFILTER, GS_GOOD) == GS_BAD) return RTERROR;
   acedRetStr(TempTabName.get_name());

   return RTNORM;
}


/********************************************************************/
/*.doc gsc_filterPrepareStatistics                       <external> */
/*+
  Questa funzione, prepara una tabella temporanea per eseguire il calcolo
  delle statistiche del risultato del filtro.
  Parametri:
  C_STRING &TempTabName;   Riferimento completo della tabella temporanea (out)
  C_LINK_SET &LS;          Link Set delle entità da elaborare
  int CounterToVideo;      Flag, se = GS_GOOD stampa a video il numero di entità che si 
                           stanno elaborando (default = GS_BAD)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/********************************************************************/
int gsc_filterPrepareStatistics(C_STRING &TempTabName, C_LINK_SET &LS, int CounterToVideo)
{
   C_CLASS        *pCls;
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (LS.ptr_KeyList()->get_count() == 0 || LS.cls == 0)
   {
      acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
      return GS_GOOD;
   }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(LS.cls, LS.sub)) == NULL)
      return GS_BAD;
   if (!pCls->ptr_attrib_list()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if (pConn->GetTempTabName(TableRef.get_name(), TempTabName) == GS_BAD) return GS_BAD;
   // esporto il risultato in una tabella temporanea
   if (gsc_filterExport2Db(pCls, *(LS.ptr_KeyList()),
                           pConn, TempTabName.get_name(), _T("w"), CounterToVideo) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_filterTerminateStatistics           <external> */
/*+
  Funzione LISP, cancella una tabella temporanea per eseguire il calcolo
  delle statistiche del risultato del filtro.
  Parametri:
  <nome della tabella temporanea>

  Ritorna True in caso di successo altrimenti nil.
-*/  
/*********************************************************/
int gs_filterTerminateStatistics(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();
   // nome attributo
   if (arg->restype != RTSTR || !arg->resval.rstring)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Se non c'erano oggetti risultanti dall'ultimo filtro allora il nome
   // della tabella temporanea è stringa ""
   if (wcslen(arg->resval.rstring) == 0) return RTNORM;

   if (gsc_filterTerminateStatistics(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/********************************************************************/
/*.doc gsc_filterTerminateStatistics                     <external> */
/*+
  Questa funzione, cancella una tabella temporanea per eseguire il calcolo
  delle statistiche del risultato del filtro.
  Parametri:
  const TCHAR *TempTabName;  Nome della tabella temporanea
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/********************************************************************/
int gsc_filterTerminateStatistics(const TCHAR *TempTabName)
{
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TempTabName, ONETEST) == GS_GOOD)
      if (pConn->DelTable(TempTabName) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_filterGetStatistics                 <external> */
/*+
  Funzione LISP per il calcolo delle statistiche del risultato del filtro.
  Parametri:
  (<riferimento tabella temp><attributo>)

  Ritorna una lista di valori così composta:
  (<n. righe><n.righe con attrib non nullo><max><min><media><somma>,<devStd>,<var>)
-*/  
/*********************************************************/
int gs_filterGetStatistics(void)
{
   presbuf   arg = acedGetArgs();
   C_RB_LIST ret;
   TCHAR     *TempTabName;

   acedRetNil();
   
   // riferimento tabella temporanea
   if (arg->restype != RTSTR || !arg->resval.rstring)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   
   // Se non c'erano oggetti risultanti dall'ultimo filtro allora il nome
   // della tabella temporanea è stringa ""
   if (wcslen(arg->resval.rstring) == 0) return RTNORM;

   TempTabName = arg->resval.rstring;
   // nome attributo
   if (!(arg = arg->rbnext) || arg->restype != RTSTR ||
       !arg->resval.rstring || wcslen(arg->resval.rstring) == 0)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   
   if ((ret << gsc_filterGetStatistics(TempTabName, arg->resval.rstring, GS_GOOD)) == NULL) return RTERROR;
   ret.LspRetList();

   return RTNORM;
}


/********************************************************************/
/*.doc gsc_filterGetStatistics                             <external> */
/*+
  Questa funzione, esegue il calcolo delle statistiche del risultato del filtro.
  Parametri:
  const TCHAR *TempTabName; Riferimento tabella temporanea (vedi "gsc_filterPrepareStatistics")
  const TCHAR *AttribName;  Nome dell'attributo su cui effettuare i calcoli
  int CounterToVideo;       Flag, se = GS_GOOD stampa a video il numero di entità che si 
                            stanno elaborando (default = GS_BAD)

  La funzione restituisce una lista resbuf contenente nell'ordine i seguenti valori:
  <n. righe>,<n.righe con attrib non nullo>,<max>,<min>,<media>,<somma>,<devStd>,<var>
-*/  
/********************************************************************/
presbuf gsc_filterGetStatistics(const TCHAR *TempTabName, const TCHAR *AttribName,
                                int CounterToVideo)
{
   C_RB_LIST      Result, Values;
   presbuf        p_rb;
   C_CLASS        *pCls;
   C_ATTRIB       *pAttrib;
   C_DBCONNECTION *pConn;
   C_STRING       statement, FieldName(AttribName);
   _RecordsetPtr  pRs;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return NULL; }

   if ((GS_LSFILTER.ptr_SelSet()->length() == 0 && GS_LSFILTER.ptr_KeyList()->get_count() == 0) ||
       GS_LSFILTER.cls == 0)
   {
      acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
      return NULL;
   }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(GS_LSFILTER.cls, GS_LSFILTER.sub)) == NULL)
      return GS_BAD;

   if (!pCls->ptr_attrib_list()) { GS_ERR_COD = eGSInvClassType; return NULL; }
   if (!(pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttribName, FALSE)))
      { GS_ERR_COD = eGSInvAttribName; return NULL; }

   if (pAttrib->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == NULL)
      return NULL;

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn) == GS_BAD) return NULL;

   if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return NULL;

   statement = _T("SELECT COUNT(*), COUNT(");
   statement += FieldName;
   statement += _T("), MAX(");
   statement += FieldName;
   statement += _T("), MIN(");
   statement += FieldName;
   statement += _T(')');

   if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD)
   {
      statement += _T(", AVG(");
      statement += FieldName;
      statement += _T("), SUM(");
      statement += FieldName;
      statement += _T("), STDEV(");
      statement += FieldName;
      statement += _T("), VAR(");
      statement += FieldName;
      statement += _T(')');
   }

   statement += _T(" FROM ");
   statement += TempTabName;

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return NULL;
   if (gsc_DBReadRow(pRs, Values) == GS_BAD)
      { gsc_DBCloseRs(pRs); return NULL; }
   gsc_DBCloseRs(pRs);
   
   // COUNT(*)
   if ((p_rb = Values.nth(0)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // COUNT(<attrib>)
   if ((p_rb = Values.nth(1)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // MAX
   if ((p_rb = Values.nth(2)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // MIN
   if ((p_rb = Values.nth(3)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // AVG
   if ((p_rb = Values.nth(4)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // SUM
   if ((p_rb = Values.nth(5)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // STDEV
   if ((p_rb = Values.nth(6)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
   // VAR
   if ((p_rb = Values.nth(7)) && (p_rb = gsc_nth(1, p_rb)))
   {
      if ((Result += gsc_copybuf(p_rb)) == NULL) return NULL;
   }
   else if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;

   Result.ReleaseAllAtDistruction(GS_BAD);

   return Result.get_head();
}


/*********************************************************/
/*.doc gs_filterExport2File <external> */
/*+
  Funzione LISP per l'esportazione del risultato del filtro 
  in un file testo.
  Parametri:
  (file_out [mode [delimiter [open_file]]])

  <mode>:
  "w"  Opens an empty file for writing.
       If the given file exists, its contents are destroyed.
  "a"  Opens for writing at the end of the file (appending) without removing 
       the EOF marker before writing new data to the file; 
       creates the file first if it doesn’t exist.
  "r+" Opens for both reading and writing. (The file must exist.)
  "w+" Opens an empty file for both reading and writing. 
       If the given file exists, its contents are destroyed.
  "a+" Opens for reading and appending; the appending operation includes 
       the removal of the EOF marker before new data is written to the file and 
       the EOF marker is restored after writing is complete; 
       creates the file first if it doesn’t exist.

  Ritorna RTNORM in caso di successo altrimenti ritorna RTERROR.
-*/  
/*********************************************************/
int gs_filterExport2File(void)
{
   presbuf  arg = acedGetArgs();
   TCHAR    *out_path_file;
   int 	   mode = DELIMITED;
   C_STRING delimiter(","), open_file("w");
   C_CLASS  *pCls;

   acedRetNil();

   // nome del file in output
   if (arg->restype != RTSTR || !arg->resval.rstring || wcslen(arg->resval.rstring) == 0)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   out_path_file = arg->resval.rstring;

   // modalità di output
   if ((arg = arg->rbnext) != NULL)
   {
      if (arg->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      mode = (int) arg->resval.rint;
      // stringa delimitatrice
      if ((arg = arg->rbnext) != NULL)
      {
         if (arg->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         delimiter = arg->resval.rstring;

         // modalità di apertura file
         if ((arg = arg->rbnext) != NULL)
         {
            if (arg->restype != RTSTR)
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            open_file= arg->resval.rstring;
         }
      }
   }

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(GS_LSFILTER.cls, GS_LSFILTER.sub)) == NULL)
      return GS_BAD;
   if (!pCls->ptr_attrib_list()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((GS_LSFILTER.ptr_SelSet()->length() == 0 && GS_LSFILTER.ptr_KeyList()->get_count() == 0) ||
       GS_LSFILTER.cls == 0)
      acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
   else
      if (gsc_filterExport2File(pCls, *(GS_LSFILTER.ptr_KeyList()),
                                out_path_file, open_file.get_name(), mode,
                                delimiter.get_name(), GS_GOOD) != GS_GOOD)
         return GS_BAD;
   acedRetT();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_sensorExport2File <external> */
/*+
  Funzione LISP per l'esportazione dei dati delle entità 
  rilevate dall'ultima ricerca per sensori in un file.
  Parametri:
  (file_out [mode [delimiter [open_file]]])

  <mode>:
  "w"  Opens an empty file for writing.
       If the given file exists, its contents are destroyed.
  "a"  Opens for writing at the end of the file (appending) without removing 
       the EOF marker before writing new data to the file; 
       creates the file first if it doesn’t exist.
  "r+" Opens for both reading and writing. (The file must exist.)
  "w+" Opens an empty file for both reading and writing. 
       If the given file exists, its contents are destroyed.
  "a+" Opens for reading and appending; the appending operation includes 
       the removal of the EOF marker before new data is written to the file and 
       the EOF marker is restored after writing is complete; 
       creates the file first if it doesn’t exist.

  Ritorna RTNORM in caso di successo altrimenti ritorna RTERROR.
-*/  
/*********************************************************/
int gs_sensorExport2File(void)
{
   FILE         *f;
   int          cls, sub, Result = GS_GOOD;
   int 	       mode = DELIMITED;
   presbuf      arg = acedGetArgs(), pname, pattr;
   TCHAR        *out_path_file;
   C_STRING     delimiter(","), open_file("w"), drive, dir, name, ext, ExpPath;
   C_STRING     Prefix, Mask, Path;
   C_CLASS      *pCls;
   long         key;
   C_LONG_BTREE KeyList;
   TCHAR        *Sep;

   acedRetNil();

   // nome del file in output
   if (arg->restype != RTSTR || !arg->resval.rstring || wcslen(arg->resval.rstring) == 0)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   out_path_file = arg->resval.rstring;

   // modalità di output
   if ((arg = arg->rbnext) != NULL)
   {
      if (arg->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      mode = (int) arg->resval.rint;
      // stringa delimitatrice
      if ((arg = arg->rbnext) != NULL)
      {
         if (arg->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         delimiter = arg->resval.rstring;

         // modalità di apertura file
         if ((arg = arg->rbnext) != NULL)
         {
            if (arg->restype != RTSTR)
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            open_file = arg->resval.rstring;
         }
      }
   }

   Prefix = GEOsimAppl::CURRUSRDIR;
   Prefix += _T('\\');
   Prefix += GEOTEMPDIR;
   Prefix += _T("\\");
   Mask = Prefix;
   Mask += _T("SENSOR_*.TXT");

   // ciclo eventuali file temporanei SENSOR*.TXT
   if (gsc_adir(Mask.get_name(), &pname, NULL, NULL, &pattr) > 0)
   {
      while (pname) 
      {
         // NON è un direttorio          
         if (pname->restype == RTSTR && *(pattr->resval.rstring + 4) != _T('D'))
         {
            Path = Prefix;
            Path += pname->resval.rstring;

            if ((f = gsc_fopen(Path, _T("r"))) == NULL)
               { Result = GS_BAD; break; }
            // Leggo codice e sottocodice della classe
            if (fwscanf(f, _T("%d,%d"), &cls, &sub) == EOF)
               { gsc_fclose(f); Result = GS_BAD; break; }
            if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL)
               { gsc_fclose(f); Result = GS_BAD; break; }
            if (!pCls->ptr_attrib_list())
               { gsc_fclose(f); Result = GS_BAD; GS_ERR_COD = eGSInvClassType; break; }
            KeyList.remove_all();
            // Leggo i codici delle entità
            while (fwscanf(f, _T("%ld"), &key) != EOF)
               KeyList.add(&key);
            gsc_fclose(f);

            // Aggiungo il postfisso "_<codice sensore>" al nome del file
            gsc_splitpath(out_path_file, &drive, &dir, &name, &ext);

            name += (pname->resval.rstring + wcslen(_T("SENSOR")));
            if ((Sep = wcsrchr(name.get_name(), _T('.'))) != NULL) *Sep = _T('\0');
            ExpPath = drive;
            ExpPath += dir;
            ExpPath += name;
            ExpPath += ext;

            if (gsc_filterExport2File(pCls, KeyList,
                                      ExpPath.get_name(), open_file.get_name(), mode,
                                      delimiter.get_name(), GS_GOOD) != GS_GOOD)
               { Result = GS_BAD; break; }
         }

         pname = pname->rbnext;
         pattr = pattr->rbnext;
      }
      acutRelRb(pname); acutRelRb(pattr);
   }

   if (Result == GS_GOOD) acedRetT();

   return Result;
}


/********************************************************************/
/*.doc gsc_filterExport2File                             <external> */
/*+
  Questa funzione, esporta in un file testo il risultato del filtro.

  Parametri:
  C_CLASS *pCls;              Puntatore alla classe da esportare
  C_LONG_BTREE &KeyList;      Puntatore all lista dei codici delle entità della classe
  const TCHAR *out_path_file; Path completo del file di testo in output
  const TCHAR *FileMode;      Modalità di apertura del file (append, create...)
  int mode;                   Modalità di export (DELIMITED, SDF, SDF_HEADER)
  TCHAR *delimiter;           Stringa delimitatrice di fine colonna   
  int CounterToVideo;         Flag, se = GS_GOOD stampa a video il numero di entità che si 
                              stanno elaborando (default = GS_BAD)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/********************************************************************/
int gsc_filterExport2File(C_CLASS *pCls, C_LONG_BTREE &KeyList,
                          const TCHAR *out_path_file, const TCHAR *FileMode,
                          int mode, TCHAR *delimiter, int CounterToVideo)
{
   long          key, written = 0, qty = 0;
   int           result = GS_BAD;
   FILE          *fhandle = NULL;
   C_STR_LIST    Formats;
   C_RB_LIST     AllColValues, SingleColValues;
   presbuf       pRb;
   C_LONG_BTREE  PartialKeyList;
   C_BLONG       *pKey = (C_BLONG *) KeyList.go_top();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(234)); // "Esportazione dati"

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(KeyList.get_count());

   if ((fhandle = gsc_fopen(out_path_file, FileMode)) == NULL) return GS_BAD;

   do
   {
      if (pCls->ptr_attrib_list()->GetTxtFileAttribFormats(mode, delimiter, Formats) == GS_BAD)
         break;
      if (mode == SDF_HEADER) // stampo intestazione
         if (pCls->ptr_attrib_list()->sprintfHeader(fhandle, Formats) == GS_BAD)
            break;

      result = GS_GOOD;
      while (pKey)
      {
         key = pKey->get_key();
         PartialKeyList.add(&key);
         qty++;

         if ((qty % VERY_LARGE_STEP) == 0) // ogni 10000 per non avere problemi di out of memory
         {
            if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
               { result = GS_BAD; break; }
            PartialKeyList.remove_all();

            pRb = AllColValues.get_head();
            if (pRb) pRb = pRb->rbnext;

            while (pRb && pRb->restype == RTLB)
            {
               if (gsc_DBsprintf(fhandle, pRb, Formats) == GS_BAD)
                  { result = GS_BAD; break; }

               if ((pRb = gsc_scorri(pRb))) // vado alla chiusa tonda successiva
                  pRb = pRb->rbnext; // vado alla aperta tonda successiva

               written++;
               if (CounterToVideo == GS_GOOD)
                  StatusBarProgressMeter.Set(written);
            }
         }

         pKey = (C_BLONG *) KeyList.go_next();
      }

      if (result == GS_BAD) break;

      if (PartialKeyList.get_count() > 0)
      {
         if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
            { result = GS_BAD; break; }

         pRb = AllColValues.get_head();
         if (pRb) pRb = pRb->rbnext;

         while (pRb && pRb->restype == RTLB)
         {
            if (gsc_DBsprintf(fhandle, pRb, Formats) == GS_BAD)
               { result = GS_BAD; break; }

            if ((pRb = gsc_scorri(pRb))) // vado alla chiusa tonda successiva
               pRb = pRb->rbnext; // vado alla aperta tonda successiva

            written++;
            if (CounterToVideo == GS_GOOD)
               StatusBarProgressMeter.Set(written);
         }
      }
   }
   while (0);

   if (fhandle)
      if (gsc_fclose(fhandle) == GS_BAD) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(310), qty); // "%ld entità GEOsim elaborate."
      acutPrintf(gsc_msg(1047), qty); // "\nAttendere rilascio memoria...\n"
   }

   return result;
}


/*********************************************************/
/*.doc gs_filterExport2Db <external> */
/*+
  Esporta il risultato dell'ultimo filtro su database.
  (<OLE_DB conn> <mode>)
  dove:
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>))
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)

  <mode> = "w"  Opens an empty file for writing.
                If the given file exists, its contents are destroyed.
           "a"  Opens for writing at the end of the file (appending) without removing 
                the EOF marker before writing new data to the file; 
                creates the file first if it doesn’t exist.

-*/  
/*********************************************************/
int gs_filterExport2Db(void)
{
   presbuf        arg = acedGetArgs();
   C_CLASS        *pCls;
   C_STRING       Table;
   C_DBCONNECTION *pConn;
   C_STRING       TabMode("w");

   acedRetNil();

   if ((GS_LSFILTER.ptr_SelSet()->length() == 0 && GS_LSFILTER.ptr_KeyList()->get_count() == 0) ||
       GS_LSFILTER.cls == 0)
   {
      acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
      acedRetT();
      return GS_GOOD;
   }

   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg), &Table)) == NULL) return RTERROR;

   // modalità di apertura file
   if ((arg = gsc_nth(1, arg)))
   {
      if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      TabMode = arg->resval.rstring;
   }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(GS_LSFILTER.cls, GS_LSFILTER.sub)) == NULL)
      return GS_BAD;
   if (!pCls->ptr_attrib_list()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (gsc_filterExport2Db(pCls, *(GS_LSFILTER.ptr_KeyList()),
                           pConn, Table.get_name(), TabMode.get_name(), GS_GOOD) != GS_GOOD)
      return GS_BAD;

   // Se il database di esportazione non è gestibile in multiutenza (es. EXCEL)
   // chiudo la connessione per permettere all'utente di aprire per conto suo
   // il risultato dell'esportazione
   if (pConn->get_IsMultiUser() == FALSE)
      GEOsimAppl::TerminateSQL(pConn);

   acedRetT();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_sensorExport2Db                     <external> */
/*+
  Funzione LISP per l'esportazione dei dati delle entità 
  rilevate dall'ultima ricerca per sensori in un database.
  (<OLE_DB conn> <mode>)
  dove:
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>))
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)
  <mode> = "w"  Opens an empty file for writing.
                If the given file exists, its contents are destroyed.
           "a"  Opens for writing at the end of the file (appending) without removing 
                the EOF marker before writing new data to the file; 
                creates the file first if it doesn’t exist.

-*/  
/*********************************************************/
int gs_sensorExport2Db(void)
{
   presbuf        arg = acedGetArgs(), pname, pattr;
   C_STRING       TableRef, Catalog, Schema, Table, ExpTableRef;
   C_CLASS        *pCls;
   C_DBCONNECTION *pConn;
   C_STRING       TabMode("w"), Mask, Prefix, Path;
   FILE           *f;
   int            cls, sub, Result = GS_GOOD;
   long           key;
   C_LONG_BTREE   KeyList;
   TCHAR          *Sep;

   acedRetNil();

   if ((pConn = gsc_getConnectionFromLisp(gsc_nth(0, arg), &TableRef)) == NULL) return RTERROR;

   // modalità di apertura file
   if ((arg = gsc_nth(1, arg)))
   {
      if (arg->restype != RTSTR)
      {
         if (pConn->get_IsMultiUser() == FALSE)
            GEOsimAppl::TerminateSQL(pConn);

         GS_ERR_COD = eGSInvalidArg;
         return RTERROR;
      }

      TabMode = arg->resval.rstring;
   }

   Prefix = GEOsimAppl::CURRUSRDIR;
   Prefix += _T('\\');
   Prefix += GEOTEMPDIR;
   Prefix += _T("\\");
   Mask = Prefix;
   Mask += _T("SENSOR_*.TXT");

   // ciclo eventuali file temporanei SENSOR*.TXT
   if (gsc_adir(Mask.get_name(), &pname, NULL, NULL, &pattr) > 0)
   {
      while (pname) 
      {
         // NON è un direttorio          
         if (pname->restype == RTSTR && *(pattr->resval.rstring + 4) != _T('D'))
         {
            Path = Prefix;
            Path += pname->resval.rstring;

            if ((f = gsc_fopen(Path, _T("r"))) == NULL)
               { Result = GS_BAD; break; }
            // Leggo codice e sottocodice della classe
            if (fwscanf(f, _T("%d,%d"), &cls, &sub) == EOF)
               { gsc_fclose(f); Result = GS_BAD; break; }
            if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL)
               { gsc_fclose(f); Result = GS_BAD; break; }
            if (!pCls->ptr_attrib_list())
               { gsc_fclose(f); Result = GS_BAD; GS_ERR_COD = eGSInvClassType; break; }
            KeyList.remove_all();
            // Leggo i codici delle entità
            while (fwscanf(f, _T("%ld"), &key) != EOF)
               KeyList.add(&key);
            gsc_fclose(f);

            // Aggiungo il postfisso "_<codice sensore>" al nome della tabella
            if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
               { Result = GS_BAD; break; }
            Table += (pname->resval.rstring + wcslen(_T("SENSOR")));
            if ((Sep = wcsrchr(Table.get_name(), _T('.'))) != NULL) *Sep = _T('\0');
            ExpTableRef.paste(pConn->get_FullRefTable(Catalog, Schema, Table));

            if (gsc_filterExport2Db(pCls, KeyList,
                                    pConn, ExpTableRef.get_name(), TabMode.get_name(),
                                    GS_GOOD) != GS_GOOD)
               { Result = GS_BAD; break; }
         }

         pname = pname->rbnext;
         pattr = pattr->rbnext;
      }
      acutRelRb(pname); acutRelRb(pattr);
   }

   // Se il database di esportazione non è gestibile in multiutenza (es. EXCEL)
   // chiudo la connessione per permettere all'utente di aprire per conto suo
   // il risultato dell'esportazione
   if (pConn->get_IsMultiUser() == FALSE)
      GEOsimAppl::TerminateSQL(pConn);

   if (Result == GS_GOOD) acedRetT();

   return Result;
}


/*********************************************************************/
/*.doc gsc_filterExport2Db <external> */
/*+
  Questa funzione, esporta in una tabella il risultato del filtro.
  Parametri:
  C_CLASS *pCls;            Puntatore alla classe da esportare
  C_LONG_BTREE &KeyList;    Puntatore all lista dei codici delle entità della classe
  C_DBCONNECTION *pOutConn; Connessione OLE-DB per tabella di esportazione
  const TCHAR *TableRef;    Riferimento a tabella in uscita
  const TCHAR *TabMode;     Modalità di apertura della tabella ("w" per create,
                            "a" per append)
  int CounterToVideo;       flag, se = GS_GOOD stampa a video il numero di entità 
                            che si stanno elaborando (default = GS_BAD)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_filterExport2Db(C_CLASS *pCls, C_LONG_BTREE &KeyList,
                        C_DBCONNECTION *pOutConn, const TCHAR *OutTableRef, 
                        const TCHAR *TabMode, int CounterToVideo)
{
   _RecordsetPtr pInsRs;
   long          key, written = 0, qty = 0;
   int           result = GS_BAD;
   C_RB_LIST     AllColValues, SingleColValues;
   presbuf       pRb;
   C_LONG_BTREE  PartialKeyList;
   C_BLONG       *pKey = (C_BLONG *) KeyList.go_top();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(234)); // "Esportazione dati"

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(KeyList.get_count());

   do
   {
      if (!TabMode || wcsicmp(TabMode, _T("w")) == 0) // tabella da creare
      {
         C_DBCONNECTION *pConn;
         C_STRING       TempTableRef;

         if (pOutConn->ExistTable(OutTableRef) == GS_GOOD)
            if (pOutConn->DelTable(OutTableRef) == GS_BAD) break; // cancello tabella
         // ricavo tabella temporanea
         if (pCls->getTempTableRef(TempTableRef) == GS_BAD) break;
         pConn = pCls->ptr_info()->getDBConnection(TEMP);

         // Copio la struttura senza indici
         if (pConn->CopyTable(TempTableRef.get_name(), *pOutConn, OutTableRef,
                              GSStructureOnlyCopyType, GS_BAD) == GS_BAD)
            break;
      }
      else // tabella a cui appendere         
         if (wcsicmp(TabMode, _T("a")) == 0)
         {
            if (pOutConn->ExistTable(OutTableRef) == GS_BAD)
               { GS_ERR_COD = eGSTableNotExisting; break; }
         }
         else
            { GS_ERR_COD = eGSInvalidArg; break; }           

      // preparo RecordSet per l'inserimento di record nella tabella out
      if (pOutConn->InitInsRow(OutTableRef, pInsRs) == GS_BAD) break;

      StatusBarProgressMeter.Init(KeyList.get_count());

      result = GS_GOOD;
      while (pKey)
      {
         key = pKey->get_key();
         PartialKeyList.add(&key);
         qty++;

         if ((qty % VERY_LARGE_STEP) == 0) // ogni 10000 per non avere problemi di out of memory
         {
            if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
               { result = GS_BAD; break; }
            PartialKeyList.remove_all();

            pRb = AllColValues.get_head();
            if (pRb) pRb = pRb->rbnext;

            while (pRb && pRb->restype == RTLB)
            {
               if (gsc_DBInsRow(pInsRs, pRb, ONETEST, GS_GOOD) != GS_GOOD)
                  { result = GS_BAD; break; }

               if ((pRb = gsc_scorri(pRb))) // vado alla chiusa tonda successiva
                  pRb = pRb->rbnext; // vado alla aperta tonda successiva

               written++;
               if (CounterToVideo == GS_GOOD)
                  StatusBarProgressMeter.Set(written);
            }
         }

         pKey = (C_BLONG *) KeyList.go_next();
      }

      if (result == GS_BAD) break;

      if (PartialKeyList.get_count() > 0)
      {
         if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
            { result = GS_BAD; break; }

         pRb = AllColValues.get_head();
         if (pRb) pRb = pRb->rbnext;

         while (pRb && pRb->restype == RTLB)
         {
            if (gsc_DBInsRow(pInsRs, pRb, ONETEST, GS_GOOD) != GS_GOOD)
               { result = GS_BAD; break; }

            if ((pRb = gsc_scorri(pRb))) // vado alla chiusa tonda successiva
               pRb = pRb->rbnext; // vado alla aperta tonda successiva

            written++;
            if (CounterToVideo == GS_GOOD)
               StatusBarProgressMeter.Set(written);
         }
      }
   }
   while (0);

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(310), qty); // "%ld entità GEOsim elaborate."
      acutPrintf(gsc_msg(1047), qty); // "\nAttendere rilascio memoria...\n"
   }

   return result;
}


/*********************************************************************/
/*.doc gsc_get_filtered_entities <external> */
/*+
  Questa funzione, recupera i codici delle schede filtrate e le inserisce (se
  non è stato già fatto) in GS_VERIFIED.

  Parametri:
  int CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_get_filtered_entities(int CounterToVideo)
{
   C_CLASS *pCls;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // se il Link-Set è vuoto allora esco
   if (GS_LSFILTER.ptr_KeyList()->get_count() == 0 || GS_LSFILTER.cls == 0)
   {
      acutPrintf(gsc_msg(117), 0); // "\n%ld oggetti grafici filtrati."
      return GS_GOOD;
   }

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(GS_LSFILTER.cls, GS_LSFILTER.sub)) == NULL)
      return GS_BAD;

   if (pCls->ptr_attrib_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (pCls->ptr_group_list() == NULL)
   { // ha riferimenti grafici diretti
      C_DBCONNECTION *pConn;
      _RecordsetPtr  pRs;
      C_STRING       TableRef, statement;

      if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD)
          return GS_BAD;
      
      statement = _T("SELECT * FROM ");
      statement += TableRef;
      if (pConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;
      
      if (gsc_isEOF(pRs) == GS_GOOD)
      {  // se la tabella era vuota la riempio dal gruppo di selezione in GS_LSFILTER
         // altrimenti se la tabella non è vuota la uso così com'è
         gsc_DBCloseRs(pRs);

         if (CounterToVideo == GS_GOOD)
            acutPrintf(gsc_msg(113), pCls->ptr_id()->name); //	"\n\nEntità classe %s:\n"

         if (pCls->SS_to_GS_VERIFIED(*(GS_LSFILTER.ptr_SelSet()), CounterToVideo) == GS_BAD) return GS_BAD;

         if (CounterToVideo == GS_GOOD) acutPrintf(GS_LFSTR);
      }
      gsc_DBCloseRs(pRs);
   }

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_SpatialSel_Load <external> */
/*+
  Questa funzione, recupera le condizioni spaziali in un file di query
  di ADE (che sono ancora in ANSI).
  Parametri:
  FILE *file;                 Puntatore a file testo 
  C_STR_LIST &SpatialSelList; Lista delle condizioni spaziali (out)
                              secondo il formato usato da ADE (vedi manuale)
                              "AND" "(" "NOT" "Location" ("window" "crossing" (x1 y1 z1) (x2 y2 z2)) ")"

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_SpatialSel_Load(FILE *file, C_STR_LIST &SpatialSelList)   
{
   TCHAR    *pStart, *pEnd;
   TCHAR    ADEQueryKeyWord[] = _T("(ade_qrydefine '(");
   TCHAR    ADELocationKeyWord[] = _T("\"Location\" (");
   long     curpos;   
   int      OpenBracket, Result = GS_GOOD;
   C_STRING Row, SpatialSel;
   C_STR    *pSpatialSel;

   SpatialSelList.remove_all();
   curpos = ftell(file); // memorizzo la posizione del cursore nel file

   // leggo riga per riga NON Unicode (i file .QRY sono ancora in ANSI)
   while (gsc_readline(file, Row, false) == GS_GOOD)
   {
      Row.alltrim();
      // Se la riga non inizia per "(ade_qrydefine '("
      if ((pStart = Row.at(ADEQueryKeyWord)) != Row.get_name()) continue;
      // Se la riga non contiene la parola "\"Location\" ("
      if (Row.at(ADELocationKeyWord) == NULL) continue;

      pStart += gsc_strlen(ADEQueryKeyWord);
      pEnd = pStart;

      OpenBracket = 1;
      while (OpenBracket > 0 && *(++pEnd) != _T('\0'))
         switch (*pEnd)
         {
            case _T('('): OpenBracket++; break;
            case _T(')'): OpenBracket--; break;
         }
      if (OpenBracket > 0) { GS_ERR_COD = eGSBadLocationQry; Result = GS_BAD; break; }

      if (SpatialSel.set_name(pStart, 0, (int) (pEnd - pStart - 1)) == GS_BAD)
         { GS_ERR_COD = eGSOutOfMem; Result = GS_BAD; break; }

      if ((pSpatialSel = new C_STR(SpatialSel.get_name())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; Result = GS_BAD; break; }
      SpatialSelList.add_tail(pSpatialSel);
   }

   fseek(file, curpos, SEEK_SET);

   return Result;
}


/*********************************************************************/
/*.doc gsc_SpatialSel_Save                                <external> */
/*+
  Questa funzione, scrive le condizioni spaziali in un file di query
  di ADE (che sono ancora in ANSI).
  Parametri:
  FILE *file;                 Puntatore a file testo 
  C_STR_LIST &SpatialSelList; Lista delle condizioni spaziali
                              secondo il formato usato da ADE (vedi manuale)
                              "AND" "(" "NOT" "window" "crossing" (x1 y1 z1) (x2 y2 z2)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_SpatialSel_Save(FILE *file, C_STRING &SpatialSel)
{
   C_STRING Row;

   Row = _T("(ade_qrydefine '(");
   Row += SpatialSel;
   Row += _T("))\n");
   if (fwprintf(file, Row.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}
int gsc_SpatialSel_Save(FILE *file, C_STR *pSpatialSel)
{
   C_STRING SpatialSel(pSpatialSel->get_name());

   return gsc_SpatialSel_Save(file, SpatialSel);
}
int gsc_SpatialSel_Save(FILE *file, C_STR_LIST &SpatialSelList)
{
   C_STR *pSpatialSel = (C_STR *) SpatialSelList.get_head();

   while (pSpatialSel)
   {
      if (gsc_SpatialSel_Save(file, pSpatialSel) == GS_BAD) return GS_BAD;
      pSpatialSel = (C_STR *) SpatialSelList.get_next();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_splitSQLCond                                                      */
/*+
  Questa funzione effettua la scomposizione di una condizione SQL in formato
  ADE, estraendo il LPN e la condizione SQL.
  Parametri:
  C_STRING &SQLCond;   Condizione SQL secondo il formato usato da ADE 
                       (vedi manuale) ad esempio:
                       ("PRJ6CLS6OLD_LINK" "GS_ID > 0 AND VALORE > 0")
  C_STRING &LPName;
  C_STRING &Cond;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione corregge i caratteri di controllo \\ in \, \" in ".
-*/  
/*****************************************************************************/
int gsc_splitSQLCond(C_STRING &SQLCond, C_STRING &LPName, C_STRING &Cond)
{
   TCHAR *p, *pCond = SQLCond.get_name();
   int   Start, End;

   LPName.clear();
   Cond.clear();

   if (SQLCond.len() == 0) return GS_GOOD;

   // cerco il LPName
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;

   while (pCond[End] != _T('\0'))
      if (pCond[End] == _T('"') && End-1 >= 0 && pCond[End-1] != _T('\\')) break;
      else End++;
   
   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   if (End > Start)
   {
      if (LPName.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;
      // caratteri speciali  
      LPName.strtran(_T("\\\\"), _T("\\")); // "\\" diventa "\"
      LPName.strtran(_T("\\\""), _T("\"")); // "\\" diventa "\"
   }
   pCond = pCond + End + 1;
   // cerco la condizione
   if ((p = wcschr(pCond, _T('"'))) == NULL) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   End = Start = (int) (p - pCond) + 1;
   while (pCond[End] != _T('\0'))
      if (pCond[End] == _T('"') && End-1 >= 0 && pCond[End-1] != _T('\\')) break;
      else End++;

   if (pCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   
   if (Cond.set_name(pCond, Start, End - 1) == GS_BAD) return GS_BAD;

   // caratteri speciali  
   Cond.strtran(_T("\\\\"), _T("\\")); // "\\" diventa "\"
   Cond.strtran(_T("\\\""), _T("\"")); // "\\" diventa "\"

   return GS_GOOD;
}
 

/*****************************************************************************/
/*.doc gsc_splitSQLAggrCond                                                  */
/*+
  Questa funzione effettua la scomposizione di una condizione di aggregazione
  SQL, estraendo la funzione di aggregazione (SUM, COUNT ...), il nome dell'attributo,
  l'operatore (=, >, < ...) ed il valore di riferimento.
  Parametri:
  C_STRING &SQLAggrCond;   Condizione SQL ad esempio:
                          "COUNT(DISTINCT N_GUASTO) > 10"
  C_STRING &AggrFun;
  C_STRING &AttribName;
  C_STRING &Operator;
  C_STRING &Value;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_splitSQLAggrCond(C_STRING &SQLAggrCond, C_STRING &AggrFun, C_STRING &AttribName,
                         C_STRING &Operator, C_STRING &Value)
{
   TCHAR *p, *pAggrCond = SQLAggrCond.get_name();
   int   Start, End;

   if (SQLAggrCond.len() == 0) return GS_GOOD;

   // cerco la funzione di aggregazione
   if ((p = wcschr(pAggrCond, _T('('))) == NULL) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   End = (int) (p - pAggrCond);
   if (AggrFun.set_name(pAggrCond, 0, End - 1) == GS_BAD) return GS_BAD;
   // alltrim e upper
   AggrFun.alltrim();
   AggrFun.toupper();
   
   if (pAggrCond[End + 1] == _T('*'))
   {
      pAggrCond  = pAggrCond + End + 3;
      AttribName = _T('*');
   }
   else
   {
      if (gsc_strstr(pAggrCond + End + 1, _T("DISTINCT"), FALSE) == pAggrCond + End + 1)
      {
         pAggrCond  = pAggrCond + End + wcslen(_T("DISTINCT")) + 1;
         AttribName = _T("DISTINCT");
      }
      else
      if (pAggrCond[End + 1] == _T('\"')) // nome di un attributo
      {
         pAggrCond = pAggrCond + End + 2;
         End = 0;
         while (pAggrCond[End] != _T('\0') && pAggrCond[End] != _T('\"') && 
                pAggrCond[End] != _T(')')) End++;
         if (pAggrCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
         if (AttribName.set_name(pAggrCond, 0, End - 1) == GS_BAD) return GS_BAD;
         pAggrCond = pAggrCond + End + 2;
      }
   }

   // alltrim e upper
   AttribName.alltrim();
   AttribName.toupper();
   if (gsc_strcmp(AttribName.get_name(), _T("DISTINCT")) == 0 ||
       gsc_strcmp(AttribName.get_name(), _T("*")) == 0)
   {
      if (gsc_strcmp(AggrFun.get_name(), _T("COUNT")) == 0)
      {
         AggrFun += _T(" ");
         AggrFun += AttribName;
      }
      else { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }

      if (gsc_strcmp(AttribName.get_name(), _T("*")) != 0)
      {
         // cerco attributo
         End = 0;
         while (pAggrCond[End] != _T('\0') && pAggrCond[End] != _T(')')) End++;
         if (pAggrCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
         if (AttribName.set_name(pAggrCond, 0, End - 1) == GS_BAD) return GS_BAD;
         pAggrCond = pAggrCond + End + 2;
      }
   }

   // cerco operatore
   Start = 0;
   while (pAggrCond[Start] != _T('\0') && pAggrCond[Start] == _T(' ')) Start++;
   if (pAggrCond[Start] == _T('\0'))
      { Operator.clear(); return GS_GOOD; }
   End = Start;
   while (pAggrCond[End] != _T('\0') && 
          (pAggrCond[End] == _T('>') || pAggrCond[End] == _T('<') ||
          pAggrCond[End] == _T('='))) End++;
   if (pAggrCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   while (pAggrCond[Start] != _T('\0') && pAggrCond[Start] == _T(' ')) Start++;
   if (pAggrCond[End] == _T('\0')) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   if (Operator.set_name(pAggrCond, Start, End - 1) == GS_BAD) return GS_BAD;
   // alltrim
   Operator.alltrim();
   pAggrCond = pAggrCond + End;

   // cerco valore di confronto
   Value = pAggrCond;
   // alltrim
   Value.alltrim();

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_fullSQLAggrCond                                                   */
/*+
  Questa funzione effettua la composizione di una condizione di aggregazione SQL.
  Parametri:
  C_STRING &AggrFun;       funzione di aggregazione
  C_STRING &AttribName;    nome attributo
  C_STRING &Operator;      operatore
  C_STRING &Value;         valore
  int      SQLType;        se TRUE la costruzione è in forma SQL
                           (senza operatore e valore)
  C_STRING &SQLAggrCond;   stringa risultante
  C_DBCONNECTION *pConn;   Connessione OLE-DB usata per stabilire i caratteri
                           delimitatori. Se = NULL si intende la sintassi di MAP
                           che utilizza il carattere " (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_fullSQLAggrCond(C_STRING &AggrFun, C_STRING &AttribName, C_STRING &Operator,
                        C_STRING &Value, int SQLType, C_STRING &SQLAggrCond,
                        C_DBCONNECTION *pConn)
{
   int SkipAttrib = FALSE;

   SQLAggrCond.clear();
   // funzione di aggregazione
   if (gsc_strcmp(AggrFun.get_name(), _T("COUNT DISTINCT"), FALSE) == 0)
   {
      SQLAggrCond = _T("COUNT(DISTINCT ");
   }
   else
   if (gsc_strcmp(AggrFun.get_name(), _T("COUNT *"), FALSE) == 0)
   {
      SQLAggrCond += _T("COUNT(*) ");
      SkipAttrib = TRUE;
   }
   else
   {
      SQLAggrCond += AggrFun;
      SQLAggrCond += _T("(");
   }

   // nome attributo
   if (!SkipAttrib)
   {
      if (!pConn)
      {
         C_STRING AttribCorrected(AttribName);

         // Correggo la sintassi del nome del campo per SQL MAP
         gsc_AdjSyntaxMAPFormat(AttribCorrected);
         SQLAggrCond += AttribCorrected;
      }
      else
      {
         C_STRING dummy(AttribName);

         if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                           pConn->get_StrCaseFullTableRef()) == GS_BAD)
            return GS_BAD;
         SQLAggrCond += dummy;
      }

      SQLAggrCond += _T(") ");
   }

   if (!SQLType)
   {
      // operatore
      SQLAggrCond += Operator;
      SQLAggrCond += _T(" ");

      // valore di riferimento
      SQLAggrCond += Value;
   }

   SQLAggrCond.alltrim();

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_SQLCond_Load <external> */
/*+
  Questa funzione, recupera la prima condizione SQL in un file di query
  di ADE.
  Parametri:
  FILE *file;           Puntatore a file testo 
  C_STRING &SQLCond;    Condizione SQL (out)
  bool        Unicode;  Flag che determina se il contenuto del file è in 
                        formato UNICODE o ANSI (default = true)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
  N.B. La funzione non modifica i caratteri di controllo \\ e \"
-*/  
/*********************************************************************/
static int gsc_SQLCond_Load(FILE *file, C_STRING &SQLCond, bool Unicode)
{
   TCHAR    *stream = NULL, *pStart, *pEnd, *pOrig_sream;
   long     FileLen, curpos;
   int      OpenBracket;
   C_STRING Orig_stream;

   SQLCond.clear();
   curpos = ftell(file);

   if ((FileLen = gsc_filesize(file)) == -1) return GS_BAD;
   if((stream = (TCHAR *) calloc((size_t) (FileLen + 2), sizeof(TCHAR))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (gsc_fread(stream, (size_t) FileLen, file, Unicode) == NULL)
      { fseek(file, curpos, SEEK_SET); GS_ERR_COD = eGSReadFile; return GS_BAD; }
   fseek(file, curpos, SEEK_SET);
   Orig_stream = stream;
   pOrig_sream = Orig_stream.get_name();

   gsc_tolower(stream);
   // questa è una versione molto semplificata che legge solo la prima condizione
   // di SQL senza interpretare gli OR o gli AND tra condizioni
   if ((pStart = pEnd = gsc_strstr(stream, _T("\"sql\" ("))) == NULL)
      { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   pStart = pEnd = gsc_strstr(pStart, _T("("));
   OpenBracket = 1;
   while (OpenBracket > 0 && *(++pEnd) != _T('\0'))
      switch (*pEnd)
      {
         case _T('('): OpenBracket++; break;
         case _T(')'): OpenBracket--; break;
      }

   if (OpenBracket > 0) { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
   if (SQLCond.set_name(pOrig_sream+(pStart-stream), 0, (int) (pEnd - pStart)) == GS_BAD)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER L'ESECUZIONE DEL FILTRO   -   INIZIO
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gs_do_sql_filter <external> */
/*+                                                                       
  Funzione LISP che restituisce un gruppo di selezione dei soli oggetti grafici
  associati al risultato del filtro SQL (variabile GS_LSFILTER).
  Parametri:
  Lista RESBUF (<cls><sub>[<gruppo di selezione>[<condizioni SQL>]])

  <condizioni SQL>:
  ((<cls> <sub> <sql> ((<sec> <aggr> <sql>) ...)) ...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*****************************************************************************/
int gs_do_sql_filter(void)
{
   presbuf  		  arg;
   int      		  cls, sub;
   C_CLASS_SQL_LIST SQLWhere;
   C_LINK_SET       LinkSet;
   C_CLASS          *pCls;
   bool             UseLinkSet = false;

   acedRetNil();
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   arg = acedGetArgs();
   // codice classe
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   cls = (int) arg->resval.rint;
   // codice sotto-classe
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   sub = (int) arg->resval.rint;

   // modo di filtro, se nil allora filtro su tutto il disegno estratto
   // altrimenti "gruppo di selezione" (opzionale)
   if ((arg = arg->rbnext) != NULL)
   {
      switch (arg->restype)
      {
         case RTNIL: break;
	      case RTPICKS: 
            *(LinkSet.ptr_SelSet()) << arg->resval.rlname; 
            UseLinkSet = true; 
            LinkSet.ptr_SelSet()->ReleaseAllAtDistruction(GS_BAD); // roby shape

            break;
         default: GS_ERR_COD = eGSInvalidArg; return RTERROR;
      }
      
      // stringa con condizioni SQL (opzionale)
		if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return RTERROR;
      if (SQLWhere.initialize(pCls) == GS_BAD) return RTERROR;
      if ((arg = arg->rbnext) != NULL && SQLWhere.from_rb(arg) == GS_BAD)
         return RTERROR;
   }
   if (gsc_do_sql_filter(cls, sub, SQLWhere, 
      (UseLinkSet) ? &LinkSet : NULL) == GS_BAD) return GS_BAD;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_do_sql_filter                      <external> */
/*+                                                                       
  Restituisce un gruppo di selezione dei soli oggetti grafici
  associati al risultato del filtro SQL (variabile GS_LSFILTER).
  Parametri:
  int cls;						      Codice classe
  int sub;						      Codice sotto-classe
  C_CLASS_SQL_LIST &SQLCondList; Condizioni SQL
  C_LINK_SET *pLinkSet;          1) Se la classe di entità ha grafica rappresenta 
                                    il gruppo di selezione degli oggetti su cui applicare il filtro;
                                 2) se la classe di entità non ha grafica rappresenta
                                    la lista dei codici delle entità su cui applicare il filtro;
                                 se = NULL vuol dire che il filtro verrà applicato 
                                 su tutto il territorio estratto
  int CounterToVideo;            flag, se = GS_GOOD stampa a video i messaggi
                                 (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_do_sql_filter(int cls, int sub, C_CLASS_SQL_LIST &SQLCondList, C_LINK_SET *pLinkSet,
                      int CounterToVideo)
{
   C_CLASS    *pCls;
   C_LINK_SET ResultLS;

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (CounterToVideo == GS_GOOD)
      acutPrintf(gsc_msg(110)); // "\nInizio filtro..."

   GS_LSFILTER.clear();
   gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   // se si tratta di spaghetti
   if (pCls->get_category() == CAT_SPAGHETTI)
   {
      C_SELSET ResultSS;

      if (!pLinkSet) // filtro su tutto il territorio estratto
         gsc_ssget(_T("X"), NULL, NULL, NULL, *(GS_LSFILTER.ptr_SelSet()));
      else GS_LSFILTER.ptr_SelSet()->add_selset(*(pLinkSet->ptr_SelSet()));

      GS_LSFILTER.ptr_SelSet()->intersectClsCode(pCls->ptr_id()->code);
      // Elimino gli oggetti scartati
      GS_LSFILTER.ptr_SelSet()->subtract(GEOsimAppl::REFUSED_SS);
      gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro

      if (GS_LSFILTER.ptr_SelSet()->length() > 0)
      {
         GS_LSFILTER.cls = pCls->ptr_id()->code;
         GS_LSFILTER.sub = pCls->ptr_id()->sub_code;
      }
   }
   else // se si tratta di classe con database
   {
      if (gsc_db_prepare_for_filter(pCls, SQLCondList, CounterToVideo) == GS_BAD)
         { QRY_CLS_LIST.remove_all(); return GS_BAD; }

      if (gsc_exe_SQL_filter(pCls, SQLCondList, pLinkSet, GS_LSFILTER, NULL, CounterToVideo) == GS_BAD)
         { QRY_CLS_LIST.remove_all(); return GS_BAD; }
      gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro

      QRY_CLS_LIST.remove_all();  // ripulisco la query_list

      // ricavo un gruppo di selezione delle entità
      if (GS_LSFILTER.ptr_SelSet()->length() > 0 || 
          GS_LSFILTER.ptr_KeyList()->get_count() > 0)
      {
         GS_LSFILTER.cls = pCls->ptr_id()->code;
         GS_LSFILTER.sub = pCls->ptr_id()->sub_code;
      }
   }

   if (CounterToVideo == GS_GOOD)
      acutPrintf(gsc_msg(111)); // "\nTerminato.\n"

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_db_prepare_for_filter <external> */
/*+
  Apre tutti i riferimenti alle tabelle delle classi (temporanee e old)
  comprese le classi membro (caso gruppo) e tabelle secondarie
  (temporanee e old).
  Crea una tabella temporanea GS_VERIFIED.
  Parametri:
  C_CLASS          *pMotherCls;   Puntatore alla classe madre
  C_CLASS_SQL_LIST &SQLCondList;  Lista delle condizioni SQL
  int CounterToVideo;             flag, se = GS_GOOD stampa a video i messaggi
                                  (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
static int gsc_db_prepare_for_filter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                     int CounterToVideo)
{
   C_INFO      *p_info = pMotherCls->ptr_info();
   C_CLASS_SQL *pClsCond;
   C_CLASS     *pCls;
   C_SEC_SQL   *pSecCond;
   C_STRING    CorrectedWhere, LPNName, MapTableRef;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (!p_info) return GS_GOOD; // niente da preparare

   if (CounterToVideo == GS_GOOD)
      acutPrintf(gsc_msg(112)); // "\nCreazione tabelle temporanee..."

   // Verifica correttezza di tutte le condizioni di SQL classi GEOsim
   pClsCond = (C_CLASS_SQL *) SQLCondList.get_head();
   while (pClsCond)
   {
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsCond->get_cls(), 
                                                     pClsCond->get_sub())) == NULL)
         return GS_BAD;
      CorrectedWhere = pClsCond->get_sql();

      if (CorrectedWhere.len() > 0)
      {
         // Preparo se necessario il file UDL
         if (pCls->getLPNameOld(LPNName) == GS_BAD) return GS_BAD;
         if (gsc_SetACADUDLFile(LPNName.get_name(), pCls->ptr_info()->getDBConnection(OLD),
                                pCls->ptr_info()->OldTableRef.get_name()) == GS_BAD)
            return GS_BAD;

         if (gsc_Table2MAPFormat(pCls->ptr_info()->getDBConnection(OLD),
                                 pCls->ptr_info()->OldTableRef, MapTableRef) == GS_BAD)
            return GS_BAD;
         // richiede eventuali parametri tramite finestra
         if (gsc_ReadASIParams(LPNName.get_name(), MapTableRef.get_name(), 
                               CorrectedWhere, GSWindowInteraction) == GS_BAD)
            return GS_BAD;
         pClsCond->set_sql(CorrectedWhere.get_name());
      }
      else
         pClsCond->set_sql(GS_EMPTYSTR);

      // Verifica correttezza di tutte le condizioni di SQL tabelle secondarie
      pSecCond = (C_SEC_SQL *) pClsCond->ptr_sec_sql_list()->get_head();
      while (pSecCond)
      {
         if (pSecCond->prepare_for_filter(pCls) == GS_BAD) return GS_BAD;

         pSecCond = (C_SEC_SQL *) pSecCond->get_next();
      }

      pClsCond = (C_CLASS_SQL *) pClsCond->get_next();
   }
   
   if (QRY_CLS_LIST.initialize(SQLCondList) == GS_BAD) return GS_BAD;

   if (gsc_DelAllGS_VERIFIED() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_CLASS::LS_to_GS_VERIFIED       <internal> */
/*+                                                                       
  Inserisco nella tabella GS_VERIFIED tutti i codici dei record puntati dai
  links del gruppo di Link Set (si da per scontato che tutti i links si
  riferiscano alla classe in oggetto e che la tabella GS_VERIFIED esista già).
  Parametri:
  C_LINK_SET & OldLS;	Link Set
  int CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_CLASS::LS_to_GS_VERIFIED(C_LINK_SET &LS, int CounterToVideo) 
{                                                  
   _RecordsetPtr pInsRs;
   C_RB_LIST     ColValues;
   int 			  result = GS_BAD;
   long          ItemNum = 0, tot;
   presbuf       p_rb_KeyVal;
   C_BLONG       *pKey;

   // controlli
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (ptr_group_list() || ptr_info() == NULL ||
       ptr_attrib_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // se il Link-Set è vuoto allora esco
   if ((tot = LS.ptr_KeyList()->get_count()) == 0) return GS_GOOD;

   // preparo istruzione per l'inserimento di record in GS_VERIFIED
   if (gsc_prepare_insert_GS_VERIFIED(pInsRs, ColValues) == GS_BAD) return GS_BAD;

   do
   {
      C_STATUSLINE_MESSAGE StatusLineMsg(NULL);

      // setto il codice della classe
      ColValues.CdrAssocSubst(_T("CLASS_ID"), id.code);
      p_rb_KeyVal = ColValues.CdrAssoc(_T("KEY_ATTRIB"));

      if (CounterToVideo == GS_GOOD)
      {
         acutPrintf(GS_LFSTR);
         StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."
      }

      result = GS_GOOD;

      pKey = (C_BLONG *) LS.ptr_KeyList()->go_top();
      while (pKey)
      {
         // setto il valore da chiave nella lista da scrivere in GS_VERIFIED
         gsc_RbSubst(p_rb_KeyVal, pKey->get_key());

         // inserisco dati in tabella
         if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
            // se l'errore non era dovuto al fatto che il record esisteva già
            if (GS_ERR_COD != eGSIntConstr) { result = GS_BAD; break; }
         
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(++ItemNum); // "%ld entità GEOsim elaborate."

         pKey = (C_BLONG *) LS.ptr_KeyList()->go_next();
      }

      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.End(gsc_msg(310), ItemNum - 1); // "%ld entità GEOsim elaborate."
   }
   while (0);

   gsc_DBCloseRs(pInsRs);

   return result;   
}
int C_CLASS::SS_to_GS_VERIFIED(C_SELSET &SelSet, int CounterToVideo) 
{                                                  
   _RecordsetPtr pInsRs;
   C_RB_LIST     ColValues;
   int 			  result = GS_BAD;
   long          ItemNum = 0, key_val, tot;
   presbuf       p_rb_KeyVal;
   ads_name      entity;

   C_STRING Title(gsc_msg(1056)), CompleteName; // "Elaborazione classe: "
   get_CompleteName(CompleteName);
   Title += _T('<'); Title += CompleteName; Title += _T('>');

   C_STATUSLINE_MESSAGE StatusLineMsg(Title);

   // controlli
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (ptr_group_list() || ptr_info() == NULL ||
       ptr_attrib_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // se il gruppo di selezione è vuoto allora esco
   if ((tot = SelSet.length()) == 0) return GS_GOOD;

   // preparo istruzione per l'inserimento di record in GS_VERIFIED
   if (gsc_prepare_insert_GS_VERIFIED(pInsRs, ColValues) == GS_BAD) return GS_BAD;

   do
   {
      // setto il codice della classe
      gsc_RbSubst(gsc_nth(1, ColValues.nth(0)), ptr_id()->code);
      p_rb_KeyVal = gsc_nth(1, ColValues.nth(1));

      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      result = GS_GOOD;

      while (SelSet.entname(ItemNum++, entity) == GS_GOOD)
      {
         // Legge il valore chiave di una entità
         if (getKeyValue(entity, &key_val) == GS_BAD) { result = GS_BAD; break; }
         // setto il valore da chiave nella lista da scrivere in GS_VERIFIED
         gsc_RbSubst(p_rb_KeyVal, key_val);

         // inserisco dati in tabella
         if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
            // se l'errore non era dovuto al fatto che il record esisteva già
            if (GS_ERR_COD != eGSIntConstr) { result = GS_BAD; break; }

         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(ItemNum); // "%ld entità GEOsim elaborate."
      }

      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.End(gsc_msg(310), --ItemNum); // "%ld entità GEOsim elaborate."
   }
   while (0);

   gsc_DBCloseRs(pInsRs);

   return result;   
}


/***********************************************************/
/*.doc gsc_exe_SQL_filter                                  */
/*+                                                                       
  Restituisce un gruppo di selezione degli oggetti grafici che soddisfano
  il filtro nella variabile globale GS_LSFILTER.

  Parametri:
  C_CLASS *pMotherCls;        Puntatore alla classe madre
  C_CLASS_SQL_LIST &SQLWhere; Condizione SQL
  C_LINK_SET *pLinkSet;       1) Se la classe di entità ha grafica rappresenta 
                                 il gruppo di selezione degli oggetti su cui applicare il filtro;
                              2) se la classe di entità non ha grafica rappresenta
                                 la lista dei codici delle entità su cui applicare il filtro;
                              se = NULL vuol dire che il filtro verrà applicato 
                              su tutto il territorio estratto
  C_LINK_SET &ResultLS;       Link Set con il risultato del filtro
  TCHAR *DestAttr;            Nome dell'attributo della tabella principale destinazione
                              del trasferimento dati da secondaria (default = NULL)
  int CounterToVideo;         flag, se = GS_GOOD stampa a video il numero di entità che si 
                              stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Valida per tutte le classi di entità che hanno un riferimento diretto in
        grafica. 
-*/  
/*********************************************************/
static int gsc_exe_SQL_filter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList, C_LINK_SET *pLinkSet,
                              C_LINK_SET &ResultLS, TCHAR *DestAttr, int CounterToVideo)
{
   switch (pMotherCls->get_category())
   {
      case CAT_SIMPLEX: case CAT_SUBCLASS: // semplici e sottoclassi
         return gsc_exe_SQL_SimplexFilter(pMotherCls, SQLCondList, 
                                          (pLinkSet) ? pLinkSet->ptr_SelSet() : NULL, 
                                          ResultLS, DestAttr, CounterToVideo);
      case CAT_GROUP: // gruppo
         return gsc_exe_SQL_GroupFilter(pMotherCls, SQLCondList, pLinkSet,
                                        ResultLS, DestAttr, CounterToVideo);
      case CAT_GRID: // griglia
         return gsc_exe_SQL_GridFilter(pMotherCls, SQLCondList, 
                                       (pLinkSet) ? pLinkSet->ptr_KeyList() : NULL,
                                       ResultLS, DestAttr, CounterToVideo);

      default:
         return GS_BAD;
   }
}

static int gsc_exe_SQL_SimplexFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                     C_SELSET *pSelSet, C_LINK_SET &ResultLS, TCHAR *DestAttr,
                                     int CounterToVideo)
{
   C_INFO        *p_info = pMotherCls->ptr_info();
   C_ID			  *p_id   = pMotherCls->ptr_id();
   C_ATTRIB_LIST *p_attrib_list = pMotherCls->ptr_attrib_list();
   C_STRING      condition;
   C_CLASS_SQL   *pClsCond;
   int           result = GS_BAD;

   if (!p_info || !p_attrib_list) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (CounterToVideo == GS_GOOD)
      acutPrintf(_T("%s %s"), gsc_msg(116), p_id->name); // "\nEsecuzione query SQL:"

   do
   {
      if ((pClsCond = (C_CLASS_SQL *) SQLCondList.search(p_id->code, p_id->sub_code)) != NULL)
         condition = pClsCond->get_sql();
      if (condition.len() == 0)
         condition = GS_EMPTYSTR;

      if (!pSelSet) // filtro su tutto il territorio estratto
      {
         if (ResultLS.initSQLCond(pMotherCls, condition.get_name()) != GS_GOOD)
            break;
      }
      else // da gruppo di selezione
         if (ResultLS.initSelSQLCond(pMotherCls, *pSelSet, condition.get_name()) != GS_GOOD)
            break;

      // Elimino gli oggetti scartati
      ResultLS.subtract(GEOsimAppl::REFUSED_SS);

      // se non sono stati passati questi 2 parametri si tratta di filtro
      if (!DestAttr)
      {
         // filtro per tabelle secondarie
         if (gsc_exe_SQL_TabSecFilter(pMotherCls, ResultLS, CounterToVideo) == GS_BAD) break;
      }
      else // si tratta di trasferimento di valori da secondaria a principale
         if (gsc_SecValueTransfer(pMotherCls, ResultLS, DestAttr) == GS_BAD) break; 

      result = GS_GOOD;
   }
   while (0);

   return result;
}
static int gsc_exe_SQL_GroupFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                   C_LINK_SET *pLinkSet, C_LINK_SET &ResultLS, TCHAR *DestAttr,
                                   int CounterToVideo)
{
   C_INFO       *p_info = pMotherCls->ptr_info();
   C_ID			 *p_id   = pMotherCls->ptr_id();
   C_GROUP_LIST *p_group_list = pMotherCls->ptr_group_list();
   C_CLASS      *pCls;
   C_INT_INT    *p_group;
   C_LINK_SET   PartialResultLS;
   int          result;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2, t3, t4;
   double tempo1=0, tempo2=0, tempo3=0, tempo4, tempo5;
   _ftime(&t1);
#endif

   if (!p_info || pMotherCls->ptr_attrib_list() == NULL) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION == NULL) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   do
   {
      result = GS_GOOD;
      // ciclo sulle classe "figlie"
      // per ogniuna eseguo il filtro da "ciò che è stato selezionato a video"
      p_group = (C_INT_INT *) p_group_list->get_head();     
      while (p_group)
      {  
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(p_group->get_key(), 0)) == NULL)
   	      { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
   
         // Eseguo eventuale filtro sulla classe "figlia" ed ottengo il Link Set
         // degli oggetti che hanno soddisfatto il filtro
         if (gsc_exe_SQL_filter(pCls, SQLCondList, pLinkSet, PartialResultLS) == GS_BAD)
            { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo1 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
   _ftime(&t3);
#endif

         // Scrivo i codici delle schede associate ai link nella tabella GS_VERIFIED
         if (pCls->LS_to_GS_VERIFIED(PartialResultLS) == GS_BAD)
            { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo2 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

         p_group = (C_INT_INT *) p_group->get_next();
      }

      if (result == GS_BAD) break;

      if (CounterToVideo == GS_GOOD)
         acutPrintf(_T("%s"), gsc_msg(274)); // "\nLettura collegamenti a gruppi..."

      // ciclo sulle classe "figlie"
      // per ogniuna eseguo verifica sulle relazioni
      p_group = (C_INT_INT *) p_group_list->get_head();     
      while (p_group)
      {  
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(p_group->get_key(), 0)) == NULL)
   	      { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif

         // Recupero i gruppi che non hanno tutte le figlie già in GS_VERIFIED
         if (gsc_do_recovery_child(pMotherCls, pCls, pLinkSet, SQLCondList, CounterToVideo) == GS_BAD)
            { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo3 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

         p_group = (C_INT_INT *) p_group->get_next();
      }

      if (result == GS_BAD) break;

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif

      // eseguo filtro sulle schede del gruppo ricavate dal ciclo precedente
      // ed elimino da gs_verified quelle non valide
      if (gsc_correct_GS_VERIFIED(pMotherCls, SQLCondList, DestAttr, CounterToVideo) == GS_BAD)
         { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo4 = (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
   _ftime(&t3);
#endif

      // Ricavo un Link Set delle entità del gruppo
      if (gsc_gs_verified_to_LS(p_id->code, ResultLS) == GS_BAD)
         { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo5 = (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

      result = GS_GOOD;
   }
   while (0);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   acutPrintf(_T("C_GROUP::do_graph_filter impiega %6.2f secondi.\n"), (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );
   acutPrintf(_T("C_SIMPLEX::do_graph_filter impiega %6.2f secondi.\n"), tempo1 );
   acutPrintf(_T("pCls->LS_to_GS_VERIFIED impiega %6.2f secondi.\n"), tempo2);
   acutPrintf(_T("pCls->do_recovery_child impiega %6.2f secondi.\n"), tempo3);
   acutPrintf(_T("gsc_correct_GS_VERIFIED impiega %6.2f secondi.\n"), tempo4);
   acutPrintf(_T("gsc_gs_verified_to_LS impiega %6.2f secondi.\n"), tempo5);
#endif

   return result;
}
static int gsc_exe_SQL_GridFilter(C_CLASS *pMotherCls, C_CLASS_SQL_LIST &SQLCondList,
                                  C_LONG_BTREE *pKeyList, C_LINK_SET &ResultLS, TCHAR *DestAttr,
                                  int CounterToVideo)
{
   C_INFO        *p_info = pMotherCls->ptr_info();
   C_ID			  *p_id   = pMotherCls->ptr_id();
   C_ATTRIB_LIST *p_attrib_list = pMotherCls->ptr_attrib_list();
   C_STRING      condition;
   C_CLASS_SQL   *pClsCond;
   int           result = GS_BAD;

   if (!p_info || !p_attrib_list) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (CounterToVideo == GS_GOOD)
      acutPrintf(_T("%s %s"), gsc_msg(116), p_id->name); // "\nEsecuzione query SQL:"

   do
   {
      if ((pClsCond = (C_CLASS_SQL *) SQLCondList.search(p_id->code, p_id->sub_code)) != NULL)
         condition = pClsCond->get_sql();
      if (condition.len() == 0)
         condition = GS_EMPTYSTR;

      if (!pKeyList) // filtro su tutto il territorio estratto
      {
         if (ResultLS.initSQLCond(pMotherCls, condition.get_name()) != GS_GOOD)
            break;
      }
      else // da gruppo di selezione
         if (ResultLS.initSelSQLCond(pMotherCls, *pKeyList, condition.get_name()) != GS_GOOD)
            break;

      // Elimino gli oggetti scartati
      ResultLS.subtract(GEOsimAppl::REFUSED_SS);

      // se non sono stati passati questi 2 parametri si tratta di filtro
      if (!DestAttr)
      {
         // filtro per tabelle secondarie
         if (gsc_exe_SQL_TabSecFilter(pMotherCls, ResultLS, CounterToVideo) == GS_BAD) break;
      }
      else // si tratta di trasferimento di valori da secondaria a principale
         if (gsc_SecValueTransfer(pMotherCls, ResultLS, DestAttr) == GS_BAD) break; 

      result = GS_GOOD;
   }
   while (0);

   return result;
}


/************************************************************/
/*.doc gsc_exe_SQL_TabSecFilter                  <internal> */
/*+                                                                       
  Esegue un filtro con condizione SQL sui dati delle tabelle
  secondarie le cui principali sono contenute nel link-set 
  passato come parametro. Lo stesso link set vien ridotto lasciando
  solo i link agli oggetti che hanno soddisfatto le condizioni.
  Parametri:
  C_CLASS *pCls;           	Puntatore alla classe
  C_LINK_SET *ResultLS;        Link Set con il risultato del filtro
  int CounterToVideo;         flag, se = GS_GOOD stampa a video il numero di entità che si 
                              stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
static int gsc_exe_SQL_TabSecFilter(C_CLASS *pCls, C_LINK_SET &ResultLS, int CounterToVideo)
{
   C_QRY_CLS      *pClsQry;
   C_QRY_SEC_LIST *pSecQryList;
   C_QRY_SEC      *pSecQry;
   int            result = GS_GOOD, refuse;
   resbuf         PrimaryKey, SecColValue;
   C_SECONDARY    *pSec;
   C_BLONG        *pKey;
   C_SELSET       entSS;
   long           counter = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1074)); // "Filtro su tabelle secondarie"

   if ((pClsQry = (C_QRY_CLS *) QRY_CLS_LIST.search(pCls->ptr_id()->code, 
                                                    pCls->ptr_id()->sub_code)) == NULL)
      return GS_GOOD;
   
   pSecQryList = pClsQry->ptr_qry_sec_list();

   if (pSecQryList->get_head() && CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

   // cerco le istruzioni compilate nella funzione "prepare_for_filter"
   pSecQry = (C_QRY_SEC *) pSecQryList->get_head();

   while (pSecQry)
   {
      if (pSecQry->get_type() == GSExternalSecondaryTable)
         pSec = (C_SECONDARY *) pCls->find_sec(pSecQry->get_key()); // usato solo per sec esterne

      pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_top();
      // ciclo sulle entità della classe
      while (pKey)
      {
         // stampo il numero di entità elaborate 
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(++counter); // "%ld entità GEOsim elaborate."

         // se NON si tratta di una secondaria esterna
         if (pSecQry->get_type() == GSInternalSecondaryTable)
         {  // allora verifico se è presente nella tabella TEMP/OLD
            if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                             pKey->get_key(), &SecColValue) == GS_BAD)
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
         }
         else // se si tratta di una secondaria esterna
         {
            // effettuo ricerca in TEMP/OLD della classe leggendo il valore primario
            if (gsc_get_data(pClsQry, pKey->get_key(), &PrimaryKey,
                             pSec->ptr_info()->key_pri.get_name()) == GS_BAD)
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }

            if (GS_ERR_COD != eGSInvalidKey && PrimaryKey.restype != RTNIL)
               // effettuo ricerca in tabella secondaria
               if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                &PrimaryKey, &SecColValue) == GS_BAD)
                  if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
         }

         // nessuna secondaria soddisfa il criterio SQL
         if (GS_ERR_COD == eGSInvalidKey)
            refuse = TRUE; 
         else
         {
            if (pSecQry->get_operat() >= 0) // se esiste una funz. di aggregaz.
               refuse = gsc_RefuseSQLAggr(&SecColValue, pSecQry->get_operat(),
                                          pSecQry->get_value());
            else
               refuse = FALSE;
         }
         if (refuse) // da scartare
         {  // Cancello gli oggetti grafici e dopo l'ID dal LinkSet
            pCls->get_SelSet(pKey->get_key(), entSS);
            ResultLS.ptr_SelSet()->subtract(entSS);
            ResultLS.ptr_KeyList()->remove_at();

            pKey = (C_BLONG *) ResultLS.ptr_KeyList()->get_cursor();
         }
         else
            pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_next(); // vado al successivo
      }

      if (result == GS_BAD) break;

      pSecQry = (C_QRY_SEC *) pSecQryList->get_next();
   }

   if (pSecQryList->get_head() && CounterToVideo == GS_GOOD)
      StatusLineMsg.End(gsc_msg(310), counter); // "%ld entità GEOsim elaborate."

   return result;
}


/************************************************************/
/*.doc gsc_exe_SQL_TabSecFilter                  <internal> */
/*+                                                                       
  Esegue un filtro con condizione SQL sui dati delle tabelle
  secondarie le cui principali sono contenute nella lista di codici 
  passata come parametro. I dati non validi vengono cancellati lista e,
  in opzione, anche dalla tabella GS_VERIFIED.
  Parametri:
  C_CLASS     *pCls;           	Puntatore alla classe
  C_LONG_LIST &IDList;           Lista di codici ID risultato dal filtro
  int         RemoveFromVERIF;   se = true rimuove anche dalla tabella GS_VERIFIED
  int CounterToVideo;            flag, se = GS_GOOD stampa a video il numero di entità che si
                                 stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
static int gsc_exe_SQL_TabSecFilter(C_CLASS *pCls, C_LONG_LIST &IDList, int RemoveFromVERIF,
                                    int CounterToVideo)
{
   C_QRY_CLS      *pClsQry;
   C_QRY_SEC_LIST *pSecQryList;
   C_QRY_SEC      *pSecQry;
   int            result = GS_GOOD, refuse, Cls;
   long           key_val, counter = 0;
   C_LONG         *pID;
   resbuf         PrimaryKey, SecColValue;
   C_PREPARED_CMD p_From_GS_VERIFIED_Cmd;
   C_SECONDARY    *pSec;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1074)); // "Filtro su tabelle secondarie"

   Cls = pCls->ptr_id()->code;
   if ((pClsQry = (C_QRY_CLS *) QRY_CLS_LIST.search(Cls, 
                                                    pCls->ptr_id()->sub_code)) == NULL)
      return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."
   
   pSecQryList = pClsQry->ptr_qry_sec_list();

   if (RemoveFromVERIF)
      // preparo l'istruzione per ricercare un dato nella tabella GS_VERIFIED
      if (gsc_prepare_data_from_GS_VERIFIED(p_From_GS_VERIFIED_Cmd) == GS_BAD) return GS_BAD;

   // cerco le istruzioni compilate nella funzione "prepare_for_filter"
   pSecQry = (C_QRY_SEC *) pSecQryList->get_head();
   while (pSecQry)
   {
      if (pSecQry->get_type() == GSExternalSecondaryTable)
         pSec = (C_SECONDARY *) pCls->find_sec(pSecQry->get_key()); // usato solo per sec esterne

      pID = (C_LONG *) IDList.get_head();
      // ciclo sulle entità della classe
      while (pID)
      {
         // stampo il numero di entità elaborate 
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(++counter); // "%ld entità GEOsim elaborate."

         key_val = pID->get_id();
         // se NON si tratta di una secondaria esterna
         if (pSecQry->get_type() == GSInternalSecondaryTable)
         {  // allora verifico se è presente nella tabella TEMP/OLD
            if (gsc_get_data(pSecQry, Cls, pCls->ptr_id()->sub_code,
                             key_val, &SecColValue) == GS_BAD)
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
         }
         else // se si tratta di una secondaria esterna
         {
            // effettuo ricerca in TEMP/OLD della classe leggendo il valore primario
            if (gsc_get_data(pClsQry, key_val, &PrimaryKey,
                             pSec->ptr_info()->key_pri.get_name()) == GS_BAD)
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }

            if (PrimaryKey.restype == RTNIL || PrimaryKey.restype == RTNONE) GS_ERR_COD = eGSInvalidKey;
            if (GS_ERR_COD != eGSInvalidKey)
               // effettuo ricerca in tabella secondaria
               if (gsc_get_data(pSecQry, Cls, pCls->ptr_id()->sub_code,
                                &PrimaryKey, &SecColValue) == GS_BAD)
                  if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
         }

         // nessuna secondaria soddisfa il criterio SQL
         if (GS_ERR_COD == eGSInvalidKey)
            refuse = TRUE; 
         else
            if (pSecQry->get_operat() >= 0) // se esiste una funz. di aggregaz.
               refuse = gsc_RefuseSQLAggr(&SecColValue, pSecQry->get_operat(),
                                          pSecQry->get_value());
            else
               refuse = FALSE;

         if (refuse) // da scartare
         {  // Cancello il codice dalla lista
            IDList.remove_at();
            pID = (C_LONG *) IDList.get_cursor();
            
            if (RemoveFromVERIF)
               // cancello il dato non valido da GS_VERIFIED
               if (gsc_canc_GS_VERIFIED(p_From_GS_VERIFIED_Cmd, Cls, key_val) == GS_BAD)
                  { result = GS_BAD; break; }
         }
         else
            pID = (C_LONG *) IDList.get_next(); // vado al successivo

         if (result == GS_BAD) break;
      }

      pSecQry = (C_QRY_SEC *) pSecQryList->get_next();
   }

   if (CounterToVideo == GS_GOOD) 
      StatusLineMsg.End(gsc_msg(310), counter); // "%ld entità GEOsim elaborate."

   return result;
}


/************************************************************/
/*.doc gsc_RefuseSQLAggr                  <internal> */
/*+                                                                       
  Esegue una comparazione del valore risultante dalla funzione
  di aggregazione SQL con un valore di riferimento.
  Parametri:
  presbuf ReadValue;    valore letto dall'istruzione SQL
  int     Operator;     operatore di confronto (vedi vettore OPERATOR_LIST)
  presbuf RefValue;     valore di confronto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
static int gsc_RefuseSQLAggr(presbuf ReadValue, int Operator, presbuf RefValue)
{
   int   refuse, ToDeallocate = FALSE;
   TCHAR *pStrRefValue = NULL;

   switch (ReadValue->restype)
   {
      case RTSTR:
         switch (RefValue->restype)
         {
            case RTREAL: // stringa letta da comparare con un numero
               pStrRefValue = gsc_tostring(RefValue->resval.rreal);
               ToDeallocate = TRUE;
               break;
            case RTSTR:
               pStrRefValue = RefValue->resval.rstring;
               break;
            default: return TRUE;
         }
         break;
      case RTREAL:
         if (RefValue->restype != RTREAL) return TRUE;
         break;
      default: return TRUE;
   }

   refuse = FALSE;
   switch (ReadValue->restype)
   {
      case RTREAL: // esiste una funzione di aggregazione con risultato numerico
         switch (Operator)
         {
            case 0: // =
               if (!(ReadValue->resval.rreal == RefValue->resval.rreal)) refuse = TRUE;
               break;
            case 1: // >
               if (!(ReadValue->resval.rreal > RefValue->resval.rreal)) refuse = TRUE;
               break;
            case 2: // <
               if (!(ReadValue->resval.rreal < RefValue->resval.rreal)) refuse = TRUE;
               break;
            case 3: // <>
               if (!(ReadValue->resval.rreal != RefValue->resval.rreal)) refuse = TRUE;
               break;
            case 4: // >=
               if (!(ReadValue->resval.rreal >= RefValue->resval.rreal)) refuse = TRUE;
               break;
            case 5: // <=
               if (!(ReadValue->resval.rreal <= RefValue->resval.rreal)) refuse = TRUE;
               break;
         }
         break;
      case RTSTR:  // esiste una funzione di aggregazione con risultato carattere
      {
         TCHAR *pStrReadValue = gsc_rtrim(gsc_tostring(ReadValue->resval.rstring));

         switch (Operator)
         {
            case 0: // =
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) == 0))
                  refuse = TRUE;
               break;
            case 1: // >
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) > 0))
                  refuse = TRUE;
               break;
            case 2: // <
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) < 0))
                  refuse = TRUE;
               break;
            case 3: // <>
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) != 0))
                  refuse = TRUE;
               break;
            case 4: // >=
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) >= 0))
                  refuse = TRUE;
               break;
            case 5: // <=
               if (!(gsc_strcmp(pStrReadValue, pStrRefValue) <= 0))
                  refuse = TRUE;
               break;
         }
         if (pStrReadValue) free(pStrReadValue);
         break;
      }
      default: refuse = TRUE;
   }

   if (ToDeallocate && pStrRefValue) free(pStrRefValue);

   return refuse;
}


/*********************************************************/
/*.doc gsc_do_recovery_child                  <internal> */
/*+                                                                       
  Inserisce in GS_VERIFIED i codici dei gruppi da filtrare
  recuperando i gruppi che non hanno tutte le figlie a video.
  Parametri:
  C_CLASS *pMother;           	puntatore alla classe madre (gruppo)
  C_CLASS *pChildClass;          puntatore a classe figlia
  C_LINK_SET *pLinkSet;          1) Se la classe di entità ha grafica rappresenta 
                                    il gruppo di selezione degli oggetti su cui applicare il filtro;
                                 2) se la classe di entità non ha grafica rappresenta
                                    la lista dei codici delle entità su cui applicare il filtro;
                                 se = NULL vuol dire che il filtro verrà applicato 
                                 su tutto il territorio estratto
  C_CLASS_SQL_LIST &SQLCondList; Condizione SQL
  int CounterToVideo;         flag, se = GS_GOOD stampa a video il numero di entità che si 
                              stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_do_recovery_child(C_CLASS *pMother, C_CLASS *pChildClass, C_LINK_SET *pLinkSet, 
                          C_CLASS_SQL_LIST &SQLCondList, int CounterToVideo)
{
   _RecordsetPtr    p_ClsFrom_GS_VERIFIED_Rs, pInsRs;
   C_PREPARED_CMD   pTempLnkCmd, pOldLnkCmd, p_GS_VERIFIED_Cmd;
   long             key_val, rel_Key, counter = 0;
   int              cls = pChildClass->ptr_id()->code, rel_Class;
   int              result = GS_BAD;
   C_RB_LIST        ColValuesClsVerif, ColValuesVerif;
   int              exist, SecQryExist;
   presbuf          pRbClassID, pRbKeyAttrVerif, pRbKeyAttribClsVerif;
   C_SELSET         EntSS;
   C_LINK_SET       ls, ls2;
   C_QRY_CLS        *p_qry, *p_MotherQry;
   C_LONG_LIST      IDGroupList;
   C_LONG           *pIDGroup;
   C_2INT_LONG_LIST ent_list;
   C_2INT_LONG      *ent_punt;
   C_STRING         condition;
   C_CLASS_SQL      *pClsCond;
   C_LONG_LIST      IDList;
   C_LONG           *pID;
   C_CLASS          *pCls;
   C_STATUSLINE_MESSAGE StatusLineMsg(NULL);

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2, t3, t4;
   double tempo1=0, tempo2=0, tempo3=0, tempo4=0, tempo5=0;
#endif

   // controlli
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (!pMother) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   do
   {
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t1);
#endif

      if ((pID = new C_LONG) == NULL) break;
      IDList.add_tail(pID);

      /////////////////////////////////////////////////////////////////////////
      // preparo istruzione per l'inserimento di record in GS_VERIFIED
      if (gsc_prepare_insert_GS_VERIFIED(pInsRs, ColValuesVerif) == GS_BAD) break;
      // codice della classe CLASS_ID KEY_ATTRIB
      pRbClassID = ColValuesVerif.CdrAssoc(_T("CLASS_ID"));
      // codice dell'entità
      pRbKeyAttrVerif = ColValuesVerif.CdrAssoc(_T("KEY_ATTRIB"));

      /////////////////////////////////////////////////////////////////////////
      // Compilo l' istruzione di lettura dei dati da GS_VERIFIED
      if (gsc_prepare_data_from_GS_VERIFIED(p_GS_VERIFIED_Cmd) == GS_BAD)
         break;

      /////////////////////////////////////////////////////////////////////////
      // Compilo le istruzioni di lettura dei codici dei gruppi legati ad un membro
      if (pMother->prepare_reldata_where_member(pTempLnkCmd, TEMP) == GS_BAD) break;
      if (pMother->prepare_reldata_where_member(pOldLnkCmd, OLD) == GS_BAD) break;

      // cerco le istruzioni compilate nella funzione "prepare_for_filter"
      if ((p_MotherQry = (C_QRY_CLS *) QRY_CLS_LIST.search(pMother->ptr_id()->code, 0)) == NULL)
         return GS_BAD;

      /////////////////////////////////////////////////////////////////////////
      // Compilo l' istruzione di lettura dei dati della classe da GS_VERIFIED
      if (gsc_getClsFrom_GS_VERIFIED(cls, p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
         break;
      if (gsc_InitDBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValuesClsVerif) == GS_BAD) break;
      // codice entità membro della classe
      pRbKeyAttribClsVerif = ColValuesClsVerif.CdrAssoc(_T("KEY_ATTRIB"));

      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      result = GS_GOOD;
      // scorro l'elenco delle figlie in GS_VERIFIED
      while (gsc_isEOF(p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
      {
         // stampo il numero di oggetti grafici elaborati
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(++counter); // "%ld entità GEOsim elaborate."

         // leggo codice entità
         if (gsc_DBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValuesClsVerif) == GS_BAD)
	         { result = GS_BAD; break; }
         if (gsc_rb2Lng(pRbKeyAttribClsVerif, &key_val) == GS_BAD) key_val = 0;

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif

         // leggo i codici dei gruppi legati a questa entità
         if (pMother->get_group_list(pTempLnkCmd, pOldLnkCmd, cls, key_val, IDGroupList) == GS_BAD)
	         { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo1 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

         // scorro l'elenco dei gruppi
         pIDGroup = (C_LONG *) IDGroupList.get_head();
         while (pIDGroup)
         {
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
            // se il gruppo non era in GS_VERIFIED
            if (gsc_in_GS_VERIFIED(p_GS_VERIFIED_Cmd, pMother->ptr_id()->code,
                                   pIDGroup->get_id()) == GS_BAD)
            {
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo4 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
   _ftime(&t3);
#endif
               // leggo i codici dei membri che costituiscono il gruppo
               if (pMother->get_member(p_MotherQry->from_temprel_where_key,
         			      	            p_MotherQry->from_oldrel_where_key,
         			 	                  pIDGroup->get_id(), &ent_list) == GS_BAD)
                  { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo2+= (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

               exist  = FALSE;

               // scorro l'elenco delle relazioni per verificare se esistono tutte
               ent_punt = (C_2INT_LONG *) ent_list.get_head();
               while (ent_punt!=NULL)
               {
                  exist     = FALSE;
         	      rel_Class = ent_punt->get_key();
         	      rel_Key   = ent_punt->get_id();

                  SecQryExist = FALSE;
                  if ((pClsCond = (C_CLASS_SQL *) SQLCondList.search(rel_Class, 0)) != NULL)
                  {
                     condition = pClsCond->get_name();

                     if ((p_qry = (C_QRY_CLS *) QRY_CLS_LIST.search(rel_Class, 0)) != NULL)
                        SecQryExist = (p_qry->ptr_qry_sec_list()->get_count() > 0) ? TRUE : FALSE;
                  }
                  else condition.clear();

                  // se l'entità appartiene ad una classe membro che non sia quella
                  // indicata da <pChildClass>
                  // o non ci sono condizioni o non si trova già in GS_VERIFIED

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
                  if ((rel_Class == cls && rel_Key == key_val) ||
                      (condition.len() == 0 && !SecQryExist) ||
                      gsc_in_GS_VERIFIED(p_GS_VERIFIED_Cmd, rel_Class, rel_Key) == GS_GOOD)
{
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo4 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif
                     exist = TRUE;
}
                  else
                  {  
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo4 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif
                     // Ritorna il puntatore alla classe cercata
                     if ((pCls = GS_CURRENT_WRK_SESSION->find_class(rel_Class, ent_punt->get_type())) == NULL)
                        return GS_BAD;

                     // verifico se soddisfa il filtro
                     if (pLinkSet) // filtro su gruppo e NON su tutto
                     {  // può essere fuori dal gruppo
                        // ricavo l'insieme degli oggetti grafici collegati
                        if (pCls->get_SelSet(rel_Key, EntSS) == GS_BAD)
                           { result = GS_BAD; break; }

                        if (EntSS.length() > 0) // se esistono oggetti grafici
                        { 
                           if (condition.len() > 0) // se esiste una condizione sulla classe
                           {
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
                              // verifico che sia soddisfatta la condizione
                              if (ls.initSelSQLCond(pCls, EntSS, condition.get_name()) == GS_BAD)
                                 { result = GS_BAD; break; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo3 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

                              if (ls.ptr_SelSet()->length() == 0) break;
                           }

                           // se esiste l'entità GEOsim ed esiste una condizione 
                           // impostata ad una tabella secondaria della classe 
                           if (SecQryExist)
                           {
                              // filtro per tabelle secondarie
                              if (gsc_exe_SQL_TabSecFilter(pCls, ls, CounterToVideo) == GS_BAD)
                                 { result = GS_BAD; break; }
                              if (ls.ptr_SelSet()->length() == 0) break;
                           }

                           exist = TRUE;
                        }
                        else // l'entità non è a video
                        {
                           // entità nuova che non esiste più a cui, probabilmente, 
                           // è stato fatto un UNDO che ha cancellato il suo inserimento                           
                           if (pCls->is_NewEntity(rel_Key) == GS_BAD)
                           {  // solo se esisteva già
                              if ((p_qry = (C_QRY_CLS *) QRY_CLS_LIST.search(rel_Class, 0)) == NULL)
                                 { result = GS_BAD; break; }
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
                              // leggo dal temp/old applicando il filtro eventualmente impostato
                              if (gsc_get_data(p_qry, rel_Key) == GS_BAD)
{
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo5 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif
                                 break;
}
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo5 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

                              // se esiste l'entità GEOsim ed esiste una condizione 
                              // impostata ad una tabella secondaria della classe 
                              if (SecQryExist)
                              {
                                 pID->set_id(rel_Key);
                                 if (gsc_exe_SQL_TabSecFilter(pCls, IDList, FALSE, CounterToVideo) == GS_BAD)
                                    { result = GS_BAD; break; }
                                 if (IDList.get_count() == 0)
                                 {
                                    // rimetto l'oggetto C_LONG
                                    if ((pID = new C_LONG) == NULL) { result = GS_BAD; break; }
                                    IDList.add_tail(pID);

                                    break;
                                 }
                              }   
                           
                              exist = TRUE;
                           }
                        }
                     }
                     else // filtro su TUTTA la grafica
                     {
                        // entità nuova                        
                        if (pCls->is_NewEntity(rel_Key) == GS_BAD)
                        {  // solo se esisteva già
                           if ((p_qry = (C_QRY_CLS *) QRY_CLS_LIST.search(rel_Class, 0)) == NULL)
                              { result = GS_BAD; break; }
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
#endif
                           // leggo dal temp/old applicando il filtro eventualmente impostato
                           if (gsc_get_data(p_qry, rel_Key) == GS_BAD)
{
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo5 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif
                              break;
}
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t4);
   tempo5 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
#endif

                           // se esiste l'entità GEOsim ed esiste una condizione 
                           // impostata ad una tabella secondaria della classe 
                           if (SecQryExist)
                           {
                              pID->set_id(rel_Key);
                              if (gsc_exe_SQL_TabSecFilter(pCls, IDList, FALSE, CounterToVideo) == GS_BAD)
                                 { result = GS_BAD; break; }
                              if (IDList.get_count() == 0)
                              {
                                 // rimetto l'oggetto C_LONG
                                 if ((pID = new C_LONG) == NULL) { result = GS_BAD; break; }
                                 IDList.add_tail(pID);

                                 break;
                              }
                           }   
                           
                           exist = TRUE;
                        }
                     }
                  }

                  if (!exist || result == GS_BAD) break;

                  ent_punt = (C_2INT_LONG *) ent_punt->get_next();
               } // fine ciclo sulle relazioni di un gruppo

               if (result == GS_BAD) break;

               // se esiste il gruppo allora la inserisco in GS_VERIFIED (se non c'era ancora)
               if (exist && 
                   gsc_in_GS_VERIFIED(p_GS_VERIFIED_Cmd, pMother->ptr_id()->code,
                                      pIDGroup->get_id()) == GS_BAD)
               {
                 // setto il codice della classe
                  gsc_RbSubst(pRbClassID, pMother->ptr_id()->code);
                  // setto il codice dell'entità
                  gsc_RbSubst(pRbKeyAttrVerif, pIDGroup->get_id());

                  // copio dati su tabella gs_verified
                  if (gsc_DBInsRow(pInsRs, ColValuesVerif) == GS_BAD)
                     // se il record era già in GS_VERIFIED non è errore
                     if (GS_ERR_COD != eGSIntConstr) { result = GS_BAD; break; }
               }
            } // fine if (se il gruppo non era in GS_VERIFIED)
#if defined(GSDEBUG) // se versione per debugging
else
{
   _ftime(&t4);
   tempo4 += (t4.time + (double)(t4.millitm)/1000) - (t3.time + (double)(t3.millitm)/1000);
}
#endif

            pIDGroup = (C_LONG *) pIDGroup->get_next();
         } // fine ciclo sui gruppi collegati ad un'entità

         if (result == GS_BAD) break;

         gsc_Skip(p_ClsFrom_GS_VERIFIED_Rs);
      } // fine ciclo sulle entità della classe in GS_VERIFIED

      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Set(gsc_msg(310), counter); // "%ld entità GEOsim elaborate."
   }
   while (0);

   #if defined(GSDEBUG) // se versione per debugging
      _ftime(&t2);
      acutPrintf(_T("\nRecovery_child impiega %6.2f secondi.\n"), (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );
      acutPrintf(_T("get_group_list impiega %6.2f secondi.\n"), tempo1);
      acutPrintf(_T("get_member impiega %6.2f secondi.\n"), tempo2);
      acutPrintf(_T("initSelSQLCond impiega %6.2f secondi.\n"), tempo3);
      acutPrintf(_T("gsc_in_GS_VERIFIED impiega %6.2f secondi.\n"), tempo4);
      acutPrintf(_T("gsc_get_data impiega %6.2f secondi.\n"), tempo5);
   #endif

   return result;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER TABELLA GS_VERIFIED
///////////////////////////////////////////////////////////////////////////////
 

/*********************************************************/
/*.doc gsc_get_gs_verified_struct             <internal> */
/*+
  Questa funzione restituisce la struttura della tabella GS_VERIFIED di GEOsim.
  N.B. Utilizzando il Provider per ACCESS 97 non è possibile creare tabelle
       con strutture che superano una certa dimensione perchè si genera un 
       errore "record too large". Si utilizza perciò un database
       già preparato contenente la tabella in questione.
       La funzione ha, quindi, solo uno scopo documentativo.
-*/  
/*********************************************************/
void gsc_get_gs_verified_struct(C_STRING &Stru)
{  
   Stru =  _T(" (CLASS_ID SMALLINT NOT NULL, "); // CODICE DELLA CLASSE
   Stru += _T("KEY_ATTRIB LONG NOT NULL)");      // CODICE DELLA SCHEDA DELL'ENTITA'
}


/************************************************************/
/*.doc gsc_DelAllGS_VERIFIED                     <internal> */
/*+                                                                       
  Svuoto la tabella GS_VERIFIED.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/************************************************************/
int gsc_DelAllGS_VERIFIED(void)
{        
   C_STRING       TableRef;         
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if (pConn->DelRows(TableRef.get_name()) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_DelSS_in_GS_VERIFIED                  <internal> */
/*+                                                                       
  Cancella dalla tabella GS_VERIFIED i record che si
  riferiscono agli oggetti presenti in ss.
  Parametri:
  C_SELSET &ss;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/************************************************************/
int gsc_DelSS_in_GS_VERIFIED(C_SELSET &ss)
{        
   C_CLASS        *pCls;
   C_PREPARED_CMD p_Del_GS_VERIFIED_Cmd;
   long           i = 0, Key;
   ads_name       ent;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // preparo l'istruzione per ricercare un dato nella tabella GS_VERIFIED
   if (gsc_prepare_data_from_GS_VERIFIED(p_Del_GS_VERIFIED_Cmd) == GS_BAD)
      // non c'è la tabella 
      return GS_GOOD;

   while (ss.entname(i++, ent) == GS_GOOD)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) &&
          pCls->getKeyValue(ent, &Key) == GS_GOOD)
         if (gsc_canc_GS_VERIFIED(p_Del_GS_VERIFIED_Cmd, pCls->ptr_id()->code, Key) == GS_BAD)
            return GS_BAD;
   }

   return GS_GOOD;
}


/************************************************************/
/*.doc (new 2) gsc_prepare_data_from_GS_VERIFIED <internal> */
/*+                                                                       
  Preparo istruzione per la selezione di dati da GS_VERIFIED.
  Parametri:
  C_PREPARED_CMD &pCmd;    Comando da preparare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/************************************************************/
int gsc_prepare_data_from_GS_VERIFIED(C_PREPARED_CMD &pCmd)
{
   // verifico se si può usare il metodo seek con i recordset
   if ((pCmd.pRs = gsc_prepare_data_from_GS_VERIFIED()) == NULL)
      // Se non è possibile usare la seek uso il comando preparato
      if (gsc_prepare_data_from_GS_VERIFIED(pCmd.pCmd) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int gsc_prepare_data_from_GS_VERIFIED(_CommandPtr &pCmd)
{
   C_STRING       statement, TableRef;         
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=? AND KEY_ATTRIB=?");
   
   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("CLASS_ID"), adSmallInt) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("KEY_ATTRIB"), adInteger) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}
_RecordsetPtr gsc_prepare_data_from_GS_VERIFIED(void)
{                 
   C_STRING       TableRef, InitQuotedId, FinalQuotedId;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
   InitQuotedId = pConn->get_InitQuotedIdentifier();
   FinalQuotedId = pConn->get_FinalQuotedIdentifier();
   TableRef.removePrefixSuffix(pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier());

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

	try
   {  
      pRs->CursorLocation = adUseServer;
      // apro il recordset
      pRs->Open(TableRef.get_name(), pConn->get_Connection().GetInterfacePtr(), 
                adOpenDynamic, adLockOptimistic, adCmdTableDirect);
      RsOpened = true;
      
      if (!pRs->Supports(adIndex) || !pRs->Supports(adSeek))
      {  // non supportato
         gsc_DBCloseRs(pRs);
         return NULL;
      }
      // Setto l'indice di ricerca
      if (gsc_DBSetIndexRs(pRs, _T("GS_VERIF")) == GS_BAD)
      {
         gsc_DBCloseRs(pRs);
         return NULL;
      }
   }

	catch (_com_error)
   {
      if (RsOpened) gsc_DBCloseRs(pRs);
      else pRs.Release();

      return NULL;
   }

   return pRs;
}


/*********************************************************/
/*.doc (new 2) gsc_getClsFrom_GS_VERIFIED     <internal> */
/*+                                                                       
  Preparo istruzione per la selezione di dati di una classe da GS_VERIFIED.
  Parametri:
  int cls;              codice classe
  _RecordsetPtr &pRs;   RecordSet

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_getClsFrom_GS_VERIFIED(int cls, _RecordsetPtr &pRs)
{        
   C_STRING       statement, TableRef;         
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT KEY_ATTRIB FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;

   // creo record-set
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   return pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockOptimistic);
}


/*********************************************************/
/*.doc (new 2) gsc_in_GS_VERIFIED             <internal> */
/*+                                                                       
  Verifica se l'entità <key> della classe <cls> è presente nella tabella GS_VERIFIED
  cioè se è già stata verificata la sua esistenza (vedi <gsc_prepare_data_from_GS_VERIFIED>).
  Parametri:
  C_PREPARED_CMD  &pCmd;   Comando preparato
  int             Cls;
  long            Key;
   _RecordsetPtr  *pRs;    Puntatore a record-set; se = NULL viene usato un 
                           record-set interno (default = NULL)
  int *IsRsCloseable;      (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_in_GS_VERIFIED(C_PREPARED_CMD &pCmd, int Cls, long Key, _RecordsetPtr *pRs,
                       int *IsRsCloseable)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 2 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      long           ix;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 2;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // CODICE CLASSE
      ix  = 0;
      gsc_set_variant_t(var, (short) Cls);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE CHIAVE
      ix  = 1;
      gsc_set_variant_t(var, Key);
      SafeArrayPutElement(psa, &ix, &var);

      KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
      KeyForSeek.parray = psa;  

      if (gsc_DBSeekRs(pCmd.pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;

      if (pRs)
      {
         if (IsRsCloseable) *IsRsCloseable = GS_BAD;
         *pRs = pCmd.pRs;
         return GS_GOOD;
      }
      else
         if (gsc_isEOF(pCmd.pRs) == GS_BAD) return GS_GOOD;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
      {
         // Setto parametri del comando
         gsc_SetDBParam(pCmd.pCmd, 0, Cls); // Codice classe
         gsc_SetDBParam(pCmd.pCmd, 1, Key); // Codice entità

         if (pRs)
         {
            if (IsRsCloseable) *IsRsCloseable = GS_GOOD;
            // Esegue il comando 
            return gsc_ExeCmd(pCmd.pCmd, *pRs, adOpenDynamic, adLockOptimistic);
         }
         else
         {
            _RecordsetPtr pInternalpRs;

            // Esegue il comando 
            if (gsc_ExeCmd(pCmd.pCmd, pInternalpRs) == GS_BAD) return GS_BAD;

            if (gsc_isEOF(pInternalpRs) == GS_BAD)
               { gsc_DBCloseRs(pInternalpRs); return GS_GOOD; }
            gsc_DBCloseRs(pInternalpRs);
         }
      }

   return GS_BAD;
}


/*********************************************************/
/*.doc (new 2) gsc_canc_GS_VERIFIED <internal> */
/*+                                                                       
  Cancella l'entità <key> della classe <cls> se è presente 
  nella tabella GS_VERIFIED.
  Parametri:
  C_PREPARED_CMD &pCmd;
  int            Cls;
  long           Key;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_canc_GS_VERIFIED(C_PREPARED_CMD &pCmd, int Cls, long Key)
{
   _RecordsetPtr pRs;
   int           IsRsCloseable;

   if (gsc_in_GS_VERIFIED(pCmd, Cls, Key, (_RecordsetPtr *) &pRs, &IsRsCloseable) == GS_BAD)
      return GS_BAD;

   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      return GS_GOOD;
   }
   if (gsc_DBDelRow(pRs) == GS_BAD)
   {
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      return GS_BAD;
   }
   
   if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_prepare_insert_GS_VERIFIED <external> */
/*+                                                                       
  Preparo istruzione per l'inserimento di un record in GS_VERIFIED.
  Parametri:
  _RecordsetPtr &pInsRs;      RecordSet per l'inserimento
  C_RB_LIST     &ColValues;   Valori da inserire

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_prepare_insert_GS_VERIFIED(_RecordsetPtr &pInsRs, C_RB_LIST &ColValues)
{                 
   C_STRING       TableRef;         
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if (pConn->InitInsRow(TableRef.get_name(), pInsRs) == GS_BAD) return GS_BAD;
   if (gsc_InitDBReadRow(pInsRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pInsRs); return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_correct_GS_VERIFIED <internal> */
/*+                                                                       
  Eseguo filtro sulle schede del gruppo ed elimino quelle
  non valide da gs_verified.
  Parametri:
  C_CLASS          *pCls;
  C_CLASS_SQL_LIST &SQLCondList; Condizione SQL
  TCHAR *DestAttr;               Nome dell'attributo della tabella principale destinazione
                                 del trasferimento dati da secondaria (default = NULL)
  int CounterToVideo;      Flag, se = GS_GOOD stampa a video il numero di entità che si 
                           stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altriomenti GS_BAD.
  N.B. da usarsi solo per gruppi.
-*/
/*********************************************************/
int gsc_correct_GS_VERIFIED(C_CLASS *pCls, C_CLASS_SQL_LIST &SQLCondList,
                            TCHAR *DestAttr, int CounterToVideo)
{
   int           result = GS_BAD, SecQryExist;
   _RecordsetPtr p_ClsFrom_GS_VERIFIED_Rs;
   presbuf       pKeyAttrib;
   C_RB_LIST     ColValues;
   long          Key;
   C_CLASS_SQL   *pClsCond;
   C_STRING      condition;
   C_ID          *p_id = pCls->ptr_id();
   C_QRY_CLS     *p_qry;
   C_LONG_LIST   IdListToVerif4Sec;
   C_LONG        *pIdToVerif4Sec;

   if (CounterToVideo == GS_GOOD)
      acutPrintf(_T("%s %s"), gsc_msg(116), p_id->name); // "\nEsecuzione query SQL:"

   do
   {
      // ricavo eventuale condizione di filtro sulla classe
      if ((pClsCond = (C_CLASS_SQL *) SQLCondList.search(p_id->code, 0)) != NULL)
         condition = pClsCond->get_sql();

      if ((p_qry = (C_QRY_CLS *) QRY_CLS_LIST.search(p_id->code, 0)) == NULL) break;
      SecQryExist = (p_qry->ptr_qry_sec_list()->get_count() > 0) ? TRUE : FALSE;

      if (condition.len() == 0 && !SecQryExist) { result = GS_GOOD; break; }

      /////////////////////////////////////////////////////////////////////////
      // Compilo l' istruzione di lettura dei dati della classe da GS_VERIFIED
      if (gsc_getClsFrom_GS_VERIFIED(p_id->code, p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
         break;
      
      if (gsc_InitDBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(p_ClsFrom_GS_VERIFIED_Rs); return GS_BAD; }
      pKeyAttrib = ColValues.CdrAssoc(_T("KEY_ATTRIB"));

      result = GS_GOOD;
      // scorro l'elenco delle righe della classe in GS_VERIFIED
      while (gsc_isEOF(p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
      {
         // leggo codice entità
         if (gsc_DBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValues) == GS_BAD) { result = GS_BAD; break; }
         if (gsc_rb2Lng(pKeyAttrib, &Key) == GS_BAD) Key = 0;

         // leggo dal temp/old applicando il filtro eventualmente impostato
         if (gsc_get_data(p_qry, Key) == GS_BAD)
         {
            // se non c'è lo cancello da GS_VERIFIED
            if (gsc_DBDelRow(p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD) { result = GS_BAD; break; }
         }
         else // se ci sono delle condizioni SQL sulle tabelle secondarie
            if (SecQryExist)
            {
               // memorizzo la lista dei codici del gruppi validi
               if ((pIdToVerif4Sec = new C_LONG) == NULL)
         	      { result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }
               IdListToVerif4Sec.add_tail(pIdToVerif4Sec);
               pIdToVerif4Sec->set_id(Key);
            }
            
         gsc_Skip(p_ClsFrom_GS_VERIFIED_Rs);
      }
      gsc_DBCloseRs(p_ClsFrom_GS_VERIFIED_Rs); // Chiude il recordset su GS_VERIFIED
      result = GS_BAD;
      
      if (IdListToVerif4Sec.get_count() > 0)
         // se non sono stati passati questi 2 parametri si tratta di filtro
         if (!DestAttr)
         {
            // applico il filtro sui gruppi cancellando da GS_VERIFIED e dalla
            // lista quelle non valide
            if (gsc_exe_SQL_TabSecFilter(pCls, IdListToVerif4Sec, TRUE, CounterToVideo) == GS_BAD) break;
         }
         else // si tratta di trasferimento di valori da secondaria a principale
            if (gsc_SecValueTransferGroup(pCls, IdListToVerif4Sec,
                                          DestAttr) == GS_BAD) break; 

      result = GS_GOOD;
   }
   while (0);

   return result;
}


/*********************************************************/
/*.doc gsc_gs_verified_to_LS <internal> */
/*+                                                                       
  Creo un Link Set delle entità della classe <cls> che hanno soddisfatto il
  filtro e che quindi sono in GS_VERIFIED.
  Parametri:
  int        cls;        Codice classe
  C_LINK_SET &ResultLS;  Puntatore al Link Set

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. da usarsi solo per gruppi.
-*/  
/*********************************************************/
int gsc_gs_verified_to_LS(int cls, C_LINK_SET &ResultLS)
{
   int              result = GS_BAD, rel_Class;
   _RecordsetPtr    p_ClsFrom_GS_VERIFIED_Rs;
   C_CLASS          *pCls, *pClsChild;
   C_RB_LIST        ColValues;
   presbuf          pKeyAttrib;
   long             gs_id_group, rel_Key;
   C_SELSET         ss;
   C_2INT_LONG_LIST ent_list;
   C_2INT_LONG      *ent_punt;
   C_QRY_CLS        *p_qry;

   ResultLS.clear();

   do
   {
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return GS_BAD;

      // cerco le istruzioni compilate nella funzione "prepare_for_filter"
      if ((p_qry = (C_QRY_CLS *) QRY_CLS_LIST.search(cls, 0)) == NULL)
         return GS_BAD;
      
      /////////////////////////////////////////////////////////////////////////
      // Compilo l'istruzione di lettura dei dati della classe da GS_VERIFIED
      if (gsc_getClsFrom_GS_VERIFIED(cls, p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
         break;
      if (gsc_InitDBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(p_ClsFrom_GS_VERIFIED_Rs); return GS_BAD; }
      pKeyAttrib = ColValues.CdrAssoc(_T("KEY_ATTRIB"));

      result = GS_GOOD;
      // scorro l'elenco dei gruppi in GS_VERIFIED
      while (gsc_isEOF(p_ClsFrom_GS_VERIFIED_Rs) == GS_BAD)
      {
         // leggo codice entità
         if (gsc_DBReadRow(p_ClsFrom_GS_VERIFIED_Rs, ColValues) == GS_BAD)
            { result = GS_BAD; break; }
         if (gsc_rb2Lng(pKeyAttrib, &gs_id_group) == GS_BAD) gs_id_group = 0;

         // aggiungo il codice del gruppo al Link Set
         ResultLS.ptr_KeyList()->add(&gs_id_group);

         // leggo i codici dei membri che costituiscono il gruppo
         if (pCls->get_member(p_qry->from_temprel_where_key,
   			                  p_qry->from_oldrel_where_key,
   			 	               gs_id_group, &ent_list) == GS_BAD)
            { result = GS_BAD; break; }

         // scorro l'elenco delle relazioni per verificare se esistono tutte
         ent_punt = (C_2INT_LONG *) ent_list.get_head();
         while (ent_punt!=NULL)
         {
   	      rel_Class = ent_punt->get_key();
   	      rel_Key   = ent_punt->get_id();

            // Ritorna il puntatore alla classe cercata
            if ((pClsChild = GS_CURRENT_WRK_SESSION->find_class(rel_Class)) == NULL)
               { result = GS_BAD; break; }

            // ricavo l'insieme degli oggetti grafici collegati
            if (pClsChild->get_SelSet(rel_Key, ss) == GS_BAD)
               { result = GS_BAD; break; }

            // aggiungo gli oggetti grafici al Link Set
            ResultLS.ptr_SelSet()->add_selset(ss);

            ent_punt = (C_2INT_LONG *) ent_punt->get_next();
         }  // fine ciclo sulle relazioni di un gruppo

         if (result == GS_BAD) break;
	      gsc_Skip(p_ClsFrom_GS_VERIFIED_Rs);
      } // fine ciclo su GS_VERIFIED

      gsc_DBCloseRs(p_ClsFrom_GS_VERIFIED_Rs);
   }
   while (0);

   return result;
}


/*********************************************************/
/*  FINE FUNZIONI PER TABELLA GS_VERIFIED                */
/*********************************************************/


/*********************************************************/
/*.doc gsc_get_data                           <internal> */
/*+                                                                       
  Ricercano un record utilizzando istruzioni preparate nella struttura
  C_QRY_CLS e C_QRY_SEC usate nelle funzioni di: filtro su classi, su secondarie,
  trasferimento di valori da secondarie a principali.
  Parametri:
  C_QRY_CLS *pClsQry;         struttura che contiene le istruzioni SQL preparate
  long key; o  presbuf key;   codice chiave
  presbuf pData;              valore letto dalla istruzione SQL
  const TCHAR *ColName;       se passato viene letto la colonna indicata, altrimenti
                              verrà letta la prima colonna (default = NULL)
  int *source;                provenienza valore (TEMP o OLD) (default = NULL)

  oppure
  C_QRY_SEC *pSecQry;         struttura che contiene le istruzioni SQL preparate
  long key; o  presbuf key;   codice chiave
  presbuf pData;              valore letto dalla istruzione SQL
  const TCHAR *ColName;       se passato viene letto la colonna indicata, altrimenti
                              verrà letta la prima colonna (default = NULL)
  int *source;                provenienza valore (TEMP o OLD) (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_data(C_QRY_CLS *pClsQry, long key, presbuf pData, const TCHAR *ColName, 
                 int *source)
{
   presbuf pRbKey = acutBuildList(RTLONG, key, 0);
   if (gsc_get_data(pClsQry, pRbKey, pData, ColName, source) == GS_BAD)
      { acutRelRb(pRbKey); return GS_BAD; }
   acutRelRb(pRbKey);

   return GS_GOOD;
}
int gsc_get_data(C_QRY_CLS *pClsQry, presbuf key, presbuf pData, const TCHAR *ColName, 
                 int *source)
{                 
   GS_ERR_COD = eGSNoError;

   // Se esiste la tabella temporanea
   if (pClsQry->pTempCsr)
   {
      _RecordsetPtr pRs;
      int           IsRsCloseable;

      // verifico se si trova nel temp
      if (gsc_get_data(pClsQry->pCmdIsInTemp, key->resval.rlong, pRs, &IsRsCloseable) == GS_GOOD)
      {
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); 

         // Esegue il comando su TEMP
         if (gsc_Rb2ASIData(key, pClsQry->pTempParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadCol(pClsQry->pTempCsr, pData, ColName) == GS_BAD) return GS_BAD;
         if (source) *source = TEMP;
      }
      else
      {  // Esegue il comando su OLD
         if (gsc_Rb2ASIData(key, pClsQry->pOldParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadCol(pClsQry->pOldCsr, pData, ColName) == GS_BAD) return GS_BAD;
         if (source) *source = OLD;
      }
   }
   else
   {  // Esegue il comando su OLD
      if (gsc_Rb2ASIData(key, pClsQry->pOldParam) == GS_BAD) return GS_BAD;
      if (gsc_ASIReadCol(pClsQry->pOldCsr, pData, ColName) == GS_BAD) return GS_BAD;
      if (source) *source = OLD;
   }

   return GS_GOOD;
}

int gsc_get_data(C_QRY_SEC *pSecQry, int cls, int sub, long key, presbuf pData, int *source)
{
   presbuf pRbKey = acutBuildList(RTLONG, key, 0);
   if (gsc_get_data(pSecQry, cls, sub, pRbKey, pData, source) == GS_BAD)
      { acutRelRb(pRbKey); return GS_BAD; }
   acutRelRb(pRbKey);

   return GS_GOOD;
}
int gsc_get_data(C_QRY_SEC *pSecQry, int cls, int sub, presbuf key, presbuf pData, int *source)
{
   if (pSecQry->get_type() == GSExternalSecondaryTable)
   {
      if (gsc_Rb2ASIData(key, pSecQry->pOldParam) == GS_BAD) return GS_BAD;
      if (gsc_ASIReadCol(pSecQry->pOldCsr, pData) == GS_BAD) return GS_BAD;
      if (source) *source = OLD;
   }
   else
   {
      _RecordsetPtr pRs;
      int           IsRsCloseable, isInTemp = FALSE;
      C_RB_LIST     Buffer;

      // Se esiste la tabella temporanea
      if (pSecQry->pTempCsr)
      {
         // Verifico che esista almeno una secondaria nel temp delle relazioni
         if (gsc_get_reldata(pSecQry->pCmdIsInTemp, cls, sub, key->resval.rlong, 
                             pRs, &IsRsCloseable) == GS_GOOD)
         {
            presbuf pStatus;
            int     Status;
         
            // Verifico che non sia cancellata
            if (gsc_DBReadRow(pRs, Buffer) == GS_BAD ||
                (pStatus = Buffer.CdrAssoc(_T("STATUS"))) == NULL ||
                gsc_rb2Int(pStatus, &Status) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            if (Status == ERASED)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
            isInTemp = TRUE;
         }
      }
      
      if (isInTemp)
      {
         // Esegue il comando su TEMP
         if (gsc_Rb2ASIData(key, pSecQry->pTempParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadCol(pSecQry->pTempCsr, pData) == GS_BAD) return GS_BAD;
         if (source) *source = TEMP;
      }
      else
      {  // Esegue il comando su OLD
         if (gsc_Rb2ASIData(key, pSecQry->pOldParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadCol(pSecQry->pOldCsr, pData) == GS_BAD) return GS_BAD;
         if (source) *source = OLD;
      }
   }

   GS_ERR_COD = eGSNoError;

   return GS_GOOD;
}
int gsc_get_alldata(C_QRY_SEC *pSecQry, int cls, int sub, long key, C_RB_LIST &ColValues, int *source)
{
   presbuf pRbKey = acutBuildList(RTLONG, key, 0);
   if (gsc_get_alldata(pSecQry, cls, sub, pRbKey, ColValues, source) == GS_BAD)
      { acutRelRb(pRbKey); return GS_BAD; }
   acutRelRb(pRbKey);

   return GS_GOOD;
}
int gsc_get_alldata(C_QRY_SEC *pSecQry, int cls, int sub, presbuf key, C_RB_LIST &ColValues, int *source)
{
   if (pSecQry->get_type() == GSExternalSecondaryTable)
   {
      if (gsc_Rb2ASIData(key, pSecQry->pOldParam) == GS_BAD) return GS_BAD;
      if (gsc_ASIReadRows(pSecQry->pOldCsr, ColValues) == GS_BAD) return GS_BAD;
      if (source) *source = OLD;
   }
   else
   {
      _RecordsetPtr pRs;
      int           IsRsCloseable, isInTemp = FALSE;
      C_RB_LIST     Buffer;

      // Se esiste la tabella temporanea
      if (pSecQry->pTempCsr)
      {
         // Verifico che esista almeno una secondaria nel temp delle relazioni
         if (gsc_get_reldata(pSecQry->pCmdIsInTemp, cls, sub, key->resval.rlong, 
                             pRs, &IsRsCloseable) == GS_GOOD)
         {
            presbuf pStatus;
            int     Status;
         
            // Verifico che non sia cancellata
            if (gsc_DBReadRow(pRs, Buffer) == GS_BAD ||
                (pStatus = Buffer.CdrAssoc(_T("STATUS"))) == NULL ||
                gsc_rb2Int(pStatus, &Status) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            if (Status == ERASED)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
            isInTemp = TRUE;
         }
      }
      
      if (isInTemp)
      {
         // Esegue il comando su TEMP
         if (gsc_Rb2ASIData(key, pSecQry->pTempParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadRows(pSecQry->pTempCsr, ColValues) == GS_BAD) return GS_BAD;
         if (source) *source = TEMP;
      }
      else
      {  // Esegue il comando su OLD
         if (gsc_Rb2ASIData(key, pSecQry->pOldParam) == GS_BAD) return GS_BAD;
         if (gsc_ASIReadRows(pSecQry->pOldCsr, ColValues) == GS_BAD) return GS_BAD;
         if (source) *source = OLD;
      }
   }

   GS_ERR_COD = eGSNoError;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI DI GESTIONE GRUPPO GS_LSFILTER - INIZIO
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gs_set_ssfilter                        <external> */
/*+                                                                       
  Funzione LISP che setta il contenuto del gruppo ssfilter
  Parametri:
  (selset)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_set_ssfilter(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   GS_LSFILTER.clear();
   GS_LSFILTER.ptr_SelSet()->add_selset(arg->resval.rlname);
   gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_PutSym_ssfilter                    <external> */
/*+                                                                       
  Funzione che esporta in ambiente LISP (creandone una copia)
  il gruppo di selezione del filtro chiamdolo "ssfilter".

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gsc_PutSym_ssfilter(void)
{
   presbuf p;

   if (GS_LSFILTER.ptr_SelSet()->length() == 0)
   {
      if ((p = acutBuildList(RTNIL, 0)) == NULL) return GS_BAD;
   }
   else
   {
      ads_name ss;

      GS_LSFILTER.ptr_SelSet()->get_selection(ss);
      if ((p = acutBuildList(RTPICKS, ss, 0)) == NULL) return GS_BAD;
   }
   
   if (acedPutSym(_T("ssfilter"), p) != RTNORM)
      { acutRelRb(p); return GS_BAD; }
   acutRelRb(p);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_get_ssfilter                        <external> */
/*+                                                                       
  Funzione LISP che legge il contenuto del gruppo ssfilter:
  risultato dell'ultimo filtro.
  Parametri:

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_get_ssfilter(void)
{
   ads_name ss;

   acedRetNil();

   if (GS_LSFILTER.ptr_SelSet()->length() == 0 || GS_LSFILTER.cls == 0) return RTNORM;
   GS_LSFILTER.ptr_SelSet()->get_selection(ss);
   acedRetName(ss, RTPICKS);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_KeyListFilter                   <external> */
/*+                                                                       
  Funzione LISP che legge la lista dei codici delle entità
  risultato dell'ultimo filtro.
  Parametri:

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_get_KeyListFilter(void)
{
   C_RB_LIST ret;

   acedRetNil();

   if (GS_LSFILTER.ptr_KeyList()->get_count() == 0 || GS_LSFILTER.cls == 0) return RTNORM;
   ret << GS_LSFILTER.ptr_KeyList()->to_rb();
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_clsfilter                        <external> */
/*+                                                                       
  Funzione LISP che legge i codici dell'ultima classe filtrata.

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gs_get_clsfilter(void)
{
   C_RB_LIST ret;

   acedRetNil();

   if (GS_LSFILTER.cls == 0) return RTNORM;

   if ((ret << acutBuildList(RTSHORT, GS_LSFILTER.cls,
                             RTSHORT, GS_LSFILTER.sub, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return RTERROR; }

   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_save_LastFilterResult              <external> */
/*+                                                                       
  Funzione che scrive su file il contenuto del gruppo LSFILTER.
  Le prime due righe del file conterranno rispettivamente codice e sottocodice
  della classe su cui è stato impostato il filtro.
  Parametri
  const TCHAR *name;  path del file (opzionale)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gsc_save_LastFilterResult(const TCHAR *name)
{
   C_STRING path;

   if (!name)
   {
      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T('\\');
      path += GEOTEMPSESSIONDIR;
      path += _T('\\');
      path += GEO_LSFILTER_FILE;
   }
   else path = name;

   if (GS_LSFILTER.save(path.get_name()) == GS_BAD) return GS_BAD; 

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_load_LastFilterResult              <external> */
/*+                                                                       
  Funzione che legge da un file il contenuto del gruppo LSFILTER.
  Le prime due righe del file conterranno rispettivamente codice e sottocodice
  della classe su cui è stato impostato il filtro.
  Parametri
  const TCHAR *name;  path del file (opzionale)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR. 
-*/  
/*********************************************************/
int gsc_load_LastFilterResult(const TCHAR *name)
{
   C_STRING path;

   if (!name)
   {
      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T('\\');
      path += GEOTEMPSESSIONDIR;
      path += _T('\\');
      path += GEO_LSFILTER_FILE;
   }
   else path = name;

   if (GS_LSFILTER.load(path.get_name()) == GS_BAD) return GS_GOOD;
   gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro
   
   return GS_GOOD;
}


/*****************************************************************************/
/*.doc get_GS_LSFILTER()                                          <external> */
/*+
   Questa funzione è stata introdotta perchè nelle librerie non si vede 
   direttamente la variabile GS_LSFILTER, nel file .h è una funzione DllExport.
-*/  
/*****************************************************************************/
C_LINK_SET *get_GS_LSFILTER(void)
{
   return &GS_LSFILTER;
}
void set_GS_LSFILTER(int *cls, int *sub, C_SELSET *pSS, C_LONG_BTREE *pKeyList)
{
   if (cls) GS_LSFILTER.cls = *cls;
   if (sub) GS_LSFILTER.sub = *sub;
   if (pSS) GS_LSFILTER.ptr_SelSet()->add_selset(*pSS);
   if (pKeyList)
   {
      C_BLONG *pKey = (C_BLONG *) pKeyList->go_top();
      long    Key;

      GS_LSFILTER.ptr_KeyList()->remove_all();
      while (pKey)
      {
         Key = pKey->get_key();
         GS_LSFILTER.ptr_KeyList()->add(&Key);
         pKey = (C_BLONG *) pKeyList->go_next();
      }
   }

   return;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI DI GESTIONE GRUPPO GS_LSFILTER                            - FINE
// FUNZIONI PER IL TRASFERIMENTO DI VALORI DA SECONDARIA A PRINCIPALE - INIZIO
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc (mod 2) gsc_do_SecValueTransfer        <external> */
/*+                                                                       
  Effettua il trasferimento di un valore di un attributo
  di una scheda secondaria in un attributo della relativa entità GEOsim principale
  della banca dati caricata nella sessione di lavoro corrente.
  Parametri:
  int cls;						      Codice classe
  int sub;						      Codice sotto-classe
  C_CLASS_SQL_LIST &SQLCondList; Condizioni SQL
  C_LINK_SET *pLinkSet;       1) Se la classe di entità ha grafica rappresenta 
                                 il gruppo di selezione degli oggetti su cui applicare il filtro;
                              2) se la classe di entità non ha grafica rappresenta
                                 la lista dei codici delle entità su cui applicare il filtro;
                              se = NULL vuol dire che il filtro verrà applicato 
                              su tutto il territorio estratto
  C_STRING &Dest;                Nome dell'attributo della tabella principale destinazione
                                 del trasferimento dati da secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_do_SecValueTransfer(int cls, int sub, C_CLASS_SQL_LIST &SQLCondList,
                            C_LINK_SET *pLinkSet, C_STRING &Dest)
{
   C_CLASS    *pCls;
   C_LINK_SET ResultLS;

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GS_BAD;
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   acutPrintf(gsc_msg(426)); // "\nInizio trasferimento dati da tabella secondaria..."

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   if (gsc_db_prepare_for_filter(pCls, SQLCondList) == GS_BAD)
      { QRY_CLS_LIST.remove_all(); return GS_BAD; }
   if (gsc_exe_SQL_filter(pCls, SQLCondList, pLinkSet, ResultLS, Dest.get_name()) == GS_BAD)
      { QRY_CLS_LIST.remove_all(); return GS_BAD; }
   QRY_CLS_LIST.remove_all();  // ripulisco la query_list

   acutPrintf(gsc_msg(111)); // "\nTerminato.\n"

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_SecValueTransfer                  <internal> */
/*+                                                                       
  Esegue un filtro con condizione SQL sui dati delle tabelle
  secondarie le cui principali sono contenute nel link-set 
  passato come parametro. Per ogni entità principale viene aggiornata
  la sua scheda con il valore della prima scheda secondaria che ha
  soddisfatto le condizioni.
  Parametri:
  C_CLASS *pCls;           Puntatore alla classe
  C_LONG_LIST &IDList;     Lista di codici ID risultato dal filtro
  const TCHAR *DestAttr;   Nome dell'attributo della tabella principale destinazione
                           del trasferimento dati da secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
static int gsc_SecValueTransferGroup(C_CLASS *pCls, C_LONG_LIST &IDList,
                                     const TCHAR *DestAttr)
{
   C_QRY_CLS      *pClsQry;
   C_QRY_SEC_LIST *pSecQryList;
   C_QRY_SEC      *pSecQry;
   int            result = GS_GOOD;
   C_ATTRIB_LIST  *p_attrib_list = pCls->ptr_attrib_list();
   C_ATTRIB       *p_attrib;

   // non si può usare come attributo di destinazione l'attributo chiave e 
   // un attributo calcolato
   if (pCls->ptr_info()->key_attrib.comp(DestAttr, FALSE) == 0)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if ((p_attrib = (C_ATTRIB *) p_attrib_list->search_name(DestAttr, FALSE)) == NULL)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (p_attrib->is_calculated() == GS_GOOD)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if ((pClsQry = (C_QRY_CLS *) QRY_CLS_LIST.search(pCls->ptr_id()->code, 
                                                    pCls->ptr_id()->sub_code)) == NULL)
      return GS_GOOD;
   
   pSecQryList = pClsQry->ptr_qry_sec_list();

   // considero solo la prima istruzione compilata nella funzione "prepare_for_filter"
   if ((pSecQry = (C_QRY_SEC *) pSecQryList->get_head()) != NULL)
   {
      C_LONG         *pID;
      C_STRING       TempTableRef;
      C_DBCONNECTION *pConn;
      resbuf         *pClsDestValue, SecColValue, *PrimaryKey = NULL;
      C_SELSET       dummySS;
      C_RB_LIST      ColValues;
      long           qty = 0, key_val;
      C_PREPARED_CMD_LIST TempOldCmdList;
      C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1075)); // "Trasferimento dati da tabella secondaria"

      acutPrintf(GS_LFSTR);
      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      pConn = pCls->ptr_info()->getDBConnection(TEMP);
      // ricavo tabella temporanea della classse
      if (pCls->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // preparo il comando per la lettura in TEMP e OLD
      if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

      pID = (C_LONG *) IDList.get_head();
      // ciclo sulle entità della classe
      while (pID)
      {
         key_val = pID->get_id();

         // se è aggiornabile viene bloccata
         if (pCls->is_updateable(key_val, NULL, GS_GOOD) == GS_GOOD)
         {
            // effettuo ricerca in TEMP/OLD della classe
            if (pCls->query_data(key_val, ColValues, &TempOldCmdList) == GS_BAD)
               { result = GS_BAD; break; }

            GS_ERR_COD = eGSNoError;
            // se NON si tratta di una secondaria esterna
            if (pSecQry->get_type() == GSInternalSecondaryTable)
            {  // allora verifico se è presente nella tabella TEMP/OLD
               if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                key_val, &SecColValue) == GS_BAD)
                  if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
            }
            else // se si tratta di una secondaria esterna
            {  // leggo valore primario
               if (!PrimaryKey)
                  PrimaryKey = ColValues.CdrAssoc(pSecQry->get_key_pri());

               if (PrimaryKey->restype != RTNIL)
               {
                  // effettuo ricerca in tabella secondaria
                  if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                   PrimaryKey, &SecColValue) == GS_BAD)
                     if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
               }
               else
               {
                  GS_ERR_COD = eGSInvalidKey; // roby 2014
                  gsc_RbSubstNIL(&SecColValue);
               }
            }

            // almeno una secondaria soddisfa il criterio SQL
            if (GS_ERR_COD == eGSNoError)
            {
               // cerco valore dell'attributo di destinazione
               if (!(pClsDestValue = ColValues.CdrAssoc(DestAttr)))
                  { GS_ERR_COD = eGSInvAttribName; result = GS_BAD; break; }

               // aggiornamento dell'attributo destinazione della scheda principale
               if (gsc_RbSubst(pClsDestValue, &SecColValue) == GS_BAD)
                  { result = GS_BAD; break; }

               // aggiorno l'entità
               if (pCls->upd_data(key_val, ColValues,
                                  ((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), // Temp
                                  &dummySS) == GS_BAD)
                     { result = GS_BAD; break; }
               
               StatusLineMsg.Set(++qty); // "%ld entità GEOsim elaborate."
            }
         }

         pID = (C_LONG *) IDList.get_next(); // vado al successivo
      }

      StatusLineMsg.End(gsc_msg(310), qty); // "%ld entità GEOsim elaborate."
   }

   return result;
}


/************************************************************/
/*.doc gsc_SecValueTransfer                  <internal> */
/*+                                                                       
  Esegue un filtro con condizione SQL sui dati delle tabelle
  secondarie le cui principali sono contenute nel link-set 
  passato come parametro. Per ogni entità principale viene aggiornata
  la sua scheda con il valore della prima scheda secondaria che ha
  soddisfatto le condizioni.
  Parametri:
  C_CLASS *pCls;           	Puntatore alla classe
  C_LINK_SET *ResultLS;       Link Set con il risultato del filtro
  const TCHAR *DestAttr;      Nome dell'attributo della tabella principale destinazione
                              del trasferimento dati da secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
static int gsc_SecValueTransfer(C_CLASS *pCls, C_LINK_SET &ResultLS,
                                const TCHAR *DestAttr)
{
   C_QRY_CLS      *pClsQry;
   C_QRY_SEC_LIST *pSecQryList;
   C_QRY_SEC      *pSecQry;
   int            result = GS_GOOD;
   C_ATTRIB_LIST  *p_attrib_list = pCls->ptr_attrib_list();
   C_ATTRIB       *p_attrib;

   // non si può usare come attributo di destinazione l'attributo chiave e 
   // un attributo calcolato
   if (pCls->ptr_info()->key_attrib.comp(DestAttr, FALSE) == 0)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if ((p_attrib = (C_ATTRIB *) p_attrib_list->search_name(DestAttr, FALSE)) == NULL)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (p_attrib->is_calculated() == GS_GOOD)
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if ((pClsQry = (C_QRY_CLS *) QRY_CLS_LIST.search(pCls->ptr_id()->code, 
                                                    pCls->ptr_id()->sub_code)) == NULL)
      return GS_GOOD;
   
   pSecQryList = pClsQry->ptr_qry_sec_list();

   // considero solo la prima istruzione compilata nella funzione "prepare_for_filter"
   if ((pSecQry = (C_QRY_SEC *) pSecQryList->get_head()) != NULL)
   {
      C_STRING       TempTableRef;
      C_DBCONNECTION *pConn;
      C_RB_LIST      ColValues;
      long           qty = 0, key_val;
      resbuf         *pClsDestValue, SecColValue, *PrimaryKey = NULL;
      C_SELSET       dummySS;
      C_BLONG        *pKey;
      C_PREPARED_CMD_LIST TempOldCmdList;
      C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1075)); // "Trasferimento dati da tabella secondaria"

      pConn = pCls->ptr_info()->getDBConnection(TEMP);
      // ricavo tabella temporanea della classse
      if (pCls->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // preparo il comando per la lettura in TEMP e OLD
      if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_top();
      while (pKey)
      {
         key_val = pKey->get_key();

         if (pCls->get_SelSet(key_val, dummySS) == GS_BAD)
            { result = GS_BAD; break; }

         // se è aggiornabile viene bloccata
         if (pCls->is_updateableSS(key_val, dummySS) == GS_GOOD)
         {
            // effettuo ricerca in TEMP/OLD della classe
            if (pCls->query_data(key_val, ColValues, &TempOldCmdList) == GS_BAD)
               { result = GS_BAD; break; }

            GS_ERR_COD = eGSNoError;
            // se NON si tratta di una secondaria esterna
            if (pSecQry->get_type() == GSInternalSecondaryTable)
            {  // allora verifico se è presente nella tabella TEMP/OLD
               if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                key_val, &SecColValue) == GS_BAD)
                  if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
            }
            else // se si tratta di una secondaria esterna
            {  // leggo valore primario
               if (!PrimaryKey)
                  PrimaryKey = ColValues.CdrAssoc(pSecQry->get_key_pri());

               if (PrimaryKey->restype != RTNIL)
               {
                  // effettuo ricerca in tabella secondaria
                  if (gsc_get_data(pSecQry, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                   PrimaryKey, &SecColValue) == GS_BAD)
                     if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
               }
               else
               {
                  GS_ERR_COD = eGSInvalidKey; // roby 2014
                  gsc_RbSubstNIL(&SecColValue);
               }
            }

            // almeno una secondaria soddisfa il criterio SQL
            if (GS_ERR_COD == eGSNoError)
            {
               // cerco valore dell'attributo di destinazione
               if (!(pClsDestValue = ColValues.CdrAssoc(DestAttr)))
                  { GS_ERR_COD = eGSInvAttribName; result = GS_BAD; break; }

               // aggiornamento dell'attributo destinazione della scheda principale
               if (gsc_RbSubst(pClsDestValue, &SecColValue) == GS_BAD)
                  { result = GS_BAD; break; }

               // aggiorno l'entità
               if (pCls->upd_data(key_val, ColValues,
                                  ((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), // Temp
                                  &dummySS) == GS_BAD)
                     { result = GS_BAD; break; }

               StatusLineMsg.Set(++qty); // "%ld entità GEOsim elaborate."
            }
         }
         
         pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_next();
      }

      StatusLineMsg.End(gsc_msg(310), qty); // "%ld entità GEOsim elaborate."
   }

   return result;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER IL TRASFERIMENTO DI VALORI DA SECONDARIA A PRINCIPALE - FINE
// ANALISI SEGMENTAZIONE DINAMICA  -  INIZIO
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_do_SecDynamicSegmentationAnalyze                       <external> */
/*+                                                                       
  Effettua l'analisi della segmentazione dinamica di una tabella secondaria 
  di una classe della banca dati caricata nella sessione. Vengono creati nuovi 
  oggetti grafici per ogni segmentazione.
  Parametri:
  C_SECONDARY *pSec;    Puntatore a tabella secondaria
  C_STRING &SQLCond;    Condizione SQL per la tabella secondaria
  C_LINK_SET *pLinkSet; 1) Se la classe di entità ha grafica rappresenta 
                           il gruppo di selezione degli oggetti su cui applicare l'analisi;
                        2) se la classe di entità non ha grafica rappresenta
                           la lista dei codici delle entità su cui applicare l'analisi;
                        se = NULL vuol dire che l'analisi verrà applicata 
                        su tutto il territorio estratto
  C_FAS &FAS;           Caratteristiche grafiche degli oggetti grafici generati
  long  flag_set;       Flag a bit per sapere quali proprietà gestire nella FAS
  C_STRING &LblFun;     Funzione geolisp per generare le etichette 
                        (se stringa vuota non verranno generate etichette)
  C_FAS &LblFAS;        Caratteristiche grafiche delle etichette generate
  long  Lbl_flag_set;   Flag a bit per sapere quali proprietà gestire nella FAS delle etichette
  bool  AddODValues     Flag per aggiungere i dati oggetto con i dati alfanumerici della 
                        scheda secondaria agli oggetti grafici generati
  C_SELSET &outSS;      Oggetti grafici generati dall'analisi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*****************************************************************************/
int gsc_do_SecDynamicSegmentationAnalyze(C_SECONDARY *pSec, C_STRING &SQLCond, C_LINK_SET *pLinkSet,
                                         C_FAS &FAS, long flag_set, 
                                         C_STRING &LblFun, C_FAS &LblFAS, long  Lbl_flag_set,
                                         bool AddODValues, C_SELSET &outSS)
{
   C_CLASS             *pCls;
   C_LINK_SET          ResultLS;
   C_SEC_SQL           SecSQL;
   C_QRY_SEC           QrySec;
   C_SELSET            GraphObjs, LblSS;
   ads_name            NewLbl;
   C_RB_LIST           ColValues, SecColValue;
   C_STRING            TempTableRef, AttribName, What;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_BLONG             *pKey;
   int                 result = GS_GOOD;
   presbuf             PrimaryKey = NULL;
   long                qty = 0;
   AcMapODTable        *pClsODTable = NULL, *pSecODTable = NULL;
   AcMapODRecordIterator *pIter = NULL;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1076)); // "Analisi segmentazione dinamica"

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (!pSec) return GS_BAD;

   StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

   // Ritorna il puntatore alla classe cercata
   if ((pCls = pSec->ptr_class()) == NULL) return GS_BAD;

   SecSQL.set_sec(pSec->get_key());
   SecSQL.set_sql(SQLCond.get_name());
   if (SecSQL.prepare_for_filter(pSec->ptr_class()) == GS_BAD) return GS_BAD;

   if (AddODValues) // leggo tutti i campi della secondaria (poi li devo scrivere in OD)
      What = _T('*');
   else
   {
      AttribName = pSec->ptr_info()->real_init_distance_attrib;
      // Correggo eventualmente la sintassi del nome del campo con i doppi apici (") per MAP
      gsc_AdjSyntaxMAPFormat(AttribName);
      What = AttribName;
      if (pSec->DynSegmentationType() == GSLinearDynSegmentation)
      {
         What += _T(",");
         AttribName = pSec->ptr_info()->real_final_distance_attrib;
         // Correggo eventualmente la sintassi del nome del campo con i doppi apici (") per MAP
         gsc_AdjSyntaxMAPFormat(AttribName);
         What += AttribName;
      }
   }

   if (QrySec.initialize(pCls->ptr_id()->code, pCls->ptr_id()->sub_code, pSec->get_key(),
                          What.get_name(), SecSQL.get_sql()) == GS_BAD) 
      return GS_BAD;

   if (!pLinkSet) // filtro su tutto il territorio estratto
   {
      if (ResultLS.initSQLCond(pCls, NULL) != GS_GOOD) return GS_BAD;
   }
   else // da gruppo di selezione
      if (ResultLS.initSelSQLCond(pCls, *(pLinkSet->ptr_SelSet()), NULL) != GS_GOOD)
         return GS_BAD;

   // ricavo tabella temporanea della classse
   if (pCls->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

   // preparo il comando per la lettura in TEMP e OLD
   if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

   pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_top();
   while (pKey)
   {
      // effettuo ricerca in TEMP/OLD della classe
      if (pCls->query_data(pKey->get_key(), ColValues, &TempOldCmdList) == GS_BAD)
         { result = GS_BAD; break; }

      GS_ERR_COD = eGSNoError;
      // se NON si tratta di una secondaria esterna
      if (QrySec.get_type() == GSInternalSecondaryTable)
      {  // allora verifico se è presente nella tabella TEMP/OLD
         if (gsc_get_alldata(&QrySec, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                             pKey->get_key(), SecColValue) == GS_BAD)
            if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
      }
      else // se si tratta di una secondaria esterna
      {  // leggo valore primario
         if (!PrimaryKey)
            PrimaryKey = ColValues.CdrAssoc(QrySec.get_key_pri());

         if (PrimaryKey->restype != RTNIL)
         {
            // effettuo ricerca in tabella secondaria
            if (gsc_get_alldata(&QrySec, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                PrimaryKey, SecColValue) == GS_BAD)
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
         }
         else 
            GS_ERR_COD = eGSInvalidKey;
      }

      // se c'è almeno una secondaria che soddisfa il criterio SQL
      if (GS_ERR_COD != eGSInvalidKey)
      {
         presbuf      pRow, p;
         double       DistFromStart, Length, ltScale, Width, Rot, OffSet;
         ads_name     ent, NewEnt;
         C_POINT_LIST PtList;
         ads_point    Pt, LblPt;
         C_STRING     Layer, LineType;
         C_COLOR      Color;

         if (pCls->get_SelSet(pKey->get_key(), GraphObjs, GRAPHICAL) == GS_BAD)
            { result = GS_BAD; break; }
         // considero solo il primo oggetto
         if (GraphObjs.entname(0, ent) == GS_BAD)
            { result = GS_BAD; break; }
         // leggo le caratteristiche grafiche
         gsc_getLayer(ent, Layer);
         gsc_get_color(ent, Color);

         pRow = SecColValue.nth(0);
         while (pRow && pRow->restype != RTLE)
         {
            if ((p = gsc_CdrAssoc(pSec->ptr_info()->real_init_distance_attrib.get_name(), pRow)) == NULL)
               { result = GS_BAD; break; }
            if (gsc_rb2Dbl(p, &DistFromStart) == GS_BAD)
            {  // Riga successiva
               if ((pRow = gsc_scorri(pRow)) != NULL) pRow = pRow->rbnext;
               continue;
            }

            if ((p = gsc_CdrAssoc(pSec->ptr_info()->real_final_distance_attrib.get_name(), pRow)) == NULL)
            { // GSPunctualDynSegmentation
               if ((p = gsc_CdrAssoc(pSec->ptr_info()->real_offset_attrib.get_name(), pRow)) == NULL)
                  OffSet = 0;
               else
                  if (gsc_rb2Dbl(p, &OffSet) == GS_BAD) OffSet = 0;

               if (gsc_getPntRtzOnObj(ent, Pt, &Rot, _T("S"), DistFromStart, OffSet) == GS_BAD)
                  { result = GS_BAD; break; }

               // Disegno l'oggetto grafico di tipo puntuale
               if (gsc_insert_block(FAS.block, Pt, FAS.block_scale, FAS.block_scale, gsc_rad2grd(Rot)) != GS_GOOD)
                  { result = GS_BAD; break; }

               // Se si deve disegnare l'etichetta
               if (LblFun.len() > 0)
                  // Offset del 60% dell'altezza
                  if (gsc_getPntRtzOnObj(ent, LblPt, &Rot, _T("S"), 0.0, LblFAS.h_text * 0.6) == GS_BAD)
                     { result = GS_BAD; break; }
            }
            else // GSLinearDynSegmentation
            {
               if (gsc_rb2Dbl(p, &Length) == GS_BAD) Length = 0;
               else Length = Length - DistFromStart;

               if ((p = gsc_CdrAssoc(pSec->ptr_info()->real_offset_attrib.get_name(), pRow)) == NULL)
                  OffSet = 0;
               else
                  if (gsc_rb2Dbl(p, &OffSet) == GS_BAD) OffSet = 0;

               if (gsc_getPtList_BetweenPts(ent, DistFromStart, Length, PtList) == GS_BAD)
                  { result = GS_BAD; break; }

               if (OffSet != 0) PtList.OffSet(OffSet);

               // leggo le caratteristiche grafiche
               gsc_get_lineType(ent, LineType);
               gsc_get_ltScale(ent, &ltScale);
               gsc_get_width(ent, &Width);

               // Disegno l'oggetto grafico di tipo lineare
               if (gsc_insert_pline(PtList, Layer.get_name(), &Color, LineType.get_name(), ltScale, Width) == GS_BAD)
                  { result = GS_BAD; break; }

               // Se si deve disegnare l'etichetta
               if (LblFun.len() > 0)
               {
                  acdbEntLast(NewEnt);
                  // Offset del 60% dell'altezza
                  if (gsc_getPntRtzOnObj(NewEnt, LblPt, &Rot, _T("MML"), 0.0, LblFAS.h_text * 0.6) == GS_BAD)
                     { result = GS_BAD; break; }
               }
            }

            acdbEntLast(NewEnt);
            outSS.add(NewEnt);

            // Se si deve disegnare l'etichetta
            if (LblFun.len() > 0)
            {
               presbuf  rb;
               TCHAR    value[2 * MAX_LEN_FIELD];
               C_ATTRIB *pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(LblFun.get_name(), FALSE);
               C_STRING LblText;

               // Se l'etichetta è il valore di un attributo
               if (pAttrib)
               {
                  if (!(rb = gsc_CdrAssoc(LblFun.get_name(), pRow, FALSE)))
                     { result = GS_BAD; break; }
                  // Questa funzione effettua la conversione di un valore in formato
                  // stringa seguendo le impostazioni di Window.
                  if (pAttrib->ParseToString(rb, LblText, pRow,
                                             pCls->ptr_id()->code, 
                                             pCls->ptr_id()->sub_code, 
                                             pSec->get_key()) == GS_BAD)
                     { result = GS_BAD; break; }
               }
               else // Se l'etichetta è il risultato di una funzione di calcolo
               {
                  size_t ptr = 0;

                  // carico le variabili locali a "GEOsim"
                  pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->get_head();
                  while (pAttrib)
                  {
                     // ricerca del valore
                     if (!(rb = gsc_CdrAssoc(pAttrib->get_name(), pRow, FALSE)))
                     { result = GS_BAD; break; }
                     // Conversione resbuf in stringa lisp
                     if (rb2lspstr(rb, value, pAttrib->len, pAttrib->dec) == GS_BAD)
                        { result = GS_BAD; break; }
                     if (alloc_var(TRUE, NULL, &ptr, _T('L'), _T("GEOsim"), 
                                   pAttrib->get_name(), value, NULL) == GS_BAD) 
                        { result = GS_BAD; break; }

                     pAttrib = (C_ATTRIB *) pAttrib->get_next();   
                  }

                  if (result == GS_GOOD && 
                      (rb = gsc_exec_lisp(_T("GEOsim"), LblFun.get_name(), NULL)) != NULL)
                     LblText.paste(gsc_rb2str(rb));
                  release_var(_T("GEOsim"));
               }

               if (LblText.len() > 0)
                  if (gsc_insert_text(LblText.get_name(), LblPt, gsc_get_CurrentTextSize(), gsc_rad2grd(Rot)) == GS_GOOD)
                  {
                     acdbEntLast(NewLbl);
                     LblSS.add(NewLbl);
                  }
            }

            if (AddODValues)
            {
               C_RB_LIST dummyList;

               if (!pIter) 
                  if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) { result = GS_BAD; break; }

               // Aggiungo i dati oggetto della classe
               if (!pClsODTable)
               {
                  pCls->get_CompleteName(TempTableRef);
                  // Se il nome della tabella OD è troppo lungo
                  if (TempTableRef.len() > MAX_LEN_ODTABLE_NAME)
                     TempTableRef = TempTableRef.left(MAX_LEN_ODTABLE_NAME);

                  pCls->ptr_attrib_list()->init_ADOType(pCls->ptr_info()->getDBConnection(OLD));
                  if ((pClsODTable = gsc_createODTable(TempTableRef, pCls->ptr_attrib_list())) == NULL)
                     { result = GS_BAD; break; }
               }
               if (gsc_setODRecord(NewEnt, ColValues, pClsODTable, pIter) == GS_BAD)
                  { result = GS_BAD; break; }

               // Aggiungo i dati oggetto della tabella secondaria
               if (!pSecODTable)
               {
                  pCls->get_CompleteName(TempTableRef);
                  // Se il nome della tabella OD è troppo lungo
                  if (TempTableRef.len() + gsc_strlen(pSec->name) + 1 > MAX_LEN_ODTABLE_NAME)
                  {                    
                     TempTableRef = TempTableRef.left(MAX_LEN_ODTABLE_NAME - int (MAX_LEN_ODTABLE_NAME / 2));
                     TempTableRef += _T('_');
                     TempTableRef += pSec->name;
                     TempTableRef = TempTableRef.left(MAX_LEN_ODTABLE_NAME);
                  }
                  else
                  {
                     TempTableRef += _T('_');
                     TempTableRef += pSec->name;
                  }

                  pSec->ptr_attrib_list()->init_ADOType(pSec->ptr_info()->getDBConnection(OLD));
                  if ((pSecODTable = gsc_createODTable(TempTableRef, pSec->ptr_attrib_list())) == NULL)
                     { result = GS_BAD; break; }
               }
               // creo la lista dei valori da inserire
               if ((p = acutBuildList(RTLB, 0)) == NULL) { result = GS_BAD; break; }
               if (gsc_scorcopy(pRow, p) == NULL)
                  { acutRelRb(p); result = GS_BAD; break; }
               dummyList << p;
               if (gsc_setODRecord(NewEnt, dummyList, pSecODTable, pIter) == GS_BAD)
                  { result = GS_BAD; break; }
            }
            
            // Riga successiva
            if ((pRow = gsc_scorri(pRow)) != NULL) pRow = pRow->rbnext;
         }

         if (result == GS_BAD) break;

         StatusLineMsg.Set(++qty); // "%ld entità GEOsim elaborate."
      }

      pKey = (C_BLONG *) ResultLS.ptr_KeyList()->go_next(); // vado al successivo
   }
   StatusLineMsg.End(gsc_msg(310), qty); // "%ld entità GEOsim elaborate."

   if (pIter) delete pIter;
   if (pClsODTable) delete pClsODTable;
   if (pSecODTable) delete pSecODTable;
   
   if (flag_set != GSNoneSetting)
      gsc_modifyEntToFas(outSS, &FAS, flag_set);

   if (Lbl_flag_set != GSNoneSetting)
      gsc_modifyEntToFas(LblSS, &LblFAS, Lbl_flag_set);

   acutPrintf(gsc_msg(111)); // "\nTerminato.\n"

   return result;
}


///////////////////////////////////////////////////////////////////////////////
// ANALISI SEGMENTAZIONE DINAMICA  -  FINE
// FUNZIONI DEL FILTRO TEMATICO   - INIZIO
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsddthemafilter                                                       */
/*+
  Questa comando imposta il filtro tematico
-*/  
/*****************************************************************************/
void gsddthemafilter(void)
{
   int          ret = GS_GOOD, status = DLGOK, dcl_file, pos = 0;
	int          flag_thm = FALSE, pos_thm = 0, num_thm = 0;
	long         FlagFas = 0, fas_value = 0;
   ads_hdlg     dcl_id;
   C_CLASS      *pCls;
   C_NODE       *ptr = NULL;
   C_LINK_SET   ResultLS;
   bool         UseLS;
   C_STRING     path, str;  
	C_STR        *thm;
	C_ATTRIB     *pAttrib;
	C_STR_LIST   thm_list;
   C_DBCONNECTION *pConn;
   Common_Dcl_main_filter_Struct CommonStruct;
	Common_Dcl_sql_bldqry_Struct CommonStructClassSQL;
	Common_Dcl_legend_Struct CommonStruct4Leg;
	Common_Dcl_SpatialSel_Struct CommonSpatialStruct;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // seleziona la classe o sottoclasse su cui impostare il filtro multiplo
   if ((ret = gsc_ddselect_class(NONE, &pCls)) != GS_GOOD)
      if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      else return GEOsimAppl::CMDLIST.CancelCmd();

   if (!pCls->ptr_attrib_list() || !pCls->ptr_info()) 
      { GS_ERR_COD = eGSInvClassType; return GEOsimAppl::CMDLIST.ErrorCmd(); }

	// verifico le caratteristiche grafiche della classe selezionata
   FlagFas = pCls->what_is_graph_updateable();

	CommonStruct4Leg.FlagThm = GSNoneSetting;
	// in funzione delle caratteristiche trovate creo la lista delle possibili scelte
	if (FlagFas > 0)
	{
		if (FlagFas & GSLayerSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(633));					   	//piano 
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSLayerSetting;
		}     
		if (FlagFas & GSBlockNameSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(826));			    			//blocco 
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSBlockNameSetting;
		}
		if (FlagFas & GSTextStyleSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(632));			    			//stile testo
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + 4;
		}	 
		if (FlagFas & GSBlockScaleSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(825));			    			//scala
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSBlockScaleSetting;
		}	 
		if (FlagFas & GSLineTypeSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(631));			    			//tipolinea
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSLineTypeSetting;
		}	
		if (FlagFas & GSWidthSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(483));			    			//larghezza
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSWidthSetting;
		}	 
		if (FlagFas & GSColorSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(629));			    			//	colore
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSColorSetting;
		}	 
		if (FlagFas & GSTextHeightSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(718));			    			// altezza testo
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSTextHeightSetting;
		}  
		if (FlagFas & GSHatchNameSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(630));			    			// riempimento
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSHatchNameSetting;
		}
		if (FlagFas & GSHatchColorSetting)
		{
			if ((thm = new C_STR) == NULL)
            { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
			thm->set_name(gsc_msg(229));			    			//	Colore riempimento
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSHatchColorSetting;
		}	 

	}
	if ((thm = new C_STR) == NULL)
      { GS_ERR_COD=eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }
	thm->set_name(gsc_msg(833));			    			//tutte le proprietà
	CommonStruct4Leg.thm_list.add_tail(thm);
   num_thm = CommonStruct4Leg.thm_list.get_count();
   // ordino per nome la lista dei tematismi
   CommonStruct4Leg.thm_list.sort_name(TRUE, TRUE); // sensitive e ascending

   // inizializzazio le CommonStruct che servono per passare i dati fra DCL
	CommonStruct.SpatialSel   = _T("\"\" \"\" \"\" \"Location\" (\"all\") \"\"");
   CommonStruct4Leg.Boundary = ALL_SPATIAL_COND; // tutto
   CommonStruct.LocationUse  = true;
   CommonStruct.LastFilterResultUse = false;
   CommonStruct.Logical_op  = UNION;
   CommonStruct.Tools       = GSHighlightFilterSensorTool;
   CommonStruct.ClsSQLSel   = 0;
   CommonStruct.SecSQLSel   = 0;

	CommonStruct4Leg.prj = GS_CURRENT_WRK_SESSION->get_PrjId();
	CommonStruct4Leg.cls	= pCls->ptr_id()->code;
	CommonStruct4Leg.sub = pCls->ptr_id()->sub_code;
	CommonStruct4Leg.type_interv = 1;
	CommonStruct4Leg.int_build = 1;
	CommonStruct4Leg.n_interv = 0;
	CommonStruct4Leg.current_line = 0;
	CommonStruct4Leg.FlagFas = CommonStruct4Leg.FlagThm;
	CommonStruct4Leg.FlagLeg = 0;
	CommonStruct4Leg.attrib_name = GS_EMPTYSTR;			
	CommonStruct4Leg.attrib_type = GS_EMPTYSTR;
   CommonStruct4Leg.thm         = GS_EMPTYSTR;
   CommonStruct4Leg.ins_leg = FALSE;
   CommonStruct4Leg.AddObjs2SaveSet = FALSE;
   CommonStruct4Leg.Inverse = false;

   if (CommonStruct.sql_list.initialize(pCls) != GS_GOOD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) == RTERROR)
      return GEOsimAppl::CMDLIST.ErrorCmd();

	while (1)
   {
      ads_new_dialog(_T("main_filter4legend"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

		ads_start_list(dcl_id, _T("Listattrib"), LIST_NEW, 0);
		gsc_add_list(GS_EMPTYSTR);        // la prima riga vuota
		pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
		while (pAttrib)
		{
			gsc_add_list(pAttrib->get_name());
			pAttrib = (C_ATTRIB *) pAttrib->get_next();
		}
		ads_end_list();

   	ads_start_list(dcl_id, _T("SelThm"), LIST_NEW, 0);
		gsc_add_list(GS_EMPTYSTR);        // la prima riga vuota
		thm = (C_STR*)CommonStruct4Leg.thm_list.get_head();
		while(thm)
		{
			gsc_add_list(thm->get_name());
			thm=(C_STR*)thm->get_next();
		}
		ads_end_list();

      if (gsc_setTileDclmain_filter(dcl_id, &CommonStruct) == GS_BAD)
         { ret = GS_BAD; break; }

      str = gsc_msg(824);									 //   "N.Int: "
   	str += CommonStruct4Leg.n_interv;	                   
   	ads_set_tile(dcl_id, _T("N_Int"), str.get_name());

		// se l'attributo è di tipo booleano inibisco il tile 'continuo'
		pConn = pCls->ptr_info()->getDBConnection(OLD);
	   if (pConn->IsBoolean(CommonStruct4Leg.attrib_type.get_name()) == GS_GOOD)
		{
	      ads_set_tile(dcl_id, _T("Discret"), _T("1"));
         ads_mode_tile(dcl_id, _T("Continuos"), MODE_DISABLE);
		}
		else
		{
         ads_mode_tile(dcl_id, _T("Discret"), MODE_ENABLE);
         ads_mode_tile(dcl_id, _T("Continuos"), MODE_ENABLE);
			if (CommonStruct4Leg.type_interv == 0)
            ads_set_tile(dcl_id, _T("Discret"), _T("1"));
         else
            ads_set_tile(dcl_id, _T("Continuos"), _T("1"));
		}

   	// in funzione del tipo di attributo scelto setto alcuni tile
		if ((pConn->IsCurrency(CommonStruct4Leg.attrib_type.get_name()) == GS_BAD) &&
  			 (pConn->IsNumeric(CommonStruct4Leg.attrib_type.get_name()) == GS_GOOD))
		{
			// se costruzione intervallo automatica
			if (CommonStruct4Leg.int_build == 0)
			{
				ads_set_tile(dcl_id, _T("Auto"), _T("1"));
				ads_set_tile(dcl_id, _T("Manual"), _T("0"));
            ads_mode_tile(dcl_id, _T("SelVal"), MODE_ENABLE);
			}
			else  // se costruzione intervallo manuale
			{
				ads_set_tile(dcl_id, _T("Manual"), _T("1"));
				ads_set_tile(dcl_id, _T("Auto"), _T("0"));
            ads_mode_tile(dcl_id, _T("SelVal"), MODE_DISABLE);
			}
		}
		else
		{
         ads_set_tile(dcl_id, _T("Manual"), _T("1"));
       	ads_mode_tile(dcl_id, _T("Auto"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Manual"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Define"), MODE_ENABLE);
         ads_mode_tile(dcl_id, _T("SelVal"), MODE_DISABLE);
		}
		// aggiungere test se il tipo è bool tipologia intervalli solo discreta
		
		if (gsc_strcmp(CommonStruct4Leg.attrib_name.get_name(), GS_EMPTYSTR) != 0)
      {
         if ((ptr = pCls->ptr_attrib_list()->search_name(CommonStruct4Leg.attrib_name.get_name(), FALSE)) == NULL)
            { ret = GS_BAD; break; }
  			if ((pos = pCls->ptr_attrib_list()->getpos(ptr)) == 0) { ret = GS_BAD; break; }

         ads_set_tile(dcl_id, _T("Listattrib"), gsc_tostring(pos));
         ads_mode_tile(dcl_id, _T("Listattrib"), MODE_DISABLE);
      }
		// se ho scelto un tematismo abilito il tile di definizione intervalli
		if (gsc_strcmp(CommonStruct4Leg.thm.get_name(), GS_EMPTYSTR) == 0)
			ads_mode_tile(dcl_id, _T("Define"), MODE_DISABLE);
		else
      {
         if ((ptr = CommonStruct4Leg.thm_list.search_name
                    (CommonStruct4Leg.thm.get_name())) == NULL) { ret = GS_BAD; break; }
  			if ((pos = CommonStruct4Leg.thm_list.getpos(ptr)) == 0) { ret = GS_BAD; break; }

         ads_mode_tile(dcl_id, _T("Define"), MODE_ENABLE);
         ads_set_tile(dcl_id, _T("SelThm"), gsc_tostring(pos));
      }

      if (CommonStruct4Leg.range_values.GetCount() > 0)
			ads_mode_tile(dcl_id, _T("Save"), MODE_ENABLE);
		else ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);

		ads_mode_tile(dcl_id, _T("Load"), MODE_ENABLE);

		if (CommonStruct4Leg.sub == 0)
		{
			str = gsc_msg(230);									 //   "Classe :"
			str += pCls->get_name();	                   
		   ads_set_tile(dcl_id, _T("Class_sel"), str.get_name());
			str.clear();
			ads_set_tile(dcl_id, _T("Subclass_sel"), str.get_name());					
	   }
		else
		{
			C_CLASS *madre;
				  
			// Ricavo il puntatore alla classe madre
			if ((madre = gsc_find_class(CommonStruct4Leg.prj,
				                         CommonStruct4Leg.cls, 0)) == NULL) {ret = GS_BAD; break; }
				
			str = gsc_msg(230);									 //   "Classe :"
			str += madre->get_name();	                   
		   ads_set_tile(dcl_id, _T("Class_sel"), str.get_name());
			str = gsc_msg(837);									 //   "Sottoclasse :"
			str += pCls->get_name();
			ads_set_tile(dcl_id, _T("Subclass_sel"), str.get_name());
		}

      ads_action_tile(dcl_id, _T("Location"), (CLIENTFUNC) dcl_SpatialSel);

      ads_action_tile(dcl_id, _T("LocationUse"), (CLIENTFUNC) dcl_main_filter_LocationUse);
      ads_client_data_tile(dcl_id, _T("LocationUse"), &CommonStruct);

      ads_action_tile(dcl_id, _T("Logical_op"), (CLIENTFUNC) dcl_main_filter_Logical_op);
      ads_client_data_tile(dcl_id, _T("Logical_op"), &CommonStruct);

      ads_action_tile(dcl_id, _T("SSFilterUse"), (CLIENTFUNC) dcl_main_filter_LastFilterResultUse);
      ads_client_data_tile(dcl_id, _T("SSFilterUse"), &CommonStruct);

	   ads_action_tile(dcl_id, _T("Listattrib"), (CLIENTFUNC) dcl_main_filt4leg_Listattrib);
		ads_client_data_tile(dcl_id, _T("Listattrib"), &CommonStruct4Leg);

	   ads_action_tile(dcl_id, _T("SelThm"), (CLIENTFUNC) dcl_main_filt4leg_SelThm);
		ads_client_data_tile(dcl_id, _T("SelThm"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Define"), (CLIENTFUNC) dcl_main_filt4leg_Define);
      ads_client_data_tile(dcl_id, _T("Define"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Discret"), (CLIENTFUNC) dcl_main_filt4leg_Discret);
      ads_client_data_tile(dcl_id, _T("Discret"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Continuos"), (CLIENTFUNC) dcl_main_filt4leg_Continuos);
      ads_client_data_tile(dcl_id, _T("Continuos"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Auto"), (CLIENTFUNC) dcl_main_filt4leg_Auto);
      ads_client_data_tile(dcl_id, _T("Auto"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Manual"), (CLIENTFUNC) dcl_main_filt4leg_Manual);
      ads_client_data_tile(dcl_id, _T("Manual"), &CommonStruct4Leg);

      str = gsc_msg(824);									 //   "N.Int: "
   	str += CommonStruct4Leg.n_interv;	                   
   	ads_set_tile(dcl_id, _T("N_Int"), str.get_name());

      ads_action_tile(dcl_id, _T("SelVal"), (CLIENTFUNC) dcl_main_filt4leg_SelVal);
      ads_client_data_tile(dcl_id, _T("SelVal"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("add_object2saveset"), (CLIENTFUNC) dcl_main_filt4leg_add_object2saveset);
      ads_client_data_tile(dcl_id, _T("add_object2saveset"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Load"), (CLIENTFUNC) dcl_main_filt4leg_Load);
      ads_client_data_tile(dcl_id, _T("Load"), &CommonStruct4Leg);

      ads_action_tile(dcl_id, _T("Save"), (CLIENTFUNC) dcl_main_filt4leg_Save);
      ads_client_data_tile(dcl_id, _T("Save"), &CommonStruct4Leg);

      // se esistono condizioni impostate attivo il tile OK
      if (CommonStruct4Leg.range_values.GetCount() > 0)
		   ads_mode_tile(dcl_id, _T("accept"), MODE_ENABLE);
      else
		   ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE);

      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_main_filt4leg_Help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK) break;                            // uscita con tasto OK
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL
      switch (status)
      {
         case LOCATION_SEL:  // definizione di area di selezione
         {
            if (gsc_ddSelectArea(CommonStruct.SpatialSel) == GS_GOOD)
            {
               C_STRING  joinOp, Boundary, SelType;
               C_RB_LIST CoordList;

			      if (gsc_SplitSpatialSel(CommonStruct.SpatialSel, joinOp, &CommonStruct4Leg.Inverse,
                                       CommonStruct4Leg.Boundary, 
					                        CommonStruct4Leg.SelType, CommonStruct4Leg.CoordList) == GS_BAD)
				   	{ ret = GS_BAD; break; }
               else
                  if (CommonStruct4Leg.Boundary.comp(ALL_SPATIAL_COND, FALSE) != 0) UseLS = true; // se Tutto
            }

            // se ho già impostato dei range di valori abilito il tasto di salvataggio query
            if (CommonStruct4Leg.range_values.GetCount() > 0)
					ads_mode_tile(dcl_id, _T("Save"), MODE_ENABLE);
				else
					ads_mode_tile(dcl_id, _T("Save"), MODE_DISABLE);
				break;
         }
         case RANGE_SEL:     // impostazione dei range per l'attributo scelto
         {
            if (gsc_ddSelectRange(&CommonStruct4Leg) == GS_BAD) ret = GS_BAD;
				break;
         }
         case LOAD_SEL:      // caricamento dei valori impostati per il filtro e al legenda
         {
            if (gsc_ddLoadParam(&CommonStruct4Leg, dcl_id,
  					                 &CommonStruct.SpatialSel) == GS_BAD) ret = GS_BAD;
				break;
         }
	   }
      if (ret != GS_GOOD) break;
   }
	ads_done_dialog(dcl_id, status);
   ads_unload_dialog(dcl_file);

   if (ret != GS_GOOD)
      if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      else return GEOsimAppl::CMDLIST.CancelCmd();

   // ricavo il LINK_SET da usare per il filtro
   ret = gsc_get_LS_for_filter(pCls, CommonStruct.LocationUse,
                               CommonStruct.SpatialSel,
                               CommonStruct.LastFilterResultUse, 
                               CommonStruct.Logical_op, 
                               ResultLS);

   // se c'è qualcosa che non va
   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // se devono essere considerate tutte le entità della classe che sono state estratte
   if (ret == GS_CAN) UseLS = false;
   else UseLS = true;

	// lancio la funzione che esegue il filtro multiplo in funzione delle condizioni
	// impostate nei vari intevalli
	if (gsc_filt4thm(&CommonStruct4Leg, &CommonStruct, (UseLS) ? &ResultLS : NULL) != GS_GOOD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   if (gsc_build_legend(&CommonStruct4Leg) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd(); 
 
   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   else if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
   else return GEOsimAppl::CMDLIST.EndCmd();
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Listattrib"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Listattrib(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg = dcl->dialog;
	Common_Dcl_legend_Struct *CommonStruct;
	TCHAR      val[10];
	int        prj, cls, sub;
	C_CLASS    *pCls;
	C_STR_LIST attrib_list_name, attrib_list_type;
	C_STR      *attrib_name, *attrib_type;
	C_ATTRIB   *pAttrib;
   C_STRING   str;
	C_DBCONNECTION *pConn;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	prj = CommonStruct->prj;
	cls = CommonStruct->cls;
	sub = CommonStruct->sub;
	wcscpy(val, _T("0"));

   // Ricavo il puntatore alla classe scelta 
   if ((pCls = gsc_find_class(prj,cls,sub)) == NULL) return;
	pConn = pCls->ptr_info()->getDBConnection(OLD);

	// carico lista attributi della classe scelta
	pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
	while (pAttrib)
	{
      if ((attrib_name = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return; }
      if ((attrib_type = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return; }
		attrib_name->set_name(pAttrib->get_name());
		attrib_type->set_name(pAttrib->type);
      attrib_list_name.add_tail(attrib_name);
      attrib_list_type.add_tail(attrib_type);
		pAttrib = (C_ATTRIB *) pAttrib->get_next();
	}

  	ads_get_tile(hdlg, _T("Listattrib"), val, 10);

	if (_wtoi(val) > 0)
	{
		CommonStruct->attrib_name = ((C_NODE *)attrib_list_name.goto_pos(_wtoi(val)))->get_name();
		CommonStruct->attrib_type = ((C_NODE *)attrib_list_type.goto_pos(_wtoi(val)))->get_name();
    	ads_mode_tile(hdlg, _T("Listattrib"), MODE_DISABLE);
	}
	else
	{
		CommonStruct->attrib_name = GS_EMPTYSTR;
		CommonStruct->attrib_type = GS_EMPTYSTR;
	}
   // se l'attributo è di tipo booleano inibisco il tile 'continuo'
	if (pConn->IsBoolean(CommonStruct->attrib_type.get_name()) == GS_GOOD)
	{
	   ads_set_tile(hdlg, _T("Discret"), _T("1"));
      ads_mode_tile(hdlg, _T("Continuos"), MODE_DISABLE);
	}

   if (CommonStruct->attrib_name.len() > 0)
	{
		if (CommonStruct->thm.len() > 0) 
      { 
         if (CommonStruct->int_build == 1) ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
			else ads_mode_tile(hdlg, _T("Define"), MODE_DISABLE);

			// se l'attributo è di tipo numerico abilito la selezione valori automatica
			if (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD)
			   ads_mode_tile(hdlg, _T("SelVal"), MODE_ENABLE);
      }
	}
	else
	{
		ads_mode_tile(hdlg, _T("Define"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("SelVal"), MODE_DISABLE);
	}
} 
///////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "SelThm" //        
///////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_SelThm(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg = dcl->dialog;
	Common_Dcl_legend_Struct *CommonStruct;
	TCHAR      val[8];
   int        posiz, passo;
   C_STRING   str;
   C_DBCONNECTION *pConn;
   C_CLASS    *pCls;

	wcscpy(val, _T("0"));
   CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   

  	ads_get_tile(hdlg, _T("SelThm"), val, 8);
	if ((posiz = _wtoi(val)) > 0)
	{
		CommonStruct->thm = ((C_NODE *)CommonStruct->thm_list.goto_pos(posiz))->get_name();
		// ricavo il flag del tematismo
		if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(633)) == 0)      // piano
			CommonStruct->FlagThm = GSLayerSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(826)) == 0) // blocco
			CommonStruct->FlagThm = GSBlockNameSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(632)) == 0) // stile testo
			CommonStruct->FlagThm = GSTextStyleSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(825)) == 0) // scala
			CommonStruct->FlagThm = GSBlockScaleSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(631)) == 0) // tipo linea
			CommonStruct->FlagThm = GSLineTypeSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(483)) == 0) // larghezza
			CommonStruct->FlagThm = GSWidthSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(629)) == 0) // colore
			CommonStruct->FlagThm = GSColorSetting;
	   else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(718)) == 0) // altezza testo
			CommonStruct->FlagThm = GSTextHeightSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(630)) == 0) // riempimento
			CommonStruct->FlagThm = GSHatchNameSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(229)) == 0) // Colore riempimento
			CommonStruct->FlagThm = GSHatchColorSetting;
		else if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) == 0) // tutte le proprietà
			CommonStruct->FlagThm = CommonStruct->FlagFas;
      
      // ricavo il numero di intervalli eventualmente impostati
      if (CommonStruct->FlagThm != CommonStruct->FlagFas) passo = 14;
      else passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
      CommonStruct->n_interv = (((CommonStruct->thm_sel[posiz - 1]).GetCount()) - 2) / passo;
      str = gsc_msg(824);									 //   "N.Int: "
 	   str += CommonStruct->n_interv;	                   
 	   ads_set_tile(hdlg, _T("N_Int"), str.get_name());

		if ((CommonStruct->thm.len() > 0) && (CommonStruct->attrib_name.len() > 0))
      {
         if (CommonStruct->int_build == 0) 
            ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
			else
            ads_mode_tile(hdlg, _T("Define"), MODE_DISABLE);

			// se l'attributo è di tipo numerico abilito alcuni tile 
			pConn = pCls->ptr_info()->getDBConnection(OLD);
			if (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD)
         {
            ads_set_tile(hdlg,  _T("Auto"), _T("1"));
       	   ads_mode_tile(hdlg, _T("Auto"), MODE_ENABLE);
			   ads_mode_tile(hdlg, _T("Manual"), MODE_ENABLE);
				ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
			   ads_mode_tile(hdlg, _T("SelVal"), MODE_ENABLE);
         }
         else
         {
        	   CommonStruct->int_build = 0;
            ads_set_tile(hdlg, _T("Manual"), _T("1"));
       	   ads_mode_tile(hdlg, _T("Auto"), MODE_DISABLE);
			   ads_mode_tile(hdlg, _T("Manual"), MODE_DISABLE);
				ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
         }
      }
		else
      { 
         ads_mode_tile(hdlg, _T("Define"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("SelVal"), MODE_DISABLE);
      }
	}
} 
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Discret"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Discret(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	CommonStruct->type_interv = 0;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Continuos"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Continuos(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	CommonStruct->type_interv = 1;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Manual"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Manual(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   ads_hdlg hdlg = dcl->dialog;
   C_STRING str;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	CommonStruct->int_build = 0;
   str = gsc_msg(824);									 //   "N.Int: "
 	str += CommonStruct->n_interv;	                   
 	ads_set_tile(hdlg, _T("N_Int"), str.get_name());
	ads_mode_tile(hdlg, _T("SelVal"), MODE_DISABLE);
	ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Auto"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Auto(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   ads_hdlg hdlg = dcl->dialog;
	C_DBCONNECTION *pConn;
	C_CLASS *pCls;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   
	pConn = pCls->ptr_info()->getDBConnection(OLD);

   if ((CommonStruct->thm.len() > 0) && 
       (CommonStruct->attrib_name.len() > 0) &&
       (pConn->IsCurrency(CommonStruct->attrib_type.get_name()) == GS_BAD) &&
       (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD))
   {
   	CommonStruct->int_build = 1;
   	ads_mode_tile(hdlg, _T("SelVal"), MODE_ENABLE); 
      if (CommonStruct->n_interv > 0)
         ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE); 
      else
         ads_mode_tile(hdlg, _T("Define"), MODE_DISABLE);
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "SelVal"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_SelVal(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   C_STR_LIST values;
	C_NODE     *ptr = NULL; 
   C_CLASS    *pCls = NULL;
   C_STRING   str;
	int        pos_thm = 0;
   
	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	values.remove_all();
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return; 

	if (gsc_scel_multiple_value(CommonStruct->prj, CommonStruct->cls,
				                   CommonStruct->sub, CommonStruct->attrib_name,
							          &values) == GS_BAD) return;
	// in funzione dei valori impostati inizializzo i ranges dei tematismi
	if (values.get_count() > 0)
	{	
		CommonStruct->n_interv = values.get_count();

		if (CommonStruct->thm.len() > 0)
   	{
	  		if((ptr = CommonStruct->thm_list.search_name
							  (CommonStruct->thm.get_name())) == NULL) return; 
			if ((pos_thm = CommonStruct->thm_list.getpos(ptr)) == 0) return; 
		}
      if (gsc_ImpostDefRangeValues(&values, &CommonStruct->thm_list, CommonStruct->thm.get_name(),
			                          CommonStruct->attrib_name.get_name(),
			                          &CommonStruct->thm_sel[pos_thm - 1], pCls) == GS_BAD) return;
      // copio i valori impostati nella lista dei range definitivi
		(CommonStruct->thm_sel[pos_thm - 1]).copy(CommonStruct->range_values);

      str = gsc_msg(824);									 //   "N.Int: "
      str += CommonStruct->n_interv;	                   
      ads_set_tile(dcl->dialog, _T("N_Int"), str.get_name());
		ads_mode_tile(dcl->dialog, _T("Define"), MODE_ENABLE); 
	}
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "add_object2saveset"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_add_object2saveset(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;  
   TCHAR val[10];
   
	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);

   if (ads_get_tile(dcl->dialog, _T("add_object2saveset"), val, 10) != RTNORM) return;

   if (gsc_strcmp(val, _T("1")) == 0)
      CommonStruct->AddObjs2SaveSet = TRUE;
   else
      CommonStruct->AddObjs2SaveSet = FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Define"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Define(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, RANGE_SEL);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Load"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Load(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, LOAD_SEL);         
}
/*****************************************************************************/
/*.doc gsc_ddLoadParam                                                       */
/*+
  Questa funzione carica i valori impostati per la legenda da un file a scelta 
  dell'utente.
  Parametri:
  Common_Dcl_legend_Struct &CommonStruct : struttura dei dati interessati;
  ads_hdlg hdlg : identificativo della dcl attiva;
  C_STRING *SelSpace : selezione spaziole effettuata.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddLoadParam(Common_Dcl_legend_Struct *CommonStruct, ads_hdlg hdlg, C_STRING *SelSpace) 
{
   Common_Dcl_SpatialSel_Struct CommonSpatialStruct;
   C_STRING  FileName, ThmDir, riga, str, LastThmQryFile;
	C_RB_LIST temp_list;
	C_CLASS   *pCls;
	C_DBCONNECTION *pConn;
	ads_point point;
	TCHAR     *pnt = NULL, *rigaletta = NULL;
   double    val1 = 0.0, val2 = 0.0;
	int       ret = GS_GOOD, num_thm, diff;

   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
   ads_point_clear(point);

   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return GS_BAD;   

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastThmQryFile"), LastThmQryFile) == GS_BAD ||
       gsc_dir_exist(LastThmQryFile) == GS_BAD)
   {
      // imposto il direttorio di ricerca del file
      ThmDir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      ThmDir += _T("\\");
      ThmDir += GEOQRYDIR;
   }
   else
      if (gsc_dir_from_path(LastThmQryFile, ThmDir) == GS_BAD) return GS_BAD;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), ThmDir, _T("leg"), UI_LOADFILE_FLAGS, FileName) == RTNORM)
   {
      FILE       *file = NULL;
      C_STR_LIST SpatialSelList;
      C_STRING   SpatialSel, JoinOp, Boundary, SelType;
      C_RB_LIST  CoordList;
      bool       Inverse;

		// apro il file di tematismo in lettura NON Unicode (i file .LEG sono ancora in ANSI)
      bool IsUnicode = false;
		if ((file = gsc_fopen(FileName, _T("r"), MORETESTS, &IsUnicode)) == NULL)
		{
			ads_set_tile(hdlg, _T("error"), gsc_err(eGSOpenFile)); // "*Errore* Apertura file fallita"
			return GS_BAD;
		}

      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastThmQryFile"), FileName);

		// leggo le impostazioni generali
		do
		{
			TCHAR *punt;

			// leggo l'attributo selezionato per la classe scelta e la sua tipologia
			if (gsc_readline(file, &rigaletta) == WEOF)
			   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }

			riga.set_name(rigaletta);
			if ((punt = gsc_strstr(riga.get_name(), _T(","))) == NULL) 
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			diff = (int) (punt - riga.get_name() - 1);
			CommonStruct->attrib_name.set_name(riga.get_name(), 0, diff);

			if ((punt = gsc_strstr(riga.get_name(), _T(","))) == NULL) 
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			CommonStruct->attrib_type.set_name(punt + 1);

			// verifico la correttezza del nome dell'attributo scelto
			C_CLASS *pCls;
			if ((pCls = gsc_find_class(CommonStruct->prj, 
												CommonStruct->cls, CommonStruct->sub)) == NULL)
				{ GS_ERR_COD = eGSClassNotFound; ret = GS_BAD; break; } // classe non trovata

			C_ATTRIB_LIST *pAttrib;
			if ((pAttrib = pCls->ptr_attrib_list()) == NULL)
				{ GS_ERR_COD = eGSInvClassType; ret = GS_BAD; break; }     // nome attributo non valido

			C_NODE *ptr;
	  		if ((ptr = pAttrib->search_name(CommonStruct->attrib_name.get_name())) == NULL)
				{ GS_ERR_COD = eGSInvAttribName; ret = GS_BAD; break; }    // nome attributo non valido

			int pos_attr;
  			if ((pos_attr = pAttrib->getpos(ptr)) == 0)
				{ GS_ERR_COD = eGSInvAttribName; ret = GS_BAD; break; }    // nome attributo non valido
	
         free(rigaletta); rigaletta = NULL;
			// leggo il valore del flagfas e il numero di tematismi scelti fra i possibili
			int flagfas;

			if (gsc_readline(file, &rigaletta) == WEOF)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			swscanf(rigaletta, _T("%d,%d"), &flagfas, &num_thm);

         free(rigaletta); rigaletta = NULL;
			// leggo il tematismo scelto e verifico se è presente nella lista tematismi della
			// classe selezionata
			C_STRING thm;

			if (gsc_readline(file, &rigaletta) == WEOF)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			thm.set_name(rigaletta);
			if ((punt = gsc_strstr(thm.get_name(), _T(","))) == NULL)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			diff = (int) (punt - thm.get_name() - 1);
			CommonStruct->FlagThm = _wtoi(punt + 1);
			thm.set_name(thm.get_name(), 0, diff);

			if (CommonStruct->thm_list.search_name(thm.get_name()) == NULL)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			CommonStruct->thm.set_name(thm.get_name());
      } while(0);

		// constato se si è verificato un errore
		if (ret == GS_BAD)
		{ 
			gsc_fclose(file); 
			free(rigaletta);
			ads_set_tile(hdlg, _T("error"), gsc_err(GS_ERR_COD)); 
			return ret;
		}

      // leggo le impostazioni spaziali            
      do
      {
         if (gsc_SpatialSel_Load(file, SpatialSelList) == GS_BAD ||
             SpatialSelList.get_count() == 0) break;

         // scompongo la condizione impostata
         SpatialSel = ((C_STR *) SpatialSelList.get_head())->get_name();
         if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &Inverse, 
                                 Boundary, SelType, CoordList) == GS_BAD)
            { GS_ERR_COD = eGSBadLocationQry; break; } // "*Errore* Condizione spaziale non valida"

         // imposto i nuovi valori
			SelSpace->set_name(SpatialSel.get_name());
         CommonStruct->Boundary = Boundary;
         CommonStruct->SelType  = SelType;
         CommonStruct->CoordList << CoordList.get_head();
         CommonStruct->Inverse  = Inverse;
         CoordList.ReleaseAllAtDistruction(GS_BAD);
      } while (0);

		// constato se si è verificato un errore
		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			ads_set_tile(hdlg, _T("error"), gsc_err(GS_ERR_COD)); 
			return ret;
		}

		do
		{
         free(rigaletta); rigaletta = NULL;
			// lettura riga a vuoto che corrisponde alla lettura delle cond.spaziali 
         // già effettuata con gsc_SpatialSel_Load (che usa la funzione fread())
			if (gsc_readline(file, &rigaletta) == WEOF) { GS_ERR_COD = eGSReadFile; break; }
         free(rigaletta); rigaletta = NULL;
			// leggo il numero di intervalli impostati e la tipologia di intervallo (discreto/continuo)
			if (gsc_readline(file, &rigaletta) == WEOF) { GS_ERR_COD = eGSReadFile; break; }
			swscanf(rigaletta, _T("%d,%d"), &CommonStruct->n_interv, &CommonStruct->type_interv);
  
			// leggo i ranges di valori impostati
			temp_list << acutBuildList(RTLB, 0);                              // parentesi globale
			for (int i = 1; i <= CommonStruct->n_interv; i++)
			{
				temp_list += acutBuildList(RTLB, 0);									// parentesi intervallo
				if (num_thm > 1) temp_list += acutBuildList(RTLB, 0);				// parentesi gruppo tematismi
				// leggo i valori del tematismo 
				for (int j = 1; j <= num_thm; j++)
				{
					riga.clear();
               free(rigaletta); rigaletta = NULL;
               if (gsc_readline(file, &rigaletta) == WEOF)
					   { GS_ERR_COD = eGSReadFile; ret = GS_BAD;	j = num_thm; continue; }
					riga.set_name(rigaletta);
					// testo il valore letto 
					if (gsc_strstr(riga.get_name(), gsc_msg(629)) != NULL) // colore
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(629)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(629), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(633)) != NULL) // piano
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(633)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(633),
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(631)) != NULL) // tipolinea
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(631)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(631), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(632)) != NULL) // stile testo
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(632)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(632), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(826)) != NULL) // blocco
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(826)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(826), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(630)) != NULL) // riempimento
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(630)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(630), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(229)) != NULL) // Colore riempimento
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(229)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(229), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(825)) != NULL) // scala
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(825)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(825), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(483)) != NULL) // larghezza
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(483)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(483), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(718)) != NULL) // altezza testo
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(718)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(718), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
				}
				if (num_thm > 1) temp_list += acutBuildList(RTLE, 0);				// parentesi gruppo tematismi

				if (ret == GS_BAD) break;

	         free(rigaletta); rigaletta = NULL;
            if (gsc_readline(file, &rigaletta) == WEOF)
				   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; i = CommonStruct->n_interv; continue; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), 
											 CommonStruct->attrib_name.get_name()) != NULL) // 'nome attributo'
				{
					// se il tematismo letto fa parte della lista tematismi della classe 
					// selezionata l'aggiungo nella lista intervalli
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
					{
						pConn = pCls->ptr_info()->getDBConnection(OLD);
						if (pConn->IsCurrency(CommonStruct->attrib_type.get_name()) == GS_GOOD)
							temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																RTSTR, (pnt + 1), 0);
						else if (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD)
						   	  temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																     RTREAL, _wtof(pnt + 1), 0);
							  else 
								  temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																	  RTSTR, (pnt + 1), 0);
					}
					temp_list += acutBuildList(RTLE, 0);
				}

	         free(rigaletta); rigaletta = NULL;
				if (gsc_readline(file, &rigaletta) == WEOF)
				   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), gsc_msg(836)) != NULL) 
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(836),	RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
				}
				temp_list += acutBuildList(RTLE, 0);                 // parentesi intervallo
			}

			if (ret == GS_BAD) break;
			temp_list += acutBuildList(RTLE, 0);						  // parentesi globale

			// aggiorno la struttura comune dei dati
			temp_list.copy(CommonStruct->range_values);
			C_NODE *ptr;
			int pos_thm;
	  		if((ptr = CommonStruct->thm_list.search_name(CommonStruct->thm.get_name())) == NULL)
				{ ret = GS_BAD; break; }
			if ((pos_thm = CommonStruct->thm_list.getpos(ptr)) == 0)
				{ ret = GS_BAD; break; } // mess errore ?
			temp_list.copy(CommonStruct->thm_sel[pos_thm - 1]);
		} while (0);

		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			ads_set_tile(hdlg, _T("error"), gsc_err(GS_ERR_COD)); 
			return ret;
		}

		// leggo le impostazioni della legenda
		do
		{
			temp_list << acutBuildList(RTLB, 0);                       // parentesi globale
			for (int i = 1; i <= 9; i++)
			{
				riga.clear();
	         free(rigaletta); rigaletta = NULL;
            if (gsc_readline(file, &rigaletta) == WEOF) // non ci sono impostazioni per la legenda
				   { i = 9; continue; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), gsc_msg(821)) != NULL)  // Impostazione legenda
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(821),	RTSHORT, _wtoi(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					CommonStruct->FlagLeg = _wtoi(pnt + 1);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(633)) != NULL)  // piano
				{																		  
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(633), RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(814)) != NULL)  // Punto inserimento
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(814), 0);
					point[X] = _wtof(pnt + 2);
					riga.set_name(pnt + 2);
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL) point[Y] = _wtof(pnt + 1);
					temp_list += acutBuildList(RTPOINT, point, 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(815)) != NULL)  // DimX simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(815),	RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(816)) != NULL)  // DimY simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(816), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(817)) != NULL)  // Offset simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(817), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(818)) != NULL)  // Dim. etichette
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(818), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(632)) != NULL)  // Stile testo
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(632), RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(819)) != NULL)  // Offset etichette
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(819),	RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
			}

			if (ret == GS_BAD) break;
			temp_list += acutBuildList(RTLE, 0);						  // parentesi globale

			// aggiorno la struttura comune dei dati
			if (temp_list.GetCount() <= 2) temp_list.remove_all();
			temp_list.copy(CommonStruct->Legend_param);
			
		} while (0);

		if ((point[X] == 0.0) && (point[Y] == 0.0)) CommonStruct->ins_leg = FALSE;
		else CommonStruct->ins_leg = TRUE;
      
		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			ads_set_tile(hdlg, _T("error"), gsc_err(GS_ERR_COD)); 
			return ret;
		}

      if (file)
         if (gsc_fclose(file) == GS_BAD)
            ads_set_tile(hdlg, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }	  
	
	if (rigaletta) free(rigaletta);

	// in funzione del tipo di attributo scelto setto alcuni tile
	pConn = pCls->ptr_info()->getDBConnection(OLD);
	if ((pConn->IsCurrency(CommonStruct->attrib_type.get_name()) == GS_BAD) &&
		 (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD))
	{
      str = gsc_msg(824);									 //   "N.Int: "
      str += CommonStruct->n_interv;	                   
      ads_set_tile(hdlg, _T("N_Int"), str.get_name());

		if (CommonStruct->int_build == 0)
		{
			ads_set_tile(hdlg, _T("Auto"), _T("0"));
			ads_set_tile(hdlg, _T("Manual"), _T("1"));
		}
		else
		{
			ads_set_tile(hdlg, _T("Manual"), _T("0"));
			ads_set_tile(hdlg, _T("Auto"), _T("1"));
		}
		ads_mode_tile(hdlg, _T("SelVal"), MODE_ENABLE);
      ads_mode_tile(hdlg, _T("Auto"),   MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Manual"), MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
   }
   else
   {
      ads_set_tile(hdlg, _T("Manual"), _T("1"));
      ads_mode_tile(hdlg, _T("Auto"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("Manual"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("Define"), MODE_ENABLE);
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gs_themafilterfromfile <external> */
/*+
  Questa funzione LISP lancia la procedura che consente creare un tematismo  
  su una classe GEOSim estratta leggendo le impostazioni da file precedentemente 
  memorizzato.
  I parametri LISP sono:
  <path_file> <cod_prj> <cod_cls> <cod_sub>

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*************************************************************************/
int gs_themafilterfromfile(void)
{
   C_STRING path_file;
   int      cod_cls = 0, cod_sub = 0;
   presbuf  arg;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else path_file = arg->resval.rstring;     // percorso completo file di impostazioni 

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else cod_cls = arg->resval.rint;            // codice classe

   arg = arg->rbnext;
   if (arg == NULL) cod_sub = 0;               // se non presente codice sottoclasse = 0
   else
   {
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      else cod_sub = arg->resval.rint;         // codice sottoclasse
   }
  
   // chiamo la funzione che crea il tematismo
   if (gsc_ThemaFilterFromFile(path_file.get_name(), cod_cls, cod_sub) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_ThemaFilterFromFile                                               */
/*+
  Questa funzione legge le impostazioni del filtro tematico da file.
  Parametri:
  TCHAR *path_file; Percorso completo del file contenente le impostazioni tematiche
  int cls;          Codice classe
  int sub;          Codice sottoclasse
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ThemaFilterFromFile(TCHAR *path_file, int cls, int sub)
{
   int        ret = GS_GOOD, status = DLGOK, pos = 0, prj = 0;
	int        flag_thm = FALSE, pos_thm = 0, num_thm = 0;
	long       FlagFas = 0, fas_value = 0;
   C_CLASS    *pCls;
   C_PROJECT  *pproject;
   C_NODE     *ptr = NULL;
	C_LINK_SET ResultLS;
   bool       UseLS;
   C_STRING   path, str;  
	C_STR      *thm;
	C_STR_LIST thm_list;
   Common_Dcl_main_filter_Struct CommonStruct;
	Common_Dcl_sql_bldqry_Struct CommonStructClassSQL;
	Common_Dcl_legend_Struct CommonStruct4Leg;
	Common_Dcl_SpatialSel_Struct CommonSpatialStruct;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }   
   pproject = GS_CURRENT_WRK_SESSION->get_pPrj();
   prj = pproject->get_key();

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GS_BAD;

   // ricavo il puntatore alla classe su cui impostare il filtro multiplo
	if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;
   if (!pCls->ptr_attrib_list() || !pCls->ptr_info()) 
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // verifico che sia presente fra quelle estratte dall'utente nella sessione di lavoro   
   if (!GS_CURRENT_WRK_SESSION->get_pClsCodeList()->search_key(cls))
   {
      acutPrintf(gsc_err(eGSNotClassExtr)); //"*Errore* Classe non estratta nella sessione di lavoro attiva"
      return GS_GOOD;
   }

	// verifico le caratteristiche grafiche della classe selezionata
   FlagFas = pCls->what_is_graph_updateable();

	CommonStruct4Leg.FlagThm = 0;
	// in funzione delle caratteristiche trovate creo la lista delle possibili scelte
	if (FlagFas > 0)
	{
		if (FlagFas & GSLayerSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(633));					   	//piano 
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSLayerSetting;
		}     
		if (FlagFas & GSBlockNameSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(826));			    			//blocco 
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSBlockNameSetting;
		}
		if (FlagFas & GSTextStyleSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(632));			    			//stile testo
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + 4;
		}	 
		if (FlagFas & GSBlockScaleSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(825));			    			//scala
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSBlockScaleSetting;
		}	 
		if (FlagFas & GSLineTypeSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(631));			    			//tipolinea
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSLineTypeSetting;
		}	
		if (FlagFas & GSWidthSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(483));			    			// larghezza
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSWidthSetting;
		}	 
		if (FlagFas & GSColorSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(629));			    			//	colore
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSColorSetting;
		}	 
		if (FlagFas & GSTextHeightSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(718));			    			//altezza testo
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSTextHeightSetting;
		}  
		if (FlagFas & GSHatchNameSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(630));			    			//riempimento
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSHatchNameSetting;
		}  
		if (FlagFas & GSHatchColorSetting)
		{
			if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			thm->set_name(gsc_msg(229));			    			//	Colore riempimento
			CommonStruct4Leg.thm_list.add_tail(thm);
			CommonStruct4Leg.FlagThm = CommonStruct4Leg.FlagThm + GSHatchColorSetting;
		}	 
	}
	if ((thm = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
	thm->set_name(gsc_msg(833));			    			//tutte le proprietà
	CommonStruct4Leg.thm_list.add_tail(thm);
   num_thm = CommonStruct4Leg.thm_list.get_count();
   // ordino per nome la lista dei tematismi
   CommonStruct4Leg.thm_list.sort_name(TRUE, TRUE); // sensitive e ascending

   // inizializzo le CommonStruct che servono per passare i dati fra DCL
	CommonStruct.SpatialSel   = _T("\"\" \"\" \"\" \"Location\" (\"all\") \"\"");
   CommonStruct4Leg.Boundary = ALL_SPATIAL_COND; // tutto
   CommonStruct.LocationUse  = true;
   CommonStruct.LastFilterResultUse = false;
   CommonStruct.Logical_op  = UNION;
   CommonStruct.Tools       = GSHighlightFilterSensorTool;
   CommonStruct.ClsSQLSel   = 0;
   CommonStruct.SecSQLSel   = 0;

	CommonStruct4Leg.prj = prj;
	CommonStruct4Leg.cls	= cls;
	CommonStruct4Leg.sub = sub;
	CommonStruct4Leg.type_interv = 1;
	CommonStruct4Leg.int_build = 1;
	CommonStruct4Leg.n_interv = 0;
	CommonStruct4Leg.current_line = 0;
	CommonStruct4Leg.FlagFas = CommonStruct4Leg.FlagThm;
	CommonStruct4Leg.FlagLeg = 0;
	CommonStruct4Leg.attrib_name = GS_EMPTYSTR;			
	CommonStruct4Leg.attrib_type = GS_EMPTYSTR;
   CommonStruct4Leg.thm         = GS_EMPTYSTR;
   CommonStruct4Leg.ins_leg = FALSE;

   if (CommonStruct.sql_list.initialize(pCls) != GS_GOOD) return GS_BAD;

   // caricamento dei valori impostati per il filtro e la legenda
   if (gsc_LoadParamFromFile(&CommonStruct4Leg, path_file,
                             &CommonStruct.SpatialSel) == GS_BAD) return GS_BAD;

   // ricavo il LINK_SET da usare per il filtro
   ret = gsc_get_LS_for_filter(pCls, CommonStruct.LocationUse,
                               CommonStruct.SpatialSel,
                               CommonStruct.LastFilterResultUse, 
                               CommonStruct.Logical_op, 
                               ResultLS);

   // se c'è qualcosa che non va
   if (ret == GS_BAD) return GS_BAD;

   // se devono essere considerate tutte le entità della classe che sono state estratte
   if (ret == GS_CAN) UseLS = false;
   else UseLS = true;

	// lancio la funzione che esegue il filtro multiplo in funzione delle condizioni
	// impostate nei vari intevalli
	if (gsc_filt4thm(&CommonStruct4Leg, &CommonStruct, (UseLS) ? &ResultLS : NULL) != GS_GOOD)
      return GS_BAD;

   if (gsc_build_legend(&CommonStruct4Leg) != GS_GOOD) return GS_BAD; 
 
   return ret;
}


/*****************************************************************************/
/*.doc gsc_LoadParamFromFile                                                 */
/*+
  Questa funzione carica i valori impostati per la legenda da un file a scelta 
  dell'utente.
  Parametri:
  Common_Dcl_legend_Struct &CommonStruct : struttura dei dati interessati;
  TCHAR *path                             : nome file da caricare
  C_STRING *SelSpace                     : selezione spaziale effettuata.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_LoadParamFromFile(Common_Dcl_legend_Struct *CommonStruct, 
                          TCHAR *path, C_STRING *SelSpace) 
{
   Common_Dcl_SpatialSel_Struct CommonSpatialStruct;
   C_STRING  FileName, riga, str, LastThmQryFile;
	C_RB_LIST temp_list;
	C_CLASS   *pCls;
	C_DBCONNECTION *pConn;
	ads_point point;
	TCHAR     *pnt = NULL, *rigaletta = NULL;
   double    val1 = 0.0, val2 = 0.0;
	int       ret = GS_GOOD, num_thm, diff;

   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return GS_BAD;   

   FileName.set_name(path);

   // Se esiste il file
   if (gsc_path_exist(FileName) == GS_GOOD)
   {
      FILE       *file = NULL;
      C_STR_LIST SpatialSelList;
      C_STRING   JoinOp, Boundary, SelType;
      C_RB_LIST  CoordList;
      bool       Inverse;

		// apro il file di tematismo in lettura
		if ((file = gsc_fopen(FileName.get_name(), _T("r"))) == NULL)
		{
			acutPrintf(gsc_err(eGSOpenFile)); // "*Errore* Apertura file fallita"
			return GS_BAD;
		}

		// leggo le impostazioni generali
		do
		{
			TCHAR *punt;
			// leggo l'attributo selezionato per la classe scelta e la sua tipologia
			if (gsc_readline(file, &rigaletta) == WEOF)
			   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }

			riga.set_name(rigaletta);
			if ((punt = gsc_strstr(riga.get_name(), _T(","))) == NULL) 
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			diff = (int) (punt - riga.get_name() - 1);
			CommonStruct->attrib_name.set_name(riga.get_name(), 0, diff);

			if ((punt = gsc_strstr(riga.get_name(), _T(","))) == NULL) 
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			CommonStruct->attrib_type.set_name(punt + 1);

			// verifico la correttezza del nome dell'attributo scelto
			C_CLASS *pCls;
			if ((pCls = gsc_find_class(CommonStruct->prj, 
												CommonStruct->cls, CommonStruct->sub)) == NULL)
				{ GS_ERR_COD = eGSClassNotFound; ret = GS_BAD; break; } // classe non trovata

			C_ATTRIB_LIST *pAttrib;
			if ((pAttrib = pCls->ptr_attrib_list()) == NULL)
				{ GS_ERR_COD = eGSInvClassType; ret = GS_BAD; break; }     // nome attributo non valido

			C_NODE *ptr;
	  		if ((ptr = pAttrib->search_name(CommonStruct->attrib_name.get_name())) == NULL)
				{ GS_ERR_COD = eGSInvAttribName; ret = GS_BAD; break; }    // nome attributo non valido

			int pos_attr;
  			if ((pos_attr = pAttrib->getpos(ptr)) == 0)
				{ GS_ERR_COD = eGSInvAttribName; ret = GS_BAD; break; }    // nome attributo non valido
	
         free(rigaletta); rigaletta = NULL;
			// leggo il valore del flagfas e il numero di tematismi scelti fra i possibili
			int flagfas;

			if (gsc_readline(file, &rigaletta) == WEOF)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			swscanf(rigaletta, _T("%d,%d"), &flagfas, &num_thm);

         free(rigaletta); rigaletta = NULL;
			// leggo il tematismo scelto e verifico se è presente nella lista tematismi della
			// classe selezionata
			C_STRING thm;

			if (gsc_readline(file, &rigaletta) == WEOF)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			thm.set_name(rigaletta);
			if ((punt = gsc_strstr(thm.get_name(), _T(","))) == NULL)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			diff = (int) (punt - thm.get_name() - 1);
			CommonStruct->FlagThm = _wtoi(punt + 1);
			thm.set_name(thm.get_name(), 0, diff);

			if (CommonStruct->thm_list.search_name(thm.get_name()) == NULL)
				{ GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }
			CommonStruct->thm.set_name(thm.get_name());
      } while(0);

		// constato se si è verificato un errore
		if (ret == GS_BAD)
		{ 
			gsc_fclose(file); 
			free(rigaletta);
			acutPrintf(gsc_err(GS_ERR_COD)); 
			return ret;
		}

      // leggo le impostazioni spaziali            
      do
      {
         if (gsc_SpatialSel_Load(file, SpatialSelList) == GS_BAD ||
             SpatialSelList.get_count() == 0) break;

         // scompongo la condizione impostata
         if (gsc_SplitSpatialSel((C_STR *) SpatialSelList.get_head(),
                                 JoinOp, &Inverse, Boundary, 
                                 SelType, CoordList) == GS_BAD)
            { GS_ERR_COD = eGSBadLocationQry; break; } // "*Errore* Condizione spaziale non valida"

         // imposto i nuovi valori
			SelSpace->set_name(((C_STR *) SpatialSelList.get_head())->get_name());
         CommonStruct->Boundary = Boundary;
         CommonStruct->SelType  = SelType;
         CommonStruct->Inverse  = Inverse;
         CommonStruct->CoordList << CoordList.get_head();
         CoordList.ReleaseAllAtDistruction(GS_BAD);
      } while (0);

		// constato se si è verificato un errore
		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			acutPrintf(gsc_err(GS_ERR_COD));
			return ret;
		}

		do
		{
         free(rigaletta); rigaletta = NULL;
			// lettura riga a vuoto che corrisponde alla lettura delle cond.spazili già effettuata con
			//	gsc_SpatialSel_Load (che usa la funzionefread())
			if (gsc_readline(file, &rigaletta) == WEOF) { GS_ERR_COD = eGSReadFile; break; }
         free(rigaletta); rigaletta = NULL;
			// leggo il numero di intervalli impostati e la tipologia di intervallo (discreto/continuo)
			if (gsc_readline(file, &rigaletta) == WEOF) { GS_ERR_COD = eGSReadFile; break; }
			swscanf(rigaletta, _T("%d,%d"), &CommonStruct->n_interv, &CommonStruct->type_interv);
  
			// leggo i ranges di valori impostati
			temp_list << acutBuildList(RTLB, 0);                              // parentesi globale
			for (int i = 1; i <= CommonStruct->n_interv; i++)
			{
				temp_list += acutBuildList(RTLB, 0);									// parentesi intervallo
				if (num_thm > 1) temp_list += acutBuildList(RTLB, 0);				// parentesi gruppo tematismi
				// leggo i valori del tematismo 
				for (int j = 1; j <= num_thm; j++)
				{
					riga.clear();
               free(rigaletta); rigaletta = NULL;
               if (gsc_readline(file, &rigaletta) == WEOF)
					   { GS_ERR_COD = eGSReadFile; ret = GS_BAD;	j = num_thm; continue; }
					riga.set_name(rigaletta);
					// testo il valore letto 
					if (gsc_strstr(riga.get_name(), gsc_msg(629)) != NULL) // colore
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(629)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(629), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(633)) != NULL) // piano
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(633)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(633),
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(631)) != NULL) // tipolinea
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(631)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(631), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(632)) != NULL) // stile testo
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(632)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(632), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(826)) != NULL) // blocco
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(826)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(826), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(630)) != NULL) // riempimento
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(630)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(630), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(229)) != NULL) // Colore riempimento
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(229)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(229), 
																	RTSTR, (pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(825)) != NULL) // scala
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(825)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(825), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(483)) != NULL) // larghezza
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(483)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(483), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
					if (gsc_strstr(riga.get_name(), gsc_msg(718)) != NULL) // altezza testo
					{
						// se il tematismo letto fa parte della lista tematismi della classe 
						// selezionata l'aggiungo nella lista intervalli
						if (CommonStruct->thm_list.search_name(gsc_msg(718)) != NULL)
						{
							if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
								temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(718), 
																	RTREAL, _wtof(pnt + 1), 0);
						}
						temp_list += acutBuildList(RTLE, 0);
						continue;
					}
				}
				if (num_thm > 1) temp_list += acutBuildList(RTLE, 0);				// parentesi gruppo tematismi

				if (ret == GS_BAD) break;

	         free(rigaletta); rigaletta = NULL;
            if (gsc_readline(file, &rigaletta) == WEOF)
				   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; i = CommonStruct->n_interv; continue; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), 
									CommonStruct->attrib_name.get_name()) != NULL) // 'nome attributo'
				{
					// se il tematismo letto fa parte della lista tematismi della classe 
					// selezionata l'aggiungo nella lista intervalli
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
					{
						pConn = pCls->ptr_info()->getDBConnection(OLD);
						if (pConn->IsCurrency(CommonStruct->attrib_type.get_name()) == GS_GOOD)
							temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																RTSTR, (pnt + 1), 0);
						else if (pConn->IsNumeric(CommonStruct->attrib_type.get_name()) == GS_GOOD)
						   	  temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																     RTREAL, _wtof(pnt + 1), 0);
							  else 
								  temp_list += acutBuildList(RTLB, RTSTR, CommonStruct->attrib_name.get_name(), 
																	  RTSTR, (pnt + 1), 0);
					}
					temp_list += acutBuildList(RTLE, 0);
				}

	         free(rigaletta); rigaletta = NULL;
				if (gsc_readline(file, &rigaletta) == WEOF)
				   { GS_ERR_COD = eGSReadFile; ret = GS_BAD; break; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), gsc_msg(836)) != NULL) 
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(836),	RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
				}
				temp_list += acutBuildList(RTLE, 0);                 // parentesi intervallo
			}

			if (ret == GS_BAD) break;
			temp_list += acutBuildList(RTLE, 0);						  // parentesi globale

			// aggiorno la struttura comune dei dati
			temp_list.copy(CommonStruct->range_values);
			C_NODE *ptr;
			int pos_thm;
	  		if((ptr = CommonStruct->thm_list.search_name(CommonStruct->thm.get_name())) == NULL)
				{ ret = GS_BAD; break; }
			if ((pos_thm = CommonStruct->thm_list.getpos(ptr)) == 0)
				{ ret = GS_BAD; break; } // mess errore ?
			temp_list.copy(CommonStruct->thm_sel[pos_thm - 1]);
		} while (0);

		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			acutPrintf(gsc_err(GS_ERR_COD));
			return ret;
		}

		// leggo le impostazioni della legenda
		do
		{
			temp_list << acutBuildList(RTLB, 0);                       // parentesi globale
			for (int i = 1; i <= 9; i++)
			{
				riga.clear();
	         free(rigaletta); rigaletta = NULL;
            if (gsc_readline(file, &rigaletta) == WEOF)
				   { GS_ERR_COD = eGSReadFile; ret = GS_BAD;	i = 9; continue; }

				riga.set_name(rigaletta);
				if (gsc_strstr(riga.get_name(), gsc_msg(821)) != NULL)  // Impostazione legenda
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(821),	RTSHORT, _wtoi(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					CommonStruct->FlagLeg = _wtoi(pnt + 1);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(633)) != NULL)  // piano
				{																		  
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(633), RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(814)) != NULL)  // Punto inserimento
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(814), 0);
					point[X] = _wtof(pnt + 2);
					riga.set_name(pnt + 2);
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL) point[Y] = _wtof(pnt + 1);
					temp_list += acutBuildList(RTPOINT, point, 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(815)) != NULL)  // DimX simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(815),	RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(816)) != NULL)  // DimY simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(816), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(817)) != NULL)  // Offset simboli
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(817), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(818)) != NULL)  // Dim. etichette
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(818), RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(632)) != NULL)  // Stile testo
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(632), RTSTR, (pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
				if (gsc_strstr(riga.get_name(), gsc_msg(819)) != NULL)  // Offset etichette
				{
					if ((pnt = gsc_strstr(riga.get_name(), _T(","))) != NULL)
						temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(819),	RTREAL, _wtof(pnt + 1), 0);
					temp_list += acutBuildList(RTLE, 0);
					continue;
				}
			}

			if (ret == GS_BAD) break;
			temp_list += acutBuildList(RTLE, 0);						  // parentesi globale

			// aggiorno la struttura comune dei dati
			if (temp_list.GetCount() <= 2) temp_list.remove_all();
			temp_list.copy(CommonStruct->Legend_param);
			
		} while (0);

		if ((point[X] == 0.0) && (point[Y] == 0.0)) CommonStruct->ins_leg = FALSE;
		else CommonStruct->ins_leg = TRUE;
      
		if (ret == GS_BAD)
		{ 
			if (file) gsc_fclose(file); 
			free(rigaletta);
			acutPrintf(gsc_err(GS_ERR_COD));
			return ret;
		}

      if (file)
         if (gsc_fclose(file) == GS_BAD)
            acutPrintf(gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"
   }
   else	  
   {
      acutPrintf(gsc_err(eGSInvalidPath)); 
      // "*Errore* Path non valida: controllare il percorso e/o le macchine in rete"
   }
	
	if (rigaletta) free(rigaletta);

   return GS_GOOD;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "Save"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Save(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   C_STRING FileName, DefaultFileName, ThmDir, LastThmQryFile;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastThmQryFile"), LastThmQryFile) == GS_BAD ||
       gsc_dir_exist(LastThmQryFile) == GS_BAD)
   {
      // imposto il direttorio di ricerca del file di .LEG 
      ThmDir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      ThmDir += _T("\\");
      ThmDir += GEOQRYDIR;
      ThmDir += _T("\\");
   }
   else
      if (gsc_dir_from_path(LastThmQryFile, ThmDir) == GS_BAD) return;

   // "Legenda"
   if (gsc_get_tmp_filename(ThmDir.get_name(), gsc_msg(57), _T(".leg"), DefaultFileName) == GS_BAD)
      return;

   // "GEOsim - Seleziona file" 
   if (gsc_GetFileD(gsc_msg(645), DefaultFileName, _T("leg"), UI_SAVEFILE_FLAGS, FileName) == RTNORM)
   {
      C_STRING JoinOp, SpatialSel, riga, ext; // JoinOp non usato
      FILE     *file;
      int      ret = GS_GOOD, num_elem, cont;
      presbuf  p = NULL;

      gsc_splitpath(FileName, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // aggiungo l'estensione "LEG"
         FileName += _T(".LEG");

      // compongo la condizione spaziale impostata
      if (gsc_fullSpatialSel(JoinOp, CommonStruct->Inverse,
                             CommonStruct->Boundary, CommonStruct->SelType,
			                    CommonStruct->CoordList, SpatialSel) == GS_BAD)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
         return;
      }

      if ((file = gsc_fopen(FileName, _T("w+"))) == NULL)
      {
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidPath)); // "*Errore* Path non valida: controllare il percorso e/o le macchine in rete"
         return;
      }

      // memorizzo la scelta in GEOSIM.INI per riproporla la prossima volta
      gsc_setPathInfoToINI(_T("LastThmQryFile"), FileName);

      // in funzione dei tematismi scelti calcolo il numero di elementi da salvare
      if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0) num_elem = 3; 
	   else num_elem = (CommonStruct->thm_list.get_count() - 1) + 2;

		do
		{
			// scrivo l'attributo selezionato e il tipo
			riga = CommonStruct->attrib_name.get_name();
			riga += _T(",");
			riga += CommonStruct->attrib_type.get_name();
			riga += GS_LFSTR;
			if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }

			// scrivo il FlagFas della classe e il numero di tematismi scelti
			riga = CommonStruct->FlagFas;
			riga += _T(",");
			if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) == 0) 
				riga += (CommonStruct->thm_list.get_count() - 1);
			else riga += 1;
			riga += GS_LFSTR;
         if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }

			// scrivo il tematismo di visualizzazione scelto e il FlagFas della classe
			riga = CommonStruct->thm.get_name();
			riga += _T(",");
			riga += CommonStruct->FlagThm;
			riga += GS_LFSTR;
         if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }

         // scrivo la condizione spaziale impostata
         if (gsc_SpatialSel_Save(file, SpatialSel) == GS_BAD) { ret = GS_BAD; break; }

			// scrivo il numero di intervalli impostati e la tipologia degli stessi(discreti/continui)
			riga = CommonStruct->n_interv;
			riga += _T(",");
			riga += CommonStruct->type_interv;
			riga += GS_LFSTR;
         if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }

         // scrivo le varie condizioni tematiche
         p = CommonStruct->range_values.get_head();
         cont = 1;
         riga.clear();
         while (p)
         {
		      switch(p->restype)
		      {
		         case RTLB: case RTLE:
			         break;
		         case RTSHORT:
			         if (riga.len() > 0)
                  {
                     riga += _T(",");
                     riga += p->resval.rint;
                     riga += GS_LFSTR;
							if (cont++ == num_elem) cont = 1;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
                  else
                  {  // "Lista valori danneggiata"
                     ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSListError));
                     ret = GS_CAN;
                  }
			         break;
		         case RTREAL:
			         if (riga.len() > 0)
                  {
                     riga += _T(",");
                     riga += p->resval.rreal;
                     riga += GS_LFSTR;
                     if (cont++ == num_elem) cont = 1;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
                  else
                  {  // "Lista valori danneggiata"
                     ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSListError));
                     ret = GS_CAN;
                  }
			         break;
		         case RTSTR:
			         if (riga.len() == 0) riga = p->resval.rstring;
                  else
                  {
                     riga += _T(",");
                     riga += p->resval.rstring;
                     riga += GS_LFSTR;
							if (cont++ == num_elem) cont = 1;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
			         break;
		      }
            if (ret == GS_GOOD) p = p->rbnext;
            else break;
         }
         // scrivo le varie impostazioni della legenda
         p = CommonStruct->Legend_param.get_head();
         riga.clear();
         while (p)
         {
		      switch(p->restype)
		      {
		         case RTLB: case RTLE:
			         break;
               case RTPOINT:
			         if (riga.len() > 0)
                  {
                     riga += _T(",(");
                     riga += p->resval.rpoint[X];
                     riga += _T(",");
                     riga += p->resval.rpoint[Y];
                     riga += _T(")\n");
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
                  else
                  {  // "Lista valori danneggiata"
                     ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSListError));
                     ret = GS_CAN;
                  }
			         break;
	            case RTSHORT:
			         if (riga.len() > 0)
                  {
                     riga += _T(",");
                     riga += p->resval.rint;
                     riga += GS_LFSTR;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
                  else
                  {  // "Lista valori danneggiata"
                     ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSListError));
                     ret = GS_CAN;
                  }
			         break;
		         case RTREAL:
			         if (riga.len() > 0)
                  {
                     riga += _T(",");
                     riga += p->resval.rreal;
                     riga += GS_LFSTR;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
                  else
                  {  // "Lista valori danneggiata"
                     ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSListError));
                     ret = GS_CAN;
                  }
			         break;
		         case RTSTR:
			         if (riga.len() == 0) riga = p->resval.rstring;
                  else
                  {
                     riga += _T(",");
                     riga += p->resval.rstring;
                     riga += GS_LFSTR;
                     if (fwprintf(file, riga.get_name()) < 0) { ret = GS_BAD; break; }
                     riga.clear();
                  }
			         break;
		      }
            if (ret == GS_GOOD) p = p->rbnext;
            else break;
         }
         break;

      } while (0);

      // se si è verificto un errore lo segnalo
      if (ret == GS_BAD)
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSWriteFile)); // "*Errore* Scrittura file fallita"

      if (gsc_fclose(file) == GS_BAD)
         ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSCloseFile)); // "*Errore* Chiusura file fallita"

   }
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "main_filter4legend" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_main_filt4leg_Help(ads_callback_packet *dcl)
   { gsc_help(IDH_LAFUNZIONEDIFILTROTEMATICO); } 

/*****************************************************************************/
/*.doc gsc_ddSelectRange                                                     */
/*+
  Questa funzione imposta un'insieme di range per l'attributo della classe scelta
  mediante finestar DCL.
  Parametri:
  Common_Dcl_legend_Struct &CommonStruct : struttura dei dati interessati

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddSelectRange(Common_Dcl_legend_Struct *CommonStruct)
{
   int        ret = GS_GOOD, status = DLGOK, dcl_file, n_interv = 0, pos_thm = 0;
   int        prj, cls, sub, num_val, passo, len_list = 0;
	C_NODE     *ptr;
   C_CLASS    *pCls;
   C_STRING   path, intest, riga, str, dcl_name, spazio;   
 	C_STR      *punt;
	C_DBCONNECTION *pConn;
   ads_hdlg   dcl_id;

	prj = CommonStruct->prj;
	cls = CommonStruct->cls;
	sub = CommonStruct->sub;

   // Ricavo il puntatore alla classe scelta 
   if ((pCls = gsc_find_class(prj,cls,sub)) == NULL) return GS_BAD;
	pConn = pCls->ptr_info()->getDBConnection(OLD);

   // se ho già impostato intervalli per il tematismo scelto li carico nella C_RB_LIST opportuna
	if (CommonStruct->thm.len() > 0)
	{
	  	if((ptr = CommonStruct->thm_list.search_name
						  (CommonStruct->thm.get_name())) == NULL) return GS_BAD; 
		if ((pos_thm = CommonStruct->thm_list.getpos(ptr)) == 0) return  GS_BAD; 

   	if (pos_thm == CommonStruct->thm_list.get_count())	dcl_name = _T("range_impost8");
		else dcl_name = _T("range_impost16");

		(CommonStruct->range_values).remove_all();
		(CommonStruct->current_values).remove_all();
		if (CommonStruct->thm_sel[pos_thm - 1].GetCount() > 0)
			(CommonStruct->thm_sel[pos_thm - 1]).copy(CommonStruct->range_values);
	}

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) == RTERROR) return GS_BAD;

	CommonStruct->dcl_file = dcl_file;
    
   while (1)
   {
      ads_new_dialog(dcl_name.get_name(), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      // disabilito i tile di Ins/Edit/Del/Leg
		ads_mode_tile(dcl_id, _T("Ins"), MODE_DISABLE); 
		ads_mode_tile(dcl_id, _T("Edit"), MODE_DISABLE); 
		ads_mode_tile(dcl_id, _T("Del"), MODE_DISABLE); 
      if (CommonStruct->n_interv == 0)
		   ads_mode_tile(dcl_id, _T("Leg"), MODE_DISABLE);
      else
         ads_mode_tile(dcl_id, _T("Leg"), MODE_ENABLE); 

		// compongo le intestazioni della finestra DCL
		if (sub == 0)
		{
			str = gsc_msg(230);									 //   "Classe :"
			str += pCls->get_name();	                   
		   ads_set_tile(dcl_id, _T("TitClass"), str.get_name());
			str = gsc_msg(837);									 //   "Sottoclasse :"
			str += GS_EMPTYSTR;
			ads_set_tile(dcl_id, _T("TitSub"), str.get_name());					
	   }
		else
		{
			C_CLASS *madre;
				  
			// Ricavo il puntatore alla classe madre
			if ((madre = gsc_find_class(prj,cls,0)) == NULL) { ret = GS_BAD; break; }

			str = gsc_msg(230);									 //   "Classe :"
			str += madre->get_name();	                   
		   ads_set_tile(dcl_id, _T("TitClass"), str.get_name());
			str = gsc_msg(837);									 //   "Sottoclasse :"
			str += pCls->get_name();
			ads_set_tile(dcl_id, _T("TitSub"), str.get_name());
		}

		str = gsc_msg(827);										 //"Attributo :"
		str += CommonStruct->attrib_name.get_name();
      ads_set_tile(dcl_id, _T("TitAttr"), str.get_name());
		str = gsc_msg(828);										 //"Evidenziazione :"
		str += CommonStruct->thm.get_name();
      ads_set_tile(dcl_id, _T("TitEvid"), str.get_name());
		
		if (CommonStruct->type_interv == 0)					 //"Tipologia :"
		{ str = gsc_msg(829); str += gsc_msg(832); }
		else
		{ str = gsc_msg(829); str += gsc_msg(831); }
		ads_set_tile(dcl_id, _T("TitTipol"), str.get_name());

      // compongo la stringa di intestazione list-box in funzione della/e tipologie di 
      // evidenziazione scelte
      intest = gsc_msg(834);                           // Intervallo
      intest += _T("\t");											 

      if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
      {	// scelta singolo tematismo
			num_val = 3;
			passo = 14;
			//intest += CommonStruct->thm.get_name();
			len_list = 2;
		   if ((punt = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
			punt->set_name(CommonStruct->thm.get_name());
		}
      else
      {	// scelta tematismo multiplo
			len_list = CommonStruct->thm_list.get_count(); 
			num_val = (CommonStruct->thm_list.get_count() - 1) + 2; 
			passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
         punt = (C_STR*) CommonStruct->thm_list.get_head();
		}
		// compongo la stringa di intestazione con  eventuali abbreviazioni
		for (int i = 1; i < len_list; i++)
		{	
			if (gsc_strcmp(punt->get_name(), gsc_msg(632)) == 0)	         // stile testo
				{ intest += gsc_msg(809); intest += _T("\t\t"); }				// Stesto
				
			else if (gsc_strcmp(punt->get_name(), gsc_msg(718)) == 0)	   // altezza testo
					{ intest += gsc_msg(810);	intest += _T("\t"); }  			// Htesto
				
			else if (gsc_strcmp(punt->get_name(), gsc_msg(483)) == 0)	   // larghezza
					{ intest += gsc_msg(811); intest += _T("\t"); }				// Largh.
	
			else if (gsc_strcmp(punt->get_name(), gsc_msg(631)) == 0)	   // tipo linea
					{ intest += gsc_msg(812); intest += _T("\t\t"); }			// Tlinea

			else if (gsc_strcmp(punt->get_name(), gsc_msg(630)) == 0)	   // riempimento
					{ intest += gsc_msg(813); intest += _T("\t\t"); }			// Riemp.

			else if (gsc_strcmp(punt->get_name(), gsc_msg(229)) == 0)	   // Colore riempimento
					{ intest += gsc_msg(229); intest += _T("\t"); }

			else if (gsc_strcmp(punt->get_name(), gsc_msg(629)) == 0)	   // colore
					{ intest += gsc_msg(629); intest += _T("\t"); }

			else if (gsc_strcmp(punt->get_name(), gsc_msg(825)) == 0)	   // scala
					{ intest += gsc_msg(825); intest += _T("\t"); }

			else if (gsc_strcmp(punt->get_name(), gsc_msg(633)) == 0)	   // piano
					{ intest += gsc_msg(633); intest += _T("\t\t"); }

			else if (gsc_strcmp(punt->get_name(), gsc_msg(826)) == 0)	   // blocco
					{ intest += gsc_msg(826); intest += _T("\t\t"); }

         punt = (C_STR*) punt->get_next();
      }

      intest += gsc_msg(835);                           // Valore di range dell'attributo
		if (pConn->IsDate(CommonStruct->attrib_type.get_name()) == GS_GOOD)
		{
			if (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD)
				{ for (int k = 0; k < 13; k++) intest += _T(" "); }
         else { for (int k = 0; k < 26; k++) intest += _T(" "); }
		}
      else intest += _T("\t\t");

      intest += gsc_msg(836);                           // Descrizione

		str = gsc_msg(830);										  //"N° Intervalli :"
		str += gsc_tostring(CommonStruct->n_interv);
      ads_set_tile(dcl_id, _T("TitInter"), str.get_name());

      if (status != LEGEND_SEL)
      {	// recupero i valori già impostati se presenti
         if ((CommonStruct->range_values).GetCount() > 0)
		   {
			   CommonStruct->n_interv = ((CommonStruct->range_values).GetCount() - 2) / passo;
			   (CommonStruct->range_values).copy(CommonStruct->current_values);
			   // riempio la list box con il set di intervalli impostati
            if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
					                   CommonStruct->attrib_name.get_name(),
                                  CommonStruct->attrib_type.get_name(),
				                      num_val, dcl_id, _T("Listrange")) == GS_BAD) break;
		   }
      }
      else
      {
			// riempio la list box con il set di intervalli impostati
         if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
				                   CommonStruct->attrib_name.get_name(),
                               CommonStruct->attrib_type.get_name(),
				                   num_val, dcl_id, _T("Listrange")) == GS_BAD) break;
         status = DLGOK;
      }

      ads_set_tile(dcl_id, _T("TitByAttr"), intest.get_name());
       
      ads_action_tile(dcl_id, _T("Listrange"), (CLIENTFUNC) dcl_ImpostRange_Listrange);
      ads_client_data_tile(dcl_id, _T("Listrange"), CommonStruct);

      ads_action_tile(dcl_id, _T("Add"), (CLIENTFUNC) dcl_ImpostRange_Add);
      ads_client_data_tile(dcl_id, _T("Add"), CommonStruct);

      ads_action_tile(dcl_id, _T("Ins"), (CLIENTFUNC) dcl_ImpostRange_Ins);
      ads_client_data_tile(dcl_id, _T("Ins"), CommonStruct);

      ads_action_tile(dcl_id, _T("Edit"), (CLIENTFUNC) dcl_ImpostRange_Edit);
      ads_client_data_tile(dcl_id, _T("Edit"), CommonStruct);

      ads_action_tile(dcl_id, _T("Del"), (CLIENTFUNC) dcl_ImpostRange_Del);
		ads_client_data_tile(dcl_id, _T("Del"), CommonStruct);

      ads_action_tile(dcl_id, _T("Slidrange"), (CLIENTFUNC) dcl_ImpostRange_Slide);
		ads_client_data_tile(dcl_id, _T("Slidrange"), CommonStruct);

      ads_action_tile(dcl_id, _T("Leg"), (CLIENTFUNC) dcl_ImpostRange_Leg);
      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_ImpostRange_Help);
 
      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                  // uscita con tasto OK
      {
         // salvo il set di intervalli impostati per un loro uso successivo
			(CommonStruct->current_values).copy(CommonStruct->range_values);
			//	copio nel vettore degli intervalli impostati dei vari tematismi 
         // scelti quello che ho aggiornato
			(CommonStruct->thm_sel[pos_thm - 1]).remove_all();
			(CommonStruct->range_values).copy(CommonStruct->thm_sel[pos_thm - 1]);
         break;
      }
      else if(status == DLGCANCEL) break;   // uscita con tasto CANCEL
          
      else if (status == LEGEND_SEL)        // uscita con tasto 'Legenda'
      { if (gsc_ddLegendImpost(CommonStruct) == GS_BAD) { ret = GS_BAD; break; } }
   }
   return ret;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Add"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Add(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
	Common_Dcl_legend_values_Struct CommonValuesStruct;
   ads_hdlg   dcl_id, hdlg = dcl->dialog;
	C_STRING   str;
	C_RB_LIST  temp_list;
	C_CLASS    *pCls;
	int        dcl_file, posiz = 0, status, num_val, pos_val, ret = GS_GOOD, num_msg = TRUE;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	dcl_file = CommonStruct->dcl_file;
	CommonValuesStruct.prj = CommonStruct->prj;
	CommonValuesStruct.cls = CommonStruct->cls;
	CommonValuesStruct.sub = CommonStruct->sub;
	CommonValuesStruct.attrib.set_name(CommonStruct->attrib_name.get_name());
	CommonValuesStruct.attrib_type.set_name(CommonStruct->attrib_type.get_name());
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   

   // ho scelto un solo tematismo
   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
	{
		CommonValuesStruct.passo = 14;
	   num_val = 3;
      pos_val = 8;
	}
	else // ho scelto più tematismi
	{
		CommonValuesStruct.passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
		num_val =(CommonStruct->thm_list.get_count() - 1) + 2; 
      pos_val = ((CommonStruct->thm_list.get_count() - 1) * 4) + 5;
	}
	CommonValuesStruct.posiz = 0;
	CommonValuesStruct.temp_values.remove_all();
	CommonValuesStruct.attrib = CommonStruct->attrib_name.get_name();

   while (1)
   {
		status = DLGOK;
	   ads_new_dialog(_T("current_range"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return; }
		if (ret == GS_GOOD)
		{
			if (gsc_ImpostNewRangeValues(&CommonStruct->thm_list, CommonStruct->thm.get_name(),
												  CommonStruct->attrib_name.get_name(), 
												  CommonStruct->attrib_type.get_name(),
												  &CommonValuesStruct.temp_values, dcl_id, 
												  _T("Editrange"), pCls) == GS_BAD) break;
         CommonValuesStruct.posiz = 1;
		}
		else
		{
			if (gsc_RefreshValues(&CommonValuesStruct.temp_values, 
			                      CommonValuesStruct.attrib.get_name(),
										 CommonValuesStruct.attrib_type.get_name(),
			                      pCls, dcl_id, _T("Editrange")) == GS_BAD) break;
	      if (num_msg != TRUE) ads_set_tile(dcl_id, _T("error"), gsc_msg(num_msg)); //Errore di numero variabile
   	}
    	str = gsc_msg(838);
		str += _T("\t\t\t");
		str += gsc_tostring((CommonStruct->n_interv) + 1);

      ads_set_tile(dcl_id, _T("TitN_Interv"), str.get_name());
       
      ads_action_tile(dcl_id, _T("Editvalue"), (CLIENTFUNC) dcl_ImpostValue_Editvalue);
      ads_client_data_tile(dcl_id, _T("Editvalue"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Select"), (CLIENTFUNC) dcl_ImpostValue_Select);
      ads_client_data_tile(dcl_id, _T("Select"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Editrange"), (CLIENTFUNC) dcl_ImpostValue_Editrange);
      ads_client_data_tile(dcl_id, _T("Editrange"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_ImpostValue_Help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  // aggiorno la lista con i nuovi valori
 			ret = gsc_AddListRange(&CommonStruct->current_values, 
				                    &CommonValuesStruct.temp_values,
										  CommonValuesStruct.attrib.get_name(), &num_msg);

   		if (ret == GS_CAN) continue;
			else if (ret == GS_BAD) break;
			// effettuo il refresh della list-box con il range aggiunto
         if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
				                   CommonStruct->attrib_name.get_name(),
										 CommonStruct->attrib_type.get_name(),
				                   num_val, hdlg, _T("Listrange")) == GS_BAD) break;

			ads_set_tile(dcl_id, _T("Listrange"), gsc_tostring(CommonStruct->n_interv));

			// disattivo i tile modifica-cancella-inserisci-legenda
			ads_mode_tile(hdlg, _T("Edit"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Ins"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Del"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Leg"), MODE_ENABLE);
			CommonStruct->n_interv++;
			str = gsc_msg(830);										 //"N° Intervalli :"
			str += gsc_tostring(CommonStruct->n_interv);
			ads_set_tile(hdlg, _T("TitInter"), str.get_name());

         break;
      }
      else if (status == DLGCANCEL) break;  // uscita con tasto CANCEL
	}
	ads_done_dialog(dcl_id, status);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Edit"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Edit(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
	Common_Dcl_legend_values_Struct CommonValuesStruct;
   ads_hdlg   dcl_id, hdlg = dcl->dialog;
	C_STRING   str;
	C_RB_LIST  temp_list;
	C_CLASS    *pCls;
	int        dcl_file, posiz, status, passo, num_val, riga, pos_val, ret = GS_GOOD, num_msg = TRUE;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	dcl_file = CommonStruct->dcl_file;
	posiz = CommonStruct->current_line;
	CommonValuesStruct.prj = CommonStruct->prj;
	CommonValuesStruct.cls = CommonStruct->cls;
	CommonValuesStruct.sub = CommonStruct->sub;
	CommonValuesStruct.attrib.set_name(CommonStruct->attrib_name.get_name());
	CommonValuesStruct.attrib_type.set_name(CommonStruct->attrib_type.get_name());
	temp_list.remove_all();
	// ricavo il puntatore alla classe
	if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
		                        CommonStruct->sub)) == NULL) return;

	// ho scelto un solo tematismo
   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
   { 
      passo = 14; 
      num_val = 3;
      pos_val = 8;
   }
	else // ho scelto più tematismi
   {
		passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
      num_val =(CommonStruct->thm_list.get_count() - 1) + 2;
      pos_val = ((CommonStruct->thm_list.get_count() - 1) * 4) + 5;
   }
   riga = CommonStruct->current_line;
	CommonValuesStruct.passo = passo;
	CommonValuesStruct.posiz = 0;
	CommonValuesStruct.temp_values.remove_all();
	CommonValuesStruct.attrib = CommonStruct->attrib_name.get_name();

	while (1)
   {
		status = DLGOK;
	   ads_new_dialog(_T("current_range"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return; }
		if (ret == GS_GOOD)
		{
			if (gsc_ReadOldRangeValues(&CommonStruct->current_values,
												&CommonValuesStruct.temp_values, pCls,
                                    CommonStruct->attrib_name.get_name(),
                                    CommonStruct->attrib_type.get_name(),
												passo, posiz, dcl_id, _T("Editrange")) == GS_BAD) break; 
         CommonValuesStruct.posiz = 1;
		}
		else
		{
			if (gsc_RefreshValues(&CommonValuesStruct.temp_values, 
			                      CommonValuesStruct.attrib.get_name(), 
										 CommonValuesStruct.attrib_type.get_name(),
			                      pCls, dcl_id, _T("Editrange")) == GS_BAD) break;
	      if (num_msg != TRUE) ads_set_tile(dcl_id, _T("error"), gsc_msg(num_msg));  //Errore di numero variabile
   	}

		str = gsc_msg(838);
		str += _T("\t\t\t");
		str += gsc_tostring(posiz);

      ads_set_tile(dcl_id, _T("TitN_Interv"), str.get_name());
       
      ads_action_tile(dcl_id, _T("Editvalue"), (CLIENTFUNC) dcl_ImpostValue_Editvalue);
      ads_client_data_tile(dcl_id, _T("Editvalue"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Select"), (CLIENTFUNC) dcl_ImpostValue_Select);
      ads_client_data_tile(dcl_id, _T("Select"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Editrange"), (CLIENTFUNC) dcl_ImpostValue_Editrange);
      ads_client_data_tile(dcl_id, _T("Editrange"), &CommonValuesStruct);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      { 	// aggiorno la lista con i nuovi valori
         ret = gsc_ModListRange(CommonStruct->current_values, 
				                    &CommonValuesStruct.temp_values, 
										  passo, riga, CommonValuesStruct.attrib.get_name(), &num_msg);
			if (ret == GS_CAN) continue;
			else if (ret == GS_BAD) break;
			// effettuo il refresh della list-box dopo la modifica dei valori dell'intervallo
         if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
				                   CommonStruct->attrib_name.get_name(),
										 CommonStruct->attrib_type.get_name(),
				                   num_val, hdlg, _T("Listrange")) == GS_BAD) break;
  			// disattivo i tile modifica-cancella-inserisci-legenda
			ads_mode_tile(hdlg, _T("Edit"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Ins"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Del"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Leg"), MODE_ENABLE);

         break;
      }
      else if (status == DLGCANCEL) break;  // uscita con tasto CANCEL
	}
	ads_done_dialog(dcl_id, status);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Ins"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Ins(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
	Common_Dcl_legend_values_Struct CommonValuesStruct;
   ads_hdlg   dcl_id, hdlg = dcl->dialog;
	C_STRING   str;
	C_RB_LIST  temp_list;
	C_CLASS    *pCls;
	int        dcl_file, posiz = 0, status, passo, riga, num_val, pos_val, ret = GS_GOOD, num_msg = TRUE;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	dcl_file = CommonStruct->dcl_file;
	CommonValuesStruct.prj = CommonStruct->prj;
	CommonValuesStruct.cls = CommonStruct->cls;
	CommonValuesStruct.sub = CommonStruct->sub;
	CommonValuesStruct.attrib.set_name(CommonStruct->attrib_name.get_name());
	CommonValuesStruct.attrib_type.set_name(CommonStruct->attrib_type.get_name());
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   
   // ho scelto un solo tematismo
   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
	{
		passo = 14;
	   num_val = 3;
      pos_val = 8;
	}
	else // ho scelto più tematismi
	{
		passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
		num_val = (CommonStruct->thm_list.get_count() - 1) + 2; 
      pos_val = ((CommonStruct->thm_list.get_count() - 1) * 4) + 5;
	}
   riga = CommonStruct->current_line;
	CommonValuesStruct.passo = passo;
	CommonValuesStruct.posiz = 0;
	CommonValuesStruct.temp_values.remove_all();
	CommonValuesStruct.attrib = CommonStruct->attrib_name.get_name();

   while (1)
   {
		status = DLGOK;
      ads_new_dialog(_T("current_range"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; break; }
		if (ret == GS_GOOD)
		{
			if (gsc_ImpostNewRangeValues(&CommonStruct->thm_list, CommonStruct->thm.get_name(),
												  CommonStruct->attrib_name.get_name(), 
												  CommonStruct->attrib_type.get_name(),
												  &CommonValuesStruct.temp_values,
												  dcl_id, _T("Editrange"), pCls) == GS_BAD) break;
         CommonValuesStruct.posiz = 1;
		}
		else
		{
			if (gsc_RefreshValues(&CommonValuesStruct.temp_values, 
			                      CommonValuesStruct.attrib.get_name(), 
										 CommonValuesStruct.attrib_type.get_name(),
			                      pCls, dcl_id, _T("Editrange")) == GS_BAD) break;
	      if (num_msg != TRUE) ads_set_tile(dcl_id, _T("error"), gsc_msg(num_msg));  //Errore di numero variabile
   	}
		str = gsc_msg(838);
		str += _T("\t\t\t");
		str += gsc_tostring(riga);

      ads_set_tile(dcl_id, _T("TitN_Interv"), str.get_name());
       
      ads_action_tile(dcl_id, _T("Editvalue"), (CLIENTFUNC) dcl_ImpostValue_Editvalue);
      ads_client_data_tile(dcl_id, _T("Editvalue"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Select"), (CLIENTFUNC) dcl_ImpostValue_Select);
      ads_client_data_tile(dcl_id, _T("Select"), &CommonValuesStruct);

      ads_action_tile(dcl_id, _T("Editrange"), (CLIENTFUNC) dcl_ImpostValue_Editrange);
      ads_client_data_tile(dcl_id, _T("Editrange"), &CommonValuesStruct);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      { 	// aggiorno la lista con i nuovi valori
         ret = gsc_InsListRange(CommonStruct->current_values, 
                                &CommonValuesStruct.temp_values,
										  passo, riga,CommonValuesStruct.attrib.get_name(), &num_msg);
			if (ret == GS_CAN) continue;
			else if (ret == GS_BAD) break; 
			// effettuo il refresh della list-box con il range aggiunto
         if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
				                   CommonStruct->attrib_name.get_name(),
										 CommonStruct->attrib_type.get_name(),
				                   num_val, hdlg, _T("Listrange")) == GS_BAD) break; 

  			// disattivo i tile modifica-cancella-inserisci-legenda
			ads_mode_tile(hdlg, _T("Edit"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Ins"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Del"), MODE_DISABLE);
			ads_mode_tile(hdlg, _T("Leg"), MODE_ENABLE);

  			// aggiorno il numero di intervalli impostati
			CommonStruct->n_interv++;
			str = gsc_msg(830);										 //"N° Intervalli :"
			str += gsc_tostring(CommonStruct->n_interv);
   		ads_set_tile(hdlg, _T("TitInter"), str.get_name());
   
         break;
      }
      else if (status == DLGCANCEL) break;  // uscita con tasto CANCEL

	}
	ads_done_dialog(dcl_id, status);
 
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Del"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Del(ads_callback_packet *dcl)
{
	int       posiz, passo = 0, incr = 0, lung = 0, cont = 1, num_val;
	presbuf   punt;
	ads_hdlg  hdlg = dcl->dialog;
	C_STRING  str;
	C_CLASS   *pCls;
	C_RB_LIST temp_list;
	Common_Dcl_legend_Struct *CommonStruct;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   

	if (CommonStruct->current_line != 0) posiz = CommonStruct->current_line;

	// ho scelto un solo tematismo
   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
   { 
      passo = 14; 
      num_val = 3;
   }
	else // ho scelto più tematismi
   {
		passo = 12 + (CommonStruct->thm_list.get_count() - 1) * 4;
      num_val =(CommonStruct->thm_list.get_count() - 1) + 2;
   }
	// calcolo l'incremento iniziale di valori di resbuf
	lung = CommonStruct->current_values.GetCount();
   incr = 2 + (posiz - 1) * passo;
	punt = CommonStruct->current_values.get_head();

	if (((lung - 2) / passo) != 1)
	{
		// copio ogni elemento della lista escludendo quelli relativi all'intervallo 
		// selezionato dalla list-box per essere cancellato
		while (punt)
		{
			if ((cont < incr) || (cont >= incr + passo))
			{
				switch (punt->restype)
				{  
					case RTSHORT:
						temp_list += acutBuildList(RTSHORT, punt->resval.rint, 0);
						break;
					case RTREAL:
						temp_list += acutBuildList(RTREAL, punt->resval.rreal, 0);
						break;
					case RTSTR:
						temp_list += acutBuildList(RTSTR, punt->resval.rstring, 0);
						break;
					case RTLB:
						temp_list += acutBuildList(RTLB, 0);
						break;
					case RTLE:
						temp_list += acutBuildList(RTLE, 0);
						break;
				}
			}
			punt = punt->rbnext;
			cont++;
		}
	}
	else temp_list.remove_all();

	// aggiorno la lista corrente che dovrà poi essere confermata oppure no
   CommonStruct->current_values.remove_all();
	temp_list.copy(CommonStruct->current_values);

	// effettuo il refresh della list-box con il range aggiunto
   if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
		                   CommonStruct->attrib_name.get_name(),
								 CommonStruct->attrib_type.get_name(),
		                   num_val, dcl->dialog, _T("Listrange")) == GS_BAD) return;

   // aggiorno il valore del n° totale di intervalli ed eventualmente disabilito i
   // tile di ins-mod-del-leg
   CommonStruct->n_interv--;
	str = gsc_msg(830);										 //"N° Intervalli :"
	str += gsc_tostring(CommonStruct->n_interv);
	ads_set_tile(hdlg, _T("TitInter"), str.get_name());

   // disattivo i tile Edit/Ins/Del/leg
	ads_mode_tile(hdlg, _T("Edit"), MODE_DISABLE);
	ads_mode_tile(hdlg, _T("Ins"), MODE_DISABLE);
	ads_mode_tile(hdlg, _T("Del"), MODE_DISABLE);
   if (CommonStruct->n_interv == 0)
		ads_mode_tile(hdlg, _T("Leg"), MODE_DISABLE);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Listrange"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Listrange(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   ads_hdlg hdlg = dcl->dialog;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);

	if (CommonStruct->current_values.GetCount() == 0) return;
   else CommonStruct->current_line = _wtoi(dcl->value) + 1;

	if (CommonStruct->current_values.GetCount() == 0)
   {
		ads_mode_tile(hdlg, _T("Edit"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("Ins"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("Del"), MODE_DISABLE);
		ads_mode_tile(hdlg, _T("Leg"), MODE_DISABLE);
      return;
   }
   else
   {
      CommonStruct->current_line = _wtoi(dcl->value) + 1;
		ads_mode_tile(hdlg, _T("Edit"), MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Ins"), MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Del"), MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Leg"), MODE_ENABLE);
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Sliderange"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Slide(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
	int      pos = 0, num_val, len_list, max_slider_value = 200;
   size_t   lung = 0;
	C_STRING intest;
	C_CLASS  *pCls;
	C_STR    *punt;
	C_DBCONNECTION *pConn;
	ads_hdlg dcl_id = dcl->dialog;
		
	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	if ((pCls = gsc_find_class(CommonStruct->prj, CommonStruct->cls, 
                              CommonStruct->sub)) == NULL) return;   
	pConn = pCls->ptr_info()->getDBConnection(OLD);
	pos = _wtoi(dcl->value);

	// compongo la stringa di intestazione list-box in funzione della/e tipologie di 
   // evidenziazione scelte
   intest = gsc_msg(834);                           // Intervallo
   intest += _T("    ");											 

   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0)
   {	// scelta singolo tematismo
		len_list = 2;
		if ((punt = new C_STR) == NULL) { GS_ERR_COD=eGSOutOfMem; return; }
		punt->set_name(CommonStruct->thm.get_name());
	}
   else
   {	// scelta tematismo multiplo
		len_list = CommonStruct->thm_list.get_count(); 
      punt = (C_STR*) CommonStruct->thm_list.get_head();
	}
	// compongo la stringa di intestazione con  eventuali abbreviazioni
	for (int i = 1; i < len_list; i++)
	{																							
		if (gsc_strcmp(punt->get_name(), gsc_msg(632)) == 0)	         // stile testo
			{ intest += gsc_msg(809); intest += _T("          "); }		// Stesto
			
		else if (gsc_strcmp(punt->get_name(), gsc_msg(718)) == 0)	   // altezza testo
				{ intest += gsc_msg(810); intest += _T("  "); }	   		// Htesto
			
		else if (gsc_strcmp(punt->get_name(), gsc_msg(483)) == 0)	   // larghezza
				{ intest += gsc_msg(811); intest += _T("  "); }				// Largh.

		else if (gsc_strcmp(punt->get_name(), gsc_msg(631)) == 0)	   // tipo linea
				{ intest += gsc_msg(812); intest += _T("          "); }	// Tlinea

		else if (gsc_strcmp(punt->get_name(), gsc_msg(630)) == 0)	   // riempimento
				{ intest += gsc_msg(813); intest += _T("          "); }	// Riemp.

		else if (gsc_strcmp(punt->get_name(), gsc_msg(229)) == 0)	   // Colore riempimento
				{ intest += gsc_msg(229); intest += _T("  "); }

		else if (gsc_strcmp(punt->get_name(), gsc_msg(629)) == 0)	   // colore
				{ intest += gsc_msg(629); intest += _T("  "); }

		else if (gsc_strcmp(punt->get_name(), gsc_msg(825)) == 0)	   // scala
				{ intest += gsc_msg(825); intest += _T("   "); }

		else if (gsc_strcmp(punt->get_name(), gsc_msg(633)) == 0)	   // piano
				{ intest += gsc_msg(633); intest += _T("           "); }

		else if (gsc_strcmp(punt->get_name(), gsc_msg(826)) == 0)	   // blocco
				{ intest += gsc_msg(826); intest += _T("          "); }

      punt = (C_STR*) punt->get_next();
   }

   intest += gsc_msg(835);                           // Valore di range dell'attributo
   if (pConn->IsDate(CommonStruct->attrib_type.get_name()) == GS_GOOD)
	{
		if (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD)
			{ for (int k = 0; k < 13; k++) intest += _T(" "); }
      else
			{ for (int k = 0; k < 26; k++) intest += _T(" "); }
	}
   else intest += _T("          ");

   intest += gsc_msg(836);                           // Descrizione
	lung = intest.len();
	for (size_t j = 0; j <= (max_slider_value - lung); j++) intest += _T(" ");

   if (gsc_strcmp(CommonStruct->thm.get_name(), gsc_msg(833)) != 0) num_val = 3;
	else num_val =(CommonStruct->thm_list.get_count() - 1) + 2; 
	
	if (pos > 0)
	{
      ads_set_tile(dcl_id, _T("TitByAttr"), intest.get_name() + pos - 1);
      if (gsc_RefreshRanges(&CommonStruct->current_values, pCls,
			                   CommonStruct->attrib_name.get_name(),
									 CommonStruct->attrib_type.get_name(),
				                num_val, dcl_id, _T("Listrange"), TRUE, 
                            pos) == GS_BAD) return;
	}
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "Leg"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Leg(ads_callback_packet *dcl)
{ ads_done_dialog(dcl->dialog, LEGEND_SEL); }

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "range_impost" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostRange_Help(ads_callback_packet *dcl)
{ gsc_help(IDH_Modalitoperative); } 

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "current_range" su controllo "Editrange"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostValue_Editrange(ads_callback_packet *dcl)
{
   ads_hdlg   hdlg = dcl->dialog;
	Common_Dcl_legend_values_Struct *CommonValuesStruct;
	C_STRING   valore;
	C_CLASS    *pCls;
	C_DBCONNECTION *pConn;
	presbuf    punt, punt1;
	int        passo, posiz, num_thm;
	TCHAR      val[10];

	wcscpy(val, _T("0"));
   CommonValuesStruct = (Common_Dcl_legend_values_Struct*)(dcl->client_data);
   if ((pCls = gsc_find_class(CommonValuesStruct->prj, CommonValuesStruct->cls, 
                              CommonValuesStruct->sub)) == NULL) return;  	
	pConn = pCls->ptr_info()->getDBConnection(OLD);

  	ads_get_tile(hdlg, _T("Editrange"), val, 10);
				
   passo = CommonValuesStruct->passo;
   CommonValuesStruct->posiz = posiz = _wtoi(val) + 1;
   num_thm = passo - 10;
   if (num_thm > 4) num_thm = (num_thm - 2) / 4;
   else num_thm = num_thm / 4;

	if (posiz > 0)
	{
		if (passo == 14)
		{
			punt = CommonValuesStruct->temp_values.getptr_at(4 * posiz); 
			punt1 = CommonValuesStruct->temp_values.getptr_at((4 * posiz) - 1); 
		}
		else if (posiz <= num_thm)
			{
				punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
				punt1 = CommonValuesStruct->temp_values.getptr_at(4 * posiz);
			} 
			else
			{
				punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 2);
				punt1 = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
			}

	   if (punt->restype == RTSTR) valore = punt->resval.rstring;
		else if (punt->restype == RTREAL) gsc_conv_Number(punt->resval.rreal, valore);
			  else valore = punt->resval.rreal;

		ads_set_tile(hdlg, _T("Editvalue"), valore.get_name());
		ads_mode_tile(hdlg, _T("Editvalue"), MODE_ENABLE);
		ads_mode_tile(hdlg, _T("Editvalue"), MODE_SETFOCUS);
	}
	else return;

}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "current_range" su controllo "Select"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostValue_Select(ads_callback_packet *dcl)
{
	Common_Dcl_legend_values_Struct *CommonValuesStruct;
	int      passo, posiz, num_thm, num_righe, valint = 0, ret = GS_GOOD;
   double   valreal = 0.0;
	ads_hdlg hdlg = dcl->dialog;
	presbuf  punt = NULL; 
   C_STRING valore, value, NumberDest;
	C_DBCONNECTION *pConn;
	C_CLASS  *pCls;

	CommonValuesStruct = (Common_Dcl_legend_values_Struct*)(dcl->client_data);
	passo = CommonValuesStruct->passo;
	posiz = CommonValuesStruct->posiz - 1;
   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonValuesStruct->prj, CommonValuesStruct->cls, 
                              CommonValuesStruct->sub)) == NULL) return;   
   // calcolo il numero di tematismi scelti dall'utente
   num_thm = passo - 10;
   if (num_thm > 4) num_thm = (num_thm - 2) / 4;
   else num_thm = num_thm / 4;
	num_righe = num_thm + 2;

   // evidenzio la riga che sto editando con 'Select'
	ads_set_tile(hdlg, _T("Editrange"), gsc_tostring(posiz - 1));

	if (posiz > 0)
	{
		if (passo == 14) punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz)-1); 
		else if (posiz <= num_thm) punt = CommonValuesStruct->temp_values.getptr_at(4 * posiz);
			  else punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
	}
	// controllo quale tematismo dell'attributo scelto sto trattando
   if (punt && punt->restype == RTSTR)
	{
	   if (gsc_strcmp(punt->resval.rstring, gsc_msg(633)) == 0)           // piano
      {
         C_RB_LIST DescrLayer;

  		   value = punt->rbnext->resval.rstring;
         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
         if ((DescrLayer << gsc_ddsellayers(value.get_name())) == NULL) return;
         if (DescrLayer.get_head() != NULL)
            value = DescrLayer.get_head()->resval.rstring;
      }

		if (gsc_strcmp(punt->resval.rstring, gsc_msg(826)) == 0)           // blocco
      {
         C_RB_LIST DescrBlock;

  		   value = punt->rbnext->resval.rstring;
         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
         if ((DescrBlock << gsc_uiSelBlock(value.get_name())) == NULL) return;
         if (DescrBlock.get_head() != NULL)
            value = DescrBlock.get_head()->resval.rstring;
      }
		if (gsc_strcmp(punt->resval.rstring, gsc_msg(632)) == 0)				// stile testo
      {
         C_RB_LIST DescrTextStyle;

  		   value = punt->rbnext->resval.rstring;
         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
         if ((DescrTextStyle << gsc_ddseltextstyles(value.get_name())) == NULL) return;
         if (DescrTextStyle.get_head() != NULL)
            value = DescrTextStyle.get_head()->resval.rstring;
      }
		if (gsc_strcmp(punt->resval.rstring, gsc_msg(825)) == 0)				// scala
      {
  		   valreal = punt->rbnext->resval.rreal;
         ads_set_tile(hdlg, _T("Editvalue"), gsc_tostring(valreal));
         if ((gsc_sel_scala(&valreal)) == GS_BAD) return; 
      }

		if (gsc_strcmp(punt->resval.rstring, gsc_msg(631)) == 0)				// tipo linea
      {
         C_RB_LIST DescrLineType;

  		   value = punt->rbnext->resval.rstring;
         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
         if ((DescrLineType << gsc_ddsellinetype(value.get_name())) == NULL) return;
         if (DescrLineType.get_head() != NULL)
            value = DescrLineType.get_head()->resval.rstring;
      }

		if (gsc_strcmp(punt->resval.rstring, gsc_msg(483)) == 0)				// larghezza
      {
  		   valreal = punt->rbnext->resval.rreal;
         ads_set_tile(hdlg, _T("Editvalue"), gsc_tostring(valreal));
         if ((gsc_sel_larg(&valreal)) == GS_BAD) return;
      }

		if (gsc_strcmp(punt->resval.rstring, gsc_msg(629)) == 0)				// colore
      {
         C_COLOR color;
		   
         color.setString(punt->rbnext->resval.rstring);
			if (gsc_sel_color(value, &color, hdlg) == GS_BAD) return;
         color.getString(value);
      }
		if (gsc_strcmp(punt->resval.rstring, gsc_msg(718)) == 0)				// altezza testo
      {
  		   valreal = punt->rbnext->resval.rreal;
         ads_set_tile(hdlg, _T("Editvalue"), gsc_tostring(valreal));
         if ((gsc_sel_htext(&valreal)) == GS_BAD) return;
      }
		if (gsc_strcmp(punt->resval.rstring, gsc_msg(630)) == 0)				// riempimento
      {
         C_RB_LIST DescrHatch;

  		   value = punt->rbnext->resval.rstring;
         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
         if ((DescrHatch << gsc_ddselhatch(value.get_name())) == NULL) return;
         if (DescrHatch.get_head() != NULL)
            value = DescrHatch.get_head()->resval.rstring;
      }
		if (gsc_strcmp(punt->resval.rstring, gsc_msg(229)) == 0)				// Colore riempimento
      {
         C_COLOR color;
		   
         color.setString(punt->rbnext->resval.rstring);
			if (gsc_sel_color(value, &color, hdlg) == GS_BAD) return;
         color.getString(value);
      }

		// se ho selezionato il 'valore' per l'attributo scelto attivo la lista valori
		if (gsc_strcmp(punt->resval.rstring, CommonValuesStruct->attrib.get_name()) == 0)
      {
			switch (punt->rbnext->restype)
			{	
				case RTSTR:
		  		   value = punt->rbnext->resval.rstring; 
					break;
				case RTREAL:
					gsc_conv_Number(punt->rbnext->resval.rreal, NumberDest);
				   value = NumberDest.get_name();
					break;
				case RTSHORT:
		  		   value = gsc_tostring(punt->rbnext->resval.rint); 
					break;
			}

         ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
			if (gsc_scel_value(CommonValuesStruct->prj, CommonValuesStruct->cls,
				                CommonValuesStruct->sub, CommonValuesStruct->attrib,
                            CommonValuesStruct->attrib_type.get_name(), &value) == GS_BAD) return;

			pConn = pCls->ptr_info()->getDBConnection(OLD);
			if ((pConn->IsCurrency(CommonValuesStruct->attrib_type.get_name()) == GS_BAD) &&
				 (pConn->IsNumeric(CommonValuesStruct->attrib_type.get_name()) == GS_GOOD))
				gsc_conv_Number(value.get_name(), &valreal);
		}
		// aggiorno il valore nella lista di resbuf
      punt = punt->rbnext;
		switch(punt->restype)
		{	
			case RTSTR:
				if (value.len() > 0)
               { if (gsc_RbSubst(punt, value.get_name()) == GS_BAD) return; }
				else 
               { if (gsc_RbSubst(punt, gsc_tostring(valint)) == GS_BAD) return; }
				break;
			case RTREAL:
				if (gsc_RbSubst(punt, valreal) == GS_BAD) return;
				break;
			case RTSHORT:
				if (gsc_RbSubst(punt, valint) == GS_BAD) return;
				break;
		}
		// effettuo il refresh dei valori nella list-box
		if (gsc_RefreshValues(&CommonValuesStruct->temp_values, 
			                   CommonValuesStruct->attrib.get_name(), 
									 CommonValuesStruct->attrib_type.get_name(),
			                   pCls, hdlg, _T("Editrange")) == GS_BAD) return;
      // evidenzio la riga successiva a quella che ho editato
      if (posiz == num_righe)
		{
		   ads_set_tile(hdlg, _T("Editrange"), _T("0"));
			posiz = CommonValuesStruct->posiz = 1;
		}
      else
		{
		   ads_set_tile(hdlg, _T("Editrange"), gsc_tostring(posiz));
			posiz++;
		}
		if (posiz > 0)
		{
			if (passo == 14) punt = CommonValuesStruct->temp_values.getptr_at(4 * posiz); 
			else if (posiz <= num_thm)	punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
				  else punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 2);

			if (punt->restype == RTSTR) valore = punt->resval.rstring;
			else gsc_conv_Number(punt->resval.rreal, valore);		

			ads_set_tile(hdlg, _T("Editvalue"), valore.get_name());
			ads_mode_tile(hdlg, _T("Editvalue"), MODE_ENABLE);
			ads_mode_tile(hdlg, _T("Editvalue"), MODE_SETFOCUS);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "current_range" su controllo "Editvalue"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostValue_Editvalue(ads_callback_packet *dcl)
{
	Common_Dcl_legend_values_Struct *CommonValuesStruct;
   TCHAR    val[255];
	double   MapNum = 0.0;
	int      passo, posiz, num_thm, num_righe;
	C_STRING valore;
	C_DBCONNECTION *pConn;
	C_CLASS  *pCls;

	presbuf  punt, punt1;

	CommonValuesStruct = (Common_Dcl_legend_values_Struct*)(dcl->client_data);
   passo = CommonValuesStruct->passo;
	posiz = CommonValuesStruct->posiz;
	gsc_strcpy(val, GS_EMPTYSTR, 255);
	pCls = gsc_find_class(CommonValuesStruct->prj, CommonValuesStruct->cls, 
								 CommonValuesStruct->sub);
	pConn = pCls->ptr_info()->getDBConnection(OLD);
   
   // calcolo il numero di tematismi scelti dall'utente
   num_thm = passo - 10;
   if (num_thm > 4) num_thm = (num_thm - 2) / 4;
   else num_thm = num_thm / 4;
	num_righe = num_thm + 2;

	if (posiz > 0)
	{
		if (passo == 14) 
		{
			punt = CommonValuesStruct->temp_values.getptr_at(4 * posiz); 
			punt1 = CommonValuesStruct->temp_values.getptr_at((4 * posiz) -1); 
		}
		else if (posiz <= num_thm)
				{
					punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
					punt1 = CommonValuesStruct->temp_values.getptr_at(4 * posiz);
				}
			   else 
				{
				  punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 2);
              punt1 = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
				}
	}

	if (ads_get_tile(dcl->dialog, _T("Editvalue"), val, 255-1) == RTNORM)
   {
		if (gsc_strcmp(punt1->resval.rstring, CommonValuesStruct->attrib.get_name(), FALSE) == 0)
		{
			// controllo il valore immesso dall'utente
			if (pConn->IsCurrency(CommonValuesStruct->attrib_type.get_name()) == GS_GOOD) 
			{
				double MapCurr;
				C_STRING CurrencyDest;

				gsc_conv_Currency(val, &MapCurr);
     			gsc_conv_Currency(MapCurr, CurrencyDest);
				gsc_strcpy(val, CurrencyDest.get_name(), 255 - 1);
			}
			else if (pConn->IsNumeric(CommonValuesStruct->attrib_type.get_name()) == GS_GOOD) 
		  			gsc_conv_Number(val, &MapNum);
			else if (pConn->IsBoolean(CommonValuesStruct->attrib_type.get_name()) == GS_GOOD) 
			{
				int MapBool;
				C_STRING BoolDest;

   			gsc_conv_Bool(val, &MapBool);
				gsc_conv_Bool(MapBool, BoolDest);
				gsc_strcpy(val, BoolDest.get_name(), 255 - 1);
			}
			else if (pConn->IsDate(CommonValuesStruct->attrib_type.get_name()) == GS_GOOD) 
			{
				C_STRING MapDate;

   			if (gsc_conv_DateTime_2_WinFmt(val, MapDate) == GS_BAD)
				{
			      ads_set_tile(dcl->dialog, _T("error"), gsc_msg(287)); // valore non valido
					return;
				}
				else ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
			}
		}

		// aggiorno il valore nella lista di resbuf
		switch(punt->restype)
		{	
			case RTSTR:
				if (gsc_RbSubst(punt, val) == GS_BAD) return;
				break;
			case RTREAL:
				if (MapNum == 0) gsc_conv_Number(val, &MapNum);
				if (gsc_RbSubst(punt, MapNum) == GS_BAD) return;
				break;
			case RTSHORT:
				if (gsc_RbSubst(punt, _wtoi(val)) == GS_BAD) return;
				break;
		}
		// effettuo il refresh dei valori nella list-box
		if (gsc_RefreshValues(&CommonValuesStruct->temp_values, 
			                   CommonValuesStruct->attrib.get_name(),
									 CommonValuesStruct->attrib_type.get_name(),
			                   pCls, dcl->dialog, _T("Editrange")) == GS_BAD) return;
      // evidenzio la riga successiva a quella che ho editato
      if (posiz == num_righe)
		{
		   ads_set_tile(dcl->dialog, _T("Editrange"), _T("0"));
			posiz = CommonValuesStruct->posiz = 1;
		}
      else
		{
		   ads_set_tile(dcl->dialog, _T("Editrange"), gsc_tostring(posiz));
			CommonValuesStruct->posiz++;
			posiz++;
		}
		if (posiz > 0)
		{
			if (passo == 14) punt = CommonValuesStruct->temp_values.getptr_at(4 * posiz); 
			else if (posiz <= num_thm)	punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 1);
				  else punt = CommonValuesStruct->temp_values.getptr_at((4 * posiz) + 2);

			if (punt->restype == RTSTR) valore = punt->resval.rstring;
			else gsc_conv_Number(punt->resval.rreal, valore);
  
			ads_set_tile(dcl->dialog, _T("Editvalue"), valore.get_name());
   		ads_mode_tile(dcl->dialog, _T("Editvalue"), MODE_ENABLE);
	      ads_mode_tile(dcl->dialog, _T("Editvalue"), MODE_SETSEL);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "current_range" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostValue_Help(ads_callback_packet *dcl)
   { gsc_help(IDH_Modalitoperative); } 

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Createlegend"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Create(ads_callback_packet *dcl)
{
   Common_Dcl_legend_Struct *CommonStruct;
   int      *flag = (int*)(dcl->client_data), abilit;
	ads_hdlg hdlg = dcl->dialog;

	*flag = _wtoi(dcl->value);
   CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);

	if (*flag == 0) abilit = MODE_DISABLE;
	else abilit = MODE_ENABLE;

	ads_mode_tile(hdlg, _T("Createonlayer"), abilit);
	ads_mode_tile(hdlg, _T("Layer"), abilit);
	ads_mode_tile(hdlg, _T("Ante"), abilit);
	ads_mode_tile(hdlg, _T("Post"), abilit);
   if (CommonStruct->ins_leg == 1)
   {
	   ads_mode_tile(hdlg, _T("XPoint"), abilit);
   	ads_mode_tile(hdlg, _T("YPoint"), abilit);
   }
   else
   {
	   ads_mode_tile(hdlg, _T("XPoint"), !abilit);
   	ads_mode_tile(hdlg, _T("YPoint"), !abilit);
   }
	ads_mode_tile(hdlg, _T("Select"), abilit);
	ads_mode_tile(hdlg, _T("DimX"), abilit);
	ads_mode_tile(hdlg, _T("DimY"), abilit);
	ads_mode_tile(hdlg, _T("Simboffset"), abilit);
	ads_mode_tile(hdlg, _T("Dim"), abilit);
	ads_mode_tile(hdlg, _T("Liststili"), abilit);
	ads_mode_tile(hdlg, _T("Etichoffset"), abilit);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Createonlayer"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Onlayer(ads_callback_packet *dcl)
{
	C_STRING *Layer = (C_STRING*)(dcl->client_data);
	TCHAR     val[MAX_LEN_LAYERNAME];

	if (ads_get_tile(dcl->dialog, _T("Createonlayer"), val, MAX_LEN_LAYERNAME - 1) == RTNORM)
		Layer->set_name(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Layer"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Layer(ads_callback_packet *dcl)
{
	C_RB_LIST DescrLayer;
 	C_STRING *Layer = (C_STRING*)(dcl->client_data);

   if ((DescrLayer << gsc_ddsellayers(Layer->get_name())) == NULL) return;
   if (DescrLayer.get_head() != NULL) Layer->set_name(DescrLayer.get_head()->resval.rstring);

	ads_set_tile(dcl->dialog, _T("Createonlayer"), Layer->get_name()); 
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Ante"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Ante(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   ads_hdlg hdlg = dcl->dialog;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	CommonStruct->ins_leg = TRUE;
   ads_mode_tile(hdlg, _T("XPoint"), MODE_ENABLE);
	ads_mode_tile(hdlg, _T("YPoint"), MODE_ENABLE);
	ads_mode_tile(hdlg, _T("Select"), MODE_ENABLE);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Post"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Post(ads_callback_packet *dcl)
{
	Common_Dcl_legend_Struct *CommonStruct;
   ads_hdlg hdlg = dcl->dialog;

	CommonStruct = (Common_Dcl_legend_Struct*)(dcl->client_data);
	CommonStruct->ins_leg = FALSE;
   // disabilito i tile relativi al punto di inserimento
	ads_mode_tile(hdlg, _T("XPoint"), MODE_DISABLE);
	ads_mode_tile(hdlg, _T("YPoint"), MODE_DISABLE);
	ads_mode_tile(hdlg, _T("Select"), MODE_DISABLE);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "XPoint"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_XPoint(ads_callback_packet *dcl)
{
	//ads_point *point = (ads_point*)(dcl->client_data);
   double    *point = (double*)(dcl->client_data);
	TCHAR     val[50];
	//double    MapNum;
	//C_STRING  Valore;

	if (ads_get_tile(dcl->dialog, _T("XPoint"), val, 50-1) == RTNORM) *point = _wtof(val);
	/*{
		gsc_conv_Number(val, &MapNum);
		*point[X] = MapNum;
		gsc_conv_Number(MapNum, Valore);
	   ads_set_tile(dcl->dialog, "XPoint", Valore.get_name());

	}*/
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "YPoint"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_YPoint(ads_callback_packet *dcl)
{
	//ads_point *point = (ads_point*)(dcl->client_data);
   double    *point = (double*)(dcl->client_data);
	TCHAR     val[50];

	if (ads_get_tile(dcl->dialog, _T("YPoint"), val, 50-1) == RTNORM) *point = _wtof(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Select"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Select(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, PNT_LEG_SEL);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "DimX"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_DimX(ads_callback_packet *dcl)
{
	double *dimx = (double*)(dcl->client_data);
	TCHAR  val[50];

	if (ads_get_tile(dcl->dialog, _T("DimX"), val, 50-1) == RTNORM) *dimx = _wtof(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "DimY"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_DimY(ads_callback_packet *dcl)
{
	double *dimy = (double*)(dcl->client_data);
	TCHAR  val[50];

	if (ads_get_tile(dcl->dialog, _T("DimY"), val, 50-1) == RTNORM) *dimy = _wtof(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Simboffset"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Simboffset(ads_callback_packet *dcl)
{
	double *simboffset = (double*)(dcl->client_data);
	TCHAR   val[50];

	if (ads_get_tile(dcl->dialog, _T("Simboffset"), val, 50-1) == RTNORM) *simboffset = _wtof(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Dim"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Dim(ads_callback_packet *dcl)
{
	double *simboffset = (double*)(dcl->client_data);
	TCHAR   val[50];

	if (ads_get_tile(dcl->dialog, _T("Dim"), val, 50-1) == RTNORM) *simboffset = _wtof(val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Liststili"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Liststili(ads_callback_packet *dcl)
{
	C_RB_LIST DescrStile;
 	C_STRING *Stile = (C_STRING*)(dcl->client_data), riga;

   if ((DescrStile << gsc_ddseltextstyles(Stile->get_name())) == NULL) return;
   if (DescrStile.get_head() != NULL) Stile->set_name(DescrStile.get_head()->resval.rstring);

   riga = gsc_msg(632);
   riga += _T(":   ");
   riga += Stile->get_name();
   ads_set_tile(dcl->dialog, _T("Stile"), riga.get_name()); 
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "legend" su controllo "Etichoffset"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Etichoffset(ads_callback_packet *dcl)
{
	double *etichoffset = (double*)(dcl->client_data);
	TCHAR   val[50];

	if (ads_get_tile(dcl->dialog, _T("Etichoffset"), val, 50-1) == RTNORM) *etichoffset = _wtof(val);
   if (*etichoffset <= 0.0) ads_set_tile(dcl->dialog, _T("error"), gsc_msg(287));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "current_range" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_ImpostLegend_Help(ads_callback_packet *dcl)
   { gsc_help(IDH_Modalitoperative); } 

/*****************************************************************************/
/*.doc gsc_ReadOldRangeValues                                                */
/*+
  Questa funzione imposta un'insieme di range per l'attributo della classe scelta
  mediante finestar DCL.
  Parametri:
  C_RB_LIST current_values : struttura dei dati interessati;
  C_RB_LIST *temp_list : puntatore alla lista temporanea;
  C_CLASS   *pCls;
  TCHAR     *attrib : nome attributo selezionato;
  TCHAR     *attrib_type;
  int       passo : passo ripetitivo del set di valori per intervallo nella 
                    lista globale degli stessi;
  int       posiz : posizione della linea selezionata nella list-box degli
                    intevalli impostati;
  ads_hdlg	hdlg : identificativo della dcl in cui visualizzare i valori;
  TCHAR     *dcl_tile : nome della list-box in questione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ReadOldRangeValues(C_RB_LIST *current_values, C_RB_LIST *temp_list, 
									C_CLASS *pCls, TCHAR *attrib, TCHAR *attrib_type,int passo,
									int posiz, ads_hdlg hdlg, TCHAR *dcl_tile)
{
	presbuf  punt;
	int      cont = 1, incr = 1, lung = 0, flag_attr = FALSE;
	C_STRING str, first_value, Destination;
	C_DBCONNECTION *pConn = pCls->ptr_info()->getDBConnection(OLD);

	// calcolo l'incremento iniziale di valori di resbuf
	lung = current_values->GetCount();
   incr = 2 + (posiz - 1 ) * passo;
	punt = current_values->get_head();

   ads_start_list(hdlg, dcl_tile, LIST_NEW, 0);

	while (punt)
	{
		if ((cont >= incr) && (cont < incr + passo))
		{
			switch (punt->restype)
			{  
				case RTSHORT:
					if (str.len() > 0)
					{
						if (gsc_strstr(str.get_name(), _T(":")) != NULL)
						{
							str += punt->resval.rint;
							gsc_add_list(str);
							str.clear();
						}
					}
        			*temp_list += acutBuildList(RTSHORT, punt->resval.rint, 0);
					break;
				case RTREAL:
					if (str.len() > 0)
					{
						if (gsc_strstr(str.get_name(), _T(":")) != NULL)
						{
							if (flag_attr) flag_attr = FALSE;
							gsc_conv_Number(punt->resval.rreal, Destination);
							str += Destination.get_name();
							gsc_add_list(str);
							str.clear();
						}
					}
      			*temp_list += acutBuildList(RTREAL, punt->resval.rreal, 0);
					break;
				case RTSTR:
					if (gsc_strcmp(punt->resval.rstring, attrib) == 0) flag_attr = TRUE; 
					if (str.len() > 0)
					{
						if (gsc_strstr(str.get_name(), _T(":")) != NULL)
						{
							if (flag_attr)
							{
								// testo se l'attributo è di tipo 'data'
								if (pConn->IsDate(attrib_type) == GS_GOOD)
								{
									gsc_conv_DateTime_2_WinFmt(punt->resval.rstring, Destination);
									str += Destination.get_name();
									flag_attr = FALSE;
								}
								else str += punt->resval.rstring;	
							}
							else str += punt->resval.rstring;

							gsc_add_list(str);
							str.clear();
    					}
						else
						{
							str += punt->resval.rstring;
							str += _T("\t: ");
						}
					}
					else
					{
                  if (gsc_strcmp(punt->resval.rstring, attrib) == NULL)
                     str += gsc_msg(835); 
                  else
   						str += punt->resval.rstring;
						str += _T("\t: ");
					}
					*temp_list += acutBuildList(RTSTR, punt->resval.rstring, 0);
					break;
				case RTLB:
					*temp_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					*temp_list += acutBuildList(RTLE, 0);
					break;
				default:
					break;
			}
		}

		punt = punt->rbnext;
		cont++;
	}

	ads_end_list();

	if (passo == 14)
	{
		if (temp_list->getptr_at(4)->restype == RTSTR) 
			first_value = temp_list->getptr_at(4)->resval.rstring;
	   else if (temp_list->getptr_at(4)->restype == RTREAL)
     		  gsc_conv_Number(temp_list->getptr_at(4)->resval.rreal, first_value);
	}
   else
	{
		if (temp_list->getptr_at(5)->restype == RTSTR) 
			first_value = temp_list->getptr_at(5)->resval.rstring;
	   else if (temp_list->getptr_at(5)->restype == RTREAL)
           gsc_conv_Number(temp_list->getptr_at(4)->resval.rreal, first_value); 
	}

   ads_set_tile(hdlg, _T("Editrange"), _T("0"));
   ads_mode_tile(hdlg, _T("Editrange"), MODE_SETSEL);
   ads_set_tile(hdlg, _T("Editvalue"), first_value.get_name());
   ads_mode_tile(hdlg, _T("Editvalue"), MODE_ENABLE);
   ads_mode_tile(hdlg, _T("Editvalue"), MODE_SETFOCUS);

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_ModListRange                                                     */
/*+
  Questa funzione imposta aggiorna il range per l'attributo della classe scelta
  Parametri:
  C_RB_LIST *current_values : struttura dei dati interessati;
  C_RB_LIST *temp_list : puntatore alla lista temporanea;
  int       passo : passo ripetitivo della serie di valori per un intervallo 
                    nella C_RB_LIST current_values;
  int       posiz : posizione nella C_RB_LIST current_values dove andrà inserito 
                    il nuovo range di valori relativo all'intervallo.
  TCHAR *attrib
  int *num_msg

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ModListRange(C_RB_LIST &current_values, C_RB_LIST *temp_list,
                     int passo, int posiz, TCHAR *attrib, int *num_msg)
{
	int       incr = 0, lung = 0, cont = 1, ret = GS_GOOD, num_mess = 0;
	presbuf   punt1, punt2, p = NULL;
	C_RB_LIST comodo_list;

	// calcolo l'incremento iniziale di valori di resbuf
	lung = current_values.GetCount();
   incr = 2 + (posiz - 1 ) * passo;
	punt1 = current_values.get_head();
	punt2 = temp_list->get_head();

	// verifico la validità dei dati immessi secondo la loro tipologia
	ret = gsc_validvalue(temp_list->get_head(), attrib, &num_mess);

	if (ret == GS_CAN) { *num_msg = num_mess; return GS_CAN; }
	else if (ret == GS_BAD) return GS_BAD;

   while (punt1)
	{
		// copio nella lista gli elementi fino al punto in cui devo inserire le 
		// modifiche dell'intervallo scelto e la lista di quelli che seguiranno 
		if ((cont < incr) || (cont >= incr + passo))
		{
			switch (punt1->restype)
			{  
				case RTSHORT:
					comodo_list += acutBuildList(RTSHORT, punt1->resval.rint, 0);
					break;
				case RTREAL:
					comodo_list += acutBuildList(RTREAL, punt1->resval.rreal, 0);
					break;
				case RTSTR:
					comodo_list += acutBuildList(RTSTR, punt1->resval.rstring, 0);
					break;
				case RTLB:
					comodo_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					comodo_list += acutBuildList(RTLE, 0);
					break;
			}
		}
		// copio i valori del nuovo intervallo inserito
		else
		{
			switch (punt2->restype)
			{  
				case RTSHORT:
					comodo_list += acutBuildList(RTSHORT, punt2->resval.rint, 0);
					break;
				case RTREAL:
					comodo_list += acutBuildList(RTREAL, punt2->resval.rreal, 0);
					break;
				case RTSTR:
					comodo_list += acutBuildList(RTSTR, punt2->resval.rstring, 0);
					break;
				case RTLB:
					comodo_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					comodo_list += acutBuildList(RTLE, 0);
					break;
			}
			punt2 = punt2->rbnext;
		}
		punt1 = punt1->rbnext;
		cont++;
	}

	// aggiorno la lista corrente che dovrà poi essere confermata oppure no
   current_values.remove_all();
	comodo_list.copy(current_values);

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_InsListRange                                                     */
/*+
  Questa funzione imposta aggiorna il range per l'attributo della classe scelta
  Parametri:
  C_RB_LIST *current_values : struttura dei dati interessati;
  C_RB_LIST *temp_list : puntatore alla lista temporanea;
  int       passo : passo ripetitivo della serie di valori per un intervallo 
                    nella C_RB_LIST current_values;
  TCHAR     * attrib : nome attributo scelto della classe;
  int       *num_msg : numero del messaggio di errore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_InsListRange(C_RB_LIST &current_values, C_RB_LIST *temp_list, 
							int passo, int posiz, TCHAR *attrib, int *num_msg)
{
	int       incr = 0, lung = 0, cont = 1, ret = GS_GOOD, num_mess = 0;
	presbuf   punt1, punt2, p = NULL;
	C_RB_LIST comodo_list;

	// verifico la validità dei dati immessi secondo la loro tipologia
	if ((attrib) || (*num_msg == FALSE))
		ret = gsc_validvalue(temp_list->get_head(), attrib, &num_mess);

	if (ret == GS_CAN) { *num_msg = num_mess; return GS_CAN; }
	else if (ret == GS_BAD) return GS_BAD;

	// calcolo l'incremento iniziale di valori di resbuf
	lung = current_values.GetCount();
   incr = 2 + (posiz - 1) * passo;
	punt1 = current_values.get_head();
	punt2 = temp_list->get_head();

	while (punt1)
	{
		// copio nella lista gli elementi fino al punto in cui devo inserire il nuovo
      do 
		{
			switch (punt1->restype)
			{  
				case RTSHORT:
					comodo_list += acutBuildList(RTSHORT, punt1->resval.rint, 0);
					break;
				case RTREAL:
					comodo_list += acutBuildList(RTREAL, punt1->resval.rreal, 0);
					break;
				case RTSTR:
					comodo_list += acutBuildList(RTSTR, punt1->resval.rstring, 0);
					break;
				case RTLB:
					comodo_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					comodo_list += acutBuildList(RTLE, 0);
					break;
			}
			punt1 = punt1->rbnext;
			cont++;

		} while (cont < incr);

		// copio i valori del nuovo intervallo inserito
		do
		{
			switch (punt2->restype)
			{  
				case RTSHORT:
					comodo_list += acutBuildList(RTSHORT, punt2->resval.rint, 0);
					break;
				case RTREAL:
					comodo_list += acutBuildList(RTREAL, punt2->resval.rreal, 0);
					break;
				case RTSTR:
					comodo_list += acutBuildList(RTSTR, punt2->resval.rstring, 0);
					break;
				case RTLB:
					comodo_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					comodo_list += acutBuildList(RTLE, 0);
					break;
			}
			(punt2 = punt2->rbnext);

		} while (punt2);
		// aggiungo alla lista gli elementi restanti già presenti
      do 
		{
			switch (punt1->restype)
			{  
				case RTSHORT:
					comodo_list += acutBuildList(RTSHORT, punt1->resval.rint, 0);
					break;
				case RTREAL:
					comodo_list += acutBuildList(RTREAL, punt1->resval.rreal, 0);
					break;
				case RTSTR:
					comodo_list += acutBuildList(RTSTR, punt1->resval.rstring, 0);
					break;
				case RTLB:
					comodo_list += acutBuildList(RTLB, 0);
					break;
				case RTLE:
					comodo_list += acutBuildList(RTLE, 0);
					break;
			}
			punt1 = punt1->rbnext;
			cont++;

		} while ((cont >= incr) && punt1);
	}

	// aggiorno la lista corrente che dovrà poi essere confermata oppure no
   current_values.remove_all();
	comodo_list.copy(current_values);

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_AddListRange                                                      */
/*+
  Questa funzione imposta aggiorna il range per l'attributo della classe scelta
  Parametri:
  C_RB_LIST *current_values : puntatore alla lista dei valori correnti;
  C_RB_LIST *temp_list : puntatore alla lista temporanea;
  TCHAR *attrib
  int       num_msg : numero del'eventuale errore da segnalere (default = FALSE).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_AddListRange(C_RB_LIST *current_values, C_RB_LIST *temp_list,
                     TCHAR *attrib, int *num_msg)
{
	presbuf p = NULL;
	int     tipo, ret = GS_GOOD, num_mess = 0;

   // verifico la validità dei valori inseriti per piano/blocco/stile testo/scala/tipo linea/
	// larghezza/colore/altezza testo/riempimento e per l'attributo selezionato

	ret = gsc_validvalue(temp_list->get_head(), attrib, &num_mess);

	if (ret == GS_CAN) { *num_msg = num_mess; return GS_CAN; }
	else if (ret == GS_BAD) return GS_BAD;

	// aggiungo alla lista la temp_list impostata
	if (current_values->GetCount() == 0)
		*current_values << acutBuildList(RTLB, 0);
	else
		current_values->remove_tail();

	p = temp_list->get_head();

	while (p)
	{
		tipo = p->restype;
		switch(tipo)
		{
			case RTLB: case RTLE:
				*current_values += acutBuildList(tipo, 0);
				break;
			case RTSHORT:
				*current_values += acutBuildList(tipo, p->resval.rint, 0);
				break;
			case RTREAL:
				*current_values += acutBuildList(tipo, p->resval.rreal, 0);
				break;
			case RTSTR:
				*current_values += acutBuildList(tipo, p->resval.rstring, 0);
				break;
		}
		p = p->rbnext;
	}
	*current_values += acutBuildList(RTLE, 0);

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_ImpostNewRangeValues                                              */
/*+
  Questa funzione visualizza in una list-box una serie di valori di un intervallo
  per renderli disponibili alla prima editazione nella forma:
  tipologia : 'valore'.La funzione inizializza anche con valori nulli una C_RB_LIST
  che conterrà i valori in seguito impostati dall'utente.

  Parametri:
  C_STR_LIST *lista : lista tematismi da elencare per l'impostazione (se tutti); 
  TCHAR       *thm	: tematismo scelto;
  TCHAR       *attrib : attributo della classe su cui impostare il filtro;
  TCHAR       *tipo_attr : tipologia dell'attributo;
  C_RB_LIST  *temp_list : lista dei valori dell'intervallo che si dovranno impostare;
  ads_hdlg   dcl_id : identificativo della dcl aperta;
  TCHAR       *dcl_tile : nome della list-box di utilizzo;
  C_CLASS    *pCls : puntatore alla classe.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ImpostNewRangeValues(C_STR_LIST *lista, TCHAR *thm, TCHAR *attrib, 
									  TCHAR *tipo_attr, C_RB_LIST *temp_list, ads_hdlg dcl_id,
									  TCHAR *dcl_tile, C_CLASS *pCls)
{
	C_NODE   *punt = lista->get_head();
	int      lung = lista->get_count();
	C_STRING str;
	C_DBCONNECTION *pConn;

	ads_start_list(dcl_id, _T("Editrange"), LIST_NEW, 0);

	// se ho scelto tutti i tematismi riempio la lista con le possibili scelte
	if (gsc_strcmp(thm, gsc_msg(833)) == 0)
	{
		*temp_list << acutBuildList(RTLB, RTLB, 0);

		for (int i = 1; i < lung; i++)
		{
			str = gsc_tostring(punt->get_name());
			if ((gsc_strcmp(punt->get_name(), gsc_msg(633)) == 0) ||    // Piano
				 (gsc_strcmp(punt->get_name(), gsc_msg(826)) == 0) ||    // Blocco
				 (gsc_strcmp(punt->get_name(), gsc_msg(629)) == 0) ||    // Colore
				 (gsc_strcmp(punt->get_name(), gsc_msg(630)) == 0) ||    // Riempimento
				 (gsc_strcmp(punt->get_name(), gsc_msg(229)) == 0) ||    // Colore riempimento
				 (gsc_strcmp(punt->get_name(), gsc_msg(631)) == 0) ||    // Tipolinea
				 (gsc_strcmp(punt->get_name(), gsc_msg(632)) == 0))      // Stile testo 
				*temp_list += acutBuildList(RTLB, RTSTR, 
													 punt->get_name(), RTSTR, GS_EMPTYSTR, RTLE, 0);
			else	// Scala Larghezza Altezza testo
				*temp_list += acutBuildList(RTLB, RTSTR, 
													 punt->get_name(), RTREAL, 0.0, RTLE, 0);

			str += _T("\t: ");
			gsc_add_list(str);

			punt = punt->get_next();
		}
		*temp_list += acutBuildList(RTLE, 0);
	}
	else	// ho scelto un solo tematismo
	{
		*temp_list << acutBuildList(RTLB, 0);

		if ((gsc_strcmp(thm, gsc_msg(633)) == 0) ||    // Piano
			 (gsc_strcmp(thm, gsc_msg(826)) == 0) ||    // Blocco
			 (gsc_strcmp(thm, gsc_msg(629)) == 0) ||    // Colore
			 (gsc_strcmp(thm, gsc_msg(630)) == 0) ||    // Riempimento
          (gsc_strcmp(thm, gsc_msg(229)) == 0) ||    // Colore riempimento
			 (gsc_strcmp(thm, gsc_msg(631)) == 0) ||    // Tipolinea
			 (gsc_strcmp(thm, gsc_msg(632)) == 0))      // Stile testo 
			*temp_list += acutBuildList(RTLB, RTSTR, thm, RTSTR, GS_EMPTYSTR, RTLE, 0);
		else	// Scala Larghezza Altezza testo
			*temp_list += acutBuildList(RTLB, RTSTR, thm, RTREAL, 0.0, RTLE, 0);
		str = thm;
		str += _T("\t: ");
		gsc_add_list(str);
	}
	
	*temp_list += acutBuildList(RTLB, RTSTR, attrib, 0);
	pConn = pCls->ptr_info()->getDBConnection(OLD);
	// in funzione della tipologia dell'attributo scelto costruisco la lista dei nuovi valori
   if (pConn->IsCurrency(tipo_attr) == GS_GOOD) 
		*temp_list += acutBuildList(RTSTR, GS_EMPTYSTR, RTLE, 0);
	else if (pConn->IsNumeric(tipo_attr) == GS_GOOD)
	   	  *temp_list += acutBuildList(RTREAL, 0.0, RTLE, 0);
	     else *temp_list += acutBuildList(RTSTR, GS_EMPTYSTR, RTLE, 0);

	str = gsc_msg(835);								 // Valore
	str += _T("\t: ");
	gsc_add_list(str);

	str = gsc_msg(836);								 // Descr.
	*temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(836), RTSTR, GS_EMPTYSTR, RTLE, 0);
	str += _T("\t: ");
	gsc_add_list(str);
	*temp_list += acutBuildList(RTLE, 0);

   ads_end_list();
   ads_set_tile(dcl_id, _T("Editvalue"), GS_EMPTYSTR);
   ads_mode_tile(dcl_id, _T("Editvalue"), MODE_ENABLE);
   ads_mode_tile(dcl_id, _T("Editvalue"), MODE_SETFOCUS);
   ads_set_tile(dcl_id, _T("Editrange"), _T("0"));
   ads_mode_tile(dcl_id, _T("Editrange"), MODE_SETSEL);

	return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_ImpostNewRangeValues                                              */
/*+
  Questa funzione visualizza in una list-box una serie di valori di un intervallo
  per renderli disponibili alla prima editazione nella forma:
  tipologia : 'valore'.La funzione inizializza anche con valori nulli una C_RB_LIST
  che conterrà i valori in seguito impostati dall'utente.

  Parametri:
  C_STR_LIST *sel_val : lista dei 'valori' dell'attributo selezionato;
  C_STR_LIST *lista : lista tematismi da elencare per l'impostazione (se tutti); 
  TCHAR       *thm	: tematismo scelto;
  TCHAR       *attrib : attributo della classe su cui impostare il filtro;
  C_RB_LIST  *temp_list : lista dei valori dell'intervallo che si dovranno impostare;
  C_CLASS    *p_cls : puntatore alla classe. 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ImpostDefRangeValues(C_STR_LIST *sel_val, C_STR_LIST *lista, TCHAR *thm, 
									  TCHAR *attrib, C_RB_LIST *temp_list, C_CLASS *p_cls)
{
	C_NODE   *punt;
	C_STRING str, layer; 
	int      lung = lista->get_count(), cont = sel_val->get_count();

   if (p_cls->ptr_fas()) layer = p_cls->ptr_fas()->layer;
   else layer = DEFAULT_LAYER;

	*temp_list << acutBuildList(RTLB, 0); 	  // apro la lista complessiva degli intevalli impostati
	for (int j = 1; j <= cont; j++)
	{
		// se ho scelto tutti i tematismi riempio la lista con le possibili scelte
		if (gsc_strcmp(thm, gsc_msg(833)) == 0)
		{
			*temp_list += acutBuildList(RTLB, RTLB, 0);
			punt = lista->get_head();

			for (int i = 1; i < lung; i++)
			{
				str = gsc_tostring(punt->get_name());
				if ((gsc_strcmp(punt->get_name(), gsc_msg(826)) == 0) ||    // Blocco
					 (gsc_strcmp(punt->get_name(), gsc_msg(630)) == 0) ||    // Riempimento
					 (gsc_strcmp(punt->get_name(), gsc_msg(631)) == 0) ||    // Tipolinea
					 (gsc_strcmp(punt->get_name(), gsc_msg(632)) == 0))      // Stile testo 
					*temp_list += acutBuildList(RTLB, RTSTR, 
														 punt->get_name(), RTSTR, GS_EMPTYSTR, RTLE, 0);

            else if (gsc_strcmp(punt->get_name(), gsc_msg(633)) == 0)   // Piano
				{
					*temp_list += acutBuildList(RTLB, RTSTR, punt->get_name(), 
                                           RTSTR, layer.get_name(), RTLE, 0);
				}

            else if (gsc_strcmp(punt->get_name(), gsc_msg(629)) == 0)   // Colore
				{
					*temp_list += acutBuildList(RTLB, RTSTR, punt->get_name(), 
                                           RTSTR, gsc_tostring(j), RTLE, 0);
				}

            else if (gsc_strcmp(punt->get_name(), gsc_msg(229)) == 0)   // Colore riempimento
				{
					*temp_list += acutBuildList(RTLB, RTSTR, punt->get_name(), 
                                           RTSTR, gsc_tostring(j), RTLE, 0);
				}

				else if (gsc_strcmp(punt->get_name(), gsc_msg(825)) == 0)   // Scala
				{
					*temp_list += acutBuildList(RTLB, RTSTR, 
														 punt->get_name(), RTREAL, (j + 1) * 0.5, RTLE, 0);
				}
				else if (gsc_strcmp(punt->get_name(), gsc_msg(483)) == 0)   // Larghezza
				{
					*temp_list += acutBuildList(RTLB, RTSTR, 
														 punt->get_name(), RTREAL, (j * 0.2), RTLE, 0);
				}
				else if (gsc_strcmp(punt->get_name(), gsc_msg(718)) == 0)   // Altezza testo 
				{
					*temp_list += acutBuildList(RTLB, RTSTR, 
														 punt->get_name(), RTREAL, (j + 1) * 0.5, RTLE, 0);
				}

				punt = punt->get_next();
			}
			*temp_list += acutBuildList(RTLE, 0);
		}
		else	// ho scelto un solo tematismo
		{
			*temp_list += acutBuildList(RTLB, 0);	// apro la lista del singolo intervallo

			if ((gsc_strcmp(thm, gsc_msg(633)) == 0) ||    // Piano
   			 (gsc_strcmp(thm, gsc_msg(826)) == 0) ||    // Blocco
	    		 (gsc_strcmp(thm, gsc_msg(630)) == 0) ||    // Riempimento
				 (gsc_strcmp(thm, gsc_msg(631)) == 0) ||    // Tipolinea
				 (gsc_strcmp(thm, gsc_msg(632)) == 0))      // Stile testo 
		 		*temp_list += acutBuildList(RTLB, RTSTR, thm, RTSTR, GS_EMPTYSTR, RTLE, 0);

		 	else if (gsc_strcmp(thm, gsc_msg(629)) == 0)   // Colore
				*temp_list += acutBuildList(RTLB, RTSTR, thm, RTSTR, gsc_tostring(j), RTLE, 0);

		 	else if (gsc_strcmp(thm, gsc_msg(229)) == 0)   // Colore riempimento
				*temp_list += acutBuildList(RTLB, RTSTR, thm, RTSTR, gsc_tostring(j), RTLE, 0);

			else if (gsc_strcmp(thm, gsc_msg(825)) == 0)   // Scala
				*temp_list += acutBuildList(RTLB, RTSTR, thm, RTREAL, (j + 1) * 0.5, RTLE, 0);

			else if (gsc_strcmp(thm, gsc_msg(483)) == 0)   // Larghezza
				*temp_list += acutBuildList(RTLB, RTSTR, thm, RTREAL, (j * 0.2), RTLE, 0);

			else if (gsc_strcmp(thm, gsc_msg(718)) == 0)   // Altezza testo 
				*temp_list += acutBuildList(RTLB, RTSTR, thm, RTREAL, (j + 1) * 0.5, RTLE, 0);
		}
		*temp_list += acutBuildList(RTLB, RTSTR, attrib, 0);                                     // 'attributo'
		*temp_list += acutBuildList(RTREAL, _wtof((sel_val->goto_pos(j))->get_name()), RTLE, 0);  // 'valore'
		*temp_list += acutBuildList(RTLB, RTSTR, gsc_msg(836), RTSTR, GS_EMPTYSTR, RTLE, 0);              // Descr.
		*temp_list += acutBuildList(RTLE, 0);	  // chiudo la lista del singolo intervallo
	}
   *temp_list += acutBuildList(RTLE, 0);    // chiudo la lista complessiva degli intevalli impostati

	return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_RefreshValues                                                     */
/*+
  Questa funzione effettua il refresh dei valori di un intervallo impostati.

  Parametri:
  C_RB_LIST  *temp_list : lista dei valori dell'intervallo che si dovranno impostare;
  TCHAR       *attrib : attributo scelto su cui applicare il filtro multiplo
  TCHAR *attrib_type
  C_CLASS *pCls
  ads_hdlg   dcl_id : identificativo della dcl aperta;
  TCHAR       *dcl_tile : nome della list-box di utilizzo.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_RefreshValues(C_RB_LIST *temp_list, TCHAR *attrib, TCHAR *attrib_type,
							 C_CLASS *pCls, ads_hdlg dcl_id, TCHAR *dcl_tile)
{
	C_STRING str, Destination, NumberDest;
   C_DBCONNECTION *pConn = pCls->ptr_info()->getDBConnection(OLD);
	presbuf  p;
	int flag_attr = FALSE;

	ads_start_list(dcl_id, dcl_tile, LIST_NEW, 0);

	p = temp_list->get_head();
	str.clear();
	while (p)
	{
		switch (p->restype)
		{
			case RTSTR:
				if (gsc_strcmp(p->resval.rstring, attrib) == 0)
				{
					str += gsc_msg(835);
					flag_attr = TRUE; 
				}
				else if (flag_attr)
						{
							// testo se l'attributo è di tipo 'data'
							if (pConn->IsDate(attrib_type) == GS_GOOD)
								{
									gsc_conv_DateTime_2_WinFmt(p->resval.rstring, Destination);
									str += Destination.get_name();
									flag_attr = FALSE;
								}
								else str += p->resval.rstring;
						}
						else str += p->resval.rstring;
				if ((p = p->rbnext)->restype == RTLE)
				{
					gsc_add_list(str);
					str.clear();
				}
				else str += _T("\t: ");
				break;
			case RTREAL:
				if (flag_attr) flag_attr = FALSE;
  				gsc_conv_Number(p->resval.rreal, Destination);
				str += Destination.get_name();
				if ((p = p->rbnext)->restype == RTLE)
				{
					gsc_add_list(str);
					str.clear();
				}
				else str += _T("\t: ");
   			break;
			case RTSHORT:
				str += gsc_tostring(p->resval.rint);
				if ((p = p->rbnext)->restype == RTLE)
				{
					gsc_add_list(str);
					str.clear();
				}
				else str += _T("\t: ");
				break;
			case RTLB: case RTLE:
				p = p->rbnext;
				break;
		}
	}
	ads_end_list();

	return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_RefreshRanges                                                     */
/*+
  Questa funzione, noto il numero di valori per riga da leggere da una lista di
  resbuf, compone la stringa relativa e la visualizza in una list-box nota. 

  Parametri:

  C_RB_LIST *current_values : lista dei valori di cui sopra;
  C_CLASS   *pCls: puntatore alla classe;
  TCHAR      *attrib : attributo selezionato;
  TCHAR      *attrib_type : tipologia attributo;
  int       valori : numero di valori per riga;
  ads_hdlg  dcl_id : identificativo del file di dcl;
  TCHAR      *dcl_tile : nome della list-box;
  int       interv : considerare anche il numero di intervallo/riga 
                     (default = TRUE);
  int       shift : valore di eventuale scorrimento in avanti della lsta valori
                    impostata (default = FALSE).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int gsc_RefreshRanges(C_RB_LIST *current_values, C_CLASS   *pCls, TCHAR *attrib, 
							 TCHAR *attrib_type,  int valori, ads_hdlg dcl_id, 
							 TCHAR *dcl_tile, int interv, int shift)
{
	C_STRING str, temp, spazio, Destination;
   C_DBCONNECTION *pConn = pCls->ptr_info()->getDBConnection(OLD);
	presbuf  p;
	int      cont = 1, num_interv = 1, limite, max_slider_value = 200, k, ls, flag_attr = FALSE;
   size_t   lung = 0;

	p = current_values->get_head();
	str.clear();
	limite = 7;
	spazio = _T("\t");

	if (interv)
	{
		str += gsc_tostring(cont);
		ls = (int) (8 - str.len());
		for (k = 1; k <= ls; k++) str += _T(" ");
	}
	
	ads_start_list(dcl_id, dcl_tile, LIST_NEW, 0);

	while (p)
	{
		switch (p->restype)
		{
			case RTSTR:
				temp = p->resval.rstring;
				// verifico il tipo di proprietà e setto i valori che servobno per l'"impaginazione"
				if ((gsc_strcmp(temp.get_name(), gsc_msg(632)) == 0) ||        // stile testo
					 (gsc_strcmp(temp.get_name(), gsc_msg(631)) == 0) ||    	   // tipo linea
					 (gsc_strcmp(temp.get_name(), gsc_msg(630)) == 0) ||   	   // riempimento
					 (gsc_strcmp(temp.get_name(), gsc_msg(633)) == 0) ||	      // piano
					 (gsc_strcmp(temp.get_name(), gsc_msg(826)) == 0))     	   // blocco
					 limite = 15;
				else if ((gsc_strcmp(temp.get_name(), gsc_msg(718)) == 0) ||	   // altezza testo
							(gsc_strcmp(temp.get_name(), gsc_msg(483)) == 0) ||	   // larghezza
							(gsc_strcmp(temp.get_name(), gsc_msg(629)) == 0) || 	   // colore
							(gsc_strcmp(temp.get_name(), gsc_msg(229)) == 0) || 	   // Colore riempimento
							(gsc_strcmp(temp.get_name(), gsc_msg(825)) == 0))  	   // scala
							limite = 7;
				else if ((gsc_strcmp(temp.get_name(), attrib) == 0) &&          // 'attributo scelto' di tipo 'data'
					      (pConn->IsDate(attrib_type) == GS_GOOD))                
						{
							limite = (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD) ? 18 : 31;
							flag_attr = TRUE;
						}
				else if (gsc_strcmp(temp.get_name(), attrib) == 0)                // 'attributo scelto'
					     limite = 15;
				else if (gsc_strcmp(temp.get_name(), gsc_msg(836)) == 0)
					     limite = 132;

      		// testo se l'attributo è di tipo 'data'
				if ((pConn->IsDate(attrib_type) == GS_GOOD) && flag_attr &&
                (gsc_strcmp(temp.get_name(), attrib) != 0))
				{
					 gsc_conv_DateTime_2_WinFmt(temp.get_name(), Destination);
                temp = Destination.get_name();
					 flag_attr = FALSE;
				}

   			if ((p = p->rbnext)->restype == RTLE)    
				{
					if (temp.len() >= (size_t) limite)
					{
						C_STRING comodo;

						comodo.set_name(temp.get_name(), 0, limite - 1);
						str += comodo.get_name();
						spazio = _T(" ");
					}
					else
					{
						str += temp.get_name();
						lung = temp.len();
						spazio.clear();
						for (size_t i = 0; i <= (limite - lung); i++) spazio += _T(" ");
					}
					if (cont < valori)
					{
                  str += spazio.get_name();
						cont++;
					}
					else 
					{
						cont = 1;
						lung = str.len();
						for (size_t j = 0; j <= (max_slider_value - lung); j++) str += _T(" ");
						if (shift > 0) str = str.get_name() +	shift - 1;		
						gsc_add_list(str);
						str.clear();
						if (interv)
						{ 
							num_interv++;
						   str += gsc_tostring(num_interv); 
            	      ls = (int) (8 - str.len());
		               for (k = 1; k <= ls; k++) str += _T(" ");
						}
					}
				}
				break;
			case RTREAL:
				gsc_conv_Number(p->resval.rreal, Destination);
				temp = Destination.get_name();
				if ((p = p->rbnext)->restype == RTLE)
				{
					str += temp.get_name();
					spazio.clear();
					lung = temp.len();
					for (size_t i = 0; i <= (limite - lung); i++) spazio += _T(" ");
					if (cont < valori)
					{
                  str += spazio.get_name();
						cont++;
					}
					else 
					{
						cont = 1;
						lung = str.len();
						for (size_t j = 0; j <= (max_slider_value - lung); j++) str += _T(" ");
						if (shift > 0) str = str.get_name() +	shift - 1;		
						gsc_add_list(str);
						str.clear();
						if (interv)
						{ 
							num_interv++;
						   str += gsc_tostring(num_interv); 
            	      ls = (int) (8 - str.len());
		               for (k = 1; k <= ls; k++) str += _T(" ");
						}
					}
				}
				break;
			case RTSHORT:
				temp = gsc_tostring(p->resval.rint);
				if ((p = p->rbnext)->restype == RTLE)
				{
					str += temp.get_name();
					spazio.clear();
					lung = temp.len();
					for (size_t i = 0; i <= (limite - lung); i++) spazio += _T(" ");
					if (cont < valori)
					{
                  str += spazio.get_name();
						cont++;
					}
					else 
					{
						cont = 1;
						lung = str.len();
               	for (size_t j = 0; j <= (max_slider_value - lung); j++) str += _T(" ");	 
						if (shift > 0) str = str.get_name() +	shift - 1;		
						gsc_add_list(str);
						str.clear();
						if (interv) 
						{
							num_interv++;
						   str += gsc_tostring(num_interv); 
            	      ls = (int) (8 - str.len());
		               for (k = 1; k <= ls; k++) str += _T(" ");
						}
					}
				}
				break;
			case RTLB: case RTLE:
				p = p->rbnext;
				break;
		}
	}
	ads_end_list();

	return GS_GOOD;
}
///////////////////
//Gestione Colore /
///////////////////
static int gsc_sel_color(C_STRING &value, C_COLOR *cur_val, ads_hdlg hdlg)
{
   C_COLOR color, CurLayerColor;

   ads_set_tile(hdlg, _T("Editvalue"), value.get_name());
   if (color.setString(value) == GS_BAD) return GS_BAD;

   CurLayerColor.setForeground();
   if (gsc_SetColorDialog(color, true, CurLayerColor) == GS_GOOD)
   {
      *cur_val = color;
		value.clear();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_validcolor                                                        */
/*+
  Questa funzione verifica la validità del colore espresso come stringa
  alfanumerica. 

  Parametri:

  C_STRING *value  : valore del colore espresso come stringa;
  int      *valint : valore di ritorno del colore espresso come intero
  int      flag    : flag che discerne se è stata lanciata la ACAD_COLORDLG di scelta colore
                    (default = FALSE).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_validcolor(C_STRING &value, C_COLOR &out, int flag)
{
   C_STRING val1, val2;
   int AutocadColorIndex;

   if (value.len() > 0)
   {
      AutocadColorIndex = value.toi();
      if (AutocadColorIndex == 0) 
      {
         if (value.comp(gsc_msg(419), FALSE) == 0) // "rosso"
            { out.setAutoCADColorIndex(1); return GS_GOOD; }      // rosso

         if (value.comp(gsc_msg(420), FALSE) == 0) // "giallo"
            { out.setAutoCADColorIndex(2); return GS_GOOD; }      // giallo

         if (value.comp(gsc_msg(421), FALSE) == 0) // "verde"
            { out.setAutoCADColorIndex(3); return GS_GOOD; }      // verde

         if (value.comp(gsc_msg(422), FALSE) == 0) // "ciano"
            { out.setAutoCADColorIndex(4); return GS_GOOD; }      // ciano

         if (value.comp(gsc_msg(423), FALSE) == 0) // "blu"
            { out.setAutoCADColorIndex(5); return GS_GOOD; }      // blu

         if (value.comp(gsc_msg(424), FALSE) == 0) // "magenta"
            { out.setAutoCADColorIndex(6); return GS_GOOD; }      // magenta

         if (value.comp(gsc_msg(425), FALSE) == 0) // "bianco"
            { out.setAutoCADColorIndex(7); return GS_GOOD; }      // bianco

         if (value.comp(gsc_msg(23), FALSE) == 0) // "DALAYER"
            { out.setByLayer(); return GS_GOOD; }      // DALAYER

         if (value.comp(gsc_msg(24), FALSE) == 0) // "DABLOCCO"
            { out.setByBlock(); return GS_GOOD; }      // DABLOCCO

         return GS_BAD;
      }
      else
		{	// controllo il valore del colore  
			if (AutocadColorIndex < 0 || AutocadColorIndex > 256)
			{
				if (flag == TRUE)	out.setByLayer(); // ho impostato la dcl di AUTOCAD di scelta colore
				else return GS_BAD;		            // segnalo l'eventuale errore di impostazione colore
			}
         else
            if (out.setAutoCADColorIndex(AutocadColorIndex) == GS_BAD)
				   if (flag == TRUE)	out.setByLayer(); // ho impostato la dcl di AUTOCAD di scelta colore
				   else return GS_BAD;		            // segnalo l'eventuale errore di impostazione colore
		}
   }
	else
	{
		if (flag == TRUE)	out.setByLayer(); // ho impostato la dcl di AUTOCAD di scelta colore
		else return GS_BAD;		           // segnalo l'eventuale errore di impostazione colore
	}

	return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_validvalue                                                        */
/*+
Questa funzione verifica la validità del valore immesso per un tematismo:
piano/scala/blocco/colore... ecc.
  alfanumerica. 

  Parametri:

  presbuf p    : puntatore alla lista valori (espressa come C_RB_LIST);
  TCHAR *attrib : attributo della classe scelto su cui applicare i vari tematismi;
  int *num_msg : numero del messaggio di eventuale errore.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_validvalue(presbuf p, TCHAR *attrib, int *num_msg)
{
	int tipo;

	while (p)
	{
		tipo = p->restype;
		switch(tipo)
		{
			case RTLB: case RTLE: case RTSHORT: case RTREAL:
				break;
			case RTSTR:
				if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) == 0)          // piano
            {
					if (gsc_validlayer(p->rbnext->resval.rstring) == GS_BAD)
					{ *num_msg = 800; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(826)) == 0)          // blocco
            {
					if (gsc_validblock(p->rbnext->resval.rstring) == GS_BAD)
					{ *num_msg = 801; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(632)) == 0)				// stile testo
            {
					if (gsc_validtextstyle(p->rbnext->resval.rstring) == GS_BAD)
					{ *num_msg = 802; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(825)) == 0)				// scala
            {
					if (gsc_validscale(p->rbnext->resval.rreal) == GS_BAD)
					{ *num_msg = 803; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(631)) == 0)				// tipo linea
            {
					if (gsc_validlinetype(p->rbnext->resval.rstring) == GS_BAD)
					{ *num_msg = 804; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(483)) == 0)				// larghezza
            {
					if (gsc_validwidth(p->rbnext->resval.rreal) == GS_BAD)
					{ *num_msg = 805; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)				// colore
            {
               C_COLOR color;

               if (color.setString(p->rbnext->resval.rstring) == GS_BAD)
                  { *num_msg = 806; return GS_CAN; } // "*Errore* Colore non valido."
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(718)) == 0)				// altezza testo
            {
					if (gsc_validhtext(p->rbnext->resval.rreal) == GS_BAD)
					{ *num_msg = 807; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(630)) == 0)				// riempimento
            {
					if (gsc_validhatch(p->rbnext->resval.rstring) == GS_BAD)
					{ *num_msg = 808; return GS_CAN; }
				}
				if (gsc_strcmp(p->resval.rstring, gsc_msg(229)) == 0)				// Colore riempimento
            {
               C_COLOR color;

               if (color.setString(p->rbnext->resval.rstring) == GS_BAD)
                  { *num_msg = 806; return GS_CAN; } // "*Errore* Colore non valido."
				}
				if (attrib) 
				{
					if (gsc_strcmp(p->resval.rstring, attrib) == 0)             //'attributo'
					switch (p->rbnext->restype)
					{
						case RTREAL:
							if (p->rbnext->resval.rreal == 0.0) { *num_msg = 287; return GS_CAN; }
							break;
						case RTSHORT:
							if (p->rbnext->resval.rint == 0) { *num_msg = 287; return GS_CAN; }
							break;
						case RTSTR:
							if (gsc_strcmp(p->rbnext->resval.rstring, GS_EMPTYSTR) == 0)
                        { *num_msg = 287; return GS_CAN; }
							break;
						case RTLB: case RTLE:
							return GS_BAD;   
					}
				}
				break;
		}
		p = p->rbnext;
	}
	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_sel_larg                                                        */
/*+
  Questa funzione permette di selezionare la larghezza tramite DCL.
  Parametri:
  double *current: valore impostato.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_sel_larg(double *current)
{      
   ads_hdlg llarg;
   int      dbstatus, dcl_file;
   C_STRING valori, path;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) == RTERROR) return GS_BAD;
                        
   if (ads_new_dialog(_T("larghezza"), dcl_file,NULLCB,&llarg) != RTNORM)
      { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   valori = *current;
   ads_set_tile(llarg, _T("editlarg"), valori.get_name());
   if (*current == 2.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg2"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else if (*current == 5.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg5"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else if (*current == 10.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg10"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else ads_set_tile(llarg, _T("radiolarg"), _T("largutente"));

   ads_action_tile(llarg, _T("larg_no"), (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(llarg, _T("larg_si"), (CLIENTFUNC)gs_acp_larg);
   ads_client_data_tile(llarg, _T("larg_si"), current);
   ads_action_tile(llarg, _T("radiolarg"), (CLIENTFUNC)gs_radiolarg);
   ads_client_data_tile(llarg, _T("radiolarg"), current);
   ads_action_tile(llarg, _T("editlarg"), (CLIENTFUNC)gs_editlarg);
   ads_client_data_tile(llarg, _T("editlarg"), current);

   ads_start_dialog(llarg, &dbstatus);

   return GS_GOOD;
}
//////////////////////////////////////////////////////
static void CALLB gs_can_dlg(ads_callback_packet *dcl)
{
    ads_done_dialog(dcl->dialog,DLGCANCEL);
}
//////////////////////////////////////////////////////
static void CALLB gs_acp_larg(ads_callback_packet *dcl)
{
   TCHAR  appo[100];
   double *value = (double*)(dcl->client_data);
   
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if (ads_get_tile(dcl->dialog, _T("editlarg"), appo, 100) != RTNORM) return;

   *value = _wtof(appo);

   if (*value >= 0) ads_done_dialog(dcl->dialog, DLGOK);
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(93));  // "*Errore* Larghezza non valida"
}
////////////////////////////////////////////////////////
static void CALLB gs_radiolarg(ads_callback_packet *dcl)
{
  C_STRING valori;
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  valori = dcl->value;

  if (gsc_strcmp(valori.get_name(), _T("larg2")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("larg5")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("larg10")) == 0) 
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_ENABLE);
}
////////////////////////////////////////////////////////
static void CALLB gs_editlarg(ads_callback_packet *dcl)
{
  double valore;
  TCHAR  valori[10];
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  valore = _wtof(dcl->value);

  if (valore >= 0)
  {
     swprintf(valori, 10, _T("%.3f"), valore);
     ads_set_tile (dcl->dialog, _T("editlarg"), valori);
     if (valore == 2.0)
     {
         ads_set_tile(dcl->dialog, _T("radiolarg"), _T("larg2"));
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
  else ads_set_tile(dcl->dialog, _T("error"), gsc_err(93));  // "*Errore* Larghezza non valida"
}
/*****************************************************************************/
/*.doc gsc_sel_htext                                                        */
/*+
  Questa funzione permette di selezionare la larghezza tramite DCL.
  Parametri:
  double *current: valore impostato.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_sel_htext(double *current)
{  
   ads_hdlg lhtext;
   int      dbstatus, dcl_file;
   C_STRING valori, path;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) == RTERROR) return GS_BAD;

   if (ads_new_dialog(_T("altesto"), dcl_file,NULLCB,&lhtext) != RTNORM)
      { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   valori = *current;
   ads_set_tile(lhtext, _T("edithtext"), valori.get_name());

   if (*current == 2.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext2"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else if (*current == 5.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext5"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else if (*current == 10.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext10"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else ads_set_tile(lhtext, _T("radiohtext"), _T("altesto"));

   ads_action_tile(lhtext, _T("htext_no"), (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(lhtext, _T("htext_si"), (CLIENTFUNC)gs_acp_htext);
   ads_client_data_tile(lhtext, _T("htext_si"), current);
   ads_action_tile(lhtext, _T("radiohtext"), (CLIENTFUNC)gs_radiohtext);
   ads_client_data_tile(lhtext, _T("radiohtext"), current);
   ads_action_tile(lhtext, _T("edithtext"), (CLIENTFUNC)gs_edithtext);
   ads_client_data_tile(lhtext, _T("edithtext"), current);

   ads_start_dialog(lhtext, &dbstatus);

   return GS_GOOD;
}
////////////////////////////////////////////////////////
static void CALLB gs_edithtext(ads_callback_packet *dcl)
{
   double   valore;
   C_STRING valori;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   valore = _wtof(dcl->value);

   if (valore > 0)
   {
      valori = valore;
      ads_set_tile (dcl->dialog, _T("edithtext"), valori.get_name());
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
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(95));  // "*Errore* Altezza testo non valida"
}
/////////////////////////////////////////////////////////
static void CALLB gs_radiohtext(ads_callback_packet *dcl)
{
  C_STRING valori;
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  valori = dcl->value;

  if (gsc_strcmp(valori.get_name(), _T("htext2")) == 0) 
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("htext5")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("htext10")) == 0) 
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_ENABLE);
}
////////////////////////////////////////////////////////
static void CALLB gs_acp_htext(ads_callback_packet *dcl)
{
  TCHAR  appo[100];
  double *value = (double*)(dcl->client_data);

  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  if (ads_get_tile(dcl->dialog, _T("edithtext"), appo, 100) != RTNORM) return;

  *value = _wtof(appo);

  if (*value > 0) ads_done_dialog(dcl->dialog,DLGOK);
  else ads_set_tile(dcl->dialog, _T("error"), gsc_err(95));  // "*Errore* Altezza testo non valida"
}


/*****************************************************************************/
/*.doc gsc_sel_scala                                                        */
/*+
  Questa funzione permette di selezionare la larghezza tramite DCL.
  Parametri:
  double *current: valore impostato.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_sel_scala(double *current)
{  
   ads_hdlg sscala;
   int      dbstatus, dcl_file;
   C_STRING valori, path;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) == RTERROR) return GS_BAD;

   if (ads_new_dialog(_T("scala1"), dcl_file, NULLCB, &sscala) != RTNORM)
      { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   valori = *current;
   ads_set_tile(sscala, _T("editscala"), valori.get_name());
   if (*current == 2.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact2"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else if (*current == 5.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact5"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else if (*current == 10.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact10"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else ads_set_tile(sscala, _T("radioscala"), _T("factutente"));

   ads_action_tile(sscala, _T("scal_no"), (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(sscala, _T("editscala"), (CLIENTFUNC)gs_editscala);
   ads_client_data_tile(sscala, _T("editscala"), current);
   ads_action_tile(sscala, _T("scal_si"), (CLIENTFUNC)gs_acp_scala);
   ads_client_data_tile(sscala, _T("scal_si"), current);
   ads_action_tile(sscala, _T("radioscala"), (CLIENTFUNC)gs_radioscala);
   ads_client_data_tile(sscala, _T("radioscala"), current);

   ads_start_dialog(sscala, &dbstatus);

	return GS_GOOD;
}
////////////////////////////////////////////////////////
static void CALLB gs_editscala(ads_callback_packet *dcl)
{
   double   valore;
   C_STRING valori;
   
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   
   valore = _wtof(dcl->value);

   if (valore > 0)
   {
      valori = valore;
      ads_set_tile (dcl->dialog, _T("editscala"), valori.get_name());
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
      ads_set_tile(dcl->dialog, _T("error"), gsc_err(94));  // "*Errore* Scala non valida"
}
/////////////////////////////////////////////////////////
static void CALLB gs_radioscala(ads_callback_packet *dcl)
{
  C_STRING valori;
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  valori = dcl->value;

  if (gsc_strcmp(valori.get_name(), _T("fact2")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editscala"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("fact5")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editscala"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori.get_name(), _T("fact10")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editscala"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("editscala"), MODE_ENABLE);

}
////////////////////////////////////////////////////////
static void CALLB gs_acp_scala(ads_callback_packet *dcl)
{
  TCHAR appo[100];
  double *value = (double*)(dcl->client_data);

  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  if (ads_get_tile(dcl->dialog, _T("editscala"), appo, 100) != RTNORM) return;

  *value = _wtof(appo);

  if (*value > 0) ads_done_dialog(dcl->dialog,DLGOK);   
  else ads_set_tile(dcl->dialog, _T("error"), gsc_err(94));  // "*Errore* Scala non valida"
}
/*****************************************************************************/
/*.doc gsc_scel_value                                                        */
/*+
  Questa funzione permette di selezionare un 'valore' per l'attributo scelto.
  Parametri:
  int       prj: codice progetto;
  int       cls: codice classe;
  int       sub: codice sottoclasse;
  C_STRING  &AttrName: nome attributo;
  TCHAR     *attrib_type: tipologia attributo;
  C_STRING  *value: valore impostato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_scel_value(int prj, int cls, int sub, C_STRING &AttrName, 
								  TCHAR *attrib_type, C_STRING *value)
{
	C_STRING result, Title;

   result.set_name(value->get_name());

   if (prj > 0 && cls > 0)
   {
		// ricavo il puntatore alla classe
      C_CLASS *pCls = gsc_find_class(prj, cls, sub);
      C_DBCONNECTION *pConn = pCls->ptr_info()->getDBConnection(OLD);
 
      if (pCls)
		{
         C_ATTRIB *pAttrib;
         C_STRING dummy;

			Title = gsc_msg(230); //  "Classe: "
         pCls->get_CompleteName(dummy);
         Title += dummy;
			Title += _T(" - ");
			Title += AttrName.get_name();
         if (gsc_dd_sel_uniqueVal(prj, cls, sub, 0,
                                  AttrName, result, Title) != GS_GOOD) return GS_BAD;

         if ((pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(AttrName.get_name(), FALSE)) == NULL)
            return GS_BAD;
         pAttrib->init_ADOType(pCls->ptr_info()->getDBConnection(OLD));

			// formatto il valore immesso dall'utente
			if (pConn->IsCurrency(attrib_type) == GS_GOOD) 
			{
				double MapCurr;
				C_STRING CurrencyDest;

				gsc_conv_Currency(result.get_name(), &MapCurr);
     			gsc_conv_Currency(MapCurr, CurrencyDest);
				result.set_name(CurrencyDest.get_name());
			}
			else if (pConn->IsNumeric(attrib_type) == GS_GOOD) 
			{
    			C_STRING NumberDest;

				gsc_conv_Number(_wtof(result.get_name()), NumberDest);
				result.set_name(NumberDest.get_name());
			}
			else if (pConn->IsBoolean(attrib_type) == GS_GOOD) 
			{
				int MapBool;
				C_STRING BoolDest;

   			gsc_conv_Bool(result.get_name(), &MapBool);
				gsc_conv_Bool(MapBool, BoolDest);
				result.set_name(BoolDest.get_name());
			}
         /*if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
         {  // Aggiungo apici perchè è di tipo carattere
            C_STRING dummy;

            dummy = _T("'");
            dummy += result;
            dummy += _T("'");
            result = dummy;
         }*/
      }
		else return GS_BAD;
   }
	// messaggio errore ?

   if (result.len() > 0) value->set_name(result.get_name());

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_scel_multiple_value                                               */      
/*+
  Questa funzione permette di selezionare un 'valore' per l'attributo scelto.
  Parametri:
  int        prj: codice progetto;
  int        cls: codice classe;
  int        sub: codice sottoclasse;
  C_STRING   &AttrName: nome attributo;
  C_STR_LIST *values: valori impostati.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_scel_multiple_value(int prj, int cls, int sub, 
                                   C_STRING &AttrName, C_STR_LIST *values)
{
   if (prj > 0 && cls > 0)
   {
		// ricavo il puntatore alla classe
      C_CLASS *pCls = gsc_find_class(prj, cls, sub);

      if (pCls)
		{
         if (gsc_dd_sel_multipleVal(pCls, AttrName, values) != GS_GOOD) return GS_BAD;
		}
		else return GS_BAD;	
   }
	// messaggio di errore ?

	return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_ddLegendImpost                                                    */
/*+
  Questa funzione imposta un'insieme di range per l'attributo della classe
  scelta mediante finestra DCL.
  Parametri:
  Common_Dcl_legend_Struct &CommonStruct : struttura dei dati interessati

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddLegendImpost(Common_Dcl_legend_Struct *CommonStruct)
{
	int       flag_leg_old, num_msg, status, dcl_file, ret = GS_GOOD;
   double    dimx, dimy, dim, simboffset, etichoffset;
   C_RB_LIST Legend_param_old;
	C_STRING  layer, text_style, riga;
	ads_hdlg  dcl_id;
	ads_point ins_point;

 	dcl_file = CommonStruct->dcl_file;

   if ((CommonStruct->FlagLeg == 0) && (CommonStruct->Legend_param.GetCount() == 0))
   {
	   ins_point[X] = ins_point[Y] = 0.0;
      dim = dimx = dimy = simboffset = etichoffset = 0.500;
	   layer = DEFAULT_LAYER;
      text_style = DEFAULT_TEXTSTYLE;
   }
   else
   {
      // recupero i valori della legenda già impostati
      presbuf punt;

      punt = CommonStruct->Legend_param.get_head();
      while (punt)
      {
         if (punt->restype == RTSTR)
         {
          	if (gsc_strcmp(punt->resval.rstring, gsc_msg(633)) == 0)        // Piano
               layer = (punt->rbnext)->resval.rstring;
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(814)) == 0)   // Punto inserimento
               ads_point_set((punt->rbnext)->resval.rpoint, ins_point);
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(815)) == 0)   // DimX simboli
               dimx = (punt->rbnext)->resval.rreal; 
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(816)) == 0)   // DimY simboli
               dimy = (punt->rbnext)->resval.rreal;
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(817)) == 0)   // Offset simboli
               simboffset = (punt->rbnext)->resval.rreal;
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(818)) == 0)   // Dim. etichette
               dim = (punt->rbnext)->resval.rreal;
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(632)) == 0)   // Stile testo
               text_style = (punt->rbnext)->resval.rstring;
          	else if (gsc_strcmp(punt->resval.rstring, gsc_msg(819)) == 0)   // Offset etichette
               etichoffset = (punt->rbnext)->resval.rreal;
         }
         punt = punt->rbnext;
      }
		if ((ins_point[X] == 0.0) && (ins_point[Y] == 0.0)) CommonStruct->ins_leg = FALSE;
		else CommonStruct->ins_leg = TRUE;
   }

   // copio i valori della legenda in una C_RB_LIST di comodo per riprisyinarli nel 
   // caso di annulla da imposta legenda
   CommonStruct->Legend_param.copy(Legend_param_old);
   flag_leg_old = CommonStruct->FlagLeg;

   while (1)
   {
		status = DLGOK;
      riga = gsc_msg(632);
      riga += _T(": ");
      riga += text_style.get_name();

      ads_new_dialog(_T("legend"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; break; }

      ads_action_tile(dcl_id, _T("Createlegend"), (CLIENTFUNC) dcl_ImpostLegend_Create);
      ads_client_data_tile(dcl_id, _T("Createlegend"), &CommonStruct->FlagLeg);
		ads_set_tile(dcl_id, _T("Createlegend"), gsc_tostring(CommonStruct->FlagLeg));

      ads_action_tile(dcl_id, _T("Createonlayer"), (CLIENTFUNC) dcl_ImpostLegend_Onlayer);
      ads_client_data_tile(dcl_id, _T("Createonlayer"), &layer);
   	ads_set_tile(dcl_id, _T("Createonlayer"), layer.get_name());

      ads_action_tile(dcl_id, _T("Layer"), (CLIENTFUNC) dcl_ImpostLegend_Layer);
      ads_client_data_tile(dcl_id, _T("Layer"), &layer);

      ads_action_tile(dcl_id, _T("Ante"), (CLIENTFUNC) dcl_ImpostLegend_Ante);
      ads_client_data_tile(dcl_id, _T("Ante"), CommonStruct);

      ads_action_tile(dcl_id, _T("Post"), (CLIENTFUNC) dcl_ImpostLegend_Post);
      ads_client_data_tile(dcl_id, _T("Post"), CommonStruct);

		if (CommonStruct->ins_leg)
		{
			ads_set_tile(dcl_id,	_T("Ante"), _T("1"));
			ads_mode_tile(dcl_id, _T("XPoint"), MODE_ENABLE);
			ads_mode_tile(dcl_id, _T("YPoint"), MODE_ENABLE);
			ads_mode_tile(dcl_id, _T("Select"), MODE_ENABLE);
		}
      else
		{
			ads_set_tile(dcl_id, _T("Post"), _T("1"));
			ads_mode_tile(dcl_id, _T("XPoint"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("YPoint"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Select"), MODE_DISABLE);
		}

      ads_action_tile(dcl_id, _T("XPoint"), (CLIENTFUNC) dcl_ImpostLegend_XPoint);
      ads_client_data_tile(dcl_id, _T("XPoint"), &ins_point[X]);
   	ads_set_tile(dcl_id, _T("XPoint"), gsc_tostring(ins_point[X], 15, 4));

      ads_action_tile(dcl_id, _T("YPoint"), (CLIENTFUNC) dcl_ImpostLegend_YPoint);
      ads_client_data_tile(dcl_id, _T("YPoint"), &ins_point[Y]);
   	ads_set_tile(dcl_id, _T("YPoint"), gsc_tostring(ins_point[Y], 15, 4));

      ads_action_tile(dcl_id, _T("Select"), (CLIENTFUNC) dcl_ImpostLegend_Select);

      ads_action_tile(dcl_id, _T("DimX"), (CLIENTFUNC) dcl_ImpostLegend_DimX);
      ads_client_data_tile(dcl_id, _T("DimX"), &dimx);           
   	ads_set_tile(dcl_id, _T("DimX"), gsc_tostring(dimx, 8, 4));

      ads_action_tile(dcl_id, _T("DimY"), (CLIENTFUNC) dcl_ImpostLegend_DimY);
      ads_client_data_tile(dcl_id, _T("DimY"), &dimy);
   	ads_set_tile(dcl_id, _T("DimY"), gsc_tostring(dimy, 8, 4));

      ads_action_tile(dcl_id, _T("Simboffset"), (CLIENTFUNC) dcl_ImpostLegend_Simboffset);
      ads_client_data_tile(dcl_id, _T("Simboffset"), &simboffset);
     	ads_set_tile(dcl_id, _T("Simboffset"), gsc_tostring(simboffset, 8, 4));

      ads_action_tile(dcl_id, _T("Dim"), (CLIENTFUNC) dcl_ImpostLegend_Dim);
      ads_client_data_tile(dcl_id, _T("Dim"), &dim);
   	ads_set_tile(dcl_id, _T("Dim"), gsc_tostring(dim, 8, 4));

   	ads_set_tile(dcl_id, _T("Stile"), riga.get_name());

      ads_action_tile(dcl_id, _T("Liststili"), (CLIENTFUNC) dcl_ImpostLegend_Liststili);
      ads_client_data_tile(dcl_id, _T("Liststili"), &text_style);

      ads_action_tile(dcl_id, _T("Etichoffset"), (CLIENTFUNC) dcl_ImpostLegend_Etichoffset);
      ads_client_data_tile(dcl_id, _T("Etichoffset"), &etichoffset);
     	ads_set_tile(dcl_id, _T("Etichoffset"), gsc_tostring(etichoffset, 8, 4));

      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_ImpostLegend_Help);
      
		if (CommonStruct->FlagLeg == 0)
		{ 
			ads_mode_tile(dcl_id, _T("Createlegend"), MODE_ENABLE);
			ads_mode_tile(dcl_id, _T("Createonlayer"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Layer"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Ante"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Post"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("XPoint"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("YPoint"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Select"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Simbinbox"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("DimX"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("DimY"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Simboffset"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Liststili"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Dim"), MODE_DISABLE);
			ads_mode_tile(dcl_id, _T("Etichoffset"), MODE_DISABLE);
		}
      if (ret == GS_CAN) ads_set_tile(dcl_id, _T("error"), gsc_msg(num_msg));
      else ads_set_tile(dcl_id, _T("error"), GS_EMPTYSTR);
		
      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                  // uscita con tasto OK
      { 	
         // verifico se esisono valori non accettabili
   		if (gsc_validlayer(layer.get_name()) == GS_BAD)
         { 
            num_msg = 800;      // "*Errore* Piano non valido."
            ret = GS_CAN;
            continue;
         }
         else if ((dimx <= 0.0) || (dimy <= 0.0) || 
                  (simboffset <= 0.0) || (etichoffset <= 0.0)) 
              {
                 num_msg = 287; // "*Errore* Valore non valido."
                 ret = GS_CAN;
                 continue;
              }
         // salvo i valori della legenda impostati
			if (!CommonStruct->ins_leg) { ins_point[X] = ins_point[Y] = 0.0; }
         if (gsc_SaveLegParam(CommonStruct->Legend_param, CommonStruct->FlagLeg, 
                              layer.get_name(), ins_point, dimx, dimy, simboffset,
                              dim, text_style.get_name(), etichoffset) == GS_BAD) 
            { ret = GS_BAD; break; }
         ret = GS_GOOD; 
         break;
      }
      else if (status == DLGCANCEL)         // uscita con tasto CANCEL
           { 
              CommonStruct->Legend_param.remove_all();
              Legend_param_old.copy(CommonStruct->Legend_param);
              CommonStruct->FlagLeg = flag_leg_old;
              break;
           }
      else if (status == PNT_LEG_SEL)       // uscita con tasto seleziona punto
      { if (gsc_selpoint(&ins_point) == GS_BAD) {ret = GS_BAD; break; } }

	}
	ads_done_dialog(dcl_id, status);

   return ret;
}

/*****************************************************************************/
/*.doc gsc_selpoint                                                     */
/*+
  Questa mini funzione permette la selezione di un punto a video dopo aver
  chiuso la DCL attiva.
  Parametri:
  ads_point *point : puntatore al punto espresso come punto 3D modalità ads.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_selpoint(ads_point *point)
{
   C_STRING msg;

   msg = GS_LFSTR;
   msg += gsc_msg(814); // "Punto inserimento"

   if ((acedGetPoint(NULL, msg.get_name(), *point)) == RTERROR)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}
/*****************************************************************************/
/*.doc gsc_SaveLegParam                                                     */
/*+
  Questa funzione salva i 'valori di impostazione della legenda.
  Parametri:
  C_RB_LIST *leg_param : lista che conterrà i valori impostati;
  int       impleg : flag di inserimento legenda 
  TCHAR     *layer : nome del piano su cui posizionare la legenda;
  ads_point *ins_pnt : punto do inserimento legenda;
  double    dimx : dimensione X dei simboli;
  double    dimy : dimensione Y dei simboli;
  double    simboffset : offset fra i simboli in legenda;
  double    dim : dimensione delle scritte legenda;
  TCHAR     *style : stile delle scritte legenda;
  double    etichoffset : offset fra le scritte dell legenda.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_SaveLegParam(C_RB_LIST &leg_param, int impleg, TCHAR *layer, ads_point ins_pnt, 
                     double dimx, double dimy, double simboffset, double dim, 
                     TCHAR *style, double etichoffset)

{
   C_RB_LIST parametri;

   if ((parametri += acutBuildList(RTLB, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(821), RTSHORT, impleg, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(633), RTSTR, layer, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(814), RTPOINT, ins_pnt, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(815), RTREAL, dimx, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(816), RTREAL, dimy, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(817), RTREAL, simboffset, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(818), RTREAL, dim, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(632), RTSTR, style, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLB, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTSTR, gsc_msg(819), RTREAL, etichoffset, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((parametri += acutBuildList(RTLE, RTLE, 0)) == NULL)
   { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   parametri.copy(leg_param);

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_dd_sel_multipleVal <external> */
/*+                                                                       
  Permette la selezione di un valore di un attributo da una lista di
  valori esistenti nella tabella.
  Parametri:
  int        prj;       codice progetto
  C_CLASS    *pCls;     puntatore alla classe
  C_STRING   &AttrName; Nome attributo
  C_STR_LIST &result;   lista valori scelti

  oppure:
  C_STRING      &Title;  intestazione dcl
  _RecordsetPtr &pRs;    puntatore al recordset 
  C_STR_LIST    &result; lista valori scelti

  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
-*/  
/*********************************************************/
int gsc_dd_sel_multipleVal(C_CLASS *pCls, C_STRING &AttrName, C_STR_LIST *result)
{
   C_STRING       statement, Title, TableRef, AdjAttribName, dummy;
	C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;

   pConn = pCls->ptr_info()->getDBConnection(OLD);
   TableRef = pCls->ptr_info()->OldTableRef;

   AdjAttribName = AttrName;
   if (gsc_AdjSyntax(AdjAttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement = _T("SELECT DISTINCT ");
   statement += AdjAttribName;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");
   statement += AdjAttribName;

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   Title = gsc_msg(230); //  "Classe: "
   pCls->get_CompleteName(dummy);
	Title += dummy;
   Title += _T(" - ");
   Title += AttrName.get_name();

   return gsc_dd_sel_multipleVal(Title, pRs, result);
}
///////////////////////////////////////////////////////////////////////////////
int gsc_dd_sel_multipleVal(C_STRING &Title, _RecordsetPtr &pRs, C_STR_LIST *result)
{
   C_STRING   path;
   TCHAR      *msg = NULL;
   ads_hdlg   dcl_id;
   C_RB_LIST  ColValues;
   presbuf    p;
   int        res = GS_GOOD, status, dcl_file;
   C_STR      *StrVal;
	Common_Val_Sel Common_Val;

   result->remove_all();

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;
   p = gsc_nth(1, ColValues.nth(0));

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // leggo riga
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      if (p->restype != RTNONE && p->restype != RTVOID && p->restype != RTNIL)
      {
         if ((msg = gsc_rb2str(p)) == NULL) { res = GS_BAD; break; }

         if ((StrVal = new C_STR(msg)) == NULL)
            { free(msg); GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }
	      Common_Val.StrValList.add_tail(StrVal);
         free(msg);
      }
      gsc_Skip(pRs);
   }
   if (res == GS_BAD) return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("AttrMultipleValues"), dcl_file, (CLIENTFUNC)NULLCB, &dcl_id);
   if (!dcl_id)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;
   }

   res = GS_GOOD;
   ads_start_list(dcl_id, _T("ValuesList"), LIST_NEW, 0);
   StrVal = (C_STR *) Common_Val.StrValList.get_head();
   while (StrVal)
   {
      gsc_add_list(StrVal->get_name());
      StrVal = (C_STR *) Common_Val.StrValList.get_next();
   }
   ads_end_list();

   Common_Val.StrValList.get_head(); // posiziono il cursore sul primo elemento
   ads_set_tile(dcl_id, _T("title"), Title.get_name());
   ads_set_tile(dcl_id, _T("ValuesList"), _T("0"));
   ads_mode_tile(dcl_id, _T("ValuesList"), MODE_SETFOCUS); 
   ads_action_tile(dcl_id, _T("ValuesList"), (CLIENTFUNC) dcl_AttrMultipleValues_ValuesList);
   ads_client_data_tile(dcl_id, _T("ValuesList"), &Common_Val);
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_AttrMultipleValues_Help);
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_AttrMultipleValues_accept);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGCANCEL) res = GS_CAN;
	else Common_Val.SelValList.copy(result);

   ads_unload_dialog(dcl_file);

   return res;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "StrValList" su controllo "ValuesList"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrMultipleValues_ValuesList(ads_callback_packet *dcl)
{
	Common_Val_Sel *Commonstruct;

	Commonstruct = (Common_Val_Sel *)(dcl->client_data);

   if (dcl->reason == CBR_SELECT)
   {
		C_NODE   *ptr = NULL;
		C_INT_LIST ListaSel;
		C_INT      *pSel;
		C_STR      *pStr;
		
		ListaSel.FromDCLMultiSel(dcl->value);
		(Commonstruct->SelValList).remove_all();

		pSel = (C_INT *) ListaSel.get_head();
		while (pSel)
		{
			ptr = (Commonstruct->StrValList).goto_pos(pSel->get_key() + 1);
			if ((pStr = new C_STR(ptr->get_name())) == NULL) { GS_ERR_COD = eGSOutOfMem; return; } 
			(Commonstruct->SelValList).add_tail(pStr);
			pSel = (C_INT *) ListaSel.get_next();
		}
   }
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "AttrMultipleValues" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrMultipleValues_Help(ads_callback_packet *dcl)
   { gsc_help(IDH_Modalitoperative); } 

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "AttrMultipleValues" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_AttrMultipleValues_accept(ads_callback_packet *dcl)
{ 
	ads_done_dialog(dcl->dialog, DLGOK);
}

/*****************************************************************************/
/*.doc gsc_evid4thm <external> */
/*+
  Questa funzione evidenzia gli oggetti grafici di una classe in base a criteri
  scelti tramite interfaccia DCL.
  Parametri:
  C_CLASS *p_class  : puntatore alla classe madre degli oggetti da evidenziare
  C_SELSET &selset  : gruppo di oggetti da evidenziare 
  C_FAS    EvidFas  : fas impostata per l'evidenziazione;
  long     flag_set : valore del flag che tiene conto dei tematismi di evidenziazione
                      scelti;
  int      n_int    : numero progressivo di intervallo in elaborazione.

  Restituisce RTERROR in caso di errore altrimenti RTNORM.
-*/  
/******************************************************************************/
int gsc_evid4thm(C_CLASS *p_class, C_SELSET &selset, C_FAS *EvidFas, 
                 long flag_set, int n_int)
{
	int        ret = GS_GOOD, j;
   C_COLOR    color;
   C_STR_LIST LayerNameList;
   C_STRING   name_layer;
   C_RB_LIST  layer;
   TCHAR      LineType[MAX_LEN_LINETYPENAME]; 

	gsc_startTransaction();
   do
   {
      if (GS_CURRENT_WRK_SESSION->get_level() == GSUpdateableData && 
          p_class->ptr_id()->abilit == GSUpdateableData)
      {
         C_STRING path;

         // memorizzo in un gruppo di selezione gli oggetti con FAS modificata
         path = GS_CURRENT_WRK_SESSION->get_dir();
         path += _T('\\');
         path += GEOTEMPSESSIONDIR;
         path += _T('\\');
         path += GS_ALTER_FAS_OBJS_FILE;
         GS_ALTER_FAS_OBJS.add_selset(selset, GS_GOOD, path.get_name()); 
      }

      // se si vuole evidenziare per riempimento, questo non può essere effettuato
      // da "gsc_modifyEntToFas" che inserendo dei riempimenti associativi legati
      // a DB cambierebbero i fattori di aggregazione (con fattore di scala relativo
      // alla scala precedente)
      if (gsc_modifyEntToFas(selset, EvidFas, flag_set, FALSE) == GS_BAD) { ret = GS_BAD; break; }

      // Cambio il tipo di riempimento (solo per polilinee chiuse)
      if ((p_class->get_type() == TYPE_SURFACE) && 
          ((flag_set & GSHatchNameSetting) || (flag_set & GSBlockScaleSetting)))
      {
         C_COLOR old_col;
         double  scale = 1, rotation = 0;
         TCHAR   old_layer[MAX_LEN_LAYERNAME];

         if (flag_set & GSLayerSetting)   // cambia piano
            if (gsc_set_CurrentLayer(EvidFas->layer, old_layer) == GS_BAD) { ret = GS_BAD; break; }
      
         if (flag_set & GSColorSetting) // se si deve impostare anche il colore
            if (gsc_set_CurrentColor(EvidFas->color, &old_col) == GS_BAD)
            {
               if (flag_set & GSLayerSetting) gsc_set_CurrentLayer(EvidFas->layer, old_layer);
                  { ret = GS_BAD; break; }
            }

         do
         {                                                
            // se si deve impostare anche la scala
            if (flag_set & GSBlockScaleSetting) scale = EvidFas->block_scale;
            // se si deve impostare anche la rotazione
            if (flag_set & GSRotationSetting) rotation = EvidFas->rotation;
      
            j = 0;
            color.setAutoCADColorIndex(7);

            // leggo la lista dei layer attuale
            if (gsc_getActualLayerNameList(LayerNameList) == GS_BAD) return GS_BAD;
            // ordino la lista degli elementi per nome
            LayerNameList.sort_name(TRUE, TRUE); // sensitive e ascending

            // se non ho scelto tutti i tematismi creo il layer con il colore 
            // della classe scelta al primo intervallo, altrimenti ne creo uno ogni intervallo
            if (flag_set != (GSLayerSetting + GSBlockScaleSetting + GSLineTypeSetting +
                             GSWidthSetting + GSColorSetting + GSHatchNameSetting)) // tutti i tematismi
            {
               // ricavo il colore del piano della classe
               if (gsc_get_charact_layer(p_class->get_name(), LineType, &color) == GS_BAD)
				      { ret = GS_BAD; break; }

               if (n_int == 1)      
               {
                  while (TRUE)
                  {
                     if (flag_set & GSLayerSetting) name_layer = EvidFas->layer;
                     else name_layer = p_class->get_name();
                     name_layer += j;
                     if (LayerNameList.search_name(name_layer.get_name(), FALSE) == NULL) break;
                     j++;
                  }
                  // viene creato e settato con le caratteristiche
                  if (gsc_set_charact_layer(name_layer.get_name(), NULL, &color) == GS_BAD)
                     { ret = GS_BAD; break; } 
               }
               else
               {
                  while (TRUE)
                  {
                     if (flag_set & GSLayerSetting) name_layer = EvidFas->layer;
                     else name_layer = p_class->get_name();
                     name_layer += j;
                     if (LayerNameList.search_name(name_layer.get_name(), FALSE) == NULL)
                     {
                        if (flag_set & GSLayerSetting) name_layer = EvidFas->layer;
                        else name_layer = p_class->get_name();
                        j--;
                        name_layer += j;
                        break;
                     }
                     j++;
                  }
               }
            }
            else
            {
               while (TRUE)
               {
                  if (flag_set & GSLayerSetting) name_layer = EvidFas->layer;
                  else name_layer = p_class->get_name();
                  name_layer += j;
                  if (LayerNameList.search_name(name_layer.get_name(), FALSE) == NULL) break;
                  j++;
               }
               // viene creato e settato con le caratteristiche
               if (gsc_set_charact_layer(name_layer.get_name(), NULL, &color) == GS_BAD)
                  { ret = GS_BAD; break; } 
            }
            // setto il piano come corrente
            if ((layer << acutBuildList(RTSTR, name_layer.get_name(), 0)) == NULL)
               { ret = GS_BAD; break; }       
            if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
               { GS_ERR_COD = eGSAdsCommandErr; ret = GS_BAD; break; } 

            if (gsc_setHatchSS(selset, EvidFas->hatch, EvidFas->hatch_scale,
                               EvidFas->hatch_rotation, &EvidFas->hatch_color,
                               EvidFas->hatch_layer) == GS_BAD)
				   { ret = GS_BAD; break; }
         }
         while (0);
      
         if (ret == GS_BAD) break;

         // setta layer precedente
         if (flag_set & GSLayerSetting) gsc_set_CurrentLayer(old_layer);
         // setta colore precedente
         if (flag_set & GSColorSetting) gsc_set_CurrentColor(old_col);
      }
   }
	while (0);

   if (ret == GS_BAD) gsc_abortTransaction();
   else gsc_endTransaction();
		
	return ret;
}


/*****************************************************************************/
/*.doc gsc_filt4thm <external> */
/*+
  Questa funzione evidenzia gli oggetti grafici di una classe in base a criteri
  scelti tramite interfaccia DCL.
  Parametri:
  Common_Dcl_legend_Struct *CommonStruct4Thm : struttura dei dati per il 
                           filtro tematico (range, tematismo, ecc.)  ;
  Common_Dcl_main_filter_Struct *CommonStructCondition : struttura delle condizioni
                           (spaziali, di insieme ecc.);
  C_LINK_SET *pLinkSet;       1) Se la classe di entità ha grafica rappresenta 
                                 il gruppo di selezione degli oggetti su cui applicare il filtro;
                              2) se la classe di entità non ha grafica rappresenta
                                 la lista dei codici delle entità su cui applicare il filtro;
                              se = NULL vuol dire che il filtro verrà applicato 
                              su tutto il territorio estratto

  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
int gsc_filt4thm(Common_Dcl_legend_Struct *CommonStruct4Thm, 
					  Common_Dcl_main_filter_Struct *CommonStructCondition, C_LINK_SET *pLinkSet)
{
	int      valint = 0, ret = GS_GOOD, passo, iniz, num_thm = 1;
	presbuf  q, r;
   C_STRING cond_sql, val_color;
	C_CLASS  *pCls;
   C_FAS    FasEvid;
	C_DBCONNECTION *pConn;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opFilterEntity) == GS_BAD) return GS_BAD;

   if ((pCls = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(),
		                        CommonStruct4Thm->cls, CommonStruct4Thm->sub)) == NULL) return GS_BAD;

   if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(833)) != 0)	// un solo tematismo
   { passo = 14; iniz = 9; }
	else																						// più tematismi
	{ passo = 12 + (CommonStruct4Thm->thm_list.get_count() - 1) * 4; iniz = passo - 5 ; }

	// per ogni intervallo ricavo la condizione sql
	for (int k = 1; k <= CommonStruct4Thm->n_interv; k++)
	{
      // Correggo la sintassi del nome del campo per SQL MAP
      cond_sql = CommonStruct4Thm->attrib_name;
      gsc_AdjSyntaxMAPFormat(cond_sql);

		// a seconda del tipo di attributo scelto imposto la condizione 
		pConn = pCls->ptr_info()->getDBConnection(OLD);

		if (pConn->IsCurrency(CommonStruct4Thm->attrib_type.get_name()) == GS_GOOD)
		{
         double MapCurr;

			if (CommonStruct4Thm->type_interv == 1)             // intervalli continui
			{
				if (k == 1)                                      // primo intervallo
				{
					q = CommonStruct4Thm->range_values.getptr_at(iniz);
               if (gsc_conv_Currency(q->resval.rstring, &MapCurr) == GS_BAD) return GS_BAD;
					cond_sql += _T(" <= "); 
					cond_sql += MapCurr;
				}
				else if (k <= CommonStruct4Thm->n_interv)		    // intervalli successivi
				{
               if (gsc_conv_Currency(q->resval.rstring, &MapCurr) == GS_BAD) return GS_BAD;
					cond_sql += _T(" > ");
					cond_sql += MapCurr;
					for (int l = 1; l <= passo; l++) q = q->rbnext;
  					cond_sql += _T(" AND ");
					cond_sql += CommonStruct4Thm->attrib_name.get_name();
               if (gsc_conv_Currency(q->resval.rstring, &MapCurr) == GS_BAD) return GS_BAD;
					cond_sql += _T(" <= ");
					cond_sql += MapCurr;
				}
			}
			else 																 // intervalli discreti
			{
				if (k == 1) q = CommonStruct4Thm->range_values.getptr_at(iniz);
				else { for (int l = 1; l <= passo; l++) q = q->rbnext; }
				if (gsc_conv_Currency(q->resval.rstring, &MapCurr) == GS_BAD) return GS_BAD;
				cond_sql += _T(" = "); 
				cond_sql += MapCurr;
			}
		}
		else if (pConn->IsNumeric(CommonStruct4Thm->attrib_type.get_name()) == GS_GOOD)
		{
			if (CommonStruct4Thm->type_interv == 1)             // intervalli continui
			{
				if (k == 1)                                      // primo intervallo
				{
					q = CommonStruct4Thm->range_values.getptr_at(iniz);
					cond_sql += _T(" <= "); 
					cond_sql += q->resval.rreal;
				}
				else if (k <= CommonStruct4Thm->n_interv)		    // intervalli successivi
				{
					cond_sql += _T(" > ");
					cond_sql += q->resval.rreal;
					for (int l = 1; l <= passo; l++) q = q->rbnext;
  					cond_sql += _T(" AND ");
					cond_sql += CommonStruct4Thm->attrib_name.get_name();
					cond_sql += _T(" <= ");
					cond_sql += q->resval.rreal;
				}
			}
			else 																 // intervalli discreti
			{
				if (k == 1) q = CommonStruct4Thm->range_values.getptr_at(iniz);
				else { for (int l = 1; l <= passo; l++) q = q->rbnext; }
				cond_sql += _T(" = "); 
				cond_sql += q->resval.rreal;
			}
		}
		else if (pConn->IsChar(CommonStruct4Thm->attrib_type.get_name()) == GS_GOOD)
		{
			if (CommonStruct4Thm->type_interv == 1)              // intervalli continui
			{
				if (k == 1)                                       // primo intervallo
				{
					q = CommonStruct4Thm->range_values.getptr_at(iniz);
					cond_sql += _T(" <= '"); 
					cond_sql += q->resval.rstring;
               cond_sql += _T("'"); 
				}
				else if (k <= CommonStruct4Thm->n_interv)		    // intervalli successivi
				{
					cond_sql += _T(" > '");
					cond_sql += q->resval.rstring;
               cond_sql += _T("'"); 
					for (int l = 1; l <= passo; l++) q = q->rbnext;
  					cond_sql += _T(" AND ");
					cond_sql += CommonStruct4Thm->attrib_name.get_name();
					cond_sql += _T(" <= '"); 
					cond_sql += q->resval.rstring;
               cond_sql += _T("'"); 
				}
			}
			else 																 // intervalli discreti
			{
				if (k == 1) q = CommonStruct4Thm->range_values.getptr_at(iniz);
				else { for (int l = 1; l <= passo; l++) q = q->rbnext; }
				cond_sql += _T(" like '"); 
				cond_sql += q->resval.rstring;
				cond_sql += _T("'");
			}
		}
		else if (pConn->IsBoolean(CommonStruct4Thm->attrib_type.get_name()) == GS_GOOD) //solo intervalli discreti
		{  
         int MapBool;

			if (k == 1) q = CommonStruct4Thm->range_values.getptr_at(iniz);
			else { for (int l = 1; l <= passo; l++) q = q->rbnext; }
			if (gsc_conv_Bool(q->resval.rstring, &MapBool) == GS_BAD) return GS_BAD;

			if (MapBool == 1) cond_sql += _T("<>0");
			else cond_sql += _T("=0");
		}
		else if (pConn->IsDate(CommonStruct4Thm->attrib_type.get_name()) == GS_GOOD)
		{
			C_STRING MapDate;

			if (CommonStruct4Thm->type_interv == 1)              // intervalli continui
			{
				if (k == 1)                                       // primo intervallo
				{
					q = CommonStruct4Thm->range_values.getptr_at(iniz);
               // Correggo la stringa "time-stamp" secondo la sintassi SQL
               MapDate = q->resval.rstring;
               if (pConn->Str2SqlSyntax(MapDate, pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
                  return GS_BAD;
					cond_sql += _T(" <= "); 
					cond_sql += MapDate;
					cond_sql += _T("'"); 
				}
				else if (k <= CommonStruct4Thm->n_interv)		     // intervalli successivi
				{
               // Correggo la stringa "time-stamp" secondo la sintassi SQL
               MapDate = q->resval.rstring;
               if (pConn->Str2SqlSyntax(MapDate, pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
                  return GS_BAD;
					cond_sql += _T(" > "); 
					cond_sql += MapDate;
   				for (int l = 1; l <= passo; l++) q = q->rbnext;
  					cond_sql += _T(" AND ");	
					cond_sql += CommonStruct4Thm->attrib_name.get_name();
               // Correggo la stringa "time-stamp" secondo la sintassi SQL
               MapDate = q->resval.rstring;
               if (pConn->Str2SqlSyntax(MapDate, pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
                  return GS_BAD;
					cond_sql += _T(" <= ");
					cond_sql += MapDate;
				}
			}                                                       
         else     // intervalli discreti
         {
				if (k == 1) q = CommonStruct4Thm->range_values.getptr_at(iniz);
				else { for (int l = 1; l <= passo; l++) q = q->rbnext; }

            // Correggo la stringa "time-stamp" secondo la sintassi SQL
            MapDate = q->resval.rstring;
            if (pConn->Str2SqlSyntax(MapDate, pConn->ptr_DataTypeList()->search_Type(adDBTimeStamp, FALSE)) == GS_BAD)
               return GS_BAD;
				cond_sql += _T(" = ");
				cond_sql += MapDate;
         }
		}
		// setto la sql_list con i valori che servono 
		C_CLASS_SQL *sql_list;

      if ((sql_list = new C_CLASS_SQL) == NULL) { GS_ERR_COD=eGSOutOfMem; ret = GS_BAD; break; }
		sql_list->set_cls(CommonStruct4Thm->cls);
		sql_list->set_sub(CommonStruct4Thm->sub);											  
		sql_list->set_sql(cond_sql.get_name());
		CommonStructCondition->sql_list.remove_all();
		CommonStructCondition->sql_list.add_tail(sql_list);			

		// invoco la funzione che esegue il filtro
  	   if (gsc_do_sql_filter(CommonStruct4Thm->cls, CommonStruct4Thm->sub,
                            CommonStructCondition->sql_list, pLinkSet) == GS_BAD)
			{ ret = GS_BAD; break; }

      // mi posiziono sul primo valore del range impostato
		if (k == 1) r = CommonStruct4Thm->range_values.getptr_at(5);

      if (GS_LSFILTER.ptr_SelSet()->length() == 0 && GS_LSFILTER.ptr_KeyList()->get_count() == 0)
	   {
		   acutPrintf(gsc_msg(117), 0);  // "\n%ld oggetti grafici filtrati."
	      // mi posiziono sul valore successivo
	      if (k	< CommonStruct4Thm->n_interv)
            { for (int g = 1; g <= passo; g++) r = r->rbnext; }
		   continue;
	   }
      
   	// costruisco la fas di evidenziazione in funzione del range corrente e del tematismo/i scelto/i
		if (passo == 14)    // un solo tematismo
		{
			if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(629)) == 0)        // colore
         {
            if (FasEvid.color.setString(r->resval.rstring) == GS_BAD)
   		   	{ gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
         }
         else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(632)) == 0)	 // st. testo
              wcscpy(FasEvid.style, r->resval.rstring);       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(631)) == 0)	 // tipolinea
              wcscpy(FasEvid.line, r->resval.rstring);       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(630)) == 0)	 // Riempimento
              gsc_strcpy(FasEvid.hatch, r->resval.rstring, MAX_LEN_HATCHNAME);
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(229)) == 0)   // Colore riempimento
         {
            if (FasEvid.hatch_color.setString(r->resval.rstring) == GS_BAD)
   		   	{ gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
         }
         else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(231)) == 0)	 // Piano riempimento
              gsc_strcpy(FasEvid.hatch_layer, r->resval.rstring, MAX_LEN_LAYERNAME);
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(464)) == 0)	 // Scala riempimento
            FasEvid.hatch_scale = r->resval.rreal;       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(465)) == 0)	 // Rotazione riempimento (gradi)
            FasEvid.hatch_rotation = r->resval.rreal;       
         else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(633)) == 0)	 // Piano
              gsc_strcpy(FasEvid.layer, r->resval.rstring, MAX_LEN_LAYERNAME);
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(826)) == 0)	 // blocco
              wcscpy(FasEvid.block, r->resval.rstring);       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(825)) == 0)	 // scala
            FasEvid.block_scale = r->resval.rreal;       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(483)) == 0)	 // larghezza
              FasEvid.width = r->resval.rreal;       
			else if (gsc_strcmp(CommonStruct4Thm->thm.get_name(), gsc_msg(718)) == 0)	 // al. testo
              FasEvid.h_text = r->resval.rreal;
			// mi posiziono sul valore successivo
			if (k	< CommonStruct4Thm->n_interv)
            { for (int g = 1; g <= passo; g++) r = r->rbnext; }
		}
		else  				  // più tematismi
		{
			num_thm = CommonStruct4Thm->thm_list.get_count() - 1;
			for (int m = 1; m <= num_thm; m++)
			{
				if (gsc_strcmp(r->resval.rstring, gsc_msg(629)) == 0)        // colore
            {
               if (FasEvid.color.setString((r = r->rbnext)->resval.rstring) == GS_BAD)
		   		   { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
            }
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(632)) == 0)	 // st. testo
				   wcscpy(FasEvid.style, (r = r->rbnext)->resval.rstring);       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(631)) == 0)	 // tipolinea
				   wcscpy(FasEvid.line, (r = r->rbnext)->resval.rstring);       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(630)) == 0)	 // riempimento
               gsc_strcpy(FasEvid.hatch, (r = r->rbnext)->resval.rstring, MAX_LEN_HATCHNAME);       
			   else if (gsc_strcmp(r->resval.rstring, gsc_msg(229)) == 0)   // Colore riempimento
            {
               if (FasEvid.hatch_color.setString((r = r->rbnext)->resval.rstring) == GS_BAD)
		   		   { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
            }
            else if (gsc_strcmp(r->resval.rstring, gsc_msg(231)) == 0)	 // Piano riempimento
                 gsc_strcpy(FasEvid.hatch_layer, (r = r->rbnext)->resval.rstring, MAX_LEN_LAYERNAME);
			   else if (gsc_strcmp(r->resval.rstring, gsc_msg(464)) == 0)	 // Scala riempimento
               FasEvid.hatch_scale = (r = r->rbnext)->resval.rreal;       
			   else if (gsc_strcmp(r->resval.rstring, gsc_msg(465)) == 0)	 // Rotazione riempimento (gradi)
               FasEvid.hatch_rotation = (r = r->rbnext)->resval.rreal;       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(633)) == 0)	 // piano
					  wcscpy(FasEvid.layer, (r = r->rbnext)->resval.rstring);       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(826)) == 0)	 // blocco
					  wcscpy(FasEvid.block, (r = r->rbnext)->resval.rstring);       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(825)) == 0)	 // scala
                 FasEvid.block_scale = (r = r->rbnext)->resval.rreal;       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(483)) == 0)	 // larghezza
					  FasEvid.width = (r = r->rbnext)->resval.rreal;       
				else if (gsc_strcmp(r->resval.rstring, gsc_msg(718)) == 0)	 // al. testo
					  FasEvid.h_text = (r = r->rbnext)->resval.rreal;
				// mi posiziono sul valore successivo
				if (m < num_thm)
               { for (int j = 1; j <= 3; j++) r = r->rbnext; }
			}
			// mi posiziono sull'intervallo successivo 
			if (k < CommonStruct4Thm->n_interv)
            { for (int a = 1; a <= 15; a++) r = r->rbnext; }
		}

		if (CommonStructCondition->Tools)
         if (pCls->get_category() != CAT_GRID)
         {
 			   if (gsc_evid4thm(pCls, *(GS_LSFILTER.ptr_SelSet()), &FasEvid, 
                             CommonStruct4Thm->FlagThm, k) != GS_GOOD)
				   { ret = GS_BAD; }
         }
         else
            if (((C_CGRID *) pCls)->DisplayCells(GS_LSFILTER.ptr_KeyList(), NULL,
                                                 &FasEvid, EXTRACTION) == GS_BAD)
               return GS_BAD;

      // devo salvare gli oggetti modificati
      if (CommonStruct4Thm->AddObjs2SaveSet)
      {
         int previous = GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet();

         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_GOOD);
         GEOsimAppl::REFUSED_SS.clear();
         //              selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
         gsc_class_align(*(GS_LSFILTER.ptr_SelSet()), GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), GS_GOOD, FAS_MOD);
         GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous);
      }
	}
     
	return ret;
}


/*****************************************************************************/
/*.doc gsc_build_legend <external> */
/*+
  Questa funzione costruisce ed inserisce la legenda in base ai valori impostati
  di tematismo ed evidenziazione.
  Parametri:
  Common_Dcl_legend_Struct *CommonStruct4Thm : struttura dei dati per il 
                           filtro tematico (range, tematismo, ecc.)  ;
                           (spaziali, di insieme ecc.);
   
  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/******************************************************************************/
int gsc_build_legend(Common_Dcl_legend_Struct *CommonStruct4Leg)
{
   presbuf    p, q;
   ads_name   obj, objhtc, elem, gsel;
   ads_point  p0 = {0.0,0.0,0.0}, pins = {0.0,0.0,0.0};
   ads_point  pi = {100.0,500.0,0.0}, pf = {100.0,500.0,0.0}, ptext = {100.0,500.0,0.0};
   TCHAR      old_style[MAX_LEN_TEXTSTYLENAME], LineType[MAX_LEN_LINETYPENAME]; 
   int        num_int, type_cls, cat_cls, j, num_val, valint = 0;
   long       FlagThm = 0;
   double     dimxsimb, dimysimb, offsimb, dim_etch, off_etch, scala;
   C_CLASS    *pCls = NULL;
   C_STRING   layer_leg, text_style, name_leg, simbolo, hatch, 
              layer_curr, val_color;
   C_COLOR    color, colore;
   C_ID       *ptr_id;
  	C_RB_LIST  DescrStile, layer;
   C_STR_LIST BlockNameList;
   C_FAS      *pfas;
   
   // verifico se devo inserire la legenda oppure no
   if (CommonStruct4Leg->FlagLeg == FALSE) 
   {
      acutPrintf(gsc_msg(823));    // "\nNessuna legenda impostata" 
      return GS_GOOD;
   }
   else // leggo i parametri della legenda
   {
      if ((q = CommonStruct4Leg->Legend_param.getptr_at(8)) == NULL) return GS_BAD;
      layer_leg = q->resval.rstring;

      // Verifico il layer e se non esiste lo creo.
      if (gsc_crea_layer(layer_leg.get_name()) == GS_BAD) return GS_BAD;

      for (j = 1; j <= 4; j++) q = q->rbnext;
      ads_point_set(q->resval.rpoint, pins);
		if ((pins[X] == 0.0) && (pins[Y] == 0.0)) CommonStruct4Leg->ins_leg = FALSE;
		else CommonStruct4Leg->ins_leg = TRUE;

      for (j = 1; j <= 4; j++) q = q->rbnext;
      dimxsimb = q->resval.rreal;
      for (j = 1; j <= 4; j++) q = q->rbnext;
      dimysimb = q->resval.rreal;
      for (j = 1; j <= 4; j++) q = q->rbnext;
      offsimb = q->resval.rreal;
      for (j = 1; j <= 4; j++) q = q->rbnext;
      dim_etch = q->resval.rreal;

      for (j = 1; j <= 4; j++) q = q->rbnext;
      text_style = q->resval.rstring;              
      // verifico se il tipo testo è valido e lo carico
      if (gsc_validtextstyle(text_style.get_name()) == GS_BAD) return GS_BAD;
      if ((DescrStile << gsc_getgeosimstyle()) == NULL) return GS_BAD;
      if (DescrStile.SearchName(text_style.get_name(), FALSE) == NULL) 
         { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }
      if (gsc_load_textstyle(text_style.get_name()) != GS_GOOD) return GS_BAD;

      for (j = 1; j <= 4; j++) q = q->rbnext;
      off_etch = q->resval.rreal;
   }

   pf[X] = 100.0 + dimxsimb;
   ptext[X] = 100.0 + dimxsimb + off_etch;

   // ricavo il puntatore alla classe e la sua categoria
   if ((pCls = gsc_find_class(CommonStruct4Leg->prj, 
                              CommonStruct4Leg->cls, 
                              CommonStruct4Leg->sub)) == NULL) return GS_BAD; 
   ptr_id = pCls->ptr_id();
   cat_cls = ptr_id->category;
   type_cls = ptr_id->type;
   if ((pfas = new C_FAS) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   pCls->ptr_fas()->copy(pfas);
   colore = pfas->color;
   layer_curr.clear();

   // in funzione del tematismo/i scelto/i costruisco la legenda
   if (gsc_strcmp(CommonStruct4Leg->thm.get_name(), gsc_msg(833)) != 0)
   {	// scelta singolo tematismo
		num_int = 1;
      p = CommonStruct4Leg->range_values.getptr_at(4);
      // inizializzo il gruppo di selezione
      if (acedSSAdd(NULL, NULL, elem) != RTNORM) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      while (p)
      {
         // in funzione della tipologia della classe invoco il comando opportuno
         if ((cat_cls == CAT_SIMPLEX) || (cat_cls == CAT_EXTERN))
         {
            // mi posiziono sul piano scelto dall'utente per la legenda
            layer_leg = CommonStruct4Leg->Legend_param.getptr_at(8)->resval.rstring;
            if ((layer << acutBuildList(RTSTR, layer_leg.get_name(), 0)) == NULL)
               return GS_BAD;

            if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
               { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

            switch (type_cls)
            {
               case TYPE_POLYLINE:
                  // disegno una polilinea
                  if (gsc_insert_pline(pi, pf) == GS_BAD)
                     { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

                  // ricavo il colore del piano della classe
                  if (gsc_get_charact_layer(pCls->ptr_fas()->layer, LineType, &color) == GS_BAD) return GS_BAD;
                  pfas->color = color;
                  
                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }              
                  // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { ads_ssfree(elem); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }              

                  // modifico l'entita con le caratteristiche grafiche della fas
                  if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) != 0)
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagFas, 
                                            FALSE) == GS_BAD) 
                        { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }
                  else  // se impostata evidenziazione per piano non vario il colore
                     if (gsc_modifyEntToFas(gsel, pfas, 
                                            CommonStruct4Leg->FlagFas - GSColorSetting, 
                                            FALSE) == GS_BAD)
                        { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }
                  // vario la FAS in funzione delle caratteristiche impostate 
			         if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)        // colore
                  {
      					val_color = (p->rbnext)->resval.rstring;
		      			if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
				      	   { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
                  }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(483)) == 0)	 // larghezza
                     { pfas->width = (p->rbnext)->resval.rreal; }
  			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) == 0)	 // piano
                     { gsc_strcpy(pfas->layer,(p->rbnext)->resval.rstring, 
                                  MAX_LEN_LAYERNAME); }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(631)) == 0)	 // tipolinea
                     { gsc_strcpy(pfas->line,(p->rbnext)->resval.rstring, 
                                  MAX_LEN_LINETYPENAME); }

                  // modifico l'entita con la nuova FAS
                  if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                         FALSE) == GS_BAD) return GS_BAD;
                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;

               case TYPE_TEXT:
               {
                  resbuf rb;
                  double FixedHText;

                  acedGetVar(_T("TEXTSTYLE"), &rb);
                  // verifico se lo stile di testo ha una altezza fissa
                  gsc_get_charact_textstyle(rb.resval.rstring, &FixedHText);

                  // disegno una semplice testo 
                  if (FixedHText == 0) // non è stata impostata altezza fissa
                  {
                     if (gsc_callCmd(_T("_.TEXT"), RTPOINT, pi, RTREAL, dimysimb,
                                     RTREAL, 0.0, RTSTR, _T("Abc"), 0) != RTNORM)
                        { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
                  }
                  else
                     if (gsc_callCmd(_T("_.TEXT"), RTPOINT, pi,
                                     RTREAL, 0.0, RTSTR, _T("Abc"), 0) != RTNORM)
                        { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

                  // ricavo il colore del piano della classe
                  if (gsc_get_charact_layer(pCls->ptr_fas()->layer, LineType, &color) == GS_BAD) return GS_BAD;
                  pfas->color = color;
                  
                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }
                  // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }    
                  
                  // modifico l'entita con le caratteristiche grafiche della fas
                  if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) != 0)
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagFas, 
                                            FALSE) == GS_BAD)
                        { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }
                  else  // se impostata evidenziazione per piano non vario il colore
                     if (gsc_modifyEntToFas(gsel, pfas, 
                                            CommonStruct4Leg->FlagFas - GSColorSetting, 
                                            FALSE) == GS_BAD)
                        { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }

                  // vario la FAS in funzione delle caratteristiche impostate 
			         if (gsc_strcmp(p->resval.rstring, gsc_msg(718)) == 0)	       // altezza testo
                     pfas->h_text = _wtoi((p->rbnext)->resval.rstring);
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)   // colore
                  {
      					val_color = (p->rbnext)->resval.rstring;

		      			if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
				      	   { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
                  } 
  			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) == 0)	 // piano
                     { gsc_strcpy(pfas->layer,(p->rbnext)->resval.rstring, 
                                  MAX_LEN_LAYERNAME); }
 			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(632)) == 0)	 // stile testo
                     { gsc_strcpy(pfas->layer,(p->rbnext)->resval.rstring, 
                                  MAX_LEN_TEXTSTYLENAME); }

                  // modifico l'entita con la nuova FAS
                  if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                         FALSE) == GS_BAD)
                     { ads_ssfree(elem); ads_ssfree(gsel); return GS_BAD; }

                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;
               }

               case TYPE_NODE:
                  // disegno la proprietà di evidenziazione
			         if (gsc_strcmp(p->resval.rstring, gsc_msg(826)) == 0)        // blocco
                  {
                     if (gsc_insert_block(p->rbnext->resval.rstring, pi,
                                          dimxsimb, dimysimb, 0.0) == GS_BAD)
                        return GS_BAD;
                     if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  }
  			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) == 0)	 // piano
                     {
                        // inizializzo il gruppo di selezione
                        if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                           { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                        gsc_strcpy(pfas->layer,(p->rbnext)->resval.rstring, MAX_LEN_LAYERNAME);
                        FlagThm = GSLayerSetting;
                        if (gsc_insert_block(pfas->block, pi, pfas->block_scale, pfas->block_scale,
                                             pfas->rotation) == GS_BAD)                      
                           return GS_BAD;

                        if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                        // aggiungo la nuova entità nel gr. di selezione per la modifica
                        if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                           { ads_ssfree(gsel); return GS_BAD; }              
                        // modifico l'entita
                        if (gsc_modifyEntToFas(gsel, pfas, FlagThm, FALSE) == GS_BAD) 
                           return GS_BAD;
                        if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                     }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(825)) == 0)	 // scala
                     {
                        // ricavo dalla fas della classe il simbolo da scalare
                        // lo inserisco scalato
                        if (gsc_insert_block(pfas->block, pi, p->rbnext->resval.rreal,
                                             p->rbnext->resval.rreal, 0.0) == GS_BAD)
                           return GS_BAD;
                        if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                     }
  			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)       // colore roby 2016
                  {
                     C_STRING dummy(p->rbnext->resval.rstring);
                     C_COLOR newColor;

	   	      		if (gsc_validcolor(dummy, newColor) == GS_BAD)
		   		      	{ gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."

                     if (gsc_insert_block(pfas->block, pi, pfas->block_scale, pfas->block_scale,
                                          pfas->rotation) == GS_BAD)                      
                        return GS_BAD;
                     if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                     if (gsc_set_color(obj, newColor) == GS_BAD)  // cambio il colore
                        return GS_BAD;
                  }

                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }              
                  break;

               case TYPE_SURFACE:
                  ads_point p2, p3, p4;
                  p2[X] = pi[X];            p2[Y] = pi[Y] + dimysimb;
                  p3[X] = pi[X] + dimxsimb; p3[Y] = pi[Y] + dimysimb;
                  p4[X] = pi[X] + dimxsimb; p4[Y] = pi[Y];
                  // disegno una polilinea chiusa quadrata
                  if (gsc_insert_rectangle(pi, p3) == GS_BAD) return GS_BAD;

                  // ricavo il colore del piano della classe
                  if (gsc_get_charact_layer(pCls->ptr_fas()->layer, LineType, &color) == GS_BAD) return GS_BAD;
                  pfas->color = color;
                  colore      = color;

                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }              
                  // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(gsel); return GS_BAD; }              
                  
                  // modifico l'entita con le caratteristiche grafiche della fas
                  if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) != 0)
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagFas, 
                                            FALSE) == GS_BAD) return GS_BAD;
                  else  // se impostata evidenziazione per piano non vario il colore
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagFas - GSColorSetting, 
                                            FALSE) == GS_BAD) return GS_BAD;

                  // vario la superficie in funzione delle caratteristiche impostate 
			         if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)        // colore
                  {
      					val_color = (p->rbnext)->resval.rstring;

	   	            if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
		   		         { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."

                     // modifico l'entita con la nuova FAS
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                            FALSE) == GS_BAD) return GS_BAD;
                  }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(483)) == 0)	 // larghezza
                     {
                        pfas->width = (p->rbnext)->resval.rreal;
                        // modifico l'entita con la nuova FAS
                        if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                               FALSE) == GS_BAD) return GS_BAD;
                     }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) == 0)	 // piano
                     { 
                        layer_curr = (p->rbnext)->resval.rstring;
                        gsc_strcpy(pfas->layer,layer_curr.get_name(), 
                                   MAX_LEN_LAYERNAME);
                        // modifico l'entita con la nuova FAS
                        if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                               FALSE) == GS_BAD) return GS_BAD;
                        // rendo corrente il piano per l'inserimento di un eventuale hatch
                        if ((layer << acutBuildList(RTSTR, layer_curr.get_name(), 0)) == NULL)
                           return GS_BAD;
                        if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
                           { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
                     }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(631)) == 0)	 // tipolinea
                     { 
                        gsc_strcpy(pfas->line,(p->rbnext)->resval.rstring, 
                                   MAX_LEN_LINETYPENAME);
                        // modifico l'entita con la nuova FAS
                        if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                               FALSE) == GS_BAD) return GS_BAD;
                     }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(630)) == 0)	 // riempimento
                     {
                        if (gsc_setHatchEnt(obj, (p->rbnext)->resval.rstring, 1.0, 0.0) == GS_BAD)
                           return GS_BAD;
                        if (acdbEntLast(objhtc) != RTNORM) return GS_BAD;
                        // aggiungo la nuova entità in gruppo di selezione
                        if (acedSSAdd(objhtc, elem, elem) != RTNORM)
                           { ads_ssfree(elem); return GS_BAD; }
                        // cambio il colore del riempimento
                        if (gsc_set_color(objhtc, colore) == GS_BAD) return GS_BAD;
                     }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(229)) == 0)        // Colore riempimento
                  {
      					val_color = (p->rbnext)->resval.rstring;

	   	            if (gsc_validcolor(val_color, pfas->hatch_color) == GS_BAD)
		   		         { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."

                     // modifico l'entita con la nuova FAS
                     if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                            FALSE) == GS_BAD) return GS_BAD;
                  }
			         else if (gsc_strcmp(p->resval.rstring, gsc_msg(825)) == 0)	 // scala
                     {
                        if (gsc_setHatchEnt(obj, pfas->hatch, 
                                            (p->rbnext)->resval.rreal, 0.0) == GS_BAD)
                           return GS_BAD;

                        if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                        // aggiungo la nuova entità in gruppo di selezione
                        if (acedSSAdd(obj, elem, elem) != RTNORM)
                           { ads_ssfree(elem); return GS_BAD; }              
                        // cambio il colore del riempimento
                        if (gsc_set_color(obj, colore) == GS_BAD) return GS_BAD;
                     }
                  // verifico se devo inserire un riempimento oppure no
                  if ((gsc_strcmp(pfas->hatch, GS_EMPTYSTR) != 0) &&
                      (gsc_strcmp(p->resval.rstring, gsc_msg(464)) != 0) && // "Scala riempimento"
                      (gsc_strcmp(p->resval.rstring, gsc_msg(630)) != 0))
                  {
                     if (gsc_setHatchEnt(obj, pfas->hatch, pfas->hatch_scale,
                                         pfas->hatch_rotation) == GS_BAD)
                        return GS_BAD;
                     if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                     // aggiungo la nuova entità in gruppo di selezione
                     if (acedSSAdd(obj, elem, elem) != RTNORM)
                        { ads_ssfree(elem); return GS_BAD; }              

                     // cambio il colore del riempimento se il tematismo è <> da 'piano'
                     if (gsc_strcmp(p->resval.rstring, gsc_msg(633)) != 0) // "Piano"
                        if (gsc_set_color(obj, colore) == GS_BAD) return GS_BAD;
                  }
                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;
            }
            // mi posiziono sul valore della descrizione intervallo
            // e lo inserisco in legenda  
            for (j = 1; j <= 9; j++) p = p->rbnext;
            if ((layer << acutBuildList(RTSTR, layer_leg.get_name(), 0)) == NULL)
               return GS_BAD;

            if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
               { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

            resbuf rb;
            double FixedHText;

            acedGetVar(_T("TEXTSTYLE"), &rb);
            // verifico se lo stile di testo ha una altezza fissa
            gsc_get_charact_textstyle(rb.resval.rstring, &FixedHText);

            if (FixedHText == 0) // non è stata impostata altezza fissa
            {
               if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"), RTSTR, text_style.get_name(),
                               RTPOINT, ptext, RTREAL, dim_etch, RTREAL, 0.0,
                               RTSTR, p->resval.rstring, 0) != RTNORM)
                  { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
            }
            else
               if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_S"), RTSTR, text_style.get_name(),
                               RTPOINT, ptext, RTREAL, 0.0,
                               RTSTR, p->resval.rstring, 0) != RTNORM)
                  { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

            if (acdbEntLast(obj) != RTNORM) return GS_BAD;
            // aggiungo la nuova entità in gruppo di selezione
            if (acedSSAdd(obj, elem, elem) != RTNORM)
               { ads_ssfree(elem); return GS_BAD; }              
            // mi posiziono sul valore successivo
            if (num_int < CommonStruct4Leg->n_interv)
               { for (j = 1; j <= 5; j++) p = p->rbnext; num_int++; }
            else p = NULL;
         }
         // decremento i punti dei valori da inserire in legenda
         pi[Y] = pf[Y] = ptext[Y] = pi[Y] - dimysimb - offsimb;
      }
	}
   else
   {	// scelta tematismo multiplo
		num_int = 1; 
   	num_val = (CommonStruct4Leg->thm_list.get_count() - 1); 
      p = CommonStruct4Leg->range_values.getptr_at(5);

      // inizializzo il gruppo di selezione
      if (acedSSAdd(NULL, NULL, elem) != RTNORM) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      while (p)
      {
         // in funzione della tipologia della classe invoco il comando opportuno
         if ((cat_cls == CAT_SIMPLEX) || (cat_cls == CAT_EXTERN))
         {
            // mi posiziono sul piano scelto dall'utente per la legenda
            layer_leg = CommonStruct4Leg->Legend_param.getptr_at(8)->resval.rstring;
            if ((layer << acutBuildList(RTSTR, layer_leg.get_name(), 0)) == NULL)
               return GS_BAD;

            if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
               { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

            switch (type_cls)
            {
               case TYPE_POLYLINE:
                  // disegno una polilinea
                  if (gsc_insert_pline(pi, pf) == GS_BAD) return GS_BAD;
                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; } 
                  // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(gsel); return GS_BAD; }                         

                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  // ricavo le caratteristiche impostate 
                  for (j = 1; j <= num_val; j++)
                  {
			            if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)        // colore
                     {
           					val_color = p->rbnext->resval.rstring;

	   	      			if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
		   		      	   { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
                     }   
			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(483)) == 0)	 // larghezza
                        { pfas->width = (p->rbnext)->resval.rreal; }
   		            else if (gsc_strcmp(p->resval.rstring, gsc_msg(631)) == 0)	 // tipolinea
                        { gsc_strcpy(pfas->line,(p->rbnext)->resval.rstring, 
                                     MAX_LEN_LINETYPENAME); }
                     // mi posiziono sul valore successivo dentro l'intervallo
                     if (j < num_val) { for (int k = 1; k <= 4; k++) p = p->rbnext; }
                  }
                  // modifico l'entita con la nuova FAS
                  if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                         FALSE) == GS_BAD) return GS_BAD;
                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;
               case TYPE_TEXT:
               {
                  resbuf rb;
                  double FixedHText;

                  acedGetVar(_T("TEXTSTYLE"), &rb);
                  // verifico se lo stile di testo ha una altezza fissa
                  gsc_get_charact_textstyle(rb.resval.rstring, &FixedHText);

                  // disegno una semplice testo 
                  if (FixedHText == 0) // non è stata impostata altezza fissa
                  {
                     if (gsc_callCmd(_T("_.TEXT"), RTPOINT, pi, RTREAL, dimysimb,
                                     RTREAL, 0.0, RTSTR, _T("Abc"), 0) != RTNORM)
                        { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
                  }
                  else
                     if (gsc_callCmd(_T("_.TEXT"), RTPOINT, pi,
                                     RTREAL, 0.0, RTSTR, _T("Abc"), 0) != RTNORM)
                        { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;  // eventuale messaggio di errore
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }

                                    // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(gsel); return GS_BAD; }                         

                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  // recupero le caratteristiche impostate 
                  for (j = 1; j <= num_val; j++)
                  {
			            if (gsc_strcmp(p->resval.rstring, gsc_msg(718)) == 0)  	    // altezza testo
                        pfas->h_text = (p->rbnext)->resval.rreal;
			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)   // colore
                        {
           					   val_color = p->rbnext->resval.rstring;

	   	      			   if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
		   		      	      { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."
                        }   
 			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(632)) == 0)	 // stile testo
                        { gsc_strcpy(pfas->line,(p->rbnext)->resval.rstring, 
                                     MAX_LEN_TEXTSTYLENAME); }
                     // mi posiziono sul valore successivo dentro l'intervallo
                     if (j < num_val) { for (int k = 1; k <= 4; k++) p = p->rbnext; }
                  }
                  // modifico l'entita con la nuova FAS
                  if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                         FALSE) == GS_BAD) return GS_BAD;
                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;
               }
               case TYPE_NODE:
                  // vario il blocco in funzione delle caratteristiche impostate 
                  for (j = 1; j <= num_val; j++)
                  {
                     // memorizzo il nome del blocco e la scala per ogni intervallo
			            if (gsc_strcmp(p->resval.rstring, gsc_msg(826)) == 0)        // blocco
                     {
                        simbolo = (p->rbnext)->resval.rstring;
                     }
			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(825)) == 0)	 // scala
                        {
                           scala = (p->rbnext)->resval.rreal;
                        }
                     // mi posiziono sul valore successivo dentro l'intervallo
                     if (j < num_val) { for (int k = 1; k <= 4; k++) p = p->rbnext; }
                  }
                  // disegno la proprietà di evidenziazione
                  if (gsc_insert_block(simbolo.get_name(), pi, scala, scala, 0.0) == GS_BAD)
                     return GS_BAD;
                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                    { ads_ssfree(elem); return GS_BAD; }              
                  break;
               case TYPE_SURFACE:
                  ads_point p2, p3, p4;
                  p2[X] = pi[X];            p2[Y] = pi[Y] + dimysimb;
                  p3[X] = pi[X] + dimxsimb; p3[Y] = pi[Y] + dimysimb;
                  p4[X] = pi[X] + dimxsimb; p4[Y] = pi[Y];
                  // disegno una polilinea chiusa quadrata
                  if (gsc_insert_rectangle(pi, p3) == GS_BAD) return GS_BAD;
                  if (acdbEntLast(obj) != RTNORM) return GS_BAD;  // eventuale messaggio di errore
                  // aggiungo la nuova entità in gruppo di selezione
                  if (acedSSAdd(obj, elem, elem) != RTNORM)
                     { ads_ssfree(elem); return GS_BAD; }              
                  // inizializzo il gruppo di selezione per la modifica
                  if (acedSSAdd(NULL, NULL, gsel) != RTNORM) 
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  // aggiungo la nuova entità nel gruppo di selezione per la modifica
                  if (acedSSAdd(obj, gsel, gsel) != RTNORM)
                     { ads_ssfree(gsel); return GS_BAD; }                         

                  // setto il piano come piano della legenda
                  gsc_strcpy(pfas->layer, layer_leg.get_name(), MAX_LEN_LAYERNAME);

                  // vario la superficie in funzione delle caratteristiche impostate 
                  for (j = 1; j <= num_val; j++)
                  {
			            if (gsc_strcmp(p->resval.rstring, gsc_msg(629)) == 0)        // colore
                     {
      					   val_color = (p->rbnext)->resval.rstring;

   	   	            if (gsc_validcolor(val_color, pfas->color) == GS_BAD)
				      	      { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."

                        colore = pfas->color;
                     }
                     else if (gsc_strcmp(p->resval.rstring, gsc_msg(483)) == 0)	 // larghezza
                        { pfas->width = (p->rbnext)->resval.rreal; }
                     else if (gsc_strcmp(p->resval.rstring, gsc_msg(631)) == 0)	 // tipolinea
                        { gsc_strcpy(pfas->line,(p->rbnext)->resval.rstring, 
                                    MAX_LEN_LINETYPENAME); }
			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(630)) == 0)	 // riempimento
                        {
                           hatch = (p->rbnext)->resval.rstring;
                           // mi posiziono sul valore successivo dentro l'intervallo
                           // per ricavare la scala di riempimento
                           if (j < num_val)
                              { for (int k = 1; k <= 4; k++) p = p->rbnext; j++; }
			                  if (gsc_strcmp(p->resval.rstring, gsc_msg(825)) == 0)	 // scala
                           {
                              if (gsc_setHatchEnt(obj, hatch.get_name(),
                                                  (p->rbnext)->resval.rreal, 0.0) == GS_BAD)
                                 return GS_BAD;
                              if (acdbEntLast(objhtc) != RTNORM) return GS_BAD;
                              // aggiungo la nuova entità in gruppo di selezione
                              if (acedSSAdd(objhtc, elem, elem) != RTNORM)
                                 { ads_ssfree(elem); return GS_BAD; }              
                              // cambio il colore del riempimento
                              if (gsc_set_color(objhtc, colore) == GS_BAD) return GS_BAD;
                           }
                        }
			            else if (gsc_strcmp(p->resval.rstring, gsc_msg(229)) == 0)        // Colore riempimento
                     {
      					   val_color = (p->rbnext)->resval.rstring;

   	   	            if (gsc_validcolor(val_color, pfas->hatch_color) == GS_BAD)
				      	      { gsc_msg(806); return GS_BAD; } // "*Errore* Colore non valido."

                        colore = pfas->hatch_color;
                     }
                     // mi posiziono sul valore successivo dentro l'intervallo
                     if (j < num_val) { for (int k = 1; k <= 4; k++) p = p->rbnext; }
                  }
                  // modifico l'entita con la nuova FAS
                  if (gsc_modifyEntToFas(gsel, pfas, CommonStruct4Leg->FlagThm, 
                                         FALSE) == GS_BAD) return GS_BAD;
                  if (ads_ssfree(gsel) != RTNORM) return GS_BAD;
                  break;
            }
            // controllo la validità, carico lo stile e lo rendo corrente
            if (gsc_set_CurrentTextStyle(text_style.get_name(), old_style) == GS_BAD) return GS_BAD;

            // mi posiziono sul valore della descrizione intervallo
            // e lo inserisco in legenda  
            for (j = 1; j <= 10; j++) p = p->rbnext;

            if (gsc_insert_text(p->resval.rstring, ptext, dim_etch, 0.0) == GS_BAD)
               return GS_BAD;

            if (acdbEntLast(obj) != RTNORM) return GS_BAD;
            // aggiungo la nuova entità in gruppo di selezione
            if (acedSSAdd(obj, elem, elem) != RTNORM)
               { ads_ssfree(elem); return GS_BAD; }              
            // mi posiziono sul valore successivo
            if (num_int < CommonStruct4Leg->n_interv)
               { for (j = 1; j <= 6; j++) p = p->rbnext; num_int++; }
            else p = NULL;
         }
         // decremento i punti dei valori da inserire in legenda
         pi[Y] = pf[Y] = ptext[Y] = pi[Y] - dimysimb - offsimb;
      }
	}

   pi[Y] = pi[Y] - 5;

   j = 0;
   if (gsc_getBlockList(BlockNameList) == GS_BAD) return GS_BAD;
   // verifico se il blocco legenda_i-esima è già stato creato
   while (TRUE)
   {
      name_leg = _T("legenda");
      name_leg += j;
      if (BlockNameList.search_name(name_leg.get_name(), FALSE) == NULL) break;
      j++;
   }

   // salvo la legenda come blocco 
   if (gsc_callCmd(_T("_.BLOCK"), RTSTR, name_leg.get_name(), RTPOINT, pi, 
                   RTPICKS, elem, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
   ads_ssfree(elem);

   // setto come corrente il piano scelto per la legenda
   if ((layer << acutBuildList(RTSTR, layer_leg.get_name(), 0)) == NULL)
      return GS_BAD;

   if (acedSetVar(_T("CLAYER"), layer.get_head()) != RTNORM)  
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 

   // inserisco la legenda chiedendo il punto all'utente
   if (CommonStruct4Leg->ins_leg == FALSE)
   {
      if (acedCommandS(RTSTR, _T("_.INSERT"), RTSTR, name_leg.get_name(), 0) != RTNORM) // roby 2015
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
   }
   else  // inserisco la legenda nel punto scelto dall'utente in precedenza
   {
      if (gsc_callCmd(_T("_.INSERT"), RTSTR, name_leg.get_name(), 
                      RTPOINT, pins, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; } 
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DEL FILTRO TEMATICO 
// INIZIO FUNZIONI DEI SENSORI 
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_selClsObjsWindowSS                 <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava gli oggetti appartenenti ad una classe dentro una finestra di dimensioni
  note centrata sul punto di inserimento di ciascuno oggetto nel gruppo EntSS 
  (ammessi solo blocchi o testi).
  Parametri:
  C_SELSET &EntSS;            entità di riferimento
  double dimX;                dimensione X della finestra
  double dimY;                dimensione X della finestra
  C_CLASS *pDetectedCls;      Classe da rilevare
  int Mode;                   modo (interno o intersezione)
  C_LINK_SET &DetectedLS;     Gruppo di oggetti rilevati (gruppo di selezione se la
                              classe non è griglia altrimenti lista di codici di entità)
  int What;                   Flag di filtro; non usato per la griglia (default = GRAPHICAL)
                              ALL       nessun filtro
                              GRAPHICAL per gli oggetti grafici
                              DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsWindowSS(C_SELSET &EntSS, double dimX, double dimY, C_CLASS *pDetectedCls,
                           int Mode, C_LINK_SET &DetectedLS, int What = GRAPHICAL)
{
   C_SELSET     PartialSS;
   C_LONG_BTREE PartialKeyList;
   ads_point    ptIns, pt1, pt2;
   C_RB_LIST    CoordList;
   ads_name     ent;
   long         i = 0;

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   while (EntSS.entname(i++, ent) == GS_GOOD)
   {
      // Applicabile ad oggetti puntuali
      if (gsc_isblock(ent) == GS_BAD && gsc_isText(ent) == GS_BAD) 
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      // Ne leggo il punto di inserimento
      if (gsc_get_firstPoint(ent, ptIns) == GS_BAD) return GS_BAD;
      // ricavo le coordinate dei punti della finestra
      pt1[X] = ptIns[X] - (fabs(dimX) / 2.0); pt1[Y] = ptIns[Y] - (fabs(dimY) / 2.0); pt1[Z] = ptIns[Z];
      pt2[X] = pt1[X] + fabs(dimX); pt2[Y] = pt1[Y] + fabs(dimY); pt2[Z] = pt1[Z];
      if ((CoordList << acutBuildList(RT3DPOINT, pt1, RT3DPOINT, pt2, 0)) == NULL) return GS_BAD;
      // Si potrebbe visualizzare la zona con tratteggio usando "grdraw"
      // Ricavo gli oggetti nella finestra
      if (pDetectedCls->get_category() != CAT_GRID)
      {
         //                CoordList,type,ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
         if (gsc_selObjsWindow(CoordList, Mode, PartialSS, GS_GOOD, GS_GOOD, GS_GOOD,
                               pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               What) == GS_BAD)
            return GS_BAD;
         DetectedLS.add(PartialSS);
      }
      else
      {
         if (((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListInWindow(CoordList, Mode, PartialKeyList) == GS_BAD)
            return GS_BAD;
         DetectedLS.ptr_KeyList()->add_list(PartialKeyList);
      }
   }

   return GS_GOOD;
}        


/*********************************************************/
/*.doc gsc_selClsObjsCircleSS                 <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava gli oggetti appartenenti ad una classe dentro un cerchio di dimensioni
  note centrata sul punto di inserimento di ciascuno oggetto nel gruppo EntSS
  (ammessi solo blocchi o testi).
  Parametri:
  C_SELSET &EntSS;            entità di riferimento
  double Radius;              raggio del cerchio
  int Mode;                   modo (interno o intersezione)
  C_CLASS *pDetectedCls;      Classe da rilevare
  C_LINK_SET &DetectedLS;     Gruppo di oggetti rilevati (gruppo di selezione se la
                              classe non è griglia altrimenti lista di codici di entità)
  int What;                   Flag di filtro; non usato per la griglia (default = GRAPHICAL)
                              ALL       nessun filtro
                              GRAPHICAL per gli oggetti grafici
                              DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsCircleSS(C_SELSET &EntSS, double Radius, C_CLASS *pDetectedCls,
                           int Mode, C_LINK_SET &DetectedLS, int What = GRAPHICAL)
{
   C_SELSET     PartialSS;
   C_LONG_BTREE PartialKeyList;
   ads_point    ptIns;
   C_RB_LIST    CoordList;
   ads_name     ent;
   long         i = 0;

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   while (EntSS.entname(i++, ent) == GS_GOOD)
   {
      // Applicabile ad oggetti puntuali
      if (gsc_isblock(ent) == GS_BAD && gsc_isText(ent) == GS_BAD) 
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      // Ne leggo il punto di inserimento
      if (gsc_get_firstPoint(ent, ptIns) == GS_BAD) return GS_BAD;
      if ((CoordList << acutBuildList(RT3DPOINT, ptIns, RTREAL, Radius, 0)) == NULL) return GS_BAD;

      if (pDetectedCls->get_category() != CAT_GRID)
      {
         // Ricavo gli oggetti nel cerchio
         if (gsc_selObjsCircle(CoordList, Mode, PartialSS, GS_GOOD, GS_GOOD, 
                               pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               What) == GS_BAD)
            return GS_BAD;
         DetectedLS.add(PartialSS);
      }
      else
      {
         if (((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListInCircle(CoordList, Mode, PartialKeyList) == GS_BAD)
            return GS_BAD;
         DetectedLS.ptr_KeyList()->add_list(PartialKeyList);
      }
   }

   return GS_GOOD;
}        


/*********************************************************/
/*.doc gsc_selClsObjsFenceSS                  <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava gli oggetti appartenenti ad una classe che intersecano 
  ciascuno oggetto nel gruppo EntSS.
  Parametri:
  C_SELSET &EntSS;         Entità di riferimento
  C_CLASS *pDetectedCls;   Classe da rilevare
  C_LINK_SET &DetectedLS;  Gruppo di oggetti rilevati (gruppo di selezione se la
                           classe non è griglia altrimenti lista di codici di entità)
  bool IncludeFirstLast;   Nel controllo considera anche se l'intersezione è 
                           esattamente il punto iniziale o finale della linea. 
                           Non usato per la griglia (default = true).
  int What;                Flag di filtro; non usato per la griglia (default = GRAPHICAL)
                           ALL       nessun filtro
                           GRAPHICAL per gli oggetti grafici
                           DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsFenceSS(C_SELSET &EntSS, C_CLASS *pDetectedCls, C_LINK_SET &DetectedLS,
                          bool IncludeFirstLast, int What)
{
   C_SELSET     PartialSS;
   C_LONG_BTREE PartialKeyList;
   ads_name     ent;
   long         i = 0;
   C_RB_LIST    CoordList;

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   if (pDetectedCls->get_category() != CAT_GRID)
      while (EntSS.entname(i++, ent) == GS_GOOD)
      {
         ads_point pt1, pt2;

         // Ricavo le estensioni dell'oggetto quindi
         // ricavo gli oggetti che stanno dentro in modalità "Crossing"
         if (gsc_get_ent_window(ent, pt1, pt2) == GS_BAD) return GS_BAD;

         if ((CoordList << acutBuildList(RT3DPOINT, pt1, RT3DPOINT, pt2, 0)) == NULL) return GS_BAD;

         //                CoordList,type,ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
         if (gsc_selObjsWindow(CoordList, CROSSING, PartialSS, GS_GOOD, GS_GOOD, GS_GOOD,
                               pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               What) == GS_BAD) return GS_BAD;
         PartialSS.subtract(EntSS); // elimino le eventuali entità di riferimento
         // Ricavo gli oggetti che intersecano ent (calcolando le coordinate)
         //                                  CheckOn2D, OnlyInsPt, CounterToVideo
         if (PartialSS.SpatialIntersectEnt(ent, GS_GOOD, GS_GOOD, GS_BAD, IncludeFirstLast) == GS_BAD)
            return GS_BAD;
         DetectedLS.add(PartialSS);
      }
   else // caso griglia
      while (EntSS.entname(i++, ent) == GS_GOOD)
      {
         int          Closed;
         C_POINT_LIST PntList;
         ads_point    Plane;

         Plane[X] = Plane[Y] = 0.0; Plane[Z] = 1; // piano parallelo all'orizzonte

         if (PntList.add_vertex_point(ent, GS_GOOD) == GS_GOOD &&
             PntList.get_count() > 0)
         {
            // <flag aperta o chiusa> <piano>
            Closed = gsc_isClosedPline(ent);
            if (CoordList << acutBuildList(RTSHORT, Closed,
                                           RT3DPOINT, Plane, 0) == NULL)
               return GS_BAD;
            // scrivo anche i bulge
            if ((CoordList += PntList.to_rb(true)) == NULL) return GS_BAD;

            if (((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListFence(CoordList, PartialKeyList) == GS_BAD)
               return GS_BAD;
            DetectedLS.ptr_KeyList()->add_list(PartialKeyList);
         }
   }

   return GS_GOOD;
}        


/*********************************************************/
/*.doc gsc_selClsObjsBufferFenceSS            <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava gli oggetti appartenenti ad una classe che stanno nell'intorno di 
  ciascuno oggetto nel gruppo EntSS.
  Parametri:
  C_SELSET &EntSS;            entità di riferimento
  double OffSet;              dimensione del buffer
  int Mode;                   modo (interno o intersezione)
  C_CLASS *pDetectedCls;      Classe da rilevare
  C_LINK_SET &DetectedLS;     Gruppo di oggetti rilevati (gruppo di selezione se la
                              classe non è griglia altrimenti lista di codici di entità)
  int What;                   Flag di filtro; non usato per la griglia (default = GRAPHICAL)
                              ALL       nessun filtro
                              GRAPHICAL per gli oggetti grafici
                              DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsBufferFenceSS(C_SELSET &EntSS, double OffSet, int Mode,
                                C_CLASS *pDetectedCls, C_LINK_SET &DetectedLS,
                                int What)
{
   C_SELSET     PartialSS;
   C_LONG_BTREE PartialKeyList;
   ads_name     ent;
   long         i = 0;
   ads_point    Plane;
   C_RB_LIST    CoordList;
   int          Closed;
   C_POINT_LIST PntList;

   Plane[X] = Plane[Y] = 0.0; Plane[Z] = 1; // piano parallelo all'orizzonte

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   while (EntSS.entname(i++, ent) == GS_GOOD)
   {
      // Ricavo gli oggetti nel buffer (calcolando le coordinate)
      if (PntList.add_vertex_point(ent, GS_GOOD) == GS_GOOD &&
          PntList.get_count() > 0)
      {
         // <offset> <flag aperta o chiusa> <piano>
         Closed = gsc_isClosedPline(ent);
         if (CoordList << acutBuildList(RTREAL, OffSet, 
                                        RTSHORT, Closed,
                                        RT3DPOINT, Plane, 0) == NULL)
            return GS_BAD;
         // scrivo anche i bulge
         if ((CoordList += PntList.to_rb(true)) == NULL) return GS_BAD;

         if (pDetectedCls->get_category() != CAT_GRID)
         {
            if (gsc_selObjsBufferFence(CoordList, Mode, PartialSS, GS_GOOD, GS_GOOD,
                                       pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                                       What) == GS_BAD)
               return GS_BAD;

            DetectedLS.add(PartialSS);
         }
         else
         {
            if (((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListBufferFence(CoordList, Mode, PartialKeyList) == GS_BAD)
               return GS_BAD;
            DetectedLS.ptr_KeyList()->add_list(PartialKeyList);
         }
      }
   }

   return GS_GOOD;
}
int gsc_selClsObjsBufferFenceGridCell(C_CGRID *pSensorCls, long Key, double OffSet, int Mode,
                                      C_CLASS *pDetectedCls, C_LINK_SET &DetectedLS,
                                      int What = GRAPHICAL)
{
   ads_point    Plane;
   C_RB_LIST    CoordList;
   int          Closed;
   C_POINT_LIST PntList;
   AcDbPolyline *pCell;

   // Ricavo il rettangolo della cella
   if ((pCell = pSensorCls->ptr_grid()->key2Rect(Key)) == NULL) return GS_BAD;
   Plane[X] = Plane[Y] = 0.0; Plane[Z] = 1; // piano parallelo all'orizzonte

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   // Ricavo gli oggetti nel buffer (calcolando le coordinate)
   if (PntList.add_vertex_point(pCell, GS_GOOD) == GS_GOOD &&
       PntList.get_count() > 0)
   {
      // <offset> <flag aperta o chiusa> <piano>
      Closed = pCell->isClosed() ? GS_GOOD : GS_BAD;
      if (CoordList << acutBuildList(RTREAL, OffSet, 
                                     RTSHORT, Closed,
                                     RT3DPOINT, Plane, 0) == NULL)
         { delete pCell; return GS_BAD; }
      // scrivo anche i bulge
      if ((CoordList += PntList.to_rb(true)) == NULL) { delete pCell; return GS_BAD; }

      if (pDetectedCls->get_category() != CAT_GRID)
      {
         if (gsc_selObjsBufferFence(CoordList, Mode, *(DetectedLS.ptr_SelSet()), GS_GOOD, GS_GOOD,
                                    pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                                    What) == GS_BAD)
            { delete pCell; return GS_BAD; }
      }
      else
         if (((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListBufferFence(CoordList, Mode, 
                                                                           *(DetectedLS.ptr_KeyList())) == GS_BAD)
            { delete pCell; return GS_BAD; }
   }
   delete pCell;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_selClsObjsInsideSS                 <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava gli oggetti appartenenti ad una classe che sono contenute 
  in ciascuno oggetto nel gruppo EntSS.
  Parametri:
  C_SELSET &EntSS;            entità di riferimento
  int Mode;                   modo (interno o intersezione)
  C_CLASS *pDetectedCls;      Classe da rilevare
  C_LINK_SET &DetectedLS;     Gruppo di oggetti rilevati (gruppo di selezione se la
                              classe non è griglia altrimenti lista di codici di entità)
  int What;                   Flag di filtro; non usato per la griglia (default = GRAPHICAL)
                              ALL       nessun filtro
                              GRAPHICAL per gli oggetti grafici
                              DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsInsideSS(C_SELSET &EntSS, int Mode, C_CLASS *pDetectedCls,
                           C_LINK_SET &DetectedLS, int What = GRAPHICAL)
{
   C_SELSET  PartialSS, ss;
   ads_name  ent;
   long      i = 0;
   ads_point pt1, pt2;
   C_RB_LIST CoordList;

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   if (pDetectedCls->get_category() != CAT_GRID)
      while (EntSS.entname(i++, ent) == GS_GOOD)
      {
         // Non applicabile ad oggetti puntuali
         if (gsc_isPunctualEntity(ent)) 
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         // Ricavo le estensioni dell'oggetto quindi
         // ricavo gli oggetti che stanno dentro in modalità "Crossing"
         if (gsc_get_ent_window(ent, pt1, pt2) == GS_BAD) return GS_BAD;
         if ((CoordList << acutBuildList(RT3DPOINT, pt1, RT3DPOINT, pt2, 0)) == NULL) return GS_BAD;

         //                CoordList,type,ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
         if (gsc_selObjsWindow(CoordList, CROSSING, ss, GS_GOOD, GS_GOOD, GS_GOOD,
                               pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               What) == GS_BAD) return GS_BAD;
         PartialSS.add_selset(ss);
      }
   else // se griglia
      return ((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListInsideSS(EntSS, Mode, 
                                                                        *(DetectedLS.ptr_KeyList()));

   i = 0;
   // Ricavo gli oggetti che sono interni a EntSS (calcolando le coordinate)
   while (PartialSS.entname(i++, ent) == GS_GOOD)
      if (gsc_IsInternalEnt(EntSS, ent, Mode) == GS_GOOD)
         DetectedLS.add_ent(ent);

   return GS_GOOD;
}        
int gsc_selClsObjsInsideGridCell(C_CGRID *pSensorCls, long Key, int Mode, C_CLASS *pDetectedCls,
                                 C_LINK_SET &DetectedLS, int What = GRAPHICAL)
{
   C_SELSET  ss;
   ads_point pt1, pt2;
   C_RB_LIST CoordList;

   // Ricavo il rettangolo della cella
   if (pSensorCls->ptr_grid()->key2Rect(Key, pt1, pt2) == NULL) return GS_BAD;
   if ((CoordList << acutBuildList(RT3DPOINT, pt1, RT3DPOINT, pt2, 0)) == NULL) return GS_BAD;

   DetectedLS.clear();
   DetectedLS.cls = pDetectedCls->ptr_id()->code;
   DetectedLS.sub = pDetectedCls->ptr_id()->sub_code;

   if (pDetectedCls->get_category() != CAT_GRID)
   {
      // ricavo gli oggetti che stanno dentro in modalità "Crossing"
      //                CoordList,type,ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
      if (gsc_selObjsWindow(CoordList, CROSSING, ss, GS_GOOD, GS_GOOD, GS_GOOD,
                            pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                            What) == GS_BAD) return GS_BAD;
   }
   else // se griglia
      return ((C_CGRID *) pDetectedCls)->ptr_grid()->getKeyListInWindow(CoordList, Mode, 
                                                                        *(DetectedLS.ptr_KeyList()));

   ads_name     ent;
   long         i = 0;
   AcDbPolyline *pCell;
   AcDbObject   *pObj;
   AcDbObjectId objId;

   // Ricavo il rettangolo della cella
   if ((pCell = pSensorCls->ptr_grid()->key2Rect(Key)) == NULL) return GS_BAD;
   // Ricavo gli oggetti che sono interni a EntSS (calcolando le coordinate)
   while (ss.entname(i++, ent) == GS_GOOD)
   {
      if (acdbGetObjectId(objId, ent) != Acad::eOk)
         { delete pCell; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
         { delete pCell; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      if (gsc_IsInternalEnt(pCell, (AcDbEntity *) pObj, Mode) == GS_GOOD)
         DetectedLS.add_ent(ent);

      pObj->close();
   }

   delete pCell;

   return GS_GOOD;
}        


/*********************************************************/
/*.doc gsc_getSpatialRules                    <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava il modo di ricerca degli oggetti (INSIDE o CROSSING) e altre
  informazioni indicate in una lista di resbuf contenente la 
  condizione spaziale di ricerca degli oggetti da rilevare.
  Parametri:
  presbuf DetectionSpatialRules; Condizioni spaziali di ricerca
  int     *Mode;                 modo (INSIDE o CROSSING), output
  double *dimX;                  se la ricerca è "finestra" è la dimensione X della finestra
                                 se la ricerca è "cerchio" è la dimensione raggio
                                 se la ricerca è "bufferfence" è la dimensione del buffer
  double *dimY;                  se la ricerca è "finestra" è la dimensione Y della finestra

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getSpatialRules(presbuf DetectionSpatialRules, int *Mode, double *dimX, double *dimY)
{
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      if (gsc_rb2Dbl(DetectionSpatialRules->rbnext, dimX) == GS_BAD ||
          gsc_rb2Dbl(DetectionSpatialRules->rbnext->rbnext, dimY) == GS_BAD ||
          DetectionSpatialRules->rbnext->rbnext->rbnext->restype != RTSTR)
         return GS_BAD;
      *Mode = (gsc_strcmp(DetectionSpatialRules->rbnext->rbnext->rbnext->resval.rstring, _T("inside")) == 0) ? INSIDE : CROSSING;
   }
   else
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      if (gsc_rb2Dbl(DetectionSpatialRules->rbnext, dimX) == GS_BAD ||
          DetectionSpatialRules->rbnext->rbnext->restype != RTSTR)
         return GS_BAD;
      *Mode = (gsc_strcmp(DetectionSpatialRules->rbnext->rbnext->resval.rstring, _T("inside")) == 0) ? INSIDE : CROSSING;
   }
   else
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      if (gsc_rb2Dbl(DetectionSpatialRules->rbnext, dimX) == GS_BAD ||
          DetectionSpatialRules->rbnext->rbnext->restype != RTSTR)
         return GS_BAD;
      *Mode = (gsc_strcmp(DetectionSpatialRules->rbnext->rbnext->resval.rstring, _T("inside")) == 0) ? INSIDE : CROSSING;
   }
   else
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, _T("inside"), FALSE) == 0)
   {
      if (DetectionSpatialRules->rbnext->restype != RTSTR) return GS_BAD;
      *Mode = (gsc_strcmp(DetectionSpatialRules->rbnext->resval.rstring, _T("inside")) == 0) ? INSIDE : CROSSING;
   }
   else
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, FENCE_SPATIAL_COND, FALSE) == 0) // fence
   {
      *Mode =  CROSSING;
   }
   else
   if (gsc_strcmp(DetectionSpatialRules->resval.rstring, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fence
   {
      *Mode =  CROSSING;
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_selClsObjsOnSpatialRules           <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava un link set degli oggetti da rilevare per un dato sensore.
  Parametri:
  C_LINK_SET &SingleSensorLS; Gruppo di oggetti grafici sensori o 
                              lista di un codice di cella (per griglia)
  const TCHAR *Boundary;      Tipo di contorno per ricarca spaziale
  int     *Mode;              Modo di ricerca (INSIDE o CROSSING)
  bool Inverse;               Inverso della rilevazione spaziale
  double *dimX;               se la ricerca è "finestra" è la dimensione X della finestra
                              se la ricerca è "cerchio" è la dimensione raggio
                              se la ricerca è "bufferfence" è la dimensione del buffer
  double *dimY;               se la ricerca è "finestra" è la dimensione Y della finestra
  C_CLASS *pDetectedCls;      Classe da rilevare
  C_LINK_SET &EntDetectedLS;  Link set degli oggetti rilevati

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsOnSpatialRules(C_LINK_SET &SingleSensorLS, 
                                 const TCHAR *Boundary, int Mode, bool Inverse,
                                 double dimX, double dimY, C_CLASS *pDetectedCls,
                                 C_LINK_SET &EntDetectedLS)
{
   C_CLASS    *pSensorCls;
   C_LINK_SET *p, Partial;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (!(pSensorCls = GS_CURRENT_WRK_SESSION->find_class(SingleSensorLS.cls, SingleSensorLS.sub)))
      return GS_BAD;

   if (pDetectedCls->get_category() == CAT_GRID)
   {
      if (Inverse)
         p = &Partial;
      else
         p = &EntDetectedLS;
   }
   else
      p = &EntDetectedLS;

   // finestra
   if (gsc_strcmp(Boundary, WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      if (pSensorCls->get_category() != CAT_GRID)
         if (gsc_selClsObjsWindowSS(*(SingleSensorLS.ptr_SelSet()), dimX, dimY, pDetectedCls, Mode,
                                    EntDetectedLS) == GS_BAD)
            return GS_BAD;
   }
   else
   if (gsc_strcmp(Boundary, CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      if (pSensorCls->get_category() != CAT_GRID)
         if (gsc_selClsObjsCircleSS(*(SingleSensorLS.ptr_SelSet()), dimX, pDetectedCls, Mode, 
                                    EntDetectedLS) == GS_BAD)
            return GS_BAD;
   }
   else
   if (gsc_strcmp(Boundary, FENCE_SPATIAL_COND, FALSE) == 0) // fence
   {
      if (pSensorCls->get_category() != CAT_GRID)
         if (gsc_selClsObjsFenceSS(*(SingleSensorLS.ptr_SelSet()), pDetectedCls, 
                                   EntDetectedLS, true) == GS_BAD)
            return GS_BAD;
   }
   else
   if (gsc_strcmp(Boundary, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
   {
      if (pSensorCls->get_category() != CAT_GRID)
         if (gsc_selClsObjsFenceSS(*(SingleSensorLS.ptr_SelSet()), pDetectedCls, 
                                   EntDetectedLS, false) == GS_BAD)
            return GS_BAD;
   }
   else
   if (gsc_strcmp(Boundary, BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
   {
      // Applicabile ad oggetti polyline + lightpolyline + line + arc + circle + ellipse
      if (pSensorCls->get_category() != CAT_GRID)
      {
         if (gsc_selClsObjsBufferFenceSS(*(SingleSensorLS.ptr_SelSet()), dimX, Mode, pDetectedCls,
                                         EntDetectedLS) == GS_BAD)
            return GS_BAD;
      }
      else // se griglia
      {
         C_BLONG *pKey = (C_BLONG *) SingleSensorLS.ptr_KeyList()->go_top();
         
         if (!pKey) return GS_BAD;
         if (gsc_selClsObjsBufferFenceGridCell((C_CGRID *) pSensorCls, pKey->get_key(), dimX, Mode,
                                               pDetectedCls, *p) == GS_BAD)
            return GS_BAD;
      }
   }
   else
   if (gsc_strcmp(Boundary, _T("inside"), FALSE) == 0)
   {
      if (pSensorCls->get_category() != CAT_GRID)
      {
         C_SELSET GraphicalEntSSNoHatch;
         long     i = 0;
         ads_name ent;

         while (SingleSensorLS.ptr_SelSet()->entname(i++, ent) == GS_GOOD)
            if (gsc_ishatch(ent) == GS_BAD) GraphicalEntSSNoHatch.add(ent);

         // Applicabile ad oggetti polyline (chiuse) + lightpolyline (chiuse) + circle + ellipse
         if (gsc_selClsObjsInsideSS(GraphicalEntSSNoHatch, Mode, pDetectedCls, 
                                    EntDetectedLS) == GS_BAD)
            return GS_BAD;
      }
      else // se griglia
      {
         C_BLONG *pKey = (C_BLONG *) SingleSensorLS.ptr_KeyList()->go_top();
         
         if (!pKey) return GS_BAD;
         if (gsc_selClsObjsInsideGridCell((C_CGRID *) pSensorCls, pKey->get_key(), Mode, 
                                          pDetectedCls, *p) == GS_BAD)
            return GS_BAD;
      }
   }
   else
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (Inverse)
      if (pDetectedCls->get_category() != CAT_GRID)
      {
         C_SELSET Global;

         gsc_ssget(_T("_X"), NULL, NULL, NULL, Global);
         
         Global.intersectClsCode(pDetectedCls->ptr_id()->code,
                                 pDetectedCls->ptr_id()->sub_code,
                                 GRAPHICAL);

         Global.subtract(*(EntDetectedLS.ptr_SelSet()));

         EntDetectedLS.clear();
         EntDetectedLS.add(Global);
      }
      else // se griglia
      {
         C_RECT Rect;
         // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
         ((C_CGRID *)pDetectedCls)->ptr_grid()->getExtension(Rect, true);
         if (((C_CGRID *)pDetectedCls)->ptr_grid()->getKeyListInWindow(Rect, CROSSING, 
                                                                       *(EntDetectedLS.ptr_KeyList())) == GS_BAD)
            return GS_BAD;

         EntDetectedLS.subtract(*p);
      }

   // se sensore e entità da rilevare sono della stessa classe
   if (pSensorCls == pDetectedCls)
      // elimino dal gruppo rilevato gli oggetti appartenenti all'entità sensore
      EntDetectedLS.subtract(SingleSensorLS);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_selClsObjsOnTopoRules           <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Ricava un link set degli oggetti da rilevare usando le regole topologiche
  per un dato sensore.
  Parametri:
  long NodeSubSensorId        Codice dell'entità sensore che appartiene ad una 
                              sottoclasse nodo di simulazione
  C_TOPOLOGY &Topo;           Topologia già caricata in memoria a cui è già
                              stato assegnato il massimo costo
  int ClosestQty;             Se > 0 rappresenta il numero massimo di entità 
                              rilevabili con costo più basso
  C_CLASS *pNodeDetectedSub;  Sottoclasse della simulazione da rilevare
  C_LINK_SET &EntDetectedLS;  Link set degli oggetti rilevati

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_selClsObjsOnTopoRules(long NodeSubSensorId,
                              C_TOPOLOGY &Topo,
                              int ClosestQty,
                              C_SUB *pNodeDetectedSub,
                              C_LINK_SET &EntDetectedLS)
{
   C_INT_LONG  *pItem;
   C_SUB       *pSub;
   double      Cost;
   C_2REAL_LIST Cost_EntId_List;  // lista dei costi e dei codici delle entità
   C_2REAL      *pCost_EntId;
   long        EntId;
   C_SELSET    EntSelSet;

   EntDetectedLS.clear();
   EntDetectedLS.cls = pNodeDetectedSub->ptr_id()->code;
   EntDetectedLS.sub = pNodeDetectedSub->ptr_id()->sub_code;

   Topo.InitForNetAnalysis(); // re-inizializzo per analizzare la rete
   long iNode = gsc_searchTopoNetNode(Topo.NodesVett, Topo.nNodesVett, NodeSubSensorId);
   if (Topo.GetNetPropagation(iNode) == GS_BAD) return GS_BAD;

   pItem = (C_INT_LONG *) Topo.ptr_NetNodes()->get_head();
   while (pItem)
   {
      pSub  = (C_SUB *) Topo.get_cls()->ptr_sub_list()->search_key(pItem->get_key());
      EntId = pItem->get_id();
      if (pNodeDetectedSub->get_key() == pSub->get_key())
      {
         iNode = gsc_searchTopoNetNode(Topo.NodesVett, Topo.nNodesVett, EntId);
         Cost = *((double *) Topo.NodesVett[iNode].Punt);

         if (Cost > 0) // il nodo di partenza viene escluso
         {
            // aggiungo un nuovo costo
            if (Cost_EntId_List.insert_ordered_2double(Cost, EntId) == GS_BAD) return GS_BAD;

            // se devo selezionare solo le entità con costo minore
            if (ClosestQty > 0 && Cost_EntId_List.get_count() > ClosestQty)
               Cost_EntId_List.remove_tail();
         }
      }

      pItem = (C_INT_LONG *) pItem->get_next();
   }

   // Libero un pò di memoria
   gsc_freeTopoNetNode(&(Topo.NodesVett), &(Topo.nNodesVett));
   gsc_freeTopoNetLink(&(Topo.LinksVett), &(Topo.nLinksVett));

   // Ricavo un link set delle entità trovate
   pCost_EntId = (C_2REAL *) Cost_EntId_List.get_head();
   while (pCost_EntId)
   {
      // Provo ad estrarre totalmente le istanze grafiche dell'entità
      EntId = (long) pCost_EntId->get_key_2_double();
      if (pNodeDetectedSub->entExtract(EntId) == GS_BAD) return GS_BAD;
      if (pNodeDetectedSub->get_SelSet(EntId, EntSelSet) == GS_BAD) return GS_BAD;

      EntDetectedLS.add_ent(EntId, EntSelSet);

      pCost_EntId = (C_2REAL *) Cost_EntId_List.get_next();
   }

   return GS_GOOD;
}
 

/*********************************************************/
/*.doc gsc_setStatisticsFilterSensor           <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Se si devono fare statistiche da memorizzare nei sensori.
  Parametri:
  C_CLASS *pSensorCls;              Classe sensore
  long Key;                         Codice sensore
  C_SELSET &EntSS;                  Gruppo di selezione del singolo sensore
  C_PREPARED_CMD *pSensorTempCmd;   Istruzione precompilata per il sensore
  C_RB_LIST &ColValues;             Lista colonna-valore del sensore
  C_LINK_SET &DetectedLS;           Link set entità rilevate
  C_STRING &TempTableRef;           Tabella temporanea dove scrivere il risultato del sensore
  presbuf pRbDestAttribValue;       Puntatore a resbuf di <ColValues> che rappresenta 
                                    l'attributo del sensore che deve memorizzare il 
                                    risultato della funzione di aggregazione 
  const TCHAR *pAggrLisp;           Funzione di aggregazione SQL in fromato ACCESS perchè viene
                                    usata una tabella temporanea.
  const TCHAR *pDestAttribTypeCalc; Tipo di calcolo sull'attributo del sensore 
                                    ("=" per "Sostituisci a", "+" per "Somma a",
                                    "-" per "Sottrai a") (default = NULL)
  bool DestAttribIsnumeric;         Campo sensore destinazione di tipo numerico

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_setStatisticsFilterSensor(C_CLASS *pSensorCls, long Key, C_SELSET &EntSS, 
                                  C_PREPARED_CMD *pSensorTempCmd, C_RB_LIST &ColValues,
                                  C_LINK_SET &DetectedLS, 
                                  C_STRING &TempTableRef,
                                  presbuf pRbDestAttribValue,
                                  const TCHAR *pAggrFun, 
                                  const TCHAR *pDestAttrib, const TCHAR *pDestAttribTypeCalc,
                                  bool DestAttribIsnumeric)
{
   C_CLASS        *pDetectedCls;
   C_STRING       statement;
   C_DBCONNECTION *pVerifiedTabConn;
   _RecordsetPtr  pVerifiedTabConnRs;
   C_RB_LIST      AggrColValues;
   presbuf        p;
  
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (!(pDetectedCls = GS_CURRENT_WRK_SESSION->find_class(DetectedLS.cls, DetectedLS.sub)))
      return GS_BAD;

   if (pDetectedCls->ptr_attrib_list()) // Se la classe rilevata ha db
   {
      if (DetectedLS.ptr_KeyList()->get_count() > 0)
      {
         // Porto tutte le schede filtrate in una tabella temporanea
         if (gsc_filterPrepareStatistics(TempTableRef, DetectedLS, GS_BAD) == GS_BAD)
            return GS_BAD;

         // Calcolo funzione di aggregazione
         if (GS_CURRENT_WRK_SESSION->getVerifiedTabInfo(&pVerifiedTabConn) == GS_BAD)
            return GS_BAD;
         statement = _T("SELECT ");
         statement += pAggrFun;
         statement += _T(" FROM ");
         statement += TempTableRef;

         if (pVerifiedTabConn->OpenRecSet(statement, pVerifiedTabConnRs) == GS_BAD)
            return GS_BAD;
         if (gsc_DBReadRow(pVerifiedTabConnRs, AggrColValues) == GS_BAD)
            { gsc_DBCloseRs(pVerifiedTabConnRs); return GS_BAD; }
         gsc_DBCloseRs(pVerifiedTabConnRs);

         gsc_filterTerminateStatistics(TempTableRef.get_name());

         p = AggrColValues.get_head()->rbnext->rbnext->rbnext; // quarto elemento ((<> <>))

         if (gsc_strcmp(pDestAttribTypeCalc, _T("=")) == 0) // sostituisci
            gsc_RbSubst(pRbDestAttribValue, p);
         else
         {
            double val, AggrVal;
         
            if (pRbDestAttribValue->restype == RTNIL || pRbDestAttribValue->restype == RTNONE)
               val = 0;
            else
               if (gsc_rb2Dbl(pRbDestAttribValue, &val) == GS_BAD) return GS_BAD;

            if (p->restype == RTNIL || p->restype == RTNONE)
               AggrVal = 0;
            else
               if (gsc_rb2Dbl(p, &AggrVal) == GS_BAD) return GS_BAD;

            if (gsc_strcmp(pDestAttribTypeCalc, _T("+")) == 0) val += AggrVal; // somma a
            else val -= AggrVal; // sottrai a

            gsc_RbSubst(pRbDestAttribValue, val);
         }
      }
      else
         if (gsc_strcmp(pDestAttribTypeCalc, _T("=")) == 0) // sostituisci
            if (DestAttribIsnumeric) // Campo sensore di tipo numerico
               gsc_RbSubst(pRbDestAttribValue, 0);
            else
               gsc_RbSubstNIL(pRbDestAttribValue);
   }
   else // classe rilevata senza db
   {
      if (gsc_strcmp(pAggrFun, _T("COUNT(*)"), GS_BAD) == 0)
      {
         double AggrVal = DetectedLS.ptr_SelSet()->length();

         if (pDetectedCls->get_category() != CAT_GRID)
            AggrVal = DetectedLS.ptr_SelSet()->length();
         else
            AggrVal = DetectedLS.ptr_KeyList()->get_count();

         if (gsc_strcmp(pDestAttribTypeCalc, _T("=")) == 0) // sostituisci
            gsc_RbSubst(pRbDestAttribValue, AggrVal);
         else
         {
            double val;
         
            if (pRbDestAttribValue->restype == RTNIL || pRbDestAttribValue->restype == RTNONE)
               val = 0;
            else
               if (gsc_rb2Dbl(pRbDestAttribValue, &val) == GS_BAD) return GS_BAD;

            if (gsc_strcmp(pDestAttribTypeCalc, _T("+")) == 0) val += AggrVal; // somma a
            else val -= AggrVal; // sottrai a

            gsc_RbSubst(pRbDestAttribValue, val);
         }
      }
   }

   // Aggiorno sensore con il risultato della funz. di aggregazione
   if (pSensorCls->upd_data(Key, ColValues, pSensorTempCmd,
                            &EntSS, RECORD_MOD) == GS_BAD)
      // Se il sensore era estratto parzialmente
      if (GS_ERR_COD == eGSPartialEntExtract)
         // riprovo la modifica senza passare EntSS così la funzione tenta l'estrazione
         if (pSensorCls->upd_data(Key, ColValues, pSensorTempCmd,
                                  NULL, RECORD_MOD) == GS_BAD)
            return GS_BAD;

   return GS_GOOD;
}


/***********************************************************/
/*.doc gsc_getSensorExprFromDetectionSQLRules    <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Compone una condizione SQL utilizzando le regole di relazione
  per la rilevazione SQL.
  La condizione verrà usata per leggere i valori dai sensori. Tali valori
  saranno usati come parametri per la ricerca delle entità rilevate tramite la
  funzione gsc_getDetectedExprFromDetectionSQLRules e successivamente il filtro.
  Parametri:
  C_DBCONNECTION *pConn;
  presbuf        DetectionSQLRules;  Regole di relazione per rilevazione SQL
  C_STRING       &SensorExpr;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***********************************************************/
int gsc_getSensorExprFromDetectionSQLRules(C_DBCONNECTION *pConn,
                                           presbuf DetectionSQLRules,
                                           C_STRING &SensorExpr)
{
   int     i = 0;
   presbuf pRel, pSensorExpr;

   SensorExpr.clear();
   pRel = gsc_nth(i++, DetectionSQLRules);
   // ciclo sulle relazioni SQL
   while (pRel)
   {  
      if (pRel->restype == RTLB)
      {
         // espressione SQL del sensore
         if ((pSensorExpr = gsc_nth(2, pRel)) == NULL || pSensorExpr->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

         if (pConn)
         {
            C_STRING FieldName(pSensorExpr->resval.rstring);

            // Considero l'espressione come il nome di un attributo
            // Correggo la sintassi del nome del campo con i delimitatori propri della connessione OLE-DB
            if (gsc_AdjSyntax(FieldName, pConn->get_InitQuotedIdentifier(),
                              pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
            SensorExpr += FieldName;
         }
         else
            SensorExpr += pSensorExpr->resval.rstring;

         if ((pRel = gsc_nth(i++, DetectionSQLRules)) != NULL)
            SensorExpr += _T(',');
      }
      else
         pRel = gsc_nth(i++, DetectionSQLRules);
   }
   
   return GS_GOOD;
}


/***********************************************************/
/*.doc gsc_getDetectedExprFromDetectionSQLRules <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Compone una condizione SQL utilizzando le regole di relazione per la
  rilevazione SQL e i valori letti dal sensore uniti ad eventuali condizioni
  SQL impostate sulle entità da rilevare.
  La condizione verrà poi passata al fltro per trovare le entità in
  relazione al sensore.
  Parametri:
  C_CLASS *pDetectedCls;                  Classe da rilevare
  presbuf DetectionSQLRules;              Regole di relazione per rilevazione SQL
  C_RB_LIST &ColValues;                   Valori letti dal sensore
  C_CLASS_SQL_LIST *pDetectedSQLCondList; Condizioni SQL impostate sulle entità da rilevare
  C_CLASS_SQL_LIST &SQLCondList;          Condizioni SQL da passare al filtro

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***********************************************************/
int gsc_getDetectedExprFromDetectionSQLRules(C_CLASS *pDetectedCls,
                                             presbuf DetectionSQLRules,
                                             C_RB_LIST &SensorColValues,
                                             C_CLASS_SQL_LIST *pDetectedSQLCondList,
                                             C_CLASS_SQL_LIST &SQLCondList)
{
   int         i = 0, nSensorValue = 0;
   presbuf     pRel, pDetectedExpr, pLogicOp, pSensorValue;
   C_CLASS_SQL *pSQLCond;
   C_STRING    DetectedExpr, StrValue;
   C_ID        *pID = pDetectedCls->ptr_id();

   if (pDetectedSQLCondList)
      pDetectedSQLCondList->copy(SQLCondList);
   else
      SQLCondList.remove_all();

   pRel = gsc_nth(i++, DetectionSQLRules);
   // ciclo sulle relazioni SQL
   while (pRel)
   {
      if (pRel->restype == RTLB)
      {
         // espressione SQL entità da rilevare
         if ((pDetectedExpr = gsc_nth(0, pRel)) == NULL || pDetectedExpr->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         // Operatore logico
         if ((pLogicOp = gsc_nth(1, pRel)) == NULL || pLogicOp->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         // Leggo il valore del sensore
         if ((pSensorValue = gsc_nth(1, SensorColValues.nth(nSensorValue++))) == NULL)
            { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

         // Compongo la condizione
         DetectedExpr += pDetectedExpr->resval.rstring;
         DetectedExpr += pLogicOp->resval.rstring;

         switch (pSensorValue->restype)
         {
            case RTSTR:
               // se si tratta di carattere aggiungo i delimitatori
               DetectedExpr += _T('\'');
               StrValue = pSensorValue->resval.rstring;
               StrValue.strtran(_T("'"), _T("''")); // devo raddoppiare eventuali apici
               DetectedExpr += StrValue;
               DetectedExpr += _T('\'');

               // Se la condizione è "<>" aggiungo la condizione IS NULL
               // perchè autocad non considera  valori nulli nell'operazione di diverso
               if (gsc_strcmp(pLogicOp->resval.rstring, _T("<>")) == 0)
               {
                  DetectedExpr += _T(" OR ");
                  DetectedExpr += pDetectedExpr->resval.rstring;
                  DetectedExpr += _T(" IS NULL");
               }

               break;
            case RTNIL: case RTVOID: case RTNONE:
               DetectedExpr += _T(" NULL");
               break;
            default:
            {           
               StrValue.paste(gsc_rb2str(pSensorValue));
               DetectedExpr += StrValue;

               // Se la condizione è "<>" aggiungo la condizione IS NULL
               // perchè autocad non considera  valori nulli nell'operazione di diverso
               if (gsc_strcmp(pLogicOp->resval.rstring, _T("<>")) == 0)
               {
                  DetectedExpr += _T(" OR ");
                  DetectedExpr += pDetectedExpr->resval.rstring;
                  DetectedExpr += _T(" IS NULL");
               }
            }
         }
      }
      else // Operatore OR o AND
      {
         DetectedExpr += _T(' ');         
         DetectedExpr += pRel->resval.rstring;
         DetectedExpr += _T(' ');
      }

      pRel = gsc_nth(i++, DetectionSQLRules);
   }

   if ((pSQLCond = (C_CLASS_SQL *) SQLCondList.search(pID->code, pID->sub_code)) == NULL)
   {
      // Aggiungo la condizione SQL
      if ((pSQLCond = new C_CLASS_SQL) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pSQLCond->set_cls(pID->code);
      pSQLCond->set_sub(pID->sub_code);
      SQLCondList.add_tail(pSQLCond);
   }
   else
   if (gsc_strlen(pSQLCond->get_sql()) > 0)
   {  // Aggiungo la condizione SQL precedente
      if (DetectedExpr.get_name())
      {
         C_STRING dummy;

         dummy = _T("("); // aggiungo una aperta tonda 
         dummy += DetectedExpr;
         DetectedExpr = dummy;

         DetectedExpr += _T(") AND (");
         DetectedExpr += pSQLCond->get_sql();
         DetectedExpr += _T(')');
      }
      else
         DetectedExpr += pSQLCond->get_sql();
   }

   pSQLCond->set_sql(DetectedExpr.get_name());

   return GS_GOOD;
}


/***********************************************************/
/*.doc gsc_delTmpExportFile                     <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Cancella tutti i file che si trovano nella cartella temporanea di GEOsim
  che utilizzano la maschera "SENSOR*.TXT". Sono file temporanei utilizzati per
  memorizzare i codici delle entità che sono state rilevate dal ciascun sensore.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***********************************************************/
int gsc_delTmpExportFile(void)
{
   C_STRING Mask, Prefix, Path;
   presbuf  pname, pattr;

   Prefix = GEOsimAppl::CURRUSRDIR;
   Prefix += _T('\\');
   Prefix += GEOTEMPDIR;
   Prefix += _T("\\");
   Mask = Prefix;
   Mask += _T("SENSOR_*.TXT");

   // cancello eventuali file temporanei SENSOR*.TXT
   if (gsc_adir(Mask.get_name(), &pname, NULL, NULL, &pattr) > 0)
   {
      while (pname) 
      {
         // NON è un direttorio          
         if (pname->restype == RTSTR && *(pattr->resval.rstring + 4) != _T('D'))
         {
            Path = Prefix;
            Path += pname->resval.rstring;

            if (gsc_path_exist(Path) == GS_GOOD)
               if (gsc_delfile(Path) != GS_GOOD)
                  { acutRelRb(pname); acutRelRb(pattr); return GS_BAD; }
         }
         pname = pname->rbnext;
         pattr = pattr->rbnext;
      }
      acutRelRb(pname); acutRelRb(pattr);
   }

   return GS_GOOD;
}


/***********************************************************/
/*.doc gsc_WriteTmpExportFile                   <internal> */
/*+                                                                       
  Funzione di ausilio a gsc_sensor.
  Scrive in un file nominato "SENSOR_<codice chiave sensore>.TXT"
  nella cartella temporanea di GEOsim i codici delle entità che
  sono state rilevate dal sensore con codice chiave noto.
  La prima riga contiene il codice e il sottocodice della classe.
  Parametri:
  long SensorKey;    Codice del sensore
  C_LINK_SET &LS;    Link Set da scrivere

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***********************************************************/
int gsc_WriteTmpExportFile(long SensorKey, C_LINK_SET &LS)
{
   C_STRING Path;
   FILE     *f;
   C_BLONG  *pKey;

   Path = GEOsimAppl::CURRUSRDIR;
   Path += _T('\\');
   Path += GEOTEMPDIR;
   Path += _T("\\SENSOR_");
   Path += SensorKey;
   Path += _T(".TXT");

   if ((f = gsc_fopen(Path.get_name(), _T("w"), ONETEST)) == NULL) return GS_BAD;

   // Scrivo il codice e il sottocodice della classe
   if (fwprintf(f, _T("%d,%d\n"), LS.cls, LS.sub) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // Scrivo dei file testo temporanei con i codici delle entità rilevate
   pKey = (C_BLONG *) LS.ptr_KeyList()->go_top();   
   while (pKey)
   {            
      if (fwprintf(f, _T("%ld\n"), pKey->get_key()) < 0)
         { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      pKey = (C_BLONG *) LS.ptr_KeyList()->go_next();
   }

   return gsc_fclose(f);
}


/*********************************************************/
/*.doc gsc_sensor                             <external> */
/*+                                                                       
  Permette la selezione di un valore di un attributo da una lista di
  valori esistenti nella tabella.
  Parametri:
  C_CLASS *pSensorCls;            Classe sensore
  C_LINK_SET &SensorLS;           Linkset dei sensori (gruppo di oggetti o lista di codici)
  C_CLASS *pDetectedCls;          Classe da rilevare
  presbuf DetectionSpatialRules;  Regole di relazione per rilevazione spaziali;
                                  opzionale (default = NULL)
  bool Inverse;                   Inverso della rilevazione spaziale (default = false)
  presbuf DetectionSQLRules;      Regole di relazione per rilevazione SQL;
                                  opzionale (default = NULL)
  int NearestSpatialSel;          Usa solo le entità più vicine dal punto di vista spaziale
                                  (non usato se <= 0) (default = 0)
  C_TOPOLOGY *pTopo;              Solo per classi simulazioni; puntatore a topologia già caricata in 
                                  memoria a cui è già stato assegnato il massimo costo (default = NULL)
  int NearestTopoSel;             Usa solo le entità più vicine dal punto di vista topologico
                                  (non usato se <= 0) (default = 0)
  C_CLASS_SQL_LIST *pDetectedSQLCondList; Condizioni per limitare gli oggetti da 
                                          rilevare (default = NULL)
  C_LINK_SET *pDetectedLS;        Gruppo di oggetti rilevati (default = NULL)
  const TCHAR *pAggrFun;          Funzione di aggregazione SQL 
                                  (caso Tools = GSStatisticsFilterSensorTool)
                                  (default = NULL)
  const TCHAR *pLispFun;          Funzione GEOLisp (caso Tools = GSDetectedEntUpdateSensorTool);
                                  (default = NULL)
  int        Tools;               Flag indicante l'operazione da eseguire 
                                  (GSStatisticsFilterSensorTool,
                                  GSDetectedEntUpdateSensorTool o 
                                  GSNoneFilterSensorTool); default = GSNoneFilterSensorTool.
  const TCHAR *pDestAttrib;       Attributo del sensore che deve memorizzare il risultato 
                                  della funzione di aggregazione 
                                  (caso Tools = GSStatisticsFilterSensorTool),
                                  Attributo dell'entità rilevate che devono memorizzare
                                  il risultato della funzione GEOLisp 
                                  (caso Tools = GSDetectedEntUpdateSensorTool); (default = NULL)
  const TCHAR *pDestAttribTypeCalc; Tipo di calcolo sull'attributo del sensore 
                                   ("=" per "Sostituisci a", "+" per "Somma a",
                                   "-" per "Sottrai a") (default = NULL)
  int DuplicateObjs;              Flag; se = GS_GOOD gli oggetti rilevati da un sensore
                                  potranno essere rilevati anche da un altro sensore 
                                  altrimenti verranno esclusi dai sensori successivi.
                                  (default = GS_BAD)
  C_SELSET *pRefused;             Gli oggetti rifiutati verranno aggiunti a questo gruppo di selezione
                                  (perchè bloccati da altri utenti, perchè non soddisfano funzioni 
                                  di validità ...); (default = NULL)
  int CounterToVideo;             flag, se = GS_GOOD stampa a video il numero di entità che si 
                                  stanno elaborando (default = GS_BAD)

  DetectionSpatialRules    = <finestra> | <cerchio> | <intercettazione> | <buffer-intercettazione> | <dentro>
  <finestra>               = "window" <dim X> <dim Y> <mode>
  <cerchio>                = "circle" <radius> <mode>
  <intercettazione>        = "fence"
  <buffer-intercettazione> = "bufferfence" <offset> <mode>
  <dentro>                 = "inside" <mode>
  <mode>                   = "inside" | "crossing"

  DetectionSQLRules = <relazione SQL> [<op. logico> <relazione SQL>]
  <relazione SQL>   = <cond. SQL entità da rilevare> <op. confronto> <cond. SQL sensore>
  <cond. SQL entità da rilevare> = per il momento è solo il nome di un attributo
  <cond. SQL sensore>            = per il momento è solo il nome di un attributo
  <op. logico>      = "OR" | "AND"
  <op. confronto>   = "<" | ">" | "<=" | ">=" | "=" | "<>" | "LIKE %VAL" | "LIKE VAL%" | "LIKE %VAL%" 

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_sensor(C_CLASS *pSensorCls, C_LINK_SET &SensorLS, C_CLASS *pDetectedCls,
               presbuf DetectionSpatialRules, bool Inverse,
               presbuf DetectionSQLRules, int NearestSpatialSel,
               C_TOPOLOGY *pTopo, int NearestTopoSel,
               C_CLASS_SQL_LIST *pDetectedSQLCondList, C_LINK_SET *pDetectedLS,
               const TCHAR *pAggrFun, const TCHAR *pLispFun, int Tools, const TCHAR *pDestAttrib, 
               const TCHAR *pDestAttribTypeCalc,
               int DuplicateObjs, C_SELSET *pRefused, int CounterToVideo)
{
   C_LINK_SET          AuxLastFilterResult;
   C_RB_LIST           ColValues, RelSQLColValues, AggrColValues;
   C_SELSET            InternalSensorSS, EntSS;
   C_LINK_SET          InternalDetectedLS, EntDetectedLS, SingleSensorLS;
   long                Key, len_sel_set, qty = 0;
   long                Refused = 0, Locked = 0, PartialExtracted = 0;
   ads_name            ent;
   C_CLASS_SQL_LIST    SQLCondList;
   C_EED               eed;
   int                 Result = GS_GOOD, Mode, Prcnt;
   bool                DestAttribIsnumeric = false;
   double              dimX,  dimY;
   C_PREPARED_CMD_LIST RelSQLTempOldCmdList, SensorTempOldCmdList;
   C_DBCONNECTION      *pSensorConn;
   C_STRING            TempTableRef;
   presbuf             pRbDestAttribValue = NULL;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1077)); // "Filtro a sensori"

   // Se non ci sono regole di relazione per la rilevazione
   if (!DetectionSpatialRules &&          // regole spaziali
       !DetectionSQLRules &&              // regole SQL
       (!pTopo || NearestTopoSel <= 0))   // regole topologiche
      return GS_GOOD;

   if (pSensorCls->get_category() == CAT_GRID)
      return gsc_sensorGrid(((C_CGRID *) pSensorCls), SensorLS, pDetectedCls,
                            DetectionSpatialRules, Inverse,
                            DetectionSQLRules, NearestSpatialSel,
                            pDetectedSQLCondList, pDetectedLS,
                            pAggrFun, pLispFun, Tools, pDestAttrib, 
                            pDestAttribTypeCalc, DuplicateObjs, 
                            NULL, CounterToVideo);


   // memorizzo il risultato del filtro precedente
   GS_LSFILTER.copy(AuxLastFilterResult);

   if (pSensorCls->ptr_fas()) // se la classe ha grafica
      SensorLS.ptr_SelSet()->copy(InternalSensorSS);

   // se esistono delle relazioni spaziali
   if (DetectionSpatialRules && DetectionSpatialRules->restype == RTSTR)
      // Ricavo le info sulla modalità di ricerca
      if (gsc_getSpatialRules(DetectionSpatialRules, &Mode, &dimX, &dimY) == GS_BAD)
         return GS_BAD;

   // Se il sensore appartiene ad una classe con DB e
   // se esiste una funzione di aggregrazione o una funzione di calcolo GEOLisp
   // o esistono delle relazioni SQL
   if ((pSensorCls->ptr_info() && pSensorCls->ptr_attrib_list()) &&
       (((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc) ||
       DetectionSQLRules != NULL))
   {
      C_STRING       SensorExpr;
      C_ATTRIB       *pAttrib;
      C_PREPARED_CMD *pCmd;

      RelSQLTempOldCmdList.remove_all();

      // Ricavo le connessioni ai database TEMP
      if ((pSensorConn = pSensorCls->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
      // Preparo i comandi di lettura dei dati della classe sensore dal temp per una espressione
      if (gsc_getSensorExprFromDetectionSQLRules(pSensorConn, DetectionSQLRules, SensorExpr) == GS_BAD)
         return GS_BAD;
      if ((pCmd = new C_PREPARED_CMD()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pSensorCls->prepare_data(*pCmd, TEMP, SensorExpr.get_name()) == GS_BAD) return GS_BAD;
      RelSQLTempOldCmdList.add_tail(pCmd);

      // Ricavo le connessioni ai database OLD
      if ((pSensorConn = pSensorCls->ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
      // Preparo i comandi di lettura dei dati della classe sensore dal old per una espressione
      if (gsc_getSensorExprFromDetectionSQLRules(pSensorConn, DetectionSQLRules, SensorExpr) == GS_BAD)
         return GS_BAD;
      if ((pCmd = new C_PREPARED_CMD()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pSensorCls->prepare_data(*pCmd, OLD, SensorExpr.get_name()) == GS_BAD) return GS_BAD;
      RelSQLTempOldCmdList.add_tail(pCmd);

      // Preparo i comandi di lettura dei dati della classe sensore dal temp/old
      if (pSensorCls->prepare_data(SensorTempOldCmdList) == GS_BAD)
         return GS_BAD;

      if (Tools == GSStatisticsFilterSensorTool)
      {
         // ricavo la tabella temporanea dei sensori
         if (pSensorCls->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

         if ((pAttrib = (C_ATTRIB *) pSensorCls->ptr_attrib_list()->search_name(pDestAttrib, FALSE)) == NULL)
            return GS_BAD;
         pAttrib->init_ADOType(pSensorCls->ptr_info()->getDBConnection(OLD));
         if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD) DestAttribIsnumeric = true;
     }
   }

   if (Tools == GSMultipleExportSensorTool)
      if (gsc_delTmpExportFile() == GS_BAD) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(113), pSensorCls->ptr_id()->name); // "\n\nEntità classe %s:\n"
      len_sel_set = InternalSensorSS.length();
      StatusBarProgressMeter.Init(len_sel_set);
   }

   if (pDetectedLS)
   {
      pDetectedLS->cls = pDetectedCls->ptr_id()->code;
      pDetectedLS->sub = pDetectedCls->ptr_id()->sub_code;
   }

   // Ciclo per ogni sensore
   while (InternalSensorSS.entname(0, ent) == GS_GOOD)
   {
      InternalSensorSS.subtract_ent(ent);

      if (CounterToVideo == GS_GOOD)
      {
         Prcnt = (int) ((len_sel_set - InternalSensorSS.length()) * 100 / len_sel_set);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }

      // se non si tratta di un oggetto sensore lo elimino dal gruppo
      if (eed.load(ent) == GS_BAD || 
          eed.cls != pSensorCls->ptr_id()->code ||
          eed.sub != pSensorCls->ptr_id()->sub_code)
         continue;

      SingleSensorLS.clear();
      SingleSensorLS.cls = pSensorCls->ptr_id()->code;
      SingleSensorLS.sub = pSensorCls->ptr_id()->sub_code;

      // Se il sensore appartiene ad una classe con DB
      if (pSensorCls->ptr_info() && pSensorCls->ptr_attrib_list())
      {
         // Ricavo il valore chiave dal sensore
         if (pSensorCls->get_Key_SelSet(ent, &Key, EntSS) == GS_BAD)
         {
            if (pRefused) pRefused->add(ent);
            Refused++;
            continue;
         }

         // Elimino il gruppo di oggetti dell'entita da quello dei sensori
         InternalSensorSS.subtract(EntSS);
         EntSS.copy(*(SingleSensorLS.ptr_SelSet()));
         SingleSensorLS.ptr_SelSet()->intersectType(GRAPHICAL); // Filtro solo gli oggetti principali
       
         // Se esiste una funzione di aggregrazione/ GEOLisp o
         // se esistono delle relazioni SQL
         if ((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc ||
             DetectionSQLRules != NULL)
         {
            // leggo i valori di relazione SQL dalla scheda del sensore 
            // con eventuali condizioni di ricerca SQL
            if (pSensorCls->query_data(Key, RelSQLColValues, &RelSQLTempOldCmdList) == GS_BAD)
            {
               if (pRefused) pRefused->add_selset(EntSS);
               Refused++;
               continue;
            }

            if (gsc_getDetectedExprFromDetectionSQLRules(pDetectedCls, DetectionSQLRules, 
                                                         RelSQLColValues, pDetectedSQLCondList,
                                                         SQLCondList) == GS_BAD)
               { Result = GS_BAD; break; }

            // leggo i valori della scheda del sensore
            if (pSensorCls->query_data(Key, ColValues, &SensorTempOldCmdList) == GS_BAD)
            {
               if (pRefused) pRefused->add_selset(EntSS);
               Refused++;
               continue;
            }
         }
         else
            if (gsc_getDetectedExprFromDetectionSQLRules(pDetectedCls, DetectionSQLRules,
                                                         RelSQLColValues, pDetectedSQLCondList,
                                                         SQLCondList) == GS_BAD)
               { Result = GS_BAD; break; }
      }
      else
      {
         if (gsc_getDetectedExprFromDetectionSQLRules(pDetectedCls, DetectionSQLRules, 
                                                      RelSQLColValues, pDetectedSQLCondList,
                                                      SQLCondList) == GS_BAD)
            { Result = GS_BAD; break; }

         SingleSensorLS.ptr_SelSet()->add(ent);
      }

      qty++;

      EntDetectedLS.clear();
      // se esistono delle relazioni spaziali
      if (DetectionSpatialRules && DetectionSpatialRules->restype == RTSTR)
         // Cerco gli oggetti da rilevare con le relazioni spaziali
         if (gsc_selClsObjsOnSpatialRules(SingleSensorLS, 
                                          DetectionSpatialRules->resval.rstring, Mode, Inverse,
                                          dimX, dimY, pDetectedCls, EntDetectedLS) == GS_BAD)
         {
            if (pRefused) pRefused->add_selset(EntSS);
            Refused++;
            continue;
         }

      // se esistono delle relazioni SQL senza relazioni spaziali
      if (DetectionSQLRules && (!DetectionSpatialRules || DetectionSpatialRules->restype != RTSTR))
      {
         // lancio il filtro su classe da rilevare usando
         // le "Regole di relazione per rilevazione SQL" unite ad eventuali
         // condizioni SQL impostate sulle entità da rilevare
         if (gsc_do_sql_filter(pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               SQLCondList, NULL, GS_BAD) == GS_BAD)
         {
            if (pRefused) pRefused->add_selset(EntSS);
            Refused++;
            continue;
         }

         GS_LSFILTER.copy(EntDetectedLS);
      }
      else
         // se esistono delle relazioni spaziali e ci sono oggetti da esaminare
         // allora si deve eseguire il filtro
         if ((DetectionSpatialRules && DetectionSpatialRules->restype == RTSTR &&
             EntDetectedLS.ptr_SelSet()->length() > 0 || EntDetectedLS.ptr_KeyList()->get_count() > 0))
         {
            // se ci sono anche delle condizioni SQL da applicare
            if (SQLCondList.hasCondition())
            {
               // lancio il filtro su classe da rilevare usando
               // le "Regole di relazione per rilevazione spaziali" e
               // le "Regole di relazione per rilevazione SQL" unite ad eventuali
               // condizioni SQL impostate sulle entità da rilevare
               if (gsc_do_sql_filter(pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                                     SQLCondList, &EntDetectedLS, GS_BAD) == GS_BAD)
               {
                  if (pRefused) pRefused->add_selset(EntSS);
                  Refused++;
                  continue;
               }

               GS_LSFILTER.copy(EntDetectedLS);
            }
         }

      // se devo considerare solo le entità con costo topologico minore
      if (pTopo && NearestTopoSel > 0)
      {
         C_LINK_SET TopoDetectedLS;

         if (gsc_selClsObjsOnTopoRules(Key, *pTopo, NearestTopoSel, (C_SUB *) pDetectedCls, 
                                       TopoDetectedLS) == GS_BAD)
            return GS_BAD; 

         if (EntDetectedLS.ptr_SelSet()->length() > 0 || 
             EntDetectedLS.ptr_KeyList()->get_count() > 0)
            EntDetectedLS.intersect(*(TopoDetectedLS.ptr_SelSet()));
         else
            TopoDetectedLS.copy(EntDetectedLS);
      }

      // se sensore e entità da rilevare sono della stessa classe
      if (pSensorCls == pDetectedCls)
         // elimino dal gruppo rilevato gli oggetti appartenenti all'entità sensore
         EntDetectedLS.subtract(Key, EntSS);

      // se devo considerare solo i più vicini
      if (NearestSpatialSel > 0)
         if (EntDetectedLS.ClosestTo(SingleSensorLS, NearestSpatialSel) == GS_BAD)
            return GS_BAD; 

      if (pDetectedLS)
      {
         // Se gli oggetti rilevati non devono già essere rilevati da altri sensori
         if (DuplicateObjs == GS_BAD)
            // elimino dal gruppo rilevato gli oggetti già rilevati
            EntDetectedLS.subtract(*pDetectedLS);

         // Aggiungo il risultato del filtro in pDetectedSS
         pDetectedLS->unite(EntDetectedLS);
      }
      else
         // Se gli oggetti rilevati non devono già essere rilevati da altri sensori
         if (DuplicateObjs == GS_BAD)
         {
            // elimino dal gruppo rilevato gli oggetti già rilevati
            EntDetectedLS.subtract(InternalDetectedLS);

            // Aggiungo il risultato del filtro in InternalDetectedSS
            InternalDetectedLS.unite(EntDetectedLS);
         }

      // se esiste una funzione di aggregrazione/GEOLisp
      if ((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc)
         if (Tools == GSStatisticsFilterSensorTool) // Se si devono fare statistiche da memorizzare nei sensori
         {
            if (!pRbDestAttribValue) pRbDestAttribValue = ColValues.CdrAssoc(pDestAttrib);
            
            if (gsc_setStatisticsFilterSensor(pSensorCls, Key, EntSS, 
                                              ((C_PREPARED_CMD *) SensorTempOldCmdList.get_head()), // Temp
                                              ColValues,
                                              EntDetectedLS, TempTableRef,
                                              pRbDestAttribValue, pAggrFun, pDestAttrib, 
                                              pDestAttribTypeCalc, DestAttribIsnumeric) == GS_BAD)
            {
               if (pRefused) pRefused->add_selset(EntSS);
               Refused++;

               switch (GS_ERR_COD)
               {
                  case eGSReadOnlySession:               
                  case eGSClassIsReadOnly:
                     break;
                  case eGSObjectIsLockedByAnotherUser:
                     Locked++; break;
                  case eGSPartialEntExtract:
                     PartialExtracted++; break;
               }
               
               Result = GS_BAD; break;
            }           
         }
         else // Se devono essere aggiornate le entità rilevate
         if (Tools == GSDetectedEntUpdateSensorTool && 
             EntDetectedLS.ptr_KeyList()->get_count() > 0)
         {
            presbuf  rb;
            TCHAR    value[2 * MAX_LEN_FIELD];
            int      Result = GS_GOOD;
            size_t   ptr = 0;
            C_ATTRIB *pAttrib;
           
            // calcolo funzione GEOLisp

            // Se la classe sensore ha collegamento a database
            if (pSensorCls->ptr_attrib_list())
            {
               // carico le variabili locali a "GEOsim"
               pAttrib = (C_ATTRIB *) pSensorCls->ptr_attrib_list()->get_head();
               while (pAttrib)
               {
                  // ricerca del valore
                  rb = ColValues.CdrAssoc(pAttrib->get_name());
                  // Conversione resbuf in stringa lisp
                  if (rb2lspstr(rb, value, pAttrib->len, pAttrib->dec) == GS_BAD)
                     { Result = GS_BAD; break; }
                  if (alloc_var(TRUE, NULL, &ptr, _T('L'), _T("GEOsim"), 
                                pAttrib->get_name(), value, NULL) == GS_BAD) 
                     { Result = GS_BAD; break; }

                  pAttrib = (C_ATTRIB *) pAttrib->get_next();   
               }
            }

            if (Result == GS_GOOD &&
                (rb = gsc_exec_lisp(_T("GEOsim"), pLispFun, NULL)) != NULL)
            {
               AggrColValues << acutBuildList(RTLB, RTLB, RTSTR, pDestAttrib, 0);
               AggrColValues.link_tail(rb);

               if (gsc_strcmp(pDestAttribTypeCalc, _T("+")) == 0) // somma a
                  AggrColValues += acutBuildList(RTSTR, _T("+"), 0);
               else
               if (gsc_strcmp(pDestAttribTypeCalc, _T("-")) == 0) // sottrai a
                  AggrColValues += acutBuildList(RTSTR, _T("-"), 0);

               AggrColValues += acutBuildList(RTLE, RTLE, 0);

               // Aggiorno entità rilevate
               if (pDetectedCls->upd_data(*(EntDetectedLS.ptr_KeyList()), AggrColValues, 
                                          RECORD_MOD) == GS_BAD)
               {
                  if (pRefused) pRefused->add_selset(GEOsimAppl::REFUSED_SS);

                  // Conto quante entità sono state rifiutate
                  C_LONG_BTREE RefusedKeyList;
                  long         i = 0, RefusedKey;

                  while (GEOsimAppl::REFUSED_SS.entname(i++, ent) == GS_GOOD)
                     if (pDetectedCls->getKeyValue(ent, &RefusedKey) == GS_GOOD)
                        RefusedKeyList.add(&RefusedKey); // Non vengono inseriti elementi doppi

                  Refused += RefusedKeyList.get_count();
               }
            }
            release_var(_T("GEOsim"));
         }

      // Se si devono fare esportazioni per sensore
      if (Tools == GSMultipleExportSensorTool &&
          EntDetectedLS.ptr_KeyList()->get_count() > 0)
         // Scrivo dei file testo temporanei con i codici delle entità rilevate
         if (gsc_WriteTmpExportFile(Key, EntDetectedLS) == GS_BAD)
            { Result = GS_BAD; break; }
   }
   // ripristino il risultato del filtro precedente
   AuxLastFilterResult.copy(GS_LSFILTER);

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      acutPrintf(gsc_msg(308), qty, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
      if (Locked > 0) acutPrintf(gsc_msg(770), Locked);   // "\n%ld entità GEOsim bloccata/e da un' altro utente."
      if (PartialExtracted > 0) acutPrintf(gsc_msg(35), PartialExtracted); // "\n%ld entità GEOsim estratte parzialmente."         
   }

   // Se ci oggetti con problemi
   if (Locked > 0 || PartialExtracted > 0)
      gsc_ddalert(gsc_msg(84)); // "Alcune entità di GEOsim non saranno salvate"

   return Result;
}
int gsc_sensorGrid(C_CGRID *pSensorCls, C_LINK_SET &SensorLS, C_CLASS *pDetectedCls,
                   presbuf DetectionSpatialRules, bool Inverse,
                   presbuf DetectionSQLRules, int NearestSpatialSel,
                   C_CLASS_SQL_LIST *pDetectedSQLCondList, C_LINK_SET *pDetectedLS,
                   const TCHAR *pAggrFun, const TCHAR *pLispFun, int Tools, const TCHAR *pDestAttrib, 
                   const TCHAR *pDestAttribTypeCalc,
                   int DuplicateObjs, C_LINK_SET *pRefused, int CounterToVideo)
{
   C_LINK_SET          AuxLastFilterResult;
   C_RB_LIST           ColValues, RelSQLColValues, AggrColValues;
   C_SELSET            EntSS;
   C_LONG_BTREE        InternalSensorKeyList;
   C_BLONG             *pKey;
   C_LINK_SET          InternalDetectedLS, EntDetectedLS, SingleSensorLS;
   long                Key, len_key_list, qty = 0;
   long                Refused = 0, Locked = 0;
   C_CLASS_SQL_LIST    SQLCondList;
   int                 Result = GS_GOOD, Mode, Prcnt;
   bool                DestAttribIsnumeric = false;
   double              dimX,  dimY;
   C_PREPARED_CMD_LIST RelSQLTempOldCmdList, SensorTempOldCmdList;
   C_DBCONNECTION      *pSensorConn;
   C_STRING            TempTableRef;
   presbuf             pRbDestAttribValue = NULL;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1077)); // "Filtro a sensori"

   // Se non ci sono regole di relazione per la rilevazione
   if (!DetectionSpatialRules && !DetectionSQLRules) return GS_GOOD;

   // memorizzo il risultato del filtro precedente
   GS_LSFILTER.copy(AuxLastFilterResult);

   InternalSensorKeyList.add_list(*(SensorLS.ptr_KeyList()));

   // se esistono delle relazioni spaziali
   if (DetectionSpatialRules && DetectionSpatialRules->restype == RTSTR)
      // Ricavo le info sulla modalità di ricerca
      if (gsc_getSpatialRules(DetectionSpatialRules, &Mode, &dimX, &dimY) == GS_BAD)
         return GS_BAD;

   // Se il sensore appartiene ad una classe con DB e
   // se esiste una funzione di aggregrazione o una funzione di calcolo GEOLisp
   // o esistono delle relazioni SQL
   if ((pSensorCls->ptr_info() && pSensorCls->ptr_attrib_list()) &&
       (((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc) ||
       DetectionSQLRules != NULL))
   {
      C_STRING       SensorExpr;
      C_ATTRIB       *pAttrib;
      C_PREPARED_CMD *pCmd;

      RelSQLTempOldCmdList.remove_all();

      // Ricavo le connessioni ai database TEMP
      if ((pSensorConn = pSensorCls->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
      // Preparo i comandi di lettura dei dati della classe sensore dal temp per una espressione
      if (gsc_getSensorExprFromDetectionSQLRules(pSensorConn, DetectionSQLRules, SensorExpr) == GS_BAD)
         return GS_BAD;
      if ((pCmd = new C_PREPARED_CMD()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pSensorCls->prepare_data(*pCmd, TEMP, SensorExpr.get_name()) == GS_BAD) return GS_BAD;
      RelSQLTempOldCmdList.add_tail(pCmd);

      // Ricavo le connessioni ai database OLD
      if ((pSensorConn = pSensorCls->ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
      // Preparo i comandi di lettura dei dati della classe sensore dal old per una espressione
      if (gsc_getSensorExprFromDetectionSQLRules(pSensorConn, DetectionSQLRules, SensorExpr) == GS_BAD)
         return GS_BAD;
      if ((pCmd = new C_PREPARED_CMD()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pSensorCls->prepare_data(*pCmd, OLD, SensorExpr.get_name()) == GS_BAD) return GS_BAD;
      RelSQLTempOldCmdList.add_tail(pCmd);

      // Preparo i comandi di lettura dei dati della classe sensore dal temp/old
      if (pSensorCls->prepare_data(SensorTempOldCmdList) == GS_BAD)
         return GS_BAD;

      if (Tools == GSStatisticsFilterSensorTool)
      {
         // ricavo la tabella temporanea dei sensori
         if (pSensorCls->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

         if ((pAttrib = (C_ATTRIB *) pSensorCls->ptr_attrib_list()->search_name(pDestAttrib, FALSE)) == NULL)
            return GS_BAD;
         pAttrib->init_ADOType(pSensorCls->ptr_info()->getDBConnection(OLD));
         if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD) DestAttribIsnumeric = true;
      }
   }

   if (Tools == GSMultipleExportSensorTool)
      if (gsc_delTmpExportFile() == GS_BAD) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(113), pSensorCls->ptr_id()->name); // "\n\nEntità classe %s:\n"
      len_key_list = InternalSensorKeyList.get_count();
      StatusBarProgressMeter.Init(len_key_list);
   }

   if (pDetectedLS)
   {
      pDetectedLS->cls = pDetectedCls->ptr_id()->code;
      pDetectedLS->sub = pDetectedCls->ptr_id()->sub_code;
   }

   // Ciclo per ogni sensore
   while ((pKey = (C_BLONG *) InternalSensorKeyList.go_top()))
   {
      Key = pKey->get_key();
      InternalSensorKeyList.remove_at();

      if (CounterToVideo == GS_GOOD)
      {
         Prcnt = (int) ((len_key_list - InternalSensorKeyList.get_count()) * 100 / len_key_list);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }

      SingleSensorLS.clear();
      SingleSensorLS.cls = pSensorCls->ptr_id()->code;
      SingleSensorLS.sub = pSensorCls->ptr_id()->sub_code;
      SingleSensorLS.ptr_KeyList()->add(&Key);

      // Se esiste una funzione di aggregrazione/ GEOLisp o
      // se esistono delle relazioni SQL
      if ((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc ||
          DetectionSQLRules != NULL)
      {
         // leggo i valori di relazione SQL dalla scheda del sensore 
         // con eventuali condizioni di ricerca SQL
         if (pSensorCls->query_data(Key, RelSQLColValues, &RelSQLTempOldCmdList) == GS_BAD)
         {
            Refused++;
            continue;
         }

         if (gsc_getDetectedExprFromDetectionSQLRules(pDetectedCls, DetectionSQLRules, 
                                                      RelSQLColValues, pDetectedSQLCondList,
                                                      SQLCondList) == GS_BAD)
            { Result = GS_BAD; break; }

         // leggo i valori della scheda del sensore
         if (pSensorCls->query_data(Key, ColValues, &SensorTempOldCmdList) == GS_BAD)
         {
            Refused++;
            continue;
         }
      }
      else
         if (gsc_getDetectedExprFromDetectionSQLRules(pDetectedCls, DetectionSQLRules,
                                                      RelSQLColValues, pDetectedSQLCondList,
                                                      SQLCondList) == GS_BAD)
            { Result = GS_BAD; break; }

      qty++;

      EntDetectedLS.clear();
      // se esistono delle relazioni spaziali
      if (DetectionSpatialRules && DetectionSpatialRules->restype == RTSTR)
         // Cerco gli oggetti da rilevare con le relazioni spaziali
         if (gsc_selClsObjsOnSpatialRules(SingleSensorLS, 
                                          DetectionSpatialRules->resval.rstring, Mode, Inverse,
                                          dimX, dimY, pDetectedCls, EntDetectedLS) == GS_BAD)
         {
            Refused++;
            continue;
         }

      // se ci sono oggetti da esaminare allora si deve eseguire il filtro
      if (EntDetectedLS.ptr_SelSet()->length() > 0 || EntDetectedLS.ptr_KeyList()->get_count() > 0)
      {
         // lancio il filtro su classe da rilevare usando
         // le "Regole di relazione per rilevazione spaziali" e
         // le "Regole di relazione per rilevazione SQL" unite ad eventuali
         // condizioni SQL impostate sulle entità da rilevare
         if (gsc_do_sql_filter(pDetectedCls->ptr_id()->code, pDetectedCls->ptr_id()->sub_code,
                               SQLCondList, &EntDetectedLS, GS_BAD) == GS_BAD)
         {
            Refused++;
            continue;
         }

         GS_LSFILTER.copy(EntDetectedLS);
      }

      // se sensore e entità da rilevare sono della stessa classe
      if (pSensorCls == pDetectedCls)
         // elimino dal gruppo rilevato gli oggetti appartenenti all'entità sensore
         EntDetectedLS.subtract(Key, EntSS);

      if (pDetectedLS)
      {
         // Se gli oggetti rilevati non devono già essere rilevati da altri sensori
         if (DuplicateObjs == GS_BAD)
            // elimino dal gruppo rilevato gli oggetti già rilevati
            EntDetectedLS.subtract(*pDetectedLS);

         // Aggiungo il risultato del filtro in pDetectedSS
         pDetectedLS->unite(EntDetectedLS);
      }
      else
         // Se gli oggetti rilevati non devono già essere rilevati da altri sensori
         if (DuplicateObjs == GS_BAD)
         {
            // elimino dal gruppo rilevato gli oggetti già rilevati
            EntDetectedLS.subtract(InternalDetectedLS);

            // Aggiungo il risultato del filtro in InternalDetectedSS
            InternalDetectedLS.unite(EntDetectedLS);
         }

      // se devo considerare solo i più vicini
      if (NearestSpatialSel > 0)
         if (EntDetectedLS.ClosestTo(EntDetectedLS, NearestSpatialSel) == GS_BAD)
            return GS_BAD; 

      // se esiste una funzione di aggregrazione/GEOLisp
      if ((pAggrFun || pLispFun) && pDestAttrib && pDestAttribTypeCalc)
         if (Tools == GSStatisticsFilterSensorTool) // Se si devono fare statistiche da memorizzare nei sensori
         {
            if (!pRbDestAttribValue) pRbDestAttribValue = ColValues.CdrAssoc(pDestAttrib);
            
            if (gsc_setStatisticsFilterSensor(pSensorCls, Key, EntSS, 
                                              ((C_PREPARED_CMD *) SensorTempOldCmdList.get_head()), // Temp
                                              ColValues,
                                              EntDetectedLS, TempTableRef,
                                              pRbDestAttribValue, pAggrFun, pDestAttrib, 
                                              pDestAttribTypeCalc, DestAttribIsnumeric) == GS_BAD)
            {
               Refused++;

               switch (GS_ERR_COD)
               {
                  case eGSReadOnlySession:               
                  case eGSClassIsReadOnly:
                     break;
                  case eGSObjectIsLockedByAnotherUser:
                     Locked++; break;
               }
               
               Result = GS_BAD; break;
            }           
         }
         else // Se devono essere aggiornate le entità rilevate
         if (Tools == GSDetectedEntUpdateSensorTool && 
             EntDetectedLS.ptr_KeyList()->get_count() > 0)
         {
            presbuf  rb;
            TCHAR    value[2 * MAX_LEN_FIELD];
            int      Result = GS_GOOD;
            size_t   ptr = 0;
            C_ATTRIB *pAttrib;
           
            // calcolo funzione GEOLisp

            // Se la classe sensore ha collegamento a database
            if (pSensorCls->ptr_attrib_list())
            {
               // carico le variabili locali a "GEOsim"
               pAttrib = (C_ATTRIB *) pSensorCls->ptr_attrib_list()->get_head();
               while (pAttrib)
               {
                  // ricerca del valore
                  rb = ColValues.CdrAssoc(pAttrib->get_name());
                  // Conversione resbuf in stringa lisp
                  if (rb2lspstr(rb, value, pAttrib->len, pAttrib->dec) == GS_BAD)
                     { Result = GS_BAD; break; }
                  if (alloc_var(TRUE, NULL, &ptr, _T('L'), _T("GEOsim"), 
                                pAttrib->get_name(), value, NULL) == GS_BAD) 
                     { Result = GS_BAD; break; }

                  pAttrib = (C_ATTRIB *) pAttrib->get_next();   
               }
            }

            if (Result == GS_GOOD &&
                (rb = gsc_exec_lisp(_T("GEOsim"), pLispFun, NULL)) != NULL)
            {
               AggrColValues << acutBuildList(RTLB, RTLB, RTSTR, pDestAttrib, 0);
               AggrColValues.link_tail(rb);

               if (gsc_strcmp(pDestAttribTypeCalc, _T("+")) == 0) // somma a
                  AggrColValues += acutBuildList(RTSTR, _T("+"), 0);
               else
               if (gsc_strcmp(pDestAttribTypeCalc, _T("-")) == 0) // sottrai a
                  AggrColValues += acutBuildList(RTSTR, _T("-"), 0);

               AggrColValues += acutBuildList(RTLE, RTLE, 0);

               // Aggiorno entità rilevate
               if (pDetectedCls->upd_data(*(EntDetectedLS.ptr_KeyList()), AggrColValues, 
                                          RECORD_MOD) == GS_BAD)
                  Refused += EntDetectedLS.ptr_KeyList()->get_count();
            }
            release_var(_T("GEOsim"));
         }

      // Se si devono fare esportazioni per sensore
      if (Tools == GSMultipleExportSensorTool &&
          EntDetectedLS.ptr_KeyList()->get_count() > 0)
         // Scrivo dei file testo temporanei con i codici delle entità rilevate
         if (gsc_WriteTmpExportFile(Key, EntDetectedLS) == GS_BAD)
            { Result = GS_BAD; break; }
   }
   // ripristino il risultato del filtro precedente
   AuxLastFilterResult.copy(GS_LSFILTER);

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      acutPrintf(gsc_msg(308), qty, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
      if (Locked > 0) acutPrintf(gsc_msg(770), Locked);   // "\n%ld entità GEOsim bloccata/e da un' altro utente."
  }

   // Se ci oggetti con problemi
   if (Locked > 0)
      gsc_ddalert(gsc_msg(84)); // "Alcune entità di GEOsim non saranno salvate"

   return Result;
}


/*****************************************************************************/
/*.doc gsc_setTileDclSensorRulesBuilder                                      */
/*+
  Questa funzione setta tutti i controlli della DCL "SensorRulesBuilder" in modo 
  appropriato secondo il tipo di selezione.
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_main_sensor_Struct *CommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileDclSensorRulesBuilder(ads_hdlg dcl_id, 
                                            Common_Dcl_main_sensor_Struct *CommonStruct)
{
   C_STRING  JoinOp, Boundary, SelType, LocSummary, Trad, dummy;
   C_RB_LIST CoordList;
   bool      Inverse;

   if (CommonStruct->pSensorCls)
   {
      ads_set_tile(dcl_id, _T("SensorClassName"), CommonStruct->pSensorCls->get_name());
      ads_mode_tile(dcl_id, _T("SensorObjsSelect"), MODE_ENABLE); 
      ads_mode_tile(dcl_id, _T("SensorSpatialRules"), MODE_ENABLE);
      if (CommonStruct->UseSensorSpatialRules)
      {
         ads_mode_tile(dcl_id, _T("SetSensorSpatialRules"), MODE_ENABLE);
         ads_mode_tile(dcl_id, _T("Location_Descr"), MODE_ENABLE);
      }
      else
      {
         ads_mode_tile(dcl_id, _T("SetSensorSpatialRules"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("Location_Descr"), MODE_DISABLE);
      }

      if (CommonStruct->pSensorCls->get_category() != CAT_GRID)
         dummy = CommonStruct->SensorLS.ptr_SelSet()->length();
      else
         dummy = CommonStruct->SensorLS.ptr_KeyList()->get_count();

      ads_set_tile(dcl_id, _T("NumSelectedSensorObjs"), dummy.get_name());
   }
   else
   {
      ads_set_tile(dcl_id, _T("SensorClassName"), GS_EMPTYSTR);
      ads_mode_tile(dcl_id, _T("SensorObjsSelect"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("SensorSpatialRules"), MODE_DISABLE);
      ads_set_tile(dcl_id, _T("NumSelectedSensorObjs"), _T("0"));
   }

   ads_set_tile(dcl_id, _T("UseSensorSpatialRules"), (CommonStruct->UseSensorSpatialRules) ? _T("1") : _T("0"));

   if (CommonStruct->pDetectedCls)
   {
      ads_set_tile(dcl_id, _T("DetectedClassName"), CommonStruct->pDetectedCls->get_name());
      ads_mode_tile(dcl_id, _T("DuplicateDetectedObjs"), MODE_ENABLE);

      if (CommonStruct->pDetectedCls && CommonStruct->pDetectedCls->ptr_info() && CommonStruct->pDetectedCls->ptr_attrib_list())
         ads_mode_tile(dcl_id, _T("DetectedSQL"), MODE_ENABLE);
      else
         ads_mode_tile(dcl_id, _T("DetectedSQL"), MODE_DISABLE);

      if (CommonStruct->pSensorCls && CommonStruct->pSensorCls->ptr_info() && CommonStruct->pSensorCls->ptr_attrib_list() &&
          CommonStruct->pDetectedCls && CommonStruct->pDetectedCls->ptr_info() && CommonStruct->pDetectedCls->ptr_attrib_list())
      {
         ads_mode_tile(dcl_id, _T("SensorSQLRules"), MODE_ENABLE);
         if (CommonStruct->UseSensorSQLRules)
         {
            ads_mode_tile(dcl_id, _T("SetSensorSQLRules"), MODE_ENABLE);
            ads_mode_tile(dcl_id, _T("SensorSQLRules_Descr"), MODE_ENABLE);
         }
         else
         {
            ads_mode_tile(dcl_id, _T("SetSensorSQLRules"), MODE_DISABLE);
            ads_mode_tile(dcl_id, _T("SensorSQLRules_Descr"), MODE_DISABLE);
         }
      }
      else
         ads_mode_tile(dcl_id, _T("SensorSQLRules"), MODE_DISABLE);
   }
   else
   {
      ads_set_tile(dcl_id, _T("DetectedClassName"), GS_EMPTYSTR);
      ads_mode_tile(dcl_id, _T("DuplicateDetectedObjs"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("SensorSQLRules"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("DetectedSQL"), MODE_DISABLE);
   }
   ads_set_tile(dcl_id, _T("UseSensorSQLRules"), 
                (CommonStruct->UseSensorSQLRules) ? _T("1") : _T("0"));
   ads_set_tile(dcl_id, _T("DuplicateDetectedObjs"),
                (CommonStruct->DuplicateDetectedObjs == GS_GOOD) ? _T("1") : _T("0"));

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(CommonStruct->SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }

   if (Inverse)
      LocSummary = gsc_msg(316); // "Inverso di: "

   // Traduzioni per SelType
   if (gsc_trad_SelType(SelType, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   LocSummary += _T(" ");
   // Traduzioni per Boundary
   if (gsc_trad_Boundary(Boundary, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   ads_set_tile(dcl_id, _T("Location_Descr"), LocSummary.get_name());

   // se impostata una condizione spaziale o SQL
   if (SelType.len() > 0 || CommonStruct->DetectionSQLRules.get_head())
   {
      ads_mode_tile(dcl_id, _T("NearestSensorSpatialRules"), MODE_ENABLE);

      if (CommonStruct->UseNearestSpatialSel) // se si vuole cercare solo i spazialmente più vicini
      {
         ads_set_tile(dcl_id, _T("UseNearestSensorSpatialRules"), _T("1"));
         ads_mode_tile(dcl_id, _T("SpatialNearest_Qty"), MODE_ENABLE);
      }
      else
      {
         ads_set_tile(dcl_id, _T("UseNearestSensorSpatialRules"), _T("0"));
         ads_mode_tile(dcl_id, _T("SpatialNearest_Qty"), MODE_DISABLE);
      }
   }
   else
      ads_mode_tile(dcl_id, _T("NearestSensorSpatialRules"), MODE_DISABLE);

   dummy = CommonStruct->NearestSpatialSel;
   ads_set_tile(dcl_id, _T("SpatialNearest_Qty"), dummy.get_name());

   // se la classe sensore e la classe da rilevare sono appartenenti alla stessa
   // classe simulazione e sono entrambe classi puntuali
   if (CommonStruct->pSensorCls && CommonStruct->pDetectedCls &&
       CommonStruct->pSensorCls->ptr_id()->code == CommonStruct->pDetectedCls->ptr_id()->code &&
       CommonStruct->pSensorCls->is_subclass() == GS_GOOD && CommonStruct->pDetectedCls->is_subclass() == GS_GOOD &&
       (CommonStruct->pSensorCls->get_type() == TYPE_NODE || CommonStruct->pSensorCls->get_type() == TYPE_TEXT) &&
       (CommonStruct->pDetectedCls->get_type() == TYPE_NODE || CommonStruct->pDetectedCls->get_type() == TYPE_TEXT))
   {
      ads_mode_tile(dcl_id, _T("NearestSensorTopoRules"), MODE_ENABLE);

      if (CommonStruct->UseNearestTopoRules) // se si vuole cercare solo i topologicamente più vicini
      {
         ads_set_tile(dcl_id, _T("UseNearestSensorTopoRules"), _T("1"));
         ads_mode_tile(dcl_id, _T("TopoNearest_Qty"), MODE_ENABLE);
      }
      else
      {
         ads_set_tile(dcl_id, _T("UseNearestSensorTopoRules"), _T("0"));
         ads_mode_tile(dcl_id, _T("TopoNearest_Qty"), MODE_DISABLE);
      }
   }
   else
      ads_mode_tile(dcl_id, _T("NearestSensorTopoRules"), MODE_DISABLE);

   ads_set_tile(dcl_id, _T("NearestSensorTopoRules_File"), CommonStruct->TopoRulesFile.get_name());
   dummy = CommonStruct->NearestTopoSel;
   ads_set_tile(dcl_id, _T("TopoNearest_Qty"), dummy.get_name());
   dummy = CommonStruct->NearestTopoMaxCost;
   ads_set_tile(dcl_id, _T("TopoNearest_MaxCost"), dummy.get_name());

   // tools
   if (CommonStruct->pSensorCls && CommonStruct->pDetectedCls)
      ads_mode_tile(dcl_id, _T("Tools"), MODE_ENABLE);
   else
      ads_mode_tile(dcl_id, _T("Tools"), MODE_DISABLE);

   switch (CommonStruct->Tools)
   {
      case GSNoneFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("None"));
         break;
      case GSHighlightFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Highlight"));
         break;
      case GSExportFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Export"));
         break;
      case GSStatisticsFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("Statistics"));
         break;
      case GSBrowseFilterSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("GraphBrowse"));
         break;
      case GSDetectedEntUpdateSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("UpdDetected"));
         break;
      case GSMultipleExportSensorTool:
         ads_set_tile(dcl_id, _T("Tools"), _T("SensorExport"));
         break;
   }

   // Regole SQL
   if (CommonStruct->DetectionSQLRules.get_head())
   {
      presbuf p;
      int i = 0;
      C_STRING SQLRules;

      while ((p = CommonStruct->DetectionSQLRules.nth(i++)))
      {
         SQLRules += gsc_nth(0, p)->resval.rstring;
         SQLRules += _T(' ');
         SQLRules += gsc_nth(1, p)->resval.rstring;
         SQLRules += _T(' ');
         SQLRules += gsc_nth(2, p)->resval.rstring;
         
         if ((p = CommonStruct->DetectionSQLRules.nth(i++))) // Operatore logico
         {
            SQLRules += _T(' ');
            SQLRules += p->resval.rstring;
            SQLRules += _T(' ');
         }
      }
      ads_set_tile(dcl_id, _T("SensorSQLRules_Descr"), SQLRules.get_name());
   }

   // Tasto OK
   if (CommonStruct->pSensorCls && CommonStruct->pDetectedCls)
      ads_mode_tile(dcl_id, _T("accept"), MODE_ENABLE); 
   else
      ads_mode_tile(dcl_id, _T("accept"), MODE_DISABLE); 

   return GS_GOOD;
}

static void CALLB dcl_SensorRulesBuilder_SensorClassSelect(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, SENSOR_CLS_SEL);
}

static void CALLB dcl_SensorRulesBuilder_SensorObjsSelect(ads_callback_packet *dcl)
{
   C_CLASS *pCls = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data))->pSensorCls;

   if (pCls) ads_done_dialog(dcl->dialog, OBJ_SEL);         
}

static void CALLB dcl_SensorRulesBuilder_DetectedClassSelect(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DETECTED_CLS_SEL);
}

static void CALLB dcl_SensorRulesBuilder_UseSensorSpatialRules(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("UseSensorSpatialRules"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
   {
      pData->UseSensorSpatialRules = TRUE;
      if (pData->SpatialSel.len() == 0)
         ads_done_dialog(dcl->dialog, LOCATION_SEL);
   }
   else
      pData->UseSensorSpatialRules = FALSE;

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_SetSensorSpatialRules(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, LOCATION_SEL);
}

// Nearest Spatial Rules

static void CALLB dcl_SensorRulesBuilder_SpatialNearest_Option(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("UseNearestSensorSpatialRules"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      pData->UseNearestSpatialSel = TRUE;
   else
      pData->UseNearestSpatialSel = FALSE;
   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_SpatialNearest_Qty(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];
   int   num;

   if (ads_get_tile(dcl->dialog, _T("SpatialNearest_Qty"), val, 10) != RTNORM) return;
   
   if ((num = _wtoi(val)) > 0) pData->NearestSpatialSel = num;

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

// Nearest Topological Rules

static void CALLB dcl_SensorRulesBuilder_TopoNearest_Option(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("UseNearestSensorTopoRules"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      pData->UseNearestTopoRules = TRUE;
   else
      pData->UseNearestTopoRules = FALSE;
   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_TopoNearest_Qty(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];
   int   num;

   if (ads_get_tile(dcl->dialog, _T("TopoNearest_Qty"), val, 10) != RTNORM) return;
   
   if ((num = _wtoi(val)) > 0) pData->NearestTopoSel = num;

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_TopoNearest_MaxCost(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];
   double   num;

   if (ads_get_tile(dcl->dialog, _T("TopoNearest_MaxCost"), val, 10) != RTNORM) return;
   
   if ((num = _wtof(val)) > 0) pData->NearestTopoMaxCost = num;

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_SetNearestSensorTopoRules_File(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   C_STRING FileName;

   if (pData->TopoRulesFile.get_name() == NULL && GS_CURRENT_WRK_SESSION)
   {
      // imposto il direttorio di ricerca del file 
      FileName = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      FileName += _T("\\");
      FileName += GEOSUPPORTDIR;
   }
   else
      FileName = pData->TopoRulesFile;

   // "Seleziona il file delle regole per la visita topologica"
   if (gsc_GetFileD(gsc_msg(493), FileName, _T("rst"), UI_LOADFILE_FLAGS, FileName) == RTNORM)
      pData->TopoRulesFile = FileName;
   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}


static void CALLB dcl_SensorRulesBuilder_UseSensorSQLRules(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("UseSensorSQLRules"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      pData->UseSensorSQLRules = TRUE;
   else
      pData->UseSensorSQLRules = FALSE;

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

void dcl_add_Attrib_list(ads_hdlg dcl_id, const TCHAR *CtrlName, C_CLASS *pCls,
                         int AllFields = GS_GOOD)
{
   C_ATTRIB *pAttrib;

   ads_start_list(dcl_id, CtrlName, LIST_NEW, 0);
   if (pCls->ptr_attrib_list() && pCls->ptr_info())
   { // Riempie la lista del controllo indicato con i nomi dei campi della classe
      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      ads_start_list(dcl_id, CtrlName, LIST_NEW, 0);
      gsc_add_list(GS_EMPTYSTR);
      
      while (pAttrib)
      {  // Se non si vogliono tutti gli attributi 
         if (AllFields == GS_BAD)
         {  // Visualizzo solo quelli che non sono calcolati escludendo l'attributo chiave
            if (pAttrib->is_calculated() != GS_GOOD &&
                pCls->ptr_info()->key_attrib.comp(pAttrib->get_name(), FALSE) != 0)
            gsc_add_list(pAttrib->get_name());
         }
         else 
            gsc_add_list(pAttrib->get_name());

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
   }
   ads_end_list();
}

void dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(ads_hdlg dcl_id,
                                                       presbuf pRel, TCHAR *CtrlNum,
                                                       C_CLASS *pDetectedCls,
                                                       C_CLASS *pSensorCls,
                                                       presbuf pLogicOp = NULL)
{
   presbuf  p;
   C_STRING CtrlName, pos;

   p = gsc_nth(0, pRel);
   if (p)
   {
      CtrlName = _T("DetectedAttr");
      CtrlName += CtrlNum;
      pos = pDetectedCls->ptr_attrib_list()->getpos_name(p->resval.rstring);
      ads_set_tile(dcl_id, CtrlName.get_name(), pos.get_name());
   }

   p = gsc_nth(1, pRel);
   if (p)
   {
      TCHAR *CompOp[] = {_T("="), _T(">"), _T("<"), _T("<>"), _T(">="), _T("<=")}; // definita anche in dcl_SensorRulesBuilder_SetSensorSQLRules

      for (int i = 0; i < ELEMENTS(CompOp); i++)
         if (gsc_strcmp(CompOp[i], p->resval.rstring) == 0)
         {
            CtrlName = _T("Cond");
            CtrlName += CtrlNum;
            pos      = i;
            ads_set_tile(dcl_id, CtrlName.get_name(), pos.get_name());
            break;
         }
   }

   p = gsc_nth(2, pRel);
   if (p)
   {
      CtrlName = _T("SensorAttr");
      CtrlName += CtrlNum;
      pos = pSensorCls->ptr_attrib_list()->getpos_name(p->resval.rstring);
      ads_set_tile(dcl_id, CtrlName.get_name(), pos.get_name());
   }

   if (pLogicOp)
   {
      TCHAR *LogicOp[] = {GS_EMPTYSTR, _T("OR"), _T("AND")}; // definita anche in dcl_SensorRulesBuilder_SetSensorSQLRules

      for (int i = 0; i < ELEMENTS(LogicOp); i++)
         if (gsc_strcmp(LogicOp[i], pLogicOp->resval.rstring) == 0)
         {
            CtrlName = _T("LogicOp");
            CtrlName += CtrlNum;
            pos      = i;
            ads_set_tile(dcl_id, CtrlName.get_name(), pos.get_name());
            break;
         }
   }
}

static void CALLB dcl_SetSensorSQLRules_accept(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR    DetectedAttr[10], Op[10], SensorAttr[10], Op1[10];
   ads_hdlg dcl_id = dcl->dialog;
   TCHAR    *CompOp[] = {_T("="), _T(">"), _T("<"), _T("<>"), _T(">="), _T("<=")}; // definita anche in dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile
   TCHAR    *LogicOp[] = {GS_EMPTYSTR, _T("OR"), _T("AND")}; // definita anche in dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile
   int      i, MaxNumRules = 5; // Numero massimo di regole ammesse dalla DCL
   C_STRING DetectedAttrCtrlName, CondCtrlName, SensorAttrCtrlName, LogicOpCtrlName;

   pData->DetectionSQLRules << acutBuildList(RTLB, 0);

   for (i = 1; i <= MaxNumRules; i++)
   {
      DetectedAttrCtrlName = _T("DetectedAttr"); DetectedAttrCtrlName += i;
      CondCtrlName         = _T("Cond"); CondCtrlName += i;
      SensorAttrCtrlName   = _T("SensorAttr"); SensorAttrCtrlName += i;
      LogicOpCtrlName      = _T("LogicOp"); LogicOpCtrlName += (i - 1);

      ads_get_tile(dcl_id, DetectedAttrCtrlName.get_name(), DetectedAttr, 10);
      ads_get_tile(dcl_id, CondCtrlName.get_name(), Op, 10);
      ads_get_tile(dcl_id, SensorAttrCtrlName.get_name(), SensorAttr, 10);

      if (i == 1) // Solo per la prima condizione
      {
         if (gsc_strcmp(DetectedAttr, _T("0")) == 0 || gsc_strcmp(SensorAttr, _T("0")) == 0)
            break; // esco dal ciclo
         pData->DetectionSQLRules += acutBuildList(RTLB,
                                                   RTSTR, pData->pDetectedCls->ptr_attrib_list()->getptr_at(_wtoi(DetectedAttr))->get_name(),
                                                   RTSTR, CompOp[_wtoi(Op)],
                                                   RTSTR, pData->pSensorCls->ptr_attrib_list()->getptr_at(_wtoi(SensorAttr))->get_name(),
                                                   RTLE, 0);
      }
      else
      {
         LogicOpCtrlName = _T("LogicOp"); LogicOpCtrlName += (i - 1);
         ads_get_tile(dcl_id, LogicOpCtrlName.get_name(), Op1, 10);

         if (gsc_strcmp(DetectedAttr, _T("0")) == 0 || gsc_strcmp(SensorAttr, _T("0")) == 0 || 
             gsc_strcmp(Op1, _T("0")) == 0)
            break; // esco dal ciclo

         pData->DetectionSQLRules += acutBuildList(RTSTR, LogicOp[_wtoi(Op1)],
                                                   RTLB,
                                                   RTSTR, pData->pDetectedCls->ptr_attrib_list()->getptr_at(_wtoi(DetectedAttr))->get_name(),
                                                   RTSTR, CompOp[_wtoi(Op)],
                                                   RTSTR, pData->pSensorCls->ptr_attrib_list()->getptr_at(_wtoi(SensorAttr))->get_name(),
                                                   RTLE, 0);
      }
   }

   if (pData->DetectionSQLRules.GetCount() == 1)
      pData->DetectionSQLRules.remove_all();
   else
      pData->DetectionSQLRules += acutBuildList(RTLE, 0);

   ads_done_dialog(dcl->dialog, DLGOK);
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SetSensorSQLRules" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SensorRulesBuilder_SetSensorSQLRules_help(ads_callback_packet *dcl)
   { gsc_help(IDH_RilevamentoSQL); }

static void CALLB dcl_SensorRulesBuilder_SetSensorSQLRules(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   int      status = DLGOK, dcl_file;
   ads_hdlg dcl_id;
   C_STRING path;   

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return;

   ads_new_dialog(_T("SetSensorSQLRules"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return; }

   dcl_add_Attrib_list(dcl_id, _T("DetectedAttr1"), pData->pDetectedCls);
   dcl_add_Attrib_list(dcl_id, _T("DetectedAttr2"), pData->pDetectedCls);
   dcl_add_Attrib_list(dcl_id, _T("DetectedAttr3"), pData->pDetectedCls);
   dcl_add_Attrib_list(dcl_id, _T("DetectedAttr4"), pData->pDetectedCls);
   dcl_add_Attrib_list(dcl_id, _T("DetectedAttr5"), pData->pDetectedCls);

   dcl_add_Attrib_list(dcl_id, _T("SensorAttr1"), pData->pSensorCls);
   dcl_add_Attrib_list(dcl_id, _T("SensorAttr2"), pData->pSensorCls);
   dcl_add_Attrib_list(dcl_id, _T("SensorAttr3"), pData->pSensorCls);
   dcl_add_Attrib_list(dcl_id, _T("SensorAttr4"), pData->pSensorCls);
   dcl_add_Attrib_list(dcl_id, _T("SensorAttr5"), pData->pSensorCls);

   if (pData->DetectionSQLRules.get_head())
   {
      presbuf  p = pData->DetectionSQLRules.nth(0), q;
      C_STRING pos;
      
      if (p)
      {
         q = pData->DetectionSQLRules.nth(1);
         dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(dcl_id, p, _T("1"),
                                                           pData->pDetectedCls,
                                                           pData->pSensorCls, q);

         if (q && (p = pData->DetectionSQLRules.nth(2)))
         {
            q = pData->DetectionSQLRules.nth(3);
            dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(dcl_id, p, _T("2"),
                                                              pData->pDetectedCls,
                                                              pData->pSensorCls, q);

            if (q && (p = pData->DetectionSQLRules.nth(4)))
            {
               q = pData->DetectionSQLRules.nth(5);
               dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(dcl_id, p, _T("3"),
                                                                 pData->pDetectedCls,
                                                                 pData->pSensorCls, q);

               if (q && (p = pData->DetectionSQLRules.nth(6)))
               {
                  q = pData->DetectionSQLRules.nth(7);
                  dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(dcl_id, p, _T("4"),
                                                                    pData->pDetectedCls,
                                                                    pData->pSensorCls, q);

                  if (q && (p = pData->DetectionSQLRules.nth(8)))
                  {
                     dcl_SensorRulesBuilder_SetSensorSQLRules_set_tile(dcl_id, p, _T("5"),
                                                                       pData->pDetectedCls,
                                                                       pData->pSensorCls);
                  }
               }
            }
         }
      }
   }

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_SetSensorSQLRules_accept);
   ads_client_data_tile(dcl_id, _T("accept"), pData);

   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SensorRulesBuilder_SetSensorSQLRules_help);

   ads_start_dialog(dcl_id, &status);

   ads_unload_dialog(dcl_file);

   gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
}

static void CALLB dcl_SensorRulesBuilder_DuplicateDetectedObjs(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[10];

   if (ads_get_tile(dcl->dialog, _T("DuplicateDetectedObjs"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      pData->DuplicateDetectedObjs = GS_GOOD;
   else
      pData->DuplicateDetectedObjs = GS_BAD;
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SensorRulesBuilder_DetectedSQL_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Selezioneentitdarilevare); } 

static void CALLB dcl_SensorRulesBuilder_DetectedSQL(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   C_STRING path;
   int      status = DLGOK, dcl_file;
   ads_hdlg dcl_id;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return;

   ads_new_dialog(_T("DetectedSQL"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) return;

   // Condizioni SQL Classi
   Common_Dcl_main_filter_Struct dummyStruct;

   pData->sql_list.copy(dummyStruct.sql_list);
   dummyStruct.ClsSQLSel   = 0;
   dummyStruct.SecSQLSel   = 0;

   gsc_setTileClsQryListDclmain_filter(dcl_id, &dummyStruct);
   // Condizioni SQL Secondarie
   gsc_setTileSecQryListDclmain_filter(dcl_id, &dummyStruct);

   ads_action_tile(dcl_id, _T("ClsQryList"), (CLIENTFUNC) dcl_main_filter_ClsQryList);
   ads_client_data_tile(dcl_id, _T("ClsQryList"), &dummyStruct);
   ads_action_tile(dcl_id, _T("ClassSQL"), (CLIENTFUNC) dcl_main_filter_ClassSQL);
   ads_client_data_tile(dcl_id, _T("ClassSQL"), &dummyStruct);
   ads_action_tile(dcl_id, _T("ClassQrySlid"), (CLIENTFUNC) dcl_main_filter_ClassQrySlid);
   ads_client_data_tile(dcl_id, _T("ClassQrySlid"), &dummyStruct);
   ads_action_tile(dcl_id, _T("SecQryList"), (CLIENTFUNC) dcl_main_filter_SecQryList);
   ads_client_data_tile(dcl_id, _T("SecQryList"), &dummyStruct);
   ads_action_tile(dcl_id, _T("SecSQL"), (CLIENTFUNC) dcl_main_filter_SecSQL);
   ads_client_data_tile(dcl_id, _T("SecSQL"), &dummyStruct);
   ads_action_tile(dcl_id, _T("SecAggr"), (CLIENTFUNC) dcl_main_filter_SecAggr);
   ads_client_data_tile(dcl_id, _T("SecAggr"), &dummyStruct);
   ads_action_tile(dcl_id, _T("SecQrySlid"), (CLIENTFUNC) dcl_main_filter_SecQrySlid);
   ads_client_data_tile(dcl_id, _T("SecQrySlid"), &dummyStruct);

   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SensorRulesBuilder_DetectedSQL_help);

   ads_start_dialog(dcl_id, &status);

   if (status == DLGOK)                                // uscita con tasto OK
      dummyStruct.sql_list.copy(pData->sql_list);
}

static void CALLB dcl_SetSensorDetectedUpdate_accept(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR    CalcFunc[255], DestAttrib[10], Op[10];
   ads_hdlg dcl_id = dcl->dialog;

   ads_get_tile(dcl_id, _T("Calc_func"), CalcFunc, 255);
   ads_get_tile(dcl_id, _T("CalcType"), Op, 10);
   ads_get_tile(dcl_id, _T("DestAttrib"), DestAttrib, 10);

   pData->Lisp_Fun = CalcFunc;
      
   switch (_wtoi(Op))
   {
      case 0: pData->SensorAttribTypeCalc = gsc_msg(94); break; // "Somma a"
      case 1: pData->SensorAttribTypeCalc = gsc_msg(97); break; // "Sostituisci a"
      case 2: pData->SensorAttribTypeCalc = gsc_msg(95); break; // "Sottrai a"
   }

   if (pData->pDetectedCls && pData->pDetectedCls->ptr_attrib_list())
   {
      C_ATTRIB *pAttrib = (C_ATTRIB *) pData->pDetectedCls->ptr_attrib_list()->get_head();
      int      i = 1;

      // prendo l'n-esimo elemento saltando quelli calcolati e quello chiave
      while (pAttrib)
      {  // Considero solo quelli che non sono calcolati escludendo l'attributo chiave
         if (pAttrib->is_calculated() != GS_GOOD &&
             pData->pDetectedCls->ptr_info()->key_attrib.comp(pAttrib->get_name(), FALSE) != 0)
         {
            if (i == _wtoi(DestAttrib)) break;
            i = i + 1;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
      if (pAttrib)
         pData->DetectedAttrib = pAttrib->get_name();
   }

   pData->Tools = GSDetectedEntUpdateSensorTool;

   ads_done_dialog(dcl->dialog, DLGOK);
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SensorRulesBuilder_DetectededUpdate_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Strumenti); } 

/*****************************************************************************/
/*.doc gsc_SetSensorDetectedUpdate                                           */
/*+
  Questa funzione permette di impostare le informazioni relative alla 
  memorizzazione di un attributo (direttamente o tramite funzione GEOLisp)
  del sensore in un attributo delle entità rilevate.
  Parametri:
  Common_Dcl_main_sensor_Struct *pData;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_SetSensorDetectedUpdate(Common_Dcl_main_sensor_Struct *pData)
{
   int       status = DLGOK, dcl_file;
   C_STRING  buffer;   
   ads_hdlg  dcl_id;

   // scompongo la condizione impostata
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   buffer = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(buffer, &dcl_file) == RTERROR) return GS_BAD;
    
   ads_new_dialog(_T("SetSensorDetectedUpdate"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SensorRulesBuilder_DetectededUpdate_help);

   if (pData->Lisp_Fun.get_name())
      ads_set_tile(dcl_id, _T("Calc_func"), pData->Lisp_Fun.get_name());

   dcl_add_Attrib_list(dcl_id, _T("DestAttrib"), pData->pDetectedCls, GS_BAD);
   if (pData->DetectedAttrib.get_name())
   {
      C_ATTRIB *pAttrib = (C_ATTRIB *) pData->pDetectedCls->ptr_attrib_list()->get_head();
      int      i = 1;

      // prendo l'n-esimo elemento saltando quelli calcolati e quello chiave
      while (pAttrib)
      {  // Considero solo quelli che non sono calcolati escludendo l'attributo chiave
         if (pAttrib->is_calculated() != GS_GOOD &&
             pData->pDetectedCls->ptr_info()->key_attrib.comp(pAttrib->get_name(), FALSE) != 0)
         {
            if (pData->DetectedAttrib.comp(pAttrib->get_name(), FALSE) == 0) break;
            i = i + 1;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      buffer = i;
      ads_set_tile(dcl_id, _T("DestAttrib"), buffer.get_name());
   }

   // list box
   ads_start_list(dcl_id, _T("CalcType"), LIST_NEW, 0);
   gsc_add_list(gsc_msg(94)); // "Somma a"
   gsc_add_list(gsc_msg(97)); // "Sostituisci a"
   gsc_add_list(gsc_msg(95)); // "Sottrai a"
   ads_end_list();

   if (pData->SensorAttribTypeCalc.comp(gsc_msg(94)) == 0) // "Somma a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("0"));
   else if (pData->SensorAttribTypeCalc.comp(gsc_msg(97)) == 0) // "Sostituisci a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("1"));
   else if (pData->SensorAttribTypeCalc.comp(gsc_msg(95)) == 0) // "Sottrai a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("2"));

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_SetSensorDetectedUpdate_accept);
   ads_client_data_tile(dcl_id, _T("accept"), pData);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   return GS_GOOD;
}


static void CALLB dcl_SetSensorStatisticSQL_accept(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR    DetectedAttr[10], Op[10], DestAttrib[10], Op1[10];
   ads_hdlg dcl_id = dcl->dialog;

   ads_get_tile(dcl_id, _T("aggrSQL"), Op, 10);
   ads_get_tile(dcl_id, _T("attrib"), DetectedAttr, 10);
   ads_get_tile(dcl_id, _T("CalcType"), Op1, 10);
   ads_get_tile(dcl_id, _T("DestAttrib"), DestAttrib, 10);

   // non verifico DetectedAttr se si effettua un "Count *"
   if ((gsc_strcmp(Op, _T("2")) == 0 || gsc_strcmp(DetectedAttr, _T("0")) != 0)
       && gsc_strcmp(DestAttrib, _T("0")) != 0)
   {
      pData->Aggr_Fun = AGGR_FUN_LIST[_wtoi(Op)];
      if (gsc_strcmp(DetectedAttr, _T("0")) != 0 && pData->pDetectedCls->ptr_attrib_list())
         pData->DetectedAttrib = pData->pDetectedCls->ptr_attrib_list()->getptr_at(_wtoi(DetectedAttr))->get_name();
      
      switch (_wtoi(Op1))
      {
         case 0: pData->SensorAttribTypeCalc = gsc_msg(94); break; // "Somma a"
         case 1: pData->SensorAttribTypeCalc = gsc_msg(97); break; // "Sostituisci a"
         case 2: pData->SensorAttribTypeCalc = gsc_msg(95); break; // "Sottrai a"
      }

      if (pData->pSensorCls && pData->pSensorCls->ptr_attrib_list())
      {
         C_ATTRIB *pAttrib = (C_ATTRIB *) pData->pSensorCls->ptr_attrib_list()->get_head();
         int      i = 1;

         // prendo l'n-esimo elemento saltando quelli calcolati e quello chiave
         while (pAttrib)
         {  // Considero solo quelli che non sono calcolati escludendo l'attributo chiave
            if (pAttrib->is_calculated() != GS_GOOD &&
                pData->pSensorCls->ptr_info()->key_attrib.comp(pAttrib->get_name(), FALSE) != 0)
            {
               if (i == _wtoi(DestAttrib)) break;
               i = i + 1;
            }

            pAttrib = (C_ATTRIB *) pAttrib->get_next();
         }
         if (pAttrib)
            pData->SensorAttrib = pAttrib->get_name();
      }

      pData->Tools = GSStatisticsFilterSensorTool;
   }

   ads_done_dialog(dcl->dialog, DLGOK);
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SensorRulesBuilder_Statistic_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Statistiche); } 


/*****************************************************************************/
/*.doc gsc_SetSensorStatisticSQL                                             */
/*+
  Questa funzione permette di impostare le informazioni relative alla memorizzazione
  di una valore statistico in un attributo del sensore.
  Parametri:
  Common_Dcl_main_sensor_Struct *pData;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_SetSensorStatisticSQL(Common_Dcl_main_sensor_Struct *pData)
{
   int       status = DLGOK, dcl_file, i;
   C_STRING  buffer;   
   ads_hdlg  dcl_id;

   // scompongo la condizione impostata
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   buffer = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(buffer, &dcl_file) == RTERROR) return GS_BAD;
    
   ads_new_dialog(_T("SetSensorStatisticSQL"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SensorRulesBuilder_Statistic_help);

   if (pData->pDetectedCls->ptr_attrib_list())
   {
      dcl_add_Attrib_list(dcl_id, _T("attrib"), pData->pDetectedCls);
      if (pData->DetectedAttrib.get_name())
      {
         buffer = pData->pDetectedCls->ptr_attrib_list()->getpos_name(pData->DetectedAttrib.get_name());
         ads_set_tile(dcl_id, _T("attrib"), buffer.get_name());
      }  

      for (i = 0; i < ELEMENTS(AGGR_FUN_LIST); i++)
         if (gsc_strcmp(AGGR_FUN_LIST[i], pData->Aggr_Fun.get_name()) == 0)
         {
            buffer = i;
            ads_set_tile(dcl_id, _T("aggrSQL"), buffer.get_name());
            break;
         }
   }
   else // caso per spaghetti
      if (gsc_strcmp(_T("Count *"), pData->Aggr_Fun.get_name()) == 0)
         ads_set_tile(dcl_id, _T("aggrSQL"), _T("2")); // terza posizione nella lista

   dcl_add_Attrib_list(dcl_id, _T("DestAttrib"), pData->pSensorCls, GS_BAD);
   if (pData->SensorAttrib.get_name())
   {
      C_ATTRIB *pAttrib = (C_ATTRIB *) pData->pSensorCls->ptr_attrib_list()->get_head();
      int      i = 1;

      // prendo l'n-esimo elemento saltando quelli calcolati e quello chiave
      while (pAttrib)
      {  // Considero solo quelli che non sono calcolati escludendo l'attributo chiave
         if (pAttrib->is_calculated() != GS_GOOD &&
             pData->pSensorCls->ptr_info()->key_attrib.comp(pAttrib->get_name(), FALSE) != 0)
         {
            if (pData->SensorAttrib.comp(pAttrib->get_name(), FALSE) == 0) break;
            i = i + 1;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      buffer = i;
      ads_set_tile(dcl_id, _T("DestAttrib"), buffer.get_name());
   }

   // list box
   ads_start_list(dcl_id, _T("CalcType"), LIST_NEW, 0);
   gsc_add_list(gsc_msg(94)); // "Somma a"
   gsc_add_list(gsc_msg(97)); // "Sostituisci a"
   gsc_add_list(gsc_msg(95)); // "Sottrai a"
   ads_end_list();

   if (pData->SensorAttribTypeCalc.comp(gsc_msg(94)) == 0) // "Somma a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("0"));
   else if (pData->SensorAttribTypeCalc.comp(gsc_msg(97)) == 0) // "Sostituisci a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("1"));
   else if (pData->SensorAttribTypeCalc.comp(gsc_msg(95)) == 0) // "Sottrai a"
      ads_set_tile(dcl_id, _T("CalcType"), _T("2"));

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_SetSensorStatisticSQL_accept);
   ads_client_data_tile(dcl_id, _T("accept"), pData);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   return GS_GOOD;
}


static void CALLB dcl_SensorRulesBuilder_Tools(ads_callback_packet *dcl)
{
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));
   TCHAR val[TILE_STR_LIMIT];

   if (ads_get_tile(dcl->dialog, _T("Tools"), val, TILE_STR_LIMIT) == RTNORM)
   {
      if (gsc_strcmp(val, _T("Highlight")) == 0)
         pData->Tools = GSHighlightFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("Export")) == 0)
         // Se la classe rilevata non ha DB
         if (!pData->pDetectedCls || pData->pDetectedCls->get_category() == CAT_SPAGHETTI)
         {
            gsc_ddalert(gsc_msg(228)); // "Opzione non consentita perchè la classe rilevata è priva di database."
            pData->Tools = GSHighlightFilterSensorTool;
            ads_set_tile(dcl->dialog, _T("Tools"), _T("Highlight"));
         }
         else
            pData->Tools = GSExportFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("SensorExport")) == 0)
         // Se la classe rilevata non ha DB
         if (!pData->pDetectedCls || pData->pDetectedCls->get_category() == CAT_SPAGHETTI)
         {
            gsc_ddalert(gsc_msg(228)); // "Opzione non consentita perchè la classe rilevata è priva di database."
            pData->Tools = GSHighlightFilterSensorTool;
            ads_set_tile(dcl->dialog, _T("Tools"), _T("Highlight"));
         }
         else
            pData->Tools = GSMultipleExportSensorTool;
      else
      if (gsc_strcmp(val, _T("Statistics")) == 0)
         // Se la classe rilevata non ha DB
         if (!pData->pDetectedCls)
         {
            gsc_ddalert(gsc_msg(228)); // "Opzione non consentita perchè la classe rilevata è priva di database."
            pData->Tools = GSHighlightFilterSensorTool;
            ads_set_tile(dcl->dialog, _T("Tools"), _T("Highlight"));
         }
         else
         {
            gsc_SetSensorStatisticSQL(pData); // setta "pData->Tools = GSStatisticsFilterSensorTool"
            gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
         }
      else
      if (gsc_strcmp(val, _T("UpdDetected")) == 0)
         // Se la classe rilevata non ha DB
         if (!pData->pDetectedCls || pData->pDetectedCls->get_category() == CAT_SPAGHETTI)
         {
            gsc_ddalert(gsc_msg(228)); // "Opzione non consentita perchè la classe rilevata è priva di database."
            pData->Tools = GSHighlightFilterSensorTool;
            ads_set_tile(dcl->dialog, _T("Tools"), _T("Highlight"));
         }
         else
         {
            gsc_SetSensorDetectedUpdate(pData); // setta "pData->Tools = GSDetectedEntUpdateSensorTool"
            gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
         }
      else
      if (gsc_strcmp(val, _T("GraphBrowse")) == 0)
         // Se la classe rilevata non ha DB
         if (!pData->pDetectedCls || pData->pDetectedCls->get_category() == CAT_SPAGHETTI)
         {
            gsc_ddalert(gsc_msg(228)); // "Opzione non consentita perchè la classe rilevata è priva di database."
            pData->Tools = GSHighlightFilterSensorTool;
            ads_set_tile(dcl->dialog, _T("Tools"), _T("Highlight"));
         }
         else
            pData->Tools = GSBrowseFilterSensorTool;
      else
      if (gsc_strcmp(val, _T("None")) == 0)
         pData->Tools = GSNoneFilterSensorTool;
   }
}

// ACTION TILE : click su tasto SAVE
static void CALLB dcl_SensorRulesBuilder_Save(ads_callback_packet *dcl)
{
   C_STRING FileName, DefaultFileName, LastSensorFile, Dir;
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSensorFile"), LastSensorFile) == GS_BAD ||
       gsc_dir_exist(LastSensorFile) == GS_BAD)
   {
      // imposto il direttorio di ricerca del file 
      Dir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      Dir += _T("\\");
      Dir += GEOQRYDIR;
   }
   else
      if (gsc_dir_from_path(LastSensorFile, Dir) == GS_BAD) return;

   // "Sensore"
   if (gsc_get_tmp_filename(Dir.get_name(), gsc_msg(657), _T(".sns"), DefaultFileName) == GS_BAD)
       return;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), DefaultFileName, _T("sns"), UI_SAVEFILE_FLAGS, FileName) == RTNORM)
      if (gsc_SaveSensorConfig(pData, FileName, _T("SENSOR")) == GS_GOOD)
         gsc_setPathInfoToINI(_T("LastSpatialQryFile"), DefaultFileName);
}

// ACTION TILE : click su tasto LOAD //
static void CALLB dcl_SensorRulesBuilder_Load(ads_callback_packet *dcl)
{ 
   C_STRING FileName, LastSensorFile, Dir;
   Common_Dcl_main_sensor_Struct *pData = ((Common_Dcl_main_sensor_Struct *) (dcl->client_data));

   // Se non esiste alcun file precedente
   if (gsc_getPathInfoFromINI(_T("LastSensorFile"), LastSensorFile) == GS_BAD ||
       gsc_dir_exist(LastSensorFile) == GS_BAD)
   {
      // imposto il direttorio di ricerca del file
      Dir = GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir();
      Dir += _T("\\");
      Dir += GEOQRYDIR;
   }
   else
      if (gsc_dir_from_path(LastSensorFile, Dir) == GS_BAD) return;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), Dir, _T("sns"), UI_LOADFILE_FLAGS, FileName) == RTNORM)
   {
      if (gsc_LoadSensorConfig(pData, FileName, _T("SENSOR")) == GS_GOOD)
      {
         gsc_setPathInfoToINI(_T("LastSpatialQryFile"), FileName);
         gsc_setTileDclSensorRulesBuilder(dcl->dialog, pData);
      }
   }
}

// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SensorRulesBuilder_help(ads_callback_packet *dcl)
   { gsc_help(IDH_RICERCAPERSENSORI); } 

/*****************************************************************************/
/*.doc gsc_setTileDclSensorSpatialRules                                      */
/*+
  Questa funzione setta tutti i controlli della DCL "SpatialSel" in modo 
  appropriato secondo il tipo di selezione ("all", "window", "circle", "polygon", 
  "fence", "bufferfence"),
  il modo di selezione ("crossing", "inside") e la lista delle coordinate.
  Parametri:
  ads_hdlg  dcl_id;     
  Common_Dcl_SpatialSel_Struct *CommonStruct;
  C_CLASS *pSensorCls;                          Puntatore alla classe sensore (opzionale)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_setTileDclSensorSpatialRules(ads_hdlg dcl_id, Common_Dcl_SpatialSel_Struct *CommonStruct,
                                     C_CLASS *pSensorCls = NULL)
{
   if (pSensorCls)
   {
      if (pSensorCls->get_category() != CAT_GRID)
         switch (pSensorCls->get_type())
         {
            case TYPE_NODE:
            case TYPE_TEXT:
               ads_mode_tile(dcl_id, _T("polygon"), MODE_DISABLE);
               ads_mode_tile(dcl_id, _T("fencebutstartend"), MODE_DISABLE);
               if (gsc_strcmp(CommonStruct->Boundary.get_name(), POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
                  CommonStruct->Boundary.clear();
               break;
            case TYPE_POLYLINE:
               ads_mode_tile(dcl_id, _T("window"), MODE_DISABLE);
               ads_mode_tile(dcl_id, _T("circle"), MODE_DISABLE);
               ads_mode_tile(dcl_id, _T("polygon"), MODE_DISABLE);
               if (gsc_strcmp(CommonStruct->Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0 || // window
                   gsc_strcmp(CommonStruct->Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0 || // cerchio
                   gsc_strcmp(CommonStruct->Boundary.get_name(), POLYGON_SPATIAL_COND, FALSE) == 0)  // polygon
                  CommonStruct->Boundary.clear();
               break;
            case TYPE_SURFACE:
               ads_mode_tile(dcl_id, _T("window"), MODE_DISABLE);
               ads_mode_tile(dcl_id, _T("circle"), MODE_DISABLE);
               ads_mode_tile(dcl_id, _T("fencebutstartend"), MODE_DISABLE);
               if (gsc_strcmp(CommonStruct->Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0 || // window
                   gsc_strcmp(CommonStruct->Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0)   // cerchio
                  CommonStruct->Boundary.clear();
               break;
         }
      else // caso griglia
      {
         ads_mode_tile(dcl_id, _T("window"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("circle"), MODE_DISABLE);
         ads_mode_tile(dcl_id, _T("fencebutstartend"), MODE_DISABLE);
         if (gsc_strcmp(CommonStruct->Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0 || // window
             gsc_strcmp(CommonStruct->Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0)   // cerchio
            CommonStruct->Boundary.clear();
      }
   }

   // tipo di selezione
   if (CommonStruct->Boundary.len() == 0 ||
       gsc_strcmp((CommonStruct->Boundary.get_name()), ALL_SPATIAL_COND, FALSE) == 0) // tutto
   {
      CommonStruct->Boundary = FENCE_SPATIAL_COND; // fence
      CommonStruct->CoordList.remove_all();
   }

   // setto i controlli
   if (gsc_setTileDclSpatialSel(dcl_id, CommonStruct) == GS_BAD) return GS_BAD;

   ads_mode_tile(dcl_id, _T("all"), MODE_DISABLE); 

   if (gsc_strcmp((CommonStruct->Boundary.get_name()), POLYGON_SPATIAL_COND, FALSE) == 0 || // polygon
       gsc_strcmp((CommonStruct->Boundary.get_name()), FENCE_SPATIAL_COND, FALSE) == 0 ||   // fence
       gsc_strcmp((CommonStruct->Boundary.get_name()), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend
   {
      ads_mode_tile(dcl_id, _T("Define"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("Select"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("Show"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("accept"), MODE_ENABLE);
   }

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" in modalità "SensorSpatialRules" su controllo "SelType"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SensorSpatialRules_SelType(ads_callback_packet *dcl)
{
   dcl_SpatialSel_SelType(dcl);
   // setto i controlli
   gsc_setTileDclSensorSpatialRules(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" in modalità "SensorSpatialRules" su controllo "Inverse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SensorSpatialRules_Inverse(ads_callback_packet *dcl)
{
   // imposto il nuovo valore
   ((Common_Dcl_SpatialSel_Struct*)(dcl->client_data))->Inverse = (gsc_strcmp(dcl->value, _T("0")) == 0) ? FALSE : TRUE;
   // setto i controlli
   gsc_setTileDclSensorSpatialRules(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" in modalità "SensorSpatialRules" su controllo "Boundary"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SensorSpatialRules_Boundary(ads_callback_packet *dcl)
{
   dcl_SpatialSel_Boundary(dcl);
   // setto i controlli
   gsc_setTileDclSensorSpatialRules(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SpatialSel" in modalità "SensorSpatialRules" su controllo "Load"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SensorSpatialRules_Load(ads_callback_packet *dcl)
{
   dcl_SpatialSel_Load(dcl);
   // setto i controlli
   gsc_setTileDclSensorSpatialRules(dcl->dialog, (Common_Dcl_SpatialSel_Struct*)(dcl->client_data));
}


/*****************************************************************************/
/*.doc gsc_ddSelectSensorSpatialRules                                        */
/*+
  Questa funzione effettua una selezione delle regole di rilevazione spaziale.
  Parametri:
  C_STRING &SpatialSel;   Condizione spaziale secondo il
                          formato usato da ADE (vedi manuale)
                          ("and" "(" "not" "window" "crossing" (x1 y1 z1) (x2 y2 z2))
  C_CLASS *pSensorCls;    Puntatore alla classe sensore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ddSelectSensorSpatialRules(C_STRING &SpatialSel, C_CLASS *pSensorCls)
{
   int       ret = GS_GOOD, status = DLGOK, dcl_file;
   C_STRING  JoinOp, buffer;   
   ads_hdlg  dcl_id;
   Common_Dcl_SpatialSel_Struct CommonStruct;

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &(CommonStruct.Inverse), 
                           CommonStruct.Boundary, CommonStruct.SelType,
                           CommonStruct.CoordList) == GS_BAD)
      return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   buffer = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(buffer, &dcl_file) == RTERROR) return GS_BAD;
    
   while (1)
   {
      ads_new_dialog(_T("SpatialSel"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      ads_action_tile(dcl_id, _T("Boundary"), (CLIENTFUNC) dcl_SensorSpatialRules_Boundary);
      ads_client_data_tile(dcl_id, _T("Boundary"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SelType"),  (CLIENTFUNC) dcl_SensorSpatialRules_SelType);
      ads_client_data_tile(dcl_id, _T("SelType"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Inverse"),  (CLIENTFUNC) dcl_SensorSpatialRules_Inverse);
      ads_client_data_tile(dcl_id, _T("Inverse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Define"),   (CLIENTFUNC) dcl_SpatialSel_Define);
      ads_action_tile(dcl_id, _T("Select"),   (CLIENTFUNC) dcl_SpatialSel_Select);
      ads_action_tile(dcl_id, _T("Show"),     (CLIENTFUNC) dcl_SpatialSel_Show);
      ads_action_tile(dcl_id, _T("Load"),     (CLIENTFUNC) dcl_SensorSpatialRules_Load);
      ads_client_data_tile(dcl_id, _T("Load"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Save"),     (CLIENTFUNC) dcl_SpatialSel_Save);
      ads_client_data_tile(dcl_id, _T("Save"), &CommonStruct);
      ads_action_tile(dcl_id, _T("help"),     (CLIENTFUNC) dcl_SpatialSel_help);

      // setto i controlli
      if (gsc_setTileDclSensorSpatialRules(dcl_id, &CommonStruct, pSensorCls) == GS_BAD)
         { ret = GS_BAD; break; }

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  // compongo la condizione impostata
         if (gsc_fullSpatialSel(JoinOp, CommonStruct.Inverse,
                                CommonStruct.Boundary, CommonStruct.SelType,
                                CommonStruct.CoordList, SpatialSel) == GS_BAD)
             { ret = GS_BAD; break; }
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case WINDOW_DEF:  // definizione di una finestra
            gsc_define_window(CommonStruct.CoordList);
            break; 
         case CIRCLE_SEL:  // selezione di un cerchio esistente
            gsc_select_circle(CommonStruct.CoordList);
            break; 
         case CIRCLE_DEF:  // definizione di un cerchio
            gsc_define_circle(CommonStruct.CoordList);
            break; 
         case POLYGON_SEL: // selezione di un poligono esistente
            gsc_select_polygon(CommonStruct.CoordList);
            break; 
         case POLYGON_DEF: // definizione di un poligono
            gsc_define_polygon(CommonStruct.CoordList);
            break; 
         case FENCE_SEL:   // selezione di una fence esistente
         case FENCEBUTSTARTEND_SEL:
            gsc_select_fence(CommonStruct.CoordList);
            break; 
         case FENCE_DEF:   // definizione di una fence
         case FENCEBUTSTARTEND_DEF:
            gsc_define_fence(CommonStruct.CoordList);
            break; 
         case BUFFERFENCE_SEL:   // selezione di un bufferfence esistente
            gsc_select_bufferfence(CommonStruct.CoordList);
            break; 
         case BUFFERFENCE_DEF:   // definizione di un bufferfence
            gsc_define_bufferfence(CommonStruct.CoordList);
            break; 
         case SHOW:        // mostra la selezione corrente
            gsc_showSpatialSel(CommonStruct.Boundary.get_name(), CommonStruct.CoordList, GS_GOOD);
            break; 
	   }
   }
   ads_unload_dialog(dcl_file);

   return ret;
}

/*****************************************************************************/
/*.doc gsddsensor                                                           */
/*+
  Questo comando effettua le impostazioni tramite interfaccia grafica per la
  ricerca tramite sensori.
-*/  
/*****************************************************************************/
void gsddsensor(void)
{
   int        ret = GS_GOOD, status = DLGOK, dcl_file;
   ads_hdlg   dcl_id;
   C_STRING   path, DestAttrib;
   C_LINK_SET DetectedLS;
   Common_Dcl_main_sensor_Struct CommonStruct;
   C_CLS_PUNT_LIST               ClassList;
   C_RB_LIST                     DetectionSpatialRules;
   bool                          Inverse = false;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) 
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opFilterEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // prima inizializzazione
   CommonStruct.pSensorCls            = NULL;
   CommonStruct.UseSensorSpatialRules = FALSE;
   CommonStruct.UseNearestSpatialSel  = FALSE;   
   CommonStruct.NearestSpatialSel     = 1;
   CommonStruct.UseNearestTopoRules   = FALSE;   
   CommonStruct.NearestTopoSel        = 1;
   CommonStruct.NearestTopoMaxCost    = 9999;
   CommonStruct.Tools                 = GSHighlightFilterSensorTool;
   CommonStruct.pDetectedCls          = NULL;
   CommonStruct.UseSensorSQLRules     = FALSE;
   CommonStruct.DuplicateDetectedObjs = GS_GOOD;

   path = GS_CURRENT_WRK_SESSION->get_dir();
   path += _T('\\');
   path += GS_INI_FILE;
   gsc_LoadSensorConfig(&CommonStruct, path, _T("LAST_SENSOR"));

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GEOsimAppl::CMDLIST.ErrorCmd();

   while (1)
   {
      ads_new_dialog(_T("SensorRulesBuilder"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      gsc_setTileDclSensorRulesBuilder(dcl_id, &CommonStruct);

      ads_action_tile(dcl_id, _T("SensorClassSelect"), (CLIENTFUNC) dcl_SensorRulesBuilder_SensorClassSelect);
      ads_client_data_tile(dcl_id, _T("SensorClassSelect"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SensorObjsSelect"), (CLIENTFUNC) dcl_SensorRulesBuilder_SensorObjsSelect);
      ads_client_data_tile(dcl_id, _T("SensorObjsSelect"), &CommonStruct);

      ads_action_tile(dcl_id, _T("DetectedClassSelect"), (CLIENTFUNC) dcl_SensorRulesBuilder_DetectedClassSelect);
      ads_client_data_tile(dcl_id, _T("DetectedClassSelect"), &CommonStruct);
      ads_action_tile(dcl_id, _T("DuplicateDetectedObjs"), (CLIENTFUNC) dcl_SensorRulesBuilder_DuplicateDetectedObjs);
      ads_client_data_tile(dcl_id, _T("DuplicateDetectedObjs"), &CommonStruct);
      ads_action_tile(dcl_id, _T("DetectedSQL"), (CLIENTFUNC) dcl_SensorRulesBuilder_DetectedSQL);
      ads_client_data_tile(dcl_id, _T("DetectedSQL"), &CommonStruct);

      ads_action_tile(dcl_id, _T("Tools"), (CLIENTFUNC) dcl_SensorRulesBuilder_Tools);
      ads_client_data_tile(dcl_id, _T("Tools"), &CommonStruct);
     
      ads_action_tile(dcl_id, _T("UseSensorSpatialRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_UseSensorSpatialRules);
      ads_client_data_tile(dcl_id, _T("UseSensorSpatialRules"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SetSensorSpatialRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_SetSensorSpatialRules);

      ads_action_tile(dcl_id, _T("UseSensorSQLRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_UseSensorSQLRules);
      ads_client_data_tile(dcl_id, _T("UseSensorSQLRules"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SetSensorSQLRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_SetSensorSQLRules);
      ads_client_data_tile(dcl_id, _T("SetSensorSQLRules"), &CommonStruct);

      ads_action_tile(dcl_id, _T("UseNearestSensorSpatialRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_SpatialNearest_Option);
      ads_client_data_tile(dcl_id, _T("UseNearestSensorSpatialRules"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SpatialNearest_Qty"), (CLIENTFUNC) dcl_SensorRulesBuilder_SpatialNearest_Qty);
      ads_client_data_tile(dcl_id, _T("SpatialNearest_Qty"), &CommonStruct);

      ads_action_tile(dcl_id, _T("UseNearestSensorTopoRules"), (CLIENTFUNC) dcl_SensorRulesBuilder_TopoNearest_Option);
      ads_client_data_tile(dcl_id, _T("UseNearestSensorTopoRules"), &CommonStruct);
      ads_action_tile(dcl_id, _T("TopoNearest_Qty"), (CLIENTFUNC) dcl_SensorRulesBuilder_TopoNearest_Qty);
      ads_client_data_tile(dcl_id, _T("TopoNearest_Qty"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SetNearestSensorTopoRules_File"), (CLIENTFUNC) dcl_SensorRulesBuilder_SetNearestSensorTopoRules_File);
      ads_client_data_tile(dcl_id, _T("SetNearestSensorTopoRules_File"), &CommonStruct);

      ads_action_tile(dcl_id, _T("Save"), (CLIENTFUNC) dcl_SensorRulesBuilder_Save);
      ads_client_data_tile(dcl_id, _T("Save"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Load"), (CLIENTFUNC) dcl_SensorRulesBuilder_Load);
      ads_client_data_tile(dcl_id, _T("Load"), &CommonStruct);

      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SensorRulesBuilder_help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case SENSOR_CLS_SEL:  // selezione di una classe
         {
            C_CLASS *pCls       = CommonStruct.pSensorCls;
            int     PrevLastCls = GEOsimAppl::LAST_CLS;
            int     PrevLastSub = GEOsimAppl::LAST_SUB;

            if (pCls)
            {
               GEOsimAppl::LAST_CLS = pCls->ptr_id()->code;
               GEOsimAppl::LAST_SUB = pCls->ptr_id()->sub_code;
            }
            // seleziona la classe o sottoclasse
            if (gsc_ddselect_class(NONE, &pCls) == GS_GOOD)
               if (CommonStruct.pSensorCls != pCls) 
               {
                  CommonStruct.pSensorCls = pCls;
                  CommonStruct.SensorLS.clear();
                  CommonStruct.UseSensorSpatialRules = FALSE;
                  CommonStruct.SensorAttrib.clear();
                  CommonStruct.DetectionSQLRules.remove_all();
                  CommonStruct.SpatialSel.clear();
               }

            GEOsimAppl::LAST_CLS = PrevLastCls;
            GEOsimAppl::LAST_SUB = PrevLastSub;
            break;
         }
         case DETECTED_CLS_SEL:  // selezione di una classe
         {
            C_CLASS *pCls       = CommonStruct.pDetectedCls;
            int     PrevLastCls = GEOsimAppl::LAST_CLS;
            int     PrevLastSub = GEOsimAppl::LAST_SUB;

            if (pCls)
            {
               GEOsimAppl::LAST_CLS = pCls->ptr_id()->code;
               GEOsimAppl::LAST_SUB = pCls->ptr_id()->sub_code;
            }
            // seleziona la classe o sottoclasse
            if (gsc_ddselect_class(NONE, &pCls) == GS_GOOD)
               if (CommonStruct.pDetectedCls != pCls) 
               {
                  CommonStruct.DetectedAttrib.clear();
                  CommonStruct.DetectionSQLRules.remove_all();
                  CommonStruct.sql_list.remove_all();

                  if (CommonStruct.sql_list.initialize(pCls) == GS_GOOD)
                     CommonStruct.pDetectedCls = pCls;
               }

            GEOsimAppl::LAST_CLS = PrevLastCls;
            GEOsimAppl::LAST_SUB = PrevLastSub;
            break;
         }
         case OBJ_SEL:  // selezione di oggetti
         {
            if (CommonStruct.pSensorCls->get_category() != CAT_GRID)
            {
               gsc_ssget(NULL, NULL, NULL, NULL, *(CommonStruct.SensorLS.ptr_SelSet()));
               acutPrintf(gsc_msg(360)); // "Attendere..."
               CommonStruct.SensorLS.ptr_SelSet()->intersectClsCode(CommonStruct.pSensorCls->ptr_id()->code,
                                                      CommonStruct.pSensorCls->ptr_id()->sub_code,
                                                      GRAPHICAL);
            }
            else // se si tratta di griglia seleziono una zona
            {
               C_RB_LIST CoordList;

               if (gsc_define_window(CoordList) == GS_GOOD)
                  // Restituisce la lista dei codici delle celle che intersecano il rettangolo
                  CommonStruct.pSensorCls->ptr_grid()->getKeyListInWindow(CoordList, CROSSING,
                                                                          *(CommonStruct.SensorLS.ptr_KeyList()));
            }

            break;
         }
         case LOCATION_SEL:  // definizione di area di selezione
         {
            gsc_ddSelectSensorSpatialRules(CommonStruct.SpatialSel, CommonStruct.pSensorCls);
            break;
         }
	   }
   }
   ads_unload_dialog(dcl_file);

   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   else if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();

   // Salva le impostazioni correnti
   path = GS_CURRENT_WRK_SESSION->get_dir();
   path += _T('\\');
   path += GS_INI_FILE;
   gsc_SaveSensorConfig(&CommonStruct, path, _T("LAST_SENSOR"));

   if (CommonStruct.UseSensorSpatialRules && CommonStruct.SpatialSel.len() > 0)
   {
      C_STRING  JoinOp, Boundary, SelType;
      C_RB_LIST CoordList;

      // scompongo la condizione impostata
      if (gsc_SplitSpatialSel(CommonStruct.SpatialSel, JoinOp, &Inverse,
                              Boundary, SelType, CoordList) == GS_BAD)
         return GEOsimAppl::CMDLIST.ErrorCmd();

      if (gsc_strcmp(Boundary.get_name(), CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
      {
         DetectionSpatialRules << acutBuildList(RTSTR, Boundary.get_name(),
                                                RTREAL, CoordList.getptr_at(2)->resval.rreal, // raggio
                                                RTSTR, SelType.get_name(), 0);
      }
      else
      if (gsc_strcmp(Boundary.get_name(), FENCE_SPATIAL_COND, FALSE) == 0 ||           // fence
          gsc_strcmp(Boundary.get_name(), FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0)  // fence
      {
         DetectionSpatialRules << acutBuildList(RTSTR, Boundary.get_name(), 0);
      }
      else
      if (gsc_strcmp(Boundary.get_name(), BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
      {
         DetectionSpatialRules << acutBuildList(RTSTR, Boundary.get_name(),
                                                RTREAL, CoordList.getptr_at(1)->resval.rreal, // distanza
                                                RTSTR, SelType.get_name(), 0);
      }
      else
      if (gsc_strcmp(Boundary.get_name(), POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
      {
         DetectionSpatialRules << acutBuildList(RTSTR, _T("inside"),
                                                RTSTR, SelType.get_name(), 0);
      }
      else
      if (gsc_strcmp(Boundary.get_name(), WINDOW_SPATIAL_COND, FALSE) == 0) // window
      {
         ads_point p1, p2;
         double    dimX, dimY;

         ads_point_set(CoordList.getptr_at(1)->resval.rpoint, p1);
         ads_point_set(CoordList.getptr_at(2)->resval.rpoint, p2);
         dimX = fabs(p1[X] - p2[X]);
         dimY = fabs(p1[Y] - p2[Y]);
         DetectionSpatialRules << acutBuildList(RTSTR, WINDOW_SPATIAL_COND, // window
                                                RTREAL, dimX, RTREAL, dimY,
                                                RTSTR, SelType.get_name(), 0);
      }
   }

   if (CommonStruct.Tools == GSStatisticsFilterSensorTool && CommonStruct.pDetectedCls)
   {
      C_STRING dummy, AggrFun;

      if (CommonStruct.pDetectedCls->ptr_info())
      {
         // Poichè la funzione di aggregazione viene appliacta su una tabella temporanea in ACCESS
         // la connessione a DB deve essere su una tabella temporanea ACCESS
         if (gsc_fullSQLAggrCond(CommonStruct.Aggr_Fun, CommonStruct.DetectedAttrib, dummy, dummy,
                                 TRUE, AggrFun, CommonStruct.pDetectedCls->ptr_info()->getDBConnection(TEMP)) == GS_BAD)
            return GEOsimAppl::CMDLIST.ErrorCmd();
      }
      else
         if (gsc_fullSQLAggrCond(CommonStruct.Aggr_Fun, CommonStruct.DetectedAttrib, dummy, dummy,
                                 TRUE, AggrFun) == GS_BAD)
            return GEOsimAppl::CMDLIST.ErrorCmd();

      CommonStruct.Aggr_Fun = AggrFun;

      if (CommonStruct.SensorAttribTypeCalc.comp(gsc_msg(95)) == 0) // "Sottrai a"
         CommonStruct.SensorAttribTypeCalc = _T("-");
      else if (CommonStruct.SensorAttribTypeCalc.comp(gsc_msg(94)) == 0) // "Somma a"
         CommonStruct.SensorAttribTypeCalc = _T("+");
      else
         CommonStruct.SensorAttribTypeCalc = _T("=");

      DestAttrib = CommonStruct.SensorAttrib.get_name();
   }
   else if (CommonStruct.Tools == GSDetectedEntUpdateSensorTool)
   {
      if (CommonStruct.SensorAttribTypeCalc.comp(gsc_msg(95)) == 0) // "Sottrai a"
         CommonStruct.SensorAttribTypeCalc = _T("-");
      else if (CommonStruct.SensorAttribTypeCalc.comp(gsc_msg(94)) == 0) // "Somma a"
         CommonStruct.SensorAttribTypeCalc = _T("+");
      else
         CommonStruct.SensorAttribTypeCalc = _T("=");

      DestAttrib = CommonStruct.DetectedAttrib.get_name();
   }

   // Se esistono regole di ricerca SQL
   // Devo aggiungere i caratteri delimitatori (sintassi di MAP) ai campi
   if (CommonStruct.DetectionSQLRules.get_head())
   {
      int      i = 0;
      presbuf  pRel, pDetectedExpr;
      C_STRING FieldName, StrValue;

      pRel = CommonStruct.DetectionSQLRules.nth(i++);
      // ciclo sulle relazioni SQL
      while (pRel)
      {
         if (pRel->restype == RTLB)
         {
            // espressione SQL entità da rilevare
            if ((pDetectedExpr = gsc_nth(0, pRel)) == NULL || pDetectedExpr->restype != RTSTR)
               { GS_ERR_COD = eGSInvalidArg; return GEOsimAppl::CMDLIST.ErrorCmd(); }
            // Correggo la sintassi del nome del campo per SQL MAP
            FieldName = pDetectedExpr->resval.rstring;
            gsc_AdjSyntaxMAPFormat(FieldName);
            gsc_RbSubst(pDetectedExpr, FieldName.get_name());
         }

         pRel = CommonStruct.DetectionSQLRules.nth(i++);
      }
   }

   C_TOPOLOGY Topo, *pTopo = NULL;
   if (CommonStruct.pDetectedCls->is_subclass() == GS_GOOD && 
       (CommonStruct.pDetectedCls->get_type() == TYPE_NODE || CommonStruct.pDetectedCls->get_type() == TYPE_NODE) &&
       CommonStruct.UseNearestTopoRules &&
       CommonStruct.NearestTopoSel > 0 && CommonStruct.NearestTopoMaxCost > 0 &&
       gsc_path_exist(CommonStruct.TopoRulesFile) == GS_GOOD)
   {
      C_2STR_INT_LIST CostSQLList;
      C_STRING Sez;

      Sez = _T("PRJ");
      Sez += CommonStruct.pDetectedCls->get_PrjId();
      Sez += _T("CLS");
      Sez += CommonStruct.pDetectedCls->ptr_id()->code;
      CostSQLList.load(CommonStruct.TopoRulesFile, _T(','), Sez.get_name());

      Topo.set_type(TYPE_POLYLINE); // tipologia di tipo rete
      Topo.set_cls(CommonStruct.pDetectedCls);
      if (Topo.LoadInMemory(&CostSQLList) == GS_BAD) return;
      Topo.NetCost = CommonStruct.NearestTopoMaxCost; // massimo costo
      pTopo = &Topo;
   }

   GEOsimAppl::REFUSED_SS.clear();
   if (gsc_sensor(CommonStruct.pSensorCls, CommonStruct.SensorLS, CommonStruct.pDetectedCls,
                  (CommonStruct.UseSensorSpatialRules == 0) ? NULL : DetectionSpatialRules.get_head(),
                  Inverse,
                  (CommonStruct.UseSensorSQLRules == 0) ? NULL : CommonStruct.DetectionSQLRules.get_head(),
                  (CommonStruct.UseNearestSpatialSel == 0) ? 0 : CommonStruct.NearestSpatialSel,
                  (CommonStruct.UseNearestTopoRules == 0) ? NULL : &Topo,
                  (CommonStruct.UseNearestTopoRules == 0) ? 0 : CommonStruct.NearestTopoSel,
                  &(CommonStruct.sql_list), &DetectedLS,
                  CommonStruct.Aggr_Fun.get_name(), CommonStruct.Lisp_Fun.get_name(),
                  CommonStruct.Tools, 
                  DestAttrib.get_name(),
                  CommonStruct.SensorAttribTypeCalc.get_name(),
                  CommonStruct.DuplicateDetectedObjs, &(GEOsimAppl::REFUSED_SS), GS_GOOD) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   
   DetectedLS.copy(GS_LSFILTER);
   gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro

   if (gsc_DelAllGS_VERIFIED() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
  
   switch (CommonStruct.Tools)
   {
      case GSHighlightFilterSensorTool:
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDFILTEREVID\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;

      case GSExportFilterSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDFILTEREXPORT\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSMultipleExportSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (fwprintf(file, _T("GSDDSENSOREXPORT\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSBrowseFilterSensorTool:
      {
         FILE *file;

         // scrivo file script
         path = GEOsimAppl::CURRUSRDIR;
         path += _T('\\');
         path += GEOTEMPDIR;
         path += _T('\\');
         path += GS_SCRIPT;
         if ((file = gsc_fopen(path, _T("w"))) == NULL)
            { gsc_fclose(file); return GEOsimAppl::CMDLIST.ErrorCmd(); }

         if (fwprintf(file, _T("GSFILTERBROWSE\n")) < 0)
            { gsc_fclose(file);  GS_ERR_COD = eGSWriteFile; return GEOsimAppl::CMDLIST.ErrorCmd(); }

         if (gsc_fclose(file) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

         if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, path.get_name(), 0) != RTNORM)
            return GEOsimAppl::CMDLIST.ErrorCmd();
         break;
      }
      case GSNoneFilterSensorTool:
         break;
      default:
         break;
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*****************************************************************************/
/*.doc gsc_SaveSensorConfig                                       <internal> */
/*+
  Questa funzione salva la configurazione di ricerca attraverso sensore.
  riferire.
  Parametri:
  Common_Dcl_main_sensor_Struct *pStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_SaveSensorConfig(Common_Dcl_main_sensor_Struct *pStruct, C_STRING &Path,
                         const TCHAR *Section)
{
   C_STRING                EntryName, EntryValue;
   int                     i, Result = GS_BAD;
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(Path) == GS_GOOD)
      if (gsc_read_profile(Path, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   // Classe sensore
   if (pStruct->pSensorCls)
   {
      EntryName = _T("SensorCls");
      EntryValue = pStruct->pSensorCls->ptr_id()->pPrj->get_key();
      EntryValue += _T(',');
      EntryValue += pStruct->pSensorCls->ptr_id()->code;
      EntryValue += _T(',');
      EntryValue += pStruct->pSensorCls->ptr_id()->sub_code;
      if (ProfileSections.set_entry(Section, EntryName.get_name(), EntryValue.get_name()) != GS_GOOD)
         return GS_BAD;
   }

   // Classe rilevata
   if (pStruct->pDetectedCls)
   {
      EntryName = _T("DetectedCls");
      EntryValue = pStruct->pDetectedCls->ptr_id()->pPrj->get_key();
      EntryValue += _T(',');
      EntryValue += pStruct->pDetectedCls->ptr_id()->code;
      EntryValue += _T(',');
      EntryValue += pStruct->pDetectedCls->ptr_id()->sub_code;
      if (ProfileSections.set_entry(Section, EntryName.get_name(), EntryValue.get_name()) != GS_GOOD)
         return GS_BAD;
   }

   // selezione spaziale
   if (ProfileSections.set_entry(Section, _T("SpatialSel"), pStruct->SpatialSel.get_name()) != GS_GOOD)
      return GS_BAD;

   // Regole SQL di rilevazione
   presbuf p, pItem;
   i = 0;
   while ((p = pStruct->DetectionSQLRules.nth(i++)))
   {
      if (p->restype == RTSTR) // Si tratta di operatore relazionale OR o AND ...
      {
         EntryName = _T("DetectionSQLRules_RelOperator");
         EntryName += i;
         if (ProfileSections.set_entry(Section, EntryName.get_name(), p->resval.rstring) != GS_GOOD)
            return GS_BAD;
      }
      else if (p->restype == RTLB) // Si tratta di condizione
      {
         EntryName = _T("DetectionSQLRules_DetectedAttrib");
         EntryName += i;
         pItem = gsc_nth(0, p);
         if (ProfileSections.set_entry(Section, EntryName.get_name(), pItem->resval.rstring) != GS_GOOD)
            return GS_BAD;

         EntryName = _T("DetectionSQLRules_CompOperator");
         EntryName += i;
         pItem = gsc_nth(1, p);
         if (ProfileSections.set_entry(Section, EntryName.get_name(), pItem->resval.rstring) != GS_GOOD)
            return GS_BAD;

         EntryName = _T("DetectionSQLRules_SensorAttrib");
         EntryName += i;
         pItem = gsc_nth(2, p);
         if (ProfileSections.set_entry(Section, EntryName.get_name(), pItem->resval.rstring) != GS_GOOD)
            return GS_BAD;
      }
   }

   // Funzione di aggregazione SQL (caso Tools=GSStatisticsFilterSensorTool) 
   if (ProfileSections.set_entry(Section, _T("AggrFun"), pStruct->Aggr_Fun.get_name()) != GS_GOOD)
      return GS_BAD;

   // Funzione GEOLisp (caso Tools=GSDetectedEntUpdateSensorTool)
   if (ProfileSections.set_entry(Section, _T("LispFun"), pStruct->Lisp_Fun.get_name()) != GS_GOOD)
      return GS_BAD;

   // Nome attributo delle entità rilevate che ospita una risultato
   // (caso Tools=GSStatisticsFilterSensorTool e Tools=GSDetectedEntUpdateSensorTool)
   if (ProfileSections.set_entry(Section, _T("DetectedAttrib"), pStruct->DetectedAttrib.get_name()) != GS_GOOD)
      return GS_BAD;

   // Nome attributo delle entità sensori da cui leggere valori
   // (caso Tools=GSStatisticsFilterSensorTool e Tools=GSDetectedEntUpdateSensorTool)
   if (ProfileSections.set_entry(Section, _T("SensorAttrib"), pStruct->SensorAttrib.get_name()) != GS_GOOD)
      return GS_BAD;

   // Funzione di calcolo ("somma a", "sostituisci a"...)
   // (caso Tools=GSStatisticsFilterSensorTool e Tools=GSDetectedEntUpdateSensorTool)
   if (ProfileSections.set_entry(Section, _T("SensorAttribTypeCalc"), pStruct->SensorAttribTypeCalc.get_name()) != GS_GOOD)
      return GS_BAD;

   // flag di uso delle regole spaziali
   if (ProfileSections.set_entry(Section, _T("UseSensorSpatialRules"), pStruct->UseSensorSpatialRules) != GS_GOOD)
      return GS_BAD;

   // flag di uso della ricerca dei più vicini al sensore (spazialmente)
   if (ProfileSections.set_entry(Section, _T("UseNearestSpatialSel"), pStruct->UseNearestSpatialSel) != GS_GOOD)
      return GS_BAD;

   // numero delle entità più vicine da considerare (spazialmente)
   if (ProfileSections.set_entry(Section, _T("NearestSpatialSel"), pStruct->NearestSpatialSel) != GS_GOOD)
      return GS_BAD;

   // flag di uso della ricerca dei più vicini al sensore (topologicamente)
   if (ProfileSections.set_entry(Section, _T("UseNearestTopoRules"), pStruct->UseNearestTopoRules) != GS_GOOD)
      return GS_BAD;

   // numero delle entità più vicine da considerare (topologicamente)
   if (ProfileSections.set_entry(Section, _T("NearestTopoSel"), pStruct->NearestTopoSel) != GS_GOOD)
      return GS_BAD;

   // costo massimo della vsita topologia
   if (ProfileSections.set_entry(Section, _T("NearestTopoMaxCost"), pStruct->NearestTopoMaxCost) != GS_GOOD)
      return GS_BAD;

   // File con le regole per la visita topologica
   if (ProfileSections.set_entry(Section, _T("TopoRulesFile"), pStruct->TopoRulesFile.get_name()) != GS_GOOD)
      return GS_BAD;

   // flag di uso delle regole SQL
   if (ProfileSections.set_entry(Section, _T("UseSensorSQLRules"), pStruct->UseSensorSQLRules) != GS_GOOD)
      return GS_BAD;

   // flag di ricerca di elementi già trovati da altri sensori
   if (ProfileSections.set_entry(Section, _T("DuplicateDetectedObjs"), pStruct->DuplicateDetectedObjs) != GS_GOOD)
      return GS_BAD;

   // Tipo di operazione sulle entità rilevate
   if (ProfileSections.set_entry(Section, _T("Tools"), pStruct->Tools) != GS_GOOD)
      return GS_BAD;

   // Lista condizioni SQL per filtrare le netità rilevate
   // pStruct->sql_list
   if (pStruct->sql_list.save(ProfileSections, Section) != GS_GOOD)
      return GS_BAD;

   return gsc_write_profile(Path, ProfileSections, Unicode);
}


/*****************************************************************************/
/*.doc gsc_LoadSensorConfig                                       <internal> */
/*+
  Questa funzione salva la configurazione di ricerca attraverso sensore.
  riferire.
  Parametri:
  Common_Dcl_main_sensor_Struct *pStruct;
  C_STRING &Path;
  const TCHAR *Section;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_LoadSensorConfig(Common_Dcl_main_sensor_Struct *pStruct, C_STRING &Path,
                         const TCHAR *Section)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_2STR_BTREE            *pProfileEntries;
   C_B2STR                 *pProfileEntry;
   C_STRING EntryName, EntryValue;
   int      i, n, prj, cls, sub = 0, Result = GS_BAD;
   TCHAR    *str;

   if (gsc_path_exist(Path) != GS_GOOD) return GS_GOOD;
   
   if (gsc_read_profile(Path, ProfileSections) != GS_GOOD)
      return GS_BAD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(Section)))
      return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // Classe sensore
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("SensorCls"))) == NULL) return GS_BAD;
   str = pProfileEntry->get_name2();
   n   = gsc_strsep(str, _T('\0')) + 1;
   if (n >= 1)
   {
      prj = _wtoi(str); // Leggo codice progetto        
      if (n >= 2)
      {
         while (*str != _T('\0')) str++; str++;           
         cls = _wtoi(str); // Leggo codice classe            
         if (n >= 3)
         {
            while (*str != _T('\0')) str++; str++;           
            sub = _wtoi(str); // Leggo sottocodice classe
         }
      }
   }
   if ((pStruct->pSensorCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // Classe rilevata
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("DetectedCls"))) == NULL) return GS_BAD;
   str = pProfileEntry->get_name2();
   n   = gsc_strsep(str, _T('\0')) + 1;
   if (n >= 1)
   {
      prj = _wtoi(str); // Leggo codice progetto        
      if (n >= 2)
      {
         while (*str != _T('\0')) str++; str++;           
         cls = _wtoi(str); // Leggo codice classe            
         if (n >= 3)
         {
            while (*str != _T('\0')) str++; str++;           
            sub = _wtoi(str); // Leggo sottocodice classe
         }
      }
   }
   if ((pStruct->pDetectedCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // selezione spaziale
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("SpatialSel"))))
      pStruct->SpatialSel = pProfileEntry->get_name2();

   // Regole SQL di rilevazione
   C_STRING DetectedAttrib, CompOperator, SensorAttrib;
   i = 1;
   pStruct->DetectionSQLRules.remove_all();
   do
   {
      EntryName = _T("DetectionSQLRules_DetectedAttrib");
      EntryName += i;
      if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())) == NULL)
         break;
      DetectedAttrib = pProfileEntry->get_name2();

      EntryName = _T("DetectionSQLRules_CompOperator");
      EntryName += i;
      if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())) == NULL)
         break;
      CompOperator = pProfileEntry->get_name2();

      EntryName = _T("DetectionSQLRules_SensorAttrib");
      EntryName += i;
      if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())) == NULL)
         break;
      SensorAttrib = pProfileEntry->get_name2();

      pStruct->DetectionSQLRules += acutBuildList(RTLB,
                                                  RTSTR, DetectedAttrib.get_name(),
                                                  RTSTR, CompOperator.get_name(),
                                                  RTSTR, SensorAttrib.get_name(),
                                                  RTLE, 0);

      i++;
      EntryName = _T("DetectionSQLRules_RelOperator");
      EntryName += i;
      if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(EntryName.get_name())) == NULL)
         break;
      pStruct->DetectionSQLRules += acutBuildList(RTSTR, pProfileEntry->get_name2(), 0);
      i++;
   }
   while (1);
      
   if (pStruct->DetectionSQLRules.GetCount() > 0)
      pStruct->DetectionSQLRules.link_head(acutBuildList(RTLB, 0 ));

   // Funzione di aggregazione SQL (caso Tools=GSStatisticsFilterSensorTool) 
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("AggrFun"))) != NULL)
      pStruct->Aggr_Fun = pProfileEntry->get_name2();

   // Funzione GEOLisp (caso Tools=GSDetectedEntUpdateSensorTool)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("LispFun"))) != NULL)
      pStruct->Lisp_Fun = pProfileEntry->get_name2();

   // Nome attributo delle entità rilevate che ospita una risultato
   // (caso Tools=GSStatisticsFilterSensorTool e Tools=GSDetectedEntUpdateSensorTool)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("DetectedAttrib"))) != NULL)
      pStruct->DetectedAttrib = pProfileEntry->get_name2();

   // Nome attributo delle entità sensori da cui leggere valori
   // (caso Tools=GSStatisticsFilterSensorTool)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("SensorAttrib"))) != NULL)
      pStruct->SensorAttrib = pProfileEntry->get_name2();

   // Funzione di calcolo ("somma a", "sostituisci a", "sottrai a")
   // (caso Tools=GSStatisticsFilterSensorTool e Tools=GSDetectedEntUpdateSensorTool)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("SensorAttribTypeCalc"))) != NULL)
      pStruct->SensorAttribTypeCalc = pProfileEntry->get_name2();

   // flag di uso delle regole spaziali
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("UseSensorSpatialRules"))) != NULL)
      pStruct->UseSensorSpatialRules = _wtoi(pProfileEntry->get_name2());

   // flag di uso della ricerca dei più vicini (spazialmente)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("UseNearestSpatialSel"))) != NULL)
      pStruct->UseNearestSpatialSel = _wtoi(pProfileEntry->get_name2());

   // numero delle entità più vicine da considerare (spazialmente)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("NearestSpatialSel"))) != NULL)
      pStruct->NearestSpatialSel = _wtoi(pProfileEntry->get_name2());

   // flag di uso della ricerca dei più vicini (topologicamente)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("UseNearestTopoRules"))) != NULL)
      pStruct->UseNearestTopoRules = _wtoi(pProfileEntry->get_name2());

   // numero delle entità più vicine da considerare (topologicamente)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("NearestTopoSel"))) != NULL)
      pStruct->NearestTopoSel = _wtoi(pProfileEntry->get_name2());

   // costo massimo della vsita topologia
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("NearestTopoMaxCost"))) != NULL)
      pStruct->NearestTopoMaxCost = _wtof(pProfileEntry->get_name2());

   // File con le regole per la visita topologica
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("TopoRulesFile"))) != NULL)
      pStruct->TopoRulesFile = pProfileEntry->get_name2();

   // flag di uso delle regole SQL
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("UseSensorSQLRules"))) != NULL)
      pStruct->UseSensorSQLRules = _wtoi(pProfileEntry->get_name2());

   // flag di ricerca di elementi già trovati da altri sensori
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("DuplicateDetectedObjs"))) != NULL)
      pStruct->DuplicateDetectedObjs = _wtoi(pProfileEntry->get_name2());

   // Tipo di operazione sulle entità rilevate
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Tools"))) != NULL)
      pStruct->Tools = _wtoi(pProfileEntry->get_name2());

   // Lista condizioni SQL per filtrare le entità rilevate
   // pStruct->sql_list
   pStruct->sql_list.load(ProfileSections, Section);

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DEI SENSORI 
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_get_LS_for_filter                                                 */
/*+
  Funzione di ausilio per impostare un LINK_SET da usare nel comando di filtro.
  Parametri:
  C_CLASS *pCls;           Puntatore alla classe
  bool LocationUse;        Se vero bisogna considerare i criteri spaziali
  C_STRING &SpatialSel;    Criteri spaziali
  bool LastFilterResultUse; Se vero bisogna considerare l'ultimo risultato del filtro
  int Logical_op;          Operatore logico tra "selezione spaziale" e "risultato ultimo filtro"
  C_LINK_SET &ResultLS;    LINK_SET risultato

  Ritorna GS_GOOD in caso di impostazione del LINK_SET, GS_BAD in caso di errore e
  GS_CAN se il LINK_SET non viene restituito perchè devono essere considerate
  tutte le entità della classe che sono state estratte.
-*/  
/*****************************************************************************/
int gsc_get_LS_for_filter(C_CLASS *pCls, bool LocationUse, C_STRING &SpatialSel,
                          bool LastFilterResultUse, int Logical_op, C_LINK_SET &ResultLS)
{
   C_LINK_SET SpatialLS, PrevFilterLS;

   // Se si è fatta una selezione spaziale
   if (LocationUse && SpatialSel.len() > 0)
   {
      // Se classe NON è griglia
      if (pCls->get_category() != CAT_GRID)
      {
         ads_name  ss;

         if (LastFilterResultUse) // se si devono fare operazioni con LastFilterResult
         {
            // cerco gli oggetti nella sessione specificata
            if (gsc_selObjsArea(SpatialSel, ss,
                                pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                GRAPHICAL) == GS_BAD)
               return GS_BAD;

         }
         else // cerco gli oggetti solo se non si tratta di ALL
         {
            C_STRING  Boundary, SelType, JoinOp;
            C_RB_LIST CoordList;
            bool      Inverse;

            // scompongo la condizione impostata
            if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &Inverse,
                                    Boundary, SelType, CoordList) == GS_BAD)
               return GS_BAD;

            // case insensitive
            if (gsc_strcmp(Boundary.get_name(), ALL_SPATIAL_COND, FALSE) == 0) // tutto
               return GS_CAN;

            // cerco gli oggetti nella sessione specificata
            if (gsc_selObjsArea(SpatialSel, ss,
                                pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                GRAPHICAL) == GS_BAD)
               return GS_BAD;
         }

         *(SpatialLS.ptr_SelSet()) << ss;
      }
      else // se si tratta di classe griglia
      {
         if (LastFilterResultUse) // se si devono fare operazioni con LastFilterResult
         {
            // cerco le celle nella sessione specificata
            if (gsc_selGridKeyListArea(SpatialSel, *(SpatialLS.ptr_KeyList()),
                                       pCls->ptr_id()->code, pCls->ptr_id()->sub_code) == GS_BAD)
               return GS_BAD;
         }
         else // cerco le celle solo se non si tratta di ALL
         {
            C_STRING  Boundary, SelType, JoinOp;
            C_RB_LIST CoordList;
            bool      Inverse;

            // scompongo la condizione impostata
            if (gsc_SplitSpatialSel(SpatialSel, JoinOp, &Inverse,
                                    Boundary, SelType, CoordList) == GS_BAD)
               return GS_BAD;

            // case insensitive
            if (gsc_strcmp(Boundary.get_name(), ALL_SPATIAL_COND, FALSE) == 0) // tutto
               return GS_CAN;

            // cerco le celle nella sessione specificata
            if (gsc_selGridKeyListArea(SpatialSel, *(SpatialLS.ptr_KeyList()),
                                       pCls->ptr_id()->code, pCls->ptr_id()->sub_code) == GS_BAD)
               return GS_BAD;
         }
      }
   }

   if (LastFilterResultUse)
      GS_LSFILTER.copy(PrevFilterLS);

   if (LastFilterResultUse && LocationUse)
      switch (Logical_op)
      {
         case UNION: // unione tra SpatialLS e PrevFilterLS
            ResultLS.ptr_SelSet()->add_selset(*(SpatialLS.ptr_SelSet()));
            ResultLS.ptr_KeyList()->add_list(*(SpatialLS.ptr_KeyList()));

            ResultLS.ptr_SelSet()->add_selset(*(PrevFilterLS.ptr_SelSet()));
            // Se la classe filtrata e quella precedentemente filtrata coincidono
            if (PrevFilterLS.cls == pCls->ptr_id()->code && 
                PrevFilterLS.sub == pCls->ptr_id()->sub_code)
               ResultLS.ptr_KeyList()->add_list(*(PrevFilterLS.ptr_KeyList()));
            break;
         case SUBTRACT_A_B: // sottrazione di PrevFilterLS da SpatialLS 
            ResultLS.ptr_SelSet()->add_selset(*(SpatialLS.ptr_SelSet()));
            ResultLS.ptr_KeyList()->add_list(*(SpatialLS.ptr_KeyList()));

            ResultLS.ptr_SelSet()->subtract(*(PrevFilterLS.ptr_SelSet()));
            // Se la classe filtrata e quella precedentemente filtrata coincidono
            if (PrevFilterLS.cls == pCls->ptr_id()->code && 
                PrevFilterLS.sub == pCls->ptr_id()->sub_code)
               ResultLS.ptr_KeyList()->subtract(*(PrevFilterLS.ptr_KeyList()));
            break;
         case SUBTRACT_B_A: // sottrazione di SpatialLS da PrevFilterLS
            ResultLS.ptr_SelSet()->add_selset(*(PrevFilterLS.ptr_SelSet()));
            // Se la classe filtrata e quella precedentemente filtrata coincidono
            if (PrevFilterLS.cls == pCls->ptr_id()->code && 
                PrevFilterLS.sub == pCls->ptr_id()->sub_code)
               ResultLS.ptr_KeyList()->add_list(*(PrevFilterLS.ptr_KeyList()));

            ResultLS.ptr_SelSet()->subtract(*(SpatialLS.ptr_SelSet()));
            ResultLS.ptr_KeyList()->subtract(*(SpatialLS.ptr_KeyList()));
            break;
         case INTERSECT: // intersezione tra SpatialLS e PrevFilterLS
            ResultLS.ptr_SelSet()->add_selset(*(SpatialLS.ptr_SelSet()));
            ResultLS.ptr_KeyList()->add_list(*(SpatialLS.ptr_KeyList()));

            ResultLS.ptr_SelSet()->intersect(*(PrevFilterLS.ptr_SelSet()));
            // Se la classe filtrata e quella precedentemente filtrata coincidono
            if (PrevFilterLS.cls == pCls->ptr_id()->code && 
                PrevFilterLS.sub == pCls->ptr_id()->sub_code)
               ResultLS.ptr_KeyList()->intersect(*(PrevFilterLS.ptr_KeyList()));

            break;
      }
   else
   if (LastFilterResultUse) // è stato scelta solo l'opzione "LastFilterResult"
      PrevFilterLS.copy(ResultLS);
   else
   if (LocationUse) // è stato scelta solo l'opzione "Location"
      SpatialLS.copy(ResultLS);
   else  // non è stata scelta alcuna origine per gli oggetti da filtrare
   {
      acutPrintf(gsc_msg(259)); // "\n*Errore* Nessuna entità selezionata."
      return GS_BAD;
   }

   return GS_GOOD;
}
