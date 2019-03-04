/**********************************************************
Name: GS_WHIP 

Module description: 
            
Author: Paolo Carlio, Roberto Poltini

(c) Copyright 1998-2016 by IREN ACQUA GAS S.p.A.

              
Modification history:
              
Notes and restrictions on use: 


**********************************************************/

 
/*********************************************************/
/* INCLUDES */
/*********************************************************/

#include "stdafx.h"

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "adslib.h"   
  
#include "scene.h"        // per funzionalità topologiche (definizione di POLYGON)
#include "topoads.h"      // per funzionalità topologiche
#include "mapads.h"       // per funzionalità map-topologiche
#include "core_rxmfcapi.h" // per AcColorSettings

#include "MapODRecord.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_opcod.h"

#include "gs_resbf.h"     // gestione resbuf
#include "gs_utily.h" 
#include "gs_init.h" 
#include "gs_prjct.h" 
#include "gs_graph.h" 
#include "gs_area.h" 
#include "gs_attbl.h"     // per gestione blocchi attributi
#include "gs_filtr.h" 

#include "gs_dbref.h"     // funzioni per tabelle OD
#include "gs_netw.h" 
#include "gs_ade.h" 
#include "gs_whip.h"


#define QGIS_XML_INDENT _T("    ")


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/

int gsc_clean_sinott(presbuf maschera, TCHAR *topo_var, int mode, presbuf AnchorMask);

int gsc_whip(C_STRING &sinottico, C_STRING &dwf_dir, C_STRING &addr_inet, 
             C_STRING &text_descr, C_STRING &prefix, int flag_script, int flag_dwf, 
			    int flag_dwg, int flag_url, int flag_descr, int flag_pre);

int gsc_pgviewAddAlfaNumAttrib(C_CLASS *pCls, C_2STR_LIST &Name_Alias_list,
                               C_STRING &Stm);
int gsc_pgviewAddGeomAttrib(C_CLASS *pCls, C_RB_LIST &GeomTableStru, C_STRING &Stm,
                            bool Lbl = false);

int gsc_pgviewGetAuxiliaryInfoOnView(C_CLASS *pCls, TCHAR *ViewType, 
                                     C_2STR_LIST *pName_Alias_list = NULL,
                                     C_STRING *pViewName = NULL, C_STRING *pFullRefView = NULL,
                                     TCHAR *RuleType = NULL, C_STRING *pRuleName = NULL, C_STRING *pFullRefFuncName = NULL,
                                     C_STRING *pFullRefFunctionRefreshEntity = NULL);
int gsc_pgviewGetLockTableStm(C_CLASS *pCls, C_STRING &Stm,
                              bool EntityTable = true, bool GeomTable = true,
                              bool LabelTable = true, bool GrpLabelTable = true);
int gsc_pgviewGetUpdEntityTableStm(C_CLASS *pCls, C_2STR_LIST *pName_Alias_list,
                                   TCHAR *Entity_id_param, C_STRING &Stm);

int gsc_pgviewAddRulesOnGeomView(C_CLASS *pCls);

void gsc_pgviewGetFunctionNameWithParamOnInsertGeomView(C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnInsertGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity,
                                          C_2STR_LIST *pName_Alias_list = NULL);

void gsc_pgviewGetFunctionNameWithParamOnUpdateGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnUpdateGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity,
                                          C_2STR_LIST *pName_Alias_list = NULL);

void gsc_pgviewGetFunctionNameWithParamOnDeleteGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                        C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnDeleteGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity);

int gsc_pgviewAddRulesOnLblView(C_CLASS *pCls);

void gsc_pgviewGetFunctionNameWithParamOnInsertLblView(C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnInsertLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                         C_STRING &FullRefView, 
                                         C_STRING &FullRefFunctionRefreshEntity,
                                         C_2STR_LIST *pName_Alias_list);

void gsc_pgviewGetFunctionNameWithParamOnUpdateLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                       C_STRING &FullRefView,
                                                       C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnUpdateLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                         C_STRING &FullRefView, 
                                         C_STRING &FullRefFunctionRefreshEntity,
                                         C_2STR_LIST *pName_Alias_list);

void gsc_pgviewGetFunctionNameWithParamOnDeleteLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                       C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionOnDeleteLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                         C_STRING &FullRefView, 
                                         C_STRING &FullRefFunctionRefreshEntity);

int gsc_ADOTypeToPLPGSQLType(DataTypeEnum ADOType, C_STRING &PLPGSQLType);
int gsc_AttribNameToPLPGSQLVariable(C_STRING &AttribName, C_STRING &PLPGSQLVariable);
int gsc_FuncFromGphToPLPGSQLFunc(C_CLASS *pCls, TCHAR *calc_func, C_STRING &PLPGSQLFunc);

int gsc_pgviewAddDefaultCalcFieldsNotOnInsertFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm);
int gsc_pgviewAddCalcFieldsFromGraphFunctionRefreshEntity(C_CLASS *pCls, bool insert_mode, C_STRING &Stm);
int gsc_pgviewAddFieldsNoFromGraphFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm);
int gsc_pgviewAddValidationFieldsFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm);

void gsc_pgviewGetFunctionNameWithParamRefreshEntity(C_STRING &ullRefFuncName, C_STRING &FuncNameWithParam);
int gsc_pgviewAddFunctionRefreshEntity(C_CLASS *pCls, C_STRING &FullRefFuncName);

void gsc_LispAddVarToListVarPLPGSQL(const TCHAR *PLPGSQLVariable, DataTypeEnum DataType, 
                                    C_STR_LIST &IntegerVarList, C_STR_LIST &DoubleVarList,
                                    C_STR_LIST &CharVarList, C_STR_LIST &GeomVarList,
                                    C_STR_LIST &BoolVarList, C_STR_LIST &DateVarList,
                                    C_STR_LIST &TimestampVarList);
void gsc_LispAddVarToListVarPLPGSQL(C_STRING &PLPGSQLVariable, DataTypeEnum DataType,
                                    C_STR_LIST &IntegerVarList, C_STR_LIST &DoubleVarList,
                                    C_STR_LIST &CharVarList, C_STR_LIST &GeomVarList,
                                    C_STR_LIST &BoolVarList, C_STR_LIST &DateVarList,
                                    C_STR_LIST &TimestampVarList);

int gsc_check_geometry_columns_table(C_DBCONNECTION *pConn);
int gsc_check_geometry_columns_table(int prj, C_INT_LIST &ClsCodeList);

bool gsc_getQGIS_isValidValuesList(C_STRING &SupportFile, C_ATTRIB *pAttrib, 
                                   C_ATTRIB_LIST *pAttribList, C_2STR_LIST &ValuesList);

int gsc_CreateHistoryGEOsimClass(C_CLASS *pCls, 
                                 C_STRING &HistoryGeomTableRef, C_STRING &HistoryAlfanumTableRef);
int gsc_CreateHistoryQryFunction(C_DBCONNECTION *pConn, C_STRING &HistoryTableRef, C_STRING &ent_key_attrib);

// roby shape
int gsc_setGoogleEarthODRecord(C_INT_LIST &ClsCodeList,  bool AllCharDataType, C_INT_INT_LIST &EmptyClsSubCodeList);
int gsc_setGoogleEarthODRecord(C_SELSET &ss, bool AllCharDataType = true);
void gsc_getGoogleEarthODTableNameFromCls(C_CLASS *pCls, C_STRING &ODTableName);
int gsc_writeScriptForShapeExport(C_STRING &ScriptPath, C_INT_LIST &ClassCodeList, C_STRING &ShapeDir,
                                  C_INT_INT_LIST *pEmptyClsSubCodeList = NULL);
int gsc_updateAcMapODTableRecord(C_CLASS *pCls, C_RB_LIST &ColValues,
                                 bool AllCharDataType, AcMapODTableRecord &Record);
int gsc_setGoogleEarthODRecord(ads_name ent, AcMapODTable *pTable, AcMapODTableRecord &NewRecord,
                               AcMapODRecordIterator *pIter);
int gsc_setGoogleEarthODRecord(C_CLASS *pCls, C_RB_LIST &ColValues,
                               AcMapODTable *pTable, bool AllCharDataType, AcMapODRecordIterator *pIter);


/*************************************************************************/
/*.doc gsc_pg_del_geometry_column                          <internal> */
/*+
  Questa funzione cancella nella tabella public.geometry_columns una riga con la
  definizione di una colonna geometrica.
  parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &TableRef;         Tabella
  C_STRING &GeomField;        Campo geometrico

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pg_del_geometry_column(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &GeomField)
{
   C_STRING  Stm, Catalog, Schema, Name, _GeomField(GeomField), Version;

   if (pConn->getPostGIS_Version(Version) == GS_BAD) return GS_BAD;

   // Da postgis 2 in poi la geometry_columns è una vista
   if (Version.startWith(_T("1")) == false)
      return GS_GOOD;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;

   // Correggo la stringa secondo la sintassi SQL 
   if (Catalog.len() == 0) Catalog = _T("''");
   else if (pConn->Str2SqlSyntax(Catalog) == GS_BAD) return GS_BAD;

   if (Schema.len() == 0) Schema = _T("''");
   else if (pConn->Str2SqlSyntax(Schema) == GS_BAD) return GS_BAD;

   if (pConn->Str2SqlSyntax(Name) == GS_BAD) return GS_BAD;

   if (pConn->Str2SqlSyntax(_GeomField) == GS_BAD) return GS_BAD;

   // cancello riga in public.geometry_columns
   Stm = _T("DELETE FROM public.geometry_columns WHERE f_table_catalog = ");
   Stm += Catalog;
   Stm += _T(" AND f_table_schema = ");
   Stm += Schema;
   Stm += _T(" AND f_table_name = ");
   Stm += Name;
   Stm += _T(" AND f_geometry_column = ");
   Stm += _GeomField;

   return pConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pg_insert_geometry_column                          <internal> */
/*+
  Questa funzione inserisce nella tabella public.geometry_columns una riga con la
  definizione di una colonna geometrica.
  parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pg_insert_geometry_column(C_DBCONNECTION *pConn, C_STRING &TableRef,
                                  C_STRING &GeomField, int CoordDim, C_STRING &SRID, int ClsType)
{
   C_STRING  Stm, Catalog, Schema, Name, _GeomField(GeomField), GeomDescr, Version;
   C_INT_STR *pGeomDescr;

   if (pConn->getPostGIS_Version(Version) == GS_BAD) return GS_BAD;

   // Da postgis 2 in poi la geometry_columns è una vista
   if (Version.startWith(_T("1")) == false)
      return GS_GOOD;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;

   // Cerco la descrizione SQL del tipo geometrico
   if ((pGeomDescr = (C_INT_STR *) pConn->get_GeomTypeDescrListPtr()->search_key(ClsType)) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   GeomDescr = pGeomDescr->get_name();

   // Correggo la stringa secondo la sintassi SQL 
   if (Catalog.len() == 0) Catalog = _T("''");
   else if (pConn->Str2SqlSyntax(Catalog) == GS_BAD) return GS_BAD;

   if (Schema.len() == 0) Schema = _T("''");
   else if (pConn->Str2SqlSyntax(Schema) == GS_BAD) return GS_BAD;

   if (pConn->Str2SqlSyntax(Name) == GS_BAD) return GS_BAD;

   if (pConn->Str2SqlSyntax(GeomDescr) == GS_BAD) return GS_BAD;

   if (pConn->Str2SqlSyntax(_GeomField) == GS_BAD) return GS_BAD;

   // cancello riga in public.geometry_columns
   if (gsc_pg_del_geometry_column(pConn, TableRef, GeomField) == GS_BAD) return GS_BAD;

   Stm = _T("INSERT INTO public.geometry_columns (f_table_catalog, f_table_schema, f_table_name, f_geometry_column, coord_dimension, srid, type) VALUES (");
   Stm += Catalog;
   Stm += _T(", ");
   Stm += Schema;
   Stm += _T(", ");
   Stm += Name;
   Stm += _T(", ");
   Stm += _GeomField;
   Stm += _T(", ");
   Stm += CoordDim;
   Stm += _T(", ");
   Stm += SRID;
   Stm += _T(", ");
   Stm += GeomDescr;
   Stm += _T(")");

   return pConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gs_whip <external> */
/*+
  Questa funzione LISP lancia la procedura che consente di creare
  da un sinottico n files tipo DWF.
  I parametri LISP sono:
  <name_sinot> <dir_dwf> <inet_addr> <descr> <prefix> <f_script> <f_dwf> 
  <f_dwg> <f_url> <f_prefix> <f_descr>
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_whip(void)
{
   C_STRING name_sinot, dir_dwf, inet_addr, descr, prefix;
   int      flag_script = 0, flag_dwg = 0, flag_url = 0, flag_dwf = 0, flag_descr = 0, flag_prefix = 0;
   presbuf  arg;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else name_sinot = arg->resval.rstring;                         // nome sinottico

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else dir_dwf = arg->resval.rstring;                            // direttorio dei DWF

   arg = arg->rbnext;
   if (arg->restype == RTNIL) inet_addr = GS_EMPTYSTR;   // indirizzo Internet vuoto
   else if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
        else inet_addr = arg->resval.rstring;   // indirizzo Internet pieno

   arg = arg->rbnext;
   if (arg->restype == RTNIL) descr = GS_EMPTYSTR;       // descrizione maschera URL vuota 
   else if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
        else descr = arg->resval.rstring;       // descrizione maschera URL piena

   arg = arg->rbnext;
   if (arg->restype == RTNIL) prefix = GS_EMPTYSTR;       // descrizione prefisso vuota 
   else if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
        else prefix = arg->resval.rstring;       // descrizione prefisso piena

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_script = arg->resval.rint;                           // lancio file script

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_dwf = arg->resval.rint;                              // genarazione dei file .DWF

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_dwg = arg->resval.rint;                              // generazione dei file .DWG

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_url = arg->resval.rint;                              // modalità inserimento URL

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_descr = arg->resval.rint;                            // modalità mascheramento URL

   arg = arg->rbnext;
   if (arg == NULL || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   else flag_prefix = arg->resval.rint;                           // modalità inserimento prefisso

   // chiamo la funzione che carica il sinottico e individua le aree
   // di estrazione per le classi selezionate
   if (gsc_whip(name_sinot, dir_dwf, inet_addr, descr, prefix,
                flag_script, flag_dwf, flag_dwg, flag_url, flag_descr, flag_prefix) == GS_BAD)
      { acedRetNil(); return RTERROR; }

   acedRetT();

   return RTNORM;
}


/*********************************************************************/
/*.doc gsc_whip <internal> */
/*+
  Questa funzione estrae il sinottico scelto effettua l'eventuale 'pulizia'
  dello stesso e prepara le n zone spaziali di estrazuine classi;
  in coda scrive un file di SCRIPT con la sequenza _NEW... estrazione classi
  salvatggio file .DWF replicata tante volte quante sono le zone spaziali 
  individuate dal sinottico.
  Parametri:
  C_STRING &sinottico  : nome del sinottico scelto (con path completa);
  C_STRING &dwf_dir    : direttorio di salvataggio dei file .DWF
  C_STRING &addr_inet  : indirizzo internet da usarsi come percorso per i file URL
  C_STRING &text_desrc : eventualte testo maschera dell'URL
  int flag_script      : flag che indica se lanciare il file di script che si
                         ottiene (1) oppuer no (0).
  int flag_dwf         : flag che indica se generare anche i file .DWF
  int flag_dwg         : flag che indica se generare anche i file .DWG
  int flag_url         : flag che indica se usare testi già presenti (1) sul sinottico
                         oppure generarne di nuovi (0);
  int flag_descr       : flag che indica se mascherare (1) l'URL nel sinottico
                         oppure no (0);

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_whip(C_STRING &sinottico, C_STRING &dwf_dir, C_STRING &addr_inet,
             C_STRING &text_descr, C_STRING &prefix,
             int flag_script, int flag_dwf, int flag_dwg, int flag_url,
             int flag_descr, int flag_pre)
{
	int          f = 0, k = 0, result, num_vert = 0, lan = 0, FlagTxt;
	long         lungh_selset_text, lungh_selset_poly, i = 0, old_error = 0, loopCount = 1;
	double       h_text, altezza = 0.0, ent_area = 0.0;
	TCHAR        cod[4], cod_prj[3];
	TCHAR        *point = NULL, *StrNum = NULL;
	C_STRING     path_new, etichetta, inet_addr_dwf, pc3file, level1, WhereSql, style_text;
	C_STRING     fileclass, filedwg, filescript, fileurl, sinoturl, filedwf, string;
	C_STR_LIST   lista_url, val_style_text;
	C_REAL_LIST  val_h_text;
	C_STR        *nod, *nod2;
	C_REAL       *nod1;
	FILE         *file, *filescr, *furl, *filemove=NULL;
	C_POINT_LIST list_vert_poly, url_ins_point, pt_allin_text;
	C_RB_LIST    p_estract_cond, rb, entmask, entita, AnchorMask;
	C_STRING     list_class, list_point, riga, stile_testo;
	ade_id       dwg_id = ADE_NULLID, var_id = ADE_NULLID;
	ads_name     sel_set, sel_text, ent, ss_poly;
	ads_point    p_centroid, ptAllinText;
	C_INT_LIST   val_gen_text, val_allinOriz_text, val_allinVert_text;
	C_INT        *pInt;
	presbuf      p;

	C_STRING		destFile, pathFile, ext, name, dir, alias, dbfiledwf;
	C_DBCONNECTION	*pConn;
	C_RB_LIST		ColValues;

	C_PROJECT     *pPrj = NULL;

	double		x1, y1, x2, y2;	
	C_STRING	filename, x, y;

	_RecordsetPtr	pRs;

	// verifico l'abilitazione dell' utente;
	if (gsc_check_op(opWhip) == GS_BAD) return GS_BAD;

	// verifico che non esistano aree di lavoro attive
	if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

	// ricavo la lingua di AUTOCAD
	if (gsc_GetAcadLanguage(&lan) == GS_BAD) 
		{ GS_ERR_COD = eGSUnknowAutoCadLanguage; return GS_BAD; }

   // Setta l'ambiente ADE per lavorare in una sessione di GEOsim.
   // In particolare disabilito la richiesta di map di salvataggio oggetti modificati
   C_MAP_ENV MapEnv;
   MapEnv.SetEnv4GEOsim();

	// converto nel file sinottico eventuali "\" in "/"
	sinottico.strtran(_T("\\"), _T("/")); 

	// converto nel percorso internet eventuali "\" in "/"
	addr_inet.strtran(_T("\\"), _T("/")); 

	// converto nel direttorio dei file .DWF eventuali "\" in "/"
	dwf_dir.strtran(_T("\\"), _T("/")); 

	// effettuo l'attach e l'activate del sinottico 
	if (gsc_ADEdrv2nethost(sinottico.get_name(), path_new) == GS_BAD) return GS_BAD;
	if ((dwg_id = gsc_dsattach(path_new.get_name())) != ADE_NULLID)
		if (gsc_dwgactivate(dwg_id) != GS_GOOD) return GS_BAD;

	// imposto condizione spaziale con ALL    // settare errore
	if ((p_estract_cond << acutBuildList(RTLB, RTSTR, ALL_SPATIAL_COND, RTLE, 0)) == NULL) return GS_BAD;
	if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("Location"),
                     p_estract_cond.get_head(), GS_EMPTYSTR) == ADE_NULLID)
		{ GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }

	// setto la modalità di estrazione
	if (ade_qrysettype(_T("draw"), FALSE, GS_EMPTYSTR, GS_EMPTYSTR) != RTNORM) return GS_BAD;

	// effettuo l'estrazione del disegno (sinottico)
	ade_qryexecute();

	// effettuo uno ZOOM Estensione del disegno
	if (gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_E"), 0) != RTNORM) return GS_BAD;

	// attivo al chiamata per ripulire il disegno da eventuali errori
	int cnt = 0, num_error = 1, val_topo = 16 + 32 + 64;

	do
	{																											
		// filtro su tutti gli oggetti presenti sul piano GEOSHAPE
		if ((entmask << acutBuildList(8, _T("GEOSHAPE"), 0)) == NULL) return GS_BAD;

		// filtro su tutti gli oggetti da preservare dalla pulizia del disegno
		if ((AnchorMask << acutBuildList(8, _T("GEOURL"), 0)) == NULL) return GS_BAD;

		result = gsc_clean_sinott(entmask.get_head(), _T("NODE_ERROR"), val_topo, 
                                AnchorMask.get_head());
		if (result == GS_CAN) return GS_GOOD;      // sono stati trovati errori
		else if (result == GS_BAD) return GS_BAD;  // la funzione ha fallito per qualche motivo

		if (acedSSGet(_T("_X"), NULL, NULL, entmask.get_head(), sel_set) != RTNORM)
			{ GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }

		// creo la topologia di tipo poligono (3 = poligono)
		if ((var_id = tpm_varalloc()) == ADE_NULLID) return GS_BAD;
		if ((tpm_mntbuild (var_id, _T("gswhip"), GS_EMPTYSTR, 3, NULL, sel_set, NULL)) != RTNORM)
			ads_ssfree(sel_set);
		else num_error = 0;

	} while ((num_error != 0) && (cnt++ < 10));

	ads_ssfree(sel_set);

	// se dopo dieci tentativi non sono riuscito a costruire la topologia poligono 
	// avverto l'utente di controllare il sinottico e correggerlo opportunamente
	//"\nImpossibile creare la topologia: controllare il disegno"
	if (num_error != 0)
	   { acutPrintf(_T("%s"), gsc_msg(789)); return GS_BAD; }

	// creo un nuovo piano su cui scaricare il risultato di creazione di polilinee 
	// chiuse dalla topologia creata
   C_COLOR color;
   color.setAutoCADColorIndex(3);
   if (gsc_set_charact_layer(_T("gswhip"), NULL, &color) == GS_BAD)
      return GS_BAD; 

   // Memorizzo ultima entita
   ads_name last;
   if (acdbEntLast(last) != RTNORM) ads_name_clear(last);

	// creo l'insieme di polilinee chiuse dalla topologia precedente che serviranno
	// in seguito per individuare le zone da caricare al fini di ottenere i file .DWF
   if (gsc_callCmd(_T("_.MAPCLPLINE"), 
                   RTSTR, _T("gswhip"), // Nome topologia
                   RTSTR, _T("gswhip"), // Nome layer
                   RTSTR, _T("_N"),     // Raggrup. Polig. complessi
                   RTSTR, _T("_N"),     // Dati Oggetto
                   RTSTR, _T("_N"),     // Link Db
                   0) != RTNORM) return GS_BAD;

   // nel caso siano stati inseriti dei nuovi nuovi oggetti grafici
   acedSSAdd(NULL, NULL, ss_poly);
   while (gsc_mainentnext(last, last) == GS_GOOD)
      acedSSAdd(last, ss_poly, ss_poly);

   // Commentato per baco map (si sono dimenticati di includere map_topoclose nelle lib)
	//if (map_topoClose(_T("gswhip"), _T("gswhip"), FALSE, FALSE, FALSE, ss_poly) == RTERROR)
	//   return GS_BAD;

	// verifico se il gruppo di selezione è vuoto o ritorna errore
	if (ads_sslength(ss_poly, &lungh_selset_poly) != RTNORM)
	{
		resbuf p;
		acedGetVar(_T("ERRNO"), &p);
		if (p.resval.rint != 0) { ads_ssfree(ss_poly); return GS_BAD; }
	}
	else if (lungh_selset_poly == 0) { ads_ssfree(ss_poly); return GS_GOOD; }

	// apro il file delle classi selezionate e compongo la lista delle stesse da scivere nel
	// file di script
	fileclass = dwf_dir;
	fileclass += _T('/');
	fileclass += _T("geowhip.sel");
	if ((file = gsc_fopen(fileclass.get_name(), _T("r"))) == NULL)
		{ gsc_fclose(file); GS_ERR_COD = eGSOpenFile; return GS_BAD; }
   
	// leggo il codice progetto
	if (fwscanf(file, _T("%s"), cod_prj) == EOF)   
		{ gsc_fclose(file); GS_ERR_COD = eGSReadFile; return GS_BAD; }

	list_class.clear();
	list_class += _T("(setq lista_class_sel '(");
   
	f = 0;
	k = 0;
	while (fwscanf(file, _T("%s"), cod) != EOF)
	{
		// testo se spezzare la lista delle classi scelte (se troppo lunga)
		if (k > 0)
		{
			if (k % 25 == 0)
			{
				list_class += _T("))\n(setq list");
				list_class += f++;
				list_class += _T(" '(");
			}
			else list_class += _T(" ");
		}

		list_class += cod;
		k++;
	}
	list_class += _T("))\n");

	if (f > 0)
	{
		list_class += _T("(setq lista_class_sel (append lista_class_sel");
		for (int qq = 0; qq < f; qq++)
		{
			list_class += _T(" list");
			list_class += qq;
		}
		list_class += _T("))\n");
	}

	// chiudo il file delle classi selezionate
	if (gsc_fclose(file) == GS_BAD) return GS_BAD;

	if (flag_pre)
	{
		destFile = dwf_dir;
		destFile += _T('/');
		destFile += ACCESSGEOWHIPDB;

		pathFile = GEOsimAppl::GEODIR + _T("\\") + GEOSAMPLEDIR + _T("\\") + ACCESSGEOWHIPSAMPLEDB;

		// eventuale creazione del database
		if ((gsc_path_exist(pathFile) == GS_GOOD) && (gsc_path_exist(destFile) == GS_BAD))
			gsc_copyfile(pathFile, destFile);

		C_2STR_LIST UDLProp(_T("Data Source"), destFile.get_name());
		pConn = get_pDBCONNECTION_LIST()->get_Connection(NULL, &UDLProp);

		if (prefix.len() > 0)
		{
			level1 = prefix;
			level1 += _T("-");

			WhereSql = _T("Level=");
			WhereSql += _T("'");
			WhereSql += prefix;
			WhereSql += _T("'");
		}
		else
		{
			prefix = GS_EMPTYSTR;
			WhereSql = GS_EMPTYSTR;
		}

		if (pConn->DelRows(_T("Coord"), WhereSql.get_name()) == GS_BAD) return GS_BAD;
		if (pConn->InitInsRow(_T("Coord"), pRs) == GS_BAD) return GS_BAD;
	}

	// apro il file script
	filescript = dwf_dir;
	filescript += _T('/');
	filescript += _T("geowhip.scr");
	if ((filescr = gsc_fopen(filescript.get_name(), _T("w"))) == NULL)
		{ gsc_fclose(filescr); GS_ERR_COD = eGSOpenFile; return GS_BAD; }
   
	// apro il file url
	fileurl = dwf_dir;
	fileurl += _T('/');
	fileurl += _T("geourl.scr");
	if ((furl = gsc_fopen(fileurl.get_name(), _T("w"))) == NULL)
		{ gsc_fclose(furl); GS_ERR_COD = eGSOpenFile; return GS_BAD; }

	// memorizzo il valore di OSMODE e lo setto a 0 per disabilitarlo
	if (fwprintf(furl, _T("(setq PrevOSMODE (getvar \"OSMODE\"))\nOSMODE 0\n")) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	// scrivo sul file di url l'apertura di un nuovo file 
	if (fwprintf(furl, _T("(command \"_.NEW\" \"_Y\" \"\")\n")) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	// scrivo nel file url la riga che apre il file sinottico
	riga = _T("(command \"_.OPEN\" \"_Y\" \"");
	riga += sinottico;
	riga += _T("\")\n");
	if (fwprintf(furl, riga.get_name()) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   riga = _T("(command \"_.ZOOM\" \"_E\")\n");
	if (fwprintf(furl, riga.get_name()) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	// scrivo la riga che rende attivo il piano su cui inserire le etichette a cui
	// agganciare l'URL
	riga = _T("(command \"-LAYER\" \"_S\" \"geourl\" \"\")\n");
	if (fwprintf(furl, riga.get_name()) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	// se flag_url=TRUE cerco tutti i testi presenti sul piano GEOURL inseriti dall'utente
	if (flag_url)
	{
		C_RB_LIST Descr;

		if ((entmask << acutBuildList(RTDXF0, _T("TEXT"), 8, _T("GEOURL"), 0)) == NULL) 
			return GS_BAD; 
		if (acedSSGet(_T("_X"), NULL, NULL, entmask.get_head(), sel_text) != RTNORM)
			{ GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }

		// verifico se vi è corrispondenza biunivoca fra le poligonali ed i testi 
		// identificativi delle stesse
		if (ads_sslength(sel_text, &lungh_selset_text) != RTNORM)
		{
			resbuf p;
			acedGetVar(_T("ERRNO"), &p);
			if (p.resval.rint != 0) { ads_ssfree(sel_text); return GS_BAD; }
		}
		else if (lungh_selset_text == 0) { ads_ssfree(sel_text); return GS_GOOD; }

		if (lungh_selset_poly != lungh_selset_text)
		   //"\nMancata corrispondenza fra poligoni ed etichette"
			{  acutPrintf(gsc_msg(788)); return GS_BAD; }

		// scorro la lista delle entità trovate e ricavo il punto di inserimento e il 
		// valore del testo 
		i = 0;
		while (acedSSName(sel_text, i++, ent) == RTNORM)
		{  
			// ricavo il valore del testo
			if (gsc_getInfoText(ent, &string) == GS_BAD) 
				{ads_ssfree(sel_text); return GS_BAD; }
			string.alltrim();
			gsc_strsep(string.get_name(), _T('-'), _T(' '));
			if ((nod = new C_STR) == NULL)
				{ GS_ERR_COD=eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			if (nod->set_name(string.get_name()) == GS_BAD)
				{ ads_ssfree(sel_text); return GS_BAD; }
			lista_url.add_tail(nod);

			// ricavo il valore dell'altezza testo
			if (gsc_getInfoText(ent, NULL, NULL, &h_text) == GS_BAD) 
				{ads_ssfree(sel_text);return GS_BAD; }
			if ((nod1 = new C_REAL) == NULL)
				{ GS_ERR_COD=eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			if (nod1->set_key(h_text) == GS_BAD) { ads_ssfree(sel_text);return GS_BAD; }
			val_h_text.add_tail(nod1);

			// ricavo lo stile del testo inserito
			if (gsc_getInfoText(ent, NULL, &style_text) == GS_BAD) 
				{ads_ssfree(sel_text); return GS_BAD; }
			if ((nod2 = new C_STR) == NULL)
				{ GS_ERR_COD=eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			if (nod2->set_name(style_text.get_name()) == GS_BAD)
            { ads_ssfree(sel_text); return GS_BAD; }
			val_style_text.add_tail(nod2);

			Descr << acdbEntGet(ent);
			// ricavo il flag di generazione testo
			FlagTxt = 0;
			if ((p = Descr.SearchType(71)) != NULL) FlagTxt = p->resval.rint;
			if ((pInt = new C_INT(FlagTxt)) == NULL)
				{ GS_ERR_COD = eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			val_gen_text.add_tail(pInt);
         
			// ricavo il flag di allineamento orizzontale testo
			FlagTxt = 0;
			if ((p = Descr.SearchType(72)) != NULL) FlagTxt = p->resval.rint;
			if ((pInt = new C_INT(FlagTxt)) == NULL)
				{ GS_ERR_COD = eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			val_allinOriz_text.add_tail(pInt);
         
			// ricavo il flag di allineamento verticale testo
			FlagTxt = 0;
			if ((p = Descr.SearchType(73)) != NULL) FlagTxt = p->resval.rint;
			if ((pInt = new C_INT(FlagTxt)) == NULL)
				{ GS_ERR_COD = eGSOutOfMem; ads_ssfree(sel_text); return GS_BAD; }
			val_allinVert_text.add_tail(pInt);

			// ricavo il punto di allineamento testo
			ads_point_clear(ptAllinText);
			if ((p = Descr.SearchType(11)) != NULL) ads_point_set(p->resval.rpoint, ptAllinText);
			pt_allin_text.add_point(ptAllinText);

			// ricavo il punto di inserimento 
			url_ins_point.add_ins_point(ent);
			((C_POINT*)url_ins_point.get_cursor())->point[Z] = i;
		}
	}
	
	// cancello tutti gli eventuali testi presenti sul piano GEOURL
	riga = _T("(command \"_.ERASE\" (ssget \"X\" '((0 . \"TEXT\") (8 . \"GEOURL\")))\"\")\n");
	if (fwprintf(furl, riga.get_name()) < 0)
		{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	k = 1; i = 0;
	// scorro ss_poly e creo le zone spaziali di estrazione
	while (acedSSName(ss_poly, i++, ent) == RTNORM)
	{
		// Modifiche Paolino ...
		// la prima volta carica l' eventuale dwt, la seconda volta no !!!
		// scrivo l'apertura di un nuovo file 
		if (loopCount == 1)
		{
			if (fwprintf(filescr, _T("(command \"_.NEW\" \"_Y\" \"\")\n")) < 0)
				{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		}
		else
		{
			if (flag_dwf == GS_GOOD)
         {
            if (fwprintf(filescr, _T("(command \"_.NEW\" \"_Y\" \"\")\n")) < 0)
               { gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }
         else
				if (fwprintf(filescr, _T("(command \"_.NEW\" \"\")\n")) < 0)
					{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		}
		loopCount++;

		stile_testo.clear();
		// scrivo l'apertura di un nuovo file 
		// if (fwprintf(filescr, _T("(command \"_.NEW\" \"_Y\" \"\")\n")) < 0)
		//   { gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		// scrivo la lista delle classi
		if (fwprintf(filescr, list_class.get_name()) < 0)
			{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		list_vert_poly.remove_all();

		if (list_vert_poly.add_vertex_point(ent, GS_GOOD) == GS_BAD)
			{ ads_ssfree(ss_poly); return GS_BAD; }

		// ricavo le coordinate del centroide dell'oggetto
		if (gsc_get_centroidpoint(ent, p_centroid) == GS_BAD) return GS_BAD;
		
		if (flag_url) // uso i testi inseriti sul sinottico con il loro p.to di inserimento
		{ 
			C_POINT *punto;
         
			// cerco nella lista dei punti testo-utente il più vicino al centroide trovato
			punto = url_ins_point.get_nearest(p_centroid);
			rb << acutBuildList(RT3DPOINT, punto->point ,0);

			// ricavo l'altezza del testo
			altezza = (val_h_text.getptr_at((int) punto->point[Z]))->get_key_double();

			// ricavo il valore dello stile associato al testo
			stile_testo += (val_style_text.getptr_at((int) punto->point[Z]))->get_name();

			// ricavo il valore del testo associato al punto
			// copio in etichetta il valore che userò per agganciarlo all'URL del DWF
			etichetta = level1;
			etichetta += (lista_url.getptr_at((int) punto->point[Z]))->get_name();
			etichetta.strtran(_T("\\"), _T("/"));

			rb.get_head()->resval.rpoint[Z] = 0;
			//((C_POINT*)url_ins_point.getptr_at((int)punto->point[Z]))->point[Z] = 0;
		}
		else // costruisco le etichette di URL in modo automatico con il prefisso 'sinot' 
		{  
			C_RB_LIST rb1;

			etichetta = level1;
			etichetta += _T("sinot");
			etichetta += k;
			// calcolo l'area dell'oggetto 
			if ((rb1 << ade_expreval(ent, _T(".area"), _T("real"))) == NULL)
				{ GS_ERR_COD = eGSInvalidGraphCalc; return GS_BAD; }
			ent_area = rb1.get_head()->resval.rreal;

			// in funzione della sessione determino l'altezza del testo che inserirò 
			// come 'etichetta' di URL
			altezza = ent_area/5000;

			stile_testo += _T("STANDARD");
		}

		riga.clear();
		riga += _T("(command \"_.TEXT\" \"_S\"	\"");
		riga += stile_testo.get_name();
		riga += _T("\" '(");
		if ((StrNum = gsc_rb2str(rb.get_head())) == NULL) return GS_BAD;
		gsc_strsep(StrNum, _T(' '), _T(',')); // sostituisco eventuali ',' con ' ' 
		riga += StrNum;
		free(StrNum);
		riga += _T(") ");
		riga += altezza;
		riga += _T(" 0.0 \""); // rotazione
		riga += etichetta;
		riga += _T("\")\n");
		if (fwprintf(furl, riga.get_name()) < 0)
			{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		if (flag_dwg)  // costruisco il nome del file .DWG da salvare
		{
			filedwg = dwf_dir;
			filedwg += _T('/');
			filedwg += etichetta;
			filedwg += _T(".dwg");

			if (gsc_path_conv(filedwg) == GS_BAD) return GS_BAD;
			// converto nel filedwg eventuali "\" in "/"
			filedwg.strtran(_T("\\"), _T("/")); 
		}

		// costruisco il nome del file .DWF da salvare
		filedwf = dwf_dir;
		filedwf += _T('/');
		filedwf += etichetta;
		filedwf += _T(".dwf");

		dbfiledwf = _T("./dwf/");
		dbfiledwf += etichetta;
		dbfiledwf += _T(".dwf");

		if (gsc_path_conv(filedwf) == GS_BAD) return GS_BAD;
		// converto nel filedwf eventuali "\" in "/"
		filedwf.strtran(_T("\\"), _T("/"));

		// costruisco l'indirizzo internet per l'URL
		inet_addr_dwf = addr_inet;
		inet_addr_dwf += etichetta;
		inet_addr_dwf += _T(".dwf");
 
		k++;

		// scrivo la riga per effettuare l'attach dell'url che mi interessa
		riga = _T("(command \"-hyperlink\" \"I\" \"O\" (entlast) \"\" \"");
		riga += inet_addr_dwf;
		riga += _T("\" \"\"");
		if (flag_descr)
		{
			riga += _T("\"");
			riga += text_descr;
			riga += _T("\"");
		}
		else riga += _T("\"\"");

		riga += _T(")\n");
		if (fwprintf(furl, riga.get_name()) < 0)
			{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		// scorro la lista dei vertici del poligono e costruisco la lista
		// completa da scrivere sul file di script
		if ((num_vert = list_vert_poly.get_count()) > 0)
		{  
			presbuf p;

			p = list_vert_poly.to_rb();
			list_point.clear();
			list_point += _T("(setq lista_punti '(");
			f = 0;

			// MODIFICA PAOLA
			x1 = y1 = 1000000000000000;
			x2 = y2 = 0;

			for (int k=0; k<num_vert; k++)
			{
				// testo se spezzare la lista dei punti poligono
				if (k > 0)
				{
					if (k % 5 == 0)
					{
						list_point += _T("))\n(setq list");
						list_point += f++;
						list_point += _T(" '(");
					}
					else list_point += _T(" ");
				}

				if (p->restype == RTPOINT || p->restype == RT3DPOINT)
					list_point += _T("(");

				if ((StrNum = gsc_rb2str(p)) == NULL) return GS_BAD;
				gsc_strsep(StrNum, _T(' '), _T(',')); // sostituisco eventuali ',' con ' ' 

				list_point += StrNum;
				free(StrNum);

				if (p->restype == RTPOINT || p->restype == RT3DPOINT)
				   list_point += _T(")");
				
				// solo per rettangoli
				if (num_vert == 4)
				{
					if (x1 >= p->resval.rpoint[0]) 
						x1 = p->resval.rpoint[0];
					if (y1 >= p->resval.rpoint[1]) 
						y1 = p->resval.rpoint[1];
					if (x2 <= p->resval.rpoint[0]) 
						x2 = p->resval.rpoint[0];
					if (y2 <= p->resval.rpoint[1]) 
						y2 = p->resval.rpoint[1];
				}

				p = p->rbnext;
			}
			list_point += _T("))\n");
		}

		if (f > 0)
		{
			list_point += _T("(setq lista_punti (append lista_punti");
			for (int qq = 0; qq < f; qq++)
			{
				list_point += _T(" list");
				list_point += qq;
			}
			list_point += _T("))\n");
		} 

		// scrivo sul file di script la lista dei punti che passerò a gs_extract_4_whip
		if (fwprintf(filescr, list_point.get_name()) < 0)
			{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		// scrivo sul file di script la chiamata a gs_extract_4_whip
		riga = _T("(gs_extract_4_whip ");
		riga += cod_prj;
		riga += _T(" lista_class_sel lista_punti)\n");
		if (fwprintf(filescr, riga.get_name()) < 0)
			{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		// se flag_dwg = TRUE scrivo sul file di script il salvataggio della zona estratta
		// anche come file .DWG
		if (flag_dwg)
		{
			riga = _T("(if (findfile \"");
			riga += filedwg;
			riga += _T("\") (gs_delfile \"");
			riga += filedwg;
			riga += _T("\"))\n");
			riga += _T("(command \"_.SAVEAS\" \"2004\" \"");
			riga += filedwg;
			riga += _T("\")\n");
			if (fwprintf(filescr, riga.get_name()) < 0)
				{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		}

		// se flag_dwf = TRUE scrivo sul file di script il salvataggio della zona estratta
		// come file .DWF
		if (flag_dwf)
		{
         riga = _T("(setq selSet (ssget \"X\"))\n");
			if (fwprintf(filescr, riga.get_name()) < 0)
		      { gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

			if ((num_vert == 4) && (flag_pre))
			{
				// carico il progetto di interesse su cui effettuare la generazione dei file .DWF
				if ((pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(_wtoi(cod_prj))) == NULL ) return GS_BAD;

				filename = get_GEODIR();
				filename += _T("\\");
				filename += _T("move.ini");
				if ((filemove = gsc_open_profile(filename, READONLY)) != NULL)
				{
					if (gsc_get_profile(filemove, pPrj->get_name(), _T("X"), x) == GS_BAD) 
						x = _T("0");
					if (gsc_get_profile(filemove, pPrj->get_name(), _T("Y"), y) == GS_BAD) 
						y = _T("0");
					gsc_close_profile(filemove);

					riga = _T("(gs_disable_reactors)\n");
					riga += _T("(if (/= selSet nil) ");
					riga += _T("(command \"_.MOVE\" \"_ALL\" \"\" \"0,0\" \"");
					riga += x;
					riga += _T(",");
					riga += y;
					riga += _T("\"))\n");
					riga += _T("(if (/= selSet nil) ");
					riga += _T("(command \"_.ZOOM\" \"_E\"))\n");
					if (fwprintf(filescr, riga.get_name()) < 0)
						{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
				}
			}

			riga = _T("(if (findfile \"");
			riga += filedwf;
			riga += _T("\") (gs_delfile \"");
			riga += filedwf;
			riga += _T("\"))\n");

			riga += _T("(if (/= selSet nil) ");
         riga += _T("(command \"_.-plot\" \"_Y\" \"Model\" \"GEOsim.pc3\" \"\" \"_M\" \"_Landscape\" \"_N\" \"_Display\" \"_Fit\" \"0.00,0.00\" \"_Y\" \".\" \"_N\" \"_N\" \"");
			riga += filedwf;
         riga += _T("\" \"_N\" \"_Y\"))\n");

			if (fwprintf(filescr, riga.get_name()) < 0)
				{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		}

		if ((num_vert == 4) && (flag_pre))
		{
			ColValues << acutBuildList(RTLB,
			   							RTLB, RTSTR, _T("X1"), RTREAL, x1, RTLE,
			 							   RTLB, RTSTR, _T("Y1"), RTREAL, y1, RTLE,
			   							RTLB, RTSTR, _T("X2"), RTREAL, x2, RTLE,
										   RTLB, RTSTR, _T("Y2"), RTREAL, y2, RTLE,
										   RTLB, RTSTR, _T("STR"), RTSTR, dbfiledwf.get_name(), RTLE,
										   RTLB, RTSTR, _T("LEVEL"), RTSTR, prefix.get_name(), RTLE,
		    						      RTLE, 0);
			if (gsc_DBInsRow(pRs, ColValues) == GS_BAD) return GS_BAD;
		}
	}

	if (furl)
	   // setto il valore di OSMODE a quello originale
	   if (fwprintf(furl, _T("(setvar \"OSMODE\" PrevOSMODE)\n")) < 0)
		   { gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	if (flag_dwg || flag_dwf)  // se ho scelto di generare i file .DWG e/o .DWF
	{
		// scrivo sul file di url la generazione del sinottico come file .DWF
		sinottico.tolower();
		if (sinottico.at(_T(".dwg")) != NULL)
		{
			gsc_splitpath(sinottico, &alias, &dir, &name, &ext);
			sinoturl = alias;
			sinoturl += dir;
			sinoturl += level1;
			sinoturl += name;
			sinoturl += _T(".dwf");
		}

		riga = _T("(if (findfile \"");
		riga += sinoturl;
		riga += _T("\") (gs_delfile \"");
		riga += sinoturl;
		riga += _T("\"))\n");
		riga += _T("_.-plot\n");
		riga += _T("_Y\nModel\nGEOsim.pc3\n\n_M\n_Landscape\n_N\n");
		riga += _T("_Display\n_Fit\n0.00,0.00\n_Y\n.\n_N\n_N\n\"");
		riga += sinoturl;
		riga += _T("\"\n_N\n_Y\n");
		if (fwprintf(furl, riga.get_name()) < 0)
			{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

		// se il flag_script = TRUE scrivo l'istruzione di lancio del file geowhip.scr
		if (flag_script)
		{
			riga = _T("(command \"_.SCRIPT\" \"");
			riga += filescript;
			riga += _T("\")\n");
			if (fwprintf(furl, riga.get_name()) < 0)
				{ gsc_fclose(furl); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
		}
	}

	if ((num_vert == 4)	&& (flag_pre))
	{
		riga = _T("(gs_enable_reactors)\n");
		if (fwprintf(filescr, riga.get_name()) < 0)
			{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
	}

	// scrivo sul file di script l'apertura di un nuovo file 
	if (flag_dwf == GS_GOOD)
   {
      if (fwprintf(filescr, _T("(command \"_.NEW\" \"_Y\" \"\")\n")) < 0)
         { gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }
   else
		if (fwprintf(filescr, _T("(command \"_.NEW\" \"\")\n")) < 0)
			{ gsc_fclose(filescr); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

	ads_ssfree(ss_poly);

	// chiudo il file di script
	if (filescr)
		if (gsc_fclose(filescr) == GS_BAD) return GS_BAD;

	// chiudo il file di url (se presente)
	if (furl) 
		if (gsc_fclose(furl) == GS_BAD) return GS_BAD;

	// se ho scelto di generare i file .DWF o .DWG lancio il file geourl.scr
	if (flag_dwf || flag_dwg)
		{ if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, fileurl.get_name(), 0) != RTNORM) return GS_BAD; }
	else // cancello il file (che non serve a nulla)
		{ if (gsc_delfile(fileurl) == GS_BAD) return GS_BAD; }
	
	if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

	return GS_GOOD;
}


/*********************************************************/
/*.doc gs_extract_4_whip <external> */
/*+
  Questa funzione LISP è lanciata dallo SCRIPT scritto in precedenza.
  Parametri:<cod_prj> <dir_dwf> <selected_class_list>
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_extract_4_whip(void)
{
	int        prjcode, num_cls_sel;
	presbuf    arg;
	C_INT_LIST sel_class;
	C_RB_LIST  p_estract_cond;

	acedRetNil();

	// ricavo i valori impostati
	arg = acedGetArgs();

	if (arg == NULL || arg->restype != RTSHORT)
		{ GS_ERR_COD = eGSInvalidArg; return RTERROR; }
	else 
		prjcode = arg->resval.rint;                             // codice progetto

	arg = arg->rbnext;
	sel_class.from_rb(arg);

	// legato all'errore di passaggio dati di ads_getargs per 2/3 interi
	if ((sel_class.get_count() == 3) || (sel_class.get_count() == 2)) 
		num_cls_sel = 1;
	else 
		num_cls_sel = sel_class.get_count() + 2;
   
	for (int i = 0; i < num_cls_sel; i++) 
		arg = arg->rbnext; // avanzo alla lista punti
	   
	// lista punti aree da estrarre
	if ((p_estract_cond << gsc_rblistcopy(arg)) == NULL) 
		return RTERROR;

	// chiamo la funzione di estrazione per WHIP   
	if (gsc_extract_4_whip(prjcode, &p_estract_cond, &sel_class, _T("gswhip"), GSReadOnlyData) == GS_BAD)
		return GS_BAD;

	acedRetT();

	return RTNORM;
}
/*********************************************************************/
/*.doc gsc_extract_4_whip() <internal> */
/*+
  Questa funzione dato un progetto, una lista di classi ed una lista di punti,
  effettua l'estrazione di dette classi sull'area individuata dai punti.
  Al momento la funzione crea una sessione di lavoro chiamata gswhip con direttorio
  Parametri:
  int prjcode           : codice progetto;
  C_RB_LIST *p_estr_cond: lista condizioni spaziali di estazione;
  C_INT_LIST *cls_2_extr: lista dei codici delle classi da estrarre;
  TCHAR *work_area      : nome sessione di lavoro e del sottodirettorio della stessa;    
  GSDataPermissionTypeEnum mode : modalità della sessione di lavoro (i valori sono:
                                  GSReadOnlyData, GSUpdateableData)
                         
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_extract_4_whip(int prjcode, C_RB_LIST *p_estr_cond,
                       C_INT_LIST *cls_2_extr, TCHAR *work_area, GSDataPermissionTypeEnum mode)
{
   int           k = 0;
   C_WRK_SESSION        *pworkarea = NULL;
   C_STRING      session_dir;
   C_PROJECT     *pproject = NULL;
   C_RB_LIST     spaz_cond;
   C_INT_INT_STR usr;

   // carico il progetto di interesse su cui effettuare la generazione dei file .DWF
   if ((pproject = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(prjcode)) == NULL)
      return GS_BAD;

   // imposto i valori per creare una sessione di lavoro temporanea "gswhip" 
   // alloco un oggetto C_WRK_SESSION
   if ((pworkarea = new C_WRK_SESSION(pproject)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // setto il nome e il direttorio della stessa
   if (pworkarea->set_name(work_area) == GS_BAD) return GS_BAD;
   session_dir = pproject->get_dir();
   session_dir += _T("/");
   session_dir += work_area;
   if (pworkarea->set_dir(session_dir.get_name()) == GS_BAD) return GS_BAD;
   pworkarea->set_level(mode);

   // per ogni poligono della lista imposto le condizioni per l'estrazione
   while (p_estr_cond->GetCount() != 0)
   {
      p_estr_cond->remove_head();  // elimino RTLB della serie punti poligono

      // imposto la condizione spaziale di estrazione
      spaz_cond << acutBuildList(RTLB, RTSTR, POLYGON_SPATIAL_COND, // polygon
                                       RTSTR, _T("crossing"), 0); 
                                                                         
      // ciclo su tutti i punti del poligono selezionato
      while (p_estr_cond->get_head()->restype != RTLE)
      {
         spaz_cond += acutBuildList(RTPOINT, p_estr_cond->get_head()->resval.rpoint, 0); 
         p_estr_cond->remove_head();
      }

      p_estr_cond->remove_head();  // elimino RTLE della serie punti poligono
      spaz_cond += acutBuildList(RTLE, 0); 
   
      // lancio la funzione di estrazione in modalità preview
	  // NO perchè in modalità PREVIEW non vengono distinti i layer
	  // if (gsc_extract_classes(pproject, *pworkarea, *cls_2_extr, FALSE,
	  //                          spaz_cond.get_head(), NULL, FALSE, PREVIEW) == GS_BAD) return GS_BAD;
      if (gsc_extract_classes(pproject, *pworkarea, *cls_2_extr, FALSE,
                              spaz_cond.get_head(), NULL, FALSE) == GS_BAD) return GS_BAD;

      // effettuo uno ZOOM Estensione del disegno
      if (gsc_callCmd(_T("_.ZOOM"), RTSTR, _T("_E"), 0) != RTNORM) return GS_BAD;
   }

   return GS_GOOD;
}

/*********************************************************************/
/*.doc gsc_clean_sinott() <internal> */
/*+
  Questa funzione effettua la pulizia del disegno caricato.
  Parametri: 
  presbuf maschera : maschera di filtro da passare a acedSSGet
  TCHAR   *topo_var: variabile topologica da settare per le modalità 
                     di cleanup(pulizia)
  int     mode     : valore a cui settare detta variabile 
                     (vedere manuale di sviluppo)
  presbuf AnchorMask : maschera di filtro degli oggetti da preservare dalla pulizia del disegno

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_clean_sinott(presbuf maschera, TCHAR *topo_var, int mode, presbuf AnchorMask)
{
   ade_id     var_id = ADE_NULLID;
   ade_id     cln_id = ADE_NULLID;
   ads_name   sel_set;
   presbuf    set_var = NULL;
   long       qty = 0;
   int        type = 0, subtype = 0, done = FALSE, result = 0;

   var_id = tpm_varalloc();
   cln_id = tpm_cleanalloc();

   if (!var_id || !cln_id)
   {
      GS_ERR_COD = eGSOutOfMem; 
      tpm_varfree(var_id);
      tpm_cleanfree(cln_id);
      return GS_BAD; 
   }

   // inizializzo il sel_set con gli oggetti che devo 'pulire'
   if (acedSSGet(_T("X"), NULL, NULL, maschera, sel_set) != RTNORM) 
      { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }

   // attivo la 'pulizia del disegno'
   if (tpm_cleaninit(cln_id, var_id, sel_set) != RTNORM)
      { GS_ERR_COD = eGSNoCleanInit; tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }
   acedSSFree(sel_set);

   if (AnchorMask)
   {
      ads_name  AnchorSS;

      // inizializzo il sel_set con gli oggetti da preservare
      if (acedSSGet(_T("X"), NULL, NULL, AnchorMask, AnchorSS) == RTNORM) 
         if (tpm_cleaninitanchorset(cln_id, var_id, AnchorSS) != RTNORM)
            { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }
      acedSSFree(AnchorSS);
   }

   // inizializzo la variabile topologica passata (topo_var) al valore passato (mode)
   set_var = acutBuildList(RTSHORT ,mode, 0);
   if (tpm_varset(var_id, topo_var, set_var) != RTNORM)
      { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }

   while (TRUE)
   {
      if (tpm_cleanstart(cln_id) != RTNORM)
      {
         // Impossibile avviare la 'pulizia del disegno' 
         GS_ERR_COD = eGSNoCleanStart;
         tpm_varfree(var_id);
         tpm_cleanfree(cln_id);
         return GS_BAD; 
      }

      // testo se il processo di pulizia è completo
      if (tpm_cleancomplete(cln_id) == TRUE)
      {
         acutPrintf(gsc_msg(703));    // "\nProcesso di pulizia completato"
         break;
      }

      // ciclo sugli errori da correggere 
      while (!done)
      {
         // cerco il prossimo gruppo di errori
         result = tpm_cleangroupnext(cln_id);
         if (result == RTNORM)
         {
            // testo se il processo di pulizia è completo
            if (tpm_cleancomplete(cln_id) == TRUE) done = TRUE;
            else
            {
               // determina il tipo del gruppo corrente
               type  = tpm_cleangrouptype(cln_id);
               // determina il sottotipo del gruppo corrente
               subtype = tpm_cleangroupsubtype(cln_id);
               // conta gli errori del gruppo corrente
               tpm_cleangroupqty(cln_id, &qty);
               
               switch (type)
               {
                  case SHORT_OBJ:
                     switch (subtype)
                     {
                        case DEG_ENT:
                           acutPrintf(_T("%s%d"), gsc_msg(704), qty); //"\nOggetti corti isolati "
                           break;
                        case SHORT_ENT:
                           acutPrintf(_T("%s%d"), gsc_msg(705), qty); //"\nOggetti corti "
                           break;
                        case SHORT_SEGM:
                           acutPrintf(_T("%s%d"), gsc_msg(706), qty); //"\nSegmenti corti interni "
                           break;
                     }
                     break;
                  case CROSS:
                     acutPrintf(_T("%s%d"), gsc_msg(707), qty);       //"\nIncroci da spezzare "
                     break;
                  case UNDERSHOOT:
                     switch (subtype)
                     {
                        case VERT_2_SEG:
                           acutPrintf(_T("%s%d"), gsc_msg(708), qty); //"\nOggetti da estendere al segmento più vicino " 
                           break;
                        case VERT_2_VERT:
                           acutPrintf(_T("%s%d"), gsc_msg(709), qty); //"\nOggetti da estendere al vertice più vicino "
                           break;
                     }
                     break;
                  case DUPLICATES:
                     acutPrintf(_T("%s%d"), gsc_msg(710), qty);       //"\nOggetti duplicati "
                     break;
                  case CLUSTER:
                     acutPrintf(_T("%s%d"), gsc_msg(711), qty);       //"\nNodi da raggruppare "
                     break;
                  case PSEUDO:
                     acutPrintf(_T("%s%d"), gsc_msg(712), qty);       //"\nPseudo nodi da eliminare " 
                     break;
                  case DANGLING:
                     acutPrintf(_T("%s%d"), gsc_msg(713), qty);       //"\nAppendici di oggetti da eliminare "
                     break;
               }

               if (qty > 0)
               {
                  if (tpm_cleangroupmark(cln_id) != RTNORM)
                     { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }
                  if (tpm_cleangroupfix(cln_id) != RTNORM)
                     { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }
               }

            }
         }
         // ciclo finchè il processo di pulizia è completo
      } 
      // torno in testa e rilancio il processo per assicurarmi che non esistono altri errori
   }

   if (tpm_cleanend(cln_id) != RTNORM)
      { tpm_varfree(var_id); tpm_cleanfree(cln_id); return GS_BAD; }

   tpm_varfree(var_id);
   tpm_cleanfree(cln_id);
    
   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER POSTGIS
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_is_class_with_pg_data                              <external> */
/*+
  Questa funzione verifica se i dati di una classe sono interamente in PostgreSQL
  (parte geometrica e alfanumerica devono usare la stessa connessione DB).
  Parametri:
  C_CLASS *pCls;

  Restituisce true in caso di successo altrimenti restituisce false.
-*/  
/*************************************************************************/
bool gsc_is_class_with_pg_data(C_CLASS *pCls)
{
   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_is_class_with_pg_data(pSub) == false) return false;
         pSub = (C_SUB *) pSub->get_next();
      }

      return true;
   }

   C_DBCONNECTION *pGphConn = NULL, *pConn = NULL;

   // Se la classe ha parte geometrica
   if (pCls->ptr_GphInfo())
   {
      C_DBGPH_INFO *pGphInfo;

      if (pCls->ptr_GphInfo()->getDataSourceType() != GSDBGphDataSource) // non è in DB
         return false;
      pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
      if (!(pGphConn = pGphInfo->getDBConnection())) return false;
      if (gsc_strcmp(pGphConn->get_DBMSName(), PG_DBMSNAME, FALSE) != 0)
      {
         return false; // non è in PostgreSQL
      }
   }

   // Se la classe ha parte alfanumerica
   if (pCls->ptr_info())
   {
      if (!(pConn = pCls->ptr_info()->getDBConnection(OLD))) return false;
      if (gsc_strcmp(pConn->get_DBMSName(), PG_DBMSNAME, FALSE) != 0)
      {
         return false; // non è in PostgreSQL
      }
   }

   if (pGphConn && pConn)
      // Le connessioni devono essere uguali
      if (pGphConn != pConn) return false;

   return true;
}


/*************************************************************************/
/*.doc gs_pgview                                              <external> */
/*+
  Questa funzione LISP crea e/o sostituisce le viste in postgis per il corretto
  funzionamento della parte web. Inoltre può controllare la correttezza delle
  righe della tabella geometry_columns.
  I parametri LISP sono:
  (<prj>[<cls list>[<check_geometry_columns>]])
  <cls list> = lista dei codici delle classi; se = nil o non specificato significa tutte le classi
  <check_geometry_columns> = Flag di controllo tabella geometry_columns
                             se T o non specificato significa controllo abilitato

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_pgview(void)
{
   int        prj, cls = 0;
   bool       check_geometry_columns = true;
   C_INT_LIST ClsCodeList;
   presbuf arg;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // codice progetto
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &prj) == GS_BAD) return RTERROR;
   
   // lista dei codici delle classi (opzionale)
   if ((arg = arg->rbnext))
   {
      if (arg->restype != RTNIL)
      {
         if (ClsCodeList.from_rb(arg) == GS_BAD) return RTERROR;
         arg = gsc_scorri(arg); // vado in fondo alla lista dei codici delle classi
      }

      // controllo su tabella geometry_columns (opzionale)
      if (arg && (arg = arg->rbnext))
         if (gsc_rb2Bool(arg, &check_geometry_columns) == GS_BAD) return RTERROR;
   }

   if (gsc_pgview(prj, ClsCodeList, check_geometry_columns) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_pgview                                             <external> */
/*+
  Questa funzione crea e/o sostituisce le viste in postgis per il corretto
  funzionamento della parte web.
  Parametri:
  int prj;                       Codice progetto
  C_INT_LIST &ClsCodeList;       Lista dei codici delle classi, se la lista è vuota
                                 significa tutte le classi
  bool check_geometry_columns;   Opzionale; Flag per controllare la tabella 
                                 geometry_columns (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgview(int prj, C_INT_LIST &ClsCodeList, bool check_geometry_columns)
{
   C_CLASS                   *pCls;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(494)); // "Creazione viste SQL"
   long                      i = 1;

   if (gsc_superuser() == GS_BAD)
	   {GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }  // "Check se superuser" 

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

      StatusBarProgressMeter.Init(SinthClassList.get_count());
      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (gsc_pgview(pCls) == GS_BAD)
            return GS_BAD;

         StatusBarProgressMeter.Set(i++);
         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();
      StatusBarProgressMeter.Init(ClsCodeList.get_count());

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         if (gsc_pgview(pCls) == GS_BAD) return GS_BAD;

         StatusBarProgressMeter.Set(i++);
         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (check_geometry_columns)
      if (gsc_check_geometry_columns_table(prj, ClsCodeList) == GS_BAD)
            return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_del_pgview                                         <external> */
/*+
  Questa funzione cancella le viste in postgis per il corretto
  funzionamento della parte web.
  Parametri:
  int prj;                       Codice progetto
  C_INT_LIST &ClsCodeList;       Lista dei codici delle classi, se la lista è vuota
                                 significa tutte le classi
  bool check_geometry_columns;   Opzionale; Flag per controllare la tabella 
                                 geometry_columns (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_del_pgview(int prj, C_INT_LIST &ClsCodeList, bool check_geometry_columns)
{
   C_CLASS                   *pCls;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1065)); // "Cancellazione viste SQL"
   long                      i = 1;

   if (gsc_superuser() == GS_BAD)
	   {GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }  // "Check se superuser" 

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

      StatusBarProgressMeter.Init(SinthClassList.get_count());
      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (gsc_del_pgview(pCls) == GS_BAD)
            return GS_BAD;

         StatusBarProgressMeter.Set(i++);
         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();
      StatusBarProgressMeter.Init(ClsCodeList.get_count());

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         if (gsc_del_pgview(pCls) == GS_BAD) return GS_BAD;

         StatusBarProgressMeter.Set(i++);
         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (check_geometry_columns)
      if (gsc_check_geometry_columns_table(prj, ClsCodeList) == GS_BAD)
            return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgview                                             <external> */
/*+
  Questa funzione crea e/o sostituisce la vista in postgis per il corretto
  funzionamento della parte web. La classe deve avere parte grafica (se esistente)
  e la parte alfanumerica (se esistente) nello stesso db postgis.
  Le viste considerano tutti i campi della tabella geometrica e i campi della 
  tabella alfanumerica eventualmente rinominati se già esistenti in quella geometrica.
  Parametri:
  C_CLASS *pCls;     puntatore a classe GEOsim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgview(C_CLASS *pCls)
{
   // verifico abilitazione
   if (gsc_check_op(opModClass) == GS_BAD) return GS_BAD;
   if (pCls->ptr_id()->abilit != GSUpdateableData) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_pgview(pSub) == GS_BAD) return GS_BAD;
         pSub = (C_SUB *) pSub->get_next();
      }

      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.view_enabled = true;
      return pCls->mod_id(&newID);
   }

   C_STRING       Stm, FullRefView;
   C_INFO         *pInfo;
   C_DBGPH_INFO   *pGphInfo;
   C_DBCONNECTION *pGphConn;
   C_STRING       GeomName, FullRefFunctionRefreshEntity, ViewName;
   C_RB_LIST      GeomTableStru;
   presbuf        p;
   int            i;
   C_INT_STR      *pGeomDescr;
   C_2STR_LIST    Name_Alias_list;

   // Se la classe non ha i dati in PostgreSQL
   if (gsc_is_class_with_pg_data(pCls) == false) return GS_GOOD;
   
   // La classe deve avere la parte grafica
   if (pCls->ptr_GphInfo() == NULL) return GS_GOOD;
   pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   if ((pGphConn = pGphInfo->getDBConnection()) == NULL) return GS_BAD;   

   pInfo = pCls->ptr_info();

   // Cerco la descrizione SQL del tipo geometrico
   if ((pGeomDescr = (C_INT_STR *) pGphConn->get_GeomTypeDescrListPtr()->search_key(pCls->get_type())) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // Vista tra geometria e dati alfanumerici - inizio
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), 
                                        &Name_Alias_list,
                                        &ViewName, &FullRefView, 
                                        NULL, NULL, NULL, &FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   // cancello vista
   if (pGphConn->DelView(FullRefView.get_name()) == GS_BAD) return GS_BAD;

   // creo la vista
   Stm = _T("CREATE VIEW ");
   Stm += FullRefView;
   Stm += _T(" AS SELECT ");

   // Leggo la struttura della tabella della geometria
   if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->TableRef.get_name())) == NULL)
      return GS_BAD;

   // Aggiungo i campi della tabella geometrica
   if (gsc_pgviewAddGeomAttrib(pCls, GeomTableStru, Stm) == GS_BAD)
      return GS_BAD;

   // Aggiungo i campi della tabella alfanumerica
   if (pCls->ptr_attrib_list())
   {
      Stm += _T(",");
      if (gsc_pgviewAddAlfaNumAttrib(pCls, Name_Alias_list, Stm) == GS_BAD)
         return GS_BAD;
   }

   Stm += _T(" FROM "); // a = alias di attributi alfanumerici
   Stm += pGphInfo->TableRef;
   Stm += _T(" g"); // g = alias della tabella geometrica
   if (pInfo) // Se la classe ha la parte alfanumerica
   {
      Stm += _T(" JOIN "); // g = alias della tabella geometrica
      Stm += pInfo->OldTableRef;
      Stm += _T("a ON g.");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(" = a.");
      Stm += pInfo->key_attrib;
   }

   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // grant su vista per l'utente postgres
   Stm = _T("GRANT ALL ON TABLE ");
   Stm += FullRefView;
   Stm += _T(" TO postgres;");   
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // grant su vista per l'utente mapserver
   Stm = _T("GRANT SELECT ON TABLE ");
   Stm += FullRefView;
   Stm += _T(" TO mapserver;");   
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // inserisco riga in public.geometry_columns
   if (gsc_pg_insert_geometry_column(pGphConn, FullRefView, pGphInfo->geom_attrib,
                                     (pGphInfo->geom_dim == GS_3D) ? 3 : 2,
                                     pGphInfo->coordinate_system, pCls->get_type()) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionRefreshEntity(pCls, FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   // Aggiungo l'intelligenza del dato attraverso delle regole sulla vista
   // su inserimento, modifica e cancellazione
   if (gsc_pgviewAddRulesOnGeomView(pCls) == GS_BAD) return GS_BAD;

   // Vista tra geometria e dati alfanumerici - fine

   if (!pInfo) // Se la classe NON ha la parte alfanumerica
   {
      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.view_enabled = true;
      return pCls->mod_id(&newID);
   }
   
   // Se la classe ha la parte alfanumerica

   /////////////////////////////////////////////////
   // Vista tra etichette e dati alfanumerici - inizio
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), 
                                        NULL, NULL, &FullRefView) == GS_BAD)
      return GS_BAD;

   // cancello vista
   if (pGphConn->DelView(FullRefView.get_name()) == GS_BAD) return GS_BAD;

   if (pGphInfo->LblTableRef.len() == 0 ||
       pCls->ptr_attrib_list()->is_visible() == GS_BAD) // non ha etichette
   {
      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.view_enabled = true;
      return pCls->mod_id(&newID);
   }
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), 
                                        &Name_Alias_list,
                                        &ViewName, &FullRefView) == GS_BAD)
      return GS_BAD;

   // creo la vista
   Stm = _T("CREATE VIEW ");
   Stm += FullRefView;

   // Se le etichette non sono raggruppate
   if (pGphInfo->LblGroupingTableRef.len() == 0)
   {
      Stm += _T(" AS SELECT "); // g = alias della tabella etichette

      // Leggo la struttura della tabella etichette
      if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
         return GS_BAD;
      // Aggiungo i campi della tabella delle etichette
      if (gsc_pgviewAddGeomAttrib(pCls, GeomTableStru, Stm, true) == GS_BAD)
         return GS_BAD;

      // Aggiungo i campi della tabella alfanumerica
      if (pCls->ptr_attrib_list())
      {
         Stm += _T(",");
         if (gsc_pgviewAddAlfaNumAttrib(pCls, Name_Alias_list, Stm) == GS_BAD)
            return GS_BAD;
      }

      Stm += _T(" FROM "); // a = alias di attributi alfanumerici
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" g JOIN "); // g = alias di geometria etichette
      Stm += pInfo->OldTableRef;
      Stm += _T(" a ON  g.");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(" = a.");
      Stm += pInfo->key_attrib;
   }
   else // Se le etichette sono raggruppate
   {
      Stm += _T(" AS SELECT "); // g = alias della tabella etichette

      // Leggo la struttura della tabella etichette
      if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
         return GS_BAD;

      i = 0;
      while ((p = GeomTableStru.nth(i++)) != NULL)
      {
         p = gsc_nth(0, p); // nome del campo

         if (pGphInfo->text_attrib.len() > 0 && pGphInfo->text_attrib.comp(p->resval.rstring, FALSE) == 0) // case insensitive
         {
            // Poichè GEOweb non supporta l'uso del carattere %%c lo devo convertire in Ø
            // regexp_replace('%%c100', '%%C', 'Ø', 'ig')
            // sostituisci '%%C' con il carattere 'Ø' insensitive (carattere 'i') e
            // globalmente in tutta la stringa (carattere 'g')

            // commentato perchè se QGIS usa il controllo "mappa valori" necessita di avere la corrispondenza 
            // con il valore nel formato "%%C" e non nel valore trasformato in "Ø"
            //Stm += _T("regexp_replace(");
            //Stm += _T("g.");  // g = alias della tabella geometrica delle etichette
            //Stm += p->resval.rstring;
            //Stm += _T(",'%%C', 'Ø', 'ig') AS ");
            //Stm += p->resval.rstring;
            Stm += _T("g.");  // g = alias della tabella geometrica delle etichette
            Stm += p->resval.rstring;
         }
         else
         {
            Stm += _T("g.");  // g = alias della tabella geometrica delle etichette
            Stm += p->resval.rstring;
         }
         Stm += _T(",");
      }

      // Aggiungo i campi della tabella alfanumerica
      if (pCls->ptr_attrib_list())
      {
         if (gsc_pgviewAddAlfaNumAttrib(pCls, Name_Alias_list, Stm) == GS_BAD)
            return GS_BAD;
      }
      else
         Stm.removePrefixSuffix(NULL, _T(",")); // Elimino ultima virgola

      Stm += _T(" FROM ("); 
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" g JOIN "); // g = alias di geometria etichette
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T("grp ON g."); // grp = alias di raggruppamento etichette
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(" = grp.");
      Stm += pGphInfo->key_attrib;

      Stm += _T(") JOIN ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" a ON grp."); // a = alias di attributi alfanumerici
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(" = a.");
      Stm += pInfo->key_attrib;
   }

   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // grant su vista per l'utente postgres
   Stm = _T("GRANT ALL ON TABLE ");
   Stm += FullRefView;
   Stm += _T(" TO postgres;");   
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // grant su vista per l'utente mapserver
   Stm = _T("GRANT SELECT ON TABLE ");
   Stm += FullRefView;
   Stm += _T(" TO mapserver;");   
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // inserisco riga in public.geometry_columns
   if (gsc_pg_insert_geometry_column(pGphConn, FullRefView, pGphInfo->geom_attrib,
                                     (pGphInfo->geom_dim == GS_3D) ? 3 : 2,
                                     pGphInfo->coordinate_system, TYPE_NODE) == GS_BAD)
      return GS_BAD;

   // Aggiungo l'intelligenza del dato attraverso delle regole sulla vista
   // su inserimento, modifica e cancellazione
   if (gsc_pgviewAddRulesOnLblView(pCls) == GS_BAD)
         return GS_BAD;
   
   // Vista tra etichette e dati alfanumerici - fine

   C_ID newID;
   pCls->ptr_id()->copy(&newID);
   newID.view_enabled = true;
   return pCls->mod_id(&newID);
}


/*************************************************************************/
/*.doc gsc_del_pgview                                         <external> */
/*+
  Questa funzione cancella la vista in postgis per il corretto
  funzionamento della parte web.
  Parametri:
  C_CLASS *pCls;     puntatore a classe GEOsim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_del_pgview(C_CLASS *pCls)
{
   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_pgview(pSub) == GS_BAD) return GS_BAD;
         pSub = (C_SUB *) pSub->get_next();
      }

      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.view_enabled = false;
      return pCls->mod_id(&newID);
   }

   C_DBGPH_INFO   *pGphInfo;
   C_DBCONNECTION *pGphConn;
   C_STRING       FullRefView, FuncNameWithParam, Stm;
   C_STRING       FullRefFunctionRefreshEntity, DelFullRefFuncName, InsFullRefFuncName, UpdFullRefFuncName;

   // Se la classe non ha i dati in PostgreSQL
   if (gsc_is_class_with_pg_data(pCls) == false) return GS_GOOD;

   // La classe deve avere la parte grafica
   if (pCls->ptr_GphInfo() == NULL) return GS_GOOD;
   pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   if ((pGphConn = pGphInfo->getDBConnection()) == NULL) return GS_BAD;   

   // Vista tra geometria e dati alfanumerici
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, &FullRefView, 
                                        _T("del"), NULL, &DelFullRefFuncName,
                                        &FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;
   // cancello la funzione di refresh
   gsc_pgviewGetFunctionNameWithParamRefreshEntity(FullRefFunctionRefreshEntity, FuncNameWithParam);
   if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;

   // cancello la funzione
   gsc_pgviewGetFunctionNameWithParamOnDeleteGeomView(pCls, DelFullRefFuncName, FuncNameWithParam);
   if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;

   // verifico che esista la vista perchè è parametro della funzione e se tento la sua cancellazione
   // ottengo un errore di "tipo parametro non conosciuto"
   if (pGphConn->ExistTable(FullRefView, ONETEST) == GS_GOOD)
   {
      // cancello la funzione
      if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL, 
                                           _T("ins"), NULL, &InsFullRefFuncName) == GS_BAD)
         return GS_BAD;
      gsc_pgviewGetFunctionNameWithParamOnInsertGeomView(InsFullRefFuncName, FullRefView, FuncNameWithParam);
      if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;
   }

   // verifico che esista la vista perchè è parametro della funzione e se tento la sua cancellazione
   // ottengo un errore di "tipo parametro non conosciuto"
   if (pGphConn->ExistTable(FullRefView, ONETEST) == GS_GOOD)
   {
      // cancello la funzione
      if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL, 
                                           _T("upd"), NULL, &UpdFullRefFuncName) == GS_BAD)
         return GS_BAD;
      gsc_pgviewGetFunctionNameWithParamOnUpdateGeomView(pCls, UpdFullRefFuncName, FullRefView, FuncNameWithParam);
      if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;
   }

   // cancello vista
   if (pGphConn->DelView(FullRefView.get_name()) == GS_BAD) return GS_BAD;
   // cancello riga in public.geometry_columns
   if (gsc_pg_del_geometry_column(pGphConn, FullRefView, pGphInfo->geom_attrib) == GS_BAD) return GS_BAD;

   if (pGphInfo->LblTableRef.len() > 0 &&  pCls->ptr_attrib_list()->is_visible() == GS_GOOD) // ha etichette
   {
      // Vista tra geometria etichette e dati alfanumerici
      if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), NULL, NULL, &FullRefView, 
                                           _T("del"), NULL, &DelFullRefFuncName) == GS_BAD)
         return GS_BAD;

      // cancello la funzione
      gsc_pgviewGetFunctionNameWithParamOnDeleteLblView(pCls, DelFullRefFuncName, FuncNameWithParam);
      if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;

      // verifico che esista la vista perchè è parametro della funzione e se tento la sua cancellazione
      // ottengo un errore di "tipo parametro non conosciuto"
      if (pGphConn->ExistTable(FullRefView, ONETEST) == GS_GOOD)
      {
         // cancello la funzione
         if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), NULL, NULL, NULL, 
                                              _T("ins"), NULL, &InsFullRefFuncName) == GS_BAD)
            return GS_BAD;
         gsc_pgviewGetFunctionNameWithParamOnInsertLblView(InsFullRefFuncName, FullRefView, FuncNameWithParam);
         if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;
      }

      // verifico che esista la vista perchè è parametro della funzione e se tento la sua cancellazione
      // ottengo un errore di "tipo parametro non conosciuto"
      if (pGphConn->ExistTable(FullRefView, ONETEST) == GS_GOOD)
      {
         // cancello la funzione
         if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), NULL, NULL, NULL, 
                                              _T("upd"), NULL, &UpdFullRefFuncName) == GS_BAD)
            return GS_BAD;
         gsc_pgviewGetFunctionNameWithParamOnUpdateLblView(pCls, UpdFullRefFuncName, FullRefView, FuncNameWithParam);
         if (pGphConn->DelFunction(FuncNameWithParam.get_name()) == GS_BAD) return GS_BAD;
      }

      // cancello vista
      if (pGphConn->DelView(FullRefView.get_name()) == GS_BAD) return GS_BAD;
      // cancello riga in public.geometry_columns
      if (gsc_pg_del_geometry_column(pGphConn, FullRefView, pGphInfo->geom_attrib) == GS_BAD) return GS_BAD;
   }

   C_ID newID;
   pCls->ptr_id()->copy(&newID);
   newID.view_enabled = false;
   return pCls->mod_id(&newID);
}


/*************************************************************************/
/*.doc gsc_pgviewAddGeomAttrib                             <internal> */
/*+
  Funzione di ausilio alla gsc_pgview. La funzione aggiunge la lista degli attributi
  della tabella geometrica all'istruzione SQL che deve creare una vista.
  Parametri:
  C_CLASS *pCls;            Puntatore a classe GEOsim (input)
  C_RB_LIST &GeomTableStru; Struttura della tabella geometrica (input)
  C_STRING &Stm;            istruzione SQL (output)
  bool Lbl;                 Opzionale; se = true signifca che si tratta di tabella delle label
                            (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddGeomAttrib(C_CLASS *pCls, C_RB_LIST &GeomTableStru, C_STRING &Stm,
                            bool Lbl)
{
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   int          i;
   C_STRING     Name;
   presbuf      p;
   bool         First = true;

   i = 0;
   // Se esistono salto i campi ent_key_attrib, aggr_factor_attrib
   while (p = GeomTableStru.nth(i++))
   {
      p = gsc_nth(0, p); // nome del campo
      if (pCls->ptr_attrib_list() &&
          pGphInfo->ent_key_attrib.len() > 0 && pGphInfo->ent_key_attrib.comp(p->resval.rstring, FALSE) == 0 || // case insensitive
          pGphInfo->aggr_factor_attrib.len() > 0 && pGphInfo->aggr_factor_attrib.comp(p->resval.rstring, FALSE) == 0) // case insensitive
         continue;

      Name = _T("g."); // g = alias della tabella geometrica
      Name +=  p->resval.rstring;

      if (pGphInfo->geom_attrib.comp(p->resval.rstring, FALSE) == 0) // se campo geometrico
      {
         C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
         C_STRING Version;
         
         if (pGphConn->getPostGIS_Version(Version) == GS_BAD) return GS_BAD;

         // Da postgis 2 in poi il campo geometry deve essere specificato con tipo e srid
         // come da esempio: select geom::geometry('LINESTRING', 3003) from dati.tabella
         if (Version.startWith(_T("1")) == false)
         {
            C_INT_STR *pGeomDescr;

            Name += _T("::geometry('");
            // Cerco la descrizione SQL del tipo geometrico
            if ((pGeomDescr = (C_INT_STR *)pGphConn->get_GeomTypeDescrListPtr()->search_key(pCls->ptr_id()->type)) == NULL)
               { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
            Name += pGeomDescr->get_name();
            Name += _T("',");
            Name += pGphInfo->coordinate_system;
            Name += _T(")");
         }
      }
      if (First) First = false;
      else Stm += _T(",");

      if (Lbl && // se si tratta di etichette
          pGphInfo->text_attrib.len() > 0 && pGphInfo->text_attrib.comp(p->resval.rstring, FALSE) == 0) // case insensitive
      {
         // Poichè GEOweb non supporta l'uso del carattere %%c lo devo convertire in Ø
         // regexp_replace('%%c100', '%%C', 'Ø', 'ig')
         // sostituisci '%%C' con il carattere 'Ø' insensitive (carattere 'i') e
         // globalmente in tutta la stringa (carattere 'g')

         // commentato perchè se QGIS usa il controllo "mappa valori" necessita di avere la corrispondenza 
         // con il valore nel formato "%%C" e non nel valore trasformato in "Ø"
         //Stm += _T("regexp_replace(");
         //Stm += Name;
         //Stm += _T(",'%%C', 'Ø', 'ig') AS ");
         Stm += Name;
      }
      else
         Stm += Name;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewAddAlfaNumAttrib                             <internal> */
/*+
  Funzione di ausilio alla gsc_pgview. La funzione aggiunge la lista degli attributi
  della tabella alfanumerica all'istruzione SQL che deve creare una vista.
  Parametri:
  C_CLASS *pCls;                 Puntatore a classe GEOsim
  C_2STR_LIST &Name_Alias_list;  Lista di  <nome attributo><nome con alias> usato nell'istruzione SQL
  C_STRING &Stm;                 Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddAlfaNumAttrib(C_CLASS *pCls, C_2STR_LIST &Name_Alias_list,
                               C_STRING &Stm)
{
   C_DBCONNECTION *pConn = pCls->ptr_info()->getDBConnection(OLD);
   C_ATTRIB       *pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
   int            i = 0;
   C_STRING       Name, AttribName;
   C_2STR         *p_Name_Alias;

   // Inizializzo i tipi ADO per ogni attributo
   if (pCls->ptr_attrib_list()->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   // Ciclo sui campi alfanumerici
   while (pAttrib)
   {
      AttribName = pAttrib->get_name();
      if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      // Se il campo è carattere
      if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD)
      {
         // Poichè GEOweb non supporta l'uso del carattere \ lo devo convertire in /
         // regexp_replace(campo, '\\\\', '/', 'g')
         // sostituisci \\ ('\\\\' ogni \ viene duplicato in \\)
         // con il carattere '/' globalmente in tutta la stringa (carattere 'g')

         // Poichè GEOweb non supporta l'uso del carattere %%c lo devo convertire in Ø
         // regexp_replace('%%c100', '%%C', 'Ø', 'ig')
         // sostituisci '%%C' con il carattere 'Ø' insensitive (carattere 'i') e
         // globalmente in tutta la stringa (carattere 'g')

         // Poichè GEOweb non supporta l'uso degli alias devo eliminare
         // evetuali gli alias di GEOsim. Uso la funzione SQl POstgreSQL "regexp_replace"
         // regexp_replace('<c>\\foobarbaz', '^<[^>]*>', '')
         // '^<[^>]*>'
         // partendo dall'inizio della stringa (primo "^")
         // carattere iniziale "<"
         // seguito da un carattere che non sia ">" ("[^>]")
         // ripetuto 0 o più volte ("*")
         // che termina con il carattere ">"

         Name = _T("regexp_replace(");
         //Name += _T("regexp_replace(");
         Name += _T("regexp_replace(a."); // a = alias di tabella alfanumerica
         Name += AttribName;
         Name += _T(",'^<[^>]*>', '') ");
         // commentato perchè se QGIS usa il controllo "mappa valori" necessita di avere la corrispondenza 
         // con il valore nel formato "%%C" e non nel valore trasformato in "Ø"
         //Name += _T(",'%%C', 'Ø', 'ig')");
         Name += _T(",E'\\\\\\\\', '/', 'g')"); // ogni \ viene duplicato
      }
      else
      {
         Name = _T("a."); // a = alias di tabella alfanumerica
         Name += AttribName;
      }

      if ((p_Name_Alias = (C_2STR *) Name_Alias_list.search_name(pAttrib->get_name(), FALSE)) != NULL)
      {
         Name += _T(" AS ");
         Name += p_Name_Alias->get_name2(); // alias del campo
      }

      if (i > 0) Stm += _T(",");
      Stm += Name;

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
      i++;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_check_geometry_columns_table                       <external> */
/*+
  Questa funzione cancella i record non validi della tabella geometry_columns.
  La tabella geometry_columns si trova nello schema public e ha tre colonne che
  definiscono la tabella a cui ci si riferisce: f_table_catalog, f_table_schema
  e f_table_name. La colonna f_geometry_column definisce il campo geometrico.
  La funzione cicla su tutte le righe della tabella geometry_columns ed effettua
  una "select f_geometry_column from f_table_catalog.f_table_schema.f_table_name
  where f_geometry_column is null" per verificare che esista quel campo nella tabella.
  Parametri:
  C_DBCONNECTION *pConn;     connessione ole-db

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_check_geometry_columns_table(C_DBCONNECTION *pConn)
{
   C_STRING       Stm, FullRefTable, Cat, Sch, Table, Col, Version;
	_RecordsetPtr	pRs;
   C_RB_LIST      ColValues, Stru;
   presbuf        pCat, pSch, pTable, pCol;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(395)); // "Controllo tabella <public.geometry_columns>"
   long                 i = 0;

   if (pConn->getPostGIS_Version(Version) == GS_BAD) return GS_BAD;

   // Da postgis 2 in poi la geometry_columns è una vista
   if (Version.startWith(_T("1")) == false)
      return GS_GOOD;

   Stm = _T("SELECT f_table_catalog,f_table_schema,f_table_name,f_geometry_column FROM public.geometry_columns");

   // leggo le righe della tabella bloccandole
   if (pConn->ExeCmd(Stm, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD) return GS_BAD;
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   pCat   = ColValues.CdrAssoc(_T("f_table_catalog"));
   pSch   = ColValues.CdrAssoc(_T("f_table_schema"));
   pTable = ColValues.CdrAssoc(_T("f_table_name"));
   pCol   = ColValues.CdrAssoc(_T("f_geometry_column"));

   StatusLineMsg.Init();
   // scorro l'elenco dei collegamenti
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      StatusLineMsg.Set(gsc_msg(416), ++i); // "%ld righe elaborate."
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
      Cat   = pCat;
      Sch   = pSch;
      Table = pTable;
      Col   = pCol;
      if (FullRefTable.paste(pConn->get_FullRefTable(Cat, Sch, Table)) != NULL)
      {
         // se non c'è la tabella
         if (pConn->ExistTable(FullRefTable) == GS_BAD) 
            gsc_DBDelRow(pRs);
         else
         {
            // leggo la struttura della tabella
            if ((Stru << pConn->ReadStruct(FullRefTable.get_name())) == NULL) return GS_BAD;
            // Se non c'è il campo
            if (!Stru.Assoc(Col.get_name()))
               gsc_DBDelRow(pRs);
         }
      }

      gsc_Skip(pRs);
   }

	if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   StatusLineMsg.End(gsc_msg(416), i); // "%ld righe elaborate."

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_check_geometry_columns_table                       <external> */
/*+
  Questa funzione cancella i record non validi della tabella geometry_columns.
  Parametri:
  int prj;                       Codice progetto
  C_INT_LIST &ClsCodeList;       Lista dei codici delle classi, se la lista è vuota
                                 significa tutte le classi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_check_geometry_columns_table(int prj, C_INT_LIST &ClsCodeList)
{
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(395)); // "Controllo tabella <public.geometry_columns>"
   C_STR_LIST ConnStrList;
   int        i = 1;
   C_CLASS    *pCls;

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;
      StatusBarProgressMeter.Init(SinthClassList.get_count());

      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (pCls->ptr_GphInfo())
            // Se la grafica era in formato DB
            if (pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
               // Se questa connessione non era ancora stata usata
               if (ConnStrList.search_name(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection()->get_OrigConnectionStr()) == NULL)
               {
                  gsc_check_geometry_columns_table(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection());
                  ConnStrList.add_tail_str(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection()->get_OrigConnectionStr());
               }

         StatusBarProgressMeter.Set(i++);
         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();
      StatusBarProgressMeter.Init(ClsCodeList.get_count());

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         // Se la grafica era in formato DB
         if (pCls->ptr_GphInfo())
            // Se la grafica era in formato DB
            if (pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
               // Se questa connessione non era ancora stata usata
               if (ConnStrList.search_name(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection()->get_OrigConnectionStr()) == NULL)
               {
                  gsc_check_geometry_columns_table(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection());
                  ConnStrList.add_tail_str(((C_DBGPH_INFO *) pCls->ptr_GphInfo())->getDBConnection()->get_OrigConnectionStr());
               }

         StatusBarProgressMeter.Set(i++);
         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewGetAuxiliaryInfoOnView                       <external> */
/*+
  Questa funzione ricava una serie di informazioni utili per creare le viste 
  POSTGIS con regole e funzioni.
  Parametri:
  C_CLASS *pCls;                 Puntatore a classe GEOsim (input)
  TCHAR *ViewType;               Tipo di vista (input)
                                 Se = "geom" -> vista "tabella geometrica/alfanumerica"
                                 Se = "lbl" -> vista "tabella etichette/alfanumerica"
  C_2STR_LIST *pName_Alias_list; Opzionale; Puntatore a lista dei nomi degli 
                                 attributi della classe accoppiati agli alias 
                                 usati nella vista (default = NULL)
  C_STRING *pViewName;           Opzionale; Nome della vista (default = NULL)
  C_STRING *pFullRefView;        Opzionale; Nome della vista completo di schema (default = NULL)
  TCHAR *RuleType;               Opzionale; Tipo di regole (input, default = NULL).
                                 Se = "del" -> regola per cancellazione
                                 Se = "ins" -> regola per inserimento
                                 Se = "upd" -> regola per aggiornamento
  C_STRING *pRuleName;           Opzionale; Nome della regola (default = NULL)
  C_STRING *pFullRefFuncName;    Opzionale; Nome della funzione chiamata dalla
                                 regola completo di schema (default = NULL)
  C_STRING *pFullRefFunctionRefreshEntity; Opzionale; Nome della funzione di riallineamento
                                           della classe (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
int gsc_pgviewGetAuxiliaryInfoOnView(C_CLASS *pCls, TCHAR *ViewType, 
                                     C_2STR_LIST *pName_Alias_list,
                                     C_STRING *pViewName, C_STRING *pFullRefView,
                                     TCHAR *RuleType, C_STRING *pRuleName, C_STRING *pFullRefFuncName,
                                     C_STRING *pFullRefFunctionRefreshEntity)
{
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();

   if (!pCls || !ViewType) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // se non è vista geometrica/alfanumerica ne vista etichette/alfanumerica
   if (gsc_strcmp(ViewType, _T("geom")) != 0 && gsc_strcmp(ViewType, _T("lbl")) != 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (pName_Alias_list) pName_Alias_list->remove_all();

   if (pName_Alias_list && pCls->ptr_attrib_list())
   {
      C_ATTRIB  *pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      C_STRING  AliasName;
      C_2STR    *p_Name_Alias;
      C_RB_LIST GeomTableStru;

      if (gsc_strcmp(ViewType, _T("geom")) == 0) // vista geometrica/alfanumerica
      {
         // Leggo la struttura della tabella geometrica
         if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->TableRef.get_name())) == NULL)
            return GS_BAD;
      }
      else
         // Leggo la struttura della tabella etichette
         if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
            return GS_BAD;

      // Ciclo sui campi alfanumerici
      while (pAttrib)
      {
         AliasName = pAttrib->get_name();
         // Se il campo è presente anche nella tabella geometrica o esiste già un alias uguale
         while (GeomTableStru.Assoc(AliasName.get_name()) || 
                pName_Alias_list->search_name2(AliasName.get_name(), FALSE))
            AliasName += _T("_a");

         if (gsc_AdjSyntax(AliasName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                           pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
            return GS_BAD;

         p_Name_Alias = new C_2STR(pAttrib->get_name(), AliasName.get_name());
         pName_Alias_list->add_tail(p_Name_Alias);

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
   }

   C_STRING Catalog, Schema, BaseName, ViewName, RuleName;

   // Ottengo il nome completo (per le simulazioni contiene anche il nome della madre)
   pCls->get_CompleteName(BaseName);
   // Correggo il nome
   BaseName.strtran(_T(" "), _T("_"));
   BaseName.strtran(_T("'"), _T("_"));
   BaseName.strtran(_T("\""), _T("_"));
   BaseName.strtran(_T("-"), _T("_"));
   ViewName = BaseName;

   if (gsc_strcmp(ViewType, _T("geom")) == 0) // vista geometrica/alfanumerica
      ViewName += _T("_v");
   else
   if (gsc_strcmp(ViewType, _T("lbl")) == 0) // vista etichette/alfanumerica
      ViewName += _T("_lbl_v");

   switch (pGphConn->get_StrCaseFullTableRef())
   {
      case Upper: ViewName.toupper(); break;
      case Lower: ViewName.tolower(); break;
   }

   if (pViewName) pViewName->set_name(ViewName.get_name());

   if (pFullRefView || pFullRefFuncName || pFullRefFunctionRefreshEntity)
   {
      C_STRING dummy;

      if (pGphConn->split_FullRefTable(pGphInfo->TableRef, Catalog, Schema, dummy) == GS_BAD)
         return GS_BAD;

      if (pFullRefView || pFullRefFunctionRefreshEntity)
         if (pFullRefView->paste(pGphConn->get_FullRefTable(Catalog, Schema, ViewName)) == NULL)
            return GS_BAD;

      if (pFullRefFunctionRefreshEntity)
      {
         dummy = BaseName;
         dummy += _T("_refresh");
         if (pFullRefFunctionRefreshEntity->paste(pGphConn->get_FullRefTable(Catalog, Schema, dummy)) == NULL)
            return GS_BAD;
      }
   }

   if (!RuleType) return GS_GOOD;
   
   RuleName = ViewName;
   if (gsc_strcmp(RuleType, _T("del")) == 0)
      RuleName += _T("_del");
   else
   if (gsc_strcmp(RuleType, _T("ins")) == 0)
      RuleName += _T("_ins");
   else
   if (gsc_strcmp(RuleType, _T("upd")) == 0)
      RuleName += _T("_upd");
   else
      return GS_BAD;

   switch (pGphConn->get_StrCaseFullTableRef())
   {
      case Upper: RuleName.toupper(); break;
      case Lower: RuleName.tolower(); break;
   }

   if (pRuleName) pRuleName->set_name(RuleName.get_name());

   if (pFullRefFuncName)
      if (pFullRefFuncName->paste(pGphConn->get_FullRefTable(Catalog, Schema, RuleName)) == NULL)
         return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewGetLockTableStm                              <internal> */
/*+
  Funzione di ausilio alla gsc_pgview. La funzione restituisce le istruzioni 
  PGPLSQL per bloccare le tabelle di una classe.
  Parametri:
  C_CLASS *pCls;                 Puntatore a classe GEOsim
  C_2STR_LIST &Name_Alias_list;  Lista di  <nome attributo><nome con alias> usato nell'istruzione SQL
  C_STRING &Stm;                 Istruzione SQL (output)
  bool EntityTable;              Flag di lock; True per bloccare la tabella entità (default = true)
  bool GeomTable;                Flag di lock; True per bloccare la tabella geometrica (default = true)
  bool LabelTable;               Flag di lock; True per bloccare la tabella etichette (default = true)
  bool GrpLabelTable;            Flag di lock; True per bloccare la tabella gruppi di etichette (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewGetLockTableStm(C_CLASS *pCls, C_STRING &Stm,
                              bool EntityTable, bool GeomTable,
                              bool LabelTable, bool GrpLabelTable)
{
   C_INFO       *pInfo = pCls->ptr_info();
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   Stm.clear();
   if (EntityTable && pInfo)
   {
      Stm += _T("LOCK TABLE ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" IN ROW EXCLUSIVE MODE NOWAIT; ");
      Stm += GS_LFSTR;
   }
   if (GeomTable && pGphInfo)
   {
      Stm += _T("LOCK TABLE ");
      Stm += pGphInfo->TableRef;
      Stm += _T(" IN ROW EXCLUSIVE MODE NOWAIT; ");
      Stm += GS_LFSTR;
   }
   if (LabelTable && pGphInfo && pGphInfo->LblTableRef.len() > 0)
   {
      Stm += _T("LOCK TABLE ");
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" IN ROW EXCLUSIVE MODE NOWAIT; ");
      Stm += GS_LFSTR;
   }
   if (GrpLabelTable && pGphInfo && pGphInfo->LblGroupingTableRef.len() > 0)
   {
      Stm += _T("LOCK TABLE ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T(" IN ROW EXCLUSIVE MODE NOWAIT; ");
      Stm += GS_LFSTR;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewGetUpdEntityTableStm                         <internal> */
/*+
  Funzione di ausilio alla gsc_pgview. La funzione restituisce una istruzioni 
  PGPLSQL per aggoirnare i dati alfanumerici della tabella di una classe.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_2STR_LIST *pName_Alias_list; Opzionale; Lista dei nomi degli attributi della classe 
                                 accoppiati agli alias usati nella vista. 
  C_STRING &Entity_id_param;     Nome del parametro che si riferisce al codice entità
  C_STRING &Stm;                 Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewGetUpdEntityTableStm(C_CLASS *pCls,
                                   C_2STR_LIST *pName_Alias_list,
                                   TCHAR *Entity_id_param,
                                   C_STRING &Stm)
{
   C_STRING       Stm1, AttribName;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn = pInfo->getDBConnection(OLD);
   C_ATTRIB       *pAttrib;
   int            i = 1;
   C_2STR         *pName_Alias;

   //      -- Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
   //      EXECUTE 'UPDATE dati.linea SET nota=$1 WHERE id=$2' USING t_row.nota, old_entity_id;

   // Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
   Stm = _T("EXECUTE 'UPDATE ");
   Stm += pInfo->OldTableRef;
   Stm += _T(" SET ");

   pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      if (gsc_strcmp(pAttrib->get_name(), pInfo->key_attrib.get_name(), FALSE) == 0) // salto il codice chiave
         { pAttrib = (C_ATTRIB *) pAttrib->get_next(); continue; }

      if (i > 1)
      {
         Stm  += _T(",");
         Stm1 += _T(",");
      }

      AttribName = pAttrib->get_name();
      if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      Stm += AttribName; // nome attributo
      Stm += _T("=$"); // parametri
      Stm += i++;

      // alias degli attributi usati nella vista
      if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pAttrib->get_name(), FALSE)) == NULL)
         return GS_BAD;
      if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD) // Se il campo è carattere
      { // per sicurezza correggo le dimensioni dei valori
         Stm1 += _T("SUBSTRING(t_row.");
         Stm1 += pName_Alias->get_name2(); // alias del campo
         Stm1 += _T(" FOR ");
         Stm1 += pAttrib->len;
         Stm1 += _T(")");
      }
      else
      {
         Stm1 += _T("t_row.");
         Stm1 += pName_Alias->get_name2(); // alias del campo
      }
      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }
   
   Stm += _T(" WHERE ");
   Stm += pInfo->key_attrib;
   Stm += _T("=$");
   Stm += i;
   Stm += _T("' USING ");
   Stm += Stm1; // alias degli attributi usati nella vista
   if (i > 1) Stm += _T(",");
   Stm += Entity_id_param;
   Stm += _T("; ");
   Stm += GS_LFSTR;
   
   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI E REGOLE PER LA VISTA GEOMETRICA
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_pgviewAddRulesOnGeomView                           <external> */
/*+
  Questa funzione crea 3 regole per la vista della tabella geometrica con quella
  alfanumerica. Le regole servono per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;  Puntatore a classe GEOsim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddRulesOnGeomView(C_CLASS *pCls)
{
   C_STRING       Stm, FullRefView, RuleName, FullRefFuncName;
   C_STRING       FullRefFunctionRefreshEntity;
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_INFO         *pInfo;
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   C_2STR_LIST    Name_Alias_list;
   C_2STR         *pName_Alias;

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), 
                                        &Name_Alias_list,
                                        NULL, &FullRefView,
                                        _T("del"), &RuleName, &FullRefFuncName,
                                        &FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnDeleteGeomView(pCls, FullRefFuncName, FullRefView, 
                                             FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   ////////////////////////////////////////////////////////////////////////////
   // Regola per la cancellazione

   // per classi con collegamento a dati alfanumerici
   // CREATE OR REPLACE RULE linea_v_del AS
   // ON DELETE TO dati.linea_v DO INSTEAD  SELECT dati.linea_v_del(old.id, old.id_a) AS linea_v_del;
   // per classi senza collegamento a dati alfanumerici
   // CREATE OR REPLACE RULE linea_v_del AS
   // ON DELETE TO dati.linea_v DO INSTEAD  SELECT dati.linea_v_del(old.id) AS linea_v_del;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON DELETE TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(old.");
   Stm += pGphInfo->key_attrib;
   if ((pInfo = pCls->ptr_info()))
   {
      if ((pName_Alias = (C_2STR *) Name_Alias_list.search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
         return GS_BAD;
      Stm += _T(", old.");
      Stm += pName_Alias->get_name2(); // alias del campo
   }
   Stm += _T(") AS ");
   Stm += RuleName;
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   ////////////////////////////////////////////////////////////////////////////
   // Regola per l'inserimento

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL,
                                        _T("ins"), &RuleName, &FullRefFuncName) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnInsertGeomView(pCls, FullRefFuncName, FullRefView, 
                                             FullRefFunctionRefreshEntity,
                                             &Name_Alias_list) == GS_BAD)
      return GS_BAD;

   // CREATE OR REPLACE RULE linea_v_ins AS
   // ON INSERT TO dati.linea_v DO INSTEAD  SELECT dati.linea_v_ins(new.*) AS linea_v_ins;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON INSERT TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(new.*) AS ");
   Stm += RuleName;
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   ////////////////////////////////////////////////////////////////////////////
   // Regola per l'aggiornamento

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL,
                                        _T("upd"), &RuleName, &FullRefFuncName) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnUpdateGeomView(pCls, FullRefFuncName, FullRefView, 
                                             FullRefFunctionRefreshEntity,
                                             &Name_Alias_list) == GS_BAD)
      return GS_BAD;

   // per classi con collegamento a dati alfanumerici
   // CREATE OR REPLACE RULE linea_v_upd AS
   // ON UPDATE TO dati.linea_v DO INSTEAD  SELECT dati.linea_v_upd(old.id, old.id_a, new.id_a, new.*) AS linea_v_upd;
   // per classi senza collegamento a dati alfanumerici
   // CREATE OR REPLACE RULE linea_v_upd AS
   // ON UPDATE TO dati.linea_v DO INSTEAD  SELECT dati.linea_v_upd(old.id, new.*) AS linea_v_upd;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON UPDATE TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(old.");
   Stm += pGphInfo->key_attrib;
   if ((pInfo = pCls->ptr_info()))
   {
      if ((pName_Alias = (C_2STR *) Name_Alias_list.search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
         return GS_BAD;
      Stm += _T(", old.");
      Stm += pName_Alias->get_name2(); // alias del campo
      Stm += _T(", new.");
      Stm += pName_Alias->get_name2(); // alias del campo
   }
   Stm += _T(", new.*) AS ");
   Stm += RuleName;

   return pGphConn->ExeCmd(Stm);

}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnInsertGeomView     <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  inserimento riga nella vista della tabella geometrica con quella
  alfanumerica.
  Parametri:
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FullRefView;       Riferimento completo della vista
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnInsertGeomView(C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(t_row ");
   FuncNameWithParam += FullRefView;
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnInsertGeomView                  <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  inserimento riga nella vista della tabella geometrica con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità
  C_2STR_LIST *pName_Alias_list; Opzionale; Lista dei nomi degli attributi della classe 
                                 accoppiati agli alias usati nella vista. 
                                 Non usato per spaghetti (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnInsertGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity,
                                          C_2STR_LIST *pName_Alias_list)
{
   C_STRING       FuncNameWithParam, Stm, Stm1, Stm2, AttribName;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   C_ATTRIB       *pAttrib;
   int            i, param_ndx;
   C_2STR         *pName_Alias;
   C_RB_LIST      GeomTableStru;
   presbuf        p, pName;
   bool           First = true;
   long           DataType;

   //CREATE OR REPLACE FUNCTION dati.linea_v_ins(t_row dati.linea_v)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //   next_entity_id integer;
   //   next_geom_id integer;
   //BEGIN
   //   m_err_msg := NULL;
   //   IF t_row IS NULL THEN
   //      RAISE EXCEPTION '*** GS *** Geometry not inserted on dati.linea_v';
   //   END IF;
   //   -- Blocco le tabelle per prevenire che altri inseriscano dati contemporaneamente
   //   LOCK TABLE dati.linea IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g IN ROW EXCLUSIVE MODE NOWAIT;
   //   -- Calcolo il prossimo codice dell'entità
   //   EXECUTE 'SELECT COALESCE(MAX(id) + 1 , 1) FROM dati.linea' INTO next_entity_id;
   //   -- Inserisco i dati alfanumerici dell'entità
   //   EXECUTE 'INSERT INTO dati.linea (id, nota) VALUES ($1, $2)' USING next_entity_id, t_row.nota;
   //   -- Calcolo il prossimo codice della geometria
   //   EXECUTE 'SELECT COALESCE(MAX(id) + 1 , 1) FROM dati.linea_g' INTO next_geom_id;
   //   -- Inserisco i dati geometrici dell'entità
   //   EXECUTE 'INSERT INTO dati.linea_g (id, id_parent, geom) VALUES 
   //            ($1, $2, $3)' USING next_geom_id, next_entity_id, t_row.geom;
   //   -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //   m_err_msg := dati.linea_refresh(next_entity_id, 1);
   //   IF m_err_msg IS NOT NULL THEN
   //      RAISE EXCEPTION '';
   //   END IF;
   //   RETURN next_entity_id;
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      IF m_err_msg IS NULL THEN
   //         RAISE EXCEPTION '*** GS *** Geometry not inserted on dati.linea_v';
   //      ELSE
   //         RAISE EXCEPTION '%', m_err_msg;
   //      END IF;
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_v_ins(t_row dati.linea_v) TO mapserver;

   gsc_pgviewGetFunctionNameWithParamOnInsertGeomView(FullRefFuncName, FullRefView, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T(" RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE ");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying; "); // per i messaggi di errore
   Stm += GS_LFSTR;
   if (pInfo) { Stm += _T("next_entity_id integer; "); Stm += GS_LFSTR; }

   Stm += _T("next_geom_id integer; ");
   Stm += GS_LFSTR;
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   Stm += _T("IF t_row IS NULL THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Geometry not inserted on dati.linea_v';");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   // Blocco la tabella delle entità e quella della geometria
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, true, true, false, false) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;
   
   if (pInfo)
   {
      // Calcolo il prossimo codice dell'entità
      Stm += _T("EXECUTE 'SELECT COALESCE(MAX(");
      Stm += pInfo->key_attrib;
      Stm += _T(") + 1 , 1) FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T("' INTO next_entity_id; ");
      Stm += GS_LFSTR;

      // Inserisco i dati alfanumerici dell'entità
      Stm += _T("EXECUTE 'INSERT INTO ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" (");

      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      i = 1;
      Stm1.clear();
      while (pAttrib)
      {
         AttribName = pAttrib->get_name();
         if (gsc_AdjSyntax(AttribName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                           pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
            return GS_BAD;

         Stm += AttribName; // nome attributo

         Stm1 += _T("$"); // parametri
         Stm1 += i++;

         // alias degli attributi usati nella vista
         if (gsc_strcmp(pAttrib->get_name(), pInfo->key_attrib.get_name(), FALSE) == 0) // Se si tratta del codice chiave
            Stm2 += _T("next_entity_id");
         else
         {
            if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pAttrib->get_name(), FALSE)) == NULL)
               return GS_BAD;

            if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD) // Se il campo è carattere
            { // per sicurezza correggo le dimensioni dei valori
               Stm2 += _T("SUBSTRING(t_row.");
               Stm2 += pName_Alias->get_name2(); // alias del campo
               Stm2 += _T(" FOR ");
               Stm2 += pAttrib->len;
               Stm2 += _T(")");
            }
            else
            {
               Stm2 += _T("t_row.");
               Stm2 += pName_Alias->get_name2(); // alias del campo
            }
         }

         if ((pAttrib = (C_ATTRIB *) pAttrib->get_next()))
         {
            Stm  += _T(",");
            Stm1 += _T(",");
            Stm2 += _T(",");
         }
      }
      
      Stm += _T(") VALUES (");
      Stm += Stm1; // parametri
      Stm += _T(")' USING ");
      Stm += Stm2; // alias degli attributi usati nella vista
      Stm += _T("; ");
      Stm += GS_LFSTR;
   }

   // Calcolo il prossimo codice della geometria
   Stm += _T("EXECUTE 'SELECT COALESCE(MAX(");
   Stm += pGphInfo->key_attrib;
   Stm += _T(") + 1 , 1) FROM ");
   Stm += pGphInfo->TableRef;
   Stm += _T("' INTO next_geom_id; ");
   Stm += GS_LFSTR;

   // Inserisco i dati geometrici dell'entità
   Stm += _T("EXECUTE 'INSERT INTO ");
   Stm += pGphInfo->TableRef;
   Stm += _T(" (");

   // Leggo la struttura della tabella della geometria
   if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->TableRef.get_name())) == NULL)
      return GS_BAD;
   i = 0;
   param_ndx = 0;
   Stm1.clear();
   Stm2.clear();
   // Se esistono salto i campi aggr_factor_attrib
   while ((p = GeomTableStru.nth(i++)) != NULL)
   {
      pName = gsc_nth(0, p); // nome del campo
      if (pGphInfo->aggr_factor_attrib.len() > 0 && 
          pGphInfo->aggr_factor_attrib.comp(pName->resval.rstring, FALSE) == 0) // case insensitive
         continue;

      if (param_ndx > 0)
      {
         Stm  += _T(",");
         Stm1 += _T(",");
         Stm2 += _T(",");
      }
      param_ndx++;

      Stm += pName->resval.rstring;    

      Stm1 += _T("$"); // parametri
      Stm1 += param_ndx;

      if (gsc_strcmp(pName->resval.rstring, pGphInfo->key_attrib.get_name(), FALSE) == 0) // Se si tratta del codice chiave
         Stm2 += _T("next_geom_id");
      else
      if (gsc_strcmp(pName->resval.rstring, pGphInfo->ent_key_attrib.get_name(), FALSE) == 0) // Se si tratta del codice entità
         Stm2 += _T("next_entity_id");
      else
      {
         gsc_rb2Lng(gsc_nth(1, p), &DataType); // tipo del campo
         if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD && // Se il campo è carattere
             gsc_strcmp(pName->resval.rstring, pGphInfo->geom_attrib.get_name(), FALSE) != 0) // Se non si tratta del campo geometrico

         { // per sicurezza correggo le dimensioni dei valori
            Stm2 += _T("SUBSTRING(t_row.");
            Stm2 += pName;
            Stm2 += _T(" FOR ");
            Stm2 += gsc_nth(2, p); // dimensione del campo
            Stm2 += _T(")");
         }
         else
         {
            Stm2 += _T("t_row.");
            Stm2 += pName;
         }
      }
   }
         
   Stm += _T(") VALUES (");
   Stm += Stm1; // parametri
   Stm += _T(")' USING ");
   Stm += Stm2; // alias degli attributi usati nella vista
   Stm += _T("; ");
   Stm += GS_LFSTR;

   // Aggiorno l'entità e restituisco il suo codice in caso di successo
   if (pInfo)
   {
      Stm += _T("m_err_msg := ");
      Stm += FullRefFunctionRefreshEntity;
      Stm += _T("(next_entity_id, 1);"); // 1 = inserimento
      Stm += GS_LFSTR;
      Stm += _T("IF m_err_msg IS NOT NULL THEN");
      Stm += GS_LFSTR;
      Stm += _T("RAISE EXCEPTION '';");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;
      Stm += _T("RETURN next_entity_id;");
   }
   else
      Stm += _T("RETURN next_geom_id;");
   Stm += GS_LFSTR;

   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;
   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Geometry not inserted on ");
   Stm += FullRefView;
   Stm += _T("'; ");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(t_row ");
   Stm += FullRefView;
   Stm += _T(") TO mapserver");
   
   return pGphConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnUpdateGeomView     <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  modifica riga nella vista della tabella geometrica con quella
  alfanumerica.
  Parametri:
  C_CLASS *pCls;               Puntatore a classe GEOsim
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FullRefView;       Riferimento completo della vista
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnUpdateGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(geom_id integer, ");
   if (pCls->ptr_info())
      FuncNameWithParam += _T("old_entity_id integer, new_entity_id integer, ");
   FuncNameWithParam += _T("t_row ");
   FuncNameWithParam += FullRefView;
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnUpdateGeomView                  <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  modifica riga nella vista della tabella geometrica con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità
  C_2STR_LIST *pName_Alias_list; Opzionale; Lista dei nomi degli attributi della classe 
                                 accoppiati agli alias usati nella vista. 
                                 Non usato per spaghetti (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnUpdateGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity,
                                          C_2STR_LIST *pName_Alias_list)
{
   C_STRING       FuncNameWithParam, Stm, Stm1, InsFullRefFuncName, DelFullRefFuncName;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   int            i, param_ndx;
   C_RB_LIST      GeomTableStru;
   presbuf        p, pName;
   bool           First = true;
   long           DataType;

   //CREATE OR REPLACE FUNCTION dati.linea_v_upd(geom_id integer, old_entity_id integer, new_entity_id integer, t_row dati.linea_v)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //  entity_count integer;
   //
   //BEGIN
   //   IF (geom_id IS NULL) OR (old_entity_id IS NULL) OR (new_entity_id IS NULL) OR (t_row IS NULL) THEN
   //      RAISE EXCEPTION '*** GS *** Entity % not updated on dati.linea_v', old_entity_id;
   //   END IF;
   //  -- Blocco le tabelle ma NON per la lettura
   //   LOCK TABLE dati.linea IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl_grp IN ROW EXCLUSIVE MODE NOWAIT;
   //
   //   -- Se il codice entità non è cambiato aggiorno l'entità
   //   IF old_entity_id = new_entity_id THEN
   //      -- Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
   //      EXECUTE 'UPDATE dati.linea SET nota=$1 WHERE id=$2' USING t_row.nota, old_entity_id;
   //      -- Aggiorno i dati geometrici dell'entità (non aggiorno il campo ID, ID_PARENT e fattore di aggregazione)
   //      EXECUTE 'UPDATE dati.linea_g SET geom=$1 WHERE id=$2' USING t_row.geom, geom_id;
   //      -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //      m_err_msg := dati.linea_refresh(old_entity_id);
	//      IF m_err_msg IS NOT NULL THEN
	//	        RAISE EXCEPTION '';
	//      END IF;
   //   ELSE
   //      -- Verifico se esiste l'entità
   //      EXECUTE 'SELECT COUNT(*) FROM dati.linea WHERE id=$1' INTO entity_count USING new_entity_id;
   //
   //       -- Se il nuovo codice entità si riferisce ad un'entità non esistente
   //      IF entity_count = 0 THEN
   //         -- Disgrego la geometria dell'entità
   //         -- Inserisco una nuova entità 
   //         PERFORM dati.linea_v_ins(t_row);
   //         -- Cancello la vecchia geometria
   //         PERFORM dati.linea_v_del(geom_id, old_entity_id);
   //      ELSE -- Se il nuovo codice entità si riferisce ad un'entità già esistente
   //         -- Aggrego la geometria dell'entità
   //         -- Aggiorno i dati geometrici aggregandoli alla nuova entità (non aggiorno il campo ID e fattore di aggregazione)
   //         EXECUTE 'UPDATE dati.linea_g SET id_parent=$1, geom=$2 WHERE id=$3'
   //                 USING new_entity_id, t_row.geom, geom_id;

   //         -- Aggiorno l'entità vecchia
   //         m_err_msg := dati.linea_refresh(old_entity_id, 0);
	//         IF m_err_msg IS NOT NULL THEN
	//	           RAISE EXCEPTION '';
	//         END IF;

   //         -- Aggiorno l'entità a cui ci si aggrega e restituisco il suo codice in caso di successo
   //         m_err_msg := dati.linea_refresh(new_entity_id, 0);
	//         IF m_err_msg IS NOT NULL THEN
	//	           RAISE EXCEPTION '';
	//         END IF;

	//	        RETURN new_entity_id;
   //      END IF;
   //   END IF;
   //   
   //   RETURN new_entity_id;
   //   
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      IF m_err_msg IS NULL THEN
   //         RAISE EXCEPTION '*** GS *** Entity % not updated on dati.linea_v', old_entity_id;
   //      ELSE
   //         RAISE EXCEPTION '%', m_err_msg;
   //      END IF;
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_v_upd(geom_id integer, old_entity_id integer, new_entity_id integer, t_row dati.linea_v) TO mapserver;

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL,
                                        _T("ins"), NULL, &InsFullRefFuncName) == GS_BAD)
      return GS_BAD;
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL, NULL,
                                        _T("del"), NULL, &DelFullRefFuncName) == GS_BAD)
      return GS_BAD;

   gsc_pgviewGetFunctionNameWithParamOnUpdateGeomView(pCls, FullRefFuncName, FullRefView, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T("RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE ");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying; "); // per i messaggi di errore
   Stm += GS_LFSTR;
   if (pInfo) { Stm += _T("entity_count integer; "); Stm += GS_LFSTR; }
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   if (pInfo) Stm += _T("IF (geom_id IS NULL) OR (old_entity_id IS NULL) OR (new_entity_id IS NULL) OR (t_row IS NULL) THEN ");
   else Stm += _T("IF (geom_id IS NULL) OR (t_row IS NULL) THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Entity % not updated on ");
   Stm += FullRefView;
   if (pInfo) Stm += _T("', old_entity_id;");
   else Stm += _T("', geom_id;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   // Blocco la tabella delle entità, quella della geometria, 
   // delle etichette e del raggruppamento di etichette
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, true, true, true, true) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   if (pInfo)
   {
      // Se il codice entità non è cambiato aggiorno l'entità
      Stm += _T("IF old_entity_id = new_entity_id THEN ");
      Stm += GS_LFSTR;
      // Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
      if (gsc_pgviewGetUpdEntityTableStm(pCls, pName_Alias_list,
                                         _T("old_entity_id"), Stm1) == GS_BAD)
         return GS_BAD;
      Stm += Stm1;
   }

   // Aggiorno i dati geometrici dell'entità (non aggiorno il campo ID, ID_PARENT e fattore di aggregazione)
   Stm += _T("EXECUTE 'UPDATE ");
   Stm += pGphInfo->TableRef;
   Stm += _T(" SET ");

   // Leggo la struttura della tabella della geometria
   if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->TableRef.get_name())) == NULL)
      return GS_BAD;
   i = 0;
   param_ndx = 0;
   Stm1.clear();
   // Se esistono salto i campi key_attrib, aggr_factor_attrib, ent_key_attrib
   while ((p = GeomTableStru.nth(i++)) != NULL)
   {
      pName = gsc_nth(0, p); // nome del campo
      if (pGphInfo->key_attrib.comp(pName->resval.rstring, FALSE) == 0 || // case insensitive
          pGphInfo->aggr_factor_attrib.len() > 0 && pGphInfo->aggr_factor_attrib.comp(pName->resval.rstring, FALSE) == 0 || // case insensitive
          pGphInfo->ent_key_attrib.len() > 0 && pGphInfo->ent_key_attrib.comp(pName->resval.rstring, FALSE) == 0) // case insensitive
          continue;

      if (param_ndx > 0)
      {
         Stm  += _T(",");
         Stm1 += _T(",");
      }
      param_ndx++;

      Stm += pName;
      Stm += _T("=$"); // parametri
      Stm += param_ndx;

      gsc_rb2Lng(gsc_nth(1, p), &DataType); // tipo del campo
      if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD && // Se il campo è carattere
          gsc_strcmp(pName->resval.rstring, pGphInfo->geom_attrib.get_name(), FALSE) != 0) // Se non si tratta del campo geometrico

      { // per sicurezza correggo le dimensioni dei valori
         Stm1 += _T("SUBSTRING(t_row.");
         Stm1 += pName;
         Stm1 += _T(" FOR ");
         Stm1 += gsc_nth(2, p); // dimensione del campo
         Stm1 += _T(")");
      }
      else
      {
         Stm1 += _T("t_row.");
         Stm1 += pName;
      }
   }
         
   Stm += _T(" WHERE ");
   Stm += pGphInfo->key_attrib;
   Stm += _T("=$");
   Stm += ++param_ndx;
   Stm += _T("' USING ");
   Stm += Stm1; // alias degli attributi usati nella vista
   if (i > 1) Stm += _T(",");
   Stm += _T("geom_id; ");
   Stm += GS_LFSTR;

   // Aggiorno l'entità e restituisco il messaggio di errore in caso di fallimento o NULL in caso di successo
   if (pInfo)
   {
      Stm += _T("m_err_msg := ");
      Stm += FullRefFunctionRefreshEntity;
      Stm += _T("(old_entity_id, 0);"); // 0 = non inserimento
      Stm += GS_LFSTR;
      Stm += _T("IF m_err_msg IS NOT NULL THEN");
      Stm += GS_LFSTR;
      Stm += _T("RAISE EXCEPTION '';");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;
   }

   if (pInfo)
   {
      // Verifico se esiste l'entità
      Stm += _T("ELSE");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'SELECT COUNT(*) FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" WHERE ");
      Stm += pInfo->key_attrib;
      Stm += _T("=$1' INTO entity_count USING new_entity_id;");
      Stm += GS_LFSTR;

      // Se il nuovo codice entità si riferisce ad un'entità non esistente
      Stm += _T("IF entity_count = 0 THEN ");
      Stm += GS_LFSTR;
      // Disgrego la geometria dell'entità
      // Inserisco una nuova entità 
      Stm += _T("PERFORM ");
      Stm += InsFullRefFuncName;
      Stm += _T("(t_row); ");
      Stm += GS_LFSTR;
      // Cancello la vecchia geometria
      Stm += _T("PERFORM ");
      Stm += DelFullRefFuncName;
      Stm += _T("(geom_id, old_entity_id); ");
      Stm += GS_LFSTR;

      //  Altrimenti, se il nuovo codice entità si riferisce ad un'entità già esistente
      Stm += _T("ELSE ");
      Stm += GS_LFSTR;
      // Aggrego la geometria dell'entità
      // Aggiorno i dati geometrici aggregandoli alla nuova entità (non aggiorno il campo ID, e fattore di aggregazione)
      Stm += _T("EXECUTE 'UPDATE ");
      Stm += pGphInfo->TableRef;
      Stm += _T(" SET ");

      i = 0;
      param_ndx = 0;
      Stm1.clear();
      First = true;
      // Se esistono salto i campi key_attrib, aggr_factor_attrib
      while ((p = GeomTableStru.nth(i++)) != NULL)
      {
         pName = gsc_nth(0, p); // nome del campo
         if (pGphInfo->key_attrib.comp(pName->resval.rstring, FALSE) == 0 || // case insensitive
             pGphInfo->aggr_factor_attrib.len() > 0 && 
             pGphInfo->aggr_factor_attrib.comp(pName->resval.rstring, FALSE) == 0) // case insensitive
            continue;

         if (param_ndx > 0)
         {
            Stm  += _T(",");
            Stm1 += _T(",");
         }
         param_ndx++;

         Stm += pName->resval.rstring;    
         Stm += _T("=$"); // parametri
         Stm += param_ndx;

         gsc_rb2Lng(gsc_nth(1, p), &DataType); // tipo del campo
         if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD && // Se il campo è carattere
             gsc_strcmp(pName->resval.rstring, pGphInfo->geom_attrib.get_name(), FALSE) != 0) // Se non si tratta del campo geometrico
         { // per sicurezza correggo le dimensioni dei valori
            Stm1 += _T("SUBSTRING(t_row.");
            Stm1 += pName;
            Stm1 += _T(" FOR ");
            Stm1 += gsc_nth(2, p); // dimensione del campo
            Stm1 += _T(")");
         }
         else
         {
            if (pGphInfo->ent_key_attrib.comp(pName->resval.rstring) == 0)
               Stm1 = _T("new_entity_id");
            else
            {
               Stm1 += _T("t_row.");
               Stm1 += pName;
            }
         }
      }

      Stm += _T(" WHERE ");
      Stm += pGphInfo->key_attrib;
      Stm += _T("=$");
      Stm += ++param_ndx;
      Stm += _T("' USING ");
      Stm += Stm1; // alias degli attributi usati nella vista
      if (param_ndx > 0) Stm += _T(",");
      Stm += _T("geom_id; ");
      Stm += GS_LFSTR;

      // Aggiorno l'entità vecchia
      Stm += _T("m_err_msg := ");
      Stm += FullRefFunctionRefreshEntity;
      Stm += _T("(old_entity_id, 0); "); // 0 = non inserimento
      Stm += GS_LFSTR;
      Stm += _T("IF m_err_msg IS NOT NULL THEN ");
      Stm += GS_LFSTR;
      Stm += _T("RAISE EXCEPTION '';");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;

      // Aggiorno l'entità a cui ci si aggrega e restituisco il suo codice in caso di successo
      Stm += _T("m_err_msg := ");
      Stm += FullRefFunctionRefreshEntity;
      Stm += _T("(new_entity_id, 0);"); // 0 = non inserimento
      Stm += GS_LFSTR;
      Stm += _T("IF m_err_msg IS NOT NULL THEN ");
      Stm += GS_LFSTR;
      Stm += _T("RAISE EXCEPTION '';");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;

      Stm += _T("END IF; ");
      Stm += GS_LFSTR;
      Stm += _T("END IF; ");
      Stm += GS_LFSTR;
   }

   if (pInfo) Stm += _T("RETURN new_entity_id; ");
   else  Stm += _T("RETURN geom_id;");
   Stm += GS_LFSTR;
   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;

   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Entity % not updated on ");
   Stm += FullRefView;
   if (pInfo) Stm += _T("', old_entity_id;");
   else  Stm += _T("', geom_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(geom_id integer, ");
   if (pInfo)
      Stm += _T("old_entity_id integer, new_entity_id integer, ");
   Stm += _T("t_row ");
   Stm += FullRefView;
   Stm += _T(") TO mapserver");
   
   return pGphConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnDeleteGeomView     <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  cancellazione riga nella vista della tabella geometrica con quella alfanumerica.
  Parametri:
  C_CLASS *pCls;               Puntatore a classe GEOsim
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnDeleteGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                        C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(geom_id integer");
   if (pCls->ptr_info())
      FuncNameWithParam += _T(", entity_id integer");
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnDeleteGeomView                  <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  cancellazione riga nella vista della tabella geometrica con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnDeleteGeomView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity)
{
   C_STRING     FuncNameWithParam, Stm, Stm1;
   C_INFO       *pInfo = pCls->ptr_info();
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   //CREATE OR REPLACE FUNCTION dati.linea_v_del(geom_id integer, entity_id integer)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //BEGIN
   //   IF (geom_id IS NULL) OR (entity_id IS NULL) THEN
   //      RAISE EXCEPTION '*** GS *** Geometry % not deleted on dati.linea_v', geom_id;
   //   END IF;
   //   -- Blocco le tabelle ma NON per la lettura
   //   LOCK TABLE dati.linea IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl_grp IN ROW EXCLUSIVE MODE NOWAIT;
   //   
   //   -- Cancella la geometria
   //   EXECUTE 'DELETE FROM dati.linea_g WHERE id=$1' USING geom_id;
   //   -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //   m_err_msg := dati.linea_refresh(entity_id, 0);
   //   IF m_err_msg IS NOT NULL THEN
   //      RAISE EXCEPTION '';
   //   END IF;
   //   RETURN entity_id;
   //   
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      IF m_err_msg IS NULL THEN
   //         RAISE EXCEPTION '*** GS *** Geometry % not deleted on dati.linea_v', geom_id;            
   //      ELSE
   //         RAISE EXCEPTION '%', m_err_msg;
   //      END IF;
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_v_del(geom_id integer, entity_id integer) TO mapserver;

   gsc_pgviewGetFunctionNameWithParamOnDeleteGeomView(pCls, FullRefFuncName, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T("RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying; "); // per i messaggi di errore
   Stm += GS_LFSTR;
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   if (pInfo) Stm += _T("IF (geom_id IS NULL) OR (entity_id IS NULL) THEN ");
   else Stm += _T("IF (geom_id IS NULL) THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Geometry % not deleted on ");
   Stm += FullRefView;
   Stm += _T("', geom_id;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   // Blocco la tabella delle entità, quella della geometria, 
   // delle etichette e del raggruppamento di etichette
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, true, true, true, true) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   // Cancella la geometria
   Stm += _T("EXECUTE 'DELETE FROM ");
   Stm += pGphInfo->TableRef;
   Stm += _T(" WHERE ");
   Stm += pGphInfo->key_attrib;
   Stm += _T("=$1' USING geom_id; ");
   Stm += GS_LFSTR;

   if (pInfo)
   {
      // Aggiorno l'entità e restituisco il suo codice in caso di successo
      Stm += _T("m_err_msg := ");
      Stm += FullRefFunctionRefreshEntity;
      Stm += _T("(entity_id, 0);"); // 0 = non inserimento
      Stm += GS_LFSTR;
      Stm += _T("IF m_err_msg IS NOT NULL THEN");
      Stm += GS_LFSTR;
      Stm += _T("RAISE EXCEPTION '';");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;
      Stm += _T("RETURN entity_id;");
   }
   else
      Stm += _T("RETURN geom_id;");

   Stm += GS_LFSTR;
   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;

   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Geometry % not deleted on ");
   Stm += FullRefView;
   Stm += _T("', geom_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphInfo->getDBConnection()->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(geom_id integer");
   if (pInfo)
      Stm += _T(", entity_id integer");
   Stm += _T(") TO mapserver");
   
   return pGphInfo->getDBConnection()->ExeCmd(Stm);
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI E REGOLE PER LA VISTA GEOMETRICA
// INIZIO FUNZIONI E REGOLE PER LA VISTA ETICHETTE
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_pgviewAddRulesOnLblView                            <external> */
/*+
  Questa funzione crea 3 regole per la vista della tabella delle etichette con quella
  alfanumerica. Le regole servono per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;  Puntatore a classe GEOsim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddRulesOnLblView(C_CLASS *pCls)
{
   C_STRING       Stm, FullRefView, RuleName, FullRefFuncName;
   C_STRING       FullRefFunctionRefreshEntity;
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   C_2STR_LIST    Name_Alias_list;
   C_2STR         *pName_Alias;

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), 
                                        &Name_Alias_list,
                                        NULL, &FullRefView,
                                        _T("del"), &RuleName, &FullRefFuncName,
                                        &FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnDeleteLblView(pCls, FullRefFuncName, FullRefView, 
                                            FullRefFunctionRefreshEntity) == GS_BAD)
      return GS_BAD;

   ////////////////////////////////////////////////////////////////////////////
   // Regola per la cancellazione

   // CREATE OR REPLACE RULE linea_lbl_v_del AS
   // ON DELETE TO dati.linea_lbl_v DO INSTEAD  SELECT dati.linea_lbl_v_del(old.id, old.id_a) AS linea_lbl_v_del;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON DELETE TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(old.");
   Stm += pGphInfo->key_attrib;
   if ((pName_Alias = (C_2STR *) Name_Alias_list.search_name(pCls->ptr_info()->key_attrib.get_name(), FALSE)) == NULL)
      return GS_BAD;
   Stm += _T(", old.");
   Stm += pName_Alias->get_name2(); // alias del campo
   Stm += _T(") AS ");
   Stm += RuleName;
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   ////////////////////////////////////////////////////////////////////////////
   // Regola per l'inserimento

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), NULL, NULL, NULL,
                                        _T("ins"), &RuleName, &FullRefFuncName) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnInsertLblView(pCls, FullRefFuncName, FullRefView, 
                                             FullRefFunctionRefreshEntity,
                                             &Name_Alias_list) == GS_BAD)
      return GS_BAD;

   // CREATE OR REPLACE RULE linea_lbl_v_ins AS
   // ON INSERT TO dati.linea_lbl_v DO INSTEAD  SELECT dati.linea_lbl_v_ins(new.*) AS linea_lbl_v_ins;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;
   
   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON INSERT TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(new.*) AS ");
   Stm += RuleName;
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   ////////////////////////////////////////////////////////////////////////////
   // Regola per l'aggiornamento

   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("lbl"), NULL, NULL, NULL,
                                        _T("upd"), &RuleName, &FullRefFuncName) == GS_BAD)
      return GS_BAD;

   if (gsc_pgviewAddFunctionOnUpdateLblView(pCls, FullRefFuncName, FullRefView, 
                                            FullRefFunctionRefreshEntity,
                                            &Name_Alias_list) == GS_BAD)
      return GS_BAD;

   // CREATE OR REPLACE RULE linea_lbl_v_upd AS
   // ON UPDATE TO dati.linea_lbl_v DO INSTEAD  SELECT dati.linea_lbl_v_upd(old.id, old.id_a, new.*) AS linea_lbl_v_upd;
   if (gsc_AdjSyntax(RuleName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   Stm = _T("CREATE OR REPLACE RULE ");
   Stm += RuleName;
   Stm += _T(" AS ON UPDATE TO ");
   Stm += FullRefView;
   Stm += _T(" DO INSTEAD SELECT ");
   Stm += FullRefFuncName;
   Stm += _T("(old.");
   Stm += pGphInfo->key_attrib;
   if ((pName_Alias = (C_2STR *) Name_Alias_list.search_name(pCls->ptr_info()->key_attrib.get_name(), FALSE)) == NULL)
      return GS_BAD;
   Stm += _T(", old.");
   Stm += pName_Alias->get_name2(); // alias del campo
   Stm += _T(", new.*) AS ");
   Stm += RuleName;

   return pGphConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnInsertLblView      <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  inserimento riga nella vista della tabella etichette con quella alfanumerica.
  Parametri:
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FullRefView;       Riferimento completo della vista
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnInsertLblView(C_STRING &FullRefFuncName, 
                                                        C_STRING &FullRefView,
                                                        C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(t_row ");
   FuncNameWithParam += FullRefView;
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnInsertLblView                   <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  inserimento riga nella vista della tabella etichette con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità
  C_2STR_LIST *pName_Alias_list; Lista dei nomi degli attributi della classe 
                                 accoppiati agli alias usati nella vista. 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnInsertLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                         C_STRING &FullRefView, 
                                         C_STRING &FullRefFunctionRefreshEntity,
                                         C_2STR_LIST *pName_Alias_list)
{
   C_STRING       FuncNameWithParam, Stm, Stm1, Stm2;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   int            i, param_ndx;
   C_2STR         *pName_Alias;
   C_RB_LIST      GeomTableStru;
   presbuf        p, pName;
   bool           First = true;
   long           DataType;

   //CREATE OR REPLACE FUNCTION dati.linea_lbl_v_ins(t_row dati.linea_tbl_v)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //   lbl_value      character varying;
   //   next_lbl_id    integer;
   //   next_lblgrp_id integer;
   //BEGIN
   //   IF t_row IS NULL THEN
   //      RAISE EXCEPTION '*** GS *** Label not inserted on dati.linea_tbl_v';
   //   END IF;
   //   -- Cerco il valore dell'attributo da etichettare nella tabella delle entità
   //   EXECUTE 'SELECT ' || quote_ident(t_row.lbl_attr_name) || ' FROM dati.linea WHERE id=$1'
   //           INTO lbl_value USING t_row.id_a;
   //   
   //   -- Blocco le tabelle per prevenire che altri inseriscano dati contemporaneamente
   //   LOCK TABLE dati.linea_g_lbl IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl_grp IN ROW EXCLUSIVE MODE NOWAIT;
   //   -- Calcolo il prossimo codice di gruppo delle etichette
   //   EXECUTE 'SELECT COALESCE(MAX(id) + 1 , 1) FROM dati.linea_g_lbl_grp' INTO next_lblgrp_id;
   //   -- Inserisco i dati del gruppo delle etichette
   //   EXECUTE 'INSERT INTO dati.linea_g_lbl_grp (id, id_parent, geom) VALUES 
   //            ($1, $2, $3)' USING next_lblgrp_id, t_row.id_a, t_row.geom;
   //   -- Calcolo il prossimo codice delle etichette
   //   EXECUTE 'SELECT COALESCE(MAX(id) + 1 , 1) FROM dati.linea_g_lbl' INTO next_lbl_id;
   //   -- Inserisco i dati dell'etichetta
   //   EXECUTE 'INSERT INTO dati.linea_g_lbl (id, id_parent, geom, lbl_attr_name, "text") VALUES 
   //            ($1, $2, $3, $4, $5)'
   //            USING next_lbl_id, next_lblgrp_id, t_row.geom, t_row.lbl_attr_name, lbl_value;
   //   -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //   RETURN dati.linea_refresh(t_row.id_a);                               
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      RAISE EXCEPTION '*** GS *** Label not inserted on dati.linea_lbl_v';
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_lbl_v_ins(t_row dati.linea_tbl_v) TO mapserver;

   gsc_pgviewGetFunctionNameWithParamOnInsertLblView(FullRefFuncName, FullRefView, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T("RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying; "); // per i messaggi di errore
   Stm += GS_LFSTR;
   Stm += _T("lbl_value      character varying; ");
   Stm += GS_LFSTR;
   Stm += _T("next_lbl_id    integer; ");
   Stm += GS_LFSTR;
   Stm += _T("next_lblgrp_id integer; ");
   Stm += GS_LFSTR;
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   Stm += _T("IF t_row IS NULL THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label not inserted on ");
   Stm += FullRefView;
   Stm += _T("'; ");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   // Cerco il valore dell'attributo da etichettare nella tabella delle entità
   Stm += _T("EXECUTE 'SELECT ' || quote_ident(t_row.");
   Stm += pGphInfo->attrib_name_attrib;
   Stm += _T(") || ' FROM ");
   Stm += pInfo->OldTableRef;
   Stm += _T(" WHERE ");
   Stm += pInfo->key_attrib;
   Stm += _T("=$1' INTO lbl_value using t_row.");
   if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
      return GS_BAD;
   Stm += pName_Alias->get_name2();
   Stm += _T("; ");
   Stm += GS_LFSTR;

   // Blocco la tabella della geometria, 
   // delle etichette e del raggruppamento di etichette
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, false, true, true, true) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   if (pGphInfo->LblGroupingTableRef.len() > 0)
   {
      // Calcolo il prossimo codice di gruppo delle etichette
      Stm += _T("EXECUTE 'SELECT COALESCE(MAX(");
      Stm += pGphInfo->key_attrib;
      Stm += _T(") + 1 , 1) FROM ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T("' INTO next_lblgrp_id; ");
      Stm += GS_LFSTR;

      // Inserisco i dati del gruppo delle etichette
      Stm += _T("EXECUTE 'INSERT INTO ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T(" (");
      Stm += pGphInfo->key_attrib;
      Stm += _T(",");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(",");
      Stm += pGphInfo->geom_attrib;
      Stm += _T(") VALUES ($1, $2, $3)' USING next_lblgrp_id, t_row.");
      if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
         return GS_BAD;
      Stm += pName_Alias->get_name2();
      Stm += _T(", t_row.");
      Stm += pGphInfo->geom_attrib;
      Stm += _T("; ");
      Stm += GS_LFSTR;
   }

   // Calcolo il prossimo codice delle etichette
   Stm += _T("EXECUTE 'SELECT COALESCE(MAX(");
   Stm += pGphInfo->key_attrib;
   Stm += _T(") + 1 , 1) FROM ");
   Stm += pGphInfo->LblTableRef;
   Stm += _T("' INTO next_lbl_id; ");
   Stm += GS_LFSTR;

   // Inserisco i dati dell'etichetta
   Stm += _T("EXECUTE 'INSERT INTO ");
   Stm += pGphInfo->LblTableRef;
   Stm += _T(" (");

   // Leggo la struttura della tabella delle etichette
   if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
      return GS_BAD;
   i = 0;
   param_ndx = 0;
   Stm1.clear();
   Stm2.clear();
   // Se esistono salto i campi aggr_factor_attrib
   while ((p = GeomTableStru.nth(i++)) != NULL)
   {
      pName = gsc_nth(0, p); // nome del campo
      if (pGphInfo->aggr_factor_attrib.len() > 0 && 
          pGphInfo->aggr_factor_attrib.comp(pName->resval.rstring, FALSE) == 0) // case insensitive
         continue;

      if (param_ndx > 0)
      {
         Stm  += _T(",");
         Stm1 += _T(",");
         Stm2 += _T(",");
      }
      param_ndx++;

      Stm += pName;    

      Stm1 += _T("$"); // parametri
      Stm1 += i;

      if (gsc_strcmp(pName->resval.rstring, pGphInfo->key_attrib.get_name(), FALSE) == 0) // Se si tratta del codice chiave
         Stm2 += _T("next_lbl_id");
      else
      if (gsc_strcmp(pName->resval.rstring, pGphInfo->ent_key_attrib.get_name(), FALSE) == 0) // Se si tratta del codice entità
         if (pGphInfo->LblGroupingTableRef.len() > 0) Stm2 += _T("next_lblgrp_id"); // codice gruppo label
         else
         {
            Stm2 += _T("t_row."); // codice entità (tramite alias)
            if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
               return GS_BAD;
            Stm2 += pName_Alias->get_name2();
         }
      else
      if (gsc_strcmp(pName->resval.rstring, pGphInfo->text_attrib.get_name(), FALSE) == 0) // Se si tratta del testo dell'etichetta
         Stm2 += _T("lbl_value");
      else
      {
         gsc_rb2Lng(gsc_nth(1, p), &DataType); // tipo del campo
         if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD && // Se il campo è carattere
             gsc_strcmp(pName->resval.rstring, pGphInfo->geom_attrib.get_name(), FALSE) != 0) // Se non si tratta del campo geometrico
         { // per sicurezza correggo le dimensioni dei valori
            Stm2 += _T("SUBSTRING(t_row.");
            Stm2 += pName;
            Stm2 += _T(" FOR ");
            Stm2 += gsc_nth(2, p); // dimensione del campo
            Stm2 += _T(")");
         }
         else
         {
            Stm2 += _T("t_row.");
            Stm2 += pName;
         }
      }
   }
         
   Stm += _T(") VALUES (");
   Stm += Stm1; // parametri
   Stm += _T(")' USING ");
   Stm += Stm2; // alias degli attributi usati nella vista
   Stm += _T("; ");
   Stm += GS_LFSTR;

   // Aggiorno l'entità e restituisco il suo codice in caso di successo
   Stm += _T("m_err_msg := ");
   Stm += FullRefFunctionRefreshEntity;
   Stm += _T("(t_row."); // codice entità (tramite alias)
   if ((pName_Alias = (C_2STR *) pName_Alias_list->search_name(pInfo->key_attrib.get_name(), FALSE)) == NULL)
      return GS_BAD;
   Stm += pName_Alias->get_name2();
   Stm += _T(");");
   Stm += GS_LFSTR;
   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RETURN (t_row.");
   Stm += pName_Alias->get_name2();
   Stm += _T(");");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '';");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;

   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label not inserted on ");
   Stm += FullRefView;
   Stm += _T("';");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(t_row ");
   Stm += FullRefView;
   Stm += _T(") TO mapserver");
   
   return pGphConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnUpdateLblView      <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  modifica riga nella vista della tabella etichette con quella alfanumerica.
  Parametri:
  C_CLASS *pCls;               Puntatore a classe GEOsim
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FullRefView;       Riferimento completo della vista
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnUpdateLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                       C_STRING &FullRefView,
                                                       C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(lbl_id integer, ");
   if (pCls->ptr_info())
      FuncNameWithParam += _T("entity_id integer, ");
   FuncNameWithParam += _T("t_row ");
   FuncNameWithParam += FullRefView;
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnUpdateLblView                  <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  modifica riga nella vista della tabella etichette con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità
  C_2STR_LIST *pName_Alias_list; Opzionale; Lista dei nomi degli attributi della classe 
                                 accoppiati agli alias usati nella vista. 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnUpdateLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity,
                                          C_2STR_LIST *pName_Alias_list)
{
   C_STRING       FuncNameWithParam, Stm, Stm1;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   int            i, param_ndx;
   C_RB_LIST      GeomTableStru;
   presbuf        p, pName;
   bool           First = true;
   long           DataType;

   //CREATE OR REPLACE FUNCTION dati.linea_lbl_v_upd(lbl_id integer, entity_id integer, t_row dati.linea_tbl_v)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //BEGIN
   //   IF (lbl_id IS NULL) OR (entity_id IS NULL) OR (t_row IS NULL) THEN
   //      RAISE EXCEPTION '*** GS *** Label % not updated on dati.linea_tbl_v', lbl_id;
   //   END IF;
   //  -- Blocco le tabelle ma NON per la lettura
   //   LOCK TABLE dati.linea IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl_grp IN ROW EXCLUSIVE MODE NOWAIT;
   //
   //   -- Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
   //   EXECUTE 'UPDATE dati.linea SET ' || quote_ident(t_row.lbl_attr_name) || '=$1 WHERE id=$2'
   //           USING t_row."text", entity_id;
   //   -- Aggiorno i dati geometrici dell'etichetta (non aggiorno il campo ID, ID_PARENT e fattore di aggregazione)
   //   EXECUTE 'UPDATE dati.linea_g_lbl SET geom=$1 WHERE id=$2'
   //           USING t_row.geom, lbl_id;
   //   -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //   RETURN dati.linea_refresh(entity_id);
   //   
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      RAISE EXCEPTION '*** GS *** Label % not updated on dati.linea_lbl_v', lbl_id;            
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_lbl_v_upd(lbl_id integer, entity_id integer, t_row dati.linea_tbl_v) TO mapserver;
   
   gsc_pgviewGetFunctionNameWithParamOnUpdateLblView(pCls, FullRefFuncName, FullRefView, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T("RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE ");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying;"); // per i messaggi di errore
   Stm += GS_LFSTR;
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   Stm += _T("IF (lbl_id IS NULL) OR (entity_id IS NULL) OR (t_row IS NULL) THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label % not updated on ");
   Stm += FullRefView;
   Stm += _T("', lbl_id; ");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   // Blocco la tabella delle entità, quella della geometria, 
   // delle etichette e del raggruppamento di etichette
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, true, true, true, true) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   // Aggiorno i dati alfanumerici dell'entità (non aggiorno il campo ID)
   if (gsc_pgviewGetUpdEntityTableStm(pCls, pName_Alias_list,
                                      _T("entity_id"), Stm1) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   // Aggiorno i dati geometrici dell'etichetta (non aggiorno il campo ID, ID_PARENT e fattore di aggregazione) 
   Stm += _T("EXECUTE 'UPDATE ");
   Stm += pGphInfo->LblTableRef;
   Stm += _T(" SET ");

   // Leggo la struttura della tabella dell'etichetta
   if ((GeomTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
      return GS_BAD;
   i = 0;
   param_ndx = 0;
   Stm1.clear();
   // Se esistono salto i campi key_attrib, aggr_factor_attrib, ent_key_attrib
   while ((p = GeomTableStru.nth(i++)) != NULL)
   {
      pName = gsc_nth(0, p); // nome del campo
      if (pGphInfo->key_attrib.comp(pName->resval.rstring, FALSE) == 0 || // case insensitive
          pGphInfo->aggr_factor_attrib.len() > 0 && pGphInfo->aggr_factor_attrib.comp(pName->resval.rstring, FALSE) == 0 || // case insensitive
          pGphInfo->ent_key_attrib.len() > 0 && pGphInfo->ent_key_attrib.comp(pName->resval.rstring, FALSE) == 0) // case insensitive
         continue;

      if (param_ndx > 0)
      {
         Stm  += _T(",");
         Stm1 += _T(",");
      }
      param_ndx++;

      Stm += pName;    
      Stm += _T("=$"); // parametri
      Stm += param_ndx;

      gsc_rb2Lng(gsc_nth(1, p), &DataType); // tipo del campo
      if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD && // Se il campo è carattere
          gsc_strcmp(pName->resval.rstring, pGphInfo->geom_attrib.get_name(), FALSE) != 0) // Se non si tratta del campo geometrico
      { // per sicurezza correggo le dimensioni dei valori
         Stm1 += _T("SUBSTRING(t_row.");
         Stm1 += pName;
         Stm1 += _T(" FOR ");
         Stm1 += gsc_nth(2, p); // dimensione del campo
         Stm1 += _T(")");
      }
      else
      {
         Stm1 += _T("t_row.");
         Stm1 += pName;
      }
   }
         
   Stm += _T(" WHERE ");
   Stm += pGphInfo->key_attrib;
   Stm += _T("=$");
   Stm += ++param_ndx;
   Stm += _T("' USING ");
   Stm += Stm1; // alias degli attributi usati nella vista
   if (param_ndx > 0) Stm += _T(",");
   Stm += _T("lbl_id; ");
   Stm += GS_LFSTR;

   // Aggiorno l'entità e restituisco il suo codice in caso di successo
   Stm += _T("m_err_msg := ");
   Stm += FullRefFunctionRefreshEntity;
   Stm += _T("(entity_id);");
   Stm += GS_LFSTR;
   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RETURN entity_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '';");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;

   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label % not updated on ");
   Stm += FullRefView;
   Stm += _T("', lbl_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(lbl_id integer, ");
   if (pInfo)
      Stm += _T("entity_id integer, ");
   Stm += _T("t_row ");
   Stm += FullRefView;
   Stm += _T(") TO mapserver");
   
   return pGphConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamOnDeleteLblView     <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata su evento di
  cancellazione riga nella vista della tabella etichette con quella alfanumerica.
  Parametri:
  C_CLASS *pCls;               Puntatore a classe GEOsim
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamOnDeleteLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                                       C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(lbl_id integer");
   if (pCls->ptr_info())
      FuncNameWithParam += _T(", entity_id integer");
   FuncNameWithParam += _T(") ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionOnDeleteLblView                  <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata su evento di
  cancellazione riga nella vista della tabella etichette con quella
  alfanumerica. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare
  C_STRING &FullRefView;         Riferimento completo della vista
  C_STRING &FullRefFunctionRefreshEntity; Riferimento completo della funzione
                                          di aggiornamento dell'entità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionOnDeleteLblView(C_CLASS *pCls, C_STRING &FullRefFuncName, 
                                          C_STRING &FullRefView, 
                                          C_STRING &FullRefFunctionRefreshEntity)
{
   C_STRING     FuncNameWithParam, Stm, Stm1;
   C_INFO       *pInfo = pCls->ptr_info();
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   //CREATE OR REPLACE FUNCTION dati.linea_lbl_v_del(lbl_id integer, entity_id integer)
   //  RETURNS integer AS
   //$BODY$
   //DECLARE
   //BEGIN
   //   IF (lbl_id IS NULL) OR (entity_id IS NULL) THEN
   //      RAISE EXCEPTION '*** GS *** Label % not deleted on dati.linea_tbl_v', lbl_id;
   //   END IF;
   //   -- Blocco le tabelle ma NON per la lettura
   //   LOCK TABLE dati.linea_g_lbl IN ROW EXCLUSIVE MODE NOWAIT;
   //   LOCK TABLE dati.linea_g_lbl_grp IN ROW EXCLUSIVE MODE NOWAIT;
   //   
   //   -- Cancella l'etichetta
   //   EXECUTE 'DELETE FROM dati.linea_g_lbl WHERE id=$1' USING lbl_id;
   //   EXECUTE 'DELETE FROM dati.poli_g_lbl_grp WHERE gs_id IN (SELECT gs_id FROM dati.poli_g_lbl_grp as GRP WHERE gs_id_parent=1 and gs_id NOT IN (SELECT gs_id_parent FROM dati.poli_g_lbl WHERE gs_id_parent=GRP.gs_id));
   //   -- Aggiorno l'entità e restituisco il suo codice in caso di successo
   //   RETURN dati.linea_refresh(entity_id);
   //   
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      RAISE EXCEPTION '*** GS *** Label % not deleted on dati.linea_lbl_v', lbl_id;            
   //      RETURN 0;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_lbl_v_del(lbl_id integer, entity_id integer) TO mapserver;


   gsc_pgviewGetFunctionNameWithParamOnDeleteLblView(pCls, FullRefFuncName, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T("RETURNS integer AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE ");
   Stm += GS_LFSTR;
   Stm += _T("m_err_msg character varying;"); // per i messaggi di errore
   Stm += GS_LFSTR;
   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;

   Stm += _T("IF (lbl_id IS NULL) OR (entity_id IS NULL) THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label % not deleted on ");
   Stm += FullRefView;
   Stm += _T("', lbl_id; ");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;


   // Blocco la tabella della geometria, 
   // delle etichette e del raggruppamento di etichette
   if (gsc_pgviewGetLockTableStm(pCls, Stm1, false, true, true, true) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   // Cancella l'etichetta
   Stm += _T("EXECUTE 'DELETE FROM ");
   Stm += pGphInfo->LblTableRef;
   Stm += _T(" WHERE ");
   Stm += pGphInfo->key_attrib;
   Stm += _T("=$1' USING lbl_id; ");
   Stm += GS_LFSTR;

   Stm += _T("EXECUTE 'DELETE FROM ");
   Stm += pGphInfo->LblGroupingTableRef;
   Stm += _T(" WHERE ");
   Stm += pGphInfo->key_attrib;
   Stm += _T(" IN (SELECT ");
   Stm += pGphInfo->key_attrib;
   Stm += _T(" FROM ");
   Stm += pGphInfo->LblGroupingTableRef;
   Stm += _T(" AS GRP WHERE ");
   Stm += pGphInfo->ent_key_attrib;
   Stm += _T("=$1 AND ");
   Stm += pGphInfo->key_attrib;
   Stm += _T(" NOT IN (SELECT ");
   Stm += pGphInfo->ent_key_attrib;
   Stm += _T(" FROM ");
   Stm += pGphInfo->LblTableRef;
   Stm += _T(" WHERE ");
   Stm += pGphInfo->ent_key_attrib;
   Stm += _T("=GRP.");
   Stm += pGphInfo->key_attrib;
   Stm += _T("))' USING entity_id; ");
   Stm += GS_LFSTR;

   // Aggiorno l'entità e restituisco il suo codice in caso di successo
   Stm += _T("m_err_msg := ");
   Stm += FullRefFunctionRefreshEntity;
   Stm += _T("(entity_id);");
   Stm += GS_LFSTR;
   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RETURN entity_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '';");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;

   Stm += GS_LFSTR;
   Stm += _T("IF m_err_msg IS NULL THEN");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS *** Label % not deleted on ");
   Stm += FullRefView;
   Stm += _T("', lbl_id;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '%', m_err_msg;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   Stm += _T("RETURN 0; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pGphInfo->getDBConnection()->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(lbl_id integer");
   if (pInfo)
      Stm += _T(", entity_id integer");
   Stm += _T(") TO mapserver");
   
   return pGphInfo->getDBConnection()->ExeCmd(Stm);
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI E REGOLE PER LA VISTA ETICHETTE
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_ADOTypeToPLPGSQLType                               <internal> */
/*+
  Questa funzione ritorna il tipo PLPGSQL corrispondente al tipo ADO indicato.
  Parametri:
  DataTypeEnum ADOType;    Tipo di dato ADO (input)
  C_STRING &PLPGSQLType;   Tipo di dato PLPGSQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_ADOTypeToPLPGSQLType(DataTypeEnum ADOType, C_STRING &PLPGSQLType)
{
   if (gsc_DBIsBoolean(ADOType) == GS_GOOD) // Se il campo è boolenao
      { PLPGSQLType = _T("integer"); return GS_GOOD; }
   else
   if (gsc_DBIsChar(ADOType) == GS_GOOD) // Se il campo è carattere
      { PLPGSQLType = _T("character varying"); return GS_GOOD; }
   else
   if (gsc_DBIsNumeric(ADOType) == GS_GOOD) // Se il campo è numerico
      { PLPGSQLType = _T("double precision"); return GS_GOOD; }
   else
   if (gsc_DBIsDate(ADOType) == GS_GOOD) // Se il campo è data 
      { PLPGSQLType = _T("date"); return GS_GOOD; }
   else
   if (gsc_DBIsTimestamp(ADOType) == GS_GOOD) // Se il campo è timestamp 
      { PLPGSQLType = _T("timestamp"); return GS_GOOD; }
   else
   if (gsc_DBIsTime(ADOType) == GS_GOOD) // Se il campo è data 
      { PLPGSQLType = _T("time"); return GS_GOOD; }

   GS_ERR_COD = eGSInvAttribType;
   return GS_BAD;
}


/*************************************************************************/
/*.doc gsc_AttribNameToPLPGSQLVariable                        <internal> */
/*+
  Questa funzione ritorna il nome di una variabile PLPGSQL corrispondente 
  al nome dell'attributo indicato.
  Parametri:
  C_STRING &AttribName;       Nome dell'attributo (input)
  C_STRING &PLPGSQLVariable;  Nome della variabile PLPGSQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_AttribNameToPLPGSQLVariable(C_STRING &AttribName, C_STRING &PLPGSQLVariable)
{
   TCHAR  insieme_non_valido[] = _T(" ;*+-'/()\"");
   size_t i, len;
   
   PLPGSQLVariable = _T("plpgsql_var_");
   PLPGSQLVariable += AttribName;
   len = PLPGSQLVariable.len();   
   
   // correggo il nome sostituendo i caratteri non validi con _
   for (i = 0; i < len; i++)
      if (wcschr(insieme_non_valido, PLPGSQLVariable.get_chr(i)) != NULL)
         PLPGSQLVariable.set_chr(_T('_'), i);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_FuncFromGphToPLPGSQLFunc                           <internal> */
/*+
  Questa funzione ritorna il nome di una variabile PLPGSQL corrispondente 
  al nome dell'attributo indicato.
  Parametri:
  C_CLASS *pCls;              Puntatore a classe GEOsim (input)
  TCHAR *calc_func;           Funzione di calcolo dalla grafica (input)
  C_STRING &PLPGSQLVariable;  Nome della variabile PLPGSQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_FuncFromGphToPLPGSQLFunc(C_CLASS *pCls, TCHAR *calc_func, C_STRING &PLPGSQLFunc)
{
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

   if (gsc_strcmp(calc_func, GS_LISP_LENGTH, FALSE) == 0) // Lunghezza totale
   {
      if (pGphInfo->geom_attrib.len() == 0) { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      PLPGSQLFunc = _T("SUM(ST_LENGTH(");
      PLPGSQLFunc += pGphInfo->geom_attrib;
      PLPGSQLFunc += _T("))");
   }
   else
   if (gsc_strcmp(calc_func, GS_LISP_AREA, FALSE) == 0) // Area totale
   {
      if (pGphInfo->geom_attrib.len() == 0) { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      PLPGSQLFunc = _T("SUM(ST_AREA(");
      PLPGSQLFunc += pGphInfo->geom_attrib;
      PLPGSQLFunc += _T("))");
   }

   else
   if (gsc_strcmp(calc_func, GS_LISP_COLOR, FALSE) == 0) // Colore
   {
      if (pGphInfo->color_attrib.len() == 0) { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      PLPGSQLFunc = pGphInfo->color_attrib;
   }

   else
   if (gsc_strcmp(calc_func, GS_LISP_BLOCKNAME, FALSE) == 0) // Nome del blocco
   {
      if (pGphInfo->block_attrib.len() == 0) { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      PLPGSQLFunc = pGphInfo->block_attrib;
   }

   else
   if (gsc_strcmp(calc_func, GS_LISP_INS_POINT, FALSE) == 0) // Punto di inserimento
   {
      if (pGphInfo->geom_attrib.len() == 0)
      {
         PLPGSQLFunc = _T("CAST(");
         PLPGSQLFunc += pGphInfo->x_attrib;
         PLPGSQLFunc += _T(" AS text) || ',' || CAST(");
         PLPGSQLFunc += pGphInfo->y_attrib;
         PLPGSQLFunc += _T(" AS text)");
         if (pGphInfo->geom_dim == GS_3D && pGphInfo->z_attrib.len() > 0)
         {
            PLPGSQLFunc += _T(" || ',' || CAST(");
            PLPGSQLFunc += pGphInfo->z_attrib;
            PLPGSQLFunc += _T(" AS text)");
         }
      }
      else
      {
         PLPGSQLFunc = _T("CAST(ST_X(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(") AS text) || ',' || CAST(ST_Y(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(") AS text) || ',' || CAST(ST_Z(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(") AS text)");
      }
   }
   else
   if (gsc_strcmp(calc_func, GS_LISP_X_INS_POINT, FALSE) == 0) // X punto di inserimento
      if (pGphInfo->geom_attrib.len() == 0)
         PLPGSQLFunc = pGphInfo->x_attrib;
      else
      {
         PLPGSQLFunc = _T("ST_X(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Y_INS_POINT, FALSE) == 0) // Y punto di inserimento
      if (pGphInfo->geom_attrib.len() == 0)
         PLPGSQLFunc = pGphInfo->y_attrib;
      else
      {
         PLPGSQLFunc = _T("ST_Y(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Z_INS_POINT, FALSE) == 0) // Z punto di inserimento
      if (pGphInfo->geom_attrib.len() == 0)
         if (pGphInfo->geom_dim == GS_3D && pGphInfo->z_attrib.len() > 0)
            PLPGSQLFunc = pGphInfo->z_attrib;
         else
            { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_Z(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")");
      }

   else
   if (gsc_strcmp(calc_func, GS_LISP_SPOINT, FALSE) == 0) // Punto iniziale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("CAST(ST_X(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Y(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Z(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text)");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_X_SPOINT, FALSE) == 0) // X punto iniziale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_X(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Y_SPOINT, FALSE) == 0) // Y punto iniziale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_Y(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Z_SPOINT, FALSE) == 0) // Z punto iniziale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_Z(ST_STARTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }

   else
   if (gsc_strcmp(calc_func, GS_LISP_EPOINT, FALSE) == 0) // Punto finale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("CAST(ST_X(ST_ENDPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Y(ST_ENDPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Z(ST_ENDPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text)");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_X_EPOINT, FALSE) == 0) // X punto finale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_X(ST_ENDPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Y_EPOINT, FALSE) == 0) // Y punto finale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_Y(ST_ENDPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_Z_EPOINT, FALSE) == 0) // Z punto finale
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("ST_Z(ST_ENDTPOINT(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))");
      }

   else
   if (gsc_strcmp(calc_func, GS_LISP_CENTROID, FALSE) == 0) // Punto centroide
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("CAST(ST_X(ST_CENTROID(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Y(ST_CENTROID(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text) || ',' || CAST(ST_Z(ST_CENTROID(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T(")) AS text)");
      }

   else
   if (gsc_strcmp(calc_func, GS_LISP_START_WIDTH, FALSE) == 0) // Larghezza iniziale
      if (pGphInfo->width_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->width_attrib;     
   else
   if (gsc_strcmp(calc_func, GS_LISP_END_WIDTH, FALSE) == 0) // Larghezza finale
      if (pGphInfo->width_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->width_attrib;     

   else
   if (gsc_strcmp(calc_func, GS_LISP_LAYER, FALSE) == 0) // Nome layer
      if (pGphInfo->layer_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->layer_attrib;     

   else
   if (gsc_strcmp(calc_func, GS_LISP_HEIGHT, FALSE) == 0) // Altezza testo
      if (pGphInfo->h_text_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->h_text_attrib;     
   else
   if (gsc_strcmp(calc_func, GS_LISP_STYLE, FALSE) == 0) // Nome dello stile testo
      if (pGphInfo->text_style_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->text_style_attrib;     

   else
   if (gsc_strcmp(calc_func, GS_LISP_ELEVATION, FALSE) == 0) // Elevazione
      if (pCls->get_type() == TYPE_NODE || pCls->get_type() == TYPE_TEXT)
      {
         if (pGphInfo->geom_attrib.len() == 0)
            if (pGphInfo->geom_dim == GS_3D && pGphInfo->z_attrib.len() > 0)
               PLPGSQLFunc = pGphInfo->z_attrib;     
            else 
               { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
         else
         {
            PLPGSQLFunc = _T("ST_Z(");
            PLPGSQLFunc += pGphInfo->geom_attrib;
            PLPGSQLFunc += _T(")");
         }
      }
      else
         if (pGphInfo->geom_attrib.len() == 0)
            { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
         else
         {
            PLPGSQLFunc = _T("ST_Z(ST_STARTPOINT(");
            PLPGSQLFunc += pGphInfo->geom_attrib;
            PLPGSQLFunc += _T("))");
         }

   else
   if (gsc_strcmp(calc_func, GS_LISP_THICKNESS, FALSE) == 0) // Spessore della linea
      if (pGphInfo->thickness_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->thickness_attrib;     
   else
   if (gsc_strcmp(calc_func, GS_LISP_LINETYPE, FALSE) == 0) // Nome del tipo di linea
      if (pGphInfo->line_type_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
         PLPGSQLFunc = pGphInfo->line_type_attrib;     

   else
   if (gsc_strcmp(calc_func, GS_LISP_ROTATION, FALSE) == 0) // Rotazione in radianti senso antiorario
      if (pGphInfo->rotation_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else // vedi funzione CounterClockwiseRadiantToRotationUnit
         switch (pGphInfo->rotation_unit)
         {
            case GSClockwiseDegreeUnit: // cambio unità (gradi->rad) e verso
               PLPGSQLFunc = _T("(2*PI()-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("PI()/180))");
               break;
            case GSCounterClockwiseDegreeUnit: // cambio unità (gradi->rad)
               PLPGSQLFunc = _T("(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("PI()/180)");
               break;
            case GSClockwiseRadiantUnit: // cambio verso
               PLPGSQLFunc = _T("(2*PI()-");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T(")");
               break;
            case GSNoneRotationUnit:
            case GSCounterClockwiseRadiantUnit: // senza conversione
               PLPGSQLFunc = pGphInfo->rotation_attrib;
               break;
            case GSClockwiseGonsUnit: // cambio unità e verso
               PLPGSQLFunc = _T("(2*PI()-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100))");
               break;
            case GSCounterClockwiseGonsUnit: // cambio unità (gons->rad)
               PLPGSQLFunc = _T("((");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100))");
               break;
            case GSTopobaseGonsUnit: // cambio unità e verso
               PLPGSQLFunc = _T("(2*PI()-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100)+100)");
               break;
            default:
               { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
         }
   else
   if (gsc_strcmp(calc_func, GS_LISP_ROTATION_DEGREE, FALSE) == 0) // Rotazione in gradi senso antiorario
      if (pGphInfo->rotation_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      { 
         switch (pGphInfo->rotation_unit)
         {
            case GSClockwiseDegreeUnit: // cambio verso
               PLPGSQLFunc = _T("(360-");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T(")");
               break;
            case GSNoneRotationUnit:
            case GSCounterClockwiseDegreeUnit: // senza conversione
               PLPGSQLFunc = pGphInfo->rotation_attrib;
               break;
            case GSClockwiseRadiantUnit: // cambio unità (rad->gradi) e verso
               PLPGSQLFunc = _T("(360-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*180/PI()))");
               break;
            case GSCounterClockwiseRadiantUnit: // cambio unità (rad->gradi)
               PLPGSQLFunc = _T("(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*180/PI())");
               break;
            case GSClockwiseGonsUnit: // cambio unità e verso
               PLPGSQLFunc = _T("(360-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100)");
               PLPGSQLFunc += _T("*180/PI())");
               break;
            case GSCounterClockwiseGonsUnit: // cambio unità (gons->rad->gradi)
               PLPGSQLFunc = _T("((");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100)");
               PLPGSQLFunc += _T("*180/PI())");
               break;
            case GSTopobaseGonsUnit: // cambio unità e verso
               PLPGSQLFunc = _T("(360-(");
               PLPGSQLFunc += pGphInfo->rotation_attrib;
               PLPGSQLFunc += _T("*(PI()/2)/100)");
               PLPGSQLFunc += _T("*180/PI()+90)");
               break;
            default:
               { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
         }
      }
   else
   if (gsc_strcmp(calc_func, GS_LISP_WINDOW, FALSE) == 0) //  Finestra contenente l'entità
      if (pGphInfo->geom_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; }
      else
      {
         PLPGSQLFunc = _T("CAST(MIN(XMIN(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text) || '','' || CAST(MIN(YMIN(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text) || '','' || CAST(MIN(ZMIN(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text) || '';'' || ");

         PLPGSQLFunc += _T("CAST(MAX(XMAX(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text) || '','' || CAST(MAX(YMAX(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text) || '','' || CAST(MAX(ZMAX(BOX3D(");
         PLPGSQLFunc += pGphInfo->geom_attrib;
         PLPGSQLFunc += _T("))) AS text)");
      }

   else
   if (gsc_strcmp(calc_func, GS_LISP_INITIAL_NODE, FALSE) == 0) // Codice nodo iniziale (info topologica)
      { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; } // da fare
   else
   if (gsc_strcmp(calc_func, GS_LISP_FINAL_NODE, FALSE) == 0) // Codice nodo finale (info topologica)
      { GS_ERR_COD = eGSInvalidCalc; return GS_BAD; } // da fare
   else
      return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_LispParserToPLPGSQL                                <internal> */
/*+
  Questa funzione ritorna il nome di una variabile PLPGSQL corrispondente 
  al nome dell'attributo indicato.
  Parametri:
  TCHAR *Lisp;          Funzione lisp (input)
  C_STRING &PLPGSQL;    Codice scritto PLPGSQL (output)
  C_CLASS *pCls;        Opzionale, puntatore alla classe (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_LispParserToPLPGSQL(C_STR_STRLIST *pParse, C_STRING &PLPGSQL, C_CLASS *pCls)
{
   C_STR_STRLIST *p;
   C_STRING      Token;
   C_ATTRIB_LIST *pAttribList = pCls->ptr_attrib_list();
   C_ATTRIB      *pAttrib;

   if (!pParse) return GS_GOOD;

   Token = pParse->get_name();
   if (Token.get_chr(0) != _T('(')) // se non è una funzione
   {
      if (Token.get_chr(0) == _T('"')) // se è una stringa
      {
         Token.removePrefixSuffix(_T("\""), _T("\""));
         Token.strtran(_T("\\\""), _T("\"")); //  \" diventa "
         Token.strtran(_T("'"), _T("''")); // ' diventa ''
         Token.addPrefixSuffix(_T("'"), _T("'"));
         PLPGSQL += Token;
      }
      else
      if (gsc_is_numeric(Token.get_name()) == GS_GOOD) // se è un numero
      {
         PLPGSQL += Token;
      }
      else
      if (Token.comp(_T("NIL"), FALSE) == 0)
      {
         PLPGSQL += _T("FALSE");
      }      
      else
      if (Token.comp(_T("T"), FALSE) == 0)
      {
         PLPGSQL += _T("TRUE");
      }      
      else
      if (pAttribList &&  // Se il Token è un attributo della classe
          (pAttrib = (C_ATTRIB *) pAttribList->search_name(Token.get_name(), FALSE)) != NULL)
      {
         C_STRING PLPGSQLVariable;

         // correggo il nome per fare riferimento ad una variabile PLPGSQL
         if (gsc_AttribNameToPLPGSQLVariable(Token, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;
         PLPGSQL += PLPGSQLVariable;
      }
   }
   else // si tratta di una funzione
   {
      Token.removePrefixSuffix(_T("("));

      // main
      if (Token.len() == 0)
      {  // Leggo i parametri della funzione (almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         while (p) // parametro successivo
         {
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            p = (C_STR_STRLIST *) p->get_next();
         }
      }
      else
      // Operatori logici
      if (Token.comp(_T(">"), FALSE) == 0 || Token.comp(_T(">="), FALSE) == 0 ||
          Token.comp(_T("<"), FALSE) == 0 || Token.comp(_T("<="), FALSE) == 0 ||
          Token.comp(_T("="), FALSE) == 0 || Token.comp(_T("EQ"), FALSE) == 0 ||
          Token.comp(_T("EQUAL"), FALSE) == 0 || Token.comp(_T("/="), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }

         PLPGSQL += _T("(");
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;

         // Operatore logico
         if (Token.comp(_T("/="), FALSE) == 0) PLPGSQL += _T("<>");
         else if (Token.comp(_T("EQ"), FALSE) == 0) PLPGSQL += _T("=");
         else if (Token.comp(_T("EQUAL"), FALSE) == 0) PLPGSQL += _T("=");
         else PLPGSQL += Token;

         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      // Operatori matematici
      if (Token.comp(_T("+"), FALSE) == 0 || Token.comp(_T("-"), FALSE) == 0 ||
          Token.comp(_T("*"), FALSE) == 0 || Token.comp(_T("/"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
         {
            PLPGSQL += _T(" ");
            PLPGSQL += Token;
            PLPGSQL += _T(" ");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;       
         }
         PLPGSQL += _T(")");
      }
      else
      // Operatori booleani
      if (Token.comp(_T("AND"), FALSE) == 0 || Token.comp(_T("OR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
         {
            PLPGSQL += _T(" ");
            PLPGSQL += Token;
            PLPGSQL += _T(" ");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;       
         }
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("ALLTRIM"), FALSE) == 0) // trim a sinistra e destra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("TRIM(BOTH FROM ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("ATOI"), FALSE) == 0) // conversione da carattere a intero
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("CAST(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(" AS INTEGER)");
      }
      else
      if (Token.comp(_T("CHANGEBLOCK"), FALSE) == 0 || // calcolo grafico (con 1 parametro) su tabella geometria
          Token.comp(_T("CHANGECOLOR"), FALSE) == 0 ||
          Token.comp(_T("CHANGEELEV"), FALSE) == 0 ||
          Token.comp(_T("CHANGEHTEXT"), FALSE) == 0 ||
          Token.comp(_T("CHANGELAYER"), FALSE) == 0 ||
          Token.comp(_T("CHANGELINETYPE"), FALSE) == 0 ||
          Token.comp(_T("CHANGEROTATION"), FALSE) == 0 || // questa funzione ha 2 parametri ma ne considero solo 1
          Token.comp(_T("CHANGESCALE"), FALSE) == 0 ||
          Token.comp(_T("CHANGETEXTSTYLE"), FALSE) == 0 ||
          Token.comp(_T("CHANGETHICKNESS"), FALSE) == 0 ||
          Token.comp(_T("CHANGEWIDTH"), FALSE) == 0)
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldName, Parameter;
         int          MaxStrLen = -1;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere 1 o 2 che però non considero)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         if (Token.comp(_T("CHANGEBLOCK"), FALSE) == 0)
         {
            FieldName += pGphInfo->block_attrib;
            Parameter = _T("m_block_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("CHANGECOLOR"), FALSE) == 0)
         {
            FieldName += pGphInfo->color_attrib;
            Parameter = _T("m_color");
         }
         else if (Token.comp(_T("CHANGEELEV"), FALSE) == 0)
         {
            FieldName += pGphInfo->geom_attrib;
            Parameter = _T("m_elevation");
         }
         else if (Token.comp(_T("CHANGEHTEXT"), FALSE) == 0)
         {
            FieldName += pGphInfo->h_text_attrib;
            Parameter = _T("m_h_text");
         }
         else if (Token.comp(_T("CHANGELAYER"), FALSE) == 0)
         {
            FieldName += pGphInfo->layer_attrib;
            Parameter = _T("m_layer_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("CHANGELINETYPE"), FALSE) == 0)
         {
            FieldName += pGphInfo->line_type_attrib;
            Parameter = _T("m_linetype_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("CHANGEROTATION"), FALSE) == 0)
         {
            FieldName += pGphInfo->rotation_attrib;
            Parameter = _T("m_rotation");
         }
         else if (Token.comp(_T("CHANGESCALE"), FALSE) == 0)
         {
            if (pCls->get_type() == TYPE_NODE) // i testi non usano la scala ma l'altezza testo
               FieldName += pGphInfo->block_scale_attrib;
            else
               FieldName += pGphInfo->line_type_scale_attrib;

            Parameter = _T("m_scale");
         }
         else if (Token.comp(_T("CHANGETEXTSTYLE"), FALSE) == 0)
         {
            FieldName += pGphInfo->text_style_attrib;
            Parameter = _T("m_textstyle_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("CHANGETHICKNESS"), FALSE) == 0)
         {
            FieldName += pGphInfo->thickness_attrib;
            Parameter = _T("m_thickness");
         }
         else if (Token.comp(_T("CHANGEWIDTH"), FALSE) == 0)
         {
            FieldName += pGphInfo->width_attrib;
            Parameter = _T("m_width");
         }

         PLPGSQL += Parameter;
         PLPGSQL += _T(" := ");
         if (Token.comp(_T("CHANGEELEV"), FALSE) == 0)
         {
            PLPGSQL += _T("ST_TRANSLATE(");
            PLPGSQL += pGphInfo->geom_attrib;
            PLPGSQL += _T(",0,0,");
         }
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         if (Token.comp(_T("CHANGEELEV"), FALSE) == 0) PLPGSQL += _T(")");
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->TableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += FieldName;
         PLPGSQL += _T("=$1 WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$2' USING ");

         if (MaxStrLen > 0) // Se il campo è carattere
         { // per sicurezza correggo le dimensioni dei valori
            PLPGSQL += _T("SUBSTRING(");
            PLPGSQL += Parameter;
            PLPGSQL += _T(" FOR ");
            PLPGSQL += MaxStrLen; // dimensione del campo
            PLPGSQL += _T(")");
         }
         else
            PLPGSQL += Parameter;

         PLPGSQL += _T(", entity_id;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("CHANGECOLORATTR"), FALSE) == 0 || // calcolo grafico attributi (con 1 parametro) su tabella geometria attributi
          Token.comp(_T("CHANGEHTEXTATTR"), FALSE) == 0 ||
          Token.comp(_T("CHANGELAYERATTR"), FALSE) == 0 ||
          Token.comp(_T("CHANGETEXTSTYLEATTR"), FALSE) == 0 ||
          Token.comp(_T("VISATTR"), FALSE) == 0)
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldName, Parameter;
         int          MaxStrLen = -1;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (devono essere 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         if (Token.comp(_T("CHANGECOLORATTR"), FALSE) == 0)
         {
            FieldName += pGphInfo->color_attrib;
            Parameter = _T("m_attr_color");
         }
         else if (Token.comp(_T("CHANGEHTEXTATTR"), FALSE) == 0)
         {
            FieldName += pGphInfo->h_text_attrib;
            Parameter = _T("m_attr_h_text");
         }
         else if (Token.comp(_T("CHANGELAYERATTR"), FALSE) == 0)
         {
            FieldName += pGphInfo->layer_attrib;
            Parameter = _T("m_attr_layer_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("CHANGETEXTSTYLEATTR"), FALSE) == 0)
         {
            FieldName += pGphInfo->text_style_attrib;
            Parameter = _T("m_attr_textstyle_name");
            MaxStrLen = 255;
         }
         else if (Token.comp(_T("VISATTR"), FALSE) == 0)
         {
            FieldName += pGphInfo->attrib_invis_attrib;
            Parameter = _T("m_attr_invisibility");
         }

         PLPGSQL += _T("m_attr_name := "); // nome dell'attributo
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         PLPGSQL += Parameter; // secondo parametro
         PLPGSQL += _T(" := ");
         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->LblTableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += FieldName;
         PLPGSQL += _T("=$1 WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$2 AND ");
         PLPGSQL += pGphInfo->attrib_name_attrib;         
         PLPGSQL += _T("=$3' USING ");

         if (MaxStrLen > 0) // Se il campo è carattere
         { // per sicurezza correggo le dimensioni dei valori
            PLPGSQL += _T("SUBSTRING(");
            PLPGSQL += Parameter;
            PLPGSQL += _T(" FOR ");
            PLPGSQL += MaxStrLen; // dimensione del campo
            PLPGSQL += _T(")");
         }
         else
            PLPGSQL += Parameter;

         PLPGSQL += _T(", entity_id, m_attr_name;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("CHANGEHATCH"), FALSE) == 0)
      { // calcolo grafico (con 1 parametro obbligatorio e 5 opzionali) su tabella geometria
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldNames, Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("m_hatch_name := ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;
         FieldNames = pGphInfo->hatch_attrib;
         FieldNames +=  _T("=$1");

         // per sicurezza correggo le dimensioni dei valori
         Parameters = _T("SUBSTRING(m_hatch_name FOR 255)"); // dimensione del campo

         if ((p = (C_STR_STRLIST *) p->get_next())) // secondo parametro
         {
            PLPGSQL += _T("m_hatch_color := ");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            PLPGSQL += _T(";");
            PLPGSQL += GS_LFSTR;
            FieldNames +=  _T(", ");
            FieldNames += pGphInfo->hatch_color_attrib;
            FieldNames +=  _T("=$2");
            Parameters += _T(",m_hatch_color");
            if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
            {
               PLPGSQL += _T("m_hatch_scale := ");
               if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
               PLPGSQL += _T(";");
               PLPGSQL += GS_LFSTR;
               FieldNames +=  _T(", ");
               FieldNames += pGphInfo->hatch_scale_attrib;
               FieldNames +=  _T("=$3");
               Parameters += _T(",m_hatch_scale");
               if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
               {
                  PLPGSQL += _T("m_hatch_rotation := ");
                  if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                  PLPGSQL += _T(";");
                  PLPGSQL += GS_LFSTR;
                  FieldNames +=  _T(", ");
                  FieldNames += pGphInfo->hatch_rotation_attrib;
                  FieldNames +=  _T("=$4");
                  Parameters += _T(",m_hatch_rotation");
                  if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
                  {
                     PLPGSQL += _T("m_hatch_rotation_unit := "); // per ora non lo considero
                     if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                     PLPGSQL += _T(";");
                     PLPGSQL += GS_LFSTR;
                     if ((p = (C_STR_STRLIST *) p->get_next())) // sesto parametro
                     {
                        PLPGSQL += _T("m_hatch_layer_name := ");
                        if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                        PLPGSQL += _T(";");
                        PLPGSQL += GS_LFSTR;
                        FieldNames +=  _T(", ");
                        FieldNames += pGphInfo->hatch_layer_attrib;
                        FieldNames +=  _T("=$5, ");
                        // per sicurezza correggo le dimensioni dei valori
                        Parameters += _T(",SUBSTRING(m_hatch_layer_name FOR 255)"); // dimensione del campo
                     }
                  }
               }
            }
         }

         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->TableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += FieldNames;
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$");
         PLPGSQL += pParse->ptr_Str_StrList()->get_count() + 1;
         PLPGSQL += _T("' USING ");
         PLPGSQL += Parameters;
         PLPGSQL += _T(", entity_id;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("IF"), FALSE) == 0)
      {
         PLPGSQL += _T("IF ");
         // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro (condizione)
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(" THEN");
         PLPGSQL += GS_LFSTR;
         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro (se la condizione è vera)
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         p = (C_STR_STRLIST *) p->get_next(); // terzo parametro (se la condizione è false); opzionale
         if (p)
         {
            PLPGSQL += _T("\nELSE\n");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         }
         PLPGSQL += _T("\nEND IF;\n");
      }
      else
      if (Token.comp(_T("ITOA"), FALSE) == 0) // conversione da intero ad carattere
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("CAST(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(" AS TEXT)");
      }
      else
      if (Token.comp(_T("LTRIM"), FALSE) == 0) // trim a sinistra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("TRIM(LEADING FROM ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("MOVEATTR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere almeno 2)
   	   // (moveattr nomeattr modalità [offset1[offset2[Flag_generaz
         // [allineam_oriz[allineam_vert[controllo_rotz[controllo_sovrapposiz]]]]]]])
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (devono essere almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         PLPGSQL += _T("m_attr_name := "); // nome dell'attributo
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         PLPGSQL += _T("m_moving_mode := "); // modalità di posizionamento
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
         {
            PLPGSQL += _T("m_moving_offset1 := ");  // offset1
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            PLPGSQL += _T(";");
            PLPGSQL += GS_LFSTR;
            Parameters += _T(",m_moving_offset1");

            if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
            {
               PLPGSQL += _T("m_moving_offset2 := ");  // offset2
               if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
               PLPGSQL += _T(";");
               PLPGSQL += GS_LFSTR;
               Parameters += _T(",m_moving_offset2");

               if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
               {
                  PLPGSQL += _T("m_moving_generating_flag := ");  // Flag_generaz
                  if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                  PLPGSQL += _T(";");
                  PLPGSQL += GS_LFSTR;
                  //Parameters += _T(",m_moving_generating_flag"); non usato

                  if ((p = (C_STR_STRLIST *) p->get_next())) // sesto parametro
                  {
                     PLPGSQL += _T("m_moving_horiz_align := ");  // allineamento orizzontale
                     if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                     PLPGSQL += _T(";");
                     PLPGSQL += GS_LFSTR;
                     //Parameters += _T(",m_moving_horiz_align"); non usato

                     if ((p = (C_STR_STRLIST *) p->get_next())) // settimo parametro
                     {
                        PLPGSQL += _T("m_moving_vert_align := ");  // allineam_vert
                        if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                        PLPGSQL += _T(";");
                        PLPGSQL += GS_LFSTR;
                        //Parameters += _T(",m_moving_vert_align"); non usato

                        if ((p = (C_STR_STRLIST *) p->get_next())) // ottavo parametro
                        {
                           PLPGSQL += _T("m_moving_rot_ctrl := "); // controllo di rotazione
                           if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                           PLPGSQL += _T(";");
                           PLPGSQL += GS_LFSTR;
                           Parameters += _T(",m_moving_rot_ctrl");

                           if ((p = (C_STR_STRLIST *) p->get_next())) // nono parametro
                           {
                              PLPGSQL += _T("m_moving_overlap_ctrl := ");  // controllo di sovrapposizione
                              if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                              PLPGSQL += _T(";");
                              PLPGSQL += GS_LFSTR;
                              //Parameters += _T(",m_moving_overlap_ctrl"); non usato
                           }
                        }
                     }
                  }
               }
            }
         }

         // leggo la geometria dell'entità
         PLPGSQL += _T("EXECUTE 'SELECT ");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T(" FROM ");
         PLPGSQL += pGphInfo->TableRef;
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$1 LIMIT 1' INTO m_geom");
         if (pCls->get_type() == TYPE_NODE || pCls->get_type() == TYPE_TEXT)
            PLPGSQL += _T(",m_rotation");
         PLPGSQL += _T(" USING entity_id;");
         PLPGSQL += GS_LFSTR;

         // calcolo la posizione
         PLPGSQL += _T("SELECT pos, rot FROM gs_get_position(m_geom,m_moving_mode,m_rotation");
         PLPGSQL += Parameters;
         PLPGSQL += _T(") INTO m_position,m_rotation;");
         PLPGSQL += GS_LFSTR;

         // aggiorno la geometria dell'etichetta
         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->LblTableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += pGphInfo->geom_attrib;
         if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
         {
            PLPGSQL += _T("=$1, ");
            PLPGSQL += pGphInfo->rotation_attrib;
            PLPGSQL += _T("=$2");
         }

         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;

         if (pGphInfo->LblGroupingTableRef.len() > 0)
         {
            PLPGSQL += _T(" IN (SELECT ");
            PLPGSQL += pGphInfo->key_attrib;
            PLPGSQL += _T(" FROM ");
            PLPGSQL += pGphInfo->LblGroupingTableRef;
            PLPGSQL += _T(" WHERE ");
            PLPGSQL += pGphInfo->ent_key_attrib;
            if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
               PLPGSQL += _T("=$3)");
            else
               PLPGSQL += _T("=$2)");
         }
         else
            if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
               PLPGSQL += _T("=$3");
            else
               PLPGSQL += _T("=$2)");

         PLPGSQL += _T(" AND ");
         PLPGSQL += pGphInfo->attrib_name_attrib;         
         if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
            PLPGSQL += _T("=$4' USING m_position, m_rotation, entity_id, m_attr_name;");
         else
            PLPGSQL += _T("=$3' USING m_position, entity_id, m_attr_name;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("MOVESHEET"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere almeno 1)
      	// (movesheet modalità [offset1[offset2[controllo_rotz[controllo_sovrapposiz]]]])
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         if (pGphInfo->LblGroupingTableRef.len() == 0) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

         // Leggo i parametri della funzione (devono essere almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }

         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("m_moving_mode := ");  // modalità
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         if ((p = (C_STR_STRLIST *) p->get_next())) // secondo parametro
         {
            PLPGSQL += _T("m_moving_offset1 := ");  // offset1
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            PLPGSQL += _T(";");
            PLPGSQL += GS_LFSTR;
            Parameters += _T(",m_moving_offset1");
            if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
            {
               PLPGSQL += _T("m_moving_offset2 := ");  // offset2
               if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
               PLPGSQL += _T(";");
               PLPGSQL += GS_LFSTR;
               Parameters += _T(",m_moving_offset2");
               if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
               {
                  PLPGSQL += _T("m_moving_rot_ctrl := ");  // controllo_rotz
                  if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                  PLPGSQL += _T(";");
                  PLPGSQL += GS_LFSTR;
                  Parameters += _T(",m_moving_rot_ctrl");
                  if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
                  {
                     PLPGSQL += _T("m_moving_overlap_ctrl := ");  // controllo_sovrapposiz
                     if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
                     PLPGSQL += _T(";");
                     PLPGSQL += GS_LFSTR;
                     //Parameters += _T(",m_moving_overlap_ctrl"); non usato
                  }
               }
            }
         }

         // leggo la geometria dell'entità
         PLPGSQL += _T("EXECUTE 'SELECT ");
         PLPGSQL += pGphInfo->geom_attrib;
         if (pCls->get_type() == TYPE_NODE || pCls->get_type() == TYPE_TEXT)
         {
            PLPGSQL += _T(",");
            PLPGSQL += pGphInfo->rotation_attrib;
         }
         PLPGSQL += _T(" FROM ");
         PLPGSQL += pGphInfo->TableRef;
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$1 LIMIT 1' INTO m_geom");
         if (pCls->get_type() == TYPE_NODE || pCls->get_type() == TYPE_TEXT)
            PLPGSQL += _T(",m_rotation");
         PLPGSQL += _T(" USING entity_id;");
         PLPGSQL += GS_LFSTR;
         
         // calcolo la posizione e la rotazione
         PLPGSQL += _T("SELECT pos, rot FROM gs_get_position(m_geom,m_moving_mode,m_rotation");
         PLPGSQL += Parameters;
         PLPGSQL += _T(") INTO m_position,m_rotation;");
         PLPGSQL += GS_LFSTR;

         // calcolo lo spostamento del solo primo gruppo di etichette rispetto la posizione precedente
         PLPGSQL += _T("EXECUTE 'SELECT ' || ST_X(m_position)::text || ' - ST_X(");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T("), ' || ST_Y(m_position) || ' - ST_Y(");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T(") FROM ");
         PLPGSQL += pGphInfo->LblGroupingTableRef;
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$1 LIMIT 1' INTO m_moving_offset1, m_moving_offset2 USING entity_id;");
         PLPGSQL += GS_LFSTR;

         // aggiorno la geometria del gruppo di etichette
         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->LblGroupingTableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T("=ST_TRANSLATE(");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T(", $1, $2, 0)");
         if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
         {
            PLPGSQL += _T(", ");
            PLPGSQL += pGphInfo->rotation_attrib;
            PLPGSQL += _T("=$3 WHERE ");
            PLPGSQL += pGphInfo->ent_key_attrib;
            PLPGSQL += _T("=$4' USING m_moving_offset1, m_moving_offset2, m_rotation, entity_id;");
         }
         else
         {
            PLPGSQL += _T(" WHERE ");
            PLPGSQL += pGphInfo->ent_key_attrib;
            PLPGSQL += _T("=$3' USING m_moving_offset1, m_moving_offset2, entity_id;");
         }
         PLPGSQL += GS_LFSTR;

         // aggiorno la geometria dell'etichetta
         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->LblTableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T("=ST_TRANSLATE(");
         PLPGSQL += pGphInfo->geom_attrib;
         PLPGSQL += _T(", $1, $2, 0)");
         if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
         {
            PLPGSQL += _T(", ");
            PLPGSQL += pGphInfo->rotation_attrib;
            PLPGSQL += _T("=$3");
         }
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T(" IN (SELECT ");
         PLPGSQL += pGphInfo->key_attrib;
         PLPGSQL += _T(" FROM ");
         PLPGSQL += pGphInfo->LblGroupingTableRef;
         PLPGSQL += _T(" WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         if (pGphInfo->rotation_attrib.len() > 0) // aggiorno anche la rotazione
            PLPGSQL += _T("=$4)' USING m_moving_offset1, m_moving_offset2, m_rotation, entity_id;");
         else
            PLPGSQL += _T("=$3)' USING m_moving_offset1, m_moving_offset2, entity_id;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("NULL"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(" IS NULL");
      }
      else
      if (Token.comp(_T("TRIM"), FALSE) == 0) // trim a destra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("TRIM(TRAILING FROM ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("SETQ"), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere pari)
         if (pParse->ptr_Str_StrList() == NULL || (pParse->ptr_Str_StrList()->get_count() % 2) == 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         while (p) // parametro successivo
         {
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            PLPGSQL += _T(" := ");
            p = (C_STR_STRLIST *) p->get_next();
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
            PLPGSQL += _T(";\n");
            p = (C_STR_STRLIST *) p->get_next();
         }
      }
      else
      if (Token.comp(_T("STRCASE"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("UPPER(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("STRCAT"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
         {
            PLPGSQL += _T(" || ");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         }
      }
      else
      if (Token.comp(_T("STRLEN"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("LENGTH(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("SUBSTR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("SUBSTRING(");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         p = (C_STR_STRLIST *) p->get_next();
         PLPGSQL += _T(" FROM ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         if ((p = (C_STR_STRLIST *) p->get_next()))
         {
            PLPGSQL += _T(" FOR ");
            if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         }
         PLPGSQL += _T(")");
      }
      else
      if (Token.comp(_T("VISSHEET"), FALSE) == 0) // calcolo grafico attributi (con 1 parametro) su tabella geometria attributi
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         PLPGSQL += _T("m_attr_invisibility := ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(";");
         PLPGSQL += GS_LFSTR;

         PLPGSQL += _T("EXECUTE 'UPDATE ");
         PLPGSQL += pGphInfo->LblTableRef;
         PLPGSQL += _T(" SET ");
         PLPGSQL += pGphInfo->attrib_invis_attrib;
         PLPGSQL += _T("=$1 WHERE ");
         PLPGSQL += pGphInfo->ent_key_attrib;
         PLPGSQL += _T("=$2' USING m_attr_invisibility, entity_id;");
         PLPGSQL += GS_LFSTR;
      }
      else
      if (Token.comp(_T("YEAR"), FALSE) == 0) // estrazione dell'anno da una data
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         PLPGSQL += _T("DATE_PART('YEAR', ");
         if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
         PLPGSQL += _T(")");
      }
      else // Funzione utente
      {
         PLPGSQL += Token;
         PLPGSQL += _T("(");
         if (pParse->ptr_Str_StrList())
         {
            p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // ciclo sui parametri
            while (p)
            {
               if (gsc_LispParserToPLPGSQL(p, PLPGSQL, pCls) == GS_BAD) return GS_BAD;
               if ((p = (C_STR_STRLIST *) p->get_next())) PLPGSQL += _T(",");
            }
         }
         PLPGSQL += _T(")");
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_LispParserToPLPGSQL                                <internal> */
/*+
  Questa funzione ritorna delle liste di variabili da dichiarare dal codice PLPGSQL
  risultato dalla traduzione del LISP in PLPGSQL.
  Parametri:
  TCHAR *Lisp;                Funzione lisp (input)
  C_STR_LIST &IntegerVarList; Lista delle varibili intere da dichiarare nel codice PLPGSQL
  C_STR_LIST &DoubleVarList;  Lista delle varibili double da dichiarare nel codice PLPGSQL
  C_STR_LIST &CharVarList;    Lista delle varibili char da dichiarare nel codice PLPGSQL
  C_STR_LIST &GeomVarList;    Lista delle varibili geometry da dichiarare nel codice PLPGSQL
  C_STR_LIST &BoolVarList;    Lista delle varibili booleane da dichiarare nel codice PLPGSQL
  C_STR_LIST &DateVarList;    Lista delle varibili date da dichiarare nel codice PLPGSQL
  C_STR_LIST &TimestampVarList; Lista delle varibili timestamp da dichiarare nel codice PLPGSQL
  C_CLASS *pCls;              Opzionale, puntatore alla classe (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_LispParserToPLPGSQL(C_STR_STRLIST *pParse, 
                            C_STR_LIST &IntegerVarList, C_STR_LIST &DoubleVarList,
                            C_STR_LIST &CharVarList, C_STR_LIST &GeomVarList,
                            C_STR_LIST &BoolVarList, C_STR_LIST &DateVarList,
                            C_STR_LIST &TimestampVarList,
                            C_CLASS *pCls)
{
   C_STR_STRLIST *p;
   C_STRING      Token, PLPGSQLVariable;
   C_ATTRIB_LIST *pAttribList = pCls->ptr_attrib_list();
   C_ATTRIB      *pAttrib;
   
   if (!pParse) return GS_GOOD;

   Token = pParse->get_name();
   if (Token.get_chr(0) != _T('(')) // se non è una funzione
   {
      if (Token.get_chr(0) == _T('"')) // se è una stringa
      {}
      else
      if (gsc_is_numeric(Token.get_name()) == GS_GOOD) // se è un numero
      {}
      else
      if (Token.comp(_T("NIL"), FALSE) == 0)
      {}      
      else
      if (Token.comp(_T("T"), FALSE) == 0)
      {}      
      else
      if (pAttribList &&  // Se il Token è un attributo della classe
          (pAttrib = (C_ATTRIB *) pAttribList->search_name(Token.get_name(), FALSE)) != NULL)
      {
         // correggo il nome per fare riferimento ad una variabile PLPGSQL
         if (gsc_AttribNameToPLPGSQLVariable(Token, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;
         gsc_LispAddVarToListVarPLPGSQL(PLPGSQLVariable, pAttrib->ADOType,
                                        IntegerVarList, DoubleVarList, CharVarList,
                                        GeomVarList, BoolVarList, DateVarList, TimestampVarList);
      }
   }
   else // si tratta di una funzione
   {
      Token.removePrefixSuffix(_T("("));

      // main
      if (Token.len() == 0)
      {  // Leggo i parametri della funzione (almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         while (p) // parametro successivo
         {
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
            p = (C_STR_STRLIST *) p->get_next();
         }
      }
      else
      // Operatori logici
      if (Token.comp(_T(">"), FALSE) == 0 || Token.comp(_T(">="), FALSE) == 0 ||
          Token.comp(_T("<"), FALSE) == 0 || Token.comp(_T("<="), FALSE) == 0 ||
          Token.comp(_T("="), FALSE) == 0 || Token.comp(_T("EQ"), FALSE) == 0 ||
          Token.comp(_T("EQUAL"), FALSE) == 0 || Token.comp(_T("/="), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }

         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      // Operatori matematici
      if (Token.comp(_T("+"), FALSE) == 0 || Token.comp(_T("-"), FALSE) == 0 ||
          Token.comp(_T("*"), FALSE) == 0 || Token.comp(_T("/"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
      }
      else
      // Operatori booleani
      if (Token.comp(_T("AND"), FALSE) == 0 || Token.comp(_T("OR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("ALLTRIM"), FALSE) == 0) // trim a sinistra e destra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("ATOI"), FALSE) == 0) // conversione da carattere a intero
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("CHANGEBLOCK"), FALSE) == 0 || // calcolo grafico (con 1 parametro) su tabella geometria
          Token.comp(_T("CHANGECOLOR"), FALSE) == 0 ||
          Token.comp(_T("CHANGEELEV"), FALSE) == 0 ||
          Token.comp(_T("CHANGEHTEXT"), FALSE) == 0 ||
          Token.comp(_T("CHANGELAYER"), FALSE) == 0 ||
          Token.comp(_T("CHANGELINETYPE"), FALSE) == 0 ||
          Token.comp(_T("CHANGEROTATION"), FALSE) == 0 || // questa funzione ha 2 parametri ma ne considero solo 1
          Token.comp(_T("CHANGESCALE"), FALSE) == 0 ||
          Token.comp(_T("CHANGETEXTSTYLE"), FALSE) == 0 ||
          Token.comp(_T("CHANGETHICKNESS"), FALSE) == 0 ||
          Token.comp(_T("CHANGEWIDTH"), FALSE) == 0)
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldName, Parameter;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere 1 o 2 che però non considero)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         if (Token.comp(_T("CHANGEBLOCK"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_block_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGECOLOR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_color"), adInteger,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGEELEV"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_elevation"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGEHTEXT"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_h_text"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGELAYER"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_layer_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGELINETYPE"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_linetype_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGEROTATION"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_rotation"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGESCALE"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_scale"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGETEXTSTYLE"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_textstyle_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGETHICKNESS"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_thickness"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGEWIDTH"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_width"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);

         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("CHANGECOLORATTR"), FALSE) == 0 || // calcolo grafico attributi (con 1 parametro) su tabella geometria attributi
          Token.comp(_T("CHANGEHTEXTATTR"), FALSE) == 0 ||
          Token.comp(_T("CHANGELAYERATTR"), FALSE) == 0 ||
          Token.comp(_T("CHANGETEXTSTYLEATTR"), FALSE) == 0 ||
          Token.comp(_T("VISATTR"), FALSE) == 0)
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldName, Parameter;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (devono essere 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         if (Token.comp(_T("CHANGECOLORATTR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_color"), adInteger,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGEHTEXTATTR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_h_text"), adDouble,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGELAYERATTR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_layer_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("CHANGETEXTSTYLEATTR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_textstyle_name"), adChar,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
         else if (Token.comp(_T("VISATTR"), FALSE) == 0)
            gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_invisibility"), adInteger,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);

         gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_name"), adChar,
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);

         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("CHANGEHATCH"), FALSE) == 0)
      { // calcolo grafico (con 1 parametro obbligatorio e 5 opzionali) su tabella geometria
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     FieldNames, Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() == 0)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_name"), adChar,
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         if ((p = (C_STR_STRLIST *) p->get_next())) // secondo parametro
         {
            gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_color"), adInteger,
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);

            if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
            {
               gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_scale"), adDouble,
                                              IntegerVarList, DoubleVarList,
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList);
               if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList,
                                           pCls) == GS_BAD) return GS_BAD;

               if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
               {
                  gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_rotation"), adDouble,
                                                 IntegerVarList, DoubleVarList,
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList);
                     return GS_BAD;
                  if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList,
                                              pCls) == GS_BAD) return GS_BAD;
                  if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
                  {
                     gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_rotation_unit"), adDouble,
                                                    IntegerVarList, DoubleVarList,
                                                    CharVarList, GeomVarList, BoolVarList,
                                                    DateVarList, TimestampVarList);
                     if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList,
                                                 pCls) == GS_BAD) return GS_BAD;

                     if ((p = (C_STR_STRLIST *) p->get_next())) // sesto parametro
                     {
                        gsc_LispAddVarToListVarPLPGSQL(_T("m_hatch_layer_name"), adChar,
                                                       IntegerVarList, DoubleVarList,
                                                       CharVarList, GeomVarList, BoolVarList,
                                                       DateVarList, TimestampVarList);
                        if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                    CharVarList, GeomVarList, BoolVarList,
                                                    DateVarList, TimestampVarList,
                                                    pCls) == GS_BAD) return GS_BAD;
                     }
                  }
               }
            }
         }
      }
      else
      if (Token.comp(_T("IF"), FALSE) == 0)
      {
         // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro (condizione)
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro (se la condizione è vera)
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
         p = (C_STR_STRLIST *) p->get_next(); // terzo parametro (se la condizione è false); opzionale
         if (p)
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("ITOA"), FALSE) == 0) // conversione da intero ad carattere
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("LTRIM"), FALSE) == 0) // trim a sinistra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("MOVEATTR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere almeno 2)
   	   // (moveattr nomeattr modalità [offset1[offset2[Flag_generaz
         // [allineam_oriz[allineam_vert[controllo_rotz[controllo_sovrapposiz]]]]]]])
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (devono essere almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }

         gsc_LispAddVarToListVarPLPGSQL(_T("m_geom"), adLongVarBinary, // geometria
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         gsc_LispAddVarToListVarPLPGSQL(_T("m_position"), adLongVarBinary, // geometria
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         gsc_LispAddVarToListVarPLPGSQL(_T("m_rotation"), adDouble, // rotazione
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);

         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_name"), adChar, // nome dell'attributo
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         p = (C_STR_STRLIST *) p->get_next(); // secondo parametro
         gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_mode"), adChar, // modalità di posizionamento
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList, pCls) == GS_BAD) return GS_BAD;

         if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
         {
            gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_offset1"), adDouble, // offset1
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
            if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
            {
               gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_offset2"), adDouble, // offset2
                                              IntegerVarList, DoubleVarList,
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList);
               if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList,
                                           pCls) == GS_BAD) return GS_BAD;

               if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
               {
                  gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_generating_flag"), adInteger, // Flag_generaz
                                                 IntegerVarList, DoubleVarList,
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList);
                  if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList,
                                              pCls) == GS_BAD) return GS_BAD;

                  if ((p = (C_STR_STRLIST *) p->get_next())) // sesto parametro
                  {
                     gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_horiz_align"), adInteger, // allineamento orizzontale
                                                    IntegerVarList, DoubleVarList,
                                                    CharVarList, GeomVarList, BoolVarList,
                                                    DateVarList, TimestampVarList);
                     if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList,
                                                 pCls) == GS_BAD) return GS_BAD;

                     if ((p = (C_STR_STRLIST *) p->get_next())) // settimo parametro
                     {
                        gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_vert_align"), adInteger, // allineamento verticale
                                                       IntegerVarList, DoubleVarList,
                                                       CharVarList, GeomVarList, BoolVarList,
                                                       DateVarList, TimestampVarList);
                        if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                    CharVarList, GeomVarList, BoolVarList,
                                                    DateVarList, TimestampVarList,
                                                    pCls) == GS_BAD) return GS_BAD;

                        if ((p = (C_STR_STRLIST *) p->get_next())) // ottavo parametro
                        {
                           gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_rot_ctrl"), adBoolean, // controllo di rotazione
                                                          IntegerVarList, DoubleVarList,
                                                          CharVarList, GeomVarList, BoolVarList,
                                                          DateVarList, TimestampVarList);
                           if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                       CharVarList, GeomVarList, BoolVarList,
                                                       DateVarList, TimestampVarList,
                                                       pCls) == GS_BAD) return GS_BAD;

                           if ((p = (C_STR_STRLIST *) p->get_next())) // nono parametro
                           {
                              gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_overlap_ctrl"), adInteger, // controllo di sovrapposizione
                                                             IntegerVarList, DoubleVarList,
                                                             CharVarList, GeomVarList, BoolVarList,
                                                             DateVarList, TimestampVarList);
                              if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                          CharVarList, GeomVarList, BoolVarList,
                                                          DateVarList, TimestampVarList,
                                                          pCls) == GS_BAD) return GS_BAD;
                           }
                        }
                     }
                  }
               }
            }
         }
      }
      else
      if (Token.comp(_T("MOVESHEET"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere almeno 1)
      	// (movesheet modalità [offset1[offset2[controllo_rotz[controllo_sovrapposiz]]]])
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
         C_STRING     Parameters;

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         if (pGphInfo->LblGroupingTableRef.len() == 0) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

         // Leggo i parametri della funzione (devono essere almeno 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }

         gsc_LispAddVarToListVarPLPGSQL(_T("m_geom"), adLongVarBinary, // geometria
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         gsc_LispAddVarToListVarPLPGSQL(_T("m_position"), adLongVarBinary, // geometria
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         gsc_LispAddVarToListVarPLPGSQL(_T("m_rotation"), adDouble, // rotazione
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);

         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_mode"), adChar, // modalità di posizionamento
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         if ((p = (C_STR_STRLIST *) p->get_next())) // secondo parametro
         {
            gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_offset1"), adDouble, // offset1
                                           IntegerVarList, DoubleVarList,
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList);
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;

            if ((p = (C_STR_STRLIST *) p->get_next())) // terzo parametro
            {
               gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_offset2"), adDouble, // offset2
                                              IntegerVarList, DoubleVarList,
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList);
               if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList,
                                           pCls) == GS_BAD) return GS_BAD;

               if ((p = (C_STR_STRLIST *) p->get_next())) // quarto parametro
               {
                  gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_rot_ctrl"), adBoolean, // controllo di rotazione
                                                 IntegerVarList, DoubleVarList,
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList);
                  if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                              CharVarList, GeomVarList, BoolVarList,
                                              DateVarList, TimestampVarList,
                                              pCls) == GS_BAD) return GS_BAD;

                  if ((p = (C_STR_STRLIST *) p->get_next())) // quinto parametro
                  {
                     gsc_LispAddVarToListVarPLPGSQL(_T("m_moving_overlap_ctrl"), adInteger, // controllo di sovrapposizione
                                                    IntegerVarList, DoubleVarList,
                                                    CharVarList, GeomVarList, BoolVarList,
                                                    DateVarList, TimestampVarList);
                     if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                                 CharVarList, GeomVarList, BoolVarList,
                                                 DateVarList, TimestampVarList,
                                                 pCls) == GS_BAD) return GS_BAD;
                  }
               }
            }
         }
      }
      else
      if (Token.comp(_T("NULL"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("TRIM"), FALSE) == 0) // trim a destra
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("SETQ"), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere pari)
         if (pParse->ptr_Str_StrList() == NULL || (pParse->ptr_Str_StrList()->get_count() % 2) == 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         while (p) // parametro successivo
         {
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
            p = (C_STR_STRLIST *) p->get_next();
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
            p = (C_STR_STRLIST *) p->get_next();
         }
      }
      else
      if (Token.comp(_T("STRCASE"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("STRCAT"), FALSE) == 0)
      {  // Leggo i parametri della funzione (almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() <= 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
         while ((p = (C_STR_STRLIST *) p->get_next()) != NULL) // parametro successivo
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("STRLEN"), FALSE) == 0)
      {  // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("SUBSTR"), FALSE) == 0)
      {  // Leggo i parametri della funzione (devono essere almeno 2)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() < 2)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         p = (C_STR_STRLIST *) p->get_next();
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;

         if ((p = (C_STR_STRLIST *) p->get_next()))
            if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList,
                                        pCls) == GS_BAD) return GS_BAD;
      }
      else
      if (Token.comp(_T("VISSHEET"), FALSE) == 0) // calcolo grafico attributi (con 1 parametro) su tabella geometria attributi
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();

         if (pGphInfo == NULL) { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         // Leggo i parametri della funzione (deve essere 1)
         if (pParse->ptr_Str_StrList() == NULL || pParse->ptr_Str_StrList()->get_count() != 1)
            { GS_ERR_COD = eGSInvLispFunc; return GS_BAD; }
         p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // primo parametro

         gsc_LispAddVarToListVarPLPGSQL(_T("m_attr_invisibility"), adInteger,
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
         if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                     CharVarList, GeomVarList, BoolVarList,
                                     DateVarList, TimestampVarList,
                                     pCls) == GS_BAD) return GS_BAD;
      }
      else // Funzione utente
      {
         if (pParse->ptr_Str_StrList())
         {
            p = (C_STR_STRLIST *) pParse->ptr_Str_StrList()->get_head(); // ciclo sui parametri
            while (p)
            {
               if (gsc_LispParserToPLPGSQL(p, IntegerVarList, DoubleVarList, 
                                           CharVarList, GeomVarList, BoolVarList,
                                           DateVarList, TimestampVarList,
                                           pCls) == GS_BAD) return GS_BAD;
               p = (C_STR_STRLIST *) p->get_next();
            }
         }
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_LispParserToPLPGSQL                                <internal> */
/*+
  Questa funzione aggiunge una variabile ala lista dell variabili da dichiarare dal codice PLPGSQL
  risultato dalla traduzione del LISP in PLPGSQL.
  Parametri:
  DataTypeEnum DataType;      Tipo di dato (adLongVarBinary = geometria) (input)
  C_STR_LIST &IntegerVarList; Lista delle varibili intere da dichiarare nel codice PLPGSQL
  C_STR_LIST &DoubleVarList;  Lista delle varibili double da dichiarare nel codice PLPGSQL
  C_STR_LIST &CharVarList;    Lista delle varibili char da dichiarare nel codice PLPGSQL
  C_STR_LIST &GeomVarList;    Lista delle varibili geometry da dichiarare nel codice PLPGSQL
  C_STR_LIST &BoolVarList;    Lista delle varibili booleane da dichiarare nel codice PLPGSQL
  C_STR_LIST &DateVarList;    Lista delle varibili date da dichiarare nel codice PLPGSQL
  C_STR_LIST &TimestampVarList; Lista delle varibili date da dichiarare nel codice PLPGSQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
void gsc_LispAddVarToListVarPLPGSQL(C_STRING &PLPGSQLVariable, DataTypeEnum DataType,
                                    C_STR_LIST &IntegerVarList, C_STR_LIST &DoubleVarList,
                                    C_STR_LIST &CharVarList, C_STR_LIST &GeomVarList,
                                    C_STR_LIST &BoolVarList, C_STR_LIST &DateVarList,
                                    C_STR_LIST &TimestampVarList)
{
   gsc_LispAddVarToListVarPLPGSQL(PLPGSQLVariable.get_name(), DataType,
                                  IntegerVarList, DoubleVarList, CharVarList,
                                  GeomVarList, BoolVarList, DateVarList,
                                  TimestampVarList);
}
void gsc_LispAddVarToListVarPLPGSQL(const TCHAR *PLPGSQLVariable, DataTypeEnum DataType,
                                    C_STR_LIST &IntegerVarList, C_STR_LIST &DoubleVarList,
                                    C_STR_LIST &CharVarList, C_STR_LIST &GeomVarList,
                                    C_STR_LIST &BoolVarList, C_STR_LIST &DateVarList,
                                    C_STR_LIST &TimestampVarList)
{
   // Verifico che la variabile non sia già stata dichiarata anche come altro tipo
   if (IntegerVarList.search_name(PLPGSQLVariable, FALSE) ||
       DoubleVarList.search_name(PLPGSQLVariable, FALSE) ||
       CharVarList.search_name(PLPGSQLVariable, FALSE) ||
       GeomVarList.search_name(PLPGSQLVariable, FALSE) ||
       BoolVarList.search_name(PLPGSQLVariable, FALSE) ||
       DateVarList.search_name(PLPGSQLVariable, FALSE) ||
       TimestampVarList.search_name(PLPGSQLVariable, FALSE))
      return;

   if (gsc_DBIsNumeric(DataType) == GS_GOOD)
   {
      if (gsc_DBIsNumericWithDecimals(DataType) == GS_GOOD)
         DoubleVarList.add_tail_unique(PLPGSQLVariable);
      else
         IntegerVarList.add_tail_unique(PLPGSQLVariable);
   }
   else
   if (gsc_DBIsChar(DataType) == GS_GOOD)
      CharVarList.add_tail_unique(PLPGSQLVariable);
   else
   if (DataType == adBoolean)
      BoolVarList.add_tail_unique(PLPGSQLVariable);
   else
   if (gsc_DBIsDate(DataType) == GS_GOOD)
      DateVarList.add_tail_unique(PLPGSQLVariable);
   else
   if (gsc_DBIsTimestamp(DataType) == GS_GOOD || gsc_DBIsTime(DataType) == GS_GOOD)
      TimestampVarList.add_tail_unique(PLPGSQLVariable);
   else
   if (DataType == adLongVarBinary) // geometry
      GeomVarList.add_tail_unique(PLPGSQLVariable);
}


/*************************************************************************/
/*.doc gsc_pgviewAddVarOnFunctionRefreshEntity                <internal> */
/*+
  Questa funzione aggiunge il codice per la dichiarazione delle variabili
  interne alla funzione che deve essere richiamata per riallineare 
  l'entità modificata.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddVarOnFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm)
{
   C_STRING       PLPGSQLType, PLPGSQLVariable;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_ATTRIB       *pAttrib;
   C_STR_LIST     IntegerVarList, DoubleVarList, CharVarList, GeomVarList, BoolVarList;
   C_STR_LIST     DateVarList, TimestampVarList;
   C_STR          *p;

   // Inizializzo i tipi ADO per ogni attributo
   if (pCls->ptr_attrib_list()->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   Stm = _T("m_err_msg character varying; "); // per i messaggi di errore
   Stm += GS_LFSTR;
   Stm += _T("m_geom_count integer; "); // contatore delle geometrie dell'entità
   Stm += GS_LFSTR;
   if (pGphInfo->LblTableRef.len() > 0)
   {
      Stm += _T("m_lbl_count integer; "); // contatore delle etuichette dell'entità
      Stm += GS_LFSTR;
   }

   // Per ogni campo calcolato aggiungo una variabile che si chiama
   // "plpgsql_var_"+nome attributo ricavato dalla funzione gsc_AttribNameToPLPGSQLVariable
   pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      // se è un attributo calcolato, validato, obbligatorio, etichettato
      // oppure è utilizzato come parametro di una funzione di calcolo o di validità di un altro attributo
      // creo una variabile PLPGSQL
      if (pAttrib->is_calculated() == GS_GOOD || pAttrib->is_DefCalculated() == GS_GOOD || 
          pAttrib->is_validated() == GS_GOOD || 
          pAttrib->is_mandatory() == GS_GOOD || pAttrib->is_visible() ||
          pCls->ptr_attrib_list()->is_funcparam(pAttrib->get_name()) == GS_GOOD)
      {
         // correggo il nome
         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;
         gsc_LispAddVarToListVarPLPGSQL(PLPGSQLVariable, pAttrib->ADOType,
                                        IntegerVarList, DoubleVarList,
                                        CharVarList, GeomVarList, BoolVarList,
                                        DateVarList, TimestampVarList);
      }
      
      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }

   // Applica funzioni di calcolo grafico per la geometria
   if (pCls->ptr_fas() && pCls->ptr_fas()->is_graph_calculated() == GS_GOOD)
   {
      C_STR_STRLIST *pParse;

      if (gsc_LispParser(pCls->ptr_fas()->fun_graph_calc, &pParse) == GS_BAD) return GS_BAD;
      if (gsc_LispParserToPLPGSQL(pParse, 
                                  IntegerVarList, DoubleVarList, CharVarList, GeomVarList, BoolVarList,
                                  DateVarList, TimestampVarList,
                                  pCls) == GS_BAD) return GS_BAD;
   }

   // variabili intere
   p = (C_STR *) IntegerVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" integer;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili double
   p = (C_STR *) DoubleVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" double precision;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili character varying
   p = (C_STR *) CharVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" character varying;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili geometriche
   p = (C_STR *) GeomVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" geometry;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili booleane
   p = (C_STR *) BoolVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" boolean;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili data
   p = (C_STR *) DateVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" date;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }
   // variabili timestamp
   p = (C_STR *) TimestampVarList.get_head();
   while (p)
   {
      Stm += p->get_name();
      Stm += _T(" timestamp;");
      Stm += GS_LFSTR;
      p = (C_STR *) p->get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewAddDefaultCalcFieldsNotOnInsertFunctionRefreshEntity  <internal> */
/*+
  Questa funzione aggiunge il codice per la lettura dei valori degli attributi
  con default calcolato quando l'operazione corrente non è di inserimento.
  I valori quindi verranno letti dalla tabella alfanumerica.
  Parametri:
  C_CLASS *pCls;     Puntatore a classe GEOsim
  C_STRING &Stm;     Stringa a cui appendere il codice PLSQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddDefaultCalcFieldsNotOnInsertFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm)
{
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn = pInfo->getDBConnection(OLD);
   C_ATTRIB       *pAttrib;
   C_STRING       PLPGSQLVariable, PLPGSQLVariableList, AttribName, AttribNameList;

   pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      // ha valore di default calcolato
      if (pAttrib->is_DefCalculated() == GS_GOOD)
      {
         if (PLPGSQLVariableList.len() > 0)
         {
            AttribNameList += _T(",");
            PLPGSQLVariableList += _T(",");
         }

         AttribName = pAttrib->get_name();
         if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                           pConn->get_StrCaseFullTableRef()) == GS_BAD)
            return GS_BAD;
         AttribNameList += AttribName; // nome attributo

         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;
         PLPGSQLVariableList += PLPGSQLVariable;
      }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }

   if (PLPGSQLVariableList.len() > 0)
   {
      Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica");
      Stm += GS_LFSTR;
      Stm += _T("-- aventi solo il valore di default calcolato nel caso in cui non si tratti di inserimento");
      Stm += GS_LFSTR;
      // i valori degli attributi con valore di default calcolato vanno letti dalla tabella degli attributi
      Stm += _T("IF insert_mode = 0 THEN");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'SELECT ");
      Stm += AttribNameList;
      Stm += _T(" FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" WHERE ");

      AttribName = pInfo->key_attrib;
      if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      Stm += AttribName;
      Stm += _T("=$1' INTO ");
      Stm += PLPGSQLVariableList;
      Stm += _T(" USING entity_id;");
      Stm += GS_LFSTR;
      Stm += _T("END IF;");
      Stm += GS_LFSTR;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewAddCalcFieldsFromGraphFunctionRefreshEntity  <internal> */
/*+
  Questa funzione aggiunge il codice per il calcolo degli attributi calcolati 
  dalla grafica. Qualora ci siano valori di default calcolati verrà considerato
  solo il caso di inserimento (la modifica e la cancellazione sono gestititi 
  dalla funzione "gsc_pgviewAddDefaultCalcFieldsNotOnInsertFunctionRefreshEntity")
  Parametri:
  C_CLASS *pCls;     Puntatore a classe GEOsim
  C_STRING &Stm;     Stringa a cui appendere il codice PLSQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddCalcFieldsFromGraphFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm)
{
   C_ATTRIB       *pAttrib;
   C_ATTRIB_LIST  CalculatedAttribList;
   C_STRING       PLPGSQLFunc, PLPGSQLVariable, AttribName;
   C_STRING       PLPGSQLFuncList, PLPGSQLFuncListDef, PLPGSQLVariableList, PLPGSQLVariableListDef;
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pConn = pGphInfo->getDBConnection();

   // Ottengo la lista degli attributi calcolati dalla grafica con funzioni di aggregazione
   pCls->ptr_attrib_list()->copy(&CalculatedAttribList);
   pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
   while (pAttrib)
      // se ha il valore calcolato dalla grafica
      if (pAttrib->is_from_graph() == GS_GOOD &&
          // se il calcolo è fatto con le funzioni di aggregazione
          // (es. ".LENGTH" per la lunghezza o ".WINDOW" per l'occupazione)
          (gsc_strcmp(pAttrib->calc_func, GS_LISP_LENGTH, FALSE) == 0 ||
           gsc_strcmp(pAttrib->calc_func, GS_LISP_WINDOW, FALSE) == 0))
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_next();
      else // lo cancello
      {
         CalculatedAttribList.remove_at();
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_cursor();
      }

   // Se ci sono attributi calcolati dalla grafica con funzioni di aggregazione
   if (CalculatedAttribList.get_count() > 0)
   {
      pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
      while (pAttrib)
      {
         if (gsc_FuncFromGphToPLPGSQLFunc(pCls, pAttrib->calc_func, PLPGSQLFunc) == GS_BAD)
            return GS_BAD;
         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;

         if (pAttrib->is_DefCalculated() == GS_BAD) // è sempre calcolato
         {
            if (PLPGSQLVariableList.len() > 0)
            {
               PLPGSQLFuncList += _T(",");
               PLPGSQLVariableList += _T(",");
            }

            PLPGSQLFuncList += PLPGSQLFunc;
            PLPGSQLVariableList += PLPGSQLVariable;
         }
         else // è calcolato solo il valore di default (in inserimento)
         {
            if (PLPGSQLVariableListDef.len() > 0)
            {
               PLPGSQLFuncListDef += _T(",");
               PLPGSQLVariableListDef += _T(",");
            }

            PLPGSQLFuncListDef += PLPGSQLFunc;
            PLPGSQLVariableListDef += PLPGSQLVariable;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      AttribName = pGphInfo->ent_key_attrib;
      if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      // attributi calcolati
      if (PLPGSQLVariableList.len() > 0)
      {
         Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica");
         Stm += GS_LFSTR;
         Stm += _T("-- con funzioni di aggregazione");
         Stm += GS_LFSTR;
         Stm += _T("EXECUTE 'SELECT ");
         Stm += PLPGSQLFuncList;
         Stm += _T(" FROM ");
         Stm += pGphInfo->TableRef;
         Stm += _T(" WHERE ");
         Stm += AttribName;
         Stm += _T("=$1' INTO ");
         Stm += PLPGSQLVariableList;
         Stm += _T(" USING entity_id; ");
         Stm += GS_LFSTR;
      }
      // attributi con il solo valore di default calcolato
      if (PLPGSQLVariableListDef.len() > 0)
      {
         Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica");
         Stm += GS_LFSTR;
         Stm += _T("-- con funzioni di aggregazione");
         Stm += GS_LFSTR;
         Stm += _T("-- aventi solo il valore di default calcolato nel caso in cui si tratti di inserimento");
         Stm += GS_LFSTR;
         Stm += _T("IF insert_mode = 1 THEN");
         Stm += GS_LFSTR;
         Stm += _T("EXECUTE 'SELECT ");
         Stm += PLPGSQLFuncListDef;
         Stm += _T(" FROM ");
         Stm += pGphInfo->TableRef;
         Stm += _T(" WHERE ");
         Stm += AttribName;
         Stm += _T("=$1' INTO ");
         Stm += PLPGSQLVariableListDef;
         Stm += _T(" USING entity_id; ");
         Stm += GS_LFSTR;
         Stm += _T("END IF;");
         Stm += GS_LFSTR;
      }
   }

   // Ottengo la lista degli attributi calcolati dalla grafica senza funzioni di aggregazione
   pCls->ptr_attrib_list()->copy(&CalculatedAttribList);
   pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
   while (pAttrib)
      // se ha il valore calcolato dalla grafica
      if (pAttrib->is_from_graph() == GS_GOOD &&
          // se il calcolo NON è fatto con le funzioni di aggregazione
          // (es. ".LENGTH" per la lunghezza o ".WINDOW" per l'occupazione)
          (gsc_strcmp(pAttrib->calc_func, GS_LISP_LENGTH, FALSE) != 0 &&
           gsc_strcmp(pAttrib->calc_func, GS_LISP_WINDOW, FALSE) != 0))
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_next();
      else // lo cancello
      {
         CalculatedAttribList.remove_at();
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_cursor();
      }

   // Se ci sono attributi calcolati dalla grafica senza funzioni di aggregazione
   if (CalculatedAttribList.get_count() > 0)
   {
      PLPGSQLVariableList.clear();
      PLPGSQLVariableListDef.clear();
      pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
      while (pAttrib)
      {
         if (gsc_FuncFromGphToPLPGSQLFunc(pCls, pAttrib->calc_func, PLPGSQLFunc) == GS_BAD)
            return GS_BAD;
         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;

         if (pAttrib->is_DefCalculated() == GS_BAD) // è sempre calcolato
         {
            if (PLPGSQLVariableList.len() > 0)
            {
               PLPGSQLFuncList += _T(",");
               PLPGSQLVariableList += _T(",");
            }

            PLPGSQLFuncList += PLPGSQLFunc;
            PLPGSQLVariableList += PLPGSQLVariable;
         }
         else // è calcolato solo il valore di default (in inserimento)
         {
            if (PLPGSQLVariableListDef.len() > 0)
            {
               PLPGSQLFuncListDef += _T(",");
               PLPGSQLVariableListDef += _T(",");
            }

            PLPGSQLFuncListDef += PLPGSQLFunc;
            PLPGSQLVariableListDef += PLPGSQLVariable;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      AttribName = pGphInfo->ent_key_attrib;
      if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      // attributi calcolati
      if (PLPGSQLVariableList.len() > 0)
      {
         Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica");
         Stm += GS_LFSTR;
         Stm += _T("-- senza funzioni di aggregazione");
         Stm += GS_LFSTR;
         Stm += _T("EXECUTE 'SELECT ");
         Stm += PLPGSQLFuncList;
         Stm += _T(" FROM ");
         Stm += pGphInfo->TableRef;
         Stm += _T(" WHERE ");
         Stm += AttribName;
         Stm += _T("=$1 LIMIT 1' INTO "); // considero solo la prima riga
         Stm += PLPGSQLVariableList;
         Stm += _T(" USING entity_id; ");
         Stm += GS_LFSTR;
      }
      // attributi con il solo valore di default calcolato
      if (PLPGSQLVariableListDef.len() > 0)
      {
         Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica");
         Stm += GS_LFSTR;
         Stm += _T("-- senza funzioni di aggregazione");
         Stm += GS_LFSTR;
         Stm += _T("-- aventi solo il valore di default calcolato nel caso in cui si tratti di inserimento");
         Stm += GS_LFSTR;
         Stm += _T("IF insert_mode = 1 THEN");
         Stm += GS_LFSTR;
         Stm += _T("EXECUTE 'SELECT ");
         Stm += PLPGSQLFuncListDef;
         Stm += _T(" FROM ");
         Stm += pGphInfo->TableRef;
         Stm += _T(" WHERE ");
         Stm += AttribName;
         Stm += _T("=$1 LIMIT 1' INTO "); // considero solo la prima riga
         Stm += PLPGSQLVariableListDef;
         Stm += _T(" USING entity_id; ");
         Stm += GS_LFSTR;
         Stm += _T("END IF;");
         Stm += GS_LFSTR;
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewAddFieldsNoFromGraphFunctionRefreshEntity    <internal> */
/*+
  Questa funzione aggiunge il codice per il calcolo degli attributi calcolati 
  NON dalla grafica.
  Parametri:
  C_CLASS *pCls;     Puntatore a classe GEOsim
  C_STRING &Stm;     Stringa a cui appendere il codice PLSQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFieldsNoFromGraphFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm)
{
   C_ATTRIB       *pAttrib;
   C_ATTRIB_LIST  CalculatedAttribList;
   C_STRING       PLPGSQLFunc, PLPGSQLVariable, AttribName;
   C_STRING       AttribNameList, PLPGSQLVariableList, PLPGSQLVariableListDef;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn = pInfo->getDBConnection(OLD);

   // Ottengo la lista degli attributi che sono parametri nelle funzioni di calcolo
   // le funzioni di calcolo dalla grafica non sono usabili insieme a funzioni GEOlisp
   pCls->ptr_attrib_list()->copy(&CalculatedAttribList);
   pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
   while (pAttrib)
      // Se l'attributo è utilizzato come parametro di una funzione di calcolo 
      // o di validità di un altro attributo (o validità di se stesso o obbligatorio)
      // e non è calcolato dalla grafica (già tratto da "gsc_pgviewAddCalcFieldsFromGraphFunctionRefreshEntity")
      if (pCls->ptr_attrib_list()->is_funcparam(pAttrib->get_name()) == GS_GOOD ||
          pAttrib->is_validated() == GS_GOOD || pAttrib->is_mandatory() == GS_GOOD)
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_next();
      else // lo cancello
      {
         CalculatedAttribList.remove_at();
         pAttrib = (C_ATTRIB *) CalculatedAttribList.get_cursor();
      }

   // Se ci sono attributi calcolati non dalla grafica
   if (CalculatedAttribList.get_count() > 0)
   {
      pAttrib = (C_ATTRIB *) CalculatedAttribList.get_head();
      while (pAttrib)
      {
         AttribName = pAttrib->get_name();
         if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                           pConn->get_StrCaseFullTableRef()) == GS_BAD)
            return GS_BAD;

         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;

         if (PLPGSQLVariableList.len() > 0)
         {
            AttribNameList += _T(",");
            PLPGSQLVariableList += _T(",");
         }

         AttribNameList += AttribName;
         PLPGSQLVariableList += PLPGSQLVariable;

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      // inizializzo le variabili PLPGSQL con i valori degli attributi usati nei calcoli
      Stm += _T("-- Inizializzazione variabili PLPGSQL con i valori degli attributi non calcolati da grafica");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'SELECT ");
      Stm += AttribNameList;
      Stm += _T(" FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" WHERE ");
      Stm += pInfo->key_attrib;
      Stm += _T("=$1' INTO ");
      Stm += PLPGSQLVariableList;
      Stm += _T(" USING entity_id;");
      Stm += GS_LFSTR;

      PLPGSQLVariableList.clear();
      PLPGSQLVariableListDef.clear();
      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      while (pAttrib)
      {
         // Se l'attributo è calcolato ma non dalla grafica          
         if ((pAttrib->is_calculated() || pAttrib->is_DefCalculated()) &&
             pAttrib->is_from_graph() == GS_BAD)
         {
            C_STR_STRLIST *pParse;

            if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
               return GS_BAD;

            if (gsc_LispParser(pAttrib->calc_func, &pParse) == GS_BAD) return GS_BAD;
            PLPGSQLFunc.clear();
            if (gsc_LispParserToPLPGSQL(pParse, PLPGSQLFunc, pCls) == GS_BAD) return GS_BAD;
            // Commento le righe perchè devono essere "verificate" manualmente
            PLPGSQLFunc.removePrefixSuffix(NULL, GS_LFSTR);
            PLPGSQLFunc.strtran(GS_LFSTR, _T("\n-- ")); 

            if (pAttrib->is_DefCalculated() == GS_BAD) // è sempre calcolato
            {
               PLPGSQLVariableList += _T("-- "); // Commento la riga perchè deve essere "verificata" manualmente
               PLPGSQLVariableList += PLPGSQLVariable;
               PLPGSQLVariableList += _T(" := ");
               PLPGSQLVariableList += PLPGSQLFunc;
               PLPGSQLVariableList += _T(";");
               PLPGSQLVariableList += GS_LFSTR;
            }
            else // è calcolato solo il valore di default (in inserimento)
            {
               PLPGSQLVariableListDef += _T("-- "); // Commento la riga perchè deve essere "verificata" manualmente
               PLPGSQLVariableListDef += PLPGSQLVariable;
               PLPGSQLVariableListDef += _T(" := ");
               PLPGSQLVariableListDef += PLPGSQLFunc;
               PLPGSQLVariableListDef += _T(";");
               PLPGSQLVariableListDef += GS_LFSTR;
            }
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      // attributi calcolati
      if (PLPGSQLVariableList.len() > 0)
      {
         Stm += _T("-- Funzioni di calcolo tra attributi");
         Stm += GS_LFSTR;
         Stm += PLPGSQLVariableList;
      }
      // attributi con il solo valore di default calcolato
      if (PLPGSQLVariableListDef.len() > 0)
      {
         Stm += _T("-- Funzioni di calcolo tra attributi");
         Stm += GS_LFSTR;
         Stm += _T("-- aventi solo il valore di default calcolato nel caso in cui si tratti di inserimento");
         Stm += GS_LFSTR;
         Stm += _T("IF insert_mode = 1 THEN");
         Stm += GS_LFSTR;
         Stm += PLPGSQLVariableListDef;
         Stm += _T("END IF;");
         Stm += GS_LFSTR;
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewAddValidationFieldsFunctionRefreshEntity     <internal> */
/*+
  Questa funzione aggiunge il codice per la validzione degli attributi
  (l'obbligatorietà è una forma di validazione).
  Parametri:
  C_CLASS *pCls;     Puntatore a classe GEOsim
  C_STRING &Stm;     Stringa a cui appendere il codice PLSQL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddValidationFieldsFunctionRefreshEntity(C_CLASS *pCls, C_STRING &Stm)
{
   C_ATTRIB *pAttrib;
   C_STRING PLPGSQLFunc, PLPGSQLVariable;
   TCHAR    msg[MAX_LEN_MSG];

   // Valida gli attributi dell'entità
   Stm += _T("-- Validazione attributi dell'entità");
   Stm += GS_LFSTR;
   pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      // Se l'attributo è da validare
      if (pAttrib->is_validated() == GS_GOOD)
      {
         C_STR_STRLIST *pParse;

         Stm += _T("-- IF NOT("); // Commento la riga perchè deve essere "verificata" manualmente

         if (gsc_LispParser(pAttrib->valid_func, &pParse) == GS_BAD) return GS_BAD;
         PLPGSQLFunc.clear();
         if (gsc_LispParserToPLPGSQL(pParse, PLPGSQLFunc, pCls) == GS_BAD) return GS_BAD;
         // Commento le righe perchè devono essere "verificate" manualmente
         PLPGSQLFunc.removePrefixSuffix(NULL, GS_LFSTR);
         PLPGSQLFunc.strtran(GS_LFSTR, _T("\n-- ")); 
         Stm += PLPGSQLFunc;
         Stm += _T(") THEN");
         Stm += GS_LFSTR;
         Stm += _T("-- m_err_msg := '");
         if (pAttrib->ValidErrMsg.len() > 0)
            Stm += pAttrib->ValidErrMsg;
         else
         {
       		// "Validità attributo <%s> non soddisfatta."           
            swprintf(msg, MAX_LEN_MSG, gsc_msg(418), pAttrib->get_name());
            Stm += msg;
         }
         Stm += _T("';");
         Stm += GS_LFSTR;
         Stm += _T("-- return '*** GS *** ' || m_err_msg;");
         Stm += GS_LFSTR;
         Stm += _T("-- END IF;");
         Stm += GS_LFSTR;
      }
      // Se l'attributo è obbligatorio
      if (pAttrib->is_mandatory() == GS_GOOD)
      {
         Stm += _T("IF ");
         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;
         Stm += PLPGSQLVariable;
         Stm += _T(" IS NULL THEN");
         Stm += GS_LFSTR;
         Stm += _T("m_err_msg := '");
     		// "Validità attributo <%s> non soddisfatta."           
         swprintf(msg, MAX_LEN_MSG, gsc_msg(418), pAttrib->get_name());
         Stm += msg;
         Stm += _T("';");
         Stm += GS_LFSTR;
         Stm += _T("return '*** GS *** ' || m_err_msg;");
         Stm += GS_LFSTR;
         Stm += _T("END IF;");
         Stm += GS_LFSTR;
      }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_pgviewGetFunctionNameWithParamRefreshEntity        <internal> */
/*+
  Questa funzione ritorna il nome della funzione che deve essere richiamata per riallineare 
  l'entità modificata.
  Parametri:
  C_STRING &FullRefFuncName;   Nome della funzione
  C_STRING &FuncNameWithParam; Nome della funzione con i parametri (out)
-*/  
/*************************************************************************/
void gsc_pgviewGetFunctionNameWithParamRefreshEntity(C_STRING &FullRefFuncName, 
                                                     C_STRING &FuncNameWithParam)
{
   FuncNameWithParam = FullRefFuncName;
   FuncNameWithParam += _T("(entity_id integer, insert_mode integer) ");
}


/*************************************************************************/
/*.doc gsc_pgviewAddFunctionRefreshEntity                     <external> */
/*+
  Questa funzione crea una funzione che deve essere richiamata per riallineare 
  l'entità modificata. La funzione serve per dare "intelligenza" a POSTGIS sulle operazioni
  di inserimento, modifica e cancellazione di una riga della vista. Questo consente di
  usare anche un client diverso da GEOsim per modificare i dati.
  Parametri:
  C_CLASS *pCls;                 puntatore a classe GEOsim
  C_STRING &FullRefFuncName;     Nome della funzione da creare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_pgviewAddFunctionRefreshEntity(C_CLASS *pCls, C_STRING &FullRefFuncName)
{
   C_STRING       FuncNameWithParam, Stm, Stm1, Stm2, AttribName, PLPGSQLType, PLPGSQLVariable, PLPGSQLFunc;
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn;
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_ATTRIB       *pAttrib;
   int            i = 0, param_ndx = 0;

   if (!pInfo || !pGphInfo) return GS_GOOD;
   pConn = pInfo->getDBConnection(OLD);

   //CREATE OR REPLACE FUNCTION dati.linea_refresh(entity_id integer, insert_mode integer)
   //  RETURNS character varying AS
   //$BODY$
   //DECLARE
   //   -- m_err_msg          character varying;
   //   -- m_geom_count       integer;
   //   -- m_lbl_count        integer;
   //   -- plpgsql_var_nota   character varying;
   //   -- plpgsql_var_length double precision;
   //   -- plpgsql_var_somma  double precision;
   //   -- plpgsql_var_campo1 double precision;
   //   -- plpgsql_var_campo2 double precision;
   //BEGIN
   //   m_err_msg := NULL;
   //   --  Conteggio delle geometrie dell'entità
   //   EXECUTE 'SELECT COUNT(*) FROM dati.linea_g WHERE id_parent=$1' INTO m_geom_count USING entity_id;
   //   IF m_geom_count = 0 THEN
   //      -- Cancellazione entità
   //      EXECUTE 'DELETE FROM dati.linea WHERE id=$1' USING entity_id;
   //      EXECUTE 'DELETE FROM dati.linea_g_lbl WHERE id_parent IN 
   //               (SELECT id FROM dati.linea_g_lbl_grp WHERE id_parent=$1)' USING entity_id;
   //      EXECUTE 'DELETE FROM dati.linea_g_lbl_grp WHERE id_parent=$1' USING entity_id;
   //      RETURN NULL;      
   //   END IF;
   //   
   //   -- Conteggio raggruppamenti di etichette dell'entità
   //   EXECUTE 'SELECT COUNT(*) FROM dati.linea_g_lbl_grp WHERE id_parent=$1)' INTO m_lbl_count USING entity_id;
   //   -- oppure Conto quante etichette ha l'entità
   //   EXECUTE 'SELECT COUNT(*) FROM dati.linea_g_lbl WHERE id_parent IN 
   //            (SELECT id FROM dati.linea_g_lbl_grp WHERE id_parent=$1)' INTO m_lbl_count USING entity_id;
   // 
   //   -- Inizializzazione variabili PLPGSQL con i valori degli attributi calcolati da grafica
   //   IF insert_mode = 1 THEN -- Se si tratta di inserimento
   //      -- Funzioni di calcolo da grafica con aggregazione
   //      -- es. di calcolo lunghezza solo per valore di default
   //      --EXECUTE 'SELECT SUM(ST_LENGTH(geom)) FROM dati.linea_g WHERE id_parent=$1' INTO plpgsql_var_length USING entity_id;
   //   END IF;
   //   IF insert_mode = 0 THEN -- Se non si tratta di inserimento
   //      --EXECUTE 'SELECT length FROM dati.linea WHERE id=$1' INTO plpgsql_var_length USING entity_id;
   //   END IF;

   //   -- Funzioni di calcolo da grafica senza aggregazione
   //   -- es. di calcolo del colore
   //   --EXECUTE 'SELECT color FROM dati.linea_g WHERE id_parent=$1 LIMIT 1' INTO plpgsql_var_color USING entity_id;
   //   
   //   -- Applica funzioni di calcolo tra attributi
   //   -- es. di calcolo tra attributi
   //   --EXECUTE 'SELECT campo1, campo2 FROM dati.linea WHERE id=$1' INTO plpgsql_var_campo1,plpgsql_var_campo2 USING entity_id;
   //   --plpgsql_var_somma := plpgsql_var_campo1 + plpgsql_var_campo2;
   //   
   //   -- Valida gli attributi dell'entità
   //   -- es. di validazione
   //   --IF plpgsql_var_somma > 10 THEN
   //   --   m_err_msg := 'L'attributo X non può essere > 10 !';
   //   --   RAISE EXCEPTION "*** GS *** %", m_err_msg;
   //   --END IF;
   //   
   //   -- Aggiorna gli attributi calcolati dell'entità
   //   -- es. di aggiornamento
   //   --EXECUTE 'UPDATE dati.linea SET somma=$1, length=$2 WHERE id=$3' USING plpgsql_var_somma, plpgsql_var_length, entity_id;
   //   
   //   -- Applica funzioni di calcolo grafico per la geometria
   //   -- es. calcolo grafico
   //   --IF t_row.diametro > 100 THEN
   //   --   EXECUTE 'UPDATE dati.linea_g SET width=5 WHERE id_parent=$1' USING entity_id;
   //   --END IF
   //   EXECUTE 'UPDATE dati.linea_g SET aggr_factor=$1 WHERE id_parent=$2' USING m_geom_count + m_lbl_count, entity_id;
   //   -- se classe testo
   //   EXECUTE 'UPDATE dati.linea_g SET "text"=$1, aggr_factor=$2 WHERE id_parent=$3' USING plpgsql_var_nota, m_geom_count + m_lbl_count, entity_id;
   //
   //   -- Legge i dati da etichettare
   //   EXECUTE 'SELECT nota FROM dati.linea WHERE id=$1' INTO plpgsql_var_nota USING entity_id;
   //   
   //   -- Aggiorno le etichette
   //   -- es. aggiorno etichetta 'nota'
   //   EXECUTE 'UPDATE dati.linea_g_lbl SET "text"=$1 WHERE id_parent IN 
   //            (SELECT id FROM dati.linea_g_lbl_grp WHERE id_parent=$2) AND lbl_attr_name=$3'
   //            USING plpgsql_var_nota, entity_id, 'nota';
   //  -- Aggiorno fattore di aggregazione etichette
   //   EXECUTE 'UPDATE dati.linea_g_lbl_grp SET aggr_factor=$1 WHERE id_parent=$2' USING m_geom_count + m_lbl_count, entity_id;
   //   
   //   RETURN NULL;
   //EXCEPTION
   //   WHEN OTHERS THEN
   //      RAISE EXCEPTION '*** GS *** Calculation failed on dati.linea. %', m_err_msg;
   //      RETURN m_err_msg;
   //END;
   //$BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION dati.linea_refresh(entity_id integer, insert_mode integer) TO mapserver; // ROBY

   // Inizializzo i tipi ADO per ogni attributo
   if (pCls->ptr_attrib_list()->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   gsc_pgviewGetFunctionNameWithParamRefreshEntity(FullRefFuncName, FuncNameWithParam);

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += FuncNameWithParam;
   Stm += GS_LFSTR;
   Stm += _T(" RETURNS character varying AS ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE ");
   Stm += GS_LFSTR;

   // Dichiarazione delle variabili
   if (gsc_pgviewAddVarOnFunctionRefreshEntity(pCls, Stm1) == GS_BAD)
      return GS_BAD;
   Stm += Stm1;

   Stm += _T("BEGIN ");
   Stm += GS_LFSTR;

   Stm += _T("m_err_msg := NULL;");
   Stm += GS_LFSTR;
   
   // Verifico quante geometrie ha l'entità 
   Stm += _T("-- Conteggio delle geometrie dell'entità");
   Stm += GS_LFSTR;
   Stm += _T("EXECUTE 'SELECT COUNT(*) FROM ");
   Stm += pGphInfo->TableRef;
   Stm += _T(" WHERE ");
   Stm += pGphInfo->ent_key_attrib;
   Stm += _T("=$1' INTO m_geom_count USING entity_id; ");
   Stm += GS_LFSTR;

   Stm += _T("IF m_geom_count = 0 THEN ");
   Stm += GS_LFSTR;
   if (pInfo)
   {  // cancello l'entità
      Stm += _T("-- Cancellazione entità");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'DELETE FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" WHERE ");
      Stm += pInfo->key_attrib;
      Stm += _T("=$1' USING entity_id; ");
      Stm += GS_LFSTR;
   }
   if (pGphInfo->LblGroupingTableRef.len() > 0)
   {  // cancello le etichette raggruppate
      Stm += _T("EXECUTE 'DELETE FROM ");
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T(" IN (SELECT ");
      Stm += pGphInfo->key_attrib;
      Stm += _T(" FROM ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T("=$1)' USING entity_id; ");
      Stm += GS_LFSTR;

      // cancello i raggruppamenti delle etichette
      Stm += _T("EXECUTE 'DELETE FROM ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T("=$1' USING entity_id; ");
      Stm += GS_LFSTR;
   }
   else
   if (pGphInfo->LblTableRef.len() > 0)
   {
      // cancello le etichette
      Stm += _T("EXECUTE 'DELETE FROM ");
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T("=$1)' USING entity_id;");
      Stm += GS_LFSTR;
   }
   Stm += _T("RETURN NULL;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;

   if (pGphInfo->LblGroupingTableRef.len() > 0)
   {  // Conto quanti raggruppamenti di etichette ha l'entità 
      Stm += _T("-- Conteggio raggruppamenti di etichette dell'entità");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'SELECT COUNT(*) FROM ");
      Stm += pGphInfo->LblGroupingTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T("=$1' INTO m_lbl_count USING entity_id; ");
      Stm += GS_LFSTR;
   }
   else
   if (pGphInfo->LblTableRef.len() > 0)
   {  // Conto quante etichette ha l'entità 
      Stm += _T("-- Conteggio etichette dell'entità");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'SELECT COUNT(*) FROM ");
      Stm += pGphInfo->LblTableRef;
      Stm += _T(" WHERE ");
      Stm += pGphInfo->ent_key_attrib;
      Stm += _T("=$1' INTO m_lbl_count USING entity_id; ");
      Stm += GS_LFSTR;
   }

   // aggiungo il codice per la lettura dei valori degli attributi
   // con default calcolato quando l'operazione corrente non è di inserimento
   if (gsc_pgviewAddDefaultCalcFieldsNotOnInsertFunctionRefreshEntity(pCls, Stm) == GS_BAD)
      return GS_BAD;

   // aggiungo il codice per il calcolo degli attributi calcolati dalla grafica
   if (gsc_pgviewAddCalcFieldsFromGraphFunctionRefreshEntity(pCls, Stm) == GS_BAD)
      return GS_BAD;

   // aggiungo il codice per il calcolo degli attributi calcolati NON dalla grafica
   if (gsc_pgviewAddFieldsNoFromGraphFunctionRefreshEntity(pCls, Stm) == GS_BAD)
      return GS_BAD;

   // Valida gli attributi dell'entità
   if (gsc_pgviewAddValidationFieldsFunctionRefreshEntity(pCls, Stm) == GS_BAD)
      return GS_BAD;

   // Aggiorna gli attributi calcolati (o solo i loro valori di default) dell'entità
   if (pCls->ptr_attrib_list()->is_calculated() == GS_GOOD ||
       pCls->ptr_attrib_list()->is_DefCalculated() == GS_GOOD) 
   {
      Stm += _T("-- Aggiornamento attributi calcolati dell'entità");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'UPDATE ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" SET ");

      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      param_ndx = 0;
      Stm1.clear();
      while (pAttrib)
      {
         // se è un attributo calcolato o solo il suo valore di default
         if (pAttrib->is_calculated() == GS_GOOD ||
             pAttrib->is_DefCalculated() == GS_GOOD)
         {
            if (param_ndx > 0)
            {
               Stm += _T(",");
               Stm1 += _T(",");
            }
            param_ndx++;
            AttribName = pAttrib->get_name();
            if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD)
               return GS_BAD;

            Stm += AttribName; // nome attributo
            Stm += _T("=$");
            Stm += param_ndx;

            if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
               return GS_BAD;

            if (gsc_DBIsChar(pAttrib->ADOType) == GS_GOOD) // Se il campo è carattere
            { // per sicurezza correggo le dimensioni dei valori
               Stm1 += _T("SUBSTRING(");
               Stm1 += PLPGSQLVariable;
               Stm1 += _T(" FOR ");
               Stm1 += pAttrib->len;
               Stm1 += _T(")");
            }
            else
               Stm1 += PLPGSQLVariable;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
      Stm += _T(" WHERE ");
      Stm += pInfo->key_attrib;
      Stm += _T("=$");
      Stm += ++param_ndx;
      Stm += _T("' USING ");
      Stm += Stm1;
      Stm += _T(",entity_id; ");
      Stm += GS_LFSTR;
   }

   // Applica funzioni di calcolo grafico per la geometria
   if (pCls->ptr_fas() && pCls->ptr_fas()->is_graph_calculated() == GS_GOOD)
   {
      C_STR_STRLIST *pParse;
      
      Stm += _T("-- Funzioni di calcolo grafico per la geometria");
      Stm += GS_LFSTR;

      if (gsc_LispParser(pCls->ptr_fas()->fun_graph_calc, &pParse) == GS_BAD) return GS_BAD;
      PLPGSQLFunc.clear();
      if (gsc_LispParserToPLPGSQL(pParse, PLPGSQLFunc, pCls) == GS_BAD) return GS_BAD;
      // Commento le righe perchè devono essere "verificate" manualmente
      PLPGSQLFunc.addPrefixSuffix(_T("-- "));
      PLPGSQLFunc.removePrefixSuffix(NULL, GS_LFSTR);
      PLPGSQLFunc.strtran(GS_LFSTR, _T("\n-- ")); 
      Stm += PLPGSQLFunc;
      Stm += GS_LFSTR;
   }

   // Aggiorno fattore di aggregazione geometria
   if (pGphInfo->aggr_factor_attrib.len() > 0)
   {
      Stm += _T("-- Aggiornamento fattore di aggregazione geometria");
      Stm += GS_LFSTR;
      Stm += _T("EXECUTE 'UPDATE ");
      Stm += pGphInfo->TableRef;
      Stm += _T(" SET ");
      Stm += pGphInfo->aggr_factor_attrib;
      Stm += _T("=$1 WHERE ");
      Stm += pGphInfo->ent_key_attrib;

      if (pGphInfo->LblTableRef.len() > 0)
         Stm += _T("=$2' USING m_geom_count + m_lbl_count, entity_id; ");
      else
         Stm += _T("=$2' USING m_geom_count, entity_id; ");

      Stm += GS_LFSTR;
   }

   // Legge i dati da etichettare
   if (pCls->ptr_attrib_list()->is_visible() == GS_GOOD)
   {
      Stm += _T("-- Lettura dati da etichettare");
      Stm += GS_LFSTR;

      Stm += _T("EXECUTE 'SELECT ");

      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
      param_ndx = 0;
      Stm1.clear();
      while (pAttrib)
      {
         // se è un attributo etichettato
         if (pAttrib->is_visible() == GS_GOOD)
         {
            if (param_ndx > 0)
            {
               Stm += _T(",");
               Stm1 += _T(",");
            }
            param_ndx++;

            AttribName = pAttrib->get_name();
            if (gsc_AdjSyntax(AttribName, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD)
               return GS_BAD;
            Stm += AttribName; // nome attributo

            if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
               return GS_BAD;
            Stm1 += PLPGSQLVariable;
         }

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
      Stm += _T(" FROM ");
      Stm += pInfo->OldTableRef;
      Stm += _T(" WHERE ");
      Stm += pInfo->key_attrib;
      Stm += _T("=$1' INTO ");
      Stm += Stm1;
      Stm += _T(" USING entity_id;");
      Stm += GS_LFSTR;

      if (pCls->get_type() == TYPE_TEXT)
      {
         // Aggiorno le etichette
         Stm += _T("-- Aggiornamento etichette");
         Stm += GS_LFSTR;

         pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->getFirstVisibleAttrib();
         Stm += _T("EXECUTE 'UPDATE ");
         Stm += pGphInfo->TableRef;
         Stm += _T(" SET ");
         Stm += pGphInfo->text_attrib;
         Stm += _T("=$1 WHERE ");
         Stm += pGphInfo->ent_key_attrib;
         Stm += _T("=$2' USING ");

         if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
            return GS_BAD;

         // per sicurezza correggo le dimensioni dei valori
         Stm += _T("SUBSTRING(");
         Stm += PLPGSQLVariable;
         Stm += _T(" FOR ");
         Stm += pAttrib->len;
         Stm += _T(")");

         Stm += _T(",entity_id;");
         Stm += GS_LFSTR;

         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }
      else if (pGphInfo->LblTableRef.len() > 0)
      {
         // Aggiorno le etichette
         Stm += _T("-- Aggiornamento etichette");
         Stm += GS_LFSTR;

         pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->get_head();
         while (pAttrib)
         {
            // se è un attributo etichettato
            if (pAttrib->is_visible() == GS_GOOD)
            {
               Stm += _T("EXECUTE 'UPDATE ");
               Stm += pGphInfo->LblTableRef;
               Stm += _T(" SET ");
               Stm += pGphInfo->text_attrib;
               Stm += _T("=$1 WHERE ");
               Stm += pGphInfo->ent_key_attrib;
               if (pGphInfo->LblGroupingTableRef.len() > 0)
               {
                  Stm += _T(" IN (SELECT ");
                  Stm += pGphInfo->key_attrib;
                  Stm += _T(" FROM ");
                  Stm += pGphInfo->LblGroupingTableRef;
                  Stm += _T(" WHERE ");
                  Stm += pGphInfo->ent_key_attrib;
                  Stm += _T("=$2) AND ");
               }
               else
                  Stm += _T("=$2 AND ");

               Stm += pGphInfo->attrib_name_attrib;
               Stm += _T("=''");
               Stm += pAttrib->name;
               Stm += _T("''' USING ");

               if (gsc_AttribNameToPLPGSQLVariable(pAttrib->name, PLPGSQLVariable) == GS_BAD)
                  return GS_BAD;

               // per sicurezza correggo le dimensioni dei valori
               Stm += _T("SUBSTRING(");
               Stm += PLPGSQLVariable;
               Stm += _T(" FOR ");
               Stm += pAttrib->len;
               Stm += _T(")");

               Stm += _T(",entity_id;");
               Stm += GS_LFSTR;
            }

            pAttrib = (C_ATTRIB *) pAttrib->get_next();
         }
      }

      if (pGphInfo->LblTableRef.len() > 0 && pGphInfo->aggr_factor_attrib.len() > 0)
      {  // Aggiorno fattore di aggregazione etichette
         Stm += _T("-- Aggiornamento fattore di aggregazione etichette");
         Stm += GS_LFSTR;

         Stm += _T("EXECUTE 'UPDATE ");
         if (pGphInfo->LblGroupingTableRef.len() > 0)
            Stm += pGphInfo->LblGroupingTableRef;
         else
            Stm += pGphInfo->LblTableRef;
         Stm += _T(" SET ");
         Stm += pGphInfo->aggr_factor_attrib;
         Stm += _T("=$1 WHERE ");
         Stm += pGphInfo->ent_key_attrib;

         if (pGphInfo->LblTableRef.len() > 0)
            Stm += _T("=$2' USING m_geom_count + m_lbl_count, entity_id; ");
         else
            Stm += _T("=$2' USING m_geom_count, entity_id; ");

         Stm += GS_LFSTR;
      }
   }

   Stm += _T("RETURN NULL; ");
   Stm += GS_LFSTR;
   Stm += _T("EXCEPTION ");
   Stm += GS_LFSTR;
   Stm += _T("WHEN OTHERS THEN ");
   Stm += GS_LFSTR;
   Stm += _T("RAISE EXCEPTION '*** GS ***  Calculation failed on ");
   Stm += pInfo->OldTableRef;
   Stm += _T(" %', m_err_msg; ");
   Stm += GS_LFSTR;
   Stm += _T("RETURN m_err_msg; ");
   Stm += GS_LFSTR;
   Stm += _T("END; ");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$ ");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE ");
   Stm += GS_LFSTR;
   Stm += _T("COST 100; ");
   if (pGphInfo->getDBConnection()->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += FullRefFuncName;
   Stm += _T("(entity_id integer, insert_mode integer) TO mapserver");

   return pGphInfo->getDBConnection()->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_LispParser_SkipDummyChrs                           <internal> */
/*+
  Funzioni di ausilio alla gsc_LispParser. Serve per saltare tutti i caratteri 
  inutili al parsing di una funzione lisp.
  Salta spazi, tabulazioni, ritorno a capo e commenti.
  Parametri:
  const TCHAR *Lisp;      Espressione lisp (input)
  long *ptr;              Puntatore nell'espressione lisp (input/output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_LispParser_SkipDummyChrs(const TCHAR *Lisp, long *ptr)
{
   while (Lisp[*ptr] != _T('\0'))
      if (Lisp[*ptr] == _T(' ') ||  // Salto gli spazi
          Lisp[*ptr] == _T('\t') || // Salto i '\t'
          Lisp[*ptr] == _T('\n'))   // Salto i '\n' 
         (*ptr)++;
      else if (*Lisp == _T(';')) // commento -> vado in fondo riga
         while (Lisp[++(*ptr)] != _T('\n') && Lisp[*ptr] != _T('\0'));
      else
         break;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_LispParser                                         <internal> */
/*+
  Funzione che fa il parsing di una espressione lisp e lo memorizza in una 
  struttura C_STR_STRLIST composta da una stringa e una lista (opzionale)
  di altre C_STR_STRLIST. Se la stringa inizia per "(" si tratta di una
  funzione e allora la lista C_STR_STRLIST conterrà tutti i parametri.
  Parametri:
  const TCHAR *Lisp;      Espressione lisp
  long *ptr;              Puntatore nell'espressione lisp
  C_STR_STRLIST **pParse; Struttura di parsing

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_LispParser(const TCHAR *Lisp, long *ptr, C_STR_STRLIST **pParse)
{
   static TCHAR   TokenSeparators[] = _T(" ()");
	C_STRING       Token;
   bool           CtrlChar;
   C_STR_STRLIST  *p;

   // Vado all'inizio del Token saltando i caratteri inutili
   if (gsc_LispParser_SkipDummyChrs(Lisp, ptr) == GS_BAD) return GS_BAD;

   if (Lisp[*ptr] == _T(')')) return GS_GOOD;

   // Vado in fondo al Token 
   CtrlChar = false;
   while (Lisp[*ptr] != _T('\0'))
      if (Lisp[*ptr] == _T('\"')) // Stringa
      {
         do
         {
            if (CtrlChar) // se il carattere precedente era un carattere di controllo "\"
               CtrlChar = false;
            else
               if (Lisp[*ptr] == _T('\\')) CtrlChar = true; // Carattere di controllo

            Token += Lisp[*ptr];
            (*ptr)++;
         }
         while (Lisp[*ptr] != _T('\0') && (Lisp[*ptr] != _T('\"') || CtrlChar)); // se il carattere precedente era un carattere di controllo "\"
         Token += _T('\"');
         (*ptr)++; // vado al carattere successivo dando per scontato che la stringa finisca con un "
         break;
      }
      else
      {
         if (wcschr(TokenSeparators, Lisp[*ptr]) && Token.len() > 0) break; // se è un carattere di speratore tra token
         Token += Lisp[*ptr];
         (*ptr)++;
      }

   if ((p = new C_STR_STRLIST(Token.get_name())) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if (*pParse == NULL)
      *pParse = p;
   else
      if ((*pParse)->add_tail(p) == GS_BAD) return GS_BAD;

   if (Token.get_chr(0) == _T('(')) // se inizia per "(" è una funzione
   {  // considero i parametri della funzione
      while (Lisp[*ptr] != _T('\0') && Lisp[*ptr] != _T(')'))
         if (gsc_LispParser(Lisp, ptr, &p) == GS_BAD) return GS_BAD;

      if (Lisp[*ptr] == _T(')')) (*ptr)++;
   }

   return GS_GOOD;
}
int gsc_LispParser(const TCHAR *Lisp, C_STR_STRLIST **pParse)
{
   long ptr = 0;

   if (!Lisp || !pParse) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 

   if ((*pParse = new C_STR_STRLIST(_T("("))) == NULL) // root
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   while (Lisp[ptr] != _T('\0'))
      if (gsc_LispParser(Lisp, &ptr, pParse) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER POSTGIS
// INIZIO FUNZIONI PER ESPORTAZIONE IN SHAPE
///////////////////////////////////////////////////////////////////////////////


// roby shape

/*************************************************************************/
/*.doc gs_shapeExport                                         <external> */
/*+
  Questa funzione LISP esporta in shape tutte le entità estratte nella sessione 
  corrente delle classi in elenco.
  I parametri LISP sono:
  (<dir output><cls list>[<check_geometry_columns>])
  <dir output> = cartella non ancora esistente dove generare gli shape
  <cls list> = lista dei codici delle classi; se = nil o non specificato significa 
               tutte le classi estratte nella sessione corrente
  <AllCharDataType> =  Flag, Se T tutti i tipi di dato saranno considerati 
                       char altrimenti verranno creati i campi con i tipi 
                       corrispondenti al DB (default = false)

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_shapeExport(void)
{
   bool       AllCharDataType = false;
   C_STRING   dir;
   C_INT_LIST ClsCodeList;
   presbuf    arg;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // cartella di output
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   dir = arg->resval.rstring;

   // lista dei codici delle classi
   if (((arg = arg->rbnext)) != NULL)
   {
      if (arg->restype != RTNIL)
      {
         if (ClsCodeList.from_rb(arg) == GS_BAD) return RTERROR;
         arg = gsc_scorri(arg); // vado in fondo alla lista dei codici delle classi
      }
      // AllCharDataType (opzionale)
      if (arg && (arg = arg->rbnext))
         if (gsc_rb2Bool(arg, &AllCharDataType) == GS_BAD) return RTERROR;
   }

   if (gsc_shapeExport(dir, ClsCodeList, AllCharDataType) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_shapeExport                                        <external> */
/*+
  Questa funzione LISP esporta in shape tutte le entità estratte nella sessione 
  corrente delle classi in elenco.
  Parametri:
  C_STRING &dir;           Cartella non ancora esistente dove generare gli shape
  C_INT_LIST &ClsCodeList; Lista dei codici delle classi, se la lista è vuota
                           significa tutte le classi estratte nella sessopne corrente
  bool AllCharDataType;    Opzionale; Flag, Se true tutti i tipi di dato saranno considerati 
                           char altrimenti verranno creati i campi con i tipi 
                           corrispondenti al DB (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_shapeExport(C_STRING &dir, C_INT_LIST &ClsCodeList, bool AllCharDataType)
{
   C_INT_LIST       _ClsCodeList;
   C_STRING         ScriptPath;
   C_AREACLASS_LIST WrkSessionClsList;
   C_INT_INT_LIST   EmptyClsSubCodeList;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (gsc_check_op(opExport) == GS_BAD) return GS_BAD;

   // ricavo la lista delle classi estratte nella sessione corrente
   if (GS_CURRENT_WRK_SESSION->get_class_list(WrkSessionClsList, true) == GS_BAD)
      return GS_BAD;

   // verifico se esisteva già il direttorio di output degli shape 
   if (gsc_path_exist(dir) == GS_GOOD)
      { GS_ERR_COD = eGSPathAlreadyExisting; return GS_BAD; }
   if (gsc_mkdir(dir) == GS_BAD) return GS_BAD;

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_AREACLASS *pWrkSessionCls;

      pWrkSessionCls = (C_AREACLASS *) WrkSessionClsList.get_head();
      while (pWrkSessionCls)
      {
         _ClsCodeList.add_tail_int(pWrkSessionCls->get_key());
         pWrkSessionCls = (C_AREACLASS *) WrkSessionClsList.get_next();
      }
   }
   else // scarto le classi che non sono ancora state estratte
   {
      ClsCodeList.copy(&_ClsCodeList);
      C_INT *pClsCode = (C_INT *) _ClsCodeList.get_head();
      while (pClsCode)
         if (WrkSessionClsList.search_key(pClsCode->get_key()) == NULL)
         {
            _ClsCodeList.remove_at();
            pClsCode = (C_INT *) _ClsCodeList.get_cursor();
         }
         else
            pClsCode = (C_INT *) _ClsCodeList.get_next();
   }

   // preparo le tabelle OD
   if (gsc_setGoogleEarthODRecord(_ClsCodeList, AllCharDataType, EmptyClsSubCodeList) == GS_BAD) return GS_BAD;

   // preparo lo script
   ScriptPath = GEOsimAppl::CURRUSRDIR;
   ScriptPath += _T('\\');
   ScriptPath += GEOTEMPDIR;
   ScriptPath += _T("\\SHAPE_EXPORT.SCR");
   if (gsc_writeScriptForShapeExport(ScriptPath, _ClsCodeList, dir, &EmptyClsSubCodeList) == GS_BAD) return GS_BAD;
   // lancio lo script scritto in precedenza
   gsc_callCmd(_T("_.SCRIPT"), RTSTR, ScriptPath.get_name(), 0);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getClassExpressionFieldMappingsFromOD                    <internal> */
/*+
  Funzione che ricava un'espressione che contiene gli attributi alfanumerici da esportare
  tramite il comando MAPEXPORT di autocad map.
  I dati sono in una tabella dati oggetto.
  Parametri:
  C_STRING &ClassExpressionFieldMappings; espressione che contiene gli attributi alfanumerici da esportare
  C_STRING &ODTableName;                  opzionale; nome della tabella dati oggetti da cui leggere i valori

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getClassExpressionFieldMappingsFromOD(C_STRING &ClassExpressionFieldMappings, C_STRING &ODTableName)
{
   C_RB_LIST ODStru;
   presbuf   pRb, pRbName, pRbType;
   int      i = 0;

   ClassExpressionFieldMappings.clear();

   if ((ODStru << gsc_getStruFromODTable(ODTableName.get_name())) == NULL)
      return GS_BAD;

   while ((pRb = ODStru.nth(i++)))
   {
      ClassExpressionFieldMappings += _T("<NameValuePair>");

      pRbName = gsc_nth(0, pRb);
      ClassExpressionFieldMappings += _T("<Name>");
      ClassExpressionFieldMappings += pRbName;
      ClassExpressionFieldMappings += _T("</Name>");
      ClassExpressionFieldMappings += _T("<Value>:");
      ClassExpressionFieldMappings += pRbName;
      ClassExpressionFieldMappings += _T("@");
      ClassExpressionFieldMappings += ODTableName;
      ClassExpressionFieldMappings += _T("</Value>");

      pRbType = gsc_nth(1, pRb);

      switch (pRbType->resval.rint)
      {
         case AcMap::kCharacter:
            ClassExpressionFieldMappings += _T("<Datatype>CharacterDataType</Datatype>");
            break;
         case AcMap::kInteger:
            ClassExpressionFieldMappings += _T("<Datatype>IntegerDataType</Datatype>");
            break;
         case AcMap::kReal:
            ClassExpressionFieldMappings += _T("<Datatype>DoubleDataType</Datatype>");
            break;
         default:
            GS_ERR_COD = eGSInvAttribType;
            return GS_BAD;
      }
            
      ClassExpressionFieldMappings += _T("</NameValuePair>");
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_writeMapExportProfileFile                          <internal> */
/*+
  Funzione che scrive un file di profile per il comando MAPEXPORT di autocad map.
  Parametri:
  C_STRING &Path;       percorso completo file di profilo
  int ObjectType;       tipi di oggetti da esportare (TYPE_POLYLINE, TYPE_TEXT, TYPE_NODE, TYPE_SURFACE)
  C_STRING &ClassExpressionFieldMappings; espressione che contiene gli attributi alfanumerici da esportare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_writeMapExportProfileFile(C_STRING &Path, int ObjectType, C_STRING &ClassExpressionFieldMappings)
{
   FILE     *file;
   C_STRING Buffer;
      
   Buffer = _T("<AdMapExportProfile version=\"2.1.3\">");
   Buffer += _T("<LoadedProfileName></LoadedProfileName>");

   Buffer += _T("<StorageOptions>");
   Buffer += _T("<StorageType>FileOneEntityType</StorageType>");
   Buffer += _T("<GeometryType>");
   switch (ObjectType)
   {
      case TYPE_POLYLINE:
         Buffer += _T("Line");
         break;
      case TYPE_TEXT:
         Buffer += _T("Text");
         break;
      case TYPE_NODE:
         Buffer += _T("Point");
         break;
      case TYPE_SURFACE:
         Buffer += _T("Polygon");
         break;
      default:
         GS_ERR_COD = eGSInvClassType;
         return GS_BAD;
   }
   Buffer += _T("</GeometryType>");
   Buffer += _T("<FilePrefix></FilePrefix>");
   Buffer += _T("</StorageOptions>");

   Buffer += _T("<SelectionOptions><UseSelectionSet>1</UseSelectionSet><UseAutoSelection>0</UseAutoSelection></SelectionOptions>");

   Buffer += _T("<TranslationOptions>");
   if (ObjectType == TYPE_SURFACE)
      Buffer += _T("<TreatClosedPolylinesAsPolygons>1</TreatClosedPolylinesAsPolygons>");
   else
      Buffer += _T("<TreatClosedPolylinesAsPolygons>0</TreatClosedPolylinesAsPolygons>");
   Buffer += _T("<ExplodeBlocks>1</ExplodeBlocks>");
   Buffer += _T("<LayersToLevels><MapLayersToLevels>0</MapLayersToLevels><LayerToLevelMapping/></LayersToLevels>");
   Buffer += _T("</TranslationOptions>");

   Buffer += _T("<TopologyOptions><GroupComplexPolygons>0</GroupComplexPolygons><TopologyName></TopologyName></TopologyOptions>");

   Buffer += _T("<LayerOptions><DoFilterByLayer>0</DoFilterByLayer><LayerList></LayerList></LayerOptions>");

   Buffer += _T("<FeatureClassOptions><DoFilterByFeatureClass>0</DoFilterByFeatureClass><FeatureClassList></FeatureClassList></FeatureClassOptions>");

   Buffer += _T("<TableDataOptions><TableDataType>None</TableDataType><Name></Name><SQLKeyOnly>0</SQLKeyOnly></TableDataOptions>");

   Buffer += _T("<CoordSysOptions><DoCoordinateConversion>0</DoCoordinateConversion><CoordSysName></CoordSysName></CoordSysOptions>");

   Buffer += _T("<TargetNameOptions><FormatName>SHP</FormatName></TargetNameOptions>");

   Buffer += _T("<DriverOptions/>");

   Buffer += _T("<UseUniqueKeyField>0</UseUniqueKeyField><UseUniqueKeyFieldName>AdMapKey</UseUniqueKeyFieldName>");
   
   Buffer += _T("<ExpressionFieldMappings>");
   if (ObjectType == TYPE_NODE)
   {
      Buffer += _T("<NameValuePair><Name>NOMEBLOCCOACAD</Name><Value>.BLOCKNAME</Value><Datatype>CharacterDataType</Datatype></NameValuePair>");
      Buffer += _T("<NameValuePair><Name>ANGOLOACAD</Name><Value>.ANGLE</Value><Datatype>DoubleDataType</Datatype></NameValuePair>");
   }
   Buffer += ClassExpressionFieldMappings;
   Buffer += _T("</ExpressionFieldMappings>");
   Buffer += _T("</AdMapExportProfile>");

   // La scrittura deve essere in UTF8 per supporto ai caratteri àùèò...
   if ((file = gsc_fopen(Path, _T("w,ccs=UTF-8"))) == NULL)
      return GS_BAD;
   if (fwprintf(file, Buffer.get_name()) < 0)
      { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return gsc_fclose(file);
}


/*************************************************************************/
/*.doc gsc_getScriptCommandForShapeExport                     <internal> */
/*+
  Funzione che prepara una stringa per lo script di esportazione.
  Parametri:
  C_STRING &ShapeFile;
  C_STRING &EPF;
  int ObjectType;
  C_STRING &Buffer

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getScriptCommandForShapeExport(C_STRING &ShapeFile, C_STRING &EPF, int ObjectType, C_STRING &Buffer)
{
   C_STRING dummy;

   Buffer = _T("-MAPEXPORT\n");

   Buffer += _T("SHP\n"); // Immettere il tipo di file in cui esportare o ? per un elenco <>:

   dummy = ShapeFile;
   //dummy.strtran(_T("\\"), _T("/"));
   Buffer += dummy; // Immettere il nome del file da creare <>:
   Buffer += _T("\n");

   if (EPF.len() > 0)
   {
      Buffer += _T("_Y\n"); // Caricare le opzioni dal file? [Sì/No] <>:

      dummy = EPF;
      //dummy.strtran(_T("\\"), _T("/"));
      Buffer += dummy; // Immettere il percorso e il nome del file .EPF per le opzioni <>:
      Buffer += _T("\n");
   }
   else
      Buffer += _T("_N\n"); // Caricare le opzioni dal file? [Sì/No] <>:

   Buffer += _T("_S\n"); // Modifica opzioni  [Seleziona/Dati/Opzioni/Procedi] <>:

   // Immettere un'opzione di tipo entità per il file di esportazione [punTo/Linea/Poligono/teSto] <>:
   switch (ObjectType)
   {
      case TYPE_POLYLINE:
         Buffer += _T("_Line\n");
         break;
      case TYPE_TEXT:
         Buffer += _T("_Text\n");
         break;
      case TYPE_NODE:
         Buffer += _T("_Point\n");
         break;
      case TYPE_SURFACE:
         Buffer += _T("_Polygon\n");
         break;
      default:
         GS_ERR_COD = eGSInvClassType;
         return GS_BAD;
   }

   Buffer += _T("_M\n"); // Selezione oggetti per esportazione [Tutti/Manuale] <>:
   
   Buffer += _T("!ssfilter\n\n"); // Selezionare oggetti: !ssfilter

   Buffer += _T("*\n"); // Immettere il nome dei layer da filtrare per gli oggetti o ? per un elenco <*>:

   Buffer += _T("*\n"); // Immettere il nome o i nomi delle classi da filtrare per gli oggetti o ? per un elenco <*>:

   Buffer += _T("_N\n"); // Esportare la topologia poligono? [Sì/No]  <>:

   Buffer += _T("_P\n"); // Modifica opzioni  [Seleziona/Dati/Opzioni/Procedi] <>:

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_isSelSetEmptyForMapExport                          <inernal> */
/*+
  Questa funzione che verifica che nel gruppo di selezione ci sia almeno un oggetto del tipo
  desiderato dal comando MAPEXPORT.
  Parametri:
  C_SELSET &ss;
  int Type

  Restituisce true se il gruppo NON ha oggetti del tipo voluto altrimenti restituisce false.
-*/  
/*************************************************************************/
bool gsc_isSelSetEmptyForMapExport(C_SELSET &ss, int Type)
{
   ads_name ent;
   long     i = 0;

   while (ss.entname(i++, ent) == GS_GOOD)
      switch (Type)
      {
         case TYPE_POLYLINE:
            if (gsc_isLinearEntity(ent)) return false;
            break;
         case TYPE_NODE:
            if (gsc_isPunctualEntity(ent) && !gsc_isText(ent)) return false;      
            break;
         case TYPE_TEXT:
            if (gsc_isText(ent) == GS_GOOD) return false;
            break;
         case TYPE_SURFACE:
            if (gsc_isClosedEntity(ent)) return false;
            break;
      }

   return true;
}


/*************************************************************************/
/*.doc gsc_writeScriptForShapeExport                          <external> */
/*+
  Funzione che prepara i files epf e scrive uno script per l'esportazione in shape
  di tutte le entità estratte nella sessione corrente delle classi in elenco.
  Parametri:
  C_STRING &ScriptPath; Path del file di script
  C_INT_LIST &ClassCodeList; Lista dei codici delle classi da esportare
  C_STRING &ShapeDir;   cartella dove scrivere gli shape
  C_INT_INT_LIST *pEmptyClsSubCodeList; opzionale; Lista dei codici e sottocodici delle classi/sottoclassi
                                        che sono senza oggetti grafici nella sessione corrente

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_writeScriptForShapeExport(C_STRING &ScriptPath, C_INT_LIST &ClassCodeList, C_STRING &ShapeDir,
                                  C_INT_INT_LIST *pEmptyClsSubCodeList)
{
   C_INT   *pClassCode = (C_INT *) ClassCodeList.get_head();
   C_CLASS *pCls;
   int      Result = GS_GOOD, cls;
   C_STRING ClassExpressionFieldMappings, EPF, PrefixPath, ODTableName, ShapeFile, Buffer, CompleteName;
   FILE     *file;
   resbuf   rb;
   int      undoCtl;
   C_CLASS_SQL_LIST SQLCondList;
   C_LINK_SET       LinkSet;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   acedGetVar(_T("UNDOCTL"), &rb);
   gsc_rb2Int(&rb, &undoCtl);

   // 0  = Il comando ANNULLA è disattivato 
   // 1  = Il comando ANNULLA è attivato
   bool isUndoOn = (undoCtl & 1) == 1; // UNDO abilitato (Control)

   gsc_ssget(_T("X"), NULL, NULL, NULL, *(LinkSet.ptr_SelSet()));

   PrefixPath = GEOsimAppl::CURRUSRDIR;
   PrefixPath += _T('\\');
   PrefixPath += GEOTEMPDIR;
   PrefixPath += _T('\\');

   if ((file = gsc_fopen(ScriptPath, _T("w"))) == NULL)
      return GS_BAD;

   if (isUndoOn)
   {
      Buffer = _T("_.UNDO\n_Control\n_None\n"); // diabilito UNDO
      if (fwprintf(file, Buffer.get_name()) < 0) return GS_BAD;
   }

   Buffer = _T("(setq ss (ssget \"X\"))\n");
   if (fwprintf(file, Buffer.get_name()) < 0) return GS_BAD;

   while (pClassCode)
   {
      cls = pClassCode->get_key();
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL)
         { Result = GS_BAD; break; }

      ClassExpressionFieldMappings.clear();

      if (pCls->get_category() == CAT_EXTERN)
      {
         C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

         while (pSub)
         {  // se la sottoclasse non ha oggetti
            if (pEmptyClsSubCodeList && pEmptyClsSubCodeList->search(cls, pSub->ptr_id()->sub_code))
               { pSub = (C_SUB *) pSub->get_next(); continue; }

            // filtro tutti gli oggetti principali della classe      
            if (gsc_do_sql_filter(cls, pSub->ptr_id()->sub_code, SQLCondList, &LinkSet) == GS_BAD)
               { Result = GS_BAD; break; }
            if (gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), pSub->get_type()) == false) // se ha oggetti grafici
            {
               LinkSet.subtract(*(GS_LSFILTER.ptr_SelSet()));

               Buffer = _T("(gs_do_sql_filter ");
               Buffer += cls;
               Buffer += _T(" ");
               Buffer += pSub->ptr_id()->sub_code;
               Buffer += _T(")\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                 { Result = GS_BAD; break; }

               Buffer = _T("(setq i -1)\n(if ssfilter (while (setq e (ssname ssfilter (setq i (1+ i)))) (ssdel e ss)))\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                  { Result = GS_BAD; break; }

               // ricavo il nome della tabella OD per la classe
               gsc_getGoogleEarthODTableNameFromCls(pSub, ODTableName);
               // genero la lista degli attributi alfanumerici da esportare
               if (gsc_getClassExpressionFieldMappingsFromOD(ClassExpressionFieldMappings, ODTableName) == GS_BAD)
                  { Result = GS_BAD; break; }
               // genero il file epf
               EPF = PrefixPath;
               EPF += ODTableName;
               EPF += _T(".EPF");
               if (gsc_writeMapExportProfileFile(EPF, pSub->get_type(), ClassExpressionFieldMappings) == GS_BAD)
                  { Result = GS_BAD; break; }
               ShapeFile = ShapeDir;
               ShapeFile += _T('\\');
               pSub->get_CompleteName(CompleteName);
               ShapeFile += CompleteName;
               ShapeFile += _T(".SHP");
               if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, pSub->get_type(), Buffer) == GS_BAD)
                  { Result = GS_BAD; break; }
               if (fwprintf(file, Buffer.get_name()) < 0)
                 { Result = GS_BAD; break; }
               
               Buffer = _T("(gs_disable_reactors)\n(ade_oddeletetab \"");
               Buffer += ODTableName;
               Buffer += _T("\")\n(gs_enable_reactors)\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                 { Result = GS_BAD; break; }
            }

            pSub = (C_SUB *) pSub->get_next();
         }
         if (Result == GS_BAD) break;
      }
      else if (pCls->get_category() == CAT_SPAGHETTI)
      {
         // se la classe non ha oggetti
         if (pEmptyClsSubCodeList && pEmptyClsSubCodeList->search(cls, 0))
            { pClassCode = (C_INT *) ClassCodeList.get_next(); continue; }

         bool exportLine, exportText, exportPoint, exportSurface;

         // filtro tutti gli oggetti principali della classe      
         if (gsc_do_sql_filter(cls, 0, SQLCondList, &LinkSet) == GS_BAD) { Result = GS_BAD; break; }
         LinkSet.subtract(*(GS_LSFILTER.ptr_SelSet()));
         // tipo linea
         exportLine = !gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), TYPE_POLYLINE); // se ha oggetti grafici
         exportText = !gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), TYPE_TEXT); // se ha oggetti grafici
         exportPoint = !gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), TYPE_NODE); // se ha oggetti grafici
         exportSurface = !gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), TYPE_SURFACE); // se ha oggetti grafici

         Buffer = _T("(gs_do_sql_filter ");
         Buffer += cls;
         Buffer += _T(" 0 ss)\n");
         if (fwprintf(file, Buffer.get_name()) < 0)
            { Result = GS_BAD; break; }

         Buffer = _T("(setq i -1)\n(if ssfilter (while (setq e (ssname ssfilter (setq i (1+ i)))) (ssdel e ss)))\n");
         if (fwprintf(file, Buffer.get_name()) < 0)
            { Result = GS_BAD; break; }

         // ricavo il nome della tabella OD per la classe
         gsc_getGoogleEarthODTableNameFromCls(pCls, ODTableName);
         pCls->get_CompleteName(CompleteName);
         // per ogni tipo geometrico genero il file epf

         // tipo linea
         if (exportLine) // se ha oggetti grafici
         {
            EPF = PrefixPath;
            EPF += ODTableName;
            EPF += _T("_POLYLINE.EPF");
            if (gsc_writeMapExportProfileFile(EPF, TYPE_POLYLINE, ClassExpressionFieldMappings) == GS_BAD)
               { Result = GS_BAD; break; }
            ShapeFile = ShapeDir;
            ShapeFile += _T('\\');
            ShapeFile += CompleteName;
            ShapeFile += _T("_POLYLINE.SHP");
            if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, TYPE_POLYLINE, Buffer) == GS_BAD)
               { Result = GS_BAD; break; }
            if (fwprintf(file, Buffer.get_name()) < 0)
               { Result = GS_BAD; break; }
         }
         // tipo testo
         if (exportText) // se ha oggetti grafici
         {
            EPF = PrefixPath;
            EPF += ODTableName;
            EPF += _T("_TEXT.EPF");
            if (gsc_writeMapExportProfileFile(EPF, TYPE_TEXT, ClassExpressionFieldMappings) == GS_BAD)
               { Result = GS_BAD; break; }
            ShapeFile = ShapeDir;
            ShapeFile += _T('\\');
            ShapeFile += CompleteName;
            ShapeFile += _T("_TEXT.SHP");
            if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, TYPE_TEXT, Buffer) == GS_BAD)
               { Result = GS_BAD; break; }
            if (fwprintf(file, Buffer.get_name()) < 0)
               { Result = GS_BAD; break; }
         }
         // tipo punto
         if (exportPoint) // se ha oggetti grafici
         {
            EPF = PrefixPath;
            EPF += ODTableName;
            EPF += _T("_NODE.EPF");
            if (gsc_writeMapExportProfileFile(EPF, TYPE_NODE, ClassExpressionFieldMappings) == GS_BAD)
               { Result = GS_BAD; break; }
            ShapeFile = ShapeDir;
            ShapeFile += _T('\\');
            ShapeFile += CompleteName;
            ShapeFile += _T("_NODE.SHP");
            if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, TYPE_NODE, Buffer) == GS_BAD)
               { Result = GS_BAD; break; }
            if (fwprintf(file, Buffer.get_name()) < 0)
               { Result = GS_BAD; break; }
         }
         // tipo superficie
         if (exportSurface) // se ha oggetti grafici
         {
            EPF = PrefixPath;
            EPF += ODTableName;
            EPF += _T("_SURFACE.EPF");
            if (gsc_writeMapExportProfileFile(EPF, TYPE_SURFACE, ClassExpressionFieldMappings) == GS_BAD)
               { Result = GS_BAD; break; }
            ShapeFile = ShapeDir;
            ShapeFile += _T('\\');
            ShapeFile += CompleteName;
            ShapeFile += _T("_SURFACE.SHP");
            if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, TYPE_SURFACE, Buffer) == GS_BAD)
               { Result = GS_BAD; break; }
            if (fwprintf(file, Buffer.get_name()) < 0)
               { Result = GS_BAD; break; }
         }
      }
      else
         if (pCls->ptr_fas()) // se ha oggetti grafici
         {
            // se la classe non ha oggetti
            if (pEmptyClsSubCodeList && pEmptyClsSubCodeList->search(cls, 0))
               { pClassCode = (C_INT *) ClassCodeList.get_next(); continue; }

            // filtro tutti gli oggetti principali della classe      
            if (gsc_do_sql_filter(cls, 0, SQLCondList, &LinkSet) == GS_BAD) { Result = GS_BAD; break; }
            LinkSet.subtract(*(GS_LSFILTER.ptr_SelSet()));

            if (gsc_isSelSetEmptyForMapExport(*(GS_LSFILTER.ptr_SelSet()), pCls->get_type()) == false) // se ha oggetti grafici
            {
               Buffer = _T("(gs_do_sql_filter ");
               Buffer += cls;
               Buffer += _T(" 0 ss)\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                  { Result = GS_BAD; break; }

               Buffer = _T("(setq i -1)\n(if ssfilter (while (setq e (ssname ssfilter (setq i (1+ i)))) (ssdel e ss)))\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                  { Result = GS_BAD; break; }

               // ricavo il nome della tabella OD per la classe
               gsc_getGoogleEarthODTableNameFromCls(pCls, ODTableName);
               // genero la lista degli attributi alfanumerici da esportare
               if (gsc_getClassExpressionFieldMappingsFromOD(ClassExpressionFieldMappings, ODTableName) == GS_BAD)
                  { Result = GS_BAD; break; }
               // genero il file epf
               EPF = PrefixPath;
               EPF += ODTableName;
               EPF += _T(".EPF");
               if (gsc_writeMapExportProfileFile(EPF, pCls->get_type(), ClassExpressionFieldMappings) == GS_BAD)
                  { Result = GS_BAD; break; }
               ShapeFile = ShapeDir;
               ShapeFile += _T('\\');
               pCls->get_CompleteName(CompleteName);
               ShapeFile += CompleteName;
               ShapeFile += _T(".SHP");
               if (gsc_getScriptCommandForShapeExport(ShapeFile, EPF, pCls->get_type(), Buffer) == GS_BAD)
                  { Result = GS_BAD; break; }
               if (fwprintf(file, Buffer.get_name()) < 0)
                  { Result = GS_BAD; break; }

               Buffer = _T("(gs_disable_reactors)\n(ade_oddeletetab \"");
               Buffer += ODTableName;
               Buffer += _T("\")\n(gs_enable_reactors)\n");
               if (fwprintf(file, Buffer.get_name()) < 0)
                 { Result = GS_BAD; break; }
            }
         }

      pClassCode = (C_INT *) ClassCodeList.get_next();
   }

   Buffer = _T("_.UNDO\n_All\n"); // diabilito UNDO
   if (fwprintf(file, Buffer.get_name()) < 0) return GS_BAD;

   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   return Result;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER ESPORTAZIONE IN SHAPE
// INIZIO FUNZIONI PER GOOGLE-EARTH
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_getGoogleEarthODTableNameFromCls                  <internal> */
/*+
  Funzione che ricava il nome della tabella OD da usare per esportare i
  valori alfanumerici della classe.
  Parametri:
  C_CLASS *pCls;         Puntatore a classe GEOsim
  C_STRING &ODTableName; Nome della tabella dati oggetto
-*/  
/*************************************************************************/
void gsc_getGoogleEarthODTableNameFromCls(C_CLASS *pCls, C_STRING &ODTableName) // roby shape
{
   // Creo tabella OD per la classe e ne ricavo il puntatore ("GE" sta per Google Earth)
   ODTableName = _T("GE_PRJ");
   ODTableName += pCls->get_PrjId();
   ODTableName += _T("_CLS");
   ODTableName += pCls->ptr_id()->code;
   if (pCls->ptr_id()->sub_code > 0)
   {
      ODTableName += _T("_SUB");
      ODTableName += pCls->ptr_id()->sub_code;
   }
}


/*************************************************************************/
/*.doc gsc_createGoogleEarthODTableFromCls                    <internal> */
/*+
  Funzione che crea una tabella dati oggetto con i campi della classe.
  I campi saranno di tipo carattere per ospitare i dati convertiti nel
  formato windows opportuno (es. numeri con la virgola, o
  nel caso ci sia una lista di scelta a 2 colonne, o date o booleani ...)
  Parametri:
  C_STRING &ODTableName; Nome della tabella dati oggetto
  C_CLASS *pCls;         Puntatore a classe GEOsim
  bool AllCharDataType;  Flag, Se true tutti i tipi di dato saranno considerati 
                         char altrimenti verranno creati i campi con i tipi 
                         corrispondenti al DB (default = false)

  Restituisce il puntatore alla tabella OD in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria.
-*/  
/*************************************************************************/
AcMapODTable *gsc_createGoogleEarthODTableFromCls(C_STRING &ODTableName, C_CLASS *pCls, bool AllCharDataType = true) // roby shape
{
   if (ODTableName.len() == 0 || !pCls) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   // cancello la tabella (nel caso esista già)
   gsc_oddeletetab(ODTableName.get_name());

   return gsc_createODTable(ODTableName, pCls->ptr_attrib_list(), AllCharDataType);
}


/*************************************************************************/
/*.doc gsc_setGoogleEarthODRecord                             <internal> */
/*+
  Funzione che aggiunge o modifica un record di dati oggetto di una
  tabella nota con i valori dei campi della classe.
  I campi saranno di tipo carattere per ospitare i dati convertiti nel
  formato windows opportuno (es. numeri con la virgola, o
  nel caso ci sia una lista di scelta a 2 colonne, o date o booleani ...)
  Parametri:
  ads_name ent;                 Oggetto grafica
  AcMapODTable *pTable;         Tabella dati oggetto
  bool AllCharDataType;         Flag, Se true tutti i tipi di dato saranno considerati 
                                char altrimenti verranno creati i campi con i tipi 
                                corrispondenti al DB
  AcMapODRecordIterator *pIter; Struttura per velocizzare la
                                funzione. Vedi
                                "gsc_GetObjectODRecordIterator"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_setGoogleEarthODRecord(C_CLASS *pCls, ads_name ent,
                               C_PREPARED_CMD_LIST &CmdList, C_RB_LIST &ColValues,
                               AcMapODTable *pTable, bool AllCharDataType, AcMapODRecordIterator *pIter) // roby shape
{
   // interrogo l'entità di GEOsim
   if (pCls->query_data(ent, ColValues, &CmdList) == GS_BAD) return GS_BAD;

   return gsc_setGoogleEarthODRecord(pCls, ColValues, pTable, AllCharDataType, pIter);
}
int gsc_setGoogleEarthODRecord(C_CLASS *pCls, C_LONG_BTREE &KeyList,
                               bool AllCharDataType, AcMapODRecordIterator *pIter) // roby shape
{
   long          key, qty = 0, written = 0;
   int           result = GS_BAD;
   C_RB_LIST     AllColValues, SingleColValues, ColValues;
   presbuf       pRow, p;
   C_LONG_BTREE  PartialKeyList;
   C_BLONG       *pKey = (C_BLONG *) KeyList.go_top();
   C_STRING      ODTableName, CompleteName;
   AcMapODTable  *pTable = NULL;

   pCls->get_CompleteName(CompleteName);

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1056), CompleteName.get_name()); // "Elaborazione classe <%s>"

   // ricavo il nome della tabella OD per la classe
   gsc_getGoogleEarthODTableNameFromCls(pCls, ODTableName);

   // Creo tabella OD per la classe e ne ricavo il puntatore
   if ((pTable = gsc_createGoogleEarthODTableFromCls(ODTableName, pCls, AllCharDataType)) == NULL)
      return GS_BAD;
   

   do
   {
      StatusBarProgressMeter.Init(KeyList.get_count());

      result = GS_GOOD;
      while (pKey)
      {
         key = pKey->get_key();
         PartialKeyList.add(&key);
         qty++;

         if ((qty % VERY_LARGE_STEP) == 0) // ogni 10000 per non avere problemi di out of memory
         {
            if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
               { result = GS_BAD; break; }
            PartialKeyList.remove_all();

            pRow = AllColValues.get_head();
            if (pRow) pRow = pRow->rbnext;

            while (pRow && pRow->restype == RTLB)
            {
               // creo la lista dei valori da inserire
               if ((p = acutBuildList(RTLB, 0)) == NULL) { result = GS_BAD; break; }
               if (gsc_scorcopy(pRow, p) == NULL)
                  { acutRelRb(p); result = GS_BAD; break; }
               ColValues << p;
               // Riga successiva
               if ((pRow = gsc_scorri(pRow)) != NULL) pRow = pRow->rbnext;

               if (gsc_setGoogleEarthODRecord(pCls, ColValues, pTable, AllCharDataType, pIter) == GS_BAD)
                  { result = GS_BAD; break; }

               StatusBarProgressMeter.Set(++written);
            }
         }

         pKey = (C_BLONG *) KeyList.go_next();
      }

      if (result == GS_BAD) break;

      if (PartialKeyList.get_count() > 0)
      {
         if (pCls->query_AllData(PartialKeyList, AllColValues, SingleColValues) == GS_BAD)
            { result = GS_BAD; break; }

         pRow = AllColValues.get_head();
         if (pRow) pRow = pRow->rbnext;

         while (pRow && pRow->restype == RTLB)
         {
            // creo la lista dei valori da inserire
            if ((p = acutBuildList(RTLB, 0)) == NULL) { result = GS_BAD; break; }
            if (gsc_scorcopy(pRow, p) == NULL)
               { acutRelRb(p); result = GS_BAD; break; }
            ColValues << p;
            // Riga successiva
            if ((pRow = gsc_scorri(pRow)) != NULL) pRow = pRow->rbnext;

            if (gsc_setGoogleEarthODRecord(pCls, ColValues, pTable, AllCharDataType, pIter) == GS_BAD)
               { result = GS_BAD; break; }

            StatusBarProgressMeter.Set(++written);
         }
      }
   }
   while (0);
   delete pTable;

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   acutPrintf(gsc_msg(1047), qty); // "\nAttendere rilascio memoria...\n"

   return result;
}
int gsc_setGoogleEarthODRecord(C_CLASS *pCls, C_RB_LIST &ColValues,
                               AcMapODTable *pTable, bool AllCharDataType, AcMapODRecordIterator *pIter) // roby shape
{
   AcDbObjectId       objId;
   AcDbObject         *pObj = NULL; 
   AcMapODTableRecord NewRecord;
   C_SELSET           SelSet;
   C_INFO             *pInfo = pCls->ptr_info();
   presbuf            pRb;
   long               gs_id, i = 0;
   ads_name           ent;

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // estraggo il codice entità
   if ((pRb = ColValues.CdrAssoc(pInfo->key_attrib.get_name())) == NULL || gsc_rb2Lng(pRb, &gs_id) == GS_BAD)
      return GS_BAD;
   // ricavo gli oggetti grafici principali dell'entità
   if (pCls->get_SelSet(gs_id, SelSet, GRAPHICAL) == GS_BAD) return GS_BAD;

   pTable->InitRecord(NewRecord);
   if (gsc_updateAcMapODTableRecord(pCls, ColValues, AllCharDataType, NewRecord) == GS_BAD)
      return GS_BAD;

   // per ogni oggetto del gruppo di selezione
   while (SelSet.entname(i++, ent) == GS_GOOD)
      if (gsc_setGoogleEarthODRecord(ent, pTable, NewRecord, pIter) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}
int gsc_setGoogleEarthODRecord(ads_name ent, AcMapODTable *pTable, AcMapODTableRecord &NewRecord,
                               AcMapODRecordIterator *pIter) // roby shape
{
   AcDbObjectId       objId;
   AcDbObject         *pObj = NULL; 
   AcMapODTableRecord OldRecord;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) 
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk) 
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // leggo l'identificatore dell'entità
   if (pIter->Init(pObj, AcMap::kOpenForWrite, Adesk::kTrue) != AcMap::kOk)
      { pObj->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   for (; pIter->IsDone() == Adesk::kFalse; pIter->Next())
      if (pIter->GetRecord(OldRecord) == AcMap::kOk && 
          gsc_strcmp(pTable->Name(), OldRecord.ODTableName()) == 0)
      {
         if (pIter->UpdateRecord(NewRecord) != AcMap::kOk)
            { pObj->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         
         if (pObj) pObj->close();
         return GS_GOOD;
      }

   if (pTable->AddRecord(NewRecord, pObj) != AcMap::kOk)
      { pObj->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   pObj->close();

   return GS_GOOD;
}
int gsc_updateAcMapODTableRecord(C_CLASS *pCls, C_RB_LIST &ColValues,
                                 bool AllCharDataType, AcMapODTableRecord &Record) // roby shape
{
   C_STRING   Buffer;
   AcMapValue Value;
   int        i = 0, numInteger;
   double     numDouble;
   C_ATTRIB   *pAttrib;
   presbuf    pRb;

   // aggiorno il record già esistente
   do
   {
      // Formatto i valori in una stringa per out su video e in clipboard
      pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->goto_pos(i + 1);
      pRb = gsc_nth(1, ColValues.nth(i));

      if (AllCharDataType || gsc_DBIsNumeric(pAttrib->ADOType) != GS_GOOD)
      {
         if (pAttrib->ParseToString(pRb, Buffer, &ColValues,
                                    pCls->ptr_id()->code,
                                    pCls->ptr_id()->sub_code) == GS_BAD)
            return GS_BAD;
         Record.Value(i++) = (Buffer.len() > 0) ? Buffer.get_name() : GS_EMPTYSTR; 
      }
      else
         if (gsc_DBIsNumericWithDecimals(pAttrib->ADOType) == GS_GOOD)
         {
            if (gsc_rb2Dbl(pRb, &numDouble) == GS_GOOD)
               Record.Value(i++) = numDouble; 
            else
               Record.Value(i++) = AcMap::kUnknownType;
         }
         else
         {
            if (gsc_rb2Int(pRb, &numInteger) == GS_GOOD)
               Record.Value(i++) = numInteger; 
            else
               Record.Value(i++) = AcMap::kUnknownType;
         }
   }
   while (i < Record.Count());

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_setGoogleEarthODRecord                             <external> */
/*+
  Funzione che setta i dati oggetto agli oggetti di GEOsim per
  esportare in Google Earth.
  Parametri:
  C_SELSET &ss;          Gruppo di oggetti grafici
  bool AllCharDataType;  Flag, Se true tutti i tipi di dato saranno considerati 
                         char altrimenti verranno creati i campi con i tipi 
                         corrispondenti al DB (default = false)
  oppure
  C_INT_LIST &ClsCodeList;             Lista dei codici delle classi
  bool AllCharDataType;  Flag, Se true tutti i tipi di dato saranno considerati 
                         char altrimenti verranno creati i campi con i tipi 
                         corrispondenti al DB (default = false)
  C_INT_INT_LIST &EmptyClsSubCodeList; Lista dei codici e sottocodici delle classi/sottoclassi
                                       che sono senza oggetti grafici nella sessione corrente

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_setGoogleEarthODRecord(C_SELSET &ss,  bool AllCharDataType)
{
   C_INT_INT_LIST        ClsSubList;
   C_INT_INT             *pClsSub;
   C_STRING              ODTableName;
   C_CLASS               *pCls;
   AcMapODRecordIterator *pIter = NULL;
   int                   Result = GS_GOOD;
   long                  cnt = 0;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (ss.get_ClsCodeList(ClsSubList) == GS_BAD) return GS_BAD;
   
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) return GS_BAD;

   acutPrintf(GS_LFSTR);
   pClsSub = (C_INT_INT *) ClsSubList.get_head();
   while (pClsSub)
   {
      C_SELSET            Cls_ss;
      C_PREPARED_CMD_LIST CmdList;
      C_RB_LIST           ColValues;
      ads_name            ent;
      AcMapODTable        *pTable = NULL;
      long                i = 0;
      C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1066)); // "Copia dei dati da database a object data"

      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSub->get_key(), pClsSub->get_type())) == NULL)
         { Result = GS_BAD; break; }

      // estraggo tutti gli oggetti principali della classe      
      if (ss.copyIntersectClsCode(Cls_ss, pClsSub->get_key(), pClsSub->get_type()) != GS_GOOD)
         { pClsSub = (C_INT_INT *) ClsSubList.get_next(); continue; }
      ss.subtract(Cls_ss);
      if (Cls_ss.intersectType(GRAPHICAL) != GS_GOOD)
         { pClsSub = (C_INT_INT *) ClsSubList.get_next(); continue; }

      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(CmdList) == GS_BAD) { Result = GS_BAD; break; }

      // ricavo il nome della tabella OD per la classe
      gsc_getGoogleEarthODTableNameFromCls(pCls, ODTableName);

      // Creo tabella OD per la classe e ne ricavo il puntatore
      if ((pTable = gsc_createGoogleEarthODTableFromCls(ODTableName, pCls, AllCharDataType)) == NULL)
         { Result = GS_BAD; break; }

      StatusBarProgressMeter.Init(Cls_ss.length());

      while (Cls_ss.entname(i++, ent) == GS_GOOD)
      {
         StatusBarProgressMeter.Set(i);

         if (gsc_setGoogleEarthODRecord(pCls, ent, CmdList, ColValues,
                                        pTable, AllCharDataType, pIter) == GS_BAD)
            { Result = GS_BAD; break; }
      }
      cnt += (i - 1);
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      delete pTable;
      if (Result == GS_BAD) break;

      pClsSub = (C_INT_INT *) ClsSubList.get_next();
   }

   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(70), cnt); // "%ld oggetti grafici elaborati."
   acutPrintf(GS_LFSTR);

   delete pIter;

   return Result;
}
int gsc_setGoogleEarthODRecord(C_INT_LIST &ClsCodeList,  bool AllCharDataType, C_INT_INT_LIST &EmptyClsSubCodeList)
{
   C_INT            *pClsCode;
   int              Result = GS_GOOD, UndoState;
   C_CLASS          *pCls;
   C_CLASS_SQL_LIST SQLCondList;
   C_LINK_SET       LinkSet;
   AcMapODRecordIterator *pIter = NULL;
   long     qty = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1059)); // "Elaborazione classi"

   EmptyClsSubCodeList.remove_all();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   gsc_ssget(_T("X"), NULL, NULL, NULL, *(LinkSet.ptr_SelSet()));
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) return GS_BAD;

   // disabilito UNDO (velocizza l'operazione)
   UndoState = gsc_SetUndoRecording(FALSE);

   StatusBarProgressMeter.Init(ClsCodeList.get_count());

   pClsCode = (C_INT *) ClsCodeList.get_head();
   while (pClsCode)
   {
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pClsCode->get_key())) == NULL)
         { Result = GS_BAD; break; }

      if (pCls->get_category() == CAT_EXTERN)
      {
         C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

         while (pSub)
         {
            if (pSub->ptr_attrib_list() && pSub->ptr_fas())
            {
               // filtro tutti gli oggetti principali della classe      
               if (gsc_do_sql_filter(pClsCode->get_key(), pSub->ptr_id()->sub_code, SQLCondList, &LinkSet) == GS_BAD)
                  { Result = GS_BAD; break; }
               if (GS_LSFILTER.ptr_KeyList()->get_count() > 0)
               {
                  LinkSet.subtract(*(GS_LSFILTER.ptr_SelSet()));
                  if (gsc_setGoogleEarthODRecord(pSub, *(GS_LSFILTER.ptr_KeyList()), AllCharDataType, pIter) == GS_BAD)
                     { Result = GS_BAD; break; }
               }
               else // vuoto
                  EmptyClsSubCodeList.values_add_tail(pClsCode->get_key(), pSub->ptr_id()->sub_code);
            }
            pSub = (C_SUB *) pCls->ptr_sub_list()->get_next();
         }
         if (Result == GS_BAD) break;
      }
      else
         if (pCls->ptr_attrib_list() && pCls->ptr_fas())
         {
            // filtro tutti gli oggetti principali della classe      
            if (gsc_do_sql_filter(pClsCode->get_key(), 0, SQLCondList, &LinkSet) == GS_BAD)
               { Result = GS_BAD; break; }
            if (GS_LSFILTER.ptr_KeyList()->get_count() > 0)
            {
               LinkSet.subtract(*(GS_LSFILTER.ptr_SelSet()));
               if (gsc_setGoogleEarthODRecord(pCls, *(GS_LSFILTER.ptr_KeyList()), AllCharDataType, pIter) == GS_BAD)
                  { Result = GS_BAD; break; }
            }
            else // vuoto
               EmptyClsSubCodeList.values_add_tail(pClsCode->get_key(), 0);
         }

      StatusBarProgressMeter.Set(++qty);

      pClsCode = (C_INT *) ClsCodeList.get_next();
   }
   delete pIter;
   gsc_SetUndoRecording(UndoState); // Ripristina situazione UNDO precedente

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return Result;
}

/*************************************************************************/
/*.doc gs_setGoogleEarthODRecord                              <external> */
/*+
  Questa funzione LISP che setta i dati oggetto agli oggetti di GEOsim per
  esportare in Google Earth.
  I parametri LISP sono:
  (<selset>[AllCharDataType])

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_setGoogleEarthODRecord(void) // roby shape
{
   presbuf  arg = acedGetArgs();
   C_SELSET ss;
   bool     AllCharDataType = true;

   acedRetNil();

   // ricavo il gruppo di selezione
   if (!arg || arg->restype != RTPICKS)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss << arg->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);
   // valore opzionale
   if ((arg = arg->rbnext) && arg->restype == RTNIL)
      AllCharDataType = false;

   if (gsc_setGoogleEarthODRecord(ss, AllCharDataType) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER GOOGLE-EARTH
// INIZIO FUNZIONI PER QUANTUM GIS 1.7.4
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_getRGBColor                                        <internal> */
/*+
  Questa funzione restituisce i codici RGB di un colore. Se questo è da blocco o
  da layer viene letta la definizione del modello di layer. Se questa non esiste
  è nero (R=0,G=0,B=0)
  Parametri:
  C_COLOR     &Color;      Colore (input)
  int         Prj;         Codice progetto
  const TCHAR *LayerName;  Nome del layer (input)
  BYTE         *Red;       output
  BYTE         *Green;     output
  BYTE         *Blue;      output

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getRGBColor(C_COLOR &Color, int Prj, const TCHAR *LayerName, BYTE *Red, BYTE *Green, BYTE *Blue)
{
   if (Color.getColorMethod() == C_COLOR::ByLayer || Color.getColorMethod() == C_COLOR::ByBlock)
      if (gsc_strlen(LayerName) > 0)
      {
         C_LAYER_MODEL LayerModel;
         LayerModel.set_name(LayerName);
         if (gsc_getLayerModel(Prj, LayerModel) != GS_GOOD)
            { *Red = *Green = *Blue = 0; return GS_GOOD; }
         LayerModel.get_color(Color);
         if (Color.getColorMethod() == C_COLOR::ByLayer || Color.getColorMethod() == C_COLOR::ByBlock)
            { *Red = *Green = *Blue = 0; return GS_GOOD; }
      }
      else
         { *Red = *Green = *Blue = 0; return GS_GOOD; }

   Color.getRed(Red);
   Color.getGreen(Green);
   Color.getBlue(Blue);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_correctRGBColor                                    <internal> */
/*+
  Questa funzione corregge i codici RGB di un colore in base al colore dello
  sfondo di autocad. Se questo è nero (R=0,G=0,B=0) il colore diventa bianco.
  Parametri:
  BYTE         *Red;        output
  BYTE         *Green;      output
  BYTE         *Blue;       output

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
void gsc_correctRGBColor(BYTE *Red, BYTE *Green, BYTE *Blue)
{
   BYTE            Background_Red, Background_Green, Background_Blue;   
   AcColorSettings ColorSettings;

   // Colore sfondo
   if (!acedGetCurrentColors(&ColorSettings)) return;

   Background_Red   = GetRValue(ColorSettings.dwGfxModelBkColor);
   Background_Green = GetGValue(ColorSettings.dwGfxModelBkColor & 0xffff);
   Background_Blue  = GetBValue(ColorSettings.dwGfxModelBkColor);

   if (Background_Red == 0 && Background_Green == 0 && Background_Blue == 0) // sfondo nero
      if (*Red == 0 && *Green == 0 && *Blue == 0) // nero
      {  // imposto il bianco
         *Red = 255; *Green = 255; *Blue = 255;
      }
}

/*************************************************************************/
/*.doc gsc_WriteQGISPostgreSQLGConnectionRegFile              <external> */
/*+
  Questa funzione scrive un file di configurazione dei registri per la connessione 
  a database PostgreSQL da QGIS.
  Parametri:
  C_STRING &QGISPostgreSQLGConnectionRegFile;   File di QGIS
  C_STRING &ConnName;
  C_STRING &Host;
  C_STRING &Database;
  C_STRING &Port;
  C_STRING &User;
  C_STRING &Password;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_WriteQGISPostgreSQLGConnectionRegFile(C_STRING &QGISPostgreSQLGConnectionRegFile, 
                                              C_STRING &ConnName, C_STRING &Host, C_STRING &Database,
                                              C_STRING &Port, C_STRING &User, C_STRING &Password)
{
   FILE     *f;
   C_STRING Buffer;

   if (gsc_path_exist(QGISPostgreSQLGConnectionRegFile) == GS_GOOD)
      if (gsc_delfile(QGISPostgreSQLGConnectionRegFile) == GS_BAD) return GS_BAD;

   Buffer = _T("Windows Registry Editor Version 5.00");
   Buffer += GS_LFSTR;
   Buffer += _T("[HKEY_CURRENT_USER\\Software\\QuantumGIS\\QGIS\\PostgreSQL\\connections\\");
   Buffer += ConnName;
   Buffer += _T("]");
   Buffer += GS_LFSTR;
   Buffer += _T("\"host\"=\"");
   Buffer += Host;
   Buffer += _T("\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"database\"=\"");
   Buffer += Database;
   Buffer += _T("\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"port\"=\"");
   Buffer += Port;
   Buffer += _T("\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"username\"=\"");
   Buffer += User;
   Buffer += _T("\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"password\"=\"");
   Buffer += Password;
   Buffer += _T("\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"publicOnly\"=\"false\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"geometryColumnsOnly\"=\"false\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"allowGeometrylessTables\"=\"true\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"sslmode\"=dword:00000001");
   Buffer += GS_LFSTR;
   Buffer += _T("\"saveUsername\"=\"true\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"savePassword\"=\"true\"");
   Buffer += GS_LFSTR;
   Buffer += _T("\"estimatedMetadata\"=\"false\"");
   Buffer += GS_LFSTR;

   if ((f = gsc_fopen(QGISPostgreSQLGConnectionRegFile, _T("w"))) == NULL) return GS_BAD;
   if (fwprintf(f, Buffer.get_name()) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   gsc_fclose(f);

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISLayerInfo                                   <internal> */
/*+
  Questa funzione restituisce informazioni sul layer di QGIS 
  (Layer ID, Layer name).
  Parametri:
  const TCHAR *Prefix;  Prefisso (nome della classe)
  int Type;             Tipo di dato GEOsim
  C_STRING &LayerID;
  C_STRING &LayerName;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
void gsc_getQGISLayerInfo(const TCHAR *Prefix, int Type, C_STRING &LayerID, C_STRING &LayerName)
{
   static C_2STR_LIST LayerIDList;

   LayerID = Prefix;
   switch (Type)
   {
      case TYPE_POLYLINE:
         LayerID += _T("_geom_line");
         break;
      case TYPE_NODE:
         LayerID += _T("_geom_point");
         break;
      case TYPE_TEXT:
         LayerID += _T("_lbl");
         break;
      case TYPE_SURFACE:
         LayerID += _T("_geom_polygon");
         break;
   }

   LayerName = LayerID;

   if (LayerIDList.search_name(LayerName.get_name()))
      LayerID = ((C_2STR *) LayerIDList.get_cursor())->get_name2();
   else
   {
      COleDateTime dummyOleDateTime = COleDateTime::GetCurrentTime();
      LayerID += dummyOleDateTime.GetYear();
      LayerID += dummyOleDateTime.GetMonth();
      LayerID += dummyOleDateTime.GetDay();
      LayerID += dummyOleDateTime.GetHour();
      LayerID += dummyOleDateTime.GetMinute();
      LayerID += dummyOleDateTime.GetSecond();
   }

   if (!LayerIDList.search_name(LayerName.get_name()))
      LayerIDList.add_tail(new C_2STR(LayerName.get_name(), LayerID.get_name()));
}
void gsc_getQGISLayerInfo(C_SPAGHETTI *pCls, int Type, C_STRING &LayerID, C_STRING &LayerName)
{
   gsc_getQGISLayerInfo(pCls->get_name(), Type, LayerID, LayerName);
   return;
}
void gsc_getQGISLayerInfo(C_SIMPLEX *pCls, bool Label, C_STRING &LayerID, C_STRING &LayerName)
{
   if (Label)
      gsc_getQGISLayerInfo(pCls->get_name(), TYPE_TEXT, LayerID, LayerName);
   else
      gsc_getQGISLayerInfo(pCls->get_name(), pCls->get_type(), LayerID, LayerName);

   return;
}


/*************************************************************************/
/*.doc gsc_getQGISGeomType                                    <internal> */
/*+
  Questa funzione restituisce il tipo geometrico di QGIS
  Parametri:
  int Type;             Tipo di dato GEOsim
  C_STRING &GeomType;   Tipo di dato QGIS

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
void gsc_getQGISGeomType(int Type, C_STRING &GeomType)
{
   switch (Type)
   {
      case TYPE_POLYLINE:
         GeomType = _T("Line");
         break;
      case TYPE_NODE:
         GeomType = _T("Point");
         break;
      case TYPE_TEXT:
         GeomType = _T("Point");
         break;
      case TYPE_SURFACE:
         GeomType = _T("Polygon");
         break;
   }
}

/*************************************************************************/
/*.doc gsc_getQGISXMLSpatialRefSys                            <internal> */
/*+
  Questa funzione restituisce la descrizione XML del sistema di coordinate.
  Parametri:
  C_STRING &Buffer;  Buffer a cui viene aggiunto in coda la descrizione XML
  int SRID;          Codice sistema di coordinate (default = 3003)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLSpatialRefSys(C_STRING &Buffer, int SRID, const TCHAR *CurrentIndent = NULL)
{
   C_STRING Indent1 = CurrentIndent, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;

   switch (SRID)
   {
      case 4326: // longlat
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs +towgs84=0,0,0</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>3452</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>4326</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:4326</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>WGS 84</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>longlat</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>WGS84</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>true</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      case 3003: // Monte Mario / Italy zone 1
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=tmerc +lat_0=0 +lon_0=9 +k=0.9996 +x_0=1500000 +y_0=0 +ellps=intl +towgs84=-104.1,-49.1,-9.9,0.971,-2.917,0.714,-11.68 +units=m +no_defs</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>968</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>3003</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:3003</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>Monte Mario / Italy zone 1</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>tmerc</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>intl</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>false</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      case 3004: // Monte Mario / Italy zone 2
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=tmerc +lat_0=0 +lon_0=15 +k=0.9996 +x_0=2520000 +y_0=0 +ellps=intl +towgs84=-104.1,-49.1,-9.9,0.971,-2.917,0.714,-11.68 +units=m +no_defs</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>969</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>3004</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:3004</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>Monte Mario / Italy zone 2</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>tmerc</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>intl</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>false</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      case 900913: // Google Mercator
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +over +no_defs</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>3644</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>900913</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:900913</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>Google Mercator</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>merc</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>WGS84</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>false</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      case 32632: // WGS 84 / UTM zone 32N 
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=utm +zone=32 +ellps=WGS84 +datum=WGS84 +units=m +no_defs +towgs84=0,0,0</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>3116</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>32632</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:32632</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>WGS 84 / UTM zone 32N</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>utm</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>WGS84</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>false</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      case 23032: // ED50 / UTM zone 32N
         Buffer += Indent1;
         Buffer += _T("<spatialrefsys>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<proj4>+proj=utm +zone=32 +ellps=intl +towgs84=-87,-98,-121,0,0,0,0 +units=m +no_defs</proj4>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srsid>1978</srsid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<srid>23032</srid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<authid>EPSG:23032</authid>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<description>ED50 / UTM zone 32N</description>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<projectionacronym>utm</projectionacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<ellipsoidacronym>intl</ellipsoidacronym>");
         Buffer += GS_LFSTR;
         Buffer += Indent2;
         Buffer += _T("<geographicflag>false</geographicflag>");
         Buffer += GS_LFSTR;
         break;

      default:
         GS_ERR_COD = eGSInvalidCoord;         
         return GS_BAD;
   }

   Buffer += Indent1;
   Buffer += _T("</spatialrefsys>");
   Buffer += GS_LFSTR;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLMapCanvas                                <internal> */
/*+
  Questa funzione restituisce la descrizione XML della finestra grafica.
  Parametri:
  FILE *f;     Puntatore a file a cui viene aggiunto in coda la descrizione XML
  int SRID;    Codice sistema di coordinate (default = 3003)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLMapCanvas(FILE *f, int SRID, const TCHAR *CurrentIndent = NULL)
{
   C_STRING Buffer;
   C_STRING Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;

   Buffer = Indent1;
   Buffer += _T("<mapcanvas>");
   Buffer += GS_LFSTR;
   // unità di misura in metri
   Buffer += Indent2;
   Buffer += _T("<units>meters</units>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<extent>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<xmin>0.0</xmin>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<ymin>0.0</ymin>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<xmax>0.0</xmax>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<ymax>0.0</ymax>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("</extent>");
   Buffer += GS_LFSTR;
   // se = 0 ogni layer è disegnato usando le coordinate lette dal dato sorgente
   // se = 1 le coordinate di ogni layer sono riproiettate verso il sistema
   //        di coordinate definito x la mappa
   Buffer += Indent2;
   Buffer += _T("<projections>1</projections>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<destinationsrs>");
   Buffer += GS_LFSTR;
   if (gsc_getQGISXMLSpatialRefSys(Buffer, SRID, Indent3.get_name()) == GS_BAD) return GS_BAD;
   Buffer += Indent2;
   Buffer += _T("</destinationsrs>");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("</mapcanvas>");
   Buffer += GS_LFSTR;

   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLLegendLayer                              <internal> */
/*+
  Questa funzione restituisce la descrizione XML di un layer nella legenda.
  Parametri:
  FILE *f;              Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_STRING &LayerName;
  C_STRING &LayerID;
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLLegendLayer(FILE *f, C_STRING &LayerName, C_STRING &LayerID, const TCHAR *CurrentIndent = NULL)
{
   C_STRING Buffer, XMLStr;
   C_STRING Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;

   Buffer = Indent1;
   Buffer += _T("<legendlayer open=\"false\" checked=\"Qt::Checked\" name=\"");
   XMLStr = LayerName; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("\" showFeatureCount=\"0\">");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<filegroup open=\"true\" hidden=\"false\">");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<legendlayerfile isInOverview=\"0\" layerid=\"");
   Buffer += LayerID;
   Buffer += _T("\" visible=\"1\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("</filegroup>");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("</legendlayer>");
   Buffer += GS_LFSTR;

   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLLegendLayer                              <internal> */
/*+
  Questa funzione restituisce la descrizione XML di una classe GEOsim nella legenda
  creando opportuni raggruppamenti.
  Parametri:
  FILE *f;        Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;  Puntatore a classe GEOsim
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLLegendLayer(FILE *f, C_CLASS *pCls, const TCHAR *CurrentIndent = NULL)
{
   C_STRING Buffer, XMLStr;
   C_STRING LayerName, LayerID;
   C_STRING Indent1 = CurrentIndent, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;

   switch (pCls->get_category())
   {
      case CAT_SIMPLEX:
         // Se ha etichette e non si tratta di classe testo
         if (pCls->ptr_attrib_list()->is_visible() == GS_GOOD && pCls->get_type() != TYPE_TEXT)
         {
            Buffer = Indent1;
            // Creo un gruppo di layer senza aprire il suo sottoalbero
            Buffer += _T("<legendgroup open=\"false\" checked=\"Qt::Checked\" name=\"");
            XMLStr = pCls->get_name(); XMLStr.toAXML();
            Buffer += XMLStr;
            Buffer += _T("\">");
            Buffer += GS_LFSTR;
            if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }

         // Creo il layer della geometria e dei testi
         if (pCls->get_type() == TYPE_TEXT)
            gsc_getQGISLayerInfo((C_SIMPLEX *) pCls, true, LayerID, LayerName); // LayerID delle etichette
         else
            gsc_getQGISLayerInfo((C_SIMPLEX *) pCls, false, LayerID, LayerName); // LayerID delle geometrie

         if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;

         // Se ha etichette e non si tratta di classe testo
         if (pCls->ptr_attrib_list()->is_visible() == GS_GOOD && pCls->get_type() != TYPE_TEXT)
         {
            // Creo il layer delle etichette
            gsc_getQGISLayerInfo((C_SIMPLEX *) pCls, true, LayerID, LayerName); // LayerID delle etichette
            if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;
            Buffer = Indent1;
            Buffer += _T("</legendgroup>");
            Buffer += GS_LFSTR;
            if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }
         break;

      case CAT_SPAGHETTI:
         // Creo un gruppo di layer senza aprire il suo sottoalbero
         Buffer = Indent1;
         Buffer += _T("<legendgroup open=\"false\" checked=\"Qt::Checked\" name=\"");
         XMLStr = pCls->get_name(); XMLStr.toAXML();
         Buffer += XMLStr;
         Buffer += _T("\">");
         Buffer += GS_LFSTR;
         if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

         LayerName = pCls->get_name();

         // Creo il layer della geometria punti
         gsc_getQGISLayerInfo((C_SPAGHETTI *) pCls, TYPE_NODE, LayerID, LayerName); // LayerID delle geometrie punti
         if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;
         // Creo il layer della geometria linea
         gsc_getQGISLayerInfo((C_SPAGHETTI *) pCls, TYPE_POLYLINE, LayerID, LayerName); // LayerID delle geometrie linee
         if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;
         // Creo il layer della geometria poligono
         gsc_getQGISLayerInfo((C_SPAGHETTI *) pCls, TYPE_SURFACE, LayerID, LayerName); // LayerID delle geometrie poligoni
         if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;
         // Creo il layer dei testi
         gsc_getQGISLayerInfo((C_SPAGHETTI *) pCls, TYPE_TEXT, LayerID, LayerName); // LayerID delle geometrie punti-etichette
         if (gsc_getQGISXMLLegendLayer(f, LayerName, LayerID, Indent2.get_name()) == GS_BAD) return GS_BAD;
         Buffer = Indent1;
         Buffer += _T("</legendgroup>");
         Buffer += GS_LFSTR;
         if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         break;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLLegend                                   <internal> */
/*+
  Questa funzione restituisce la descrizione XML della legenda 
  creando opportuni raggruppamenti.
  Parametri:
  FILE *f;                 Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS_LIST &ClassList; Lista delle classi
  C_2STR_LIST *pGroupMaskList; opzionale, puntatore alla lista dei raggruppamenti di classe;
                               ogni elemento contiene (<nome gruppo> < maschera di raggruppamento)
                               (default = NULL)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLLegend(FILE *f, C_CLASS_LIST &ClassList, C_2STR_LIST *pGroupMaskList,
                         const TCHAR *CurrentIndent = NULL)
{
   C_STRING     Buffer, XMLStr;
   C_CLASS      *pCls, *new_class;
   C_STRING     LayerName, LayerID, ClassName;
   C_CLASS_LIST mClassList, ClassGroup;
   C_2STR       *pGroupMask = NULL;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(115)); // "Creazione legenda QGIS"
   long         i;
   C_STRING     Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;

   Buffer = Indent1;
   Buffer += _T("<legend>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   StatusBarProgressMeter.Init(2); // 2 fasi
   StatusBarProgressMeter.Set(1); // prima fase

   ClassList.copy(mClassList);
   if (pGroupMaskList)
   {
      C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter2(gsc_msg(1067)); // "Raggruppamento classi in legenda QGIS"

      StatusBarProgressMeter2.Init(pGroupMaskList->get_count());
      i = 1;

      pGroupMask = (C_2STR *) pGroupMaskList->get_head();
      while (pGroupMask)
      {
         StatusBarProgressMeter2.Set(i++);

         ClassGroup.remove_all();
         pCls = (C_CLASS *) mClassList.get_head();
         while (pCls)
         {
            ClassName = pCls->get_name();
            if (ClassName.wildcomp(pGroupMask->get_name2()) != 0)
            {
		         if ((new_class = gsc_alloc_new_class(pCls->get_category())) == NULL)
                  return GS_BAD;
               pCls->copy(new_class);
               ClassGroup.add_tail(new_class);
               mClassList.remove_at(); // la rimuovo dalla lista delle classi
               pCls = (C_CLASS *) mClassList.get_cursor();
            }
            else
               pCls = (C_CLASS *) mClassList.get_next();
         }

         if (ClassGroup.get_count() > 0)
         {
            // Creo un gruppo di layer senza aprire il suo sottoalbero
            Buffer = Indent2;
            Buffer += _T("<legendgroup open=\"false\" checked=\"Qt::Checked\" name=\"");
            XMLStr = pGroupMask->get_name(); XMLStr.toAXML();
            Buffer += XMLStr;
            Buffer += _T("\">");
            Buffer += GS_LFSTR;
            if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

            pCls = (C_CLASS *) ClassGroup.get_head();
            while (pCls)
            {
               if (gsc_getQGISXMLLegendLayer(f, pCls, Indent3.get_name()) == GS_BAD) return GS_BAD;
               pCls = (C_CLASS *) ClassGroup.get_next();
            }
            Buffer = Indent2;
            Buffer += _T("</legendgroup>");
            Buffer += GS_LFSTR;
            if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }

         pGroupMask = (C_2STR *) pGroupMaskList->get_next();
      }

      StatusBarProgressMeter2.End(gsc_msg(1090)); // "Terminato."
   }

   StatusBarProgressMeter.Set(2); // seconda fase

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter2(gsc_msg(1068)); // "Inserimento classi in legenda QGIS"
   StatusBarProgressMeter2.Init(mClassList.get_count());

   i = 1;

   // inserisco in legenda tutte le classi rimaste
   pCls = (C_CLASS *) mClassList.get_head();
   while (pCls)
   {
      StatusBarProgressMeter2.Set(i++);

      if (gsc_getQGISXMLLegendLayer(f, pCls, Indent2.get_name()) == GS_BAD) return GS_BAD;

      pCls = (C_CLASS *) mClassList.get_next();
   }
   StatusBarProgressMeter2.End(gsc_msg(1090)); // "Terminato."

   Buffer = Indent1;
   Buffer += _T("</legend>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}

/*************************************************************************/
/*.doc gsc_getQGISXMLEditTypes                                <internal> */
/*+
  Questa funzione restituisce la descrizione XML della configurazione dei tipi di editing
  che sono permessi sui campi della tabella del layer.
  Parametri:
  C_STRING &Buffer;  Buffer a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;     Puntatore alla classe
  TCHAR *ViewType;   Tipo di vista (input)
                     Se = "geom" -> vista "tabella geometrica/alfanumerica"
                     Se = "lbl" -> vista "tabella etichette/alfanumerica"
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLEditTypes(FILE *f, C_CLASS *pCls, TCHAR *ViewType, const TCHAR *CurrentIndent = NULL)
{
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   C_ATTRIB_LIST  *pAttribList = pCls->ptr_attrib_list();
   C_ATTRIB       *pAttrib;
   C_STRING       FullRefView, AliasName, SupportFile;
   C_2STR_LIST    Name_Alias_list;
   C_2STR         *p_Name_Alias;
   C_RB_LIST      ViewStru;
   presbuf        pRb;
   int            i = 0, prj, cls, sub;
   ValuesListTypeEnum FileType;
   C_2STR_LIST    ValuesList;
   C_STRING       Buffer, XMLStr;
   C_STRING       Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;

   enum EditTypeEnum
   {
      EDITTYPE_EDITABLE         = 0,   // Modifica valore
      EDITTYPE_VALUES_MAP       = 3,   // Mappa valori
      EDITTYPE_NUMERIC_INTERVAL = 5,   // Modifica intervallo
      EDITTYPE_CHECKBOX         = 7,   // Campo checkbox
      EDITTYPE_READONLY         = 10,  // Campo immutabile
      EDITTYPE_HIDDEN           = 11,  // Campo nascosto
      EDITTYPE_CALENDAR         = 13   // Campo calendario
   } EditType;

   prj = pCls->get_PrjId();
   cls = pCls->ptr_id()->code;
   sub = pCls->ptr_id()->sub_code;

   // Inizializzo i tipi ADO per ogni attributo
   if (pCls->ptr_attrib_list())
      if (pCls->ptr_attrib_list()->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == GS_BAD)
         return GS_BAD;

   // ricavo il nome della vista
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, ViewType, &Name_Alias_list, NULL,
                                        &FullRefView) == GS_BAD) return GS_BAD;

   // Leggo la struttura della vista per la geometria
   if ((ViewStru << pGphConn->ReadStruct(FullRefView.get_name())) == NULL)
      return GS_BAD;

   Buffer = Indent1;
   Buffer += _T("<edittypes>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   while ((pRb = ViewStru.nth(i++)) != NULL)
   {
      pRb = gsc_nth(0, pRb); // Nome del campo

      AliasName = pRb->resval.rstring;
      if (gsc_AdjSyntax(AliasName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                        pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
         return GS_BAD;

      Buffer = Indent2;
      Buffer += _T("<edittype type=\"");

      EditType = EDITTYPE_EDITABLE; // Modifica valore
      // se il campo non appartiene alla tabella alfanumerica
      if ((p_Name_Alias = (C_2STR *) Name_Alias_list.search_name2(AliasName.get_name(), FALSE)) == NULL)
      {
         if (gsc_strcmp(ViewType, _T("lbl")) == 0) // se si tratta di label
         {
            // Se si tratta del campo a cui si riferisce l'etichetta
            if (pGphInfo->attrib_name_attrib.comp(pRb->resval.rstring) == 0)
            {
               EditType = EDITTYPE_VALUES_MAP; // Mappa valori

               // Carico la lista degli attributi visibili
               ValuesList.remove_all();
               pAttrib = (C_ATTRIB *) pAttribList->get_head();
               while (pAttrib)
               {
                  if (pAttrib->is_visible() == GS_GOOD)
                     ValuesList.add_tail_2str(pAttrib->get_name(), pAttrib->get_name());
                  pAttrib = (C_ATTRIB *) pAttribList->get_next();
               }
            }
            else
               EditType = EDITTYPE_HIDDEN; // Campo nascosto
         }
         else
            EditType = EDITTYPE_HIDDEN; // Campo nascosto
      }
      else // se il campo appartiene alla tabella alfanumerica
         if (pAttribList)
            if ((pAttrib = (C_ATTRIB *) pAttribList->search_name(p_Name_Alias->get_name(), FALSE)))
            {  // Se calcolato
               if (pAttrib->is_calculated())
                  EditType = EDITTYPE_READONLY; // Campo immutabile
               // Se ha una lista valori associata
               else if (gsc_FindSupportFiles(pAttrib->get_name(), prj, cls, sub, 0,
                                             SupportFile, &FileType) == GS_GOOD &&
                        gsc_getQGIS_isValidValuesList(SupportFile, pAttrib, pAttribList, ValuesList) == true)
                  EditType = EDITTYPE_VALUES_MAP; // Mappa valori
               // se attributo di tipo date
               else if (gsc_DBIsDate(pAttrib->ADOType) == GS_GOOD || gsc_DBIsTimestamp(pAttrib->ADOType) == GS_GOOD)
                  EditType = EDITTYPE_CALENDAR; // Campo calendario
               // se attributo di tipo booleano
               else if (gsc_DBIsBoolean(pAttrib->ADOType) == GS_GOOD)
                  EditType = EDITTYPE_CHECKBOX; // Campo checkbox
            }

      Buffer += EditType;
      Buffer += _T("\" name=\"");
      XMLStr = pRb->resval.rstring; XMLStr.toAXML();
      Buffer += XMLStr;
      Buffer += _T("\"");

      switch (EditType)
      {
         case EDITTYPE_VALUES_MAP: // Mappa valori
         {
            C_2STR   *pValue = (C_2STR *) ValuesList.get_head();

            Buffer += _T(">");
            Buffer += GS_LFSTR;

            while (pValue)
            {
               Buffer += Indent3;
               Buffer += _T("<valuepair key=\"");               
               XMLStr = pValue->get_name2(); XMLStr.toAXML(); // descrizione
               Buffer += XMLStr;
               Buffer += _T("\" value=\"");
               XMLStr = pValue->get_name(); XMLStr.toAXML(); // valore
               Buffer += XMLStr;
               Buffer += _T("\"/>");
               Buffer += GS_LFSTR;

               pValue = (C_2STR *) ValuesList.get_next();
            }
            Buffer += Indent2;
            Buffer += _T("</edittype>");
            Buffer += GS_LFSTR;
            break;
         }
      
         case EDITTYPE_NUMERIC_INTERVAL: // Modifica intervallo
            Buffer += _T(" min=\"-999999999\" max=\"999999999\" step=\"1\"/>");
            Buffer += GS_LFSTR;
            break;

         case EDITTYPE_CHECKBOX: // checkbox
            Buffer += _T(" unchecked=\"0\" checked=\"1\"/>");
            Buffer += GS_LFSTR;
            break;
      
         default:
            Buffer += _T("/>");
            Buffer += GS_LFSTR;
      }

      if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   Buffer = Indent1;
   Buffer += _T("</edittypes>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGIS_isValidValuesList                          <internal> */
/*+
  Questa funzione valuta se la lista di scelta di un attributo può essere adatta a QGIS.
  La lista di scelta non deve essere condizionata e non deve essere parametrica.
  Parametri:
  C_STRING &SupportFile;         File di supporto per la lista di scelta
  C_ATTRIB *pAttrib;             Attributo da valutare
  C_ATTRIB_LIST *pAttribList;    Lista degli attributi
  C_2STR_LIST &ValuesList;       Lista dei valori caricata (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
bool gsc_getQGIS_isValidValuesList(C_STRING &SupportFile, C_ATTRIB *pAttrib, C_ATTRIB_LIST *pAttribList,
                                   C_2STR_LIST &ValuesList)
{
   FILE       *f;
   bool       Unicode;
   C_STR_LIST LispCondList;
   C_STRING   ConnStrUDLFile, UdlProperties, SelectStm;

   if ((f = gsc_open_profile(SupportFile, READONLY, MORETESTS, &Unicode)) == NULL)
      return NULL;

   // Carico la lista di eventuali condizioni GEOlisp
   if (gsc_get_profile(f, LispCondList, Unicode) == GS_BAD) 
      { gsc_close_profile(f); return false; }
   // Se ci sono condizioni non è adatta a QGIS
   if (LispCondList.get_count() > 0)
      { gsc_close_profile(f); return false; }

   C_CACHE_ATTRIB_VALUES AttribValues(pAttrib);
   if (AttribValues.init_AttribListDependencies(f, Unicode, pAttribList, LispCondList) == GS_BAD)
      { gsc_close_profile(f); return false; }

   // Se ci sono parametri non è adatta a QGIS
   if (AttribValues.ptr_AttribListDependencies()->get_count() > 0)
      { gsc_close_profile(f); return false; }

   gsc_close_profile(f);

   // Carico la lista di valori
   if (ValuesList.load(SupportFile.get_name(), _T(';')) != GS_GOOD) return false;
   if (gsc_is_C_2STR_LIST_referred_to_DB(ValuesList, ConnStrUDLFile, 
                                         UdlProperties, SelectStm))
      if (gsc_C_2STR_LIST_load(ValuesList, ConnStrUDLFile, UdlProperties,
                               SelectStm, (presbuf) NULL) != GS_GOOD)
         return false;

   // Se è il caso, correggo le dimensioni dei valori
   C_2STR   *pValue = (C_2STR *) ValuesList.get_head();
   C_STRING CorrectedStr;
   while (pValue)
   {
      // Valore
      CorrectedStr = pValue->get_name();
      if (CorrectedStr.len() > (size_t) pAttrib->len)
         CorrectedStr = CorrectedStr.left(pAttrib->len);
      CorrectedStr.toAXML();
      pValue->set_name(CorrectedStr.get_name());

      // Descrizione
      if (pValue->get_name2())
      {
         CorrectedStr = pValue->get_name2();
         CorrectedStr.toAXML();
      }
      else
         CorrectedStr.clear();

      // commentato perchè se QGIS usa il controllo "mappa valori" necessita di avere la corrispondenza 
      // con il valore nel formato "%%C" e non nel valore trasformato in "Ø"

      // converto il %%C in Ø (il % era stato raddoppiato)
      //CorrectedStr.strtran(_T("%%%%C"), _T("&#934;"), FALSE);
      //CorrectedStr.strtran(_T("%%%%C"), _T("Ã˜"), FALSE); // Ø (alt 0216), ø (alt 155 o alt 0248)
      pValue->set_name2(CorrectedStr.get_name());

      pValue = (C_2STR *) ValuesList.get_next();
   }

   // Se il valore dell'attributo non è obbligatorio
   if (pAttrib->is_mandatory() == GS_BAD)
      // Aggiungo una riga vuota per i valori nulli
	   if ((pValue = new C_2STR))
         ValuesList.add_head(pValue);

   return true;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLAliases                                  <internal> */
/*+
  Questa funzione restituisce la descrizione XML degli alias dei campi della tabella del layer.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;     Puntatore alla classe
  TCHAR *ViewType;   Tipo di vista (input)
                     Se = "geom" -> vista "tabella geometrica/alfanumerica"
                     Se = "lbl" -> vista "tabella etichette/alfanumerica"
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Da finire
-*/  
/*************************************************************************/
int gsc_getQGISXMLAliases(FILE *f, C_CLASS *pCls, TCHAR *ViewType, const TCHAR *CurrentIndent = NULL)
{
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   C_ATTRIB_LIST  *pAttribList = pCls->ptr_attrib_list();
   C_ATTRIB       *pAttrib;
   C_STRING       FullRefView;
   C_2STR_LIST    Name_Alias_list;
   C_2STR         *p_Name_Alias;
   C_RB_LIST      ViewStru;
   presbuf        pRb;
   int            i = 0;
   C_STRING       Buffer;
   C_STRING       Indent1 = CurrentIndent, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;

   // ricavo il nome della vista
   if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, ViewType, &Name_Alias_list, NULL,
                                        &FullRefView) == GS_BAD) return GS_BAD;

   // Leggo la struttura della vista per la geometria
   if ((ViewStru << pGphConn->ReadStruct(FullRefView.get_name())) == NULL)
      return GS_BAD;

   Buffer = Indent1;
   Buffer += _T("<aliases>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   while ((pRb = ViewStru.nth(i++)) != NULL)
   {
      pRb = gsc_nth(0, pRb); // Nome del campo

      // se il campo appartiene alla tabella alfanumerica
      if ((p_Name_Alias = (C_2STR *) Name_Alias_list.search_name2(pRb->resval.rstring, FALSE)))
         if ((pAttrib = (C_ATTRIB *) pAttribList->search_name(p_Name_Alias->get_name(), FALSE)))
         {
            Buffer = Indent2;
            Buffer += _T("<edittype type=\"");
            Buffer += _T("<alias field=\"");
            Buffer += pRb->resval.rstring;
            Buffer += _T("\" index=\"8\" name=\"");
            Buffer += i - 1; // 0-indexed
            Buffer += _T("\" name=\"");
            Buffer += pAttrib->Caption;
            Buffer += _T("\"/>");
            Buffer += GS_LFSTR;
         }

      if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   Buffer = Indent1;
   Buffer = _T("</aliases>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLRenderer                                 <internal> */
/*+
  Questa funzione restituisce la descrizione XML del render del layer.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;     Puntatore alla classe
  int Type;          Tipo di oggetti (TYPE_POLYLINE per oggetti lineari, 
                     TYPE_TEXT per etichette, TYPE_NODE per elementi puntuali,
                     TYPE_SURFACE per poligoni)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLRenderer(FILE *f, C_CLASS *pCls, int Type, const TCHAR *CurrentIndent = NULL)
{
   BYTE         Red, Green, Blue;
   C_STRING     rotation_field, sizescale_field;
   C_FAS        *pFAS = pCls->ptr_fas();
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_STRING     Buffer;
   C_STRING     Indent1 = CurrentIndent, Indent2, Indent3, Indent4, Indent5;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;
   Indent4 = Indent3; Indent4 += QGIS_XML_INDENT;
   Indent5 = Indent4; Indent5 += QGIS_XML_INDENT;

   if (!pFAS) return GS_GOOD;

   Buffer = Indent1;
   Buffer += _T("<renderer-v2 symbollevels=\"0\" type=\"singleSymbol\">");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<symbols>");
   Buffer += GS_LFSTR;

   switch (Type)
   {
      case TYPE_POLYLINE:
         if (gsc_getRGBColor(pFAS->color, pCls->get_PrjId(), pFAS->layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
         gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

         Buffer += Indent3;
         Buffer += _T("<symbol alpha=\"1\" type=\"line\" name=\"0\">");
         Buffer += GS_LFSTR;
         Buffer += Indent4;
         Buffer += _T("<layer pass=\"0\" class=\"SimpleLine\" locked=\"0\">");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"capstyle\" v=\"square\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"color\" v=\"");
         Buffer += (int) Red;
         Buffer += _T(",");
         Buffer += (int) Green;
         Buffer += _T(",");
         Buffer += (int) Blue;
         Buffer += _T(",255\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"customdash\" v=\"5;2\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"customdash_unit\" v=\"MM\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"joinstyle\" v=\"bevel\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"offset\" v=\"0\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"offset_unit\" v=\"MM\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"penstyle\" v=\"solid\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"use_custom_dash\" v=\"0\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"width\" v=\"");
         Buffer += pFAS->width;
         Buffer += _T("\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"width_unit\" v=\"MM\"/>");
         Buffer += GS_LFSTR;
         break;

      case TYPE_NODE:
      case TYPE_TEXT:
         if (gsc_getRGBColor(pFAS->color, pCls->get_PrjId(), pFAS->layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
         gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

         Buffer += Indent3;
         if (Type == TYPE_NODE)
         {  // Trasparenza al 0%
            Buffer += _T("<symbol outputUnit=\"MM\" alpha=\"1\" type=\"marker\" name=\"0\">");
            rotation_field = pGphInfo->rotation_attrib;
            rotation_field = pGphInfo->block_scale_attrib;
         }
         else
         {  // Trasparenza al 100%
            Buffer += _T("<symbol outputUnit=\"MM\" alpha=\"0\" type=\"marker\" name=\"0\">");
         }

         Buffer += GS_LFSTR;
         Buffer += Indent4;
         Buffer += _T("<layer pass=\"0\" class=\"SimpleMarker\" locked=\"0\">");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"angle\" v=\"0\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"color\" v=\"");
         Buffer += (int) Red;
         Buffer += _T(",");
         Buffer += (int) Green;
         Buffer += _T(",");
         Buffer += (int) Blue;
         Buffer += _T(",255\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"color_border\" v=\"0,0,0,255\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"name\" v=\"circle\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"offset\" v=\"0,0\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"size\" v=\"2\"/>");
         Buffer += GS_LFSTR;
         break;

      case TYPE_SURFACE:
         // colore riempimento
         if (gsc_getRGBColor(pFAS->hatch_color, pCls->get_PrjId(), pFAS->hatch_layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
         gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

         Buffer += Indent3;
         Buffer += _T("<symbol outputUnit=\"MM\" alpha=\"1\" type=\"fill\" name=\"0\">");
         Buffer += GS_LFSTR;
         Buffer += Indent4;
         Buffer += _T("<layer pass=\"0\" class=\"SimpleFill\" locked=\"0\">");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"color\" v=\"");
         Buffer += (int) Red;
         Buffer += _T(",");
         Buffer += (int) Green;
         Buffer += _T(",");
         Buffer += (int) Blue;
         Buffer += _T(",255\"/>");
         Buffer += GS_LFSTR;

         // colore bordo
         if (gsc_getRGBColor(pFAS->color, pCls->get_PrjId(), pFAS->layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
         gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

         Buffer += Indent5;
         Buffer += _T("<prop k=\"color_border\" v=\"");
         Buffer += (int) Red;
         Buffer += _T(",");
         Buffer += (int) Green;
         Buffer += _T(",");
         Buffer += (int) Blue;
         Buffer += _T(",255\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"offset\" v=\"0,0\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         if (gsc_strlen(pFAS->hatch) == 0) // nessun riempimento
            Buffer += _T("<prop k=\"style\" v=\"no\"/>");
         else
            Buffer += _T("<prop k=\"style\" v=\"solid\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"style_border\" v=\"solid\"/>");
         Buffer += GS_LFSTR;
         Buffer += Indent5;
         Buffer += _T("<prop k=\"width_border\" v=\"");
         Buffer += pFAS->width;
         Buffer += _T("\"/>");
         Buffer += GS_LFSTR;
         break;

      default:
         GS_ERR_COD = eGSInvClassType;
         return GS_BAD;
   }
   
   Buffer += Indent4;
   Buffer += _T("</layer>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("</symbol>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("</symbols>");
   Buffer += GS_LFSTR;

   Buffer += Indent2;
   Buffer += _T("<rotation field=\"");
   Buffer += rotation_field;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<sizescale field=\"");
   Buffer += sizescale_field;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("</renderer-v2>");
   Buffer += GS_LFSTR;

   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLLabelAttributes                          <internal> */
/*+
  Questa funzione restituisce la descrizione XML delle etichette del layer con il vecchio
  sistema di etichettatura di QGIS 1.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;     Puntatore alla classe
  int Type;          Tipo di oggetti (TYPE_POLYLINE per oggetti lineari, 
                     TYPE_TEXT per etichette, TYPE_NODE per elementi puntuali,
                     TYPE_SURFACE per poligoni)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLLabelAttributes(FILE *f, C_CLASS *pCls, int Type, const TCHAR *CurrentIndent = NULL)
{
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_FAS        *pFAS = pCls->ptr_fas();
   BYTE         Red, Green, Blue;
   C_STRING     Buffer;
   C_STRING Indent1 = CurrentIndent, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;

   if (gsc_getRGBColor(pFAS->color, pCls->get_PrjId(), pFAS->layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
   gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

   Buffer = Indent1;
   Buffer += _T("<labelattributes>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<label fieldname=\"");
   if (Type == TYPE_TEXT) Buffer += pGphInfo->text_attrib;
   Buffer += _T("\" text=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<family fieldname=\"\" name=\"MS Shell Dlg 2\"/>");
   Buffer += GS_LFSTR;
   // da Unità di mappa con valore di altezza testo di default = 1
   Buffer += Indent2;
   Buffer += _T("<size fieldname=\"");
   if (Type == TYPE_TEXT) Buffer += pGphInfo->h_text_attrib;
   Buffer += _T("\" units=\"mu\" value=\"1\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<bold fieldname=\"\" on=\"0\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<italic fieldname=\"\" on=\"0\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<underline fieldname=\"\" on=\"0\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<strikeout fieldname=\"\" on=\"0\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<color fieldname=\"\" red=\"");
   Buffer += (int) Red;
   Buffer += _T("\" blue=\"");
   Buffer += (int) Blue;
   Buffer += _T("\" green=\"");
   Buffer += (int) Green;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<x fieldname=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<y fieldname=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<offset x=\"0\" y=\"0\" units=\"pt\" yfieldname=\"\" xfieldname=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<angle fieldname=\"");
   if (Type == TYPE_TEXT) Buffer += pGphInfo->rotation_attrib;
   Buffer += _T("\" value=\"0\" auto=\"0\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<alignment fieldname=\"\" value=\"aboveright\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<buffercolor fieldname=\"\" red=\"255\" blue=\"255\" green=\"255\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<buffersize fieldname=\"\" units=\"pt\" value=\"1\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<bufferenabled fieldname=\"\" on=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<multilineenabled fieldname=\"\" on=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<selectedonly on=\"\"/>");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("</labelattributes>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLLabelAttributesCustomProperties          <internal> */
/*+
  Questa funzione restituisce la descrizione XML delle etichette del layer con il nuovo
  sistema di etichettatura di QGIS 2.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_CLASS *pCls;     Puntatore alla classe
  long minimumScale; Scala minima di visualizzazione (0 = non usato))
  long maximumScale; Scala massima di visualizzazione (0 = non usato))
  int Type;          Tipo di oggetti (TYPE_POLYLINE per oggetti lineari, 
                     TYPE_TEXT per etichette, TYPE_NODE per elementi puntuali,
                     TYPE_SURFACE per poligoni)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLLabelAttributesCustomProperties(FILE *f, C_CLASS *pCls, 
                                                  long minimumScale, long maximumScale,
                                                  const TCHAR *CurrentIndent = NULL)
{
   C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_FAS        *pFAS = pCls->ptr_fas();
   BYTE         Red, Green, Blue;
   C_STRING     Buffer;

   if (gsc_getRGBColor(pFAS->color, pCls->get_PrjId(), pFAS->layer, &Red, &Green, &Blue) == GS_BAD) return GS_BAD; 
   gsc_correctRGBColor(&Red, &Green, &Blue); // correggo il colore in base al colore di sfondo acad

   Buffer = CurrentIndent;
   Buffer += _T("<property key=\"labeling\" value=\"pal\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/bufferDraw\" value=\"false\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/dataDefined/Rotation\" value=\"1~~0~~~~");
   Buffer += pGphInfo->rotation_attrib;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/dataDefined/Size\" value=\"1~~0~~~~");
   Buffer += pGphInfo->h_text_attrib;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/enabled\" value=\"true\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/fieldName\" value=\"");
   Buffer += pGphInfo->text_attrib;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/fontSizeInMapUnits\" value=\"true\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/scaleMax\" value=\""); // QGIS inverte maximumScale e minimumScale nel file di progetto
   if (minimumScale == -1) Buffer += _T("0");
   else Buffer += minimumScale;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/scaleMin\" value=\""); // QGIS inverte maximumScale e minimumScale nel file di progetto
   if (maximumScale == -1) Buffer += _T("1e+08");
   else Buffer += maximumScale;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/scaleVisibility\" value=\"true\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/shadowDraw\" value=\"false\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/shapeDraw\" value=\"false\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/textColorA\" value=\"255\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/textColorB\" value=\"");
   Buffer += (int) Blue;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/textColorG\" value=\"");
   Buffer += (int) Green;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   Buffer += CurrentIndent;
   Buffer += _T("<property key=\"labeling/textColorR\" value=\"");
   Buffer += (int) Red;
   Buffer += _T("\"/>");
   Buffer += GS_LFSTR;

   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLMapLayer                                 <internal> */
/*+
  Questa funzione restituisce la descrizione XML del layer.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_STRING &DBName;  Nome del database PostGIS
  C_STRING &Host;    Nome o tcpip del computer
  C_STRING &Port;    postra di comunicazione
  C_STRING &User;
  C_STRING &Password;                      
  C_CLASS *pCls;     Puntatore alla classe
  int Type;          Tipo di oggetti (TYPE_POLYLINE per oggetti lineari, 
                     TYPE_TEXT per etichette, TYPE_NODE per elementi puntuali,
                     TYPE_SURFACE per poligoni) usato solo identificare 
                     la componente delle classi spaghetti
  bool Label;        usato per identificare la componente delle classi 
                     escluse gli spaghetti
  int DefaultSRID;   Codice sistema di coordinate usato se la classe ne è priva (default = 3003)
  C_2LONG_INT_LIST *pGeomScaleFactorList; opzionale, puntatore alla lista dei fattori di scala 
                                          di visualizzazione per i layer di geometria;
                                          ogni elemento contiene 
                                          (<cls> <fattore minimo> <fattore massimo>) (default = NULL)
  C_2LONG *pLblScaleFactors; opzionale, puntatore al fattore di scala 
                             di visualizzazione per i layer delle etichette;
                             (<fattore minimo> <fattore massimo>) (default = NULL)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLMapLayer(FILE *f, C_STRING &DBName, 
                           C_STRING &Host, C_STRING &Port,
                           C_STRING &User, C_STRING &Password,                      
                           C_CLASS *pCls, int Type, bool Label,
                           int DefaultSRID, C_2LONG_INT_LIST *pGeomScaleFactorList = NULL,
                           C_2LONG *pLblScaleFactors = NULL,
                           const TCHAR *CurrentIndent = NULL)
{
   C_STRING       Buffer, XMLStr, LayerID, LayerName, GeomType, FullRefView, SQL, LabelField, ViewType = _T("geom");
   C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
   C_DBCONNECTION *pGphConn = pGphInfo->getDBConnection();
   int            TransparencyLevelInt = 255, LabelInt = 0, SRID; // non trasparente     
   C_2LONG_INT    *pGeomScaleFactor = NULL; // fattore di scala di visualizzazione
   long           minimumScale = -1, maximumScale = -1;
   C_STRING       Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;

   if (pCls->get_category() == CAT_SPAGHETTI)
   {
      if (pGeomScaleFactorList &&
          (pGeomScaleFactor = (C_2LONG_INT *) pGeomScaleFactorList->search_key(pCls->ptr_id()->code)))
      {
         minimumScale = pGeomScaleFactor->get_id(); maximumScale = pGeomScaleFactor->get_id_2();
      }

      gsc_getQGISLayerInfo((C_SPAGHETTI *) pCls, Type, LayerID, LayerName);
      gsc_getQGISGeomType(Type, GeomType);
      // ricavo il nome della vista
      if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, _T("geom"), NULL, NULL,
                                           &FullRefView) == GS_BAD) return GS_BAD;
      SQL = _T("geometrytype(\"");
      XMLStr = pGphInfo->geom_attrib; XMLStr.toAXML();
      SQL += XMLStr;
      SQL += _T("\") IN ");
      switch (Type)
      {
         case TYPE_POLYLINE:
            SQL += _T("('LINESTRING','MULTILINESTRING')");
            break;
         case TYPE_NODE:
         {
            C_STRING AttribName(pGphInfo->text_attrib);

            if (gsc_AdjSyntax(AttribName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                              pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
               return GS_BAD;
            SQL += _T("('POINT','MULTIPOINT') AND ");
            XMLStr = AttribName; XMLStr.toAXML();
            SQL += XMLStr;
            SQL += _T(" IS NULL");
            break;
         }
         case TYPE_TEXT:
         {
            C_STRING AttribName(pGphInfo->text_attrib);

            if (gsc_AdjSyntax(AttribName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                              pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
               return GS_BAD;
            SQL += _T("('POINT','MULTIPOINT') AND ");
            XMLStr = AttribName; XMLStr.toAXML();
            SQL += XMLStr;
            SQL += _T(" IS NOT NULL");
            //TransparencyLevelInt = 0;
            LabelInt             = 1;
            LabelField           = pGphInfo->text_attrib;
            // fattore di scala di visualizzazione per le etichette
            if (pLblScaleFactors)
            {
               minimumScale = pLblScaleFactors->get_id(); maximumScale = pLblScaleFactors->get_id_2();
            }
            break;
         }
         case TYPE_SURFACE:
            SQL += _T("('POLYGON','MULTIPOLYGON')");
            break;
         default:
            GS_ERR_COD = eGSInvClassType;
            return GS_BAD;
      }
   }
   else // classe semplice
   {
      gsc_getQGISLayerInfo((C_SIMPLEX *) pCls, Label, LayerID, LayerName);

      if (pGeomScaleFactorList && 
          (pGeomScaleFactor = (C_2LONG_INT *) pGeomScaleFactorList->search_key(pCls->ptr_id()->code)))
      {
         minimumScale = pGeomScaleFactor->get_id(); maximumScale = pGeomScaleFactor->get_id_2();
      }

      if (Label)
      {
         C_RB_LIST LblTableStru;
         presbuf   p;
         long      DataType;


         // Leggo la struttura della tabella delle label per sapere il tipo del campo attrib_invis_attrib
         if ((LblTableStru << pGphConn->ReadStruct(pGphInfo->LblTableRef.get_name())) == NULL)
            return GS_BAD;
         if ((p = LblTableStru.Assoc(pGphInfo->attrib_invis_attrib.get_name(), FALSE)) == NULL)
            return GS_BAD;
         if ((p = gsc_nth(1, p)) == NULL || gsc_rb2Lng(p, &DataType) == GS_BAD) return GS_BAD;

         XMLStr = pGphInfo->attrib_invis_attrib; XMLStr.toAXML();
         SQL = XMLStr;
         SQL += _T(" IS NULL OR ");
         SQL += XMLStr;

         if (gsc_DBIsBoolean((DataTypeEnum) DataType) == GS_GOOD)
            SQL += _T("=FALSE");
         else if (gsc_DBIsNumeric((DataTypeEnum) DataType) == GS_GOOD)
            SQL += _T("=0");
         else if (gsc_DBIsChar((DataTypeEnum) DataType) == GS_GOOD)
            SQL += _T("='0'");
         else
            return GS_BAD;

         Type                 = TYPE_TEXT;
         //TransparencyLevelInt = 0;
         LabelInt             = 1;
         LabelField           = pGphInfo->text_attrib;
         ViewType             = _T("lbl");
         if (pLblScaleFactors)
         {
            minimumScale = pLblScaleFactors->get_id(); maximumScale = pLblScaleFactors->get_id_2();
         }
      }
      else
      {
         Type = pCls->get_type();
         if (Type == TYPE_TEXT)
         {
            //TransparencyLevelInt = 0;
            LabelInt             = 1;
            LabelField           = pGphInfo->text_attrib;
         }
      }

      gsc_getQGISGeomType(Type, GeomType);

      // ricavo il nome della vista
      if (gsc_pgviewGetAuxiliaryInfoOnView(pCls, ViewType.get_name(), NULL, NULL,
                                           &FullRefView) == GS_BAD) return GS_BAD;
   }

   C_STRING Catalog, Schema, ViewName;

   SRID = pGphInfo->coordinate_system.toi();
   if (gsc_getQGISXMLSpatialRefSys(Catalog, SRID) == GS_BAD) SRID = DefaultSRID;
   Catalog.clear();

   // elimino eventuale schema (database)
   if (pGphConn->split_FullRefTable(FullRefView, Catalog, Schema, ViewName) == GS_BAD)
      return GS_BAD;
   if (gsc_AdjSyntax(Schema, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD ||
       gsc_AdjSyntax(ViewName, pGphConn->get_InitQuotedIdentifier(), pGphConn->get_FinalQuotedIdentifier(),
                     pGphConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;
   FullRefView = Schema;
   FullRefView += _T(".");
   FullRefView += ViewName;

   // fattore di scala di visualizzazione
   Buffer = Indent1;
   Buffer += _T("<maplayer minimumScale=\""); // QGIS inverte maximumScale e minimumScale nel file di progetto
   if (maximumScale == -1) Buffer += _T("1e+08");
   else Buffer += maximumScale;
   
   Buffer += _T("\" maximumScale=\""); // QGIS inverte maximumScale e minimumScale nel file di progetto
   if (minimumScale == -1) Buffer += _T("0");
   else Buffer += minimumScale;
   
   Buffer += _T("\" geometry=\"");

   Buffer += GeomType;
   Buffer += _T("\" type=\"vector\" hasScaleBasedVisibilityFlag=\"");
   if (minimumScale == -1 && maximumScale == -1) // senza fattore di scala di visualizzazione
      Buffer += _T("0\">");
   else
      Buffer += _T("1\">");

   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<id>");
   Buffer += LayerID;
   Buffer += _T("</id>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<datasource>dbname='");
   XMLStr = DBName; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("' host=");
   XMLStr = Host; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T(" port=");
   Buffer += Port;
   Buffer += _T(" user='");
   XMLStr = User; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("' password='");
   XMLStr = Password; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("' sslmode=disable key='");
   XMLStr = pGphInfo->key_attrib; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("' table=");
   Buffer += FullRefView;
   Buffer += _T(" (");
   XMLStr = pGphInfo->geom_attrib; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T(") sql=");
   Buffer += SQL;
   Buffer += _T("</datasource>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<layername>");
   XMLStr = LayerName; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("</layername>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<srs>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   Buffer.clear();
   if (gsc_getQGISXMLSpatialRefSys(Buffer, SRID, Indent3.get_name()) == GS_BAD) return GS_BAD;
   Buffer += Indent2;
   Buffer += _T("</srs>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<transparencyLevelInt>");
   Buffer += TransparencyLevelInt;
   Buffer += _T("</transparencyLevelInt>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<provider encoding=\"System\">postgres</provider>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<vectorjoins/>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (gsc_getQGISXMLRenderer(f, pCls, Type, Indent2.get_name()) == GS_BAD) return GS_BAD;

   Buffer = Indent2;
   Buffer += _T("<customproperties>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (Type == TYPE_TEXT && LabelInt == 1)
      if (gsc_getQGISXMLLabelAttributesCustomProperties(f, pCls, minimumScale, maximumScale, Indent3.get_name()) == GS_BAD) return GS_BAD;

   Buffer = Indent2;
   Buffer += _T("</customproperties>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<displayfield>");
   XMLStr = pGphInfo->key_attrib; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("</displayfield>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (gsc_getQGISXMLEditTypes(f, pCls, ViewType.get_name(), Indent2.get_name()) == GS_BAD) return GS_BAD;

   Buffer = Indent2;
   Buffer += _T("<editform></editform>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<editforminit></editforminit>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<attributeactions/>");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("</maplayer>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLProjectLayers                            <internal> */
/*+
  Questa funzione restituisce la descrizione XML del layer.
  Parametri:
  FILE *f;           Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_STRING &DBName;  Nome del database PostGIS
  C_STRING &Host;    Nome o tcpip del computer
  C_STRING &Port;    postra di comunicazione
  C_STRING &User;
  C_STRING &Password;
  C_CLASS_LIST &ClassList;                Lista delle classi
  int DefaultSRID;                        Codice sistema di coordinate usato se la classe ne è priva (default = 3003)
  C_2LONG_INT_LIST *pGeomScaleFactorList; Opzionale, puntatore alla lista dei fattori di scala 
                                          di visualizzazione per i layer di geometria;
                                          ogni elemento contiene 
                                          (<cls> <fattore minimo> <fattore massimo>) (default = NULL)
  C_2LONG *pLblScaleFactors; Opzionale, puntatore al fattore di scala 
                             di visualizzazione per i layer delle etichette;
                             (<fattore minimo> <fattore massimo>) (default = NULL)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLProjectLayers(FILE *f, C_STRING &DBName, 
                                C_STRING &Host, C_STRING &Port,
                                C_STRING &User, C_STRING &Password,                      
                                C_CLASS_LIST &ClassList, int DefaultSRID,
                                C_2LONG_INT_LIST *pGeomScaleFactorList,
                                C_2LONG *pLblScaleFactors,
                                const TCHAR *CurrentIndent = NULL)
{

   C_CLASS  *pCls = (C_CLASS *) ClassList.get_head();
   int      LayerCount = 0;
   C_STRING Buffer;
   long     i;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(492)); // "Creazione layers QGIS"
   C_STRING Indent1 = CurrentIndent, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;

   while (pCls)
   {
      if (pCls->get_category() == CAT_SPAGHETTI) LayerCount += 4;
      else
         if (pCls->get_category() == CAT_SIMPLEX)
         {
            LayerCount += 1;
            if (pCls->ptr_attrib_list()->is_visible() == GS_GOOD && pCls->get_type() != TYPE_TEXT)
               LayerCount += 1;
         }
      pCls = (C_CLASS *) ClassList.get_next();
   }

   Buffer = Indent1;
   Buffer += _T("<projectlayers layercount=\"");
   Buffer += LayerCount;
   Buffer += _T("\">");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   StatusBarProgressMeter.Init(ClassList.get_count());
   i = 1;

   pCls = (C_CLASS *) ClassList.get_head();
   while (pCls)
   {
      StatusBarProgressMeter.Set(i++);

      if (pCls->get_category() == CAT_SPAGHETTI)
      {  // componente lineare
         if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                    pCls, TYPE_POLYLINE, false, DefaultSRID,
                                    pGeomScaleFactorList, pLblScaleFactors,
                                    Indent2.get_name()) == GS_BAD)
            return GS_BAD;
         // componente puntuale
         if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                    pCls, TYPE_NODE, false, DefaultSRID,
                                    pGeomScaleFactorList, pLblScaleFactors,
                                    Indent2.get_name()) == GS_BAD)
            return GS_BAD;
         // componente etichette
         if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                    pCls, TYPE_TEXT, false, DefaultSRID,
                                    pGeomScaleFactorList, pLblScaleFactors,
                                    Indent2.get_name()) == GS_BAD)
            return GS_BAD;
         // componente areale
         if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                    pCls, TYPE_SURFACE, false, DefaultSRID,
                                    pGeomScaleFactorList, pLblScaleFactors,
                                    Indent2.get_name()) == GS_BAD)
            return GS_BAD;
      }
      else
      if (pCls->get_category() == CAT_SIMPLEX)
      {  // componente geometrica
         if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                    pCls, pCls->get_type(), false, DefaultSRID,
                                    pGeomScaleFactorList, pLblScaleFactors,
                                    Indent2.get_name()) == GS_BAD)
            return GS_BAD;
         // se la classe ha etichette
         if (((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LblTableRef.len() > 0 &&
             pCls->ptr_attrib_list()->is_visible() == GS_GOOD)
         {
            // componente etichette
            if (gsc_getQGISXMLMapLayer(f, DBName, Host, Port, User, Password,               
                                       pCls, 0, true, DefaultSRID,
                                       pGeomScaleFactorList, pLblScaleFactors,
                                       Indent2.get_name()) == GS_BAD)
               return GS_BAD;
         }
      }

      pCls = (C_CLASS *) ClassList.get_next();
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   Buffer = Indent1;
   Buffer += _T("</projectlayers>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getQGISXMLProperties                               <internal> */
/*+
  Questa funzione restituisce la descrizione XML delle proprietà del progetto.
  Parametri:
  FILE *f;     Puntatore a file a cui viene aggiunto in coda la descrizione XML
  int SRID;    Codice sistema di coordinate (default = 3003)
  const TCHAR *CurrentIndent; indentazione corrente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXMLProperties(FILE *f, int SRID, const TCHAR *CurrentIndent = NULL)
{
   C_STRING Buffer;
   C_STRING Indent1 = CurrentIndent, Indent2, Indent3;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;
   Indent3 = Indent2; Indent3 += QGIS_XML_INDENT;
   
   Buffer = Indent1;
   Buffer += _T("<properties>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<SpatialRefSys>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<ProjectCrs type=\"QString\">EPSG:");
   Buffer += SRID;
   Buffer += _T("</ProjectCrs>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("</SpatialRefSys>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("<PositionPrecision>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<DecimalPlaces type=\"int\">2</DecimalPlaces>");
   Buffer += GS_LFSTR;
   Buffer += Indent3;
   Buffer += _T("<Automatic type=\"bool\">true</Automatic>");
   Buffer += GS_LFSTR;
   Buffer += Indent2;
   Buffer += _T("</PositionPrecision>");
   Buffer += GS_LFSTR;

   Buffer += Indent2;
   Buffer += _T("<Gui>");
   Buffer += GS_LFSTR;
   // Colore sfondo
   AcColorSettings ColorSettings;
   if (acedGetCurrentColors(&ColorSettings))
   {
      BYTE Red, Green, Blue;

      Red   = GetRValue(ColorSettings.dwGfxModelBkColor);
      Green = GetGValue(ColorSettings.dwGfxModelBkColor & 0xffff);
      Blue  = GetBValue(ColorSettings.dwGfxModelBkColor);

      Buffer += Indent3;
      Buffer += _T("<CanvasColorRedPart type=\"int\">");
      Buffer += (int) Red;
      Buffer += _T("</CanvasColorRedPart>");
      Buffer += GS_LFSTR;
      Buffer += Indent3;
      Buffer += _T("<CanvasColorGreenPart type=\"int\">");
      Buffer += (int) Green;
      Buffer += _T("</CanvasColorGreenPart>");
      Buffer += GS_LFSTR;
      Buffer += Indent3;
      Buffer += _T("<CanvasColorBluePart type=\"int\">");
      Buffer += (int) Blue;
      Buffer += _T("</CanvasColorBluePart>");
      Buffer += GS_LFSTR;
   }
   Buffer += Indent2;
   Buffer += _T("</Gui>");
   Buffer += GS_LFSTR;

   Buffer += Indent1;
   Buffer += _T("</properties>");
   Buffer += GS_LFSTR;

   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}

/*************************************************************************/
/*.doc gsc_getQGISXML                                         <internal> */
/*+
  Questa funzione restituisce la descrizione XML di un file di progetto QGIS.
  Parametri:
  FILE *f;                 Puntatore a file a cui viene aggiunto in coda la descrizione XML
  C_STRING &ProjectName;   Nome del progetto
  C_STRING &DBName;        Nome del database PostGIS
  C_STRING &Host;
  C_STRING &Port;
  C_STRING &User;
  C_STRING &Password;
  int SRID;                Codice sistema di coordinate (default = 3003)
  C_CLASS_LIST &ClassList; Lista delle classi
  C_2STR_LIST *pGroupMaskList; opzionale, puntatore alla lista dei raggruppamenti di classe;
                               ogni elemento contiene (<nome gruppo> < maschera di raggruppamento)
                               (default = NULL)
  C_2LONG_INT_LIST *pGeomScaleFactorList; opzionale, puntatore alla lista dei fattori di scala 
                                          di visualizzazione per i layer di geometria;
                                          ogni elemento contiene 
                                          (<cls> <fattore minimo> <fattore massimo>) (default = NULL)
  C_2LONG *pLblScaleFactors; opzionale, puntatore al fattore di scala 
                             di visualizzazione per i layer delle etichette;
                             (<fattore minimo> <fattore massimo>) (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getQGISXML(FILE *f, C_STRING &ProjectName, 
                   C_STRING &DBName, C_STRING &Host, C_STRING &Port,
                   C_STRING &User, C_STRING &Password,
                   int SRID, C_CLASS_LIST &ClassList, C_2STR_LIST *pGroupMaskList,
                   C_2LONG_INT_LIST *pGeomScaleFactorList, C_2LONG *pLblScaleFactors)
{
   C_STRING Buffer, XMLStr;
   C_STRING Indent1 = QGIS_XML_INDENT, Indent2;
   
   Indent2 = Indent1; Indent2 += QGIS_XML_INDENT;


   Buffer = _T("<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>");
   Buffer += GS_LFSTR;
   Buffer += _T("<qgis projectname=\"");
   XMLStr = ProjectName; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("\" version=\"2.2.0-Valmiera\">");
   Buffer += GS_LFSTR;
   Buffer += Indent1;
   Buffer += _T("<title>");
   XMLStr = ProjectName; XMLStr.toAXML();
   Buffer += XMLStr;
   Buffer += _T("</title>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (gsc_getQGISXMLMapCanvas(f, SRID, Indent1.get_name()) == GS_BAD) return GS_BAD;
   if (gsc_getQGISXMLLegend(f, ClassList, pGroupMaskList, Indent1.get_name()) == GS_BAD) return GS_BAD;
   if (gsc_getQGISXMLProjectLayers(f, DBName, Host, Port, User, Password,                      
                                   ClassList, SRID, pGeomScaleFactorList,
                                   pLblScaleFactors, Indent1.get_name()) == GS_BAD) return GS_BAD;
   if (gsc_getQGISXMLProperties(f, SRID, Indent1.get_name()) == GS_BAD) return GS_BAD;

   Buffer = _T("</qgis>");
   Buffer += GS_LFSTR;
   if (fwprintf(f, Buffer.get_name()) < 0) { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_WriteQGISProjectFile                               <external> */
/*+
  Questa funzione scrive un file di progetto QGIS.
  Parametri:
  C_STRING &QGISPathFile;   File di QGIS
  int      prj;             Codice progetto
  C_STRING &Host;
  C_STRING &Port;
  C_STRING &Database;
  C_STRING &User;
  C_STRING &Password;
  int      SRID;            Codice sistema di coordinate
  C_INT_LIST &ClsCodeList;  Lista dei codici delle classi da esportare in QGIS
  C_2STR_LIST *pGroupMaskList; opzionale, puntatore alla lista dei raggruppamenti di classe;
                               ogni elemento contiene (<nome gruppo> < maschera di raggruppamento)
                               (default = NULL)
  C_2LONG_INT_LIST *pGeomScaleFactorList; opzionale, puntatore alla lista dei fattori di scala 
                                          di visualizzazione per i layer di geometria;
                                          ogni elemento contiene 
                                          (<cls> <fattore minimo> <fattore massimo>) (default = NULL)
  C_2LONG *pLblScaleFactors; opzionale, puntatore al fattore di scala 
                             di visualizzazione per i layer delle etichette;
                             (<fattore minimo> <fattore massimo>) (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_WriteQGISProjectFile(C_STRING &QGISProjectFile, int prj, 
                             C_STRING &Host, C_STRING &Port,
                             C_STRING &Database, C_STRING &User, C_STRING &Password,
                             int SRID, C_INT_LIST &ClsCodeList, C_2STR_LIST *pGroupMaskList,
                             C_2LONG_INT_LIST *pGeomScaleFactorList, C_2LONG *pLblScaleFactors)
{
   C_PROJECT      *pPrj;
   C_STRING       Buffer, ProjectName;
   C_CLASS        *pCls, *pCopiedCls;
   C_CLASS_LIST   ClassList;
   C_INT          *pClsCode = (C_INT *) ClsCodeList.get_head();
   FILE           *f;
   C_STRING       Drive, Dir, FileName, Ext;

   gsc_splitpath(QGISProjectFile, NULL, NULL, NULL, &Ext);
   if (Ext.len() == 0) QGISProjectFile += _T(".qgs"); // aggiungo l'estensione

   if (gsc_path_exist(QGISProjectFile) == GS_GOOD)
      if (gsc_delfile(QGISProjectFile) == GS_BAD) return GS_BAD;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   // Considero solo le classi postgis di tipo spaghetti e semplici
   while (pClsCode)
   {
      if ((pCls = pPrj->find_class(pClsCode->get_key())) == NULL) return GS_BAD;

      // Le classi gestite sono quelle con db in PostGIS
      if (gsc_is_class_with_pg_data(pCls) == false)
         { pClsCode = (C_INT *) ClsCodeList.get_next(); continue; }

      // per ora solo classi sempilici e spaghetti
      if (pCls->get_category() != CAT_SPAGHETTI && pCls->get_category() != CAT_SIMPLEX)
         { pClsCode = (C_INT *) ClsCodeList.get_next(); continue; }

      if (!(pCopiedCls = gsc_alloc_new_class(pCls->get_category()))) return GS_BAD;

      if (pCls->copy(pCopiedCls) == GS_BAD) return GS_BAD;
      ClassList.add_tail(pCopiedCls);

      pClsCode = (C_INT *) ClsCodeList.get_next();
   }

   ProjectName = pPrj->get_name();

   if ((f = gsc_fopen(QGISProjectFile, _T("w"))) == NULL) return GS_BAD;

   if (gsc_getQGISXML(f, ProjectName, Database, Host, Port,
                      User, Password, SRID, ClassList, 
                      pGroupMaskList, pGeomScaleFactorList, pLblScaleFactors) == GS_BAD)
      { gsc_fclose(f); return GS_BAD; }

   gsc_fclose(f);

   // Scrivo il file per registrare la connessione postgresql di qgis
   gsc_splitpath(QGISProjectFile, &Drive, &Dir, &FileName);
   Drive += Dir;
   Drive += FileName;
   Drive += _T(".reg");
   if (gsc_WriteQGISPostgreSQLGConnectionRegFile(Drive, ProjectName, Host, Database, Port, User, Password) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER QUANTUM GIS 1.7.4
// INIZIO FUNZIONI PER HISTORY
///////////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_getHistorySequenceRef                              <external> */
/*+
  Questa funzione ricava il riferimento della sequenza sull'history_id dello storico.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &HistoryTableRef;  Riferimento completo tabella storica (input)
  C_STRING &SequenceRef;      Riferimento completo sequenza (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getHistorySequenceName(C_DBCONNECTION *pConn, C_STRING &HistoryTableRef, C_STRING &SequenceName)
{
   C_STRING Catalog, Schema;

   if (pConn->split_FullRefTable(HistoryTableRef, Catalog, Schema, SequenceName) == GS_BAD)
      return GS_BAD;
   SequenceName += _T("_history_id_seq");
   SequenceName.strtran(_T(" "), _T("_"));

   return GS_GOOD;
}
int gsc_getHistorySequenceRef(C_DBCONNECTION *pConn, C_STRING &HistoryTableRef, C_STRING &SequenceRef)
{
   C_STRING Catalog, Schema, SequenceName;

   if (pConn->split_FullRefTable(HistoryTableRef, Catalog, Schema, SequenceName) == GS_BAD)
      return GS_BAD;
   if (gsc_getHistorySequenceName(pConn, HistoryTableRef, SequenceName) == GS_BAD)
      return GS_BAD;
   if (SequenceRef.paste(pConn->get_FullRefTable(Catalog, Schema, SequenceName)) == NULL)
       return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getHistoryTriggerFunctionRef                       <external> */
/*+
  Questa funzione ricava il riferimento della funzione trigger per popolare lo storico.
  Parametri:
  C_DBCONNECTION *pConn;         Connessione OLE-DB (input)
  C_STRING &TableRef;            Riferimento completo tabella dati (input)
  C_STRING &TriggerFunctionRef;  Riferimento completo funzione trigger (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getHistoryTriggerFunctionRef(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &TriggerFunctionRef)
{
   C_STRING Catalog, Schema, Name, TriggerName;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;
   TriggerName = _T("gs_");
   TriggerName += Name;
   TriggerName += _T("_audit");
   TriggerName.strtran(_T(" "), _T("_"));
   if (TriggerFunctionRef.paste(pConn->get_FullRefTable(Catalog, Schema, TriggerName)) == NULL)
       return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_getHistoryTriggerName                              <external> */
/*+
  Questa funzione ricava il nome del trigger per popolare lo storico.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &TableRef;         Riferimento completo tabella dati (input)
  C_STRING &TriggerName;      Nome del trigger (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_getHistoryTriggerName(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &TriggerName)
{
   C_STRING Catalog, Schema, Name;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;
   TriggerName = _T("gs_");
   TriggerName += Name;
   TriggerName += _T("_audit");
   TriggerName.strtran(_T(" "), _T("_"));

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_DropHistoryTrigger                                 <external> */
/*+
  Questa funzione cancella il trigger associato alla tabella dati.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &TableRef;         Riferimento completo tabella (input)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_DropHistoryTrigger(C_DBCONNECTION *pConn, C_STRING &TableRef)
{
   C_STRING TriggerName;

   if (gsc_getHistoryTriggerName(pConn, TableRef, TriggerName) == GS_BAD)
      return GS_BAD;

   return pConn->DelTrigger(TriggerName.get_name(), TableRef.get_name());
}


/*************************************************************************/
/*.doc gsc_CreateHistoryTrigger                               <external> */
/*+
  Questa funzione crea la funzione per l'auditing sullo storico e il trigger 
  da associare alla tabella dati.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &TableRef;         Riferimento completo tabella (input)
  C_STRING &HistoryTableRef;  Riferimento completo tabella storica (input)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_CreateHistoryTrigger(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &HistoryTableRef)
{
   C_STRING Stm, TriggerFunctionRef, SequenceRef, TriggerName;

   if (gsc_getHistoryTriggerFunctionRef(pConn, TableRef, TriggerFunctionRef) == GS_BAD)
      return GS_BAD;
   if (gsc_getHistorySequenceRef(pConn, HistoryTableRef, SequenceRef) == GS_BAD)
      return GS_BAD;

   // per creare la funzione lanciata dal trigger:
   // CREATE OR REPLACE FUNCTION history.gs_dati_audit()
   //  RETURNS trigger AS
   // $BODY$
   // DECLARE
   //    BEGIN
   //        IF (TG_OP = 'DELETE') THEN
   //           INSERT INTO dati SELECT nextval('dati_id_seq'::regclass), 'D', now(), SUBSTRING(user FOR 20), OLD.*;
   //        ELSE
   //           INSERT INTO dati SELECT nextval('dati_id_seq'::regclass), USING SUBSTRING(TG_OP FOR 1 now(), SUBSTRING(user FOR 20), NEW.*;
   //        END IF;

   //        RETURN NULL; -- result is ignored since this is an AFTER trigger
   //    END;
   // $BODY$
   //  LANGUAGE plpgsql VOLATILE
   //  COST 100;
   // GRANT EXECUTE ON FUNCTION history.gs_dati_audit() TO mapserver;

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += TriggerFunctionRef;
   Stm += _T("()");
   Stm += GS_LFSTR;
   Stm += _T("RETURNS trigger AS");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$");
   Stm += GS_LFSTR;
   Stm += _T("DECLARE");
   Stm += GS_LFSTR;
   Stm += _T("BEGIN");
   Stm += GS_LFSTR;
   Stm += _T("IF (TG_OP = 'DELETE') THEN");
   Stm += GS_LFSTR;
   Stm += _T("INSERT INTO ");
   Stm += HistoryTableRef;
   Stm += _T(" SELECT nextval('");
   Stm += SequenceRef;
   Stm += _T("'::regclass), 'D', now(), SUBSTRING(user FOR 20), OLD.*;");
   Stm += GS_LFSTR;
   Stm += _T("ELSE");
   Stm += GS_LFSTR;
   Stm += _T("INSERT INTO ");
   Stm += HistoryTableRef;
   Stm += _T(" SELECT nextval('");
   Stm += SequenceRef;
   Stm += _T("'::regclass), SUBSTRING(TG_OP FOR 1), now(), SUBSTRING(user FOR 20), NEW.*;");
   Stm += GS_LFSTR;
   Stm += _T("END IF;");
   Stm += GS_LFSTR;
   Stm += _T("RETURN NULL;");
   Stm += GS_LFSTR;
   Stm += _T("END;");
   Stm += GS_LFSTR;
   Stm += _T("$BODY$");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE plpgsql VOLATILE");
   Stm += GS_LFSTR;
   Stm += _T("COST 100;");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // per creare il trigger:
   // CREATE TRIGGER gs_dati_audit
   //  AFTER INSERT OR UPDATE OR DELETE
   //  ON prove.dati
   //  FOR EACH ROW
   //  EXECUTE PROCEDURE history.gs_process_audit('prove.dati_history');

   // prima lo cancello per sicurezza
   if (gsc_DropHistoryTrigger(pConn, TableRef) == GS_BAD) return GS_BAD;

   if (gsc_getHistoryTriggerName(pConn, TableRef, TriggerName) == GS_BAD)
      return GS_BAD;
   Stm = _T("CREATE TRIGGER ");
   Stm += TriggerName;
   Stm += GS_LFSTR;
   Stm += _T("AFTER INSERT OR UPDATE OR DELETE ON ");
   Stm += TableRef;
   Stm += GS_LFSTR;
   Stm += _T("FOR EACH ROW ");
   Stm += GS_LFSTR;
   Stm += _T("EXECUTE PROCEDURE ");
   Stm += TriggerFunctionRef;
   Stm += _T("()");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += TriggerFunctionRef;
   Stm += _T("() TO mapserver");

   return pConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_CreateHistoryTable                                 <external> */
/*+
  Questa funzione crea le tabelle per memorizzare lo storicizzazione 
  dei dati di una tabella.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &TableRef;         Riferimento completo tabella (input)
  C_STRING &HistorySchema;    Schema in cui creare la tabella storica (input)
  C_STRING &ent_key_attrib;   Nome del campo contenente il codice dell'entita' (input)
  C_STRING &HistoryTableRef;  Riferimento completo tabella storica (out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_CreateHistoryTable(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &HistorySchema,
                           C_STRING &ent_key_attrib, C_STRING &HistoryTableRef)
{
   C_STRING  Stm, Catalog, Schema, Name, FullRefTable, SequenceRef;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;
   Name += _T("_history");

   HistoryTableRef.paste(pConn->get_FullRefTable(Catalog, HistorySchema, Name));

   if (gsc_getHistorySequenceRef(pConn, HistoryTableRef, SequenceRef) == GS_BAD)
      return GS_BAD;

   // se esiste già la tabella history la rinomino aggiungendo al suo nome 
   // il suffisso yyyy_mm_dd_hh_mm_ss
   if (pConn->ExistTable(HistoryTableRef) == GS_GOOD) 
   {
      C_STRING NewTableName, dummy;

      COleDateTime CurrentDateTime;
 
      NewTableName = Name;
      NewTableName += _T("_till_");
      CurrentDateTime = COleDateTime::GetCurrentTime();
      NewTableName += CurrentDateTime.GetYear();
      NewTableName += _T('_');
      NewTableName += CurrentDateTime.GetMonth();
      NewTableName += _T('_');
      NewTableName += CurrentDateTime.GetDay();
      NewTableName += _T('_');
      NewTableName += CurrentDateTime.GetHour();
      NewTableName += _T('_');
      NewTableName += CurrentDateTime.GetMinute();
      NewTableName += _T('_');
      NewTableName += CurrentDateTime.GetSecond();

      // creo una tabella di backup
      // CREATE TABLE new_table AS SELECT * FROM old_table
      FullRefTable.paste(pConn->get_FullRefTable(Catalog, HistorySchema, NewTableName));
      Stm = _T("CREATE TABLE ");
      Stm += FullRefTable;
      Stm += _T(" AS SELECT * FROM ");
      Stm += HistoryTableRef;
      if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

      // INIZIO STRUTTURE PER INTERROGARE IL BACKUP STORICO

      // per aggiungere un indice primario con chiave history_id:
      // ALTER TABLE history.dati_history ADD CONSTRAINT dati_history_history_id PRIMARY KEY(history_id);
      dummy = NewTableName;
      dummy += _T("_history_id");
      dummy.strtran(_T(" "), _T("_"));
      if (pConn->CreatePrimaryKey(FullRefTable.get_name(), dummy.get_name(), _T("history_id")) == GS_BAD)
         return GS_BAD;

      // per aggiungere un indice con chiave history_date in ordine decrescente:
      // CREATE INDEX dati_history_history_date ON history.dati_history USING btree (history_date DESC);
      dummy = NewTableName;
      dummy += _T("_history_date");
      dummy.strtran(_T(" "), _T("_"));
      Stm = _T("CREATE INDEX ");
      Stm += dummy;
      Stm += _T(" ON ");
      Stm += FullRefTable;
      Stm += _T(" USING btree (history_date DESC)");
      if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

      // per aggiungere un indice con chiave gs_id_parent:
      // CREATE INDEX dati_history_gs_id_parent ON history.dati_history USING btree (gs_id_parent);
      dummy = NewTableName;
      dummy += _T("_");
      dummy += ent_key_attrib;
      dummy.strtran(_T(" "), _T("_"));
      Stm = _T("CREATE INDEX ");
      Stm += dummy;
      Stm += _T(" ON ");
      Stm += FullRefTable;
      Stm += _T(" USING btree (");
      Stm += ent_key_attrib;
      Stm += _T(")");
      if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

      if (gsc_CreateHistoryQryFunction(pConn, FullRefTable, ent_key_attrib) == GS_BAD)
         return GS_BAD;

      // FINE STRUTTURE PER INTERROGARE IL BACKUP STORICO

      // cancello lo storico originale
      // DROP TABLE old_table CASCADE
      Stm = _T("DROP TABLE ");
      Stm += HistoryTableRef;
      Stm += _T(" CASCADE");
      if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
      // cancello la sequenza
      if (pConn->DelSequence(SequenceRef.get_name()) == GS_BAD) return GS_BAD;
      // cancello il trigger della tabella dati
      if (gsc_DropHistoryTrigger(pConn, TableRef) == GS_BAD) return GS_BAD;
   }

   // creo la sequenza
   if (pConn->CreateSequence(SequenceRef.get_name()) == GS_BAD) return GS_BAD;

   // per creare e riempire la tabella history:
   // creo la tabella storica come oggetti nuovi ("I" di Inser) nella data corrente (now)
   // CREATE TABLE history.dati_history AS
   //  SELECT nextval('history.dati_history_id_seq'::regclass)::integer AS id, 'I'::varchar(1) AS operation, now()::timestamp AS date, user::varchar(20) AS usr, * FROM prove.dati;
   Stm = _T("CREATE TABLE ");
   Stm += HistoryTableRef;
   Stm += _T(" AS SELECT nextval('");
   Stm += SequenceRef;
   Stm += _T("'::regclass)::integer AS history_id, 'I'::varchar(1) AS history_operation, ");
   Stm += _T("now()::timestamp AS history_date, user::varchar(20) AS history_usr, * FROM ");
   Stm += TableRef;
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // per rendere il campo history_id di tipo serial (contatore):
   // ALTER TABLE history.dati_history ALTER COLUMN history_id SET NOT NULL;
   // ALTER TABLE history.dati_history ALTER COLUMN history_id SET DEFAULT nextval('history.dati_history_id_seq'::regclass);
   Stm = _T("ALTER TABLE ");
   Stm += HistoryTableRef;
   Stm += _T(" ALTER COLUMN history_id SET NOT NULL");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   Stm = _T("ALTER TABLE ");
   Stm += HistoryTableRef;
   Stm += _T(" ALTER COLUMN history_id SET DEFAULT nextval('");
   Stm += SequenceRef;
   Stm += _T("'::regclass)");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // per aggiungere un indice primario con chiave history_id:
   // ALTER TABLE history.dati_history ADD CONSTRAINT dati_history_history_id PRIMARY KEY(history_id);
   FullRefTable = Name;
   FullRefTable += _T("_history_id");
   FullRefTable.strtran(_T(" "), _T("_"));
   if (pConn->CreatePrimaryKey(HistoryTableRef.get_name(), FullRefTable.get_name(), _T("history_id")) == GS_BAD)
      return GS_BAD;

   // per aggiungere un indice con chiave history_date in ordine decrescente:
   // CREATE INDEX dati_history_history_date ON history.dati_history USING btree (history_date DESC);
   FullRefTable = Name;
   FullRefTable += _T("_history_date");
   FullRefTable.strtran(_T(" "), _T("_"));
   Stm = _T("CREATE INDEX ");
   Stm += FullRefTable;
   Stm += _T(" ON ");
   Stm += HistoryTableRef;
   Stm += _T(" USING btree (history_date DESC)");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   // per aggiungere un indice con chiave gs_id_parent:
   // CREATE INDEX dati_history_gs_id_parent ON history.dati_history USING btree (gs_id_parent);
   FullRefTable = Name;
   FullRefTable += _T("_");
   FullRefTable += ent_key_attrib;
   FullRefTable.strtran(_T(" "), _T("_"));
   Stm = _T("CREATE INDEX ");
   Stm += FullRefTable;
   Stm += _T(" ON ");
   Stm += HistoryTableRef;
   Stm += _T(" USING btree (");
   Stm += ent_key_attrib;
   Stm += _T(")");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_CreateHistoryQryFunction                           <external> */
/*+
  Questa funzione crea una funzione per la lettura dei dati storici in un 
  momento noto. Riceve un parametro di tipo timestamp.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB (input)
  C_STRING &HistoryTableRef;  Riferimento completo tabella storica (input)
  C_STRING &ent_key_attrib;   Nome del campo contenente il codice dell'entita' (input)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_CreateHistoryQryFunction(C_DBCONNECTION *pConn, C_STRING &HistoryTableRef, C_STRING &ent_key_attrib)
{
   C_STRING Stm;

   // per creare la funzione di interrogazione:
   //CREATE OR REPLACE FUNCTION prove.dati_history(my_date TIMESTAMP)
   //RETURNS SETOF prove.dati_history
   //AS $$ 
   // SELECT a.*
   //   FROM prove.dati_history a
   //  WHERE a.history_id = (SELECT b.history_id
   //          FROM prove.dati_history b
   //          WHERE b.history_date <= $1 AND b.gs_id = a.gs_id
   //          ORDER BY b.history_date DESC
   //          LIMIT 1) AND a.history_operation <> 'D'
   //$$
   //LANGUAGE SQL;
   // GRANT EXECUTE ON FUNCTION prove.dati_history(my_date TIMESTAMP) TO mapserver;

   Stm = _T("CREATE OR REPLACE FUNCTION ");
   Stm += HistoryTableRef;
   Stm += _T("(my_date TIMESTAMP)");
   Stm += GS_LFSTR;
   Stm += _T("RETURNS SETOF ");
   Stm += HistoryTableRef;
   Stm += GS_LFSTR;
   Stm += _T("AS $$");
   Stm += GS_LFSTR;
   Stm += _T("SELECT a.* FROM");
   Stm += HistoryTableRef;
   Stm += _T(" a ");
   Stm += GS_LFSTR;
   Stm += _T("WHERE a.history_id = (SELECT b.history_id FROM ");
   Stm += HistoryTableRef;
   Stm += _T(" b ");
   Stm += GS_LFSTR;
   Stm += _T("WHERE b.history_date <= $1 AND b.");
   Stm += ent_key_attrib;
   Stm += _T(" = a.");
   Stm += ent_key_attrib;
   Stm += _T(" ORDER BY b.history_date DESC LIMIT 1)");
   Stm += GS_LFSTR;
   Stm += _T("AND a.history_operation <> 'D'");
   Stm += GS_LFSTR;
   Stm += _T("$$");
   Stm += GS_LFSTR;
   Stm += _T("LANGUAGE SQL;");
   if (pConn->ExeCmd(Stm) == GS_BAD) return GS_BAD;
   
   Stm = _T("GRANT EXECUTE ON FUNCTION ");
   Stm += HistoryTableRef;
   Stm += _T("(my_date TIMESTAMP) TO mapserver");

   return pConn->ExeCmd(Stm);
}


/*************************************************************************/
/*.doc gsc_CreateHistorySystem4Cls                            <external> */
/*+
  Questa funzione crea le tabelle per memorizzare lo storicizzazione 
  dei dati di una classe di GEOsim (solo geometria e alfanumerici senza etichette).
  Parametri:
  C_CLASS *pCls;                    Classe di geosim
  C_STRING &HistoryGeomTableRef;    Riferimento completo tabella storica x la geometria (out)
  C_STRING &HistoryAlfanumTableRef; Riferimento completo tabella storica x gli alfanumerici (out)
  bool CreateClass;                 Flag di creazione classi di GEOsim collegate alla vista 
                                    della tabella storica (default = true)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_CreateHistorySystem4Cls(C_CLASS *pCls, 
                                C_STRING &HistoryGeomTableRef, C_STRING &HistoryAlfanumTableRef,
                                bool CreateClass)
{
   C_STRING       Stm, HistorySchema(_T("history"));
	_RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   int            n;
   C_DBGPH_INFO   *pGphInfo;
   C_DBCONNECTION *pConn;

   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_CreateHistorySystem4Cls(pSub,
                                         HistoryGeomTableRef, HistoryAlfanumTableRef,
                                         CreateClass) == false)
            return GS_BAD;
         pSub = (C_SUB *) pSub->get_next();
      }

      return GS_GOOD;
   }

   // Se la classe non ha i dati in PostgreSQL
   if (gsc_is_class_with_pg_data(pCls) == false) return GS_BAD;

   if (pCls->ptr_info())
   {
      if (!(pConn = pCls->ptr_info()->getDBConnection(OLD))) return GS_BAD;
   }
   else if (pCls->ptr_GphInfo())
   {
      pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
      if (!(pConn = pGphInfo->getDBConnection())) return GS_BAD;   
   }

   // per sapere se uno schema esiste già:
   // SELECT COUNT(*) FROM pg_catalog.pg_namespace WHERE nspname='history';
   Stm = _T("SELECT COUNT(*) FROM pg_catalog.pg_namespace WHERE nspname='");
   Stm += HistorySchema;
   Stm += _T("'");
   if (pConn->ExeCmd(Stm, pRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);
   if ((pRb = gsc_nth(1, ColValues.nth(0))) == NULL || gsc_rb2Int(pRb, &n) == GS_BAD)
      return GS_BAD;

   pConn->BeginTrans();

   if (n == 0)
      // per creare lo schema history:
      if (pConn->CreateSchema(HistorySchema.get_name()) == GS_BAD) { pConn->RollbackTrans(); return GS_BAD; }

   // Se la classe ha la parte grafica
   if (pCls->ptr_GphInfo())
   {
      pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
      if (!(pConn = pGphInfo->getDBConnection())) { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryTable(pConn, pGphInfo->TableRef, HistorySchema,
                                 pGphInfo->ent_key_attrib, HistoryGeomTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
      // siccome la funzione di copia tabella non aggiorna la tabella public.geometry_columns in postgis 1
      if (gsc_pg_insert_geometry_column(pConn, HistoryGeomTableRef, pGphInfo->geom_attrib,
                                        (pGphInfo->geom_dim == GS_3D) ? 3 : 2,
                                        pGphInfo->coordinate_system, pCls->get_type()) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryTrigger(pConn, pGphInfo->TableRef, HistoryGeomTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryQryFunction(pConn, HistoryGeomTableRef, pGphInfo->ent_key_attrib) == GS_BAD)
         return GS_BAD;
   }

   // Se la classe ha la parte alfanumerica
   if (pCls->ptr_info())
   {
      if (!(pConn = pCls->ptr_info()->getDBConnection(OLD))) { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryTable(pConn, pCls->ptr_info()->OldTableRef, HistorySchema,
                                 pCls->ptr_info()->key_attrib, HistoryAlfanumTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryTrigger(pConn, pCls->ptr_info()->OldTableRef, HistoryAlfanumTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_CreateHistoryQryFunction(pConn, HistoryAlfanumTableRef, pCls->ptr_info()->key_attrib) == GS_BAD)
         return GS_BAD;
   }

   if (pConn->CommitTrans() != GS_GOOD) return GS_BAD;

   if (CreateClass)
      if (gsc_CreateHistoryGEOsimClass(pCls, HistoryGeomTableRef, HistoryAlfanumTableRef) == GS_BAD)
         return GS_BAD;

   C_ID newID;
   pCls->ptr_id()->copy(&newID);
   newID.history_enabled = true;
   return pCls->mod_id(&newID);
}


/*************************************************************************/
/*.doc gsc_CreateHistoryGEOsimClass                           <external> */
/*+
  Questa funzione crea una classe di GEOsim collegata ai dati storici.
  Parametri:
  C_CLASS *pCls;                    Classe di geosim
  C_STRING &HistoryGeomTableRef;    Riferimento completo tabella storica x la geometria
  C_STRING &HistoryAlfanumTableRef; Riferimento completo tabella storica x gli alfanumerici

  Restituisce il codice della classe creata altrimenti restituisce 0.
-*/  
/*************************************************************************/
int gsc_CreateHistoryGEOsimClass(C_CLASS *pCls, 
                                C_STRING &HistoryGeomTableRef, C_STRING &HistoryAlfanumTableRef)
{
   C_CLASS  *pHistoryCls;
   int      category;
   C_STRING NewClsName(pCls->get_name());

   if (pCls->get_category() == CAT_EXTERN ||
       pCls->get_category() == CAT_GROUP)
      { GS_ERR_COD = eGSInvClassType; return 0; }

   if (pCls->get_category() == CAT_SUBCLASS) category = CAT_SIMPLEX;
   // Alloca una classe
   if ((pHistoryCls = gsc_alloc_new_class(pCls->get_category())) == NULL)
      return 0;
   pCls->copy(pHistoryCls);

   // cambio ID
   pHistoryCls->ptr_id()->code = 0;
   pHistoryCls->ptr_id()->sub_code = 0;
   pHistoryCls->ptr_id()->Descr = gsc_msg(1046); // "Archivio storico della classe "
   pHistoryCls->ptr_id()->Descr += _T("<");
   pHistoryCls->ptr_id()->Descr += pCls->get_name();
   pHistoryCls->ptr_id()->Descr += _T(">");
   pHistoryCls->ptr_id()->view_enabled = true;
   pHistoryCls->ptr_id()->history_enabled = false;
   // cambio il nome
   NewClsName += _T(" (");
   NewClsName += gsc_msg(1045); // "storico"
   NewClsName += _T(")");
   gsc_strcpy(pHistoryCls->ptr_id()->name, NewClsName.get_name(), MAX_LEN_CLASSNAME);
   gsc_strcpy(pHistoryCls->ptr_id()->usr_cmds, _T(""), MAX_LEN_FIELD);

   if (pHistoryCls->ptr_GphInfo())
   {
      C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pHistoryCls->ptr_GphInfo();
      
      pGphInfo->LinkedTable = true;
      pGphInfo->TableRef = HistoryGeomTableRef;
      // Aggiungo un parametro di tipo timestamp per indicare che i dati vengono letti
      // da una funzione con parametro (indicato da NULL) di tipo data (indicato da "::timestamp")
      pGphInfo->TableRef += _T("(NULL::timestamp)");
      pGphInfo->LblGroupingTableRef.clear();
      pGphInfo->LblTableRef.clear();
      pGphInfo->pGeomConn = NULL;
   }

   if (pHistoryCls->ptr_info())
   {
      C_INFO *pInfo = pHistoryCls->ptr_info();

      pInfo->OldTableRef = HistoryAlfanumTableRef;
      // Aggiungo un parametro di tipo timestamp per indicare che i dati vengono letti
      // da una funzione con parametro (indicato da NULL) di tipo data (indicato da "::timestamp")
      pInfo->OldTableRef += _T("(NULL::timestamp)");
      pInfo->TempTableRef.clear();
      pInfo->LinkedTable = true;
      pInfo->pOldTabConn = NULL;
      pInfo->pTempTabConn = NULL;

      if (pCls->get_type() != TYPE_TEXT)
      {
         // Setto tutti gli attributi invisibili
         C_ATTRIB *pAttrib = (C_ATTRIB *) pHistoryCls->ptr_attrib_list()->get_head();
         while (pAttrib)
         {
            pAttrib->vis = GS_BAD;
            pAttrib = (C_ATTRIB *) pAttrib->get_next();
         }
      }
   }

   return pHistoryCls->create(); // restituisce il codice classe
}


/*************************************************************************/
/*.doc gs_CreateHistorySystem                                 <external> */
/*+
  Questa funzione crea il sistema di storicizzazione per le classi di GEOsim.
  I parametri LISP sono:
  (<prj>[<Crea classi>[<cls list>])
  <prj>         = codice progetto
  <Crea classi> = Flag per la creazione delle classi di GEOsim collegate alla vista
                  della tabelle storiche (default T)
  <cls list>    = lista dei codici delle classi; se = nil o non specificato significa tutte le classi
                  (default = nil)

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_CreateHistorySystem(void)
{
   presbuf    arg;
   int        prj, cls = 0;
   C_INT_LIST ClsCodeList;
   bool       CreateClass = true;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // codice progetto
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &prj) == GS_BAD) return RTERROR;
   
   // Flag di creazione classi (opzionale)
   if ((arg = arg->rbnext))
   {
      if (gsc_rb2Bool(arg, &CreateClass) == GS_BAD) return RTERROR;
      // Lista dei codici delle classi (opzionale)
      if ((arg = arg->rbnext))
         if (arg->restype != RTNIL)
            if (ClsCodeList.from_rb(arg) == GS_BAD) return RTERROR;
   }

   if (gsc_CreateHistorySystem(prj, CreateClass, ClsCodeList) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_CreateHistorySystem                                <external> */
/*+
  Questa funzione crea il sistema di storicizzazione per le classi di GEOsim.
  Parametri:
  int prj;                    Codice progetto
  bool CreateClass;           Flag di creazione classi di GEOsim collegate alla vista 
                              della tabella storica
  C_INT_LIST &ClsCodeList;    Lista dei codici delle classi, se la lista è vuota
                              significa tutte le classi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_CreateHistorySystem(int prj, bool CreateClass, C_INT_LIST &ClsCodeList)
{
   C_CLASS  *pCls;
   C_STRING HistoryGeomTableRef, HistoryAlfanumTableRef;

   if (gsc_superuser() == GS_BAD)
	   {GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }  // "Check se superuser" 

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (gsc_CreateHistorySystem4Cls(pCls, 
                                         HistoryGeomTableRef, HistoryAlfanumTableRef,
                                         CreateClass) == GS_BAD)
            return GS_BAD;

         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         if (gsc_CreateHistorySystem4Cls(pCls, 
                                         HistoryGeomTableRef, HistoryAlfanumTableRef,
                                         CreateClass) == GS_BAD)
            return GS_BAD;

         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_DisableHistorySystem4Cls                            <external> */
/*+
  Questa funzione disabilita il sistema di storicizzazione cancellando 
  i trigger per memorizzare lo storicizzazione dei dati di una classe di GEOsim 
  (solo geometria e alfanumerici senza etichette).
  Parametri:
  C_CLASS *pCls;                    Classe di geosim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_DisableHistorySystem4Cls(C_CLASS *pCls)
{
   C_STRING       HistorySchema(_T("history"));
   C_DBGPH_INFO   *pGphInfo;
   C_DBCONNECTION *pConn;

   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_DisableHistorySystem4Cls(pSub) == false) return GS_BAD;
         pSub = (C_SUB *) pSub->get_next();
      }

      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.history_enabled = false;
      return pCls->mod_id(&newID);
   }

   // Se la classe non ha i dati in PostgreSQL
   if (gsc_is_class_with_pg_data(pCls) == false) return GS_BAD;

   // Se la classe ha la parte grafica
   if (pCls->ptr_GphInfo())
   {
      pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
      if (!(pConn = pGphInfo->getDBConnection())) { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_DropHistoryTrigger(pConn, pGphInfo->TableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
   }

   // Se la classe ha la parte alfanumerica
   if (pCls->ptr_info())
   {
      if (!(pConn = pCls->ptr_info()->getDBConnection(OLD))) { pConn->RollbackTrans(); return GS_BAD; }
      if (gsc_DropHistoryTrigger(pConn, pCls->ptr_info()->OldTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
   }

   C_ID newID;
   pCls->ptr_id()->copy(&newID);
   newID.history_enabled = false;
   return pCls->mod_id(&newID);
}


/*************************************************************************/
/*.doc gsc_DisableHistorySystem                               <external> */
/*+
  Questa funzione disabilita il sistema di storicizzazione cancellando 
  i trigger per memorizzare lo storicizzazione dei dati di una classe di GEOsim 
  (solo geometria e alfanumerici senza etichette).
  Parametri:
  int prj;                 Codice progetto
  C_INT_LIST &ClsCodeList; Lista dei codici delle classi, se la lista è vuota
                           significa tutte le classi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_DisableHistorySystem(int prj, C_INT_LIST &ClsCodeList)
{
   C_CLASS  *pCls;
   C_STRING HistoryGeomTableRef, HistoryAlfanumTableRef;

   if (gsc_superuser() == GS_BAD)
	   {GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }  // "Check se superuser" 

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (gsc_DisableHistorySystem4Cls(pCls) == GS_BAD) return GS_BAD;

         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         if (gsc_DisableHistorySystem4Cls(pCls) == GS_BAD) return GS_BAD;

         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gs_DisableHistorySystem                                <external> */
/*+
  Questa funzione disabilita il sistema di storicizzazione per le classi di GEOsim.
  I parametri LISP sono:
  (<prj>[<cls list>])
  <cls list> = lista dei codici delle classi; se = nil o non specificato significa tutte le classi

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_DisableHistorySystem(void)
{
   int        prj, cls = 0;
   C_INT_LIST ClsCodeList;
   presbuf arg;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // codice progetto
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &prj) == GS_BAD) return RTERROR;
   
   // Lista dei codici delle classi (opzionale)
   if ((arg = arg->rbnext))
      if (arg->restype != RTNIL)
         if (ClsCodeList.from_rb(arg) == GS_BAD) return RTERROR;

   if (gsc_DisableHistorySystem(prj, ClsCodeList) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*************************************************************************/
/*.doc gsc_EnableHistorySystem4Cls                            <external> */
/*+
  Questa funzione abilita il sistema di storicizzazione creando 
  i trigger per memorizzare la storicizzazione dei dati di una classe di GEOsim 
  (solo geometria e alfanumerici senza etichette).
  Parametri:
  C_CLASS *pCls;              Classe di geosim

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_EnableHistorySystem4Cls(C_CLASS *pCls)
{
   C_STRING       HistorySchema(_T("history")), Catalog, Schema, Name, HistoryTableRef;
   C_DBGPH_INFO   *pGphInfo;
   C_DBCONNECTION *pConn;

   // Le simulazioni vanno elaborate sottoclasse per sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

      while (pSub)
      {
         if (gsc_EnableHistorySystem4Cls(pSub) == false) return GS_BAD;
         pSub = (C_SUB *) pSub->get_next();
      }

      C_ID newID;
      pCls->ptr_id()->copy(&newID);
      newID.history_enabled = true;
      return pCls->mod_id(&newID);
   }

   // Se la classe non ha i dati in PostgreSQL
   if (gsc_is_class_with_pg_data(pCls) == false) return GS_BAD;

   // Se la classe ha la parte grafica
   if (pCls->ptr_GphInfo())
   {
      pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
      if (!(pConn = pGphInfo->getDBConnection())) { pConn->RollbackTrans(); return GS_BAD; }
      if (pConn->split_FullRefTable(pGphInfo->TableRef, Catalog, Schema, Name) == GS_BAD)
         return GS_BAD;
      Name += _T("_history");
      HistoryTableRef.paste(pConn->get_FullRefTable(Catalog, HistorySchema, Name));
      if (gsc_CreateHistoryTrigger(pConn, pGphInfo->TableRef, HistoryTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
   }

   // Se la classe ha la parte alfanumerica
   if (pCls->ptr_info())
   {
      if (!(pConn = pCls->ptr_info()->getDBConnection(OLD))) { pConn->RollbackTrans(); return GS_BAD; }
      if (pConn->split_FullRefTable(pCls->ptr_info()->OldTableRef, Catalog, Schema, Name) == GS_BAD)
         return GS_BAD;
      Name += _T("_history");
      HistoryTableRef.paste(pConn->get_FullRefTable(Catalog, HistorySchema, Name));
      if (gsc_CreateHistoryTrigger(pConn, pCls->ptr_info()->OldTableRef, HistoryTableRef) == GS_BAD)
         { pConn->RollbackTrans(); return GS_BAD; }
   }

   C_ID newID;
   pCls->ptr_id()->copy(&newID);
   newID.history_enabled = true;
   return pCls->mod_id(&newID);
}


/*************************************************************************/
/*.doc gsc_EnableHistorySystem                                <external> */
/*+
  Questa funzione abilita il sistema di storicizzazione creando 
  i trigger per memorizzare lo storicizzazione dei dati di una classe di GEOsim 
  (solo geometria e alfanumerici senza etichette).
  Parametri:
  int prj;                    Codice progetto
  C_INT_LIST &ClsCodeList;    Lista dei codici delle classi, se la lista è vuota
                              significa tutte le classi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_EnableHistorySystem(int prj, C_INT_LIST &ClsCodeList)
{
   C_CLASS  *pCls;
   C_STRING HistoryGeomTableRef, HistoryAlfanumTableRef;

   if (gsc_superuser() == GS_BAD)
	   {GS_ERR_COD = eGSInvalidLogin; return GS_BAD; }  // "Check se superuser" 

   if (ClsCodeList.get_count() == 0) // tutte le classi del progetto
   {
      C_PROJECT          *pPrj;
      C_SINTH_CLASS_LIST SinthClassList;
      C_SINTH_CLASS      *pSinthClass;

      // Cerca progetto nella lista GEOsimAppl::PROJECTS
      if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
         { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

      if (pPrj->getSinthClassList(SinthClassList) == GS_BAD) return GS_BAD;

      pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_head();
      while (pSinthClass)
      {
         if ((pCls = gsc_find_class(prj, pSinthClass->get_key())) == NULL) return GS_BAD;
         if (gsc_EnableHistorySystem4Cls(pCls) == GS_BAD) return GS_BAD;

         pSinthClass = (C_SINTH_CLASS *) SinthClassList.get_next();
      }
   }
   else
   {
      C_INT *pClsCode = (C_INT *) ClsCodeList.get_head();

      while (pClsCode)
      {
         if ((pCls = gsc_find_class(prj, pClsCode->get_key())) == NULL) return GS_BAD;
         if (gsc_EnableHistorySystem4Cls(pCls) == GS_BAD) return GS_BAD;

         pClsCode = (C_INT *) ClsCodeList.get_next();
      }
   }

   return GS_GOOD;
}


/********************************* ****************************************/
/*.doc gs_EnableHistorySystem                                <external> */
/*+
  Questa funzione abilita il sistema di storicizzazione per le classi di GEOsim.
  I parametri LISP sono:
  (<prj>[<cls list>])
  <prj>         = codice progetto
  <cls list>    = lista dei codici delle classi; se = nil o non specificato significa tutte le classi
                  (default = nil)

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*************************************************************************/
int gs_EnableHistorySystem(void)
{
   presbuf    arg;
   int        prj, cls = 0;
   C_INT_LIST ClsCodeList;

   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // codice progetto
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &prj) == GS_BAD) return RTERROR;

   // Lista dei codici delle classi (opzionale)
   if ((arg = arg->rbnext))
      if (arg->restype != RTNIL)
         if (ClsCodeList.from_rb(arg) == GS_BAD) return RTERROR;

   if (gsc_EnableHistorySystem(prj, ClsCodeList) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*
CREATE OR REPLACE FUNCTION gs_get_position(IN geom geometry, IN position_mode character varying, OUT pos geometry, INOUT rot double precision DEFAULT 0, IN offset1 double precision DEFAULT 0, IN offset2 double precision DEFAULT 0, IN rot_ctrl boolean DEFAULT TRUE)
  RETURNS record AS
$BODY$ 
DECLARE 
   m_length double precision; 
   m_offset_prcnt double precision;
   m_offset_prcnt1 double precision;
   m_pos1 geometry;
   m_x1 double precision;
   m_y1 double precision;
   m_x2 double precision;
   m_y2 double precision;
BEGIN
   pos := NULL;
   CASE geometrytype(geom)
      WHEN 'LINESTRING' THEN
         m_length := LENGTH2D(geom);
         m_offset_prcnt = offset1 / m_length;
         CASE upper(position_mode)
            WHEN 'S' THEN -- Start
               m_offset_prcnt := m_offset_prcnt + 0;
               m_offset_prcnt1 := m_offset_prcnt + 0.001;
            WHEN 'E' THEN -- End
               m_offset_prcnt := 1 - m_offset_prcnt;
               m_offset_prcnt1 := m_offset_prcnt - 0.001;
            WHEN 'M', 'MML'  THEN -- Middle e Middle Max Len
               m_offset_prcnt := m_offset_prcnt + 0.5;
               m_offset_prcnt1 := m_offset_prcnt + 0.001;
         END CASE;

         pos := LINE_INTERPOLATE_POINT(geom, m_offset_prcnt);

         IF m_offset_prcnt1 > 1 THEN
            m_offset_prcnt1 := m_offset_prcnt - 0.001;
         ELSE
            IF m_offset_prcnt1 < 0 THEN
               m_offset_prcnt1 := m_offset_prcnt + 0.001;
            END IF;
         END IF;

         m_pos1 := LINE_INTERPOLATE_POINT(geom, m_offset_prcnt1);

         IF m_offset_prcnt < m_offset_prcnt1 THEN
            m_x1 := ST_X(pos);
            m_y1 := ST_Y(pos);
            m_x2 := ST_X(m_pos1);
            m_y2 := ST_Y(m_pos1);
         ELSE
            m_x1 := ST_X(m_pos1);
            m_y1 := ST_Y(m_pos1);
            m_x2 := ST_X(pos);
            m_y2 := ST_Y(pos);
         END IF;

         IF m_x1 = m_x2 THEN
            IF m_y1 < m_y2 THEN
	       rot := 90;
            ELSE
	       rot := 270;
            END IF;
         ELSE       
            rot := degrees(atan((m_y2 - m_y1) / (m_x2 - m_x1)));
            if m_x1 > m_x2 THEN
               rot := rot + 180;
            END IF;
         END IF;
         
      WHEN 'POINT' THEN
         pos := geom;
         
      WHEN 'POLYGON', 'MULTIPOLYGON' THEN
         pos := centroid(geom);
         rot := 0;

      ELSE
         rot := 0;
   END CASE;

   IF offset2 <> 0 THEN
      m_x1 = ST_X(pos) + cos(radians(rot + 90)) * offset2;
      m_y1 = ST_Y(pos) + sin(radians(rot + 90)) * offset2;
      pos = st_point(m_x1, m_y1);
   END IF;

   -- Controllo sulla rotazione che non deve essre capovolta
   IF rot_ctrl = TRUE AND rot > 90 AND rot < 270 THEN
      rot := rot - 180;
   END IF;

END; 
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;
*/


/*
per avere la vista dei dati in un momento noto:
SELECT * FROM prove.dati_history AS A
WHERE A.ID = (SELECT B.ID FROM prove.dati_history AS B WHERE B.DATE<=timestamp'2011-10-27 15:08:59.071' AND B.GS_ID=A.GS_ID ORDER BY B.DATE DESC LIMIT 1) AND A.operation <> 'D';
*/

/*
CREATE OR REPLACE FUNCTION prove.dati_history(my_date TIMESTAMP)
RETURNS SETOF prove.dati_history
AS $$ 
 SELECT a.*
   FROM prove.dati_history a
  WHERE a.id = (( SELECT b.id
           FROM prove.dati_history b
          WHERE b.date <= $1 AND b.gs_id = a.gs_id
          ORDER BY b.date DESC
         LIMIT 1)) AND a.operation::text <> 'D'::text
$$
LANGUAGE SQL;
*/


