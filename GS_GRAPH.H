/*************************************************************************/
/*   GS_GRAPH.H                                                          */
/*************************************************************************/


#ifndef _gs_graph_h
#define _gs_graph_h 1

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif

#ifndef _gs_class_h
   #include "gs_class.h" 
#endif

#include "dbmain.h" 

#ifndef AD_DBPL_H
   #include "dbpl.h" 
#endif


// INIZIO CLASSE PER INTERROGAZIONE DINAMICA
#ifndef _ACEDINPT_H
   #include "acedinpt.h"
#endif

#ifndef ACDOCMAN_H
   #include "acdocman.h"
#endif

#include "dbMPolygon.h"


class C_DYNAMIC_QRY_ATTR : public AcEdInputPointMonitor
{
public:

	// Constructor / Destructor
	C_DYNAMIC_QRY_ATTR(AcApDocument* pDoc = curDoc(), C_CLS_PUNT_LIST *pClsList = NULL);
	virtual ~C_DYNAMIC_QRY_ATTR();

	virtual bool excludeFromOsnapCalculation(const AcArray<AcDbObjectId>& nestedEntity,
	                                         int gsSelectionMark);
   virtual ACAD_PORT Acad::ErrorStatus monitorInputPoint(
       const AcEdInputPoint& input, 
       AcEdInputPointMonitorResult& output
   );

   // non va a 64 bit
	//virtual Acad::ErrorStatus monitorInputPoint(
 //      bool& bAppendToTooltipStr,
	//    TCHAR*& pAdditionalTooltipString,
	//    AcGiViewportDraw* pDrawContext,
	//    AcApDocument* pDocument,
	//    bool pointComputed,
	//    int history,
	//    const AcGePoint3d& lastPoint,
	//    const AcGePoint3d& rawPoint,
	//    const AcGePoint3d& grippedPoint,
	//    const AcGePoint3d& cartesianSnappedPoint,
	//    const AcGePoint3d& osnappedPoint,
	//    AcDb::OsnapMask osnapMask,
	//    const AcArray<AcDbCustomOsnapMode*>& customOsnapModes,
	//    AcDb::OsnapMask osnapOverrides,
	//    const AcArray<AcDbCustomOsnapMode*>& customOsnapOverrides,
	//    const AcArray<AcDbObjectId>& apertureEntities,
	//    const AcArray< AcDbObjectIdArray,
	//    AcArrayObjectCopyReallocator< AcDbObjectIdArray > >& nestedApertureEntities,
	//    const AcArray<int>& gsSelectionMark,
	//    const AcArray<AcDbObjectId>& keyPointEntities,
	//    const AcArray< AcDbObjectIdArray,
	//    AcArrayObjectCopyReallocator< AcDbObjectIdArray > >& nestedKeyPointEntities,
	//    const AcArray<int>& keyPointGsSelectionMark,
	//    const AcArray<AcGeCurve3d*>& alignmentPaths,
	//    const AcGePoint3d& computedPoint,
	//    const TCHAR* pTooltipString);

private:
	// Pointer to the document this instance belongs to.
	AcApDocument    *m_pDoc;
   C_CLS_PUNT_LIST GridClassList; // Lista delle classi griglia estratte
	C_STRING       m_TooltipString;
};


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


extern C_CLASS *SEL_CLS;   
extern C_CLASS *SEL_EXT;   

extern double     _SCALE, XSCALE, YSCALE, ROTA;                                 
extern ads_point  BASE, GS_POINT_CURSOR;

//-----------------------------------------------------------------------//   

// Funzione per archi
ads_real gsc_getArcLen(ads_real ray, ads_real angle);
ads_real gsc_getArcLen(ads_real ray, ads_real InitAngle, ads_real FinalAngle);
ads_real gsc_getArcAngle(ads_real ray, ads_real len);
void gsc_getArcRayCenter(AcGePoint3d &p1, AcGePoint3d &p2, ads_real angle, 
                         ads_real *radius, AcGePoint3d &center,
                         ads_real *startAngle = NULL, ads_real *endAngle = NULL);
void gsc_getArcRayCenter(AcGePoint2d &p1, AcGePoint2d &p2, ads_real angle, 
                         ads_real *radius, AcGePoint2d &center,
                         ads_real *startAngle = NULL, ads_real *endAngle = NULL);
void gsc_getArcRayCenter(ads_point p1, ads_point p2, ads_real angle, 
                         ads_real *ray, ads_point center,
                         ads_real *startAngle = NULL, ads_real *endAngle = NULL);
void gsc_retMidArc(ads_point pippo, ads_point iniz, ads_point fine, ads_point dato);
void gsc_retMidArc(AcGePoint3d &center, AcGePoint3d &iniz, AcGePoint3d &fine, AcGePoint3d &dato);

void gsc_getMidLine(ads_point pt1, ads_point pt2, ads_point pt);
void gsc_getMidLine(AcGePoint3d &pt1, AcGePoint3d &pt2, AcGePoint3d &pt);
void gsc_getMid2DLine(ads_point pt1, ads_point pt2, ads_point pt);

resbuf *gsc_getMiddlePts(ads_name ent);
DllExport int gsc_getLenParts(ads_name ent, C_REAL_LIST &LenParts);
int gsc_getLenParts(AcDbEntity *pEnt, C_REAL_LIST &LenParts);
DllExport int gsc_getPtOnLinearObj(ads_name ent, ads_real offset, ads_point pt,
                                   ads_real *Angle);
int gsc_getPtOnLinearObj(AcDbEntity *pEnt, ads_real offset, AcGePoint3d &pt, ads_real *Angle);

int gsc_getPtList_BetweenPts(ads_name ent, double DistanceFromStart, double Length, C_POINT_LIST &PtList);
int gsc_getPtList_BetweenPts(AcDbEntity *pEnt, double DistanceFromStart, double Length, C_POINT_LIST &PtList);

DllExport int gsc_getClosestPointTo(ads_name ent, ads_point pt, ads_point ClosestPointTo, 
                                    double *distance = NULL);
int gsc_getClosestPointTo(AcDbObject *pObj, AcGePoint3d &pt, AcGePoint3d &ClosestPointTo,
                          double *distance = NULL);

int gsc_getClosestPointTo(ads_name Ent, ads_name ReferenceEnt,
                          ads_point EntClosestPointTo, ads_point ReferenceEntClosestPointTo,
                          double *distance = NULL);
int gsc_getClosestPointTo(AcDbLine *pObj, AcDbLine *pReferenceObj,
                          AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo,
                          double *distance = NULL);
int gsc_getClosestPointToLineOrArc(AcDbEntity *pObj, AcDbArc *pReferenceObj,
                          AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo,
                          double *distance = NULL);
int gsc_getClosestPointTo(AcDbEntity *pObj, AcDbEntity *pReferenceObj,
                          AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo,
                          double *distance = NULL);
int gsc_getClosestTo(AcDbEntityPtrArray &ObjEntitySet, AcDbEntityPtrArray &ReferenceEntitySet,
                     AcDbEntityPtrArray &ResultEntitySet, int Qty = 1);


DllExport double gsc_getDistAtPoint(ads_name ent, ads_point pt);

double gsc_getLength(AcDbEntity *pEnt);

int gs_EEDload(void);
int gs_EEDsave(void);
int gs_EEDset(void);
int gs_set_linetype (void);
int gs_set_textstyle (void);
int gs_set_elevation (void);
int gs_set_htext (void);
int gs_set_width (void);
int gs_set_thickness (void);
int gs_provadcl(void);
///////////////////////////////////////////

//-----------------------------------------------------------------------//   
// FUNZIONI PER LA GESTIONE DELL'UNDO //
//-----------------------------------------------------------------------//   
int gsc_finto(void);
DllExport void gsc_disableUndoRecording(bool disable);
DllExport int gsc_startTransaction(void);    
DllExport int gsc_endTransaction(void);    
DllExport int gsc_abortTransaction(void);
DllExport int gsc_SetUndoRecording(int State);
int gsc_ClearUndoStack(void);

//-----------------------------------------------------------------------//   
// FUNZIONI PER IL SETTAGGIO DELL'AMBIENTE GRAFICO AUTOCAD //
//-----------------------------------------------------------------------//   


int gsc_set_CurrentElevation(double elev, double *old = NULL);
int gsc_set_width(double width, double *old = NULL);
int gsc_set_CurrentThickness(double thick, double *old = NULL);                                                                                                                                       
int gsc_set_CurrentHText(double htext, double *old = NULL);
int gsc_set_CurrentLTScale(double ltscale, double *old = NULL);

DllExport int gsc_setenv_graph(int cat, int type, C_FAS &NewFAS, C_FAS *OldFAS = NULL);

DllExport int gsc_acedGetStrVar(const TCHAR *var, C_STRING &Value);
DllExport int gsc_set_echo(int echo, int *old = NULL);
int gsc_set_cmddia(int cmddia, int *old = NULL);
int gsc_set_expert(int exp, int *old = NULL);
int gsc_set_env_cmd();
int gsc_reset_env_cmd();
int gsc_set_osmode(int osmode, int *old = NULL);
int gsc_set_CLIPROMPTUPDATE(int new_value, int *old_value = NULL);

          int gsc_get_centroidpoint(AcDbEntity *pEnt, ads_point point);
          int gsc_get_centroidpoint(ads_name ent, ads_point point);
          int gsc_get_centroidpoint(C_SELSET &ss, ads_point point);

          int gsc_get_firstPoint(AcDbEntity *pEnt, ads_point point);
DllExport int gsc_get_firstPoint(ads_name ent, ads_point point);
DllExport int gsc_set_firstPoint(ads_name ent, ads_point point, ads_real *rot = NULL, 
                                 int check_rotz = GS_GOOD);
          int gsc_set_firstPoint(AcDbEntity *pEnt, ads_point point);
DllExport int gsc_get_lastPoint(ads_name ent,ads_point point);
          int gsc_get_lastPoint(AcDbEntity *pEnt, ads_point point);
DllExport int gsc_set_lastPoint(ads_name ent, ads_point point);
          int gsc_set_lastPoint(AcDbEntity *pEnt, ads_point point);

DllExport int gsc_ispline(ads_name ent);
DllExport bool gsc_isLinearEntity(ads_name ent);
bool gsc_isLinearEntity(AcDbEntity *pEnt);
bool gsc_isClosedEntity(ads_name ent); // roby shape
bool gsc_isClosedEntity(AcDbEntity *pEnt);
bool gsc_isPunctualEntity(ads_name ent);
bool gsc_isPunctualEntity(AcDbEntity *pEnt);
int gsc_isText(ads_name ent);
bool gsc_isText(AcDbEntity *pEnt);
int gsc_isPoint(ads_name ent);
DllExport int gsc_isline(ads_name ent);
int gsc_isblock(ads_name ent);
int gsc_iscircle(ads_name ent);
int gsc_ishatch(ads_name ent);
int gsc_isattDef(ads_name ent);
int gsc_ismpolygon(ads_name ent);

int gsc_set_hText(AcDbEntity *pEnt, double htext);
int gsc_set_hText(ads_name ent, double htext);
int gsc_set_text(AcDbEntity *pEnt, const TCHAR *text);
int gsc_set_text(ads_name ent, const TCHAR *text);
int gsc_get_textstyle(ads_name ent, C_STRING &Style, double *StyleHeight);
int gsc_getInfoText(ads_name ent, C_STRING *pText = NULL,
                    C_STRING *pStyleName = NULL, double *pHText = NULL,
                    int *pHorizAlign = NULL, int *pVertAlign = NULL,
                    double *pObliqueAng = NULL, double *pWidthFactor = NULL);
int gsc_getInfoText(AcDbEntity *pEnt, C_STRING *pText = NULL,
                    C_STRING *pStyleName = NULL, double *pHText = NULL,
                    int *pHorizAlign = NULL, int *pVertAlign = NULL,
                    double *pObliqueAng = NULL, double *pWidthFactor = NULL);

int gsc_get_elevation(ads_name ent, double *elev);
int gsc_set_elevation(AcDbEntity *pEnt, double elevation);
int gsc_set_elevation(ads_name ent, double elevation);

int gsc_get_ltScale(ads_name ent, double *scale);
int gsc_set_lineTypeScale(ads_name ent, double ltScale);
int gsc_set_lineTypeScale(AcDbEntity *pEnt, double ltScale);

int gsc_get_thickness(ads_name ent, double *thickness);
int gsc_get_thickness(AcDbEntity *pEnt, double *thickness);
int gsc_set_thickness(ads_name ent, double thickness);
int gsc_set_thickness(AcDbEntity *pEnt, double thickness);

int gsc_get_blockName(ads_name ent, C_STRING &BlockName);

int gsc_get_prompt(ads_name ent, TCHAR **prompt);
int gsc_get_center(ads_name ent);

DllExport int gsc_get_numvertex(ads_name ent);
int gsc_open_pline(ads_name ent);
int gsc_close_pline(AcDbEntity *pEnt);
int gsc_close_pline(ads_name ent);
DllExport int gsc_enthand(ads_name ent, TCHAR *hand, size_t hand_len = MAX_LEN_HANDLE);
          int gsc_enthand(AcDbObjectId &objId, TCHAR *hand, size_t hand_len = MAX_LEN_HANDLE);
          int gsc_enthand(AcDbObject *pObj, TCHAR *hand, size_t hand_len = MAX_LEN_HANDLE);

DllExport int gsc_DeepClone2ModSpace(ads_name ent);
int gsc_DeepClone2ModSpace(C_SELSET &ss);

int gsc_getEntOnXY(ads_name ent, AcDbEntity **pOutEnt);
int gsc_getEntOnXY(AcDbEntity *pInEnt, AcDbEntity **pOutEnt);

int gsc_MoveEntOnXY(AcDbEntity *pEnt, double OffSetX = 0.0, double OffSetY = 0.0,
                    int Dec = -1);

int gsc_ReleaseSubEnts(AcDbEntity *pEnt);

int gsc_addvertex(AcDbEntity *pEnt, ads_point point);
int gsc_addvertex(ads_name ent,ads_point point);
int gsc_delLastVertex(AcDbEntity *pEnt);
int gsc_delLastVertex(ads_name ent);
int gsc_join_point(ads_point first, ads_point second, ads_name ent_out,
                   C_CLASS *pCls = NULL);
int gsc_breakpline(ads_name ent, ads_point point, C_EED *new_EED);
int gsc_graph_type(ads_name ent, TCHAR *type);
int gsc_isClosedPline(ads_name ent);

////////////////////////////////////////////////////////////
int gsc_regapp();
int gsc_regapp(const TCHAR* appl);
////////////////////////////////////////////////////////////

DllExport int gsc_select_entity(ads_name in, long *gs_id, int *num_el,
                                ads_name out, C_CLASS **ppcls);
DllExport int gsc_get_class_list(C_SELSET SelSet, C_CLS_PUNT_LIST &ClassList, int *Sub);
int gsc_ddselect_selset(C_SELSET &ss, C_LONG_BTREE &KeyList, C_CLASS **pSelCls);
int gsc_ddselect_selset(ads_name ss, C_LONG_BTREE &KeyList, C_CLASS **pSelCls);
int gsc_ddMultiSelectClass(C_INT_INT_LIST &ClsSubList, C_INT_INT_LIST &SelClsList);

DllExport int gsc_get_pick_first(C_SELSET &SelSet);
int gsc_clear_pick_first();


// FUNZIONI PER INSERIMENTO QUERY E MODIFICA ENTITA' GEOSIM //
void gsDynamicQueryAttr(void);

int gsc_get_list_selcls(C_CLS_PUNT_LIST *out,int vis);
int gsc_get_connect_point(C_CLASS *p_class, ads_point start, ads_name con_ent,
                          ads_pointp *ins, int *SplitMode = NULL);

// FUNZIONI DCL
int gs_ddselect_class(void);
int gsc_ddselect_class(int mode = NONE, C_CLASS **pSelCls = NULL, int *ConnectCheck = FALSE,
                       C_CLS_PUNT_LIST *pClsListToSel = NULL);
int gsc_ddselect_subnode(C_CONNECT_LIST *con_list, TCHAR *title);
  
// FUNZIONI PER INSERIMENTO ENTITA' GRAFICHE AUTOCAD
AcDbHatch *gsc_create_hatch(AcDbEntityPtrArray Ents, TCHAR *hatch, double scale = 1.0,
                            double rotation = 0.0, C_COLOR *color = NULL, 
                            const TCHAR *layer = NULL);
int gsc_setHatchEnt(ads_name ent, TCHAR *hatch, double scale = 1.0, double rotation = 0.0,
                    C_COLOR *color = NULL, const TCHAR *layer = NULL,
                    AcDbBlockTableRecord *pBlockTableRecord = NULL);
int gsc_setHatchSS(C_SELSET &selset, TCHAR *hatch, double scale = 1.0, double rotation = 0.0,
                   C_COLOR *color = NULL, const TCHAR *layer = NULL,
                   C_SELSET *pResultSS = NULL,
                   AcDbBlockTableRecord *pBlockTableRecord = NULL);
int gsc_setHatchSS(ads_name selset, TCHAR *hatch, double scale = 1.0, double rotation = 0.0,
                   C_COLOR *color = NULL, const TCHAR *layer = NULL,
                   C_SELSET *pResultSS = NULL,
                   AcDbBlockTableRecord *pBlockTableRecord = NULL);
int gsc_UpdToDefHatch(C_SELSET &SelSet, TCHAR *hatch, double scale = 1.0, double rotation = 0.0,
                      C_COLOR *color = NULL, const TCHAR *layer = NULL);

int gsc_insert_pline(ads_point p1, ads_point p2, const TCHAR *Layer = NULL, C_COLOR *color = NULL,
                     const TCHAR *LineType = NULL, double Scale = -1, double Width = -1);
int gsc_insert_pline(C_POINT_LIST &PointList, const TCHAR *Layer = NULL, C_COLOR *color = NULL,
                     const TCHAR *LineType = NULL, double Scale = -1, double Width = -1);
int gsc_insert_pline(ads_point point = NULL, C_CLASS *ClassToCheckOverlap = NULL);

int gsc_insert_text(const TCHAR *Txt, ads_point InsPt, double Height, double Rot = 0.0,
                    const TCHAR *Style = _T("STANDARD"), double Thickness = 0.0,
                    double xScale = 1.0, double OblAng = 0.0);
AcDbText *gsc_create_text(const TCHAR *Txt, ads_point InsPt, double Height = 1.0, 
                          double Rot = 0.0, const AcDbObjectId &StyleId = AcDbObjectId::kNull, 
                          double Thickness = 0.0, double Width = 1.0, double OblAng = 0.0,
                          AcDb::TextHorzMode HorzMode = AcDb::kTextLeft,
                          AcDb::TextVertMode VertMode = AcDb::kTextBase);
int gsc_insert_text(TCHAR *style, ads_point point,  double htext,  double rot, TCHAR *intext,
                    TCHAR *DefaultText = NULL, C_CLASS *ClassToCheckOverlap = NULL);
int gsc_insert_point(ads_point Pt, const TCHAR *Layer = NULL, C_COLOR *Color = NULL,
                     double Thickness = 0.0, AcDbBlockTableRecord *pBlockTableRecord = NULL,
                     AcDbPoint **pResult = NULL);
AcDbPoint *gsc_create_point(ads_point Pt, const TCHAR *Layer = NULL, 
                            C_COLOR *Color = NULL, double Thickness = 0.0);

int gsc_insert_3dFace(ads_point Pt1, ads_point Pt2, ads_point Pt3, ads_point Pt4 = NULL,
                      const TCHAR *Layer = NULL, C_COLOR *Color = NULL,
                      AcDbBlockTableRecord *pBlockTableRecord = NULL);
DllExport int gsc_insert_rectangle(ads_point Pt1, ads_point Pt2,
                                   const TCHAR *Layer = NULL, C_COLOR *Color = NULL,
                                   int Mode = EXTRACTION, AcDbBlockTableRecord *pBlockTableRecord = NULL);
int gsc_insert_lwpline(ads_point Pt1, ads_point Pt2, 
                       const TCHAR *Layer = NULL, C_COLOR *Color = NULL,
                       const TCHAR *LineType = NULL, double Scale = 1.0, double Width = 0,
                       int Mode = EXTRACTION, AcDbBlockTableRecord *pBlockTableRecord = NULL);
int gsc_insert_hatch(ads_point Pt1, ads_point Pt2, const TCHAR *hatch = _T("SOLID"), 
                     const TCHAR *Layer = NULL, double scale = 1.0, double rotation = 0.0,
                     C_COLOR *color = NULL, AcDbBlockTableRecord *pBlockTableRecord = NULL);

AcDbMPolygon *gsc_create_MPolygon(AcDbEntityPtrArray &EntArray);
AcDbMPolygon *gsc_create_MPolygon(C_SELSET &SelSet);
int gs_check_mpolygon_by_topology(void);

int gsc_Hscale(ads_point pp, ads_matrix mat);
int gsc_HYscale(ads_point pp, ads_matrix mat);

DllExport int gsc_get_rotation(ads_name ent, double *rotation);
int gsc_get_rotation(AcDbEntity *pEnt, double *rotation);
int gsc_set_rotation(ads_name ent, double rotation, int check_rot = GS_GOOD,
                     bool AttribRepositioning = true);
int gsc_set_rotation(AcDbEntity *pEnt, double rotation, int check_rot = GS_GOOD,
                     bool AttribRepositioning = true);

DllExport int gsc_get_scale(ads_name ent, double *XScale,
                            double *YScale = NULL, double *ZScale = NULL);
int gsc_get_scale(AcDbEntity *pEnt, double *XScale,
                  double *YScale = NULL, double *ZScale = NULL);
int gsc_set_scale(ads_name ent, double XScale, double YScale, double ZScale,
                  bool AbsScale = true, bool AttribRepositioning = true);
int gsc_set_scale(AcDbEntity *pEnt, double XScale, double YScale, double ZScale,
                  bool AbsScale = true, bool AttribRepositioning = true);

int gsc_set_width(ads_name ent, double width);
int gsc_set_width(AcDbEntity *pEnt, double width);
int gsc_get_width(ads_name ent, double *width);
int gsc_get_width(AcDbEntity *pEnt, double *width);

int gsc_reverseCurve(ads_name ent);

DllExport int gsc_insert_block(const TCHAR *BlkName, ads_point InsPt, double xScale = 1.0,
                               double yScale = 1.0, double Rot = 0.0);
DllExport int gsc_insert_block(TCHAR *name, ads_point point, double sca, double rot,
                               C_CLASS *ClassToCheckOverlap = NULL);
AcDbBlockReference *gsc_create_block(AcDbObjectId &BlockId, ads_point InsPt, double xScale = 1.0,
                                     double yScale = 1.0, double Rot = 0.0);

AcDbAttribute *gsc_create_attribute(const TCHAR *Txt, const TCHAR *AttrName,
                                    ads_point InsPt, double Height, double Rot = 0.0,
                                    const AcDbObjectId &StyleId = AcDbObjectId::kNull,
                                    double Thickness = 0.0,
                                    double Width = 1.0, double OblAng = 0.0,
                                    AcDb::TextHorzMode HorzMode = AcDb::kTextLeft,
                                    AcDb::TextVertMode VertMode = AcDb::kTextBase);

int gsc_XYscale(ads_point pp, ads_matrix mat);
int gsc_Zrotate(ads_point pp, ads_matrix mat);

int gsc_insert_spaghetti(C_ID *id);

// VARIAZIONE SCALA E ROTAZIONE //
int gsc_ddscale_block(ads_name ent, ads_real sca, ads_real *new_sca);
int gsc_ddrotate_block(ads_name ent, ads_real rot, ads_real *new_rot);


// FUNZIONI GENERICHE per OPERAZIONI sulle MATRICI(di ROTAZIONE) //
void gsc_mat_ident(ads_matrix matrix);
void gsc_subvec(ads_point ap, ads_point bp, ads_point dp);
ads_real gsc_fabsv(ads_point ap);

DllExport ads_real gsc_dist(ads_point p1, ads_point p2);
ads_real gsc_dist(AcGePoint3d &p1, AcGePoint3d &p2);
ads_real gsc_dist(AcGePoint2d &p1, AcGePoint2d &p2);
ads_real gsc_dist(C_POINT *p1, AcGePoint3d &p2);
ads_real gsc_dist(ads_point p1, AcGePoint3d &p2);
DllExport ads_real gsc_dist2d(ads_point p1, ads_point p2);

ads_real gsc_angle(AcGePoint3d &p1, AcGePoint3d &p2);
ads_real gsc_angle(AcGePoint2d &p1, AcGePoint2d &p2);

void     gsc_polar(AcGePoint3d &pt1, double angle, double dist, AcGePoint3d &p2);
void     gsc_polar(AcGePoint2d &pt1, double angle, double dist, AcGePoint2d &p2);

extern int gsc_mat_ixlate(ads_point vec, ads_matrix result);
extern int gsc_mat_iscale(ads_point vec, ads_matrix result);
extern int gsc_mat_irotate(ads_point vec, ads_matrix result);
void gsc_mat_rot(ads_real angle, int axis, ads_matrix m);
void gsc_mat_scale(ads_real xscale, ads_real yscale, ads_real zscale, ads_matrix m);
void gsc_mat_x_pt(ads_matrix mat, ads_point pin, ads_point pout);
void gsc_mat_x_vec(ads_matrix mat, ads_point pin, ads_point pout);
void gsc_mat_x_mat(ads_matrix mata, ads_matrix matb, ads_matrix matout);

DllExport int gsc_upd_initfinal_node(C_ATTRIB_LIST *p_attrib_list, C_RB_LIST &ColValues,
                                     long initial_node, long final_node);

void gsdummyalign(void);
void gsddclassalign(void);
DllExport int gsc_getKeyValue(ads_name ent, long *KeyValue);
DllExport int gsc_reset_SAVE_SS(void);

//-----------------------------------------------------------------------//   
// FUNZIONI GENERICHE //
//-----------------------------------------------------------------------//   

DllExport int gsc_IsErasedEnt(ads_name ent);
DllExport int gsc_EraseEnt(ads_name ent);
          int gsc_EraseEnt(AcDbObjectId &objId);
          int gsc_EraseEnt(AcDbObjectIdArray &objIds);
DllExport int gsc_UnEraseEnt(ads_name ent);
          int gsc_UnEraseEnt(AcDbObjectId objId);

  
DllExport presbuf gsc_get_graphical_data(ads_name ent, TCHAR *what, const TCHAR *type = NULL,
                                         TCHAR *pDestSRID = NULL);

DllExport TCHAR *gsc_checkValue(C_CLASS *pCls, presbuf pRbAttrib, C_ATTRIB *pAttrib, 
                               TCHAR *Value);

DllExport int gsc_get_window(ads_name selset, ads_point corner1, ads_point corner2);
int         gsc_get_ent_window(AcDbEntity *pEnt, ads_point corner1, ads_point corner2);
DllExport int gsc_get_ent_window(ads_name ent, ads_point corner1, ads_point corner2);
int         gsc_get_window(AcDbBlockTableRecordIterator *pIterator, ads_point corner1, ads_point corner2);
int         gs_get_window(void);

int gsc_get_txt_rectangle(ads_name ent, ads_point p1, ads_point p2,
                          ads_point p3, ads_point p4);
int gsc_get_block_rectangle(ads_name ent, ads_point p1, ads_point p2,
                            ads_point p3, ads_point p4);

int gsc_getPtNoOverlappedEnt(ads_name ent, const TCHAR *NoOverlapMode, 
                             ads_point RefPt, ads_point NewPt);

int         gsc_get_AdeQryPolyFence_window(C_RB_LIST &AdeQryCond, ads_point corner1, 
                                           ads_point corner2);
DllExport int gsc_IsInternalEnt(AcDbEntity *pContainerEnt, AcDbEntity *pEnt, 
                                int Mode = INSIDE);
DllExport int gsc_IsInternalEnt(C_SELSET &SelSet, ads_name ent, int Mode = INSIDE,
                                int DeepOrder = 1);
int gsc_IsInternalEnt(C_SELSET &SelSet, AcDbEntity *pEnt, int Mode = INSIDE,
                      int DeepOrder = 1);

int gsc_IsInternalPt(AcDbEntity *pContainerEnt, ads_point point);

int gsc_getBufferOnArc(AcGePoint3d Pt1, AcGePoint3d Pt2, double Bulge, double OffSet,
                       AcDbPolyline **pExtBuffer, AcDbPolyline **pIntBuffer);
int gsc_getBufferOnLine(AcGePoint3d Pt1, AcGePoint3d Pt2,
                        double OffSet, AcDbPolyline **pBuffer);


presbuf     gsc_setVarForCommandSpeed(void);
int         gsc_setVarForCommandNormal(presbuf OldValues);

DllExport int gsc_getCurrentScreenCoordinate(ads_point pt1, ads_point pt2);

int         gsc_conversion_from_line2polyline(ads_name line, ads_name new_entity);

int         gsc_ssadd(ads_name ss1, ads_name ss2, ads_name ss3);
int         gsc_select_pick(ads_name sss);
int         gsc_clear_pick_first(void);

int         gs_get_group_by_member(void);


int gs_AcadObjToPline(void);
int gsc_AcadObjToPline(ads_name eent, ads_name enew);
int gsc_AcadObjToPline(ads_name ent);
int gs_AcadObjToBlock(void);
int gsc_AcadObjToBlock(ads_name ent, const TCHAR *BlkName);
int gs_AcadObjToText(void);
int gsc_AcadObjToText(ads_name ent);

int gsc_getOwnerAdsName(ads_name Ent, ads_name &OwnerEnt);

DllExport int gsc_modifyEntToFas(C_SELSET &SelSet, C_FAS *pFas, long Flag, bool AbsScale = true);
DllExport int gsc_modifyEntToFas(ads_name Group, C_FAS *pFas, long Flag, bool AbsScale = true);
DllExport int gsc_SetBitForChangeFas(int Category, int Type, long *BitForFAS);

DllExport void gsc_zoom(ads_point min_2d, ads_point max_2d,
                        ads_real min_dim_x = 0, ads_real min_dim_y = 0);
void gsc_zoom(AcGePoint2d min_2d, AcGePoint2d max_2d, 
              ads_real min_dim_x = 0, ads_real min_dim_y = 0);
void gsc_zoom_extents(ads_real min_dim_x = 0, ads_real min_dim_y = 0);

int gs_GrDrawCross(void);
int gsc_GrDrawCross(ads_point pt);

Acad::ErrorStatus getPointAtDist(const AcGeCurve2d *pGeCurve, double dist, AcGePoint2d& point);

bool gsc_isPaperSpaceCurrent(void);

int gs_insert_ent(void);


////////////////////////////////////////////////////////////////////
// test
////////////////////////////////////////////////////////////////////
int gs_prova();


#endif
