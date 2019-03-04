/*************************************************************************/
/*   GS_CLASS.H                                                          */
/*************************************************************************/


#ifndef _gs_class_h
#define _gs_class_h 1

#ifndef _gs_ase_h
   #include "gs_ase.h" 
#endif

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif

#ifndef _gs_sql_h
   #include "gs_sql.h" 
#endif

#ifndef _gs_dwg
   #include "gs_dwg.h" 
#endif

#ifndef _gs_dbgph_h
   #include "gs_gphdata.h" 
#endif

#ifndef _gs_thm_h
   #include "gs_thm.h" 
#endif


//-----------------------------------------------------------------------//
class C_ID
{
   public :
   C_NODE   *pPrj;           // puntatore a progetto
   int      code;            // codice classe
   int      sub_code;        // codice sottoclasse
   int      category;        // categoria
   int      type;            // tipo della categoria
   TCHAR    name[MAX_LEN_CLASSNAME]; // nome classe
   C_STRING Descr;                   // Descrizione classe
   GSDataPermissionTypeEnum abilit;  // se invisib, in sola lettura, modificabile 
   int      sel;             // se selezionato per la prossima estrazione
   TCHAR    usr_cmds[MAX_LEN_FIELD]; // comandi personalizzati
   // la sintassi per descrivere la chiamata ad un comando utente è:
   // <quando>.<in quale operazione>.<nome comando>[<separatore>]
   // <quando> ::= BEFORE|AFTER|SUBST
   // <in quale operazione> ::= INSERT|UPDATE|ERASE
   // <separatore> ::= ;
   int      modified;    // se = GS_GOOD un'entità è stata modificata
   C_STRING Version;     // Versione di GEOsim 
   bool     view_enabled;    // classe che ha delle viste associate
   bool     history_enabled; // classe che ha la storicizzazione abilitata

   C_ID();
 
   int copy(C_ID *out);

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(C_STRING &filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   resbuf *to_rb(void);
   int from_rb(resbuf *rb);

			 int from_rb_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
DllExport int to_rb_db(presbuf *rb);

   int refresh_abilit();
   int reportHTML(FILE *file, bool SynthMode = false);
};


//-----------------------------------------------------------------------//
class C_FAS : public C_NODE
{
   friend class C_FAS_LIST;
   friend class C_NODE;

   public :
   C_COLOR color;                          // colore int C_COLOR
   TCHAR   hatch[MAX_LEN_HATCHNAME];       // riempimento
   TCHAR   hatch_layer[MAX_LEN_LAYERNAME]; // piano del riempimento
   C_COLOR hatch_color;                    // colore del riempimento
   double  hatch_scale;                    // fattore di scala del riempimento
   double  hatch_rotation;                 // angolo di rotazione del riempimento (gradi)
   TCHAR   line[MAX_LEN_LINETYPENAME];     // tipo linea
   double  line_scale;                     // fattore di scala (tipolinea)
   TCHAR   style[MAX_LEN_TEXTSTYLENAME];   // stile testo
   TCHAR   layer[MAX_LEN_LAYERNAME];       // piano   
   TCHAR   block[MAX_LEN_BLOCKNAME];       // nome blocco
   double  block_scale;                    // fattore di scala (blocco)
   double  width;                          // larghezza linea
   double  elevation;                      // elevazione 
   double  rotation;                       // angolo di rotazione (gradi)
   double  h_text;                         // altezza testo
   double  thickness;                      // spessore
   TCHAR   *file_ref_block;                // direttorio + file DWG del blocco di riferimento
                                          // (senza alias)
   TCHAR  *ref_block;                     // nome del blocco di riferimento per caratteristi-
                                          // che grafiche e posizioni attributi
   TCHAR  *file_graph_calc;               // direttorio + file funzione lisp per grafica
                                          // (senza alias)
   TCHAR  *fun_graph_calc;                // funzione lisp per calcolare e variare le
                                          // caratteristiche grafiche degli oggetti grafici,
                                          // dei blocchi Disp.Attrib. e loro attributi
   C_STRING dimension_style;              // Nome dello stile di quotatura

DllExport C_FAS();
DllExport virtual ~C_FAS();

DllExport int copy(C_FAS *out);

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(C_STRING &filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(ads_name ent);

   resbuf *to_rb(bool ConvertDrive2nethost = FALSE);
   int from_rb(C_RB_LIST &ColValues);
   int from_rb(resbuf *rb);

	int from_db(C_NODE *pPrj, int cls, int sub = 0);
   int to_db(C_NODE *pPrj, int cls, int sub = 0);
   int del_db(C_NODE *pPrj, int cls, int sub = 0);

DllExport long get_GraphDifferences(C_FAS &fas2);
   int is_graph_calculated(void);

   void convertUnit(double UnitConvFactor);

   int reportHTML(C_NODE *pCls, FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);

   int dd_create_LayerModel(int prj, int type);
};                                          

class C_FAS_LIST : public C_LIST
{
   friend class C_LIST;

   public :
   DllExport C_FAS_LIST();
   DllExport ~C_FAS_LIST();

   int copy(C_FAS_LIST &out);
   int from_rb(resbuf *rb);
   resbuf *to_rb(void);
   double getValue(C_FAS *pFAS, GraphSettingsEnum GraphSettingEnum);
   DllExport void sortByNum(GraphSettingsEnum GraphSettingEnum, int ascending = TRUE);

   DllExport int Save(C_STRING &filename, const TCHAR *SectionPrefix);
   DllExport int Load(C_STRING &filename, const TCHAR *sezSectionPrefix);
   DllExport int Load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *SectionPrefix);
};


//-----------------------------------------------------------------------//
class C_INFO
{
   friend class C_CLASS;
   friend class C_SECONDARY;
   friend class C_INFO_SEC;

   public :

   C_DBCONNECTION *pTempTabConn;    // Puntatore alla connessione OLE-DB per tabella TEMP

   C_STRING TempTableRef;        // Riferimento completo alla tabella TEMP
   C_STRING TempLnkTableRef;     // Riferimento completo alla tabella TEMP dei link
   
   bool     SeekSupportedOnTemp; // Metodo seek supportato sulla tabella temporanea
   bool     SeekSupportedOnOld;  // Metodo seek supportato sulla tabella temporanea

   C_DBCONNECTION *pOldTabConn;  // Puntatore alla connessione OLE-DB per tabella OLD

   C_STRING OldTableRef;         // Riferimento completo alla tabella OLD
   C_STRING OldLnkTableRef;      // Riferimento completo alla tabella OLD dei link
   C_STRING ConnStrUDLFile;      // Stringa di connessione o File di tipo .UDL
   C_2STR_LIST UDLProperties;    // Lista di proprietà UDL 

   bool     LinkedTable;         // Flag che indica se la tabella OldTableRef è interamente
                                 // gestita da GEOSIM (false) o se è una tabella già esistente
                                 // a cui ci si collega (true)
   C_STRING SqlCondOnTable;      // eventuale condizione SQL da applicare per cercare i dati nella
                                 // tabella OldTableRef nel caso questa non contenga dati eclusivamente
                                 // della classe (usato se LinkedTable = true)

   C_STRING key_attrib;          // Attributo chiave
   long     TempLastId;          // Valore dell'ultima entità
   long     OldLastId;           // Valore dell'ultima entità nella tabella OLD

   C_INFO();

DllExport C_DBCONNECTION* getDBConnection(int Type);
   
   int copy(C_INFO *out);
   
   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(TCHAR *filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   resbuf *to_rb(bool ConvertDrive2nethost = false, bool ToDB = false);
   virtual int from_rb(resbuf *rb);

   virtual int from_rb_db(C_RB_LIST &ColValues);

   int reportHTML(FILE *file, bool SynthMode = false);

   long getOldLastIdFromDB(void);
   int RefreshOldLastId(bool SetTempLastIdEqualToOldLastId = true);

   void TerminateSQL(C_DBCONNECTION *pConnToTerminate = NULL);
   int get_PrimaryKeyName(C_STRING &PriKeyName);

   int AdjTableParams(void);
};


//-----------------------------------------------------------------------//   
class C_GRID
{
public :
   double x, y;       // punto di origine
   double dx, dy;     // dimensione x e y cella griglia
   long   nx, ny;     // n. elementi x e y

   C_GRID() { x = y = dx = dy = 0; nx = ny = 0; }
   
   int copy(C_GRID *out);
   
   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(TCHAR *filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   resbuf *to_rb(void);
   int from_rb(resbuf *rb);

   int from_rb_db(C_RB_LIST &ColValues);
   int to_rb_db(presbuf *rb);
   int is_valid(void);

   DllExport int pt2key(ads_point pt, long *key);
   DllExport int key2pt(long key, ads_point pt, bool Centroid = TRUE);
   DllExport int RowColumn2pt(long Column, long Row, ads_point pt, bool Centroid = TRUE);
   int key2Rect(long key, ads_point pt1, ads_point pt2);
   AcDbPolyline *key2Rect(long key);
   long getRow(ads_point pt);
   long getColumn(ads_point pt);
   long getRow(long Key);
   long getColumn(long Key);
   long getKey(long Column, long Row);
   double getXLimit();
   double getYLimit();
   void getExtension(C_RECT &Rect, bool SubtractOffSetForTopRight = false);

   DllExport int getKeyListInWindow(C_RECT &Rect, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListInWindow(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListInCircle(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListInPolygon(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListInEntity(AcDbEntity *pEnt, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListFence(C_RB_LIST &CoordList, C_LONG_BTREE &KeyList);
   int getKeyListBufferFence(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList);
   int getKeyListInsideSS(C_SELSET &EntSS, int Mode, C_LONG_BTREE &KeyList);

   int reportHTML(FILE *file, bool SynthMode = false);

private:
   int getKeyListBufferFence(AcGePoint3d &Vertex, AcGePoint3d &NextVertex, 
                             double Bulge, double OffSet, int Mode,
                             C_LONG_BTREE &KeyList);
};

//-----------------------------------------------------------------------//
class C_CONNECT_LIST : public C_INT_INT_LIST
{
   public :
DllExport C_CONNECT_LIST() : C_INT_INT_LIST() { }
                  
DllExport virtual ~C_CONNECT_LIST(){ } 

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(TCHAR *, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   int mod(C_CONNECT_LIST *in);
   int once_selclass(void);
DllExport int is_to_be_connected(void);
   int is_to_be_unique(void);

   int reportHTML(FILE *file, C_NODE *pPrj, int cls, int sub, bool SynthMode = false);
};
//-----------------------------------------------------------------------//
// Key  = il codice della classe 
// Type = la quantità di entità (se <= 0 non è definita)
class C_GROUP_LIST : public C_INT_INT_LIST
{
   public :
   C_GROUP_LIST() : C_INT_INT_LIST() { }
               
   virtual ~C_GROUP_LIST(){ } 

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(TCHAR *filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int is_valid(C_NODE *pPrj);

   int reportHTML(FILE *file, C_NODE *pPrj, bool SynthMode = false);
};


//-----------------------------------------------------------------------//
// Classe per campi attributi su DB delle classi //
//-----------------------------------------------------------------------//
class C_ATTRIB : public C_NODE 
{
   friend class C_CLASS;
   friend class C_SECONDARY;  
   friend class C_ATTRIB_LIST;

   private :

   public :
   C_STRING name;                    // Nome attributo
   int      OrderPos;                // Posizione dell'attribtuo per la visualizzazione
   C_STRING Caption;                 // Etichetta attributo
   TCHAR    type[MAX_LEN_FIELDTYPE]; // Descrizione tipo
   long     len;                     // Dimensioni (-1 se non usato)
                                     // rappresenta il numero delle cifre memorizzabili
                                     // (segno e sep. decimale non contano)
   int      dec;                     // Decimali (-1 se non usato)
                                     // rappresenta il numero delle cifre a destra del
                                     // separatore decimale
   int      vis;                     // Flag di visibilità (GS_GOOD o GS_BAD)
   int      VisOnTooltip;            // Flag di visibilità nella tooltip della
                                     // interrogazione dinamica (GS_GOOD o GS_BAD)
   int      man;                     // Flag di obbligatorietà (GS_GOOD o GS_BAD)
   TCHAR    *calc_file;              // File contenente la funzione di calcolo
   TCHAR    *calc_func;              // Funzione di calcolo (nome o corpo)
   TCHAR    *valid_file;             // File contenente la funzione di validità
   TCHAR    *valid_func;             // Funzione di validità (nome o corpo)
   int      IsDefCalc;               // Flag che indica se la funzione di calcolo
                                     // è riferita al valore di default (GS_GOOD, GS_BAD)
   resbuf   *def;                    // Valore di default
   int      mod_attr;                // Flag di modificabilità attributo
   C_STRING Descr;                   // Descrizione attributo
   C_STRING ValidErrMsg;             // Messaggio di errore per validazione non rispettata
   C_STRING GphPrefix;               // Prefisso da inserire in grafica se attributo visibile
   C_STRING GphSuffix;               // Suffisso da inserire in grafica se attributo visibile
   C_STRING InputMask;               // Maschera di input

   DataTypeEnum ADOType;             // Tipo dell'attributo in formato ADO per tabella OLD
   DataTypeEnum TempADOType;         // Tipo dell'attributo in formato ADO per tabella TEMP

DllExport C_ATTRIB(); 
DllExport C_ATTRIB(C_STRING &_name, int _OrderPos, C_STRING &_Caption, TCHAR *_type,
                   long _len, int _dec, int _vis, int _VisOnTooltip, int _man,
                   TCHAR *_calc_file, TCHAR *_calc_func, int _IsDefCalc,
                   TCHAR *_valid_file, TCHAR *_valid_func, C_STRING &_ValidErrMsg,
                   resbuf *_def, int _mod_attr, C_STRING &_Descr,
                   C_STRING &_GphPrefix, C_STRING &_GphSuffix,
                   C_STRING &_InputMask); 

DllExport virtual ~C_ATTRIB();
            
DllExport TCHAR *get_name();
DllExport int is_validated();
DllExport int is_calculated(void);
   int is_DefCalculated(void);
DllExport int is_visible(void);
DllExport int is_VisibleOnTooltip(void);
DllExport int is_mandatory(void);
   int is_valid(C_DBCONNECTION *pConn);
DllExport int is_from_graph();
   int sql_to_drv(TCHAR *drvname);

DllExport int CheckReal(ads_real *Value);
DllExport int CheckValue(presbuf Value);
   int from_rb(presbuf p_rb_attrib);
   presbuf to_rb(bool ConvertDrive2nethost = false);
   
   presbuf NoGraphInfoToRb(bool ConvertDrive2nethost = false);
   int NoGraphInfoFromRb(C_RB_LIST &ColValues);
   int NoGraphInfoFromRb(resbuf *rb);
   presbuf GraphInfoToRb(void);
   int GraphInfoFromRb(C_RB_LIST &ColValues);
   int GraphInfoFromRb(resbuf *rb);

DllExport int set(C_ATTRIB *in);
DllExport int set_charact(int _OrderPos, C_STRING &_Caption, int _vis, int _VisOnTooltip, int _man,
                          TCHAR *_calc_file, TCHAR *_calc_func, int _IsDefCalc,
                          TCHAR *_valid_file, TCHAR *_valid_func, C_STRING &_ValidErrMsg,
                          C_STRING &_Descr,
                          C_STRING &_GphPrefix, C_STRING &_GphSuffix,
                          C_STRING &_InputMask); 
   int set(C_STRING &_name, int _OrderPos, C_STRING &_Caption, TCHAR *_type,
           long _len, int _dec, int _vis, int _VisOnTooltip, int _man,
           TCHAR *_calc_file, TCHAR *_calc_func, int _IsDefCalc,
           TCHAR *_valid_file, TCHAR *_valid_func, C_STRING &_ValidErrMsg,
           resbuf *_def, int _mod_attr, C_STRING &_Descr,
           C_STRING &_GphPrefix, C_STRING &_GphSuffix,
           C_STRING &_InputMask); 

   int ToFile(C_STRING &filename, const TCHAR *sez, int n_attrib);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez, int n_attrib);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez, C_DBCONNECTION *pConn, int n_attrib);

   int reportHTML(FILE *file, int prj, int cls, int sub, int sec, 
                  bool SynthMode = false, int What = 0);

DllExport int Convert2ProviderType(C_DBCONNECTION *pSrc, C_DBCONNECTION *pDest);
DllExport int ParseToString(presbuf ValIn, C_STRING &ValOut,
                            presbuf ColValues, int cls = 0, int sub = 0, int sec = 0);
DllExport int ParseToString(presbuf ValIn, C_STRING &ValOut,
                            C_RB_LIST *pColValues = NULL, int cls = 0, int sub = 0, int sec = 0);

   int init_ADOType(C_DBCONNECTION *pConn);
   int init_TempADOType(C_DBCONNECTION *pOldConn, C_DBCONNECTION *pTempConn);  
};


//-----------------------------------------------------------------------//
class C_ATTRIB_LIST : public C_LIST
{
   friend class C_ATTRIB;

   public :

   C_INT_VOIDPTR_LIST CalcOrderAttribPtrList; // Lista di puntatori agli attributi per 
                                              // determinare l'ordine di calcolo (per gestire
                                              // gli attributi calcolati da calcolati)


DllExport C_ATTRIB_LIST();
DllExport virtual ~C_ATTRIB_LIST();

      void sort_by_orderpos(void);   
      int ToFile(C_STRING &filename, const TCHAR *sez);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
      int load(TCHAR *filename, const TCHAR *sez, C_DBCONNECTION *pConn);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez, C_DBCONNECTION *pConn);

DllExport resbuf *to_rb(void);
DllExport int from_rb(resbuf *rb);
      presbuf to_rb_db(void);

      int from_Block(C_DBCONNECTION *pConn, const TCHAR *BlockName);
      int from_OD(C_DBCONNECTION *pDestConn, const TCHAR *ODTableName); 
DllExport int from_DB(C_DBCONNECTION *pDestConn, C_DBCONNECTION *pSrcConn,
                      const TCHAR *TableName); 

DllExport int get_StaticDefValues(C_RB_LIST &ColValues);
DllExport int set_default_values(C_RB_LIST &ColValues);

DllExport int copy(C_ATTRIB_LIST *out);
      int mod(C_ATTRIB_LIST *in);
      int mod_def(C_ATTRIB_LIST *in);
      int mod_charact(C_ATTRIB_LIST *in);
      int is_in_calc(TCHAR *fieldname, int FilterOnVisib = GS_BAD);
      int is_in_valid(TCHAR *fieldname);
      int is_funcparam(TCHAR *fieldname);
      int is_valid(C_DBCONNECTION *pConn, C_ATTRIB **pWrongAttrib = NULL);
      int is_valid(int category, int type, C_DBCONNECTION *pConn, C_STRING &key_attrib,
                   C_FAS *p_fas = NULL, C_ATTRIB **pWrongAttrib = NULL);
DllExport presbuf get_vis_values(C_RB_LIST &ColValues);
DllExport int is_calculated(void);
      int is_DefCalculated(void);
      int is_validated();
      int is_mandatory();
DllExport int is_visible(int *Qty = NULL);
      C_ATTRIB* getFirstVisibleAttrib();
DllExport int calc(const TCHAR *attr, C_RB_LIST &ColValues, int Op); // = MODIFY
DllExport int calc_all(C_RB_LIST &ColValues, int Op, // = MODIFY
                       C_SELSET *GlobalSelSet = NULL);
DllExport int validate(const TCHAR *attr, C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib = NULL);
      int validate_all(C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib = NULL);
DllExport int is_from_graph();
      int TestAllCalc(C_RB_LIST &ColValues);
      int TestAllValid(C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib = NULL);
      int getVisPos_name(const TCHAR *in);  // ordinal position (1 è il primo)
DllExport int WhatIsChanged(C_ATTRIB_LIST *p_attrib_list, presbuf link,
                        int *stru, int *charact, int *def, int *vis,
                        int *calc, int *valid, int *mand);
DllExport int CheckValues(C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib = NULL);

      int reportHTML(FILE *file, int prj, int cls, int sub, int sec,
                     bool SynthMode = false);

DllExport int Convert2ProviderType(C_DBCONNECTION *pSrc, C_DBCONNECTION *pDest);

DllExport int init_ADOType(C_DBCONNECTION *pConn);
      int init_TempADOType(C_DBCONNECTION *pOldConn, C_DBCONNECTION *pTempConn);

DllExport int FilterOnNumericType(void);
DllExport int FilterOnCharType(void);

DllExport int ParseToString(C_RB_LIST &ColValues, C_STRING &Out, bool ToolTipFmt = FALSE,
                            int cls = 0, int sub = 0, int sec = 0);

          int CopySupportFiles(int SrcPrj, int SrcCls, int SrcSub, int SrcSec,
                               int DstPrj, int DstCls, int DstSub, int DstSec);
          int DelSupportFiles(int Prj, int Cls, int Sub, int Sec);

          int InitCalcOrderPos(void);
          int GetTxtFileAttribFormats(int modo, TCHAR *delimiter, C_STR_LIST &Formats);
          int sprintfHeader(FILE *fhandle, C_STR_LIST &Formats);
};

///////////////////////////////////////////////////////////////////////////

class C_CLASS : public C_NODE
{
   friend class C_PROJECT;
   friend class C_SPAGHETTI;
   friend class C_SIMPLEX;
   friend class C_GROUP;
   friend class C_CGRID;
   friend class C_SUB;
   friend class C_EXTERN;
	friend class C_TOPOLOGY;

   protected :
      C_ID id;
      long m_BitForUpdateableFAS;

   private:
      // LETTURA E SCRITTURA DEFINIZIONE CLASSE IN DATABASE
      int mod_cls_to_gs_class(C_RB_LIST &ColVal, _RecordsetPtr &RsCls);
      virtual int from_db    (C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);

      // VALORI DI DEFAULT
      int default_to_db(C_ATTRIB_LIST *p_attrib_list);

      // STRUTTURA TABELLA
      int create_tab(void);
      int modi_tab(C_ATTRIB_LIST *p_attrib_list, presbuf link);
      int Check_KeyPri_On_Sec(C_ATTRIB_LIST &NewAttribList);

      // CARATTERISTICHE DEGLI ATTRIBUTI
      int del_db_carattattr(void);

      // SET DI CLASSI
      int del_db_ClassSet(void);

      // LISTA DELLE CONNESSIONI CON LE ENTITA' DELLE ALTRE CLASSI
      int restore_connectlist(void);
      int del_db_conc        (int mode = ALL + 1);
      int del_mem_conc       (void);

      // MULTIUTENZA

      // COPIA DI CLASSI
      virtual int AlignGraphDataOnCopyClass(C_CLASS *old_cls);

      // INSERIMENTO RECORD IN TABELLA
      virtual int ins_row(C_RB_LIST &ColValues, long *gs_id = NULL,
                          C_CLS_PUNT_LIST *lista_cls_punt = NULL,
                          _RecordsetPtr pRsIns = NULL, C_SELSET *GlobalSelSet = NULL);

      // FUNZIONI AUSILIARIE PER SALVATAGGIO
      int get_list_mother_group(C_PREPARED_CMD_LIST &list_mother);
      int get_alldata_from_temp(_RecordsetPtr &pRs, long *original_last);
      int get_all_reldata_from_temp(_RecordsetPtr &pRs);
      int prepare_del_reldata_from_old_where_key(C_PREPARED_CMD &pCmd);
      int get_list_mother_sec(C_PREPARED_CMD_LIST &list_mother);
      long delete_2_old(void);
      int save_default(void);

      // Utilizzata da IMPORTAZIONE
      virtual int import_data(C_SELSET &ObjGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, _RecordsetPtr pInsRs,
                              C_CLASS *pObj = NULL, int loadGSent = GS_GOOD,
                              long BitForChangeToNewFAS = GSNoneSetting, C_FAS *pNewFas = NULL);

      // estrazione disegni
      long extract_GraphData_internal(int mode, C_SELSET *pSS = NULL,
                                      TCHAR *SpatialQryName = ADE_SPATIAL_QRY_NAME,
                                      C_STR_LIST *IdList = NULL,
                                      C_STRING *pRptTemplate = NULL,
                                      C_STRING *pRptFile = NULL, int CounterToVideo = GS_BAD);

   public :
      C_LIST *ptr_secondary_list;  // lista delle tabelle secondarie

      C_CLASS();
      virtual ~C_CLASS();

      int   get_key(void);
DllExport int   get_category(void);
DllExport int   get_type(void);
      TCHAR *get_name(void);
DllExport void get_CompleteName(C_STRING &CompleteName);

      virtual int copy(C_CLASS *out);

      // CARATTERISTICHE DEGLI ATTRIBUTI
DllExport int restore_attriblist(void);

      //------------ ACCESSO AI DATI DELLA CLASSE --------------------------//
      C_ID                     *ptr_id() { return &id; }
      virtual C_FAS            *ptr_fas()          { return NULL; }
      virtual C_GPH_INFO       *ptr_GphInfo()      { return NULL; }
      virtual C_INFO           *ptr_info()         { return NULL; }
      virtual C_GRID           *ptr_grid()         { return NULL; }
      virtual C_CONNECT_LIST   *ptr_connect_list() { return NULL; }
      virtual C_GROUP_LIST     *ptr_group_list()   { return NULL; }
      virtual C_LIST           *ptr_sub_list()     { return NULL; }
      virtual C_ATTRIB_LIST    *ptr_attrib_list()  { return NULL; }

DllExport int get_PrjId(void);
DllExport C_PROJECT *get_pPrj();

DllExport int get_Bitmap(bool LargeSize, CBitmap &CBMP);

      virtual int is_member_class(int cls) { return (id.code == cls) ? GS_GOOD : GS_BAD; }

      // RIFERIMENTI ALLE TABELLE
DllExport int getTempTableRef(C_STRING &_TempTableRef, int Create = GS_GOOD);
DllExport int getOldLnkTableRef(C_STRING &TblRef);
      int getTempLnkTableRef(C_STRING &TblRef);

      // GESTIONE LPN
      int getLPNameOld(C_STRING &LPName);
      int getLPNameTemp(C_STRING &LPName);
DllExport int GetLPN4OLD (C_STRING &LPNName, int Create = GS_GOOD);
      int DisconnectAse4OLD();

      // GESTIONE INFO SORGENTI GRAFICHE
      virtual int allocGraphInfo(GraphDataSourceEnum Type);
      virtual void deallocGphInfo(void);
      virtual int GphDetach(void);

      // GESTIONE CARATTERISTICHE GRAFICHE
      virtual int is_validfas(int type, C_DBCONNECTION *pConn, C_ATTRIB_LIST *p_attrib_list);
      long what_is_graph_updateable(void);

      // GESTIONE FILE UDL
      int WriteOLDDBConnToAutoCADUDL(void);
      
	// RESE PUBBLICHE PER UTILIZZO IN MODULO ESTERNO
		
		// LETTURA E SCRITTURA DEFINIZIONE CLASSE IN DATABASE
DllExport int ins_cls_to_gs_class(C_RB_LIST &ColValues);
      // CARATTERISTICHE DEGLI ATTRIBUTI
DllExport int carattattr_to_db (C_ATTRIB_LIST *p_attrib_list);
DllExport int del_tab_class    (void);
      // CONNESSIONI OLE-DB
DllExport int to_db_connectlist  (C_CONNECT_LIST *list_conct);
      void TerminateSQL(C_DBCONNECTION *pConnToTerminate);


      //------------  ACCESSO A PROFILE-FILE CLASSI ------------------------//

      int ToFile_id(C_STRING &filename);
      int ToFile_id(C_PROFILE_SECTION_BTREE &ProfileSections);
      int is_validid();
      int ToFile_info(C_STRING &fileneme);
      int ToFile_info(C_PROFILE_SECTION_BTREE &ProfileSections);
      int ToFile_gphInfo(C_STRING &fileneme);
      int ToFile_gphInfo(C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int load_info(TCHAR *fileneme);
      virtual int is_validinfo();
      int ToFile_grid(C_STRING &filename);
      int ToFile_grid(C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int load_grid(TCHAR *filename);
      int ToFile_connect_list(C_STRING &filename);
      int ToFile_connect_list(C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int load_connect_list(TCHAR *filename);
      virtual int is_validconnectlist(void);
      int ToFile_group_list(C_STRING &filename);
      int ToFile_group_list(C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int load_group_list(TCHAR *filename);
      int ToFile_attrib_list(C_STRING &filename);
      int ToFile_attrib_list(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load_attrib_list(TCHAR *filename);

      virtual int ToFile(C_STRING &filename);
      virtual int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int load(TCHAR *filename, int cl);
      virtual int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

DllExport virtual long GetNewEntCode();
      virtual int set_last(long value);

   //------ Solo per classe C_SUB ------------------//

      virtual int load(TCHAR *filename, int cl, int sub);
      virtual int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl, int sub);
      virtual int is_subclass();

   //------ Interfaccia con strutture LISP (liste di resbuf) ------------//

      virtual resbuf *to_rb(void);
      virtual int from_rb(resbuf *rb);

   //------ Creazione  ------------//

      virtual int create(void);
      // Lettura/scrittura struttura classe da file per creazione automatica
      virtual presbuf read_stru   (C_PROFILE_SECTION_BTREE &ProfileSections);
      virtual int write_stru      (C_STRING &filename, C_DBCONNECTION *pConn = NULL);
      presbuf read_id             (C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));      
      presbuf read_fas            (C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));
      presbuf read_info           (C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));
      presbuf read_GphInfo        (C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));
      presbuf read_attrib         (C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez = _T("0.A"));
      virtual presbuf read_connect(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));

   //------ Cancellazione ------------//

      virtual int del   (const TCHAR *Password);

      // MODIFICA DEFINIZIONE CLASSE
      int mod_stru(void);

   //-------- TABELLE SECONDARIE ------------//

DllExport C_NODE *find_sec(int sec);
      C_NODE *load_sec_from_db(int cod_sec);
DllExport int rtab_sec(C_INT_INT_STR_LIST &SinthSecList, int FilterType = -1,
                       int DynSegmentationSupportedOnly = GS_BAD);
      int del_all_tab_sec(const TCHAR *Password);
      int reindex_sec(bool OnlyTemp = false);
      int synchronize_sec(void);
      int CreateExternSecondary(C_NODE *pSec);
      int CreateSecondary(C_NODE *pSec);
DllExport int CalibrateDynSegmentationData(C_SELSET &SelSet, C_CLASS *pRefCls,
                                           C_STRING &DistAttrib, double Tolerance = 0.0,
                                           int CounterToVideo = GS_BAD);
      int CalibrateDynSegmentationData(int sec, long gs_id, C_2REAL_LIST &NominalGeomDistList);

   //------ Aree di lavoro ------------//

      int is_inarea        (long *SessionCode);
DllExport int is_extracted (void);

      TCHAR *check_sql      (const TCHAR *SQLWhere);

      // inserimento dati
      virtual int ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id = NULL, 
                           int visib_block = VISIBLE, int Reason = UNKNOWN_MOD,
                           _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
DllExport virtual int ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
      	                      long *gs_id = NULL, int visib_block = VISIBLE,
	                            int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);     
DllExport int prepareInsRs(_RecordsetPtr &pInsRs, int Type = TEMP);

      // interrogazione dati
DllExport virtual int query_data(long gs_id, C_RB_LIST &ColValues,
                                 C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport virtual int query_data(ads_name entity, C_RB_LIST &ColValues,
                                 C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport virtual int query_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues,
                                 int CounterToVideo = GS_BAD);
DllExport virtual int query_AllData(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues,
                                    C_RB_LIST &SingleColValues, int CounterToVideo = GS_BAD);
      virtual int query_data(ads_point pt, C_RB_LIST &ColValues,
                             C_PREPARED_CMD_LIST *pTempOldCmdList = NULL); // solo per griglie

          // STAMPA DEI DATI
DllExport int data_to_html(const TCHAR *Path, long gs_id, 
                           C_PREPARED_CMD_LIST *pTempOldCmdList = NULL,
                           int What = ALL, C_STR_LIST *PrintableAttrNameList = NULL,
                           const TCHAR *Mode = _T("w"));

DllExport int getKeyValue(ads_name ent, long *gs_id);
          int getKeyValue(C_RB_LIST &ColValues, long *key);
      
DllExport int get_SelSet(ads_name ent, C_SELSET &SelSet, int what = ALL);
DllExport int get_SelSet(long key, C_SELSET &SelSet, int what = ALL);
DllExport int get_SelSet(C_SELSET &SelSet, int what = ALL);
DllExport int get_SelSet(C_RB_LIST &ColValues, C_SELSET &SelSet, int what = ALL);
DllExport int get_Key_SelSet(ads_name Ent, long *Key, C_SELSET &SelSet, int What = ALL);

DllExport int get_LinkSet(const TCHAR *Cond, C_LINK_SET &LinkSet, C_SELSET *pInputSS = NULL);

      // ESTRAZIONE DATI
DllExport int entExtract(long KeyValue);

      // AGGIORNAMENTO DATI
DllExport virtual int upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
                               int Reason = UNKNOWN_MOD);
      virtual int upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                           C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
	   virtual int upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,                        
                           int Reason = UNKNOWN_MOD);
	   virtual int upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                           C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
DllExport int upd_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues, int Reason = UNKNOWN_MOD,
                       int CounterToVideo = GS_BAD);
      int upd_data(C_LONG_LIST &KeyList, C_RB_LIST &ColValues, int Reason = UNKNOWN_MOD,
                   int CounterToVideo = GS_BAD);

DllExport int CalcValidData(C_RB_LIST &ColValues, int Op, C_SELSET *GlobalSelSet = NULL); // = MODIFY
      int CheckValue(C_ATTRIB *pAttrib, presbuf NewValue);
      int CheckValues(C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib = NULL);

      // riallineamento da reattori
      virtual long align_for_inserted(C_SELSET &selset, int CounterToVideo = GS_BAD,
                                      int Reason = UNKNOWN_MOD);

DllExport int Backup(BackUpModeEnum Mode = GSCreateBackUp, 
                     int What = ALL, int MsgToVideo = GS_GOOD);
      virtual int UpdGraphData(int change_fas, long *BitForFAS = NULL, 
                               int AttribsDABlocksToUpd = GS_GOOD, int MoveOldVisAttrib = GS_BAD);

      // CANCELLAZIONE DATI
      int Ins_Ent_Into_GsDelete(long Key, C_RB_LIST *param_list = NULL);
DllExport virtual int erase_data(long gs_id, int Reason = UNKNOWN_MOD);
DllExport virtual int erase_data(ads_name ent, int Reason = UNKNOWN_MOD);

      // Per i comandi di AutoCAD con Reattori.
      virtual int insert_data(ads_name ent, long OldKey, long *NewKey = NULL);
      virtual int update_data(ads_name ent);

      DllExport virtual long import_data(C_SELSET &ObjGraph, C_SELSET &objDA, int loadGSent,
                                         long BitForChangeToNewFAS, C_FAS *pNewFas,
                                         C_RB_LIST &ColValues, C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked);
      
      // AGGREGAZIONE E DISAGGREGAZIONE DATI
      virtual int aggr_data(ads_name newSS, ads_name entity, long *gs_id = NULL);
      virtual int disaggr_data(ads_name entity, long *gs_id = NULL);

      // COPIA DI CLASSI
      C_CLASS *CopyToPrj(int NewPrj, const TCHAR *Name, const TCHAR *Descr,
                         const TCHAR *TableRef,
                         const TCHAR *ConnStrUDLFile, C_2STR_LIST &PropList,
                         DataCopyTypeEnum CopyType, C_FAS_LIST *fasListconst,
                         C_GPH_INFO_LIST *pGraphInfoList,
                         C_INT_INT_LIST *pNewGroupList = NULL);

      // funzioni per lettura dati da tabella TEMP e OLD
      DllExport int prepare_data(C_PREPARED_CMD_LIST &CmdList, const TCHAR *what = NULL,
                                 const TCHAR *SQLWhere = NULL);
      DllExport int prepare_data(C_PREPARED_CMD &pCmd, int Type, const TCHAR *what = NULL,
                                 const TCHAR *SQLWhere = NULL);
      DllExport int prepare_data(_CommandPtr &pCmd, int Type, const TCHAR *what = NULL,
                                 const TCHAR *SQLWhere = NULL);
      _RecordsetPtr prepare_data(int Type);

      // funzioni per lettura dati da tabella relazioni TEMP e OLD
DllExport int prepare_reldata_where_member(C_PREPARED_CMD_LIST &CmdList);
DllExport int prepare_reldata_where_member(C_PREPARED_CMD &pCmd, int Type);
      _RecordsetPtr prepare_reldata_where_member(int Type);
DllExport int prepare_reldata_where_key(C_PREPARED_CMD_LIST &CmdList);
DllExport int prepare_reldata_where_key(C_PREPARED_CMD &pCmd, int Type);
      _RecordsetPtr prepare_reldata_where_key(int Type);

	   int get_member(C_PREPARED_CMD &pTempCmd, C_PREPARED_CMD &pOldCmd,
							long gs_id, C_2INT_LONG_LIST *member_list);
DllExport int get_group_list(C_PREPARED_CMD &pTempCmd, C_PREPARED_CMD &pOldCmd,
						            int cls, long gs_id, C_LONG_LIST &groupIDList);

	   int check_graph_compatib(ads_name ent);
      int check_graph_compatib(C_SELSET &SelSet);
      int check_SS_graph_compatib_on_import(C_SELSET &SelSet, int RemoveIncompatib = GS_BAD);


      int find_list_memb_simplex(C_INT_INT_LIST *class_code_list);
      
      // GESTIONE BLOCCHI DI ATTRIBUTI VISIBILI
DllExport int updfromDA(C_RB_LIST &ColValues, ads_name entity,
                        C_SELSET *GlobalSelSet = NULL, int *is_torecalc = NULL);
      int updtoDA(ads_name ent, C_RB_LIST &ColValues, int ToSAVE_SS = GS_BAD);
DllExport int updtoDA(C_SELSET &SelSet, C_RB_LIST &ColValues, int ToSAVE_SS = GS_BAD);
DllExport int SetVisibDA(ads_name ent, int Mode, int CounterToVideo = GS_BAD);
      int SetVisibDA(C_SELSET &SelSet, int Mode, int CounterToVideo = GS_BAD);

      virtual int is_updateable(long gs_id, int *WhyNot = NULL, int set_lock = GS_GOOD,
                                int TryToExtractPartialEnt = GS_BAD);
      virtual int is_updateable(ads_name entity, int *WhyNot = NULL, int set_lock = GS_GOOD,
                                int TryToExtractPartialEnt = GS_BAD);
      virtual int is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot = NULL, 
                                  int set_lock = GS_GOOD, int TryToExtractPartialEnt = GS_BAD);
      virtual int is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot = NULL, 
                                  int set_lock = GS_GOOD);
      virtual int is_NewEntity(long Key);
      virtual int is_NewEntity(ads_name entity);

	   virtual int set_status_link_ent(long gs_id, int NewStatus);

      // MULTIUTENZA
      int set_exclusive_use(long OwnerCode, int *result);
      int set_share_use(void);
      int set_locked_on_extract(long OwnerCode, int *result);
      int set_locked_on_save(long OwnerCode, int *result, long *LastEnt = NULL);
      int set_unlocked(long OwnerCode, int Notify = GS_GOOD, bool UpdateLastEnt = false,
                       bool UpdateLastSaveDate = false);
      int is_inExclusiveUse(long *SessionCode);
                                                
DllExport int mod_fas        (C_FAS *p_fas);
      int mod_id             (C_ID *p_id);
      virtual int mod_conct  (C_CONNECT_LIST *p_conct);
DllExport int get_default_values(C_RB_LIST &ColValues, ads_name ent);
DllExport int get_default_values(C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL);
      int CheckValidFuncOnData(C_ATTRIB_LIST *p_attrib_list);
      int ChangeCalcFuncOnData(C_ATTRIB_LIST *p_attrib_list);
      int CheckMandatoryOnData(C_ATTRIB_LIST *p_attrib_list, bool AssignDefault = FALSE,
                               bool *VisAttribModified = NULL);

      // funzioni per grafica
DllExport virtual int InsertEnt(ads_point start, int check_con, ads_name ent,
                                C_SELSET *pSSOut = NULL, long *gs_id = NULL);
DllExport virtual int InsertEnt(C_SUB *pSub, C_SELSET *pSSOut = NULL, bool Undo = TRUE); // per simulazioni

      int fill_group_class(int cls, long Key, C_ENT_FAMILY_LIST &out,
                             C_PREPARED_CMD_LIST *pCmdList = NULL);

      virtual presbuf get_graphical_data(C_RB_LIST &key_values, TCHAR *what, int len,
                                         int dec, TCHAR *type = NULL);
      virtual presbuf get_graphical_data(C_SELSET &SelSet, TCHAR *what, int len, int dec,
                                         const TCHAR *type = NULL, bool SelSetToClean = true);
 
      int graph_calc(C_RB_LIST &ColValues, C_SELSET *SelSet = NULL);

      // Salvataggio globale (tabelle e disegni)
      int save(void);
      int BeginTransaction(int Mode, bool Sec, C_STR_LIST &TransConnStrList);
      int RollbackTrans(C_STR_LIST &TransConnStrList);
      int CommitTrans(C_STR_LIST &TransConnStrList);

      virtual int setModified(int value);
      virtual int isDataModified(void);

      // salvataggio tabelle
      virtual int save_DBdata(void);
      // salvataggio disegni
      virtual int save_GeomData(void);
DllExport long extract_GraphData(int mode, C_SELSET *pSS = NULL,
                                 TCHAR *SpatialQryName = ADE_SPATIAL_QRY_NAME,
                                 C_STR_LIST *pIdList = NULL,
                                 C_STRING *pRptTemplate = NULL, C_STRING *pRptFile = NULL,
                                 int CounterToVideo = GS_BAD,
                                 C_FAMILY_LIST *pFamilyList = NULL);

      // reindicizzazione
      virtual int reindexTab(bool OnlyTemp = false);
      virtual int RefreshTempLastId(void);
      virtual int RefreshOldLastId(bool SetTempLastIdEqualToOldLastId = true);
      int RefreshSecTempLastId(void);
      int CheckGraphData(int WhatOperation = REINDEX || UNLOCKOBJS || SAVEEXTENTS);
      int DWGsUnlock(int *unlocked = NULL);
      
      // sincronizzazione
      virtual int synchronize(const TCHAR *DeletedObjsDWG = NULL, TCHAR *NextCmd = NULL);

      // Ottimizzazione disegni
DllExport int DWGOptimization(void);

      // segmentazione dinamica
      bool is_DynSegmentationSupported(void);

      int LS_to_GS_VERIFIED(C_LINK_SET &LS, int CounterToVideo = GS_BAD);
      int SS_to_GS_VERIFIED(C_SELSET &SS, int CounterToVideo = GS_BAD);

      virtual int reportHTML(FILE *file, const TCHAR *Path = NULL,
                             bool SynthMode = false);

      // File di supporto per la lista di scelta dei valori degli attributi
      int CopySupportFiles(C_CLASS *pDstCls);
      int DelSupportFiles(void);
};


///////////////////////////////////////////////////////////////////////////

class C_SPAGHETTI : public C_CLASS 
{
   private :
      C_FAS          fas;
      C_GPH_INFO     *pGphInfo; // Informazioni per lettura della grafica
      C_CONNECT_LIST connect_list;

      // COPIA DI CLASSI
      int AlignGraphDataOnCopyClass(C_CLASS *old_cls);

      // Utilizzata per IMPORTAZIONE
      int import_data(C_SELSET &ObjGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, _RecordsetPtr pInsRs,
                      C_CLASS *pObj = NULL, int loadGSent = GS_GOOD,
                      long BitForChangeToNewFAS = GSNoneSetting, C_FAS *pNewFas = NULL);

   public:
      C_SPAGHETTI(); 
      ~C_SPAGHETTI();

      C_FAS *ptr_fas()          { return &fas; }
      C_GPH_INFO *ptr_GphInfo() { return pGphInfo; }

      int copy(C_CLASS *out);

      int is_validfas(int type, C_DBCONNECTION *pConn, C_ATTRIB_LIST *p_attrib_list);
      int is_validinfo();

      // GESTIONE INFO SORGENTI GRAFICHE
      int allocGraphInfo(GraphDataSourceEnum Type);
      void deallocGphInfo(void);

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

      resbuf *to_rb(void);             
      int    from_rb(resbuf *rb);
                                     
      int create(void);

      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
DllExport int InsertEnt(ads_point start, int check_con, ads_name ent,
                        C_SELSET *pSSOut = NULL, long *gs_id = NULL);

      // inserimento dati
      int ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id = NULL, 
                   int visib_block = VISIBLE, int Reason = UNKNOWN_MOD,
                   _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
DllExport int ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
                       long *gs_id = NULL, int visib_block = VISIBLE,
                       int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);

      // aggiornamento dati
      int upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
      				 int Reason = UNKNOWN_MOD);

      int is_updateable(ads_name entity, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_NewEntity(ads_name entity);
      int UpdGraphData(int change_fas, long *BitForFAS = NULL,
                       int AttribsDABlocksToUpd = GS_GOOD, int MoveOldVisAttrib = GS_BAD);

      // Per i comandi di AutoCAD con Reattori.
      int insert_data(ads_name ent, long OldKey, long *NewKey = NULL);
      
       // cancellazione dati
      int erase_data(ads_name ent, int Reason = UNKNOWN_MOD);
      
      int update_data(ads_name entity);

      long import_data(C_SELSET &ObjGraph, C_SELSET &objDA, int loadGSent, 
                       long BitForChangeToNewFAS, C_FAS *pNewFas,
                       C_RB_LIST &ColValues, C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked);

      // riallineamento da reattori
      long align_for_inserted(C_SELSET &selset, int CounterToVideo = GS_BAD,
                              int Reason = UNKNOWN_MOD);

      presbuf get_graphical_data(long code, TCHAR *what, TCHAR *type);

      // sincronizzazione
      int synchronize(const TCHAR *DeletedObjsDWG = NULL, TCHAR *NextCmd = NULL);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};

///////////////////////////////////////////////////////////////////////////

class C_SIMPLEX : public C_CLASS
{
   private :
      C_ATTRIB_LIST    attrib_list;
      C_INFO           info;
      C_FAS            fas;
      C_GPH_INFO       *pGphInfo; // Informazioni per lettura della grafica
      C_CONNECT_LIST   connect_list;

      // Utilizzata da IMPORTAZIONE
      int import_data(C_SELSET &ObjGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, _RecordsetPtr pInsRs,
                      C_CLASS *pObj = NULL, int loadGSent = GS_GOOD,
                      long BitForChangeToNewFAS = GSNoneSetting, C_FAS *pNewFas = NULL);

   public :
      C_SIMPLEX();
     ~C_SIMPLEX();

      int copy(C_CLASS *out);

      C_ATTRIB_LIST    *ptr_attrib_list()  { return &attrib_list; }
      C_INFO           *ptr_info()         { return &info; }
      C_FAS            *ptr_fas()          { return &fas; }
      C_GPH_INFO       *ptr_GphInfo()      { return pGphInfo; }
      C_CONNECT_LIST   *ptr_connect_list() { return &connect_list; }

      // GESTIONE INFO SORGENTI GRAFICHE
      int allocGraphInfo(GraphDataSourceEnum Type);
      void deallocGphInfo(void);

      int load_info(TCHAR *filename);        
      int is_validfas(int type, C_DBCONNECTION *pConn, C_ATTRIB_LIST *p_attrib_list);
      int load_connect_list(TCHAR *filename);

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

      resbuf *to_rb(void);             
      int from_rb(resbuf *rb);

      int create(void);

      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
DllExport int InsertEnt(ads_point start, int check_con, ads_name ent,
                        C_SELSET *pSSOut = NULL, long *gs_id = NULL);

      // interrogazione dati
DllExport int query_data(ads_name entity, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport int query_data(long gs_id, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);

      // inserimento dati
      int ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id = NULL, 
                   int visib_block = VISIBLE, int Reason = UNKNOWN_MOD,
                   _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
DllExport int ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
                       long *gs_id = NULL, int visib_block = VISIBLE,
                       int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
      
      // AGGREGAZIONE E DISAGGREGAZIONE DATI
      int aggr_data(ads_name newSS, ads_name entity, long *gs_id=NULL);
      int disaggr_data(ads_name entity, long *gs_id = NULL);

      // aggiornamento dati
      int upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
                   int Reason = UNKNOWN_MOD);
      int upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
DllExport int upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
                       int Reason = UNKNOWN_MOD);
      int upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);

      // cancellazione dati
DllExport int erase_data(ads_name ent, int Reason = UNKNOWN_MOD);
DllExport int erase_data(long Key, int Reason = UNKNOWN_MOD);

      // Per i comandi di AutoCAD con Reattori.
      int insert_data(ads_name ent, long OldKey, long *NewKey = NULL);
      int update_data(ads_name ent);

      long import_data(C_SELSET &ObjGraph, C_SELSET &objDA, int loadGSent, 
                       long BitForChangeToNewFAS, C_FAS *pNewFas,
                       C_RB_LIST &ColValues, C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked);

      // riallineamento da reattori
      long align_for_inserted(C_SELSET &selset, int CounterToVideo = GS_BAD,
                              int Reason = UNKNOWN_MOD);

      int is_updateable(ads_name entity, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_updateable(long gs_id, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot = NULL, 
                          int set_lock = GS_GOOD, int TryToExtractPartialEnt = GS_BAD);
      int is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot = NULL,
                          int set_lock = GS_GOOD);
      
      int is_NewEntity(long Key);
      int is_NewEntity(ads_name entity);

      // salvataggio tabelle
      int save_DBdata(void);

      // reindicizzazione
      int reindexTab(bool OnlyTemp = false);

      // sincronizzazione
      int synchronize(const TCHAR *DeletedObjsDWG = NULL, TCHAR *NextCmd = NULL);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};

///////////////////////////////////////////////////////////////////////////

class C_GROUP : public C_CLASS
{
   private :
      C_ATTRIB_LIST attrib_list;
      C_INFO        info;
      C_GROUP_LIST  group_list;

      int del_db_GroupRel(void);
      int restore_GroupList(void);

   public :
      C_GROUP();
      ~C_GROUP();

      int copy(C_CLASS *out);
    
      C_ATTRIB_LIST *ptr_attrib_list() { return &attrib_list; }
      C_INFO        *ptr_info()        { return &info; }
      C_GROUP_LIST  *ptr_group_list()  { return &group_list; }

      int load_info(TCHAR *filename);        
      int load_group_list(TCHAR *filename);

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

      resbuf *to_rb(void);             
      int from_rb(resbuf *rb);

      int is_member_class(int cls)
      {
		   if (id.code == cls) return GS_GOOD;
		   if (group_list.search_key(cls) == NULL) return GS_BAD;
		   return GS_GOOD;
      }

      int create(void);

      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
      int create_tab_link();
      int del_tab_link();
DllExport int to_db_relcompl(void); // reso pubblico per Paola De Prati (mod. Fasi per Italcogim)

      //------ Aree di lavoro ------------//
      
      int ins_members_row(long gs_id, C_2INT_LONG_LIST &lista_cls_punt);
      int ins_members_row(long gs_id, C_CLS_PUNT_LIST &lista_cls_punt);
      int check_members(C_CLS_PUNT_LIST *p_cls_punt_list);
      int check_members(C_2INT_LONG_LIST &lista_cls);

      // inserimento dati
DllExport int ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
                       long *gs_id = NULL, int visib_block = VISIBLE,
                       int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);

      // interrogazione dati
DllExport int query_data(long gs_id, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport int query_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues,
                         int CounterToVideo = GS_BAD);

      // aggiornamento dati
DllExport int upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
                       int Reason = UNKNOWN_MOD);
      int upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
      int add_members(long gs_id, C_2INT_LONG_LIST &memberList_to_add);
      int remove_members(long gs_id, C_2INT_LONG_LIST &memberList_to_subtract);

      // cancellazione dati
DllExport int erase_data(long codGroup, int Reason = UNKNOWN_MOD);

      int is_updateable(long gs_id, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_NewEntity(long Key);

      int set_status_link_ent(long gs_id, int NewStatus);

DllExport int InsertEnt(ads_point start, int check_con, ads_name ent,
                        C_SELSET *pSSOut = NULL, long *gs_id = NULL);
      presbuf get_graphical_data(long code, TCHAR *what, TCHAR *type);
      int isDeleted(long gs_id);

      // salvataggio tabelle
      int save_DBdata(void);

      // reindicizzazione
      int reindexTab(bool OnlyTemp = false);

      // sincronizzazione
      int synchronize(const TCHAR *DeletedObjsDWG = NULL, TCHAR *NextCmd = NULL);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};                

///////////////////////////////////////////////////////////////////////////

class C_CGRID : public C_CLASS
{
   private :
      C_ATTRIB_LIST attrib_list;
      C_INFO        info;
      C_GRID        grid;
      
   public :
      C_CGRID();
      ~C_CGRID();

      int copy(C_CLASS *out);

      C_ATTRIB_LIST    *ptr_attrib_list() { return &attrib_list; }
      C_INFO           *ptr_info()        { return &info; }
      C_GRID           *ptr_grid()        { return &grid; }

      int load_info(TCHAR *filename);        
      int load_grid(TCHAR *filename);

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

      int set_last(long value);

      resbuf *to_rb(void);             
      int from_rb(resbuf *rb);

      int create(void);

      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
      int init_db_grid(void);

      // interrogazione dati
      int prepare_data_on_row(C_PREPARED_CMD &Cmd,
                              const TCHAR *what = NULL, const TCHAR *SQLWhere = NULL);
      int query_data(ads_point pt, C_RB_LIST &ColValues,
                     C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport int query_data(ads_name entity, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport int query_data(long gs_id, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);

      // aggiornamento dati
      int is_updateable(long gs_id, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int upd_data(long gs_id, C_RB_LIST &ColValues,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
      int upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
DllExport int upd_data_fromDB(C_DBCONNECTION *pConnSrc, C_STRING &TableRefSrc,
                          C_STRING &AttribXSrc, C_STRING &AttribYSrc,
                          C_STRING *pAttribSrc= NULL, C_STRING *pAttribDst = NULL,
                          int Reason = UNKNOWN_MOD);
      int upd_data(C_RB_LIST &ColValues, ads_point pt1 = NULL, ads_point pt2 = NULL,
                   int Reason = UNKNOWN_MOD, int CounterToVideo = GS_BAD);
DllExport int upd_data_fromGraph(C_SELSET &SelSet, C_STRING &AttribDst,
                             C_STRING *pAttribSrc = NULL, int Reason = UNKNOWN_MOD);
      int upd_data_fromGraph(C_SELSET &SelSet, double *Vector, C_STRING *pAttribSrc = NULL);

      // Visualizzazione dati
DllExport int DisplayBorder(C_FAS *pFAS = NULL, int Mode = PREVIEW);
DllExport int DisplayPts(ads_point pt1 = NULL, ads_point pt2 = NULL, C_STRING *pAttribZ = NULL,
                         C_STRING *pSQLCond = NULL, bool SQLMAPFmt = TRUE,
                         C_FAS *pFAS = NULL, bool Centroid = FALSE, int Mode = PREVIEW);
DllExport int DisplayPts(C_LONG_BTREE *pKeyList, C_STRING *pAttribZ = NULL,
                         C_FAS *pFAS = NULL, bool Centroid = FALSE, int Mode = PREVIEW);
DllExport int DisplayCells(ads_point pt1 = NULL, ads_point pt2 = NULL,
                           C_STRING *pSQLCond = NULL, bool SQLMAPFmt = TRUE,
                           C_FAS *pFAS = NULL, int Mode = PREVIEW);
DllExport int DisplayCells(C_STRING &AttribZ, ads_point pt1 = NULL, ads_point pt2 = NULL,
                           C_STRING *pSQLCond = NULL, bool SQLMAPFmt = TRUE,
                           C_FAS *pFAS = NULL, int Mode = PREVIEW);
DllExport int DisplayCells(C_LONG_BTREE *pKeyList, C_STRING *pAttribZ = NULL,
                           C_FAS *pFAS = NULL, int Mode = PREVIEW);
          int get2DCells(C_LONG_BTREE *pKeyList, AcDbEntityPtrArray &EntitySet);
      // Curve di livello
DllExport int DisplayContours(C_STRING &AttribZ, C_FAS_LIST &FAS_list, 
                              ads_point pt1 = NULL, ads_point pt2 = NULL,
                              int Mode = PREVIEW,
                              bool Join = TRUE, bool Fit = TRUE);
      // Interpolazione spaziale
DllExport int SpatialInterpolationIDW(C_STRING &AttribZ, double Ray = 0.0,
                                      int MinKnownPts = 3, bool Quadratic = false, 
                                      ads_point pt1 = NULL, ads_point pt2 = NULL,
                                      bool Recursive = TRUE,
                                      C_STRING *pBarrierAttrib = NULL);
      int LoadToMemory(C_STRING &AttribZ, double **Vector, long *VectorLen);
      int LoadToMemory(C_STRING &Attrib, short **Vector, long *VectorLen);
      int LoadToMatrix(C_STRING &AttribZ, ads_point pt1, ads_point pt2,
                       C_DBL_MATRIX &Matrix, long *LeftBottomKey = NULL);
      int UpdFromMatrix(ads_point pt1, ads_point pt2,
                        C_DBL_MATRIX &Matrix, C_STRING &Attrib);

      // Spostamento oggetti sulla griglia
      int ObjectOnGrid(ads_name ent, ads_name NewEnt,
                       C_PREPARED_CMD_LIST &CmdList,
                       C_RB_LIST &ColValues, presbuf pZValue);
DllExport int ObjectOnGrid(C_SELSET &SelSet, C_STRING &AttribZ);
DllExport int SetAttribEntity(ads_name ent, C_STRING &DstAttrib,
                              C_PREPARED_CMD_LIST &CmdList,
                              C_RB_LIST &ColValues, presbuf pZValue);
DllExport int SetAttribEntity(C_SELSET &SelSet, C_STRING &DstAttrib, C_STRING &SrcAttrib);

      // Fiumi e crinali dei monti
DllExport int DisplayValleyOrRidge(C_STRING &AttribZ, C_FAS &FAS,
                               ads_point pt1 = NULL, ads_point pt2 = NULL,
                               bool Valley = TRUE, int Mode = PREVIEW);

      // Idrologia
DllExport int SetHydrologyFlow(C_STRING &AttribZ, ads_point pt1, ads_point pt2,
                               C_STRING &ValleyAttrib);
DllExport int SetHydrologyCountUpstreamCells(C_STRING &DownstreamCellAttrib, 
                                         ads_point pt1, ads_point pt2,
                                         C_STRING &CountUpstreamCellsAttrib);
DllExport int GetCatchmentAreaCells(long Key, C_STRING &DownstreamCellAttrib, 
                                C_LINK_SET &ResultLS);
DllExport int GetFlowDownCells(long Key, C_STRING &DownstreamCellAttrib, C_LINK_SET &ResultLS);

      // reindicizzazione
      int reindexTab(bool OnlyTemp = false);

      // salvataggio tabelle
      int save_DBdata(void);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};                                 

///////////////////////////////////////////////////////////////////////////

class C_SUB : public C_CLASS 
{
   private :
      C_ATTRIB_LIST  attrib_list;
      C_FAS          fas;
      C_GPH_INFO     *pGphInfo; // Informazioni per lettura della grafica
      C_INFO         info;
      C_CONNECT_LIST connect_list;

      // INSERIMENTO RECORD IN TABELLA
      int ins_row(C_RB_LIST &ColValues, long *gs_id = NULL,
                  C_CLS_PUNT_LIST *lista_cls_punt = NULL,
                  _RecordsetPtr pRsIns = NULL, C_SELSET *GlobalSelSet = NULL);

   public:
      C_SUB();
      ~C_SUB();

      C_ATTRIB_LIST  *ptr_attrib_list()  { return &attrib_list; }
      C_FAS          *ptr_fas()          { return &fas; }
      C_GPH_INFO *ptr_GphInfo()          { return pGphInfo; }
      C_CONNECT_LIST *ptr_connect_list() { return &connect_list; }
      C_INFO         *ptr_info()         { return &info; }

DllExport int   get_key()         { return id.sub_code; }

      // GESTIONE INFO SORGENTI GRAFICHE
      int allocGraphInfo(GraphDataSourceEnum Type);
      void deallocGphInfo(void);

      int copy(C_CLASS *out);

      int is_validfas(int type, C_DBCONNECTION *pConn, C_ATTRIB_LIST *p_attrib_list);
      int is_subclass() { return GS_GOOD; }
      int load_info(TCHAR *filename);        
      int load_connect_list(TCHAR *filename);
      int is_validconnectlist(void);

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl, int sub);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl, int sub);

DllExport long GetNewEntCode();
      int set_last(long value);

      resbuf *to_rb(void);             
      int from_rb(resbuf *rb);
                                     
      int create(void);

      int mod_conct(C_CONNECT_LIST *p_conct);
      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
DllExport int InsertEnt(ads_point start, int check_con, ads_name ent,
                        C_SELSET *pSSOut = NULL, long *gs_id = NULL);

      // inserimento dati
DllExport int ins_data(ads_name lista_ent, C_RB_LIST &ColValues,
                       long *gs_id = NULL, int visib_block = VISIBLE,
                       int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
DllExport int ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
                       long *gs_id = NULL, int visib_block = VISIBLE,
                       int Reason = UNKNOWN_MOD, _RecordsetPtr pRsIns = NULL, C_BTREE *pPtObjsBTree = NULL);
      // interrogazione dati
DllExport int query_data(ads_name entity, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);
DllExport int query_data(long gs_id, C_RB_LIST &ColValues,
                         C_PREPARED_CMD_LIST *pTempOldCmdList = NULL);

      // aggiornamento dati
      int upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
                   int Reason = UNKNOWN_MOD);
      int upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);
DllExport int upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet = NULL,
      	              int Reason = UNKNOWN_MOD);
      int upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                   C_SELSET *GlobalSelSet = NULL, int Reason = UNKNOWN_MOD);

      // cancellazione dati
DllExport int erase_data(ads_name ent, int Reason = UNKNOWN_MOD);

      // Per i comandi di AutoCAD con Reattori.
      int insert_data(ads_name ent, long OldKey, long *NewKey = NULL);
      int update_data(ads_name Entity);

      // riallineamento da reattori
      long align_for_inserted(C_SELSET &selset, int CounterToVideo = GS_BAD,
                              int Reason = UNKNOWN_MOD);

      int is_updateable(ads_name entity, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_updateable(long gs_id, int *WhyNot = NULL, int set_lock = GS_GOOD,
                        int TryToExtractPartialEnt = GS_BAD);
      int is_updateableSS(ads_name entity, C_SELSET &SelSet, int *WhyNot = NULL, 
                          int set_lock = GS_GOOD);
      int is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot = NULL,
                          int set_lock = GS_GOOD, int TryToExtractPartialEnt = GS_BAD);
      int is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot = NULL, int set_lock = GS_GOOD);

      int is_NewEntity(long Key);
      int is_NewEntity(ads_name entity);

      // salvataggio tabelle
      int save_DBdata(void);

      virtual int setModified(int value);
      virtual int isDataModified(void);

      // reindicizzazione
      int reindexTab(bool OnlyTemp = false);
      int RefreshTempLastId(void);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};
                                 
///////////////////////////////////////////////////////////////////////////

class C_EXTERN :  public C_CLASS
{
   private :
      C_INFO info;     // Utilizzo "last" per ultimo codice entità puntuali
      C_LIST sub_list; // Lista di SUB-CLASS (C_SUB)

   public :
      C_EXTERN();
      ~C_EXTERN();

      C_INFO *ptr_info()      { return &info; }
DllExport C_LIST *ptr_sub_list()  { return &sub_list; }

      int copy(C_CLASS *out);

      int load_info(TCHAR *filename);        

      int ToFile(C_STRING &filename);
      int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections);
      int load(TCHAR *filename, int cl);
      int load(C_PROFILE_SECTION_BTREE &ProfileSections, int cl);

      int set_last(long value);

      resbuf *to_rb(void);             
      int from_rb(resbuf *rb);

      int create(void);
      // Lettura/scrittura struttura classe da file per creazione automatica
      presbuf read_stru   (C_PROFILE_SECTION_BTREE &ProfileSections);
      int     write_stru  (C_STRING &filename, C_DBCONNECTION *pConn = NULL);
      presbuf read_connect(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sub_gs_id = _T("0"));

      int get_file_stru   (C_STRING &filename);

      int del(const TCHAR *Password);
      int from_db(C_RB_LIST &ColValues, GSDataPermissionTypeEnum cod_level);
DllExport int restore_sublist(void);
DllExport int get_DirectInsertableSubs(C_CLS_PUNT_LIST &out);
DllExport int get_DynSegmentationSupportedSubs(C_CLS_PUNT_LIST &out);

      int is_IndipendentSub(int sub);
      int is_validconnectlist(int subclass_code, C_LIST *ptr_sub_list);
DllExport int add_connected_2_sub(int sub, C_INT_LIST &Result);
      C_SUB *getJollyNodalSub(int LinkSub);

DllExport int InsertEnt(C_SUB *pSub, C_SELSET *pSSOut = NULL, bool Undo = TRUE);
      presbuf get_graphical_data(long code, TCHAR *what, TCHAR *type);

      int GphDetach(void);

      // salvataggio tabelle
      int save_DBdata(void);
      // salvataggio disegni
      int save_GeomData(void);

      // reindicizzazione
      int reindexTab(bool OnlyTemp = false);
      int RefreshOldLastId(bool SetTempLastIdEqualToOldLastId = true);
      int RefreshTempLastId(void);

      // sincronizzazione
      int synchronize(const TCHAR *DeletedObjsDWG = NULL, TCHAR *NextCmd = NULL);

      int reportHTML(FILE *file, const TCHAR *Path = NULL, bool SynthMode = false);
};

//////////////////////////////////////////////////////////////////////////
                                           
class C_CLASS_LIST : public C_LIST
{
   private :
      C_NODE *father_prj;  // progetto padre
      
   public :
      C_CLASS_LIST(C_NODE *p_prj = NULL);
      ~C_CLASS_LIST();
                                                                        
      C_NODE *get_father_prj(void);
      int set_father_prj(C_NODE *p_prj);
DllExport C_CLASS *search_key(int cls, int sub = 0);
      int sort_for_extraction(void);
      int copy(C_CLASS_LIST &out);
};


//----------------------------------------------------------------------------//
//    class C_CACHE_ATTRIB_VALUES                                             //
//    Classe usata per memorizzare i valori tipo look-up, domain table        //
//    relativi ad un attributo che in GEOsim vengono implementati da lista    //
//    di valori attributi a 2 colonne (codice-descrizione)                    //
//----------------------------------------------------------------------------//
class C_CACHE_ATTRIB_VALUES : public C_STR
{
   friend class C_CACHE_ATTRIB_VALUES_LIST;

   protected :
      C_2STR_LIST m_ValuesList;
      bool        ListNotExisting;     // vero se non esiste una lista attributi a 
                                       // 2 colonne associata all'attributo
      C_STR_LIST  LispCondList;        // Lista di eventuali condizioni lisp
      bool        LispCondListLoaded;  // vero se la lista è già stata caricata
      int         UpdateableFromDA;    // Flag di aggiornamento.
                                       // -1 = non inizializzato
                                       // 0  = attributo non aggiornabile da blocco DA
                                       // 1  = attributo aggiornabile da blocco DA
                                       // Un attributo NON è aggiornabile da blocco DA se:
                                       // 1) il suo valore è associato ad una lista di  
                                       //    scelta a 2 colonne condizionata o parametrica
                                       // 2) se almeno 1 condizione o un parametro si
                                       //    riferisce ad un attributo visibile
                                       //   (uno potrebbe condizionare la
                                       //    lista di scelta dell'altro)
      C_STR_LIST AttribListDependencies; // Lista degli attributi da cui dipende la lista dei valori
                                         // (es. l'elenco delle vie dipende dal comune)
      bool       AttribListDependenciesLoaded; // vero se la lista è già stata caricata

   private :
      void init_UpdateableFromDA(FILE *f, bool Unicode, C_ATTRIB_LIST *pAttribList,
                                 C_STR_LIST &CondList);
      TCHAR *get_ValueWithoutSuffix(C_2STR *p);

   public :
      DllExport C_CACHE_ATTRIB_VALUES(C_STRING &AttribName);
      DllExport C_CACHE_ATTRIB_VALUES(C_ATTRIB *pAttrib);
      DllExport virtual ~C_CACHE_ATTRIB_VALUES();
      int get_UpdateableFromDA(void);
      C_STR_LIST *ptr_AttribListDependencies(void);
      bool has_parameters(int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(C_RB_LIST &ColValues, int prj, int cls, 
                                  int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(presbuf ColValues, int prj, int cls, 
                                  int sub = 0, int sec = 0);
      TCHAR *get_Value(C_RB_LIST &ColValues, C_STRING &Descr, int prj, int cls,
                       int sub = 0, int sec = 0);
      DllExport bool isDependingOnAttrib(C_STRING &AttribName);
      DllExport bool isListNotExisting(void);
      int init_AttribListDependencies(FILE *f, bool Unicode, C_ATTRIB_LIST *pAttribList,
                                      C_STR_LIST &LispCondList);
};


//----------------------------------------------------------------------------//
//    class C_CACHE_ATTRIB_VALUES_LIST                                        //
//----------------------------------------------------------------------------//
class C_CACHE_ATTRIB_VALUES_LIST : public C_LIST
{
   public :
      DllExport C_CACHE_ATTRIB_VALUES_LIST();
      DllExport virtual ~C_CACHE_ATTRIB_VALUES_LIST();  // chiama ~C_LIST
      bool has_parameters(C_STRING &AttribName, int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, C_RB_LIST &ColValues,
                                  int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, presbuf ColValues,
                                  int prj, int cls, int sub = 0, int sec = 0);
      TCHAR *get_Value(C_STRING &AttribName, C_RB_LIST &ColValues, C_STRING &Descr, 
                       int prj, int cls, int sub = 0, int sec = 0);
};


//----------------------------------------------------------------------------//
//    class C_CACHE_CLS_ATTRIB_VALUES                                         //
//    Classe usata per memorizzare i riferimenti di una classe, sottoclasse
//    o tab. secondaria associati ad una lista di attributi con i valori 
//    tipo look-up, domain table
//----------------------------------------------------------------------------//
class C_CACHE_CLS_ATTRIB_VALUES : public C_NODE
{
   friend class C_CACHE_CLS_ATTRIB_VALUES_LIST;

   protected :
      int m_prj;
      int m_cls;
      int m_sub;
      int m_sec;
      C_CACHE_ATTRIB_VALUES_LIST m_attrib_values_list;

   public :
      DllExport C_CACHE_CLS_ATTRIB_VALUES(int prj, int cls, int sub = 0, int sec = 0);
      DllExport virtual ~C_CACHE_CLS_ATTRIB_VALUES();
      bool has_parameters(C_STRING &AttribName);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, C_RB_LIST &ColValues);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, presbuf ColValues);
      TCHAR *get_Value(C_STRING &AttribName, C_RB_LIST &ColValues, C_STRING &Descr);
      DllExport C_CACHE_ATTRIB_VALUES *get_pCacheAttribValues(C_STRING &AttribName);
      DllExport int remove(C_CACHE_ATTRIB_VALUES *p);
};


//----------------------------------------------------------------------------//
//    class C_CACHE_CLS_ATTRIB_VALUES_LIST                                    //
//----------------------------------------------------------------------------//
class C_CACHE_CLS_ATTRIB_VALUES_LIST : public C_LIST
{
   public :
      DllExport C_CACHE_CLS_ATTRIB_VALUES_LIST();
      DllExport virtual ~C_CACHE_CLS_ATTRIB_VALUES_LIST();  // chiama ~C_LIST
      DllExport C_CACHE_CLS_ATTRIB_VALUES *search(int prj, int cls, int sub = 0, int sec = 0);
      bool has_parameters(C_STRING &AttribName, int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, C_RB_LIST &ColValues,
                                  int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_2STR *get_Descr(C_STRING &AttribName, presbuf ColValues,
                                  int prj, int cls, int sub = 0, int sec = 0);
      TCHAR *get_Value(C_STRING &AttribName, C_RB_LIST &ColValues, C_STRING &Descr, 
                        int prj, int cls, int sub = 0, int sec = 0);
      DllExport C_CACHE_ATTRIB_VALUES *get_pCacheAttribValues(C_STRING &AttribName, int prj, 
                                                              int cls, int sub = 0, int sec = 0);
      DllExport void removeDependingOnAttrib(C_STRING &AttribName, int prj, 
                                             int cls, int sub, int sec);
};


//----------------------------------------------------------------------------//
//   CLASSE PER GESTIRE UN DI SET DI CLASSI                                   //
//----------------------------------------------------------------------------//
class C_CLASS_SET;

class C_CLASS_SET_LIST : public C_LIST
{
   friend class C_LIST;

   public :
   DllExport C_CLASS_SET_LIST();
   DllExport ~C_CLASS_SET_LIST();

   int copy(C_CLASS_SET_LIST &out);
   int from_rb(resbuf *rb);
   resbuf *to_rb(void);

   DllExport int from_db(int prj);
   DllExport int to_db(int prj);

   C_CLASS_SET *get_next_on_tree(void);

   int Insert(C_CLASS_SET *pClsSet, int SetIdParent = 0, int Pos = 1);
   int InsertCls(int SetId, int Cls, int Pos = 1);
   C_CLASS_SET *Search(int SetId);
   C_CLASS_SET *Search(C_STRING &Mask, bool FromStart = true);
   C_CLASS_SET *SearchCls(int Cls);
   C_CLASS_SET *Search(C_INT_INT_LIST &Cls_ClsSet_CodeList, bool FromStart = true);
   int Remove(int SetId);
   int RemoveCls(int Cls);
   int Move(int SetId, int IdParent = 0, int Pos = 1);
   int MoveCls(int Cls, int SetId, int Pos = 1);
   DllExport void get_ClsCodeList(C_INT_LIST &out);
};

class C_CLASS_SET : public C_NODE
{
   friend class C_CLASS_SET_LIST;
   friend class C_NODE;

   public :
   int      Id;                   // codice set di classi (> 0, se == 0 si tratta della root)
   C_STRING Name;                 // nome set di classi
   C_STRING Descr;                // Opzionale; Descrizione set di classi
   C_STRING ImagePath;            // Opzionale; percorso di un file di mmagine del set di classi
   C_CLASS_SET_LIST ClassSetList; // Lista dei set di classi figli
   C_INT_INT_LIST ClsCodeList;    // Lista dei codici delle classi e dei set che formano il set
                                  // Il primo elemento è il codice e il secondo 
                                  // se = GSClassSet significa set di classi, se = GSClass significa classe di entità

   DllExport C_CLASS_SET();
   DllExport virtual ~C_CLASS_SET();

   DllExport int get_key(void);
   DllExport TCHAR *get_name(void);
   DllExport TCHAR *get_Descr(void);
   int copy(C_CLASS_SET &out);

   int Insert(C_CLASS_SET *pClsSet, int Pos = 1);
   int InsertCls(int Cls, int Pos = 1);
   C_CLASS_SET *SearchCls(int Cls);
   C_CLASS_SET *Search(C_INT_INT_LIST &Cls_ClsSet_CodeList);
   int RemoveCls(int Cls);

   void get_ClsCodeList(C_INT_LIST &out, bool Append = false);
   void clear(void);

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int Load(TCHAR *filename,const TCHAR *sez);
   int Load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   DllExport int from_db(C_NODE *pPrj, int SetId = 0);
   DllExport int to_db(C_NODE *pPrj, int SetId = 0, int *pNextSetClassId = NULL);

   resbuf *to_rb(void);
   int from_rb(C_RB_LIST &ColValues);

   int reportHTML(FILE *file, bool SynthMode = false);
};


//-----------------------------------------------------------------------//   
// FUNZIONI PER CHIAMATE LISP DA MODULO ARX  //
// Gestione delle classi di entita' //
//-----------------------------------------------------------------------//   

int gs_save_class_sel(void);
int gs_load_class_sel(void);
                  
int gs_get_id(void);
int gs_get_fas(void);
int gs_get_gph_info(void);
int gs_get_info(void);
int gs_get_grid(void);
int gs_get_connect_list(void);
int gs_get_group_list(void);
int gs_get_attrib_list(void);

int gs_get_class(void);
int gs_createsimplex(void);
int gs_createspaghetti(void);
int gs_creategroup(void);
int gs_creategrid(void);
int gs_createextern(void);
int gs_delclass(void);
int gs_destroyclass(void);
int gs_synchronizeclass(void);
int gs_dwgoptimizationclass(void);
int gs_share_class(void);

int gs_mod_id(void);
int gs_mod_fas(void);
int gs_ddSetLayerModelOnFAS(void);
int gs_mod_coord(void);
int gs_mod_connect_list(void);
int gs_mod_stru(void);
                               
// Gestione dei set di classi di entita' //
int gs_move_classes_to_class_set(void);


DllExport int gsc_createclass(int prj, C_CLASS *pCls);

int gs_is_graph_calc_to_recalc(void);
DllExport int gsc_mod_fas(int prj, int cls, int sub, C_FAS *p_fas, int change_fas = GS_BAD);
int gsc_mod_coord(int prj, int cls, TCHAR *coord);
int gsc_destroyclass(int prj, int cls, const TCHAR *Password);

int gs_dealloc_mod_stru(void);
int gs_alloc_mod_stru(void);
int gs_add_attr(void);    
int gs_del_attr(void);
int gs_mod_attr(void);

int gsc_dealloc_GS_RESBUF(void);
int gsc_alloc_mod_stru(int prj, int cls, int sub, int sec);
int gsc_add_attr(C_ATTRIB_LIST *p_attr_list, int Pos = 0);
int gsc_del_attr(presbuf list_colname);
int gsc_mod_attr(presbuf lista);

C_CLASS *gsc_alloc_new_class(resbuf *arg);
DllExport C_CLASS *gsc_alloc_new_class(int);

DllExport int gsc_validwidth          (double width);
DllExport int gsc_validelevation      (double elevation);
DllExport int gsc_validscale          (double scale);
DllExport int gsc_validrotation       (double rotation);
DllExport int gsc_validhtext          (double h_text);
DllExport int gsc_validWidhtFactorText(double WidhtFactor);
DllExport int gsc_validthickness      (double thickness);
int gsc_validcattype             (int category, int type);
int gsc_valid_attrib_name        (C_STRING &key_attrib, C_DBCONNECTION *pConn, C_ATTRIB_LIST *pattrib_list);
int gsc_is_validconnect          (int conct_code, int cat, int tip);
int gsc_validclassname           (const TCHAR *name);
int gsc_validdirdwg              (TCHAR *dir, int maxlen);
int gsc_validdirdwg              (C_STRING &dir);
int gsc_validgphcalc             (TCHAR **file_graph_calc, TCHAR *const fun_graph_calc,
                                  C_ATTRIB_LIST *p_attrib_list);
                                 
int gsc_NoGraphCharact2attriblist(_RecordsetPtr &pRs, C_ATTRIB_LIST *p_attrib_list);
int gsc_stru_valdef2attriblist   (C_CLASS *pCls, int def);
int gsc_stru_valdef2attriblist   (C_STRING &TableRef, C_DBCONNECTION *pConn,
                                  C_ATTRIB_LIST *p_attrib_list, TCHAR *key_attrib, 
                                  int def);

int gsc_list_from_lsp     (presbuf *rb, presbuf *p);
int gsc_create_tab        (C_STRING &TableRef, C_DBCONNECTION *pConn, C_ATTRIB_LIST *p_attriblist,
                           C_STRING &IndexName, C_STRING &key_attrib, IndexTypeEnum IndexMode = PRIMARY_KEY);
int gsc_default_to_db     (C_DBCONNECTION *pConn, const TCHAR *TableRef,
                           TCHAR *key_attrib, C_ATTRIB_LIST *p_attrib_list_old,
                           C_ATTRIB_LIST *p_attrib_list_new);
int gsc_carattattr_to_db  (C_NODE *pPrj, int code, int sub_code, int sec_code,
                           C_ATTRIB_LIST *p_attrib_list);
int gsc_del_db_carattattr (C_NODE *pPrj, int code, int sub_code, int sec_code);
int gsc_del_db_ClassSet(C_NODE *pPrj, int cls);
int gsc_CheckValidFuncOnData(C_DBCONNECTION *pConn, const TCHAR *TableRef,
                             C_ATTRIB_LIST *p_attrib_list, TCHAR *key_attrib = NULL);
int gsc_ChangeCalcFuncOnData(C_DBCONNECTION *pConn, const TCHAR *TableRef,
                             C_ATTRIB_LIST *p_attrib_list, TCHAR *key_attrib = NULL);
int gsc_CheckMandatoryOnData(C_DBCONNECTION *pConn, const TCHAR *TableRef,
                             C_ATTRIB_LIST *p_attrib_list, bool AssignDefault = FALSE,
                             bool *VisAttribModified = NULL);

int gsc_get_temp_clstablename(TCHAR *dir_area, TCHAR *OldTablePath, TCHAR *cls_name,
                              C_STRING &pathfile);
int gsc_get_temp_clstablename(TCHAR *dir_area, TCHAR *OldTablePath, TCHAR *cls_name,
                              TCHAR *pathfile, int dim_pathfile);

int gsc_get_link_tab_name(C_DBCONNECTION *pClsConn, C_STRING &ClsTableRef, C_STRING &LnkTableRef);
int gs_get_default_values(void);
int gsc_is_from_graph(const TCHAR *stream);

//-----------------------------------------------------------------------//   

extern int        gs_getlisttypeclass  (void);
extern presbuf    gsc_getlisttypeclass (bool ExternalData = false);
extern int        gs_getdefaultFAS     (void);
extern int        gsc_getdefaultFAS    (C_FAS &fas);
extern int        gs_getdefaultattr    (void);
extern int        gsc_getdefaultattr   (C_DBCONNECTION *pConn, int categoria,
                                        int type, presbuf *result_list);
       int        gs_getAttrListFromBlock(void);
       int        gs_getAttrListFromOD(void);
       int        gs_getAttrListFromDB(void);

extern presbuf    gsc_getextclasstypes (void);
DllExport   extern int gsc_rtab_sec         (int prj, int cls, int sub,
                                        C_INT_INT_STR_LIST &lista);
extern int        gs_rtab_sec          (void);
DllExport   extern int gsc_del_dwg (int prj, int cls, TCHAR *dir_dwg);

//-----------------------------------------------------------------------//   
// Funzioni Locali //
//-----------------------------------------------------------------------//   

DllExport C_CLASS *gsc_find_class(int prj, int cls, int sub = 0);

int arg_to_prj_cls_sub(resbuf **arg, int *prj, int *cls, int *sub = NULL);
                   
//-----------------------------------------------------------------------//   
int gs_get_class_ord (void);
int gsc_get_class_ord(int prj,int cls,int sub);
int gs_copy_class    (void);
int gs_what_is_graph_updateable(void);
int gs_is_inarea     (void);
int gsc_is_inarea    (int prj, int cls, long *SessionCode, int *FilterByUser = NULL,
                      long *ExceptionCode = NULL);
int gsc_get_class_lock(int prj, int cls, C_LONG_LIST &LockedBy, int *Mode, int *State);

int gsc_Backup_gs_attrib(BackUpModeEnum Mode, C_PROJECT *pPrj, 
                         int cls, int sub, int sec = -1);

DllExport int gsc_getClassBitmap(int category, int type, C_COLOR &color, bool LargeSize, CBitmap &CBMP);
DllExport int gsc_getClassSet_StandardBitmap(bool LargeSize, CBitmap &CBMP);

// creazione automatica classi
int gs_readAcpfile();
int gs_fromDclToRB();
int gs_AutomaticCreationClass();
int gs_class_write_stru();


#endif
