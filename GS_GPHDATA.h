/*************************************************************************/
/*   GS_DBGPH.H                                                          */
/*************************************************************************/


#ifndef _gs_dbgph_h
#define _gs_dbgph_h 1

#ifndef _gs_def_h
   #include "..\gs_def.h"
#endif

#ifndef _gs_list_h
   #include "gs_list.h"
#endif

#ifndef _gs_class_h
   #include "gs_class.h"
#endif

#ifndef _gs_utily_h
   #include "gs_utily.h"
#endif


#include "dbMPolygon.h"

#define MAX_GRAPH_CONDITIONS 1000

enum gsGphObjArrayTypeEnum
{
   AcDbObjectIdType = 1,
   AcDbEntityType = 2
};

class C_GPH_OBJ_ARRAY
{
   public :

   C_GPH_OBJ_ARRAY();
   virtual ~C_GPH_OBJ_ARRAY();

   // Se ObjType = AcDbObjectIdType si deve usare il vettore di AcDbObjectId (Ids)
   // Se ObjType = AcDbEntityType si deve usare il vettore di AcDbEntity (Ents)
   gsGphObjArrayTypeEnum ObjType;

   bool mReleaseAllAtDistruction; // flag perchè il distruttore rilasci tutti gli oggetti

   C_DWG_LIST DwgList; // Lista dei disegni con gli oggetti letti dai disegni (usato dalla C_DWG_INFO)
   
   AcDbEntityPtrArray Ents; // Vettore di puntatori a AcDbEntity (usato dalla C_DBGPH_INFO)
   C_INT_LONG_LIST    KeyEnts; // Lista di chiavi numeriche associate agli oggetti 
                               // grafici (non alle entità di GEOsim)
                               // presenti in Ents (nello stesso ordine) con un flag
                               // intero che > 0 si riferisce ad oggetti grafici principali
                               //   in particolare se = 1 se l'oggetto grafico è da rilasciare 
                               //   (ad esempio se è stata inserita nel db di acad
                               //   con appendAcDbEntity) in fase di remove_all
                               // se l'intero < 0 si riferisce a blocchi DA
                               //   in particolare se = -1 se l'oggetto grafico è da rilasciare 
                               //   (ad esempio se è stata inserita nel db di acad
                               //   con appendAcDbEntity) in fase di remove_all   
   int  length(void);
   void removeAll(void);
};


//-----------------------------------------------------------------------//
// C_GPH_INFO inizio
//-----------------------------------------------------------------------//
class C_GPH_INFO : public C_NODE
{
   friend class C_DBGPH_INFO;
   friend class C_DWG_INFO;

   public :

   C_GPH_INFO();
   virtual ~C_GPH_INFO();

   int      prj;         // codice progetto
   int      cls;         // codice classe
   int      sub;         // codice sotto-classe
   C_STRING coordinate_system; // Nome del sistema di coordinate
   double UnitCoordConvertionFactorFromClsToWrkSession; // fattore moltiplicativo per convertire le unità delle coordinate
                                                        // degli oggetti del DB a quelli della sessione di lavoro corrente
                                                        // (1 = nessuna correzione, 0 = non inizializzato)

   virtual GraphDataSourceEnum getDataSourceType(void);
   virtual int copy(C_GPH_INFO *out);

   virtual int ToFile(C_STRING &filename, const TCHAR *sez);
   virtual int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   virtual int load(C_STRING &filename, const TCHAR *sez);
   virtual int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   virtual resbuf *to_rb(bool ConvertDrive2nethost = false, bool ToDB = false);
   virtual int from_rb(C_RB_LIST &ColValues);
   virtual int from_rb(resbuf *rb);

   virtual int to_db(void);
   int del_db(bool DelResource = false);

   virtual int isValid(int GeomType);

   virtual int CreateResource(int GeomType = TYPE_SPAGHETTI);
   virtual int RemoveResource(void);
   virtual bool IsTheSameResource(C_GPH_INFO *p);
   virtual bool ResourceExist(bool *pGeomExist = NULL, bool *pLblGroupingExist = NULL, bool *pLblExist = NULL);

   int QueryClear(void);
   int LoadQueryFromCls(TCHAR *qryName, C_STR_LIST &IdList);
   virtual int LoadSpatialQueryFromADEQry(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);
   int AddQueryFromEntityId(long Id);
   int AddQueryFromEntityIds(C_STR_LIST &IdList, bool FromBeginnig = false,
                             int MaxConditions = MAX_GRAPH_CONDITIONS);
   int AddQueryOnlyLabels(void);
   int AddQueryOnlyGraphObjs(void);

   int QueryDefine(TCHAR* joinOp, TCHAR* bgGroups, TCHAR* notOp,
                   TCHAR* condType, presbuf qryCond, TCHAR* endGroups);

   // Cerca gli oggetti e li visualizza e/o ne fa il preview e/o ne fa un report
   virtual long Query(int WhatToDo,
                      C_SELSET *pSelSet = NULL, long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL, // in caso di EXTRACTION e/o PREVIEW
                      C_STRING *pRptTemplate = NULL, C_STRING *pRptFile = NULL, const TCHAR *pRptMode = NULL, // in caso di REPORT
                      int CounterToVideo = GS_BAD);
   virtual int Save(void);
   virtual int editnew(ads_name sel_set);
   virtual bool HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode = false, AcDbVoidPtrArray *pExplodedSet = NULL);
   bool HasCompatibleGeom(ads_name ent, bool TryToExplode = false, C_SELSET *pExplodedSS = NULL);
   bool HasValidGeom(ads_name ent, C_STRING &WhyNotValid);
   bool HasValidGeom(C_SELSET &entSS, C_STRING &WhyNotValid);
   virtual bool HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid);
   virtual bool HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid);
   virtual int Check(int WhatOperation = REINDEX || UNLOCKOBJS || SAVEEXTENTS);
   virtual int Detach(void);

   virtual int reportHTML(FILE *file, bool SynthMode = false);

   virtual int Backup(BackUpModeEnum Mode, int MsgToVideo = GS_GOOD);

   int getLocatorReportTemplate(C_STRING &Template);

   virtual int get_isCoordToConvert(void);
   double get_UnitCoordConvertionFactorFromClsToWrkSession(void);
   virtual C_STRING *get_ClsSRID_converted_to_AutocadSRID(void);
   GSSRIDUnitEnum get_ClsSRID_unit(void);

private:
   int      isCoordToConvert; // Flag che determina se è necessario convertire le coordinate
                              // degli oggetti del DB a quelli della sessione di lavoro corrente
                              // (GS_GOOD = da convertire, GS_BAD = da NON convertire, GS_CAN = non inizializzato)
   GSSRIDUnitEnum ClsSRID_unit; // unità dello SRID della classe

   virtual int ApplyQuery(C_GPH_OBJ_ARRAY &Ids, int CounterToVideo = GS_BAD); // Cerca gli oggetti nei DWG
   virtual int QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet = NULL,
                       long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL,
                       int CounterToVideo = GS_BAD); // Copia gli oggetti nel disegno corrente
   virtual int Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
                      C_STRING &Path, const TCHAR *Mode); // Scrive le informazioni degli oggetti su file testo
   virtual int Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS = GSNoneSetting,
                       C_FAS *pFAS = NULL); // Crea un blocco di anteprima degli oggetti nel disegno corrente
};


//---------------------------------------------------------------------------//
// MEMORIZZA UN B-ALBERO DEI CODICI NUMERICI CHIAVE DELLA TABELLA DB E,      //
// PER CIASCUNO DI QUESTI, UN PUNTATORE A C_BSTR_REAL CHE CONTIENE IL SUO HANDLE //
// LA CLASSE HA COME SCOPO LA RICERCA VELOCE CON CHIAVE ID DEGLI OGGETTI CREATI DA DB //
//---------------------------------------------------------------------------//
class C_BID_HANDLE : public C_BREAL
{
   protected :  
      C_BSTR_REAL *pHandle;

   public :
      DllExport C_BID_HANDLE();
      DllExport C_BID_HANDLE(double in1);
      DllExport virtual ~C_BID_HANDLE();

      DllExport C_BSTR_REAL *get_pHandle(void);
      DllExport int set_pHandle(C_BSTR_REAL *in);
};


class C_BID_HANDLE_BTREE : public C_LONG_BTREE
{
   public :
      DllExport C_BID_HANDLE_BTREE();
      DllExport virtual ~C_BID_HANDLE_BTREE();  // chiama ~C_LONG_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//-----------------------------------------------------------------------//
// C_DBGPH_INFO inizio
//-----------------------------------------------------------------------//
class C_DBGPH_INFO : public C_GPH_INFO
{
   public :

   C_DBCONNECTION *pGeomConn; // Puntatore alla connessione OLE-DB alla geometria

   C_STRING TableRef;         // Riferimento completo alla tabella
   bool     LinkedTable;      // Flag che indica se la tabella TableRef è interamente
                              // gestita da GEOSIM (false) o se è una tabella già esistente
                              // a cui ci si collega (true)

   C_STRING LblGroupingTableRef; // Riferimento completo alla tabella 
                                 // di raggruppamento delle etichette
   bool     LinkedLblGrpTable;   // Flag che indica se la tabella LblGroupingTableRef è interamente
                                 // gestita da GEOSIM (false) o se è una tabella già esistente
                                 // a cui ci si collega (true)

   C_STRING LblTableRef;         // Riferimento completo alla tabella delle etichette
   bool     LinkedLblTable;      // Flag che indica se la tabella LblTableRef è interamente
                                 // gestita da GEOSIM (false) o se è una tabella già esistente
                                 // a cui ci si collega (true)

   C_STRING      ConnStrUDLFile; // Stringa di connessione o File di tipo .UDL
   C_2STR_LIST   UDLProperties;  // Lista di proprietà UDL 

   C_STRING key_attrib;       // Nome del campo chiave per la ricerca 
                              // dell'oggetto grafico della classe
   C_STRING ent_key_attrib;   // Nome del campo contenente il codice 
                              // dell'entita' di appartenenza dell'oggetto grafico

   C_STRING attrib_name_attrib; // Nome del campo contenente il nome
                                // dell'attributo a cui l'etichetta fa riferimento
   C_STRING attrib_invis_attrib; // Nome del campo contenente il flag
                                 // di invisibilità dell'etichetta

   GeomDimensionEnum geom_dim; // dato geometrico 2D o 3D (lo standard OpenGIS prevede 2D)
   C_STRING geom_attrib;      // Nome del campo contenente la geometria dell'oggetto grafico
   // In alternativa (geom_attrib è vuoto), se la geometria è sotto forma di 3 campi numerici
   C_STRING x_attrib;         // Nome del campo contenente la coordinata x dell'oggetto grafico
   C_STRING y_attrib;         // Nome del campo contenente la coordinata y dell'oggetto grafico
   C_STRING z_attrib;         // Nome del campo contenente la coordinata z dell'oggetto grafico

   C_STRING rotation_attrib;  // Nome del campo contenente la rotazione dell'oggetto 
                              // grafico (per blocchi, testi, riempimenti)
   RotationMeasureUnitsEnum rotation_unit; // Unita' di misura della rotazione indicata da rotation_attrib 
   C_STRING vertex_parent_attrib; // Nome del campo contenente un codice per 
                                  // raggruppare i vertici per comporre una linea (polilinee)
                                  // (se le coordinate sono memorizzate con 2 o 3 campi numerici, polilinee)
   C_STRING bulge_attrib;     // Nome del campo contenente il bulge tra un vertice e 
                              // l'altro dell'oggetto grafico lineare della classe 
                              // (se le coordinate sono memorizzate con 2 o 3 campi numerici, polilinee)
   C_STRING aggr_factor_attrib; // Nome del campo contenente il fattore di aggregazione
                                // degli oggetti grafici relativi alla stessa entità
   C_STRING layer_attrib;     // Nome del campo contenente il layer dell'oggetto grafico

   GSFormatColorEnum color_format; // Formato del colore indicato da color_attrib e hatch_color_attrib
   C_STRING color_attrib;          // Nome del campo contenente il codice colore dell'oggetto grafico

   C_STRING line_type_attrib; // Nome del campo contenente il nome del tipo linea dell'oggetto grafico
   C_STRING line_type_scale_attrib; // Nome del campo contenente il fattore di scala del tipolinea dell'oggetto grafico
   C_STRING width_attrib;     // Nome del campo contenente :
                              // la larghezza dell'oggetto grafico se si tratta di linea
                              // il fattore di larghezza se si tratta di testo

   C_STRING text_attrib;      // Nome del campo contenente il testo dell'oggetto grafico
   C_STRING text_style_attrib; // Nome del campo contenente lo stile testo dell'oggetto grafico
   C_STRING h_text_attrib;    // Nome del campo contenente l'altezza testo dell'oggetto grafico
   C_STRING horiz_align_attrib; // Nome del campo contenente il codice acad di allineamento orizzontale dell'oggetto grafico
   C_STRING vert_align_attrib; // Nome del campo contenente il codice acad di allineamento verticale dell'oggetto grafico
   C_STRING oblique_angle_attrib; // Nome del campo contenente l'angolo obliquo del testo dell'oggetto grafico
   
   C_STRING block_attrib;     // Nome del campo contenente il nome del blocco dell'oggetto grafico
   C_STRING block_scale_attrib; // Nome del campo contenente il fattore di scala dell'oggetto grafico

   C_STRING thickness_attrib; // Nome del campo contenente lo spessore verticale dell'oggetto grafico

   C_STRING hatch_attrib;     // Nome del campo contenente il nome del riempimento dell'oggetto grafico
   C_STRING hatch_layer_attrib; // Nome del campo contenente il layer del riempimento dell'oggetto grafico
   C_STRING hatch_color_attrib; // Nome del campo contenente il codice colore del riempimento dell'oggetto grafico
   C_STRING hatch_scale_attrib; // Nome del campo contenente il fattore di scala del riempimento dell'oggetto grafico
   C_STRING hatch_rotation_attrib; // Nome del campo contenente la rotazione del riempimento dell'oggetto grafico

   DllExport C_DBGPH_INFO();
   DllExport ~C_DBGPH_INFO();

   C_DBCONNECTION* getDBConnection(void);
   
   GraphDataSourceEnum getDataSourceType(void);

   TCHAR *get_SqlCondOnTable(void);
   int set_SqlCondOnTable(C_STRING &in);
   TCHAR *get_SqlCondOnLblGrpTable(void);
   int set_SqlCondOnLblGrpTable(C_STRING &in);
   TCHAR *get_SqlCondOnLblTable(void);
   int set_SqlCondOnLblTable(C_STRING &in);

   int copy(C_GPH_INFO *out);

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(C_STRING &filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   resbuf *to_rb(bool ConvertDrive2nethost = false, bool ToDB = false);
   int from_rb(C_RB_LIST &ColValues);
   int from_rb(resbuf *rb);

   int isValid(int GeomType);
   bool LabelWith(void);

   int CreateResource(int GeomType = TYPE_SPAGHETTI);
   int RemoveResource(void);
   bool IsTheSameResource(C_GPH_INFO *p);
   bool ResourceExist(bool *pGeomExist = NULL, bool *pLblGroupingExist = NULL, bool *pLblExist = NULL);

   int LoadSpatialQueryFromADEQry(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);

   int AdjTableParams(void);
   // Cerca gli oggetti e li visualizza e/o ne fa il preview e/o ne fa un report
   long Query(int WhatToDo,
              C_SELSET *pSelSet = NULL, long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL, // in caso di EXTRACTION e/o PREVIEW
              C_STRING *pRptTemplate = NULL, C_STRING *pRptFile = NULL, const TCHAR *pRptMode = NULL, // in caso di REPORT
              int CounterToVideo = GS_BAD);
   int Save(void);
   int editnew(ads_name sel_set);
   bool HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode = false, AcDbVoidPtrArray *pExplodedSet = NULL);
   bool HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid);
   bool HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid);

   int Check(int WhatOperation = REINDEX || UNLOCKOBJS || SAVEEXTENTS);
   int Detach(void);

   int reportHTML(FILE *file, bool SynthMode = false);

   int Backup(BackUpModeEnum Mode, int MsgToVideo = GS_GOOD);

   int CurrentQryAdeToSQL(C_STRING &GeomSQL, C_STRING &LabelSQL);
   int LocationQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr,
                           int *SpatialMode = NULL, AcDbPolyline **pPolygon = NULL);
   int DataQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr);
   int PropertyQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr);

   int AdjustForTableParams(void);
   void TerminateSQL(C_DBCONNECTION *pConnToTerminate = NULL);

   // Questi 4 membri della classe dovrebbero essere privati ma servono a gshatch
   // per gestire i riempimenti per gli spaghetti
   C_STR_REAL_BTREE HandleId_LinkTree;    // Albero con handle autocad+codice oggetto grafico
                                          // indicizzato per Handle
   C_BID_HANDLE_BTREE IndexByID_LinkTree; // Indice dell'albero HandleId_LinkTree
                                          // su codice oggetto grafico
   C_BID_HANDLE_BTREE DAIndexByID_LinkTree; // Indice dell'albero HandleId_LinkTree
                                            // su codice blocco DA
   void AddToLinkTree(const TCHAR *Handle, double GeomKey, int What);
   void DelToLinkTree(const TCHAR *Handle, int What);

   double ToCounterClockwiseDegreeUnit(double Rot);

   int get_isCoordToConvert(void);
   C_STRING *get_CurrWrkSessionSRID_converted_to_DBSRID(void);
   C_STRING *get_ClsSRID_converted_to_AutocadSRID(void);

private :

   C_STRING SqlCondOnTable;   // eventuale condizione SQL da applicare per cercare i dati nella
                              // tabella TableRef nel caso questa non contenga dati esclusivamente
                              // della classe.
   C_2STR_LIST FieldValueListOnTable; // Lista campo-valore (derivata da SqlCondOnTable).
                                      // L'operatore di confronto è "=", quello di relazione è "AND"

   C_STRING SqlCondOnLblGrpTable;// eventuale condizione SQL da applicare per cercare i dati nella
                                 // tabella LblGroupingTableRef nel caso questa non contenga dati esclusivamente
                                 // della classe.
   C_2STR_LIST FieldValueListOnLblGrpTable; // Lista campo-valore (derivata da SqlCondOnLblGrpTable).
                                            // L'operatore di confronto è "=", quello di relazione è "AND"

   C_STRING SqlCondOnLblTable;   // eventuale condizione SQL da applicare per cercare i dati nella
                                 // tabella LblTableRef nel caso questa non contenga dati esclusivamente
                                 // della classe.
   C_2STR_LIST FieldValueListOnLblTable; // Lista campo-valore (derivata da SqlCondOnLblTable).
                                         // L'operatore di confronto è "=", quello di relazione è "AND"

   C_STRING CurrWrkSessionSRID_converted_to_DBSRID; // SRID della sessione di lavoro convertito nello SRID del DB
   C_STRING ClsSRID_converted_to_AutocadSRID; // SRID della classe convertito nello SRID di Autocad

   bool mPlinegen; // sets the polyline so it displays its linetype across vertices if plineGen == Adesk::kTrue.
                   // If plineGen == Adesk::kFalse, the linetype generation starts over at each vertex

   int rb2Color(presbuf pRb, C_COLOR &color);
   int Color2rb(C_COLOR &color, presbuf pRb);
   int Color2SQLValue(C_COLOR &color, C_STRING &SQLValue);

   int CreateGphTable(C_STRING &MyTableRef, bool Label, int GeomType = TYPE_TEXT);
   int InitAppendOnAcDbEntityPtrArray(int GeomType,
                                   C_RB_LIST &ColValues,
                                   presbuf *pKey, presbuf *pEntKey,
                                   presbuf *pAggrFactor,
                                   presbuf *pGeom, // geometria
                                   presbuf *pX, presbuf *pY, presbuf *pZ, // geometria
                                   presbuf *pVertParent, presbuf *pBulge, // geometria
                                   presbuf *pLayer, presbuf *pColor,
                                   presbuf *plineType, presbuf *pLineTypeScale, presbuf *pWidth,
                                   presbuf *pText, presbuf *pTextStyle, presbuf *pTextHeight,
                                   presbuf *pVertAlign, presbuf *pHorizAlign, presbuf *pObliqueAng,
                                   presbuf *pBlockName, presbuf *pBlockScale,
                                   presbuf *pRot, 
                                   presbuf *pThickness,
                                   presbuf *pHatchName, presbuf *pHatchLayer,
                                   presbuf *pHatchColor, presbuf *pHatchScale, presbuf *pHatchRot,
                                   presbuf *pLblAttrName = NULL, presbuf *pLblAttrInvis = NULL);
   int InitAppendOnAcDbEntityPtrArrayTopo(C_RB_LIST &ColValues, presbuf *pInitID, presbuf *pFinalID);

   int AppendOnAcDbEntityPtrArray(_RecordsetPtr &pRs, 
                                  AcDbEntityPtrArray &Ents, 
                                  C_INT_LONG_LIST &KeyEnts, int CounterToVideo = GS_BAD);
   int AppendDABlockOnAcDbEntityPtrArray(_RecordsetPtr &pRs,
                                         AcDbEntityPtrArray &Ents, 
                                         C_INT_LONG_LIST &KeyEnts, int CounterToVideo = GS_BAD);

   int SaveDA(C_SELSET &ClsSS);

   int GetNextKeyValue(C_STRING &_TableRef, long *NextKeyValue);

   AcDbEntity *CreateBlockFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                      TCHAR *pWKTGeom, AcDbObjectId &BlockId,
                                      presbuf pRot = NULL, 
                                      presbuf pScale = NULL,
                                      presbuf pLayer = NULL, presbuf pColor = NULL);
   AcDbEntityPtrArray *CreateBlocksFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                               TCHAR *pWKTGeom, AcDbObjectId &BlockId,
                                               presbuf pRot = NULL, 
                                               presbuf pScale = NULL,
                                               presbuf pLayer = NULL, presbuf pColor = NULL);
   AcDbEntityPtrArray *CreateBlockFromNumericFields(presbuf pEntKey, presbuf pAggrFactor,
                                                    presbuf pX, presbuf pY, AcDbObjectId &BlockId,
                                                    presbuf pZ = NULL, 
                                                    presbuf pRot = NULL, 
                                                    presbuf pScale = NULL,
                                                    presbuf pLayer = NULL, presbuf pColor = NULL);
   AcDbEntityPtrArray *CreateBlocksFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                         C_POINT_LIST &PtList, AcDbObjectId &BlockId,
                                         presbuf pRot = NULL, 
                                         presbuf pScale = NULL,
                                         presbuf pLayer = NULL, presbuf pColor = NULL);
 
   int AppendAttribFromOpenGis(AcDbBlockReference *pEnt, TCHAR *pWKTGeom,
                               AcDbObjectId &StyleId,
                               presbuf pText, presbuf pAttrName,
                               presbuf pRot = NULL, presbuf pWidth = NULL,
                               presbuf pLayer = NULL, presbuf pColor = NULL,                               
                               presbuf pTextHeight = NULL,
                               presbuf pHorizAlign = NULL, presbuf pVertAlign = NULL,
                               presbuf pThickness = NULL, presbuf pObliqueAng = NULL,
                               presbuf pAttrInvis = NULL);
   AcDbEntityPtrArray *CreateTextsFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                              TCHAR *pWKTGeom, 
                                              AcDbObjectId &StyleId, presbuf pText,
                                              presbuf pRot = NULL, presbuf pWidth = NULL,
                                              presbuf pLayer = NULL, presbuf pColor = NULL,
                                              presbuf pTextHeight = NULL,
                                              presbuf pHorizAlign = NULL, presbuf pVertAlign = NULL,
                                              presbuf pThickness = NULL, presbuf pObliqueAng = NULL);
   AcDbEntityPtrArray *CreateTextFromNumericFields(presbuf pEntKey, presbuf pAggrFactor,
                                                   presbuf pX, presbuf pY,  
                                                   AcDbObjectId &StyleId, presbuf pText,
                                                   presbuf pZ = NULL, 
                                                   presbuf pRot = NULL, presbuf pWidth = NULL,
                                                   presbuf pLayer = NULL, presbuf pColor = NULL,
                                                   presbuf pTextHeight = NULL,
                                                   presbuf pHorizAlign = NULL, presbuf pVertAlign = NULL,
                                                   presbuf pThickness = NULL, presbuf pObliqueAng = NULL);
   AcDbEntityPtrArray *CreateTextsFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                         C_POINT_LIST &PtList, 
                                         AcDbObjectId &StyleId, presbuf pText,
                                         presbuf pRot = NULL, presbuf pWidth = NULL,
                                         presbuf pLayer = NULL, presbuf pColor = NULL,
                                         presbuf pTextHeight = NULL,
                                         presbuf pHorizAlign = NULL, presbuf pVertAlign = NULL,
                                         presbuf pThickness = NULL, presbuf pObliqueAng = NULL);

   AcDbEntityPtrArray *CreatePointsFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                               TCHAR *pWKTGeom,
                                               presbuf pLayer = NULL, presbuf pColor = NULL,
                                               presbuf pThickness = NULL);
   AcDbEntityPtrArray *CreatePointsFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                          C_POINT_LIST &PtList, 
                                          presbuf pLayer = NULL, presbuf pColor = NULL,
                                          presbuf pThickness = NULL);

   AcDbEntityPtrArray *CreatePolylinesFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                  TCHAR *pWKTGeom,
                                                  presbuf pLayer, presbuf pColor, 
                                                  presbuf pLineType, presbuf pScale,
                                                  presbuf pWidth, presbuf pThickness,
                                                  presbuf pInitID = NULL, presbuf pFinalID = NULL);
   AcDbEntity *CreatePolylineFromNumericFields(presbuf pEntKey, presbuf pAggrFactor,
                                               presbuf pVertParent, presbuf pX, presbuf pY,
                                               presbuf pBulge,
                                               presbuf pLayer, presbuf pColor, 
                                               presbuf pLineType, presbuf pScale,
                                               presbuf pWidth, presbuf pThickness,
                                               presbuf pInitID = NULL, presbuf pFinalID = NULL);
   AcDbEntity *CreatePolylineFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                    C_POINT_LIST &PtList,
                                    presbuf pLayer, presbuf pColor, 
                                    presbuf pLineType, presbuf pScale,
                                    presbuf pWidth, presbuf pThickness,
                                    presbuf pInitID = NULL, presbuf pFinalID = NULL);

   AcDbEntityPtrArray *CreatePolygonFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                        TCHAR *pWKTGeom,
                                        presbuf pLayer, presbuf pColor, 
                                        presbuf pLineType, presbuf pScale,
                                        presbuf pWidth, presbuf pThickness,
                                        presbuf pHatchName, presbuf pHatchLayer,
                                        presbuf pHatchColor, presbuf pHatchScale,
                                        presbuf pHatchRot);
   double CounterClockwiseRadiantToRotationUnit(double Rot);
   
   void get_AliasFieldName(C_STRING &AliasTableName, C_STRING &FieldName,
                           C_STRING &AliasFieldName, C_STRING *pOperationStr = NULL);

   int internal_get_SQLFieldNameList(C_STRING &TableRef, C_STRING &AliasTabName,
                                     C_STRING &SQLFieldNameList, int objectType);
   int get_SQLFieldNameList(C_STRING &SQLFieldNameList);
   int get_SQLLblFieldNameList(C_STRING &SQLFieldNameList);
   int PrepareCmdCountAggr(_CommandPtr &pCmd);
   int InternalQueryIn(C_GPH_OBJ_ARRAY &Objs,
                       bool Label,
                       _CommandPtr &pGetAggrFactorCmd,
                       C_LONG_INT_BTREE &IDAggrFactor_Tree,
                       AcDbBlockTableRecord *pBlkTableRec,
                       C_SELSET *pSelSet = NULL,
                       long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL);

   int SQLFromEnt(ads_name ent, long GeomKey, int Mode, C_STR_LIST &statement_list);
   int SQLFromAttrib(AcDbAttribute *pEnt, long GeomKey, int Mode, long ent_key, int AggrFactor,
                     C_STRING &statement);
   int SQLFromBlock(AcDbBlockReference *pEnt, long GeomKey,
                    int Mode, C_STRING &statement, bool Label);
   int SQLFromText(AcDbEntity *pEnt, long GeomKey,
                   int Mode, C_STRING &statement);
   int SQLFromPoint(AcDbPoint *pEnt, long GeomKey,
                    int Mode, C_STRING &statement);
   int SQLFromPolyline(AcDbEntity *pEnt, long GeomKey,
                       int Mode, C_STR_LIST &statement_list);

   int SQLFromPolygon(AcDbEntityPtrArray &EntArray, long GeomKey,
                      int Mode, C_STRING &statement);
   int SQLFromPolygon(C_SELSET &ss, long GeomKey, int Mode, C_STR_LIST &statement_list);
   int SQLFromMPolygon(AcDbMPolygon *pEnt, long GeomKey, int Mode, C_STRING &statement);
   int SQLFromHatch(AcDbHatch *pEnt, long GeomKey, int Mode, C_STRING &statement);

   int WKTFromPoint(AcDbEntity *pEnt, C_STRING &WKT);
   
   int WKTFromPolyline(AcDbEntity *pEnt, C_STRING &WKT);
   
   int WKTFromMPolygonLoop(AcDbMPolygon *pMPolygon, int LoopIndex, C_STRING &WKT);
   int WKTFromMPolygon(AcDbMPolygon *pMPolygon, C_STRING &WKT);
   int WKTFromPolygon(AcDbEntityPtrArray &EntArray, C_STRING &WKT);
   int WKTFromPolygon(C_SELSET &ss, C_STRING &WKT);
   int WKTFromPolygon(AcDbEntity *pEnt, C_STRING &WKT);
   int WKTFromPolygon(AcDbHatch *pEnt, C_STRING &WKT);
   int WKTToPolygonLoop(TCHAR **WKT, AcDbCurve **pEnt, 
                        C_EED &eed, const TCHAR *Layer = NULL, 
                        C_COLOR *Color = NULL, const TCHAR *LineType = NULL, 
                        double LineTypeScale = -1, double Width = -1, double Thickness = -1,
                        bool *Plinegen = NULL, int *nRead = NULL);
   int WKTToPolygon(TCHAR *pWKTGeom, AcDbEntityPtrArray &EntArray,
                    C_EED &eed, const TCHAR *Layer = NULL, 
                    C_COLOR *Color = NULL, const TCHAR *LineType = NULL, 
                    double LineTypeScale = -1, double Width = -1, double Thickness = -1,
                    bool *Plinegen = NULL);


   int SQLGetEntKeyFromGeomKey(long GeomKey, C_STRING &statement);

   int ReindexTable(C_STRING &MyTableRef);
   int BackupTable(C_STRING &MyTableRef, BackUpModeEnum Mode, int MsgToVideo = GS_GOOD);

   int ApplyQuery(C_GPH_OBJ_ARRAY &Ids, int CounterToVideo = GS_BAD); // Cerca gli oggetti nei DWG
   int QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet = NULL,
               long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL,
               int CounterToVideo = GS_BAD); // Copia gli oggetti nel disegno corrente
   int Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
              C_STRING &Path, const TCHAR *ModeD); // Scrive le informazioni degli oggetti su file testo
   int Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS = GSNoneSetting,
               C_FAS *pFAS = NULL); // Crea un blocco di anteprima degli oggetti nel disegno corrente

   int get_PrimaryKeyName(C_STRING &_TableRef, C_STRING &PriKeyName);

   int SqlCondToFieldValueList(C_STRING &Cond, C_2STR_LIST &FieldValueList);
};


//-----------------------------------------------------------------------//
// C_DWG_INFO inizio
//-----------------------------------------------------------------------//
class C_DWG_INFO : public C_GPH_INFO
{
   public :

   C_DWG_INFO();
   ~C_DWG_INFO();

   C_STRING   dir_dwg;     // direttorio dei *.DWG (senza alias)
   C_DWG_LIST dwg_list;    // Lista dei disegni

   GraphDataSourceEnum getDataSourceType(void);
   int copy(C_GPH_INFO *out);

   int ToFile(C_STRING &filename, const TCHAR *sez);
   int ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
   int load(C_STRING &filename, const TCHAR *sez);
   int load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);

   resbuf *to_rb(bool ConvertDrive2nethost = false, bool ToDB = false);
   int from_rb(C_RB_LIST &ColValues);
   int from_rb(resbuf *rb);

   int isValid(int GeomType);

   int CreateResource(int GeomType = TYPE_SPAGHETTI);
   int RemoveResource(void);
   bool IsTheSameResource(C_GPH_INFO *p);
   bool ResourceExist(bool *pGeomExist = NULL, bool *pLblGroupingExist = NULL, bool *pLblExist = NULL);

   int LoadSpatialQueryFromADEQry(TCHAR *qryName = ADE_SPATIAL_QRY_NAME);

   // Cerca gli oggetti e li visualizza e/o ne fa il preview e/o ne fa un report
   long Query(int WhatToDo,
              C_SELSET *pSelSet = NULL, long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL, // in caso di EXTRACTION e/o PREVIEW
              C_STRING *pRptTemplate = NULL, C_STRING *pRptFile = NULL, const TCHAR *pRptMode = NULL, // in caso di REPORT
              int CounterToVideo = GS_BAD);
   int Save(void);
   int editnew(ads_name sel_set);
   bool HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode = false, AcDbVoidPtrArray *pExplodedSet = NULL);
   bool HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid);
   bool HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid);
   int Check(int WhatOperation = REINDEX || UNLOCKOBJS || SAVEEXTENTS);
   int Detach(void);

   int reportHTML(FILE *file, bool SynthMode = false);

   int Backup(BackUpModeEnum Mode, int MsgToVideo = GS_GOOD);

   int get_isCoordToConvert(void);
   C_STRING *get_ClsSRID_converted_to_AutocadSRID(void);

private:

   int ApplyQuery(C_GPH_OBJ_ARRAY &Ids, int CounterToVideo = GS_BAD); // Cerca gli oggetti nei DWG
   int QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet = NULL,
               long BitForFAS = GSNoneSetting, C_FAS *pFAS = NULL,
               int CounterToVideo = GS_BAD); // Copia gli oggetti nel disegno corrente
   int Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
              C_STRING &Path, const TCHAR *Mode); // Scrive le informazioni degli oggetti su file testo
   int Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS = GSNoneSetting,
               C_FAS *pFAS = NULL); // Crea un blocco di anteprima degli oggetti nel disegno corrente
};

class C_GPH_INFO_LIST : public C_LIST
{
	public :
		C_GPH_INFO_LIST();
      ~C_GPH_INFO_LIST();

		int copy(C_GPH_INFO_LIST &out);
		int from_rb(resbuf *rb);
      resbuf *to_rb(void);
};

int gsc_lock_on_gs_class_graph_info(C_DBCONNECTION *pConn, C_STRING &ClassesTableRef, int cls,
                                    int sub, _RecordsetPtr &pRs);
int gsc_unlock_on_gs_class_graph_info(C_DBCONNECTION *pConn, _RecordsetPtr &pRs, int Mode = UPDATEABLE);


C_GPH_INFO *gsc_GphDataSrcFromDB(C_NODE *pPrj, int cls, int sub = 0);
C_GPH_INFO *gsc_GphDataSrcFromRb(C_RB_LIST &ColValues);
C_GPH_INFO *gsc_GphDataSrcFromRb(resbuf *rb);

int gs_getDefaultGraphInfo(void);
C_GPH_INFO *gsc_getDefaultGraphInfo(int prj = 0, bool ExternalData = false);

GraphDataSourceEnum gsc_getGraphDataSourceType(resbuf *arg);
GraphDataSourceEnum gsc_getGraphDataSourceType(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez);
C_GPH_INFO *gsc_alloc_GraphInfo(GraphDataSourceEnum Type);

int gs_GetSpatialFromSQL(void);
int gsc_GetSpatialFromSQL(int cod_cls, int cod_sub, const TCHAR *Cond,
                          ads_point corner1, ads_point corner2);

int gsc_test_cond_fas(C_FAS &FAS, long BitForFAS);

int gsc_ExtrOldGraphObjsFrom_SS(int cls, int sub, C_SELSET &ss, C_SELSET &ss_from_dwg);
int gsc_SQLExtract(int cls, int sub, const TCHAR *cond, int mode, 
                   C_STRING *pReportFile = NULL);
int gsc_SQLExtract(int cls, int sub, 
                   C_DBCONNECTION *pConn, const TCHAR *TableRef, const TCHAR *Field,
                   int mode, C_STRING *pReportFile = NULL);
int gsc_extractFromKeyList(int cls, int sub, C_STR_LIST &KeyList, int mode,
                           C_STRING *pReportFile = NULL, int CounterToVideo = GS_BAD);
int gsc_getKeyListFromASISQLonOld(int cls, int sub, const TCHAR *Cond, C_STR_LIST &KeyList);

int gs_check_geoms_for_save(void);
int gs_mod_dbgph_info(void);

#endif