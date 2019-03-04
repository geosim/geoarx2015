/*************************************************************************/
/*   GS_QUERY.H                                                          */
/*************************************************************************/


#ifndef _gs_query_h
#define _gs_query_h 1


int gs_ins_data(void);
int gs_erase_data(void);
int gs_aggr_data(void);
int gs_disaggr_data(void);

int gs_query_data(void);

// AGGIORNAMENTO DATI
int gs_upd_data(void);
int gsc_upd_data(ads_name entity, C_RB_LIST &ColValues);
int gs_upd_data_fromdb(void);
int gs_upd_grid_data(void);

// FUNZIONE SPECIALE PER AGGIORNAMNETO IN FASE DI SALVATAGGIO
int gsc_UpdDataForSave(C_CLASS *pCls, C_RB_LIST &ColValues, C_SELSET &EntSS,
                       C_STRING &SubstUpdUsrFun, C_STRING &BeforeUpdUsrFun, C_STRING &AfterUpdFun);
int gsc_getAction(TCHAR *usr_cmds, const TCHAR *when, const TCHAR *OrigCmd, C_STRING &result);

// LETTURA DATI DA TABELLE
int gsc_get_data(_CommandPtr &pTempCmd, long Key, C_RB_LIST &ColValues);
DllExport int gsc_get_data(C_PREPARED_CMD &pCmd, long Key, C_RB_LIST &ColValues);
int gsc_get_data(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, C_RB_LIST &ColValues, int *Source = NULL);
int gsc_get_data(_CommandPtr &pTempCmd, _CommandPtr &pOldCmd, long Key, C_RB_LIST &ColValues,
                 int *Source = NULL);
int gsc_get_data(_CommandPtr &pCmd, long Key, _RecordsetPtr &pRs);
DllExport int gsc_get_data(C_PREPARED_CMD &pCmd, long Key, _RecordsetPtr &pRs, int *IsRsCloseable);
int gsc_get_data(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, _RecordsetPtr &pRs,
                 int *IsRsCloseable, int *Source = NULL);

// LETTURA RELAZIONI DA TABELLE
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, long Key, C_RB_LIST &ColValues);
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, long Key, C_RB_LIST &ColValues);
int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, int Cls, long Key, 
                    C_RB_LIST &ColValues, int *Source = NULL);
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, long Key, _RecordsetPtr &pRs);
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, long Key, _RecordsetPtr &pRs, 
                    int *IsRsCloseable);

int gsc_get_reldata(_CommandPtr &pCmd, long Key, C_RB_LIST &ColValues);
int gsc_get_reldata(C_PREPARED_CMD &pCmd, long Key, C_RB_LIST &ColValues);
int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, 
                    C_RB_LIST &ColValues, int *Source = NULL);
int gsc_get_reldata(_CommandPtr &pCmd, long Key, _RecordsetPtr &pRs);
int gsc_get_reldata(C_PREPARED_CMD &pCmd, long Key, _RecordsetPtr &pRs, int *IsRsCloseable);

// LETTURA DATI IN GRIGLIA
int gsc_get_data_on_row(C_PREPARED_CMD &Cmd, long id_min, long id_max, _RecordsetPtr &pRs);


int gsc_prepare_for_linked_ent(C_PROJECT *pPrj, int Cls, C_FAMILY_LIST &family_list,
                               C_PREPARED_CMD_LIST &CmdList);
int gsc_is_member_of(C_CLASS *pClass, long key, C_FAMILY_LIST &family_list, int *result,
                     C_PREPARED_CMD_LIST *pCmdList = NULL);

int gsc_copy_rel_to_temp(_RecordsetPtr &pRsIns, C_RB_LIST &LinkValues,
                         int Status = UNMODIFIED);

DllExport long gsc_class_align(C_SELSET &selset, int change_fas, int AttribValuesFromVideo = GS_GOOD,
                               C_SELSET *pRefused = NULL, int CounterToVideo = GS_BAD,
                               int Reason = UNKNOWN_MOD);

int gsc_topo_align_for_modified(C_SELSET &SelSet, C_SELSET &ToAlign,
                                C_SELSET *pRefused, int CounterToVideo = GS_BAD);
int gsc_topo_align_for_erased(C_STR_BTREE &HandleList, C_SELSET &ToAlign,
                              C_SELSET *pRefused, int CounterToVideo = GS_BAD);

// per gestione file GS_DELET 
int gsc_EmptyGsDeletedNoSub(int *cls = NULL);


int set_GS_CURRENT_OPERATION(int Value = NONE);

// ALLINEAMENTO DA REATTORI PER SIMULAZIONI
int gsc_verify_topo_on_align_for_node(C_SELSET &SelSet, int CounterToVideo = GS_BAD);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

extern int GS_CURRENT_OPERATION; // codice operazione corrente di GEOsim

#if defined(GSDEBUG) // se versione per debugging
extern double  tempo, tempo1, tempo2, tempo3, tempo4, tempo5, tempo6, tempo7;
#endif

#endif