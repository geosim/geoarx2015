/*************************************************************************/
/*   GS_ASE.H                                                            */
/*************************************************************************/


#ifndef _gs_ase_h
#define _gs_ase_h 1

#ifndef _ASECLASS_H
   #include <aseclass.h> 
#endif

#ifndef _gs_sql_h
   #include "gs_sql.h"
#endif

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

extern CAseAppl *GS_ASE;      // Riferimento all'ambiente ASE


class C_LINK
{
   private:
      ads_name ent;                    // oggetto grafico
      int      cls;                    // codice classe GEOsim
      int      sub;                    // codice sottoclasse GEOsim
      long     key;                    // valore chiave
      TCHAR    handle[MAX_LEN_HANDLE]; // handle oggetto grafico   

   public :
   DllExport C_LINK();
   DllExport C_LINK(ads_name _ent);
   DllExport ~C_LINK();

   DllExport int GetEnt(ads_name _ent);
   DllExport int GetHandle(TCHAR _handle[MAX_LEN_HANDLE]);
   DllExport int GetKey(long *key_val, int *cls_code = NULL, int *sub_code = NULL);
   DllExport int Set(ads_name _ent, int _cls, int _sub, long _key, int mode = INSERT);
   DllExport int Set(const TCHAR *_handle, int _cls, int _sub, long _key, int mode = INSERT);
   DllExport int SetEnt(ads_name _ent);
   DllExport int SetEnt(const TCHAR *_handle);
   DllExport int erase(ads_name _ent = NULL);

   private:
   void initialize(void);
   C_RB_LIST ColValues;                             // di uso interno
   presbuf   prb_Cls, prb_Sub, prb_Key, prb_Handle; // di uso interno
};


class C_LINK_SET
{
   private:
      int AddEntFromSQL(C_NODE *pCls, CAsiSession *pSession, const TCHAR *statement);

   public:
   DllExport C_LINK_SET();
   DllExport ~C_LINK_SET();

   int cls;
   int sub;
   DllExport C_SELSET     *ptr_SelSet(void);
   DllExport C_LONG_BTREE *ptr_KeyList(void);

   DllExport int clear(void);
   DllExport int copy(C_LINK_SET &Dest);

   int save(const TCHAR *Path);
   int load(const TCHAR *Path);

   DllExport int erase(C_SELSET &ss, int CounterToVideo = GS_BAD);
   DllExport int erase(ads_name ss, int CounterToVideo = GS_BAD);

   DllExport long getLinkQty();

   DllExport int GetSS(ads_name ent, C_SELSET &ss);
   DllExport int GetSS(int cls, int sub, long key, C_SELSET &ss);

             long GetCountClassObjs(int cls, int sub);
   DllExport int GetGSClassSS(int cls, int sub, C_SELSET &ss, C_LONG_BTREE *pKeyList = NULL,
                              int ObjType = GRAPHICAL);

   DllExport int GetHandleList(int cls, int sub, long key, C_STR_LIST &HandleList,
                               int What = ALL);

   double getApproxTimeForInitSQLCond(C_NODE *pCls, const TCHAR *Cond);
   DllExport int initSQLCond(C_NODE *pCls, const TCHAR *Cond, int ObjType = GRAPHICAL);

   double getApproxTimeForInitSelSQLCond(long Qty);
   DllExport int initSelSQLCond(C_NODE *pCls, C_SELSET &_SelSet, const TCHAR *Cond,
                                int ObjType = GRAPHICAL);
   DllExport int initSelSQLCond(C_NODE *pCls, C_LONG_BTREE &_KeyList, const TCHAR *Cond,
                                int ObjType = GRAPHICAL);

   DllExport int RefreshSS(C_SELSET &ss, int CounterToVideo = GS_BAD);
   DllExport int RefreshSS(ads_name ss, int CounterToVideo = GS_BAD);

   int add_ent(ads_name ent);
   int add_ent(long EntId, C_SELSET &EntSelSet);
   int add(C_SELSET &_SelSet);
   
   int unite(long _Key, C_SELSET &_SelSet);
   int unite(C_LINK_SET &_LinkSet);

   int subtract(long _Key, C_SELSET &_SelSet);
   int subtract(C_LINK_SET &_LinkSet);
   int subtract(C_SELSET &_SelSet);
   int subtract(C_LONG_BTREE &_KeyList);
   int subtract_ent(ads_name ent);
   int subtract_key(long Key);

   int intersect(C_LINK_SET &_LinkSet);
   int intersect(C_SELSET &_SelSet);

   int ClosestTo(C_LINK_SET &_LinkSet, int Qty = 1);
   int ClosestTo(C_SELSET &_SelSet, int Qty = 1);

   void terminate(void);

   private:
      C_SELSET     SelSet;
      C_LONG_BTREE KeyList;

      void initialize(void);
      C_RB_LIST ColValues;                             // di uso interno
      presbuf   prb_Cls, prb_Sub, prb_Key, prb_Handle; // di uso interno
};


extern CAseAppl *gsc_initase(void);
extern int gsc_termase(CAseAppl **pAppl);

int gsc_Table2MAPFormat(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &MapTableRef);
void gsc_AdjSyntaxMAPFormat(C_STRING &FieldName);
C_STRING gsc_AdjSyntaxDateToTimestampMAPFormat(C_STRING &statement);
C_STRING gsc_AdjSyntaxTimeToTimestampMAPFormat(C_STRING &statement);

// GESTIONE LINK ASE
DllExport int gsc_getASEKey(CAseLink &Link, long *Key, const TCHAR *LPName = NULL);
DllExport int gsc_getASEKey(CAseLink &Link, resbuf *rbKey, const TCHAR *LPName = NULL);
DllExport int gsc_getASEKey(CAseLink &Link, CAsiRow *pKeyCols, resbuf *rbKey);
DllExport int gsc_SetASEKey(C_SELSET &SelSet, int CounterToVideo = GS_BAD);
DllExport int gsc_EraseASELink(C_STRING &NameLPN, C_SELSET &SelSet, int CounterToVideo = GS_BAD);

// GESTIONE LINK PATH NAME
int gsc_getLinkNames(ads_name ent, C_STR_LIST &LPNList);
int gs_getLPNfromSS(void);
int gsc_getLinkNames(C_SELSET &SelSet, C_STR_LIST &listLPN, int CounterToVideo = GS_BAD);
int gsc_getDOR(const TCHAR *UDLName, C_DBCONNECTION *pConn, const TCHAR *TableRef,
               C_STRING &DOR);
DllExport int gsc_CreateLinkName(const TCHAR *NameLPN, const TCHAR *UDLName, C_DBCONNECTION *pConn,
                                 const TCHAR *TableRef, const TCHAR *KeyAttrib);
int gsc_EraseASELinkName(const TCHAR *NameLPN, int delLinkOnEnt = GS_BAD);
int gsc_getLPNameInfo(TCHAR *LPName, int *prj = NULL, int *cls = NULL, int *sub = NULL);

void gsc_printASEErr(CAseApiObj *dsc);
DllExport int gsc_LPNConnectAse(CAseLinkPath &LPN, const TCHAR *usr=NULL, const TCHAR *pwd=NULL);
int gsc_LPNDisconnectAse(CAseLinkPath &LPN);
int gsc_LPNDisconnectAse(const TCHAR *pName);
int gsc_disconnectUDL(const TCHAR *UDL);
int gsc_SetACADUDLFile(const TCHAR *UDLName, C_DBCONNECTION *pConn,
                       const TCHAR *TableRef);

extern int gsc_getBlockDef(ads_name entity, C_RB_LIST *BlockDefinition);

DllExport CAseAppl *get_GS_ASE();

// GESTIONE TABELLA LINK
int gsc_PrepareSelLinkWhereCls(C_DBCONNECTION *pConn, C_STRING &TableRef,
                               _CommandPtr &pCmd);
int gsc_PrepareSelLinkWhereKey(C_DBCONNECTION *pConn, C_STRING &TableRef,
                               _CommandPtr &pCmd);
int gsc_PrepareSelLinkWhereHandle(C_DBCONNECTION *pConn, C_STRING &TableRef,
                                  _CommandPtr &pCmd);

// FUNZIONI LISP
int gs_PurgeLPTs(void);
int gsc_PurgeLPTs(const TCHAR *DwgPath = NULL);

// Controllo istruzione SQL via ASI
void gsc_printASIErr(CAsiSQLObject *dsc);
CAsiAppl *gsc_initasi(void);
int gsc_termasi(CAsiAppl **pAppl);
int gsc_ASITermSession(CAsiSession **pSession);
int gsc_ASITermStm(CAsiExecStm **pStm);
int gsc_ASIPrepareSql(CAsiSession *pSession, const TCHAR *statement, const TCHAR *CsrName,
                      CAsiExecStm **pExecStm, CAsiCsr **pCsr, 
                      CAsiData **pParam1 = NULL, CAsiData **pParam2 = NULL, CAsiData **pParam3 = NULL,
                      int PrintError = GS_GOOD);
int gsc_ASIReadCol(CAsiCsr *pCsr, presbuf OutData, const TCHAR *ColName = NULL);
CAsiSession *gsc_ASICreateSession(const TCHAR *UDLName);
TCHAR *gsc_ASICheckSql(const TCHAR *UDLName, const TCHAR *statement);
int gsc_ReadASIParams(const TCHAR *UDLName, const TCHAR *TableRef, C_STRING &Where,
                      UserInteractionModeEnum WINDOW_mode = GSTextInteraction);
int gsc_ASIData2Rb(CAsiData *pAsiData, presbuf rb);
int gsc_Rb2ASIData(presbuf p_rb, CAsiData *pAsiData);
int gsc_InitASIReadRow(CAsiRow *pRow, C_RB_LIST &ColValues);
int gsc_ASIReadRow(CAsiCsr *pCsr, C_RB_LIST &ColValues);
int gsc_ASIReadRows(CAsiCsr *pCsr, C_RB_LIST &ColValues);

/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


#endif
