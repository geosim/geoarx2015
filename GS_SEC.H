/*************************************************************************/
/*   GS_SEC.H                                                          */
/*************************************************************************/


#ifndef _gs_sec_h
#define _gs_sec_h 1

                                                                          
//////////////////////////////////////////////////////////////////////////
// CLASSI PER LE TABELLE SECONDARIE
//////////////////////////////////////////////////////////////////////////
class C_INFO_SEC : public C_INFO
{
   friend class C_SECONDARY;

   private :
      // Da vedere se corretto per secondarie tabelle appoggio
      C_STRING SupportTableRef;     // Riferimento completo alla tabella TEMP di supporto
      C_STRING SupportLnkTableRef;  // Riferimento completo alla tabella TEMP dei link di supporto

   public :
      C_STRING key_pri;             // Nome attributo della classe madre
      C_STRING vis_attrib;          // Nome attributo per l'ordinamento dei dati
      bool     vis_attrib_desc_order_type; // se vero impone l'ordinamento descrescente

      C_STRING real_init_distance_attrib;     // per la segmentazione dinamica, Nome attributo per
                                              // la distanza reale tra il primo vertice della 
                                              // polilinea e l'inizio del segmento
      C_STRING real_final_distance_attrib;    // per la segmentazione dinamica, Nome attributo per
                                              // la distanza reale tra il primo vertice della 
                                              // polilinea e la fine del segmento
      C_STRING nominal_init_distance_attrib;  // per la segmentazione dinamica, Nome attributo per
                                              // la distanza nominale tra il primo vertice della 
                                              // polilinea e l'inizio del segmento
      C_STRING nominal_final_distance_attrib; // per la segmentazione dinamica, Nome attributo per
                                              // la distanza nominale tra il primo vertice della 
                                              // polilinea e la fine del segmento
      C_STRING real_offset_attrib;            // per la segmentazione dinamica, Nome attributo per
                                              // la distanza (offset) dalla linea. usato solo
                                              // nel caso GSPunctualDynSegmentation

   C_INFO_SEC();

DllExport int copy(C_INFO_SEC* out);

   resbuf *to_rb(bool ConvertDrive2nethost = false, bool ToDB = false);
   int from_rb(resbuf *rb);
DllExport int from_rb_dynamic_segmentation_info(resbuf *rb);
DllExport resbuf * to_rb_dynamic_segmentation_info(void);

   int from_rb_db(C_RB_LIST &ColValues);

   // I/O file
   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(TCHAR *filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   int reportHTML(FILE *file, bool SynthMode = false);
};
//-----------------------------------------------------------------------//   

class C_SECONDARY: public C_NODE
{
   friend class C_CLASS;

   private:
      C_CLASS *pCls;           // puntatore a classe

      // FUNZIONI AUSILIARIE PER SALVATAGGIO
      int get_all_temp_rel_sec(_RecordsetPtr &pRs);

   public :
      int           code;
      enum SecondaryTableTypesEnum type; // tipo di secondaria
      TCHAR         name[MAX_LEN_CLASSNAME];
      C_STRING      Descr;              // Descrizione secondaria
      GSDataPermissionTypeEnum abilit;  // se invisib, in sola lettura, modificabile 
      int           sel;                // se = SELECTED già scritta in GS_CLASS_FILE
      C_INFO_SEC    info;
      C_ATTRIB_LIST attrib_list;

   public :
DllExport C_SECONDARY();
 
DllExport C_ATTRIB_LIST *ptr_attrib_list()  { return &attrib_list; }
DllExport C_INFO_SEC    *ptr_info()         { return &info; }

DllExport C_CLASS	*ptr_class()	 { return pCls; }

DllExport int   get_key(void);
DllExport int   get_type(void);
DllExport TCHAR *get_name(void);
          bool isValidName(TCHAR *_name);
DllExport TCHAR *get_Descr(void);
          bool isValidDescr(TCHAR *_Descr);
	  
DllExport int get_Bitmap(bool LargeSize, CBitmap &CBMP);

DllExport int copy(C_SECONDARY* out, C_CLASS *pNewCls = NULL);
DllExport int baseval_to_rb_db(C_RB_LIST &ColValues);

// I/O file
          int     ToFile(C_STRING &filename);
          int     ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
          int     load(TCHAR *filename, C_CLASS *pClass, int sec);
          int     load(C_PROFILE_SECTION_BTREE &ProfileSections, C_CLASS *pClass, int sec);
          int     ToFile_id(C_STRING &filename, const TCHAR *sez);
          int     ToFile_id(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
          int     load_id(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
         
          int     read_stru(C_PROFILE_SECTION_BTREE &ProfileSections, C_CLASS *pCls, int sec);
          presbuf read_attrib(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
         
          int     from_rb   (resbuf *rb);
          presbuf to_rb     (void);
          int     from_db   (C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
          int     from_rb_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
          int     restore_attriblist(void);
          int     is_validsec(int prj);
          int     create_tab(int type);
          int     create_tab_link(int Status, int Type = OLD, int isSupport = GS_BAD);
          int     create();
          int     del(const TCHAR *Password);
DllExport int     ins_sec_to_gs_sec(C_RB_LIST &ColValues);
          int     new_code(int *new_code);
          int     default_to_db(C_ATTRIB_LIST *p_attrib_list);
DllExport int     carattattr_to_db(C_ATTRIB_LIST *p_attrib_list);
          int     del_tab_sec();
          int     del_db_carattattr();
          int     CheckValidFuncOnData(C_ATTRIB_LIST *p_attrib_list);
          int     ChangeCalcFuncOnData(C_ATTRIB_LIST *p_attrib_list);
          int     CheckMandatoryOnData(C_ATTRIB_LIST *p_attrib_list, bool AssignDefault = FALSE);
       
          // MODIFICA DEFINIZIONE CLASSE
          int     mod_stru(void);
          
          int     read_sec_extattriblist(void);
          int     link();
          int     mod_link(C_INFO_SEC *pinfo);
          int     mod_sec_to_gs_sec(C_RB_LIST &ColValues, _RecordsetPtr &RsCls);
          int     mod_sec_to_gs_sec(C_RB_LIST &ColValues);
          int     mod_vis(C_STRING &vis, bool desc_order_type = false);
          int     mod_dynamic_segmentation(C_STRING &_real_init_dist_attrib,
                                           C_STRING &_real_final_dist_attrib,
                                           C_STRING &_nominal_init_dist_attrib,
                                           C_STRING &_nominal_final_dist_attrib,
                                           C_STRING &_real_offset_attrib);
          int     modi_tab(C_ATTRIB_LIST *p_attrib_list, presbuf link);
          int     mod_name(TCHAR *name_sec);
          int     mod_Descr(TCHAR *Descr_sec);
DllExport int     get_temp_tablename(C_STRING &pathfile);

          int    is_updateable(int *WhyNot = NULL);

DllExport int    ins_data(long key_pri, C_RB_LIST &ColValues, long *gs_id = NULL,
                          bool CheckEnt = TRUE);
DllExport int    upd_data(long key_pri, long gs_id, C_RB_LIST &ColValues,
                          C_PREPARED_CMD *pTempCmd = NULL);
DllExport int    upd_data(long key_pri, C_LONG_BTREE &KeyList, C_RB_LIST &ColValues);
DllExport int    del_data(long key_pri, long gs_id);
DllExport int    query_data(long gs_id, C_RB_LIST &ColValues,
                            C_PREPARED_CMD_LIST *pTempOldCmdList = NULL,
                            C_PREPARED_CMD_LIST *pTempOldRelCmdList = NULL);
DllExport int    query_AllData(presbuf key_pri, C_RB_LIST &ColValues);
DllExport int    query_AllData(long key_pri, C_RB_LIST &ColValues);
          int    synchronize(void);

          // stampa dati
DllExport int data_to_html(const TCHAR *Path, long gs_id, 
                           C_PREPARED_CMD_LIST *pTempOldCmdList = NULL,
                           C_PREPARED_CMD_LIST *pTempOldRelCmdList = NULL,
                           C_STR_LIST *PrintableAttrNameList = NULL,
                           const TCHAR *Mode = _T("w"));

          int CheckValue(C_ATTRIB *pAttrib, presbuf Value);
        
          int     GetNewSecCode();
        
          int     CalcValidData(C_RB_LIST &ColValues, int Op);
DllExport int get_default_values(C_RB_LIST &ColValues);

          // reindicizzazione
          int reindex(bool OnlyTemp = false);
          int RefreshTempLastId(void);
        
          // salvataggio
          int save_DBdata(void);
        
          // RIFERIMENTI ALLE TABELLE
DllExport int getOldLnkTableRef(C_STRING &TableRef);      // Tabella old dei link della sec.
          int getTempTableRef(C_STRING &TableRef, int Create = GS_GOOD);    // Tabella temp della sec.
          int getTempLnkTableRef(C_STRING &TableRef); // Tabella temp dei link della sec.
          // Da vedere se giusto usare un tabella di appoggio
          int getSupportTableRef(C_STRING &TableRef);     // Tabella di appoggio temp della sec.
          int getSupportLnkTableRef(C_STRING &TableRef);  // Tabella di appoggio temp dei link della sec.
        
          // funzioni per lettura dati da tabella TEMP e OLD
          DllExport int prepare_data(C_PREPARED_CMD_LIST &CmdList, const TCHAR *what = NULL,
                                     const TCHAR *SQLWhere = NULL);
          DllExport int prepare_data(C_PREPARED_CMD &pCmd, int Type, const TCHAR *what = NULL,
                                     const TCHAR *SQLWhere = NULL);
          DllExport int prepare_data(_CommandPtr &pCmd, int Type, const TCHAR *what = NULL,
                                     const TCHAR *SQLWhere = NULL);
          _RecordsetPtr prepare_data(int Type);
        
          // funzioni per lettura dati da tabella relazioni TEMP e OLD
          int prepare_reldata_where_keyAttrib(C_PREPARED_CMD_LIST &CmdList);
          int prepare_reldata_where_keyAttrib(C_PREPARED_CMD &pCmd, int Type);
          _RecordsetPtr prepare_reldata_where_keyAttrib(int Type);
          int prepare_reldata_where_keyAttrib(_CommandPtr &pCmd, int Type);

          int prepare_reldata_where_keyPri(C_PREPARED_CMD_LIST &CmdList);
          int prepare_reldata_where_keyPri(C_PREPARED_CMD &pCmd, int Type);
          _RecordsetPtr prepare_reldata_where_keyPri(int Type);
          int prepare_reldata_where_keyPri(_CommandPtr &pCmd, int Type);
        
          // Funzioni NUOVE
          int CopyDataToTemp(long key_pri,
                             C_PREPARED_CMD *pOldCmd = NULL,
                             C_PREPARED_CMD_LIST *pTempOldRelCmdList = NULL);
        
          int isDataModified(void);
DllExport DynamicSegmentationTypeEnum DynSegmentationType(void);
        
          int is_NewEntity(long Key);
DllExport int Backup(BackUpModeEnum Mode, TCHAR *Dest = NULL);
          C_SECONDARY* CopyToCls(int PrjDest, int ClsDest, int SubDest, DataCopyTypeEnum CopyType,
                                 const TCHAR *NewName, const TCHAR *NewDescr, const TCHAR *TableRef,
                                 const TCHAR *ConnStrUDLFile, C_2STR_LIST &PropList);
        
          int reportHTML(FILE *file, bool SynthMode = false);

          // File di supporto per la lista di scelta dei valori degli attributi
          int CopySupportFiles(C_SECONDARY *pDstSec);
          int DelSupportFiles(void);
};

class C_SECONDARY_LIST :  public C_LIST
{
   private :
      C_CLASS *mother_class;  // classe madre
      
   public :
      C_SECONDARY_LIST(C_CLASS *pCls = NULL);
      ~C_SECONDARY_LIST() {}
                                                                        
      C_CLASS *get_mother_class(void);
      int set_mother_class(C_NODE *p_class);

      int reportHTML(FILE *file, bool SynthMode = false);
};


DllExport extern C_SECONDARY *gsc_find_sec(int prj, int cls, int sub, int sec);

          extern int gs_create_tab_sec	   (void);
          extern int gsc_create_tab_sec (int prj, int cls, int sub, C_SECONDARY *pSec);
          extern int gsc_create_extern_secondary(int prj, int cls, int sub, C_SECONDARY *pSec);

          int gs_copy_sec(void);

          extern int gs_link_tab_sec		(void);
          extern int gsc_link_tab_sec		(int prj, C_SECONDARY *pSec);
          extern int gs_del_tab_sec	      (void);
          extern int gsc_del_tab_sec		(int prj, int cls, int sub, int sec,
                                           const TCHAR *Password);
          extern int gs_get_tab_sec	      (void);
          extern int gs_get_sec_attrib_list(void);
          extern int gs_get_sec_info	(void);
          extern int gs_mod_sec_link	(void);
          extern int gsc_mod_sec_link	(int prj, int cls, int sub, int sec,
                                        const TCHAR *key_pri, const TCHAR *key_attrib);
          extern int gs_mod_sec_vis	(void);
          int gsc_mod_sec_vis(int prj, int cls, int sub, int sec, 
                              C_STRING &vis_attrib, 
                              bool vis_attrib_desc_order_type = false);

          int gs_mod_sec_dynamic_segmentation(void);
          int gsc_mod_sec_dynamic_segmentation(int prj, int cls, int sub, int sec, 
                                               C_STRING &_real_init_dist_attrib,
                                               C_STRING &_real_final_dist_attrib, 
                                               C_STRING &_nominal_init_dist_attrib, 
                                               C_STRING &_nominal_final_dist_attrib,
                                               C_STRING &_reaL_offset_attrib);
          
          extern int gs_mod_sec_name	(void);
          extern int gsc_mod_sec_name	(int prj, int cls, int sub, int sec, TCHAR *name);
          extern int gs_mod_sec_descr	(void);
          extern int gsc_mod_sec_descr(int prj, int cls, int sub, int sec, TCHAR *NewDescr);
          extern int gs_getdefaultattrsec(void);
          extern int gsc_destroy_tab_sec (int prj, int cls, int sub, int sec,
                                          const TCHAR *Password);
         
          int gs_destroysec(void);

          extern int gsc_stru_valdef2attriblist(C_SECONDARY *pSec, int def);

          void gsddSecValueTransfer(void);
          int gsc_getSecLnkTableRef(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &LinkTableRef);

int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, int Cls, int Sub, long Key, 
                    C_RB_LIST &ColValues, int *Source = NULL);
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, int Sub, long Key, C_RB_LIST &ColValues);

int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, int Sub, long Key, 
                    _RecordsetPtr &pRs, int *IsRsCloseable);

DllExport TCHAR *gsc_checkValue(C_SECONDARY *pSec, presbuf pRbAttrib, C_ATTRIB *pAttrib,
                                TCHAR *Value);

DllExport int gsc_getSecTabBitmap(bool LargeSize, CBitmap &CBMP);

// Segmentazione dinamica
int gsc_get_dynamic_segmentation_intersections(C_2REAL_LIST &List1, C_2REAL_LIST &List2,
                                               C_2REAL_LIST &Result);
void gsddSecDynamicSegmentationAnalyzer(void);


#endif
