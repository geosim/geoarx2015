/*************************************************************************/
/*   GS_WHIP.H                                                           */
/*************************************************************************/

#ifndef _gs_whip_h
#define _gs_whip_h 1

int gs_whip(void);
int gs_extract_4_whip(void);
DllExport extern int gsc_extract_4_whip(int prjcode, C_RB_LIST *p_estr_cond,
                                        C_INT_LIST *cls_2_extr, TCHAR *work_area,
                                        GSDataPermissionTypeEnum mode);

// FUNZIONI PER POSTGIS
DllExport bool gsc_is_class_with_pg_data(C_CLASS *pCls);
int gs_pgview(void);
DllExport int gsc_pgview(int prj, C_INT_LIST &ClsCodeList, bool check_geometry_columns = true);
int gsc_pgview(C_CLASS *pCls);
DllExport int gsc_del_pgview(int prj, C_INT_LIST &ClsCodeList, bool check_geometry_columns = true);
int gsc_del_pgview(C_CLASS *pCls);
int gsc_LispParser(const TCHAR *Lisp, C_STR_STRLIST **pParse);
int gsc_LispParserToPLPGSQL(C_STR_STRLIST *pParse, C_STRING &PLPGSQL, C_CLASS *pCls = NULL);

// FUNZIONI PER EXPORT IN SHAPE
int gs_shapeExport(void);
int gsc_shapeExport(C_STRING &dir, C_INT_LIST &ClsCodeList, bool AllCharDataType);

// FUNZIONI PER GOOGLE-EARTH
int gs_setGoogleEarthODRecord(void);

// FUNZIONI PER QUANTUM GIS
DllExport int gsc_WriteQGISProjectFile(C_STRING &QGISPathFile, int prj, 
                                       C_STRING &Host, C_STRING &Port,
                                       C_STRING &Database, C_STRING &User, C_STRING &Password,
                                       int SRID, C_INT_LIST &ClsCodeList,
                                       C_2STR_LIST *pGroupMaskList = NULL,
                                       C_2LONG_INT_LIST *pGeomScaleFactorList = NULL,
                                       C_2LONG *pLblScaleFactors = NULL);

// FUNZIONI PER HISTORY
int gs_CreateHistorySystem(void);
DllExport int gsc_CreateHistorySystem(int prj, bool CreateClass, C_INT_LIST &ClsCodeList);
int gsc_CreateHistorySystem4Cls(C_CLASS *pCls, 
                                C_STRING &HistoryGeomTableRef, C_STRING &HistoryAlfanumTableRef,
                                bool CreateClass = true);
int gs_DisableHistorySystem(void);
DllExport int gsc_DisableHistorySystem(int prj, C_INT_LIST &ClsCodeList);
int gsc_DisableHistorySystem4Cls(C_CLASS *pCls);
int gs_EnableHistorySystem(void);
DllExport int gsc_EnableHistorySystem(int prj, C_INT_LIST &ClsCodeList);
int gsc_EnableHistorySystem4Cls(C_CLASS *pCls);

#endif