/**********************************************************/
/*   GS_DBREF.H                                           */
/**********************************************************/


#ifndef _gs_dbref_h
#define _gs_dbref_h 1

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif

#ifndef _gs_sql_h
   #include "gs_sql.h" 
#endif

#include "MapODTable.h"
#include "MapODIterator.h"
#include "MapValue.h"

// Object Data Table
DllExport void gsc_getODTableName(int prj, int cls, int sub, C_STRING &ODTableName);
AcMapODTable *gsc_getODTable(int prj, int cls, int sub);
AcMapODTable *gsc_getODTable(C_STRING &ODTableName);
int gsc_setODTable(C_STRING &ODTableName);
DllExport int gsc_setODTable(int prj, int cls, int sub, C_STRING *pODTableName = NULL);
DllExport int gsc_GetObjectODRecordIterator(AcMapODRecordIterator *&pIter);
int gsc_setID2ODTable(ads_name ent, C_STRING &ODTableName);
int gsc_setID2ODTable(ads_name ent, C_STRING &ODTableName, long ID);
int gsc_setID2ODTable(const TCHAR *handle, C_STRING &ODTableName, long ID);
int gsc_setID2ODTable(AcDbObject *pObj, AcMapODTable *pTable, C_STRING &ID,
                      AcMapODRecordIterator *pIter);
int gsc_delID2ODTable(AcDbObjectId &eId, C_STRING &ODTableName,
                      AcMapODRecordIterator *pIter = NULL);
int gsc_delID2ODTable(ads_name ent, C_STRING &ODTableName,
                      AcMapODRecordIterator *pIter = NULL);
DllExport int gsc_getIDfromODTable(ads_name ent, C_STRING &ODTableName, C_STRING &ID,
                                   AcMapODRecordIterator *pIter = NULL);
int gsc_getIDfromODTable(ads_name ent, C_STRING &ODTableName, long *ID,
                         AcMapODRecordIterator *pIter = NULL);
DllExport int gsc_renODTable(ads_name ent, C_STRING &OldODTableName, C_STRING &NewODTableName);
int gsc_oddeletetab(const TCHAR *TableName);
int gs_getODTablesfromSS(void);
int gsc_getODTables(C_SELSET &SelSet, C_STR_LIST &ODTables, bool IncludeGEOsimOD = false, int CounterToVideo = GS_BAD);
int gsc_getODTables(ads_name ent, C_STR_LIST &ODTables,
                    AcMapODRecordIterator *pIter = NULL, bool IncludeGEOsimOD = false);
int gsc_getODTables(AcDbObjectId &eId, C_STR_LIST &ODTables,
                    AcMapODRecordIterator *pIter = NULL, bool IncludeGEOsimOD = false);
int gs_getODTables(void);
int gsc_getODTables(C_STR_LIST &ODTables, bool IncludeGEOsimOD = false);
presbuf gsc_getRbFromODValue(const AcMapValue &ODValue);
int gsc_getValuesFromODTable(ads_name ent, C_STRING &ODTableName, C_RB_LIST &ColValues,
                             AcMapODRecordIterator *pIter = NULL);
presbuf gsc_getStruFromODTable(const TCHAR *ODTableName);
int gsc_renameODField(const TCHAR *ODTableName, const TCHAR *OldODFieldName, const TCHAR *NewODFieldName);

AcMapODTable *gsc_createODTable(C_STRING &ODTableName, C_LIST *_AttribList, 
                                bool AllCharDataType = false);
int gsc_setODRecord(ads_name ent, C_RB_LIST &ColValues,
                    AcMapODTable *pTable, AcMapODRecordIterator *pIter);
int gsc_PurgeODTables(const TCHAR *DwgPath = NULL);

// Extended Entity Data
int gsc_setID2EED(const TCHAR *handle, long ID);
int gsc_setID2EED(ads_name ent, long ID);

int gs_PurgeODTables(void);
int gs_RenameODField(void);

#endif
