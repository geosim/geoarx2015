/*************************************************************************/
/*   GS_CONV.h                                                           */
/*************************************************************************/


#ifndef _gs_conv_h
#define _gs_conv_h 1


int gs_MainTabsConv2ActualVer(void);
int gsc_MainTabsConv2ActualVer(void);

int gs_MainTabsConv2DB(void);

int gs_ClsConv2ActualVer(void);
DllExport int gsc_ClsConv2ActualVer(int prj, C_INT_LIST &ClassList);

int gsc_isCompatibGEOsimVersion(const TCHAR *Version);
int gs_isCompatibGEOsimVersion(void);

int gs_setActualGEOsimVersion(void);

int gs_change_group_link(void);
int gsc_change_group_link(C_CLASS *pCls, C_INT_INT_LIST &NewLinkList);

int gsc_prepareScriptForPurgeAll(FILE *ScriptFile);

int gs_UpdateGasSimStru(void);

int gs_UpdateIrenSimStru(void);
int gsc_updateIrenSimStru(int prj, int cls);

#endif
