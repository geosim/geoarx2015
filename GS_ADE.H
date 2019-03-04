/*************************************************************************/
/*   GS_ADE.H                                                            */
/*************************************************************************/


#ifndef _gs_ade_h
#define _gs_ade_h 1

#ifndef _gs_selst_h
   #include "gs_selst.h"
#endif

#ifndef _ADEADS_H
   #include <adeads.h>
#endif

#ifndef _gs_class_h
   #include "gs_class.h"
#endif

#ifndef _gs_dwg_h
   #include "gs_dwg.h"
#endif

#ifndef _gs_graph_h
   #include "gs_graph.h"
#endif


//-----------------------------------------------------------------------//
//  CLASSE GESTIONE AMBIENTE MAP                                         //
//-----------------------------------------------------------------------//   


class C_MAP_ENV
{
   public :

   // dimension
   int        DimAssoc;

   // work session preferences
   presbuf    RestoreLastActiveDwgsOnStartup;
   presbuf    ActivateDwgsOnAttach;
   presbuf    DontAddObjectsToSaveSet;
   presbuf    MarkObjectsForEditingWithoutPrompting;
   presbuf    LogFileActive;
   presbuf    LogFileName;
   presbuf    LogMessageLevel;

   // query preferences
   presbuf    QueryFileDirectory;
   presbuf    CaseSensitiveMatch;
   presbuf    SaveCurrQueryInSession;
   presbuf    MkSelSetWithQryObj;
   presbuf    DefaultJoinOperator;
   presbuf    ColorForAdd;
   presbuf    ColorForRemove;
   presbuf    BlockLocnForQuery;
   presbuf    TextLocnForQuery;
   presbuf    ShowBlockAsInsPt;

   // save back preferences
   presbuf    RedefineBlockDefinitions;
   presbuf    RedefineLayerDefinitions;
   presbuf    RedefineTextStyleDefinitions;
   presbuf    RemoveUnusedGroups;
   presbuf    EraseSavedBackObjects;
   presbuf    RemoveLockAfterSave;
   presbuf    CreateHistoryFileOfChanges;
   presbuf    CreateBackupFileOfSourceDwg;

   // external database preferences
   presbuf    NoOfSQLConditionsInHistory;

   // Coordinate transformation preferences
   presbuf    AdjustSizesAndScalesForChangesInUnits;
   presbuf    AdjustRotationsForMapDistortions;
   presbuf    AdjustSizesAndScalesForMapDistortions;
   presbuf    AdjustElevations;
   presbuf    AdjustZeroRotationObjects;

   // System preferences
   presbuf    ForceUserLogin;
   presbuf    EnableObjectLocking;
   presbuf    NumberofOpenDwgs;

   DllExport C_MAP_ENV();
   DllExport ~C_MAP_ENV();
   
   void ReleaseCurrentAdeEnvironment(void);
   void FreezeCurrentAdeEnvironment(void);
   void RestoreLastAdeEnvironment(void);
   DllExport int SetAdeEnvVariable(TCHAR *AdeVar, int AdeVarValue);
   DllExport int SetAdeEnvVariable(TCHAR *AdeVar, TCHAR *AdeVarValue);
   DllExport int SetAdeEnvVariable(TCHAR *AdeVar, EAsiBoolean AdeVarValue);
   void SetEnv4GEOsim(void);
   void SetEnv4GEOsimSave(void);
};


int gs_ade_qlqrygetid(void);
DllExport ade_id gsc_ade_qlqrygetid(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);
int gs_ade_qlloadqry(void);
DllExport int gsc_ade_qlloadqry(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);
DllExport int gsc_ExistCurrentAdeQry(void);
int gs_ade_qrylist(void);
DllExport presbuf gsc_ade_qrylist(void);
int gs_ade_qldelquery(void);
DllExport int gsc_ade_qldelquery(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);
DllExport int gsc_ade_qldelquery(ade_id qryId);
presbuf gsc_ade_qrygetdwgandhandle(ads_name ename);

int gsc_ade_userset(const TCHAR *userName, const TCHAR *pswd);

int gsc_ClsODQryDefine(int cls, int sub, C_STR_LIST &KeyList, const TCHAR *JoinOp = _T("and"));

int gs_ClsSQLQryDel(void);
int gsc_ClsSQLQryDel(int cls, int sub, C_STRING *File = NULL);
int gs_ClsSQLQrySave(void);
int gsc_ClsSQLQrySave(int cls, int sub, C_STRING &SQL, C_STRING *File = NULL);
int gsc_ClsSQLQryLoad(int cls, int sub, C_STRING &SQL, C_STRING *File = NULL);

int gsc_ClsAlterPropQryDel(int cls, int sub, C_STRING *File = NULL);
int gsc_ClsAlterPropQrySave(int cls, int sub, C_FAS &FAS, long flag_set, C_STRING *File = NULL);
int gsc_ClsAlterPropQryLoad(int cls, int sub, C_FAS &FAS, long *flag_set, C_STRING *File = NULL);

int gsc_altpdefine(C_FAS &FAS, long flag_set);
int gsc_getGraphSettingsFromCurrentADEQry(C_FAS &FAS, long *flag_set);

ads_real gsc_editlockobjs(C_SELSET &SelSet);
ads_real gsc_editlockobjs(ads_name selSet);
ads_real gsc_editlockobjs(AcDbObjectIdArray &ObjIds);
int gsc_adeSelObjs(C_SELSET &elSet);

int gsc_getDwgIdList(C_SELSET &SelSet, C_REAL_LIST &DwgIdList);
int gsc_getDwgsShouldBeRequeried(C_SELSET &SelSet, C_REAL_LIST &DwgIdList);


ade_id gsc_getAdeId(AcMapAttachedDrawing *pMapAttachedDwg);
DllExport int gsc_dwgactivate(ade_id dwgId, int retest = MORETESTS);
DllExport ade_id gsc_dsattach(TCHAR *dwgName, int retest = MORETESTS);
DllExport int gsc_dsdetach(ade_id dwgId, int retest = MORETESTS);
DllExport int gsc_AllDwgDetach(int prj = 0, int cls = 0);
DllExport int gsc_AllDwgDeactivate(int ExceptionPrj, int ExceptionCls);
DllExport int gsc_AllDwgDeactivate(int ExceptionPrj = 0, C_INT_LIST *ExceptionClsList = NULL);
int gsc_AllDwgActivate(void);
int gsc_ade_qrygettype(C_STRING *qryType = NULL, ade_boolean *multiLine = NULL,
                       C_STRING *templ = NULL, C_STRING *fileName = NULL);
int gsc_getADECurrRptFile(C_STRING &ReportFile);
int gsc_setADECurrRptFile(C_STRING &ReportFile);

void gsAttachExtractDetach(void);
int gsc_attachExtractDetach(C_STRING &dwg, int retest = MORETESTS);

DllExport int gsc_load_spatial_qry(const TCHAR *SrcQryFile, AcMapQuery &Query);
DllExport int gsc_save_qry(TCHAR *cat = _T("estrazione"), TCHAR *name = ADE_SPATIAL_QRY_NAME,
                           bool OnlyCoordinates = FALSE, const TCHAR *SrcQryFile = NULL);
int gsc_find_qry(C_CLASS *pCls, presbuf cat_list, TCHAR *cat_qry, TCHAR *name_query, int mod);

int gsc_IsClassWithFASOrSQL(int cod_prj, int cod_cls, int cod_sub, int *type);

//int gsc_EditLockObjs(AcDbObjectIdArray &ObjIds);
DllExport int gsc_saveobjs(void);
int gsc_savetodwg(C_SELSET &SelSet, AcMapAttachedDrawing *pMapAttachedDwg, int retest = MORETESTS);
int gsc_savetodwg(ads_name selSet, AcMapAttachedDrawing *pMapAttachedDwg, int retest = MORETESTS);

int gsc_GetAreaFromReport(const TCHAR *Path, ads_point corner1, ads_point corner2);

int gsc_getCurrQryExtents(C_RECT_LIST &RectList);
int gsc_getCurrQryExtents(double *Xmin, double *Ymin, double *Xmax, double *Ymax);
int gsc_getQryCondExtents(C_RB_LIST &QryCond, double *Xmin, double *Ymin, double *Xmax, double *Ymax);

DllExport void gsc_printAdeErr(void);
DllExport presbuf gsc_get_ADEdiag(void);

#endif
