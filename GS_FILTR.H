/*************************************************************************/
/*   GS_FILTR.H                                                         */
/*************************************************************************/


#ifndef _gs_filtr_h
#define _gs_filtr_h 1

#ifndef _gs_list_h
   #include "gs_list.h" 
#endif


//----------------------------------------------------------------------------//
// Classe C_SEC_SQL per la memorizzazione di condizioni SQL per una tabella secondaria
// La classe contiene:
// 1) Codice tabella secondaria
// 2) Raggruppamento (AVG, COUNT, MAX, MIN, SUM)
// 3) Condizione SQl (es. CAMPO1 > 10)
//----------------------------------------------------------------------------//
class C_SEC_SQL : public C_2STR_INT
{
   friend class C_2STR_INT_LIST;
   
   public :
      C_SEC_SQL();
      virtual ~C_SEC_SQL();

      int  get_sec();
      int  set_sec(int sec);
      TCHAR *get_grp();
      int  set_grp(const TCHAR *grp);
      TCHAR *get_sql();
      int  set_sql(const TCHAR *sql);

      int load(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      int save(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

      int prepare_for_filter(C_CLASS *pCls);
};
//----------------------------------------------------------------------------//
// Classe C_SEC_SQL_LIST per la memorizzazione della lista di condizioni SQL
// per le tabelle secondarie di una stessa classe
//----------------------------------------------------------------------------//
class C_SEC_SQL_LIST : public C_2STR_INT_LIST
{
   friend class C_CLASS_SQL;
   friend class C_CLASS_SQL_LIST;

   public :
      DllExport   C_SEC_SQL_LIST();
      DllExport   virtual ~C_SEC_SQL_LIST();
      int copy(C_SEC_SQL_LIST &out);
      int from_rb(resbuf *list);

      int initialize(int cls, int sub);

      int save(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      int load(int cls, int sub, C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
};


//----------------------------------------------------------------------------//
// Classe per la memorizzazione di condizioni SQL per una classe GEOsim
// La classe contiene:
// 1) Codice classe
// 2) Codice sotto-classe
// 3) Condizione SQL (es. CAMPO1 > 10)
// 4) Puntatore alla lista di eventuali condizioni di tabelle secondarie
//----------------------------------------------------------------------------//
class C_CLASS_SQL : public C_INT_INT_STR
{
   friend class C_CLASS_SQL_LIST;

   private:
      C_SEC_SQL_LIST sec_sql_list;
   
   public :
      C_CLASS_SQL();
      virtual ~C_CLASS_SQL();

      int   get_cls();
      int   set_cls(int cls);
      int   get_sub();
      int   set_sub(int sub); 
      TCHAR *get_sql();
      int   set_sql(const TCHAR *sql);
      C_SEC_SQL_LIST *ptr_sec_sql_list();

      int save(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
};
//----------------------------------------------------------------------------//
// Classe C_CLASS_SQL_LIST per la memorizzazione della lista di condizioni SQL
// per le classi GEOsim
//----------------------------------------------------------------------------//
class C_CLASS_SQL_LIST : public C_INT_INT_STR_LIST
{
   public :
      C_CLASS_SQL_LIST();
      virtual ~C_CLASS_SQL_LIST();
      int initialize(C_CLASS *pMotherCls, bool FromFile = FALSE);
      int from_rb(resbuf *list);
      int copy(C_CLASS_SQL_LIST &out);
      int save(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      bool hasCondition(void);
};


void gsddfilter(void);
void gsddthemafilter(void);
int gs_themafilterfromfile(void);
int gs_filter_sql(void);

int gsc_ddSelectArea(C_STRING &SpatialSel);
DllExport int gsc_ddBuildQry(const TCHAR *title, C_DBCONNECTION *pConn, C_STRING &TableRef,
                             C_STRING &SQLCond, int prj = 0, int cls = 0, 
                             int sub = 0, int sec = 0, int UseSQLMAPSyntax = GS_GOOD);
int gsc_ddBuildAggrQry(const TCHAR *title, C_DBCONNECTION *pConn, C_STRING &TableRef,
                       C_STRING &SQLAggrCond, int prj = 0, int cls = 0, 
                       int sub = 0, int sec = 0);

DllExport int gsc_SpatialSel_Load(FILE *file, C_STR_LIST &SpatialSelList);
          int gsc_SpatialSel_Save(FILE *file, C_STRING &pSpatialSel);
          int gsc_SpatialSel_Save(FILE *file, C_STR *pSpatialSel);
DllExport int gsc_SpatialSel_Save(FILE *file, C_STR_LIST &SpatialSelList);
DllExport int gsc_SplitSpatialSel(C_STR *pSpatialSel, C_STRING &JoinOp,
                                  bool *Inverse, C_STRING &Boundary, 
                                  C_STRING &SelMode, C_RB_LIST &CoordList);
DllExport int gsc_SplitSpatialSel(C_STRING &SpatialSel, C_STRING &JoinOp,
                                  bool *Inverse, C_STRING &Boundary, 
                                  C_STRING &SelMode, C_RB_LIST &CoordList);
int gsc_fullSpatialSel(C_STRING &JoinOp, bool Inverse, C_STRING &Boundary,
                       C_STRING &SelType, C_RB_LIST &CoordList, C_STRING &SpatialSel);
int gsc_trad_SelType(C_STRING &SelType, C_STRING &Trad);
int gsc_trad_Boundary(C_STRING &Boundary, C_STRING &Trad);

int gsc_selObjsArea(C_STRING &SpatialSel, ads_name ss, 
                    int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);
int gsc_showSpatialSel(const TCHAR *Boundary, C_RB_LIST &CoordList,
                       int WaitForUser = GS_BAD);

int gsc_selGridKeyListArea(C_STRING &SpatialSel, C_LONG_BTREE &KeyList, 
                          int Cls, int Sub);

DllExport int gsc_define_window(C_RB_LIST &CoordList);
int gsc_define_window(C_RECT &Rectangle);
DllExport int gsc_selObjsWindow(C_RB_LIST &CoordList, int type, C_SELSET &ss, int ExactMode = GS_BAD,
                                int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                                int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);

int gsc_select_polygon(C_RB_LIST &CoordList);
int gsc_define_polygon(C_RB_LIST &CoordList);
int gsc_selObjsPolygon(C_RB_LIST &CoordList, int type, C_SELSET &ss, int ExactMode = GS_BAD,
                      int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                      int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);

int gsc_select_circle(C_RB_LIST &CoordList);
int gsc_define_circle(C_RB_LIST &CoordList);
int gsc_selObjsCircle(C_RB_LIST &CoordList, int type, C_SELSET &ss,
                      int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                      int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);

int gsc_select_fence(C_RB_LIST &CoordList);
int gsc_define_fence(C_RB_LIST &CoordList);
int gsc_selObjsFence(C_RB_LIST &CoordList, C_SELSET &ss,
                     int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                     int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL, 
                     bool IncludeFirstLast = true);
int gsc_selClsObjsFenceSS(C_SELSET &EntSS, C_CLASS *pDetectedCls, C_LINK_SET &DetectedLS,
                          bool IncludeFirstLast = true, int What = GRAPHICAL);

int gsc_select_bufferfence(C_RB_LIST &CoordList);
int gsc_define_bufferfence(C_RB_LIST &CoordList);
int gsc_selObjsBufferFence(C_RB_LIST &CoordList, int type, C_SELSET &ss,
                           int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                           int Cls = 0, int Sub = 0, int ObjType = GRAPHICAL);
int gsc_selClsObjsBufferFenceSS(C_SELSET &EntSS, double OffSet, int Mode,
                                C_CLASS *pDetectedCls, C_LINK_SET &DetectedLS,
                                int What = GRAPHICAL);

int gsc_redraw(ads_name ss,int mode);
DllExport int gsc_grdraw(resbuf *points,int color,int mode, bool Closed = false);
DllExport int gsc_circle_point(ads_point center,ads_real raggio,int num_vertex,
                               resbuf **out);

int gs_do_sql_filter(void);
int gsc_do_sql_filter(int cls, int sub, C_CLASS_SQL_LIST &SQLCondList,
							 C_LINK_SET *pLinkSet, int CounterToVideo = GS_GOOD);
int gsc_GridFilter(C_CGRID *pCls, ads_point pt1, ads_point pt2,
                   C_CLASS_SQL_LIST &SQLCondList, C_LINK_SET &ResultLS,
                   int CounterToVideo = GS_GOOD);

int gsc_fullSQLAggrCond(C_STRING &AggrFun, C_STRING &AttribName, C_STRING &Operator,
                        C_STRING &Value, int SQLType, C_STRING &SQLAggrCond,
                        C_DBCONNECTION *pConn = NULL);

int gsc_do_SecValueTransfer(int cls, int sub, C_CLASS_SQL_LIST &SQLCondList,
                            C_LINK_SET *pLinkSet, C_STRING &Dest);

int gsc_get_LS_for_filter(C_CLASS *pCls, bool LocationUse, C_STRING &SpatialSel,
                          bool LastFilterResultUse, int Logical_op, C_LINK_SET &ResultLS);

extern void gsddfilterevid(void);

int gs_filterPrepareStatistics(void);
int gsc_filterPrepareStatistics(C_STRING &TempTabName, C_LINK_SET &LS, int CounterToVideo = GS_BAD);
int gs_filterTerminateStatistics(void);
int gsc_filterTerminateStatistics(const TCHAR *TempTabName);
int gs_filterGetStatistics(void);
presbuf gsc_filterGetStatistics(const TCHAR *TempTabName, const TCHAR *AttribName,
                                int CounterToVideo = GS_BAD);

int gs_filterExport2File(void);
int gsc_filterExport2File(C_CLASS *pCls, C_LONG_BTREE &KeyList,
                          const TCHAR *out_path_file, const TCHAR *FileMode,
                          int mode = DELIMITED, TCHAR *delimiter = _T(","),
                          int CounterToVideo = GS_BAD);
int gs_filterExport2Db(void);
int gsc_filterExport2Db(C_CLASS *pCls, C_LONG_BTREE &KeyList,
                        C_DBCONNECTION *pOutConn, const TCHAR *OutTableRef,
                        const TCHAR *TabMode, int CounterToVideo = GS_BAD);

int gs_sensorExport2Db(void);
int gs_sensorExport2File(void);

int gsc_sensor(C_CLASS *pSensorCls, C_LINK_SET &SensorLS, C_CLASS *pDetectedCls,
               presbuf DetectionSpatialRules = NULL, bool Inverse = false,
               presbuf DetectionSQLRules = NULL, int NearestSpatialSel = 0,
               C_TOPOLOGY *pTopo = NULL, int ClosestTopo = 0,
               C_CLASS_SQL_LIST *pDetectedSQLCondList = NULL, C_LINK_SET *pDetectedLS = NULL,
               const TCHAR *pAggrFun = NULL, const TCHAR *pLispFun = NULL,
               int Tools = NONE, const TCHAR *pDestAttrib = NULL,
               const TCHAR *pDestAttribTypeCalc = NULL, int DuplicateObjs = GS_BAD,
               C_SELSET *pRefused = NULL, int CounterToVideo = GS_BAD);

void gsddsensor(void);

// segmentazione dinamica
int gsc_do_SecDynamicSegmentationAnalyze(C_SECONDARY *pSec, C_STRING &SQLCond, C_LINK_SET *pLinkSet,
                                         C_FAS &FAS, long flag_set,
                                         C_STRING &LblFun, C_FAS &LblFAS, long  Lbl_flag_set,
                                         bool AddODValues, C_SELSET &outSS);

int gs_set_ssfilter(void);
int gsc_PutSym_ssfilter(void);
int gs_get_ssfilter(void);
int gs_get_KeyListFilter(void);
int gs_get_clsfilter(void);
int gsc_save_LastFilterResult(const TCHAR *name = NULL);
int gsc_load_LastFilterResult(const TCHAR *name = NULL);

extern C_LINK_SET GS_LSFILTER; // LinkSet ad uso esclusivo del filtro

DllExport C_LINK_SET *get_GS_LSFILTER(void);
DllExport void set_GS_LSFILTER(int *cls = NULL, int *sub = NULL,
                               C_SELSET *pSS = NULL, C_LONG_BTREE *pKeyList = NULL);

#endif
