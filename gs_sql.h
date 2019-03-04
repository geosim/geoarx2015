/**********************************************************/
/*   GS_SQL.H
/**********************************************************/


#ifndef _gs_sql_h
#define _gs_sql_h 1

#ifndef _gs_list_h
   #include "gs_list.h"
#endif

#define DEFAULT_CATALOG_SEP     _T('\\')  // separatore di catalogo usato dalla maggioranza
                                          // dei DBMS (ODBC): dBASE, FOX, EXCEL, PARADOX, TEXT
#define DEFAULT_INIT_QUOTED_ID  _T('"')   // delimitatore iniziale di catalogo, schema,
                                          // tabella usato da ODBC
#define DEFAULT_FINAL_QUOTED_ID _T('"')   // delimitatore finale di catalogo, schema,
                                          // tabella usato da ODBC
#define MAX_TABLE_NAME_LEN      8         // Lunghezza nome tabella usato dalla maggioranza
                                          // dei DBMS (ODBC): dBASE 3,dBASE 4, dBASE 5,
                                          // FOX 2, FOX 2.5, FOX 2.6
#define MAX_FIELD_NAME_LEN     10         // Lunghezza nome attributo usato dalla maggioranza
                                          // dei DBMS (ODBC): dBASE 3,dBASE 4, dBASE 5,
                                          // FOX 2, FOX 2.5, FOX 2.6
#define UDL_FILE_REQUIRED_EXT  _T("REQUIRED")


enum CatSchUsageTypeEnum
{
   DML,        // Data Manipulation Language: supportato in tutte le istruzioni di
               // manipolazione dati)
   Definition, // Supportato in tutte le istruzioni di definizione (CREATE...)
   Privilege   // Supportato in tutte le istruzioni per i privilegi (GRANT...)
};

enum ResourceTypeEnum
{
   EmptyRes,
   DirectoryRes,
   FileRes,
   ServerRes,
   WorkbookRes,
   Database
};

enum StrCaseTypeEnum
{
   Upper,      // Maiuscolo
   Lower,      // Minuscolo
   NoSensitive // Non importa se maiuscolo o minuscolo
};

typedef _RecordsetPtr *p_RecordsetPtr;


// Tipo di dato del Provider OLE-DB
//-----------------------------------------------------------------------//   
class C_PROVIDER_TYPE : public C_STR
{
   friend class C_PROVIDER_TYPE_LIST;

   private :
                                    // C_STRING name; derivato da C_STR = Nome del tipo 
                                    // fornito dal provider
      DataTypeEnum Type;            // Tipo di dato
      long         Size;            // La lunghezza definita o lunghezza massima
                                    // del dato NON numerico. Per i dati numerici
                                    // è la massima precisione.
      int          Dec;             // Il numero di decimali supportato, -1 se non si sa.
      C_STRING     LiteralPrefix;   // Carattere usato come prefisso nelle istruzioni SQL
      C_STRING     LiteralSuffix;   // Carattere usato come suffisso nelle istruzioni SQL
      C_STRING     CreateParams;    // Specifiche per la creazione della colonna
                                    // es. DECIMAL richiede "precision,scale" = DECIMAL(10,2)
      short        IsLong;          // Se TRUE Il dato è un BLOB contenente un dato molto grande
      short        IsFixedLength;   // Se RTT è di lunghezza fissa altrimenti, se RTNIL
                                    // non è di lunghezza fissa, RTNONE se non si sa.
      short        IsFixedPrecScale; // Se TRUE il dato ha precisione e scala fissi
      short        IsAutoUniqueValue; // Se TRUE il valore è autoincrementato
    
   public :
      C_PROVIDER_TYPE();
      virtual ~C_PROVIDER_TYPE();

DllExport DataTypeEnum get_Type();
      int  set_Type(DataTypeEnum in);
      long get_Size();
      int  set_Size(long in);
      int  get_Dec();
      int  set_Dec(int in);
      TCHAR *get_LiteralPrefix();
      int  set_LiteralPrefix(const TCHAR *in);
      TCHAR *get_LiteralSuffix();
      int  set_LiteralSuffix(const TCHAR *in);
      TCHAR *get_CreateParams();
      int  set_CreateParams(const TCHAR *in);
      short get_IsLong();
      int   set_IsLong(short in);
      short get_IsFixedLength();
      int   set_IsFixedLength(short in);
      short get_IsFixedPrecScale();
      int   set_IsFixedPrecScale(short in);
      short get_IsAutoUniqueValue();
      int   set_IsAutoUniqueValue(short in);
      presbuf DataValueStr2Rb(TCHAR *Val);

      int copy(C_PROVIDER_TYPE &out);
};


// Lista dei tipi di dato del Provider OLE-DB
//-----------------------------------------------------------------------//   
class C_PROVIDER_TYPE_LIST : public C_LIST
{
   public :
   C_PROVIDER_TYPE_LIST();
   virtual ~C_PROVIDER_TYPE_LIST(); // chiama ~C_LIST

   // search a type
DllExport C_PROVIDER_TYPE *search_Type(DataTypeEnum in, int IsAutoUniqueValue);      // begin from head
   C_PROVIDER_TYPE *search_Next_Type(DataTypeEnum in, int IsAutoUniqueValue); // begin from cursor

   int load(_ConnectionPtr &pConn);
   int copy(C_PROVIDER_TYPE_LIST &out);
   resbuf *to_rb();
   presbuf DataValueStr2Rb(const TCHAR *Type, TCHAR *Val);
};


// Connessione OLE-DB gestita da GEOsim
//-----------------------------------------------------------------------//   
class C_DBCONNECTION: public C_NODE
{
   friend class C_DBCONNECTION_LIST;

   private:
   C_STRING         DBMSName;              // Nome del Data Base Management System (in maiuscolo)
   C_STRING         DBMSVersion;           // Versione del Data Base Management System (in maiuscolo)
   C_STRING         OrigConnectionStr;     // Stringa di connessione originale proposta
                                           // all'apertura del collegamento OLE-DB
   TCHAR            CatalogSeparator;      // Carattere separatore di catalogo
   ResourceTypeEnum CatalogResourceType;   // Tipo di risorsa rappresentata dal catalogo
   C_STRING         CatalogUDLProperty;    // Proprietà UDL che rappresenta il catalogo
                                           // (es. "[Extended Properties]DBQ")
   TCHAR            InitQuotedIdentifier;  // Carattere iniziale per delimitare catalogo,
                                           // schema e tabella (es. per ODBC = ", per ACCESS 97 = [)
   TCHAR            FinalQuotedIdentifier; // Carattere finale per delimitare il catalogo,
                                           // schema e tabella (es. per ODBC = ", per ACCESS 97 = ])
   C_STRING         MultiCharWildcard;     // Parola chiave SQL wildcard per ricerca molti caratteri
                                           // (es. per ACCESS = *, per POSTGRESQL, ORACLE = %)
   C_STRING         SingleCharWildcard;    // Parola chiave SQL wildcard per ricerca singolo carattere
                                           // (es. per ACCESS = ?, per POSTGRESQL, ORACLE = _)
   int              MaxTableNameLen;       // Lunghezza massima del nome della tabella
   int              MaxFieldNameLen;       // Lunghezza massima del campo di una tabella
   StrCaseTypeEnum  StrCaseFullTableRef;   // se il riferimento completo alla tabella 
                                           // e ai campi va indicato in
                                           // maiuscolo (es. per ORACLE),
                                           // minuscolo (es. per PostgreSQL)
                                           // o non ha importanza
   C_STRING         InvalidFieldNameCharacters; // Caratteri non validi per i nomi dei campi di una tabella
   bool             EscapeStringSupported; // se = true i valori stringa che contengono lo '\' devono essere
                                           // raddoppiati in '\\' (default false)
   C_STRING         RowLockSuffix_SQLKeyWord; // Parola chiave SQL da aggiungere in fondo 
                                              // all'istruzione SELECT...per bloccare le righe
                                              // (es. per PostgreSQL "FOR UPDATE NOWAIT" in una transazione)
   C_STRING         DBSample;              // File campione di DB
   short            IsMultiUser;           // se = TRUE il database è multiutente altrimenti
                                           // è monoutente (es. EXCEL)
   short            IsUniqueIndexName;     // se = TRUE il nome dell'indice deve essere
                                           // unico nello stesso schema (tabella e indice
                                           // non possono avere lo stesso nome)
   bool             SchemaViewsSupported;  // se = TRUE questa connessione OLE-DB supporta
                                           // l'opzione adSchemaViews su OpenSchema e quindi la lista
                                           // delle viste nel DB

   C_PROVIDER_TYPE_LIST DataTypeList;      // Lista dei tipi di dato della connessione OLE-DB
   C_PROVIDER_TYPE *pCharProviderType;     // Puntatore a un tipo carattere (usato in Str2SqlSyntax)
   _ConnectionPtr   pConn;                 // Puntatore alla connessione a DB

   // proprietà spaziali
   C_INT_STR_LIST   GeomTypeDescrList;     // Lista di tipi geometrici GEOsim associati
                                           // alla descrizione SQL usata dal provider
                                           // (es. (TYPE_POLYLINE "LINESTRING") in PostGIS)
   C_STRING GeomToWKT_SQLKeyWord;          // Parola chiave SQL per trasformare testo
                                           // in geometria (es. "st_geomfromtext" in PostGIS)
   C_STRING WKTToGeom_SQLKeyWord;          // Parola chiave SQL per trasformare geometria
                                           // in testo (es. "st_asewkt" in PostGIS)
   C_STRING InsideSpatialOperator_SQLKeyWord; // la parola chiave SQL usata come 
                                              // operatore spaziale per cercare 
                                              // gli oggetti totalmente interni
                                              // ad una area (es. " @ " in PostGIS
                                              // e "SDO_CONTAINS" in Oracle)
   C_STRING CrossingSpatialOperator_SQLKeyWord; // la parola chiave SQL usata come 
                                                // operatore spaziale per cercare 
                                                // gli oggetti totalmente interni
                                                // e anche parzialmente esterni 
                                                // ad una area (es. "st_intersects" in PostGIS
                                                // e "SDO_ANYINTERACT" in Oracle)
   C_STRING SRIDList_SQLKeyWord;           // istruzione SQL per avere la lista dei SRID
                                           // (es. "select srid,srtext from public.spatial_ref_sys"
                                           // in PostGIS e "SELECT SRID,CS_NAME FROM MDSYS.SDO_CS_SRS"
                                           // in Oracle)
   C_STRING SRIDFromField_SQLKeyWord;      // istruzione SQL per avere lo SRID di un campo di una tabella
                                           // (es. "select srid,coord_dimension from public.geometry_columns where 
                                           //  f_table_schema='%s' and f_table_name='%s' and f_geometry_column='%s'"
                                           //  in PostGIS e "SELECT SDO_SRID FROM MDSYS.SDO_GEOM_METADATA_TABLE WHERE 
                                           //  SDO_OWNER='%s' AND SDO_TABLE_NAME='%s' AND SDO_COLUMN_NAME='%s'"
                                           //  in Oracle)
   C_STRING SpatialIndexCreation_SQLKeyWord; // parola chiave SQL per la creazione 
                                             // di un indice spaziale da aggiungere
                                             // in fondo alla "CREATE INDEX ..." 
                                             // (es. "USING gist(%s)" in PostGIS, 
                                             // "(%s) INDEXTYPE IS MDSYS.SPATIAL_INDEX" in Oracle) 
   C_STRING IsValidGeom_SQLKeyWord;          // Parola chiave SQL per verificare la correttezza della geometria
                                             // (es. "st_isvalid" in PostGIS e "SDO_GEOM.VALIDATE_GEOMETRY" in Oracle)
   C_STRING IsValidReasonGeom_SQLKeyWord;    // Parola chiave SQL per sapere il motivo per cui la geometria non è valida 
                                             // (es. "st_isvalidreason" in PostGIS)
   C_STRING CoordTransform_SQLKeyWord;     // Parola chiave SQL per trasformare geometria da un SRID 
                                           // all'altro (es. "st_tranform(geom, srid)" in PostGIS o
                                           // "SDO_CS.TRANSFORM(geom, srid)" in Oracle)

   double MaxTolerance2ApproxCurve;       // la distanza massima ammessa tra 
                                          // segmenti retti e la curva (unita autocad)
   int    MaxSegmentNum2ApproxCurve;      // Numero massimo di segmenti retti
                                          // usati per approssimare una curva
   int    SQLMaxVertexNum;                // Numero massimo di vertici ammesso per LINESTRING e POLYGON

   int ReadCatProperty(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadQuotedIdentifiers(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadWildcards(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadMaxTableNameLen(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadMaxFieldNameLen(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadStrCaseFullTableRef(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadInvalidFieldNameCharacters(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadEscapeStringSupported(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadRowLockSuffix_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadIsMultiUser(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadIsUniqueIndexName(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadGeomTypeDescrList(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadGeomToWKT_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadWKTToGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadInsideSpatialOperator_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadCrossingSpatialOperator_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadSRIDList_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadSRIDFromField_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadSpatialIndexCreation_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadMaxTolerance2ApproxCurve(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadMaxSegmentNum2ApproxCurve(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadSQLMaxVertexNum(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadIsValidGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadIsValidReasonGeom_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);
   int ReadCoordTransform_SQLKeyWord(C_PROFILE_SECTION_BTREE &ProfileSections);

   TCHAR *get_FullRefSQL(const TCHAR *Catalog, int CatCheck, const TCHAR *Schema,
                         int SchCheck, const TCHAR *Component, const TCHAR *ComponentParams = NULL);
   int split_FullRefSQL(const TCHAR *FullRefTable, C_STRING &Catalog, int CatCheck,
                        C_STRING &Schema, int SchCheck, C_STRING &Component,
                        bool RudeComponent = false);

   public:
DllExport C_DBCONNECTION(const TCHAR *FileRequired = NULL);
   ~C_DBCONNECTION();

DllExport int Open(const TCHAR *ConnStr, bool Prompt = true, int PrintError = GS_GOOD);
   int UDLProperties_nethost2drive(C_STRING &UDLProperties);
   int UDLProperties_nethost2drive(C_2STR_LIST &PropList);
   int UDLProperties_drive2nethost(C_STRING &UDLProperties);
   int UDLProperties_drive2nethost(C_2STR_LIST &PropList);
   int WriteToUDL(const TCHAR *UDLFile);

DllExport _ConnectionPtr get_Connection();

DllExport const TCHAR *get_OrigConnectionStr();
DllExport const TCHAR *get_DBMSName();
   TCHAR       *getConnectionStrSubstCat(const TCHAR *TableRef);

DllExport short get_CatalogUsage();
   short get_CatalogLocation();
DllExport TCHAR  get_CatalogSeparator();
DllExport ResourceTypeEnum get_CatalogResourceType();
   TCHAR* get_CatalogUDLProperty();

DllExport C_PROVIDER_TYPE_LIST* ptr_DataTypeList();
DllExport C_PROVIDER_TYPE *getCharProviderType(void);
   TCHAR *C_DBCONNECTION::boolToSQL(bool Value);

DllExport short get_SchemaUsage();
   TCHAR  get_SchemaSeparator();

DllExport TCHAR get_InitQuotedIdentifier();
DllExport TCHAR get_FinalQuotedIdentifier();
   const TCHAR  *get_MultiCharWildcard();                                        
   const TCHAR  *get_SingleCharWildcard();

   int   get_MaxTableNameLen();
   int   get_MaxFieldNameLen();

DllExport StrCaseTypeEnum get_StrCaseFullTableRef();
   const TCHAR *get_InvalidFieldNameCharacters();
   bool get_EscapeStringSupported();
   const TCHAR *get_RowLockSuffix_SQLKeyWord();
DllExport short get_IsMultiUser();
DllExport short get_IsUniqueIndexName();

          // spatial
          C_INT_STR_LIST *get_GeomTypeDescrListPtr();
          const TCHAR *get_GeomToWKT_SQLKeyWord();
          int   get_GeomToWKT_SQLSyntax(C_STRING &AliasTabName,
                                        C_STRING &GeomAttribName,
                                        C_STRING &DestSRID,
                                        C_STRING &Result);
          const TCHAR *get_WKTToGeom_SQLKeyWord();
          int get_WKTToGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID,
                                      C_STRING &DestSRID, C_STRING &Result);
          const TCHAR *get_InsideSpatialOperator_SQLKeyWord();
          int get_InsideSpatialOperator_SQLSyntax(C_STRING &Geom1,
                                                  C_STRING &Geom2,
                                                  C_STRING &Result);
          const TCHAR *get_CrossingSpatialOperator_SQLKeyWord();
          int get_CrossingSpatialOperator_SQLSyntax(C_STRING &Geom1,
                                                    C_STRING &Geom2,
                                                    C_STRING &Result);
          const TCHAR *get_SRIDList_SQLKeyWord(void);
DllExport int get_SRIDList(C_2STR_LIST &SRID_descr_list);

          const TCHAR *get_SRIDFromField_SQLKeyWord(void);
          int get_SRIDFromField_SQLSyntax(C_STRING &CatalogSchema, C_STRING &TableName, 
                                          C_STRING &FieldName, C_STRING &Result);
DllExport int get_SRIDFromField(C_STRING &CatalogSchema, C_STRING &TableName, C_STRING &FieldName,
                                C_STRING &SRID, int *NDim = NULL);
          int get_SRIDType(GSSRIDTypeEnum &SRIDType);

          const TCHAR *get_SpatialIndexCreation_SQLKeyWord(void);
          double get_MaxTolerance2ApproxCurve(void);
          int get_MaxSegmentNum2ApproxCurve(void);
          int get_SQLMaxVertexNum(void);

          const TCHAR *get_IsValidGeom_SQLKeyWord();
          int get_IsValidGeom_SQLSyntax(C_STRING &Geom, C_STRING &Result);
          int get_IsValidWKTGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID, C_STRING &DestSRID, C_STRING &Result);
          const TCHAR *get_IsValidReasonGeom_SQLKeyWord();
          int get_IsValidReasonGeom_SQLSyntax(C_STRING &Geom, C_STRING &Result);
          int get_IsValidReasonWKTGeom_SQLSyntax(C_STRING &WKT, C_STRING &SrcSRID, C_STRING &DestSRID, C_STRING &Result);
          const TCHAR *get_CoordTransform_SQLKeyWord();
          int get_CoordTransform_SQLSyntax(C_STRING &Geom, C_STRING &SRID, C_STRING &Result);

   // FUNZIONI GESTIONE RIFERIMENTO A TABELLA
DllExport TCHAR *get_FullRefTable(const TCHAR *Catalog, const TCHAR *Schema, 
                                  const TCHAR *Table, CatSchUsageTypeEnum CatSchUsage = DML,
                                  const TCHAR *TableParams = NULL);
DllExport TCHAR *get_FullRefTable(C_STRING &Catalog, C_STRING &Schema,
                                  C_STRING &Table, CatSchUsageTypeEnum CatSchUsage = DML,
                                  C_STRING *pTableParams = NULL);
DllExport int  split_FullRefTable(const TCHAR *FullRefTable, C_STRING &Catalog,
                                  C_STRING &Schema, C_STRING &Table,
                                  CatSchUsageTypeEnum CatSchUsage = DML,
                                  bool RudeComponent = false);
          int  split_FullRefTable(C_STRING &FullRefTable, C_STRING &Catalog,
                                  C_STRING &Schema, C_STRING &Table,
                                  CatSchUsageTypeEnum CatSchUsage = DML,
                                  bool RudeComponent = false);
   TCHAR *FullRefTable_drive2nethost(const TCHAR *FullRefTable);
DllExport TCHAR *FullRefTable_nethost2drive(const TCHAR *FullRefTable);
   
   int AdjTableParams(C_STRING &FullRefTable);
   int  IsValidTabName(C_STRING &TabName);
   int GetTempTabName(const TCHAR *TableRef, C_STRING &TempTabName);

   int  IsValidFieldName(C_STRING &TabName);

   // FUNZIONI GESTIONE RIFERIMENTO A INDICE
DllExport TCHAR *get_FullRefIndex(const TCHAR *Catalog, const TCHAR *Schema,
                                  const TCHAR *Index, 
                                  CatSchUsageTypeEnum CatSchUsage = DML,
                                  short Reason = NONE);
DllExport TCHAR *getNewFullRefIndex(const TCHAR *Catalog, const TCHAR *Schema,
                                    const TCHAR *Name, const TCHAR *Suffix,
                                    short Reason = NONE);

   // FUNZIONI PER TIPI DI DATO
DllExport TCHAR *Type2ProviderDescr(DataTypeEnum DataType, short IsLong, 
                                    short IsFixedPrecScale, short IsFixedLength,
                                    short IsWrite, long Size, 
                                    int ExactMode = TRUE, int Prec = 0);
DllExport int  ProviderDescr2InfoType(const TCHAR *DataDescr, DataTypeEnum *pDataType = NULL,
                                      long *pDim = NULL, int *pDec = NULL, short *pIsLong = NULL,
                                      short *pIsFixedPrecScale = NULL, short *pIsAutoUniqueValue = NULL,
                                      C_STRING *pCreateParam = NULL, short *pIsFixedLength = NULL);
DllExport int Type2ProviderType(DataTypeEnum DataType, short IsLong, 
                                short IsFixedPrecScale, short IsFixedLength,
                                short IsWrite, long Size, int Prec, C_STRING &ProviderDescr,
                                long *ProviderSize, int *ProviderPrec);
   int Descr2ProviderDescr(const TCHAR *SrcProviderDescr, long SrcSize, int SrcPrec,
                           C_DBCONNECTION &DestConn, C_STRING &DestProviderDescr,
                           long *DestSize, int *DestPrec);
DllExport int IsCompatibleType(const TCHAR *SrcProviderDescr, long SrcSize, int SrcPrec,
                               C_DBCONNECTION &DestConn, const TCHAR *DestProviderDescr,
                               long DestSize, int DestPrec);
   int IsBlob(const TCHAR *DataDescr);
DllExport int IsBoolean(const TCHAR *DataDescr);
DllExport int IsChar(const TCHAR *DataDescr);
   int IsCurrency(const TCHAR *DataDescr);
DllExport int IsDate(const TCHAR *DataDescr);
DllExport int IsTimestamp(const TCHAR *DataDescr);
DllExport int IsTime(const TCHAR *DataDescr);
DllExport int IsNumeric(const TCHAR *DataDescr);
DllExport int IsNumericWithDecimals(const TCHAR *DataDescr);
DllExport presbuf getStruForCreatingTable(presbuf Stru);

   int get_DefValue(const TCHAR *DataDescr, presbuf *Result);

   int IsUsable(const TCHAR *ConnStr);

   // FUNZIONI PER TRANSAZIONI
DllExport int BeginTrans(long *pTransLevel = NULL);
DllExport int CommitTrans(void);
DllExport int RollbackTrans(void);

   // FUNZIONI PER RECORDSET
DllExport int OpenRecSet(C_STRING &statement, _RecordsetPtr &pRs, enum CursorTypeEnum CursorType = adOpenForwardOnly,
                         enum LockTypeEnum LockType = adLockReadOnly,
                         int retest = MORETESTS, int PrintError = GS_GOOD);
DllExport int ExeCmd(C_STRING &statement, _RecordsetPtr &pRs, enum CursorTypeEnum CursorType = adOpenForwardOnly,
                     enum LockTypeEnum LockType = adLockReadOnly, int retest = MORETESTS);

   // FUNZIONI PER COMANDI
   DllExport int ExeCmd(C_STRING &statement, long *RecordsAffected = NULL, 
                        int retest = MORETESTS, int PrintError = GS_GOOD);
   DllExport int PrepareCmd(C_STRING &statement, _CommandPtr &pCmd);

   // FUNZIONI PER TABELLE
DllExport int CreateDB(const TCHAR *Ref, int retest = MORETESTS);
          int CreateSchema(const TCHAR *Schema, int retest = MORETESTS);
          int CreateSequence(const TCHAR *FullRefSequence, int retest = MORETESTS);
          int DelSequence(const TCHAR *FullRefSequence, int retest = MORETESTS);
DllExport int getTableAndViewList(C_STR_LIST &ItemList,
                                  const TCHAR *Catalog, const TCHAR *Schema,
                                  const TCHAR *ItemName = NULL,
                                  bool Tables = true, const TCHAR *TableTypeFilter = NULL,
                                  bool Views = true,
                                  int retest = MORETESTS, int PrintError = GS_GOOD);
DllExport int ExistTable(const TCHAR *FullRefTable, int retest = MORETESTS);
DllExport int ExistTable(C_STRING &FullRefTable, int retest = MORETESTS);
DllExport presbuf ReadStruct(const TCHAR *FullRefTable, int retest = MORETESTS);
DllExport int CreateTable(const TCHAR *TableRef, presbuf DefTab, int retest = MORETESTS);
DllExport int DelTable(const TCHAR *TableRef, int retest = MORETESTS);
DllExport int UpdStruct(const TCHAR *TableRef, presbuf DefStru, presbuf SetValues, int retest = MORETESTS);
DllExport int CopyTable(const TCHAR *SourceTableRef, const TCHAR *DestTableRef,
                        DataCopyTypeEnum Mode = GSStructureAndDataCopyType, int Index = GS_GOOD);
DllExport int CopyTable(const TCHAR *SourceTableRef, C_DBCONNECTION &DestConn, const TCHAR *DestTableRef,
                        DataCopyTypeEnum Mode = GSStructureAndDataCopyType, int Index = GS_GOOD, int CounterToVideo = GS_BAD);
          int getPostGIS_Version(C_STRING &Version);
          int AddGeomField(const TCHAR *TableRef, const TCHAR *FieldName, const TCHAR *SRID,
                           int GeomType, GeomDimensionEnum n_dim, int retest = MORETESTS);
          int AddField(const TCHAR *TableRef, const TCHAR *FieldName, const TCHAR *ProviderDescr,
                       long ProviderLen,  int ProviderDec, int retest = MORETESTS);
          int DropField(const TCHAR *TableRef, const TCHAR *FieldName, int retest = MORETESTS);
          int RenameField(const TCHAR *TableRef, const TCHAR *OldFieldName, const TCHAR *NewFieldName,
                          int retest = MORETESTS);
          int ConvFieldToChar(C_STRING &TableRef, const TCHAR *pField, int FieldLen);
          int ConvFieldToBool(C_STRING &TableRef, const TCHAR *pField);

          int DelView(const TCHAR *FullRefView, int retest = MORETESTS);
          int DelTrigger(const TCHAR *TriggerName, const TCHAR *TableRef, int retest = MORETESTS);
          int DelFunction(const TCHAR *FullRefFunction, int retest = MORETESTS);

   // FUNZIONI PER INSERIMENTO\AGGIORNAMENTO\CANCELLAZIONE\LETTURA RIGHE
DllExport int InitInsRow(const TCHAR *TableRef, _RecordsetPtr &pRs);
   int InitInsRow(const TCHAR *TableRef, _CommandPtr &pCmd);
DllExport int InsRow(const TCHAR *TableRef, C_RB_LIST &ColVal, int retest = MORETESTS,
                     int PrintError = GS_GOOD);
DllExport int DelRows(const TCHAR *TableRef, const TCHAR *WhereSql = NULL, int retest = MORETESTS);
   long RowsQty(const TCHAR *TableRef, const TCHAR *WhereSql = NULL);
DllExport int ReadRows(C_STRING &statement, C_RB_LIST &ColValues, int retest = MORETESTS);

   // FUNZIONI PER INDICI
DllExport int CreateIndex(const TCHAR *indexname, const TCHAR *TableRef, const TCHAR *keyespr,
                          IndexTypeEnum indexmode = INDEX_NOUNIQUE,
                          int retest = MORETESTS, int PrintError = GS_GOOD);
   int CreateIndex(presbuf IndexList, const TCHAR *TableRef, int retest = MORETESTS);
DllExport int getTableIndexes(const TCHAR *FullRefTable, presbuf *pIndexList, 
                              short Reason = NONE, int retest = MORETESTS);
   int getTablePrimaryKey(const TCHAR *FullRefTable, presbuf *pPKDescr,
                          int retest = MORETESTS);

   int DelIndex(C_STRING &TableRef, C_STRING &IndexRef);
   int DelIndex(const TCHAR *TableRef, const TCHAR *IndexRef);
   int DelIndex(const TCHAR *TableRef, presbuf IndexList);
   int Reindex(const TCHAR *TableRef, int retest = MORETESTS);

   int CreatePrimaryKey(const TCHAR *TableRef, const TCHAR *ConstraintName, const TCHAR *KeyEspr,
                        int retest = MORETESTS, int PrintError = GS_GOOD);
   int DelPrimaryKey(const TCHAR *TableRef, const TCHAR *ConstraintName,
                     int retest = MORETESTS, int PrintError = GS_GOOD);

   // ISTRUZIONI SQL DI UTILITA'
DllExport TCHAR *CheckSql(const TCHAR *Statement);
DllExport int  GetNumAggrValue(const TCHAR *TableRef, const TCHAR *FieldName,
                               const TCHAR *AggrFun, ads_real *Value);
   int GetParamNames(C_STRING &Stm, C_STR_LIST &ParamList);
   int SetParamValues(C_STRING &Stm, C_RB_LIST *pColValues = NULL, 
                      UserInteractionModeEnum WINDOW_mode = GSNoInteraction);
   int SetParamValues(C_STRING &Stm, presbuf pColValues = NULL, 
                      UserInteractionModeEnum WINDOW_mode = GSNoInteraction);

   int GetTable_iParamType(C_STRING &TableRef, int i, DataTypeEnum *ParamType);
   int SetTable_iParam(C_STRING &TableRef, int i, C_STRING &ParamValue, DataTypeEnum ParamType);

   int Str2SqlSyntax(C_STRING &Value, C_PROVIDER_TYPE *pProviderType = NULL);
   int Rb2SqlSyntax(presbuf pRb, C_STRING &Value, C_PROVIDER_TYPE *pProviderType = NULL);

   int getConcatenationStrSQLKeyWord(C_STRING &SQLKeyWord);
};


// Lista delle connessioni OLE-DB gestite da GEOSIM
//-----------------------------------------------------------------------//   
class C_DBCONNECTION_LIST : public C_LIST
{
   public :
   C_DBCONNECTION_LIST();
   virtual ~C_DBCONNECTION_LIST(); // chiama ~C_LIST

DllExport C_DBCONNECTION* get_Connection(const TCHAR *ConnStrUDLFile = NULL, 
                                         C_2STR_LIST *pUDLParams = NULL,
                                         bool Prompt = false,
                                         int PrintError = GS_GOOD);
   C_DBCONNECTION* search_connection(_ConnectionPtr &pConn);
   C_DBCONNECTION* search_connection(const TCHAR *OrigConnectionStr);
};


//-----------------------------------------------------------------------//   
// Classe che viene usata per preparare delle strutture usate per
// operazioni su database
// Le strutture usate possono essere di 2 tipi
// 1) Comando preparato (precompilato)
// 2) Recordset aperto in modalità tale da poter effettuare il metodo seek
//    (ricerca veloce)
//-----------------------------------------------------------------------//   
class C_PREPARED_CMD : public C_INT
{
   friend class C_PREPARED_CMD_LIST;

   public :
      _CommandPtr   pCmd;  // Comando preparato
      _RecordsetPtr pRs;   // Recordset aperto in modo "seek"

DllExport C_PREPARED_CMD();
      C_PREPARED_CMD(_ConnectionPtr &pConn, C_STRING &statement);
      C_PREPARED_CMD(_CommandPtr &_pCmd);
      C_PREPARED_CMD(_RecordsetPtr &_pRs);
      void Terminate(void);
DllExport virtual ~C_PREPARED_CMD();
};

// Lista di comandi preparati
//-----------------------------------------------------------------------//   
class C_PREPARED_CMD_LIST : public C_INT_LIST
{
   public :
	   DllExport C_PREPARED_CMD_LIST();
	   DllExport virtual ~C_PREPARED_CMD_LIST(); // chiama ~C_LIST
};


// GESTIONE CONNESSIONE SQL
int gsc_DBOpenConnFromUDL(const TCHAR *UdlFile, _ConnectionPtr &pConn, 
                          TCHAR *username, TCHAR *password, int PrintError = GS_GOOD);
int gsc_DBOpenConnFromStr(const TCHAR *ConnStr, _ConnectionPtr &pConn, 
                          TCHAR *username, TCHAR *password, 
                          bool Prompt = true, int PrintError = GS_GOOD);
DllExport int gsc_DBCloseConn(_ConnectionPtr &pConn);
DllExport int gsc_getUDLProperties(presbuf *arg, C_2STR_LIST &PropList);
DllExport TCHAR *gsc_PropListToConnStr(C_2STR_LIST &PropList);
int gs_PropListToConnStr(void);
DllExport int gsc_PropListFromConnStr(const TCHAR *ConnStr, C_2STR_LIST &PropList);
int gs_PropListFromConnStr(void);
DllExport int gsc_ModUDLProperties(C_2STR_LIST &UDLProperties, C_2STR_LIST &NewUDLProperties);
DllExport presbuf gsc_getUDLPropertiesDescrFromFile(const TCHAR *File, 
                                                    bool UdlFile = true);
int gsc_PropListFromExtendedPropConnStr(const TCHAR *ExtendedPropConnStr,
                                        C_2STR_LIST &ExtendedPropList);
TCHAR *gsc_ExtendedPropListToConnStr(C_2STR_LIST &ExtendedPropList);
DllExport int gsc_UDLProperties_nethost2drive(const TCHAR *UDLFile, C_STRING &UDLProperties);
DllExport int gsc_UDLProperties_nethost2drive(const TCHAR *UDLFile, C_2STR_LIST &PropList);
DllExport int gsc_UDLProperties_drive2nethost(const TCHAR *UDLFile, C_STRING &UDLProperties);
DllExport int gsc_UDLProperties_drive2nethost(const TCHAR *UDLFile, C_2STR_LIST &PropList);
ResourceTypeEnum gsc_DBstr2CatResType(TCHAR *str);
int gsc_getUDLFullPath(C_STRING &UDLFile);
DllExport int gsc_AdjSyntax(C_STRING &Item, const TCHAR InitQuotedId, const TCHAR FinalQuotedId,
                            StrCaseTypeEnum StrCase = Upper);
int gsc_AdjSyntax(C_STR_LIST &ItemList, C_DBCONNECTION *pConn);
int gs_IsValidFieldName(void);

// GESTIONE COMANDI SQL
int gsc_PrepareCmd(_ConnectionPtr &pConn, C_STRING &statement, _CommandPtr &pCmd);
int gsc_PrepareCmd(_ConnectionPtr &pConn, const TCHAR *statement, _CommandPtr &pCmd);
int gsc_ExeCmd(_ConnectionPtr &pConn, C_STRING &statement, long *RecordsAffected = NULL,
               int retest = MORETESTS, int PrintError = GS_GOOD);
int gsc_ExeCmd(_ConnectionPtr &pConn, const TCHAR *statement, long *RecordsAffected = NULL,
               int retest = MORETESTS, int PrintError = GS_GOOD);
int gsc_ExeCmd(_CommandPtr &pCmd, long *RecordsAffected = NULL, int retest = MORETESTS,
               int PrintError = GS_GOOD);
int gsc_ExeCmd(_ConnectionPtr &pConn, C_STRING &statement, _RecordsetPtr &pRs,
               enum CursorTypeEnum CursorType = adOpenForwardOnly,
               enum LockTypeEnum LockType = adLockReadOnly, int retest = MORETESTS);
int gsc_ExeCmd(_ConnectionPtr &pConn, const TCHAR *statement, _RecordsetPtr &pRs,
               enum CursorTypeEnum CursorType = adOpenForwardOnly,
               enum LockTypeEnum LockType = adLockReadOnly, int retest = MORETESTS);
DllExport int gsc_ExeCmd(_CommandPtr &pCmd, _RecordsetPtr &pRs, CursorTypeEnum CursorType = adOpenForwardOnly,
                         LockTypeEnum LockType = adLockReadOnly, int retest = MORETESTS);
int gsc_ExeCmdFromFile(C_STRING &SQLFile);

// GESTIONE RECORDSET SQL
DllExport int gsc_DBSetIndexRs(_RecordsetPtr &pRs, const TCHAR* IndexName, 
                               int PrintError = GS_GOOD);
DllExport int gsc_DBSeekRs(_RecordsetPtr &pRs, const _variant_t &KeyValues, 
                           enum SeekEnum SeekOption = adSeekFirstEQ);
DllExport int gsc_DBCloseRs(_RecordsetPtr &pRs);
DllExport int gsc_DBIsClosedRs(_RecordsetPtr &pRs);
int gsc_DBOpenRs(_ConnectionPtr &pConn, const TCHAR *statement, _RecordsetPtr &pRs,
                 enum CursorTypeEnum CursorType = adOpenForwardOnly,
                 enum LockTypeEnum LockType = adLockReadOnly, long Options = adCmdText,
                 int retest = MORETESTS, int PrintError = GS_GOOD);
DllExport int gsc_RowsQty(_RecordsetPtr &pRs, long *qty);
          int gsc_DBLockRs(_RecordsetPtr &pRs);
          int gsc_DBUnLockRs(_RecordsetPtr &pRs, int mode = UPDATEABLE, bool CloseRs = false);
int gsc_OpenSchema(_ConnectionPtr &pConn, SchemaEnum Schema, _RecordsetPtr &pRs,
                   const _variant_t &Restrictions = vtMissing,
                   int retest = MORETESTS, int PrintError = GS_GOOD);
// LETTURA RIGHE
DllExport int gsc_InitDBReadRow(_RecordsetPtr &pRs, C_RB_LIST &ColValues);
DllExport int gsc_DBReadRow(_RecordsetPtr &pRs, C_RB_LIST &Values);
int gsc_DBReadRows(_RecordsetPtr &pRs, C_RB_LIST &ColValues);
DllExport int gsc_isEOF(_RecordsetPtr &pRs);
int gsc_isBOF(_RecordsetPtr &pRs);
DllExport int gsc_Skip(_RecordsetPtr &pRs, long posiz = 1);
int gsc_MoveFirst(_RecordsetPtr &pRs);
int gsc_GetNumAggrValue(_ConnectionPtr &pConn, const TCHAR *TableRef, const TCHAR *FieldName,
                        const TCHAR *AggrFun, ads_real *Value);

// INSERIMENTO RIGHE
int gsc_DBInsRow(_CommandPtr &pCmd, C_RB_LIST &ColVal, int retest = MORETESTS, 
                 int PrintError = GS_GOOD);
int gsc_InitDBInsRow(_ConnectionPtr &pConn, const TCHAR *TableRef, _RecordsetPtr &pRs);
DllExport int gsc_DBInsRow(_RecordsetPtr &pRs, C_RB_LIST &ColVal, int retest = MORETESTS,
                           int PrintError = GS_GOOD); // più veloce
DllExport int gsc_DBInsRow(_RecordsetPtr &pRs, presbuf ColVal, int retest = MORETESTS, 
                           int PrintError = GS_GOOD);
int gsc_DBInsRow(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs, int retest = MORETESTS,
                 int PrintError = GS_GOOD); // più veloce
DllExport int gsc_DBInsRowSet(_RecordsetPtr &pDestRs, _RecordsetPtr &pSourceRs,
                              int retest = MORETESTS, int PrintError = GS_GOOD, 
                              int CounterToVideo = GS_BAD); // più veloce
DllExport int gsc_DBInsRowSet(_CommandPtr &pDestCmd, _RecordsetPtr &pSourceRs,
                              int retest = MORETESTS, int PrintError = GS_GOOD, 
                              int CounterToVideo = GS_BAD); // più veloce

// AGGIORNAMENTO RIGHE
DllExport int gsc_DBUpdRow(_RecordsetPtr &pRs, C_RB_LIST &ColVal, int retest = MORETESTS,
                           int PrintError = GS_GOOD);

// CANCELLAZIONE RIGHE
DllExport int gsc_DBDelRow(_RecordsetPtr &pRs, int retest = MORETESTS);
DllExport int gsc_DBDelRows(_RecordsetPtr &pRs, int retest = MORETESTS);
DllExport int gsc_DBDelRows(_ConnectionPtr &pConn, const TCHAR *TableRef, 
                            const TCHAR *WhereSql = NULL, int retest = MORETESTS);
int gsc_GetBLOB(FieldPtr &pFld, TCHAR **pValue, long *Size);
int gsc_SetBLOB(void *pValue, long Size, FieldPtr &pFld);

// GESTIONE PARAMETRI SQL
DllExport int gsc_CreateDBParameter(_ParameterPtr &pParam, const TCHAR *Name,
                                    enum DataTypeEnum Type,
                                    long Size = 0,
                                    enum ParameterDirectionEnum Direction = adParamInput);
int gsc_GetParamCount(_CommandPtr &pCmd, bool Refresh = FALSE);
int gsc_GetDBParam(_ConnectionPtr &pConn, _CommandPtr &pCmd);
DllExport int gsc_SetDBParam(_CommandPtr &pCmd, presbuf ColVal);
DllExport int gsc_SetDBParam(_CommandPtr &pCmd, int Index, presbuf Value);
DllExport int gsc_SetDBParam(_CommandPtr &pCmd, int Index, const TCHAR *Str);
DllExport int gsc_SetDBParam(_CommandPtr &pCmd, int Index, long Num);
DllExport int gsc_SetDBParam(_CommandPtr &pCmd, int Index, int Num);
int gsc_ReadDBParams(_CommandPtr &pCmd, UserInteractionModeEnum WINDOW_mode = GSTextInteraction);

// ISTRUZIONI SQL DI UTILITA' INDICI
int gsc_getTableIndexes(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
                        const TCHAR *Schema, presbuf *pIndexList, int retest = MORETESTS);
int gsc_Reindex(_ConnectionPtr &pConn, const TCHAR *TableRef, int retest = MORETESTS);
int gsc_getTablePrimaryKey(_ConnectionPtr &pConn, const TCHAR *Table, const TCHAR *Catalog,
                           const TCHAR *Schema, presbuf *pPrimaryKey, int retest = MORETESTS);

// ISTRUZIONI SQL DI UTILITA' TABELLE
int gsc_CreateTable(_ConnectionPtr &pConn, const TCHAR *TableRef, presbuf deftab, 
                    int retest = MORETESTS);
int gsc_DelTable(_ConnectionPtr &pConn, const TCHAR *TableRef, int retest = MORETESTS);
DllExport presbuf gsc_ReadStruct(_ConnectionPtr &pConn, const TCHAR *Table, 
                                 const TCHAR *Catalog, const TCHAR *Schema, 
                                 int retest = MORETESTS);

// ISTRUZIONI SQL DI UTILITA'
TCHAR *gsc_DBCheckSql(_ConnectionPtr &pConn, const TCHAR *Statement);
int gsc_isUdpateableField(_RecordsetPtr &pRs, const TCHAR *FieldName);

// GESTIONE MESSAGGI DI ERRORE
presbuf gsc_get_DBErrs(_com_error &e);
presbuf gsc_get_DBErrs(_ConnectionPtr &pConn);
presbuf gsc_get_DBErrs(_com_error &e, _ConnectionPtr &pConn);
void printDBErrs(_com_error &e, _ConnectionPtr &pConn);
void printDBErrs(_com_error &e);
void printDBErrs(_ConnectionPtr &pConn);

// FUNZIONI GENERICHE
DllExport C_DBCONNECTION *gsc_getConnectionFromLisp(presbuf arg, C_STRING *Table = NULL);
C_DBCONNECTION *gsc_getConnectionFromRb(C_RB_LIST &ColValues, C_STRING *FullRefTable);
int gsc_RefTable2Path(const TCHAR *DBMSName, C_STRING &Catalog, C_STRING &Schema,
                      C_STRING &Table, C_STRING &Path);
int gs_CreateACCDB(void);
DllExport int gsc_CreateACCDB(const TCHAR *MDBFile);
int gsc_ConvMDBToACCDB(C_STRING &MDBFile);

// FUNZIONI PER TIPI DI DATO
int gs_DBIsBlob(void);
DllExport int gsc_DBIsBlob(DataTypeEnum DataType);
int gs_DBIsBoolean(void);
DllExport int gsc_DBIsBoolean(DataTypeEnum DataType);
int gs_DBIsChar(void);
DllExport int gsc_DBIsChar(DataTypeEnum DataType);
int gs_DBIsCurrency(void);
DllExport int gsc_DBIsCurrency(DataTypeEnum DataType);
int gs_DBIsDate(void);
DllExport int gsc_DBIsDate(DataTypeEnum DataType);
int gs_DBIsTimestamp(void);
DllExport int gsc_DBIsTimestamp(DataTypeEnum DataType);
int gs_DBIsTime(void);
DllExport int gsc_DBIsTime(DataTypeEnum DataType);
int gs_DBIsNumeric(void);
DllExport int gsc_DBIsNumeric(DataTypeEnum DataType);
int gs_DBIsNumericWithDecimals(void);
int gsc_DBIsNumericWithDecimals(DataTypeEnum DataType);

// FUNZIONI PER SELEZIONE VALORE DA LISTA VALORI UNIVOCI VIA DCL  INIZIO //
DllExport int gsc_dd_sel_uniqueVal(int prj, int cls, int sub, int sec, C_STRING &AttribName,
                                   C_STRING &result, C_STRING &Title);
int gsc_dd_sel_uniqueVal(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &AttribName,
                         C_STRING &result, C_STRING &Title);
int gsc_dd_sel_uniqueVal(C_STRING &Title, _RecordsetPtr &pRs, C_STRING &result);
int gsc_dd_sel_uniqueVal(C_STRING &Title, C_2STR_LIST &StrValList, int n_col, C_STRING &result);

// ISTRUZIONI PER IMPORT\EXPORT
int gsc_DBGetFormats(_RecordsetPtr &pRs, int modo, TCHAR *delimiter, C_STR_LIST &Formats);
int gsc_DBsprintf(FILE *fhandle, C_RB_LIST &ColValues, C_STR_LIST &Formats);
int gsc_DBsprintf(FILE *fhandle, presbuf ColValues, C_STR_LIST &Formats);
int gsc_DBsprintfHeader(FILE *fhandle, _RecordsetPtr &pRs, C_STR_LIST &Formats);

int gs_existtable(void);
int gs_readstruct(void);
int gs_exesql(void);
int gs_getUDLPropertiesDescrFromFile(void);
int gs_getDBConnection(void);
int gs_readrows(void);
int gs_getCatSchInfoFromFile(void);
int gs_getUDLList(void);
DllExport int gsc_getUDLList(C_STR_LIST &ResList);
int gs_getProviderTypeList(void);
int gs_Descr2ProviderDescr(void);
int gs_SplitFullRefTable(void);
int gs_getFullRefTable(void);
int gs_IsCompatibleType(void);
int gs_getSRIDList(void);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


#endif
