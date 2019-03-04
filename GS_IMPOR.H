#ifndef _gs_import_h
#define _gs_import_h 1

   DllExport int gsc_load_Princ_and_DABlk(C_SELSET &ssAll, ads_name ent, C_STRING &lpnName,
                             C_SELSET &SS_Princ, C_SELSET &SS_BlockDA, C_CLASS **pCObj,
                             CAseLinkSel *pLS = NULL);
	DllExport int gsc_RemoveInvalidObject(C_SELSET &ss, C_CLASS *pCls, int loadGEOsimObj);

	DllExport int RbToRbOnlyFieldCompatible(C_CLASS *pCls, C_RB_LIST &RbInput, 
                                           C_RB_LIST &RbOut);
	DllExport int RbToRbOnlyFieldCompatible(C_SECONDARY *pSec, C_RB_LIST &RbInput, 
                                           C_RB_LIST &RbOut);
   int gs_getLPNstatusFromMemory(void);
   int gs_setLPNstatusOnMemory(void);

   int gsc_importFromFile(C_CLASS *pCls, C_STRING &fileName, C_DBCONNECTION *pConn = NULL,
                          C_STRING *pTable = NULL, C_STRING *pKeyAttr = NULL);
   int gsc_importFromGraph(ads_name SelSet, C_CLASS *pCls, int ObjGEOsimLoad,
                           long BitForChangeToNewFAS, C_FAS *pNewFas = NULL,
                           C_STR_LIST *pLPNList = NULL, int RemoveLPN = GS_BAD,
                           C_STR_LIST *pODTableList = NULL, int RemoveODData = GS_BAD,
                           bool ReadAttrBlk = false);

   int gs_tDA();

   extern C_2STR_INT_LIST LPN_USER_STATUS;

#endif