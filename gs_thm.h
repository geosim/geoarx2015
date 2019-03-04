/**********************************************************/
/*   GS_THM.H                                             */
/**********************************************************/


#ifndef _gs_thm_h
#define _gs_thm_h 1

#ifndef _gs_list_h
   #include "gs_list.h" 
#endif

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif


/*********************************************************/
/* TYPEDEFS */
/*********************************************************/


//----------------------------------------------------------------------------//
//    class C_LAYER_SETTINGS                                                  //
// per ogni fattore minimo di zoom verrà associato un file                    //
// contenente i settaggi dei layer (dal quel fattore di zoom in poi)          //
//----------------------------------------------------------------------------//
class C_LAYER_SETTINGS : public C_2STR
{
   friend class C_LAYER_DISPLAY_MODEL;
      
   public :
      DllExport C_LAYER_SETTINGS();
      DllExport virtual ~C_LAYER_SETTINGS();

      DllExport double get_min_scale(void);
      DllExport int set_min_scale(double in);
      DllExport int set_path(const TCHAR *in);
      DllExport TCHAR *get_path(void);

      DllExport int apply(void);
};


//----------------------------------------------------------------------------//
//    class C_LAYER_DISPLAY_MODEL                                             //
// il primo fattore minimo di scala significa -infinito                       //
//----------------------------------------------------------------------------//
class C_LAYER_DISPLAY_MODEL : public C_2STR_LIST
{
   private :
      C_STRING Path;
      C_STRING Descr;

   public :
      DllExport C_LAYER_DISPLAY_MODEL();
      DllExport virtual ~C_LAYER_DISPLAY_MODEL();  // chiama ~C_LIST

      DllExport TCHAR *get_path(void);
      DllExport TCHAR *get_descr(void);
      DllExport int set_descr(const TCHAR *in);

      DllExport void clear();
      DllExport int copy(C_LAYER_DISPLAY_MODEL &out);
      DllExport int save(C_STRING &_Path);
      DllExport int load(C_STRING &_Path);
      DllExport int apply(double ZoomFactor = 0.0);      
      DllExport C_LAYER_SETTINGS *searchByZoomFactor(double ZoomFactor = 0.0);
};


///////////////////////////////////////////////////////////////////////////////
// Colori

//----------------------------------------------------------------------------//
// class C_COLOR                                                              //
//----------------------------------------------------------------------------//
class C_COLOR : public C_NODE
{
   public :
      DllExport C_COLOR();
      DllExport virtual ~C_COLOR();

    // Color Method.
    enum ColorMethodEnum
    {
       ByLayer = 256, 
       ByBlock = 0,
       ByRGBColor,
       ByAutoCADColorIndex,
       Foreground = 7,
       None
    };

    // The Look Up Table (LUT) provides a mapping between ACI and RGB 
    // and contains each ACI's representation in RGB.
    static const BYTE AutoCADLookUpTable[256][3]; 

    DllExport C_COLOR& operator =  (const C_COLOR &color);
    DllExport bool     operator == (const C_COLOR &color);
    DllExport bool     operator != (const C_COLOR &color);

    DllExport void setByLayer(void);
    DllExport void setByBlock(void);
    DllExport void setForeground(void);
    DllExport void setNone(void);
    DllExport ColorMethodEnum getColorMethod(void) const;

    // get/set RGB
    DllExport int getRGB(COLORREF *out) const;
    DllExport int getRed(BYTE *red) const;
    DllExport int getGreen(BYTE *green) const;
    DllExport int getBlue(BYTE *blue) const;
    DllExport int setRGB(COLORREF in);
    DllExport int setRGB(const BYTE red, const BYTE green, const BYTE blue);
    DllExport int setRed(const BYTE red);
    DllExport int setGreen(const BYTE green);
    DllExport int setBlue (const BYTE blue);

    // get/set autocad color [0-256]
    DllExport int getAutoCADColorIndex(int *AutoCADColorIndex) const;
    DllExport int setAutoCADColorIndex(int AutoCADColorIndex);

    // get/set HTML color (#FFFFFF)
    DllExport int getHTMLColor(C_STRING &HTMLColor) const;
    DllExport int setHTMLColor(C_STRING &HTMLColor);

    // get/set Hexadecimal color (FFFFFF)
    DllExport int getHexColor(C_STRING &HexColor) const;
    DllExport int setHexColor(C_STRING &HexColor);

    // get/set decimal color (255 255 255)
    DllExport int getRGBDecColor(C_STRING &RGBDecColor, const TCHAR Sep = _T(',')) const;
    DllExport int setRGBDecColor(C_STRING &RGBDecColor, const TCHAR Sep = _T(','));

    // get/set DXF decimal color (R G B, es 16711680 = FF0000 = 255 rosso, 0 verde, 0 blu)
    DllExport int getRGBDXFColor(COLORREF *RGBDXFColor) const;
    DllExport int setRGBDXFColor(COLORREF RGBDXFColor);

    // get/set by resbuf (RTSHORT = autocad color (0-255), RTSTR = RGB es. "255,255,255")
    DllExport presbuf getResbuf(void) const;
    DllExport int setResbuf(presbuf pRb);

    // get/set by string (autocad color [0-255],RGB "255,255,255")
    DllExport int getString(C_STRING &StrColor) const;
    DllExport int setString(const TCHAR *StrColor);
    DllExport int setString(C_STRING &StrColor);

    DllExport const TCHAR *getColorName(void) const;

    // transformation functions
    DllExport static int RGB2AutoCADColorIndex(COLORREF in, int *out);
    DllExport static int AutoCADColorIndex2RGB(int in, COLORREF *out);

protected:
    union color_value_union
    {
       COLORREF Color; // B G R (es 16711680 = FF0000 = 255 blue, 0 verde, 0 rosso)
       int      AutoCADColorIndex;
    };
    color_value_union mColor_value;
    ColorMethodEnum   mColorMethod;
};

//----------------------------------------------------------------------------//
//    class C_COLOR_LIST                                                      //
//----------------------------------------------------------------------------//
class C_COLOR_LIST : public C_LIST
{
   public :
      DllExport C_COLOR_LIST();
      DllExport virtual ~C_COLOR_LIST(); // chiama ~C_LIST

      int add_tail_color(COLORREF Color);
      int add_tail_color(int AutoCADColorIndex);
      DllExport int getColorsFromTo(C_COLOR &From, C_COLOR &To, int nColors);
};

DllExport int gsc_C_COLOR_to_AcCmColor(const C_COLOR &in, AcCmColor &out);
DllExport int gsc_AcCmColor_to_C_COLOR(const AcCmColor &in, C_COLOR &out);

          int gsc_set_CurrentColor(C_COLOR &color, C_COLOR *old = NULL);
          int gsc_set_color(AcDbEntity *pEnt, C_COLOR &color);
          int gsc_set_color(ads_name ent, C_COLOR &color);
          int gsc_get_color(AcDbEntity *pEnt, C_COLOR &color);
          int gsc_get_color(ads_name ent, C_COLOR &color);
DllExport bool gsc_SetColorDialog(C_COLOR &Color, bool AllowMetaColor,
                                  const C_COLOR &CurLayerColor);
          int gs_SetColorDialog(void);
          int gs_ColorToString(void);
          int gs_StringToColor(void);

///////////////////////////////////////////////////////////////////////////////
// tipilinea
          int     gsc_getCurrentGEOsimLTypeFilePath(C_STRING &Path);
extern    int     gs_getgeosimltype (void);
extern    int     gsc_getGEOsimLType(C_2STR_LIST &lineTypeList);
DllExport int     gsc_validlinetype (const TCHAR *line);
          int     gsc_load_linetype (const TCHAR *line);
          int     gs_ddsellinetype  (void);
DllExport presbuf gsc_ddsellinetype (const TCHAR *current);
          int     gsc_DefineTxtLineType(C_STRING &LineType, C_STRING &Txt);
          int     gs_DefineTxtLineType(void);
          double  gsc_getLTScaleOnTxtLineType(double LineLen);
          int     gsc_set_CurrentLineType(const TCHAR *name, TCHAR *old = NULL);
          int     gsc_getLineTypeId(const TCHAR *LineType, AcDbObjectId &LineTypeId,
                                    AcDbLinetypeTable *pLineTypeTable = NULL);
          int     gsc_set_lineType(AcDbEntity *pEnt, AcDbObjectId &LineTypeId);
          int     gsc_set_lineType(AcDbEntity *pEnt, const TCHAR *LineType);
          int     gsc_set_lineType(ads_name ent, const TCHAR *LineType);
          int     gsc_set_lineType(ads_name ent, AcDbObjectId &LineTypeId);
          int     gsc_get_lineType(AcDbEntity *pEnt, C_STRING &LineType);
          int     gsc_get_lineType(ads_name ent, C_STRING &LineType);
          int     gsc_get_lineType(C_SELSET &ss, C_STRING &LineType);
          int     gsc_save_lineTypes(C_STR_LIST &lineTypeList, C_STRING &PathFileLin,
                                     int MsgToVideo = GS_GOOD);

///////////////////////////////////////////////////////////////////////////////
// riempimenti
          int     gsc_getCurrentGEOsimHatchFilePath(C_STRING &Path, bool ClientVersion);
          int     gs_getgeosimhatch (void);
          int     gsc_getGEOsimHatch(C_2STR_LIST &hatchList);
DllExport int     gsc_validhatch    (const TCHAR *hatch);
          int     gsc_printGEOsimHatchList(void);
          int     gs_ddselhatch(void);
          presbuf gsc_ddselhatch(const TCHAR *current);
          int     gsc_set_CurrentHatch(const TCHAR *name, TCHAR *old = NULL);
          int     gsc_set_CurrentHatchRotation(const double value, double *old = NULL);
          int     gsc_set_CurrentHatchScale(const double value, double *old = NULL);
          int     gsc_set_hatch(ads_name ent, const TCHAR *hatch = NULL,
                                double *Rot = NULL, double *Scale = NULL,
                                C_COLOR *Color = NULL, const TCHAR *Layer = NULL);
          int     gsc_set_hatch(AcDbEntity *pEnt, const TCHAR *hatch = NULL,
                                double *Rot = NULL, double *Scale = NULL,
                                C_COLOR *Color = NULL, const TCHAR *Layer = NULL);
          int     gsc_get_ASCIIFromHatch(AcDbHatch *pHatch, C_STRING &ASCII);
          int     gsc_get_ASCIIFromHatch(AcDbEntity *pEnt, C_STRING &ASCII);
          int     gsc_get_ASCIIFromHatch(ads_name ent, C_STRING &ASCII);
          int     gsc_get_ASCIIFromHatch(C_SELSET &ss, C_STRING &ASCII);
          int     gsc_save_hatches(C_2STR_LIST &HatchDescrList, C_STRING &PathFilePat, 
                                   int MsgToVideo = GS_GOOD);
          int gsc_getInfoHatchSS(C_SELSET &SelSet, TCHAR *Hatch=NULL, double *Scale=NULL, 
                                 double *Rotation=NULL, TCHAR *Layer=NULL, C_COLOR *Color = NULL);
          int gsc_getInfoHatchSS(ads_name SelSet, TCHAR *Hatch=NULL, double *Scale=NULL, 
                                 double *Rotation=NULL, TCHAR *Layer=NULL, C_COLOR *Color = NULL);
          int gsc_getInfoHatch(ads_name ent, TCHAR *Hatch=NULL, double *Scale=NULL, 
                               double *Rotation=NULL, TCHAR *Layer=NULL, C_COLOR *Color = NULL);
          int gsc_getInfoHatch(AcDbEntity *pEnt, TCHAR *Hatch=NULL, double *Scale=NULL, 
                               double *Rotation=NULL, TCHAR *Layer=NULL, C_COLOR *Color = NULL);


///////////////////////////////////////////////////////////////////////////////
// blocchi
          int     gs_getGEOsimBlockList(void);
          int     gsc_getGEOsimBlockList(C_STR_LIST &BlockNameList);
          int     gs_getBlockList(void);
          int     gsc_getBlockList(C_STR_LIST &BlockNameList, const TCHAR *DwgPath = NULL);
          int     gs_getBlockListWithAttributes(void);
          int     gsc_getBlockListWithAttributes(C_STR_LIST &BlockNameList);
          int gsc_isBlockWithAttributes(const TCHAR *block, AcDbBlockTableRecord *pBlockTableRec = NULL);
          presbuf gsc_getBlockAttributeDefs(const TCHAR *block);
          int     gsc_validblock    (const TCHAR *block);
          int     gsc_validrefblk   (TCHAR **file_ref_block, const TCHAR *ref_block);

          int     gs_uiselblock(void);
          presbuf gsc_uiSelBlock(const TCHAR *current, const TCHAR *DWGFile = NULL);
          HANDLE  gsc_extractDIBBlock(const TCHAR *BlockName, const TCHAR *DWGFile = NULL);
          HBITMAP gsc_extractBMPBlock(const TCHAR *BlockName, HDC hdc, 
                                      const TCHAR *DWGFile = NULL);

          DllExport int gsc_explode(ads_name ent, bool Recursive = false, bool TryAcadCmd = true);
          int     gsc_explode(AcDbEntity *pEntity, bool Recursive = false, 
							         AcDbBlockTableRecord *pBlockTableRecord = NULL,
                              AcDbVoidPtrArray *pExplodedSet = NULL);
          int     gsc_explodeNoGraph(AcDbEntity *pEntity, AcDbVoidPtrArray &ExplodedSet, bool Recursive = false);

          int     gsc_getBlockId(const TCHAR *BlockName, AcDbObjectId &BlockId,
                                 AcDbBlockTable *pBlockTable = NULL);
          int     gsc_set_block(ads_name ent, const TCHAR *block);
          int     gsc_set_block(ads_name ent, AcDbObjectId &BlockId);
          int     gsc_set_block(AcDbEntity *pEnt, AcDbObjectId &BlockId);
          int     gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName,
                                        bool PropValue);
          int     gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, 
                                        const TCHAR *PropValue);
          int     gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, 
                                        double PropValue);
          int     gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, 
                                        ads_point PropValue);
          int     gsc_insert_dwg(const TCHAR *DwgPath, const TCHAR *BlockName = NULL);
          int     gsc_save_blocks(C_STR_LIST &BlockNameList, C_STRING &OutDwg, int MsgToVideo = GS_GOOD);

///////////////////////////////////////////////////////////////////////////////
// stili testo
extern    int     gs_getgeosimstyle (void);
extern    presbuf gsc_getgeosimstyle(void);
DllExport int     gsc_validtextstyle(const TCHAR *style);
          int     gs_ddseltextstyles(void);
          presbuf gsc_ddseltextstyles(const TCHAR *current);
          int     gsc_load_textstyle(const TCHAR *style);
          int     gsc_set_CurrentTextStyle (const TCHAR *name, TCHAR *old = NULL);
          double  gsc_get_CurrentTextSize(void);
          int     gsc_get_charact_textstyle(const TCHAR *style, double *FixedHText = NULL,
                                            double *Width = NULL, double *Angle = NULL,
                                            C_STRING *FontName = NULL);
          int     gsc_getTextStyleId(const TCHAR *StyleName, AcDbObjectId &StyleId, 
                                     AcDbTextStyleTable *pStyleTable = NULL);
          int     gsc_set_textStyle(ads_name ent, const TCHAR *TextStyle);
          int     gsc_set_textStyle(ads_name ent, AcDbObjectId &textStyleId);
          int     gsc_set_textStyle(AcDbEntity *pEnt, AcDbObjectId &textStyleId);

///////////////////////////////////////////////////////////////////////////////
// sistemi coordinate
DllExport int     gsc_validcoord(const TCHAR *coord);
          int     gs_validcoord(void);
DllExport int     gsc_get_category_coord(const TCHAR *coord, C_STRING &Category);
extern    int     gsc_getgeosimcoord(C_STR_LIST &list);
extern    int     gs_getgeosimcoord(void);
extern    int     printlistcoord(void);   
          int     gsc_projsetwscode(TCHAR *coordinate);
          int     gsc_get_srid(const TCHAR *srid_in, GSSRIDTypeEnum SRIDType_in,
                               GSSRIDTypeEnum SRIDType_out, C_STRING &srid_out, GSSRIDUnitEnum *unit_out);
          int     gsc_get_srid(C_STRING &srid_in, GSSRIDTypeEnum SRIDType_in,
                               GSSRIDTypeEnum SRIDType_out, C_STRING &srid_out, GSSRIDUnitEnum *unit_out);
          int     gsc_getCoordConv(ads_point pt_src, TCHAR *SrcCoordSystem,
                                   ads_point pt_dst, TCHAR *DstCoordSystem);

///////////////////////////////////////////////////////////////////////////////
// piani



//----------------------------------------------------------------------------//
// class C_LAYER_MODEL                                                        //
//----------------------------------------------------------------------------//
class C_LAYER_MODEL : public C_STR
{
   friend class C_LAYER_MODEL_LIST;
      
   protected :  
      C_COLOR          Color;
      C_STRING         LineType;
      AcDb::LineWeight LineWeight;
      C_STRING         PlotStyle;
      C_STRING         Description;
      double           TransparencyPercent;

   public :
      DllExport C_LAYER_MODEL();
      DllExport virtual ~C_LAYER_MODEL();

      int set_color(C_COLOR &in);
      void get_color(C_COLOR &out);
      int set_linetype(const TCHAR *in);
      TCHAR *get_linetype(void);
      int set_lineweight(AcDb::LineWeight in);
      AcDb::LineWeight get_lineweight(void);
      int set_plotstyle(const TCHAR *in);
      TCHAR *get_plotstyle(void);
      int set_description(const TCHAR *in);
      TCHAR *get_description(void);
      int set_TransparencyPercent(double percent);
      double get_TransparencyPercent(void);

      int from_rb(C_RB_LIST &List);
      presbuf to_rb(void);
      int to_LayerTableRecord(AcDbLayerTableRecord &LayerRec);
      int from_LayerTableRecord(const TCHAR *layer);
      int from_LayerTableRecord(AcDbLayerTableRecord &LayerRec);
};


//----------------------------------------------------------------------------//
// class C_LAYER_MODEL_LIST                                                   //
//----------------------------------------------------------------------------//
class C_LAYER_MODEL_LIST : public C_STR_LIST
{
   public :
      DllExport C_LAYER_MODEL_LIST();
      DllExport virtual ~C_LAYER_MODEL_LIST();  // chiama ~C_LIST

      presbuf C_LAYER_MODEL_LIST::to_rb(void);
};

          int     gsc_getActualLayerNameList(C_STR_LIST &LayerNameList);
DllExport int     gsc_validlayer(const TCHAR *layer);
          int     gsc_is_lockedLayer   (const TCHAR *layer);
          int     gsc_is_offLayer      (const TCHAR *layer);
          int     gsc_is_freezeLayer   (const TCHAR *layer);
          int     gsc_save_layer_status(const TCHAR *FileName);
DllExport int     gsc_load_layer_status(const TCHAR *FileName);
          int     gs_crea_layer(void);
DllExport int     gsc_crea_layer(const TCHAR *name, AcDbLayerTable *pTable = NULL, 
                                 bool UseLayerModel = true);
DllExport int     gsc_get_tmp_layer(const TCHAR *prefix, C_STRING &Layer);
          int     gs_set_CurrentLayer(void);
DllExport int     gsc_set_CurrentLayer(const TCHAR *name, TCHAR *old = NULL);
          int     gs_load_layer_status(void);
          int     gs_save_layer_status(void);
          int     gsc_get_charact_layer(const TCHAR *layer, TCHAR LineType[MAX_LEN_LINETYPENAME], int *color = NULL,
                              int *Flag = NULL, int *LayerOff = NULL);
          int     gsc_get_charact_layer(const TCHAR *layer, TCHAR LineType[MAX_LEN_LINETYPENAME], 
                                        C_COLOR *color = NULL, bool *isFrozen = NULL,
                                        bool *isLocked = NULL, bool *isOff = NULL, 
                                        double *Transparency = NULL);
DllExport int     gsc_set_charact_layer(const TCHAR *layer, const TCHAR *LineType, C_COLOR *color = NULL,
                                        bool *isFrozen = NULL, bool *isLocked = NULL, bool *isOff = NULL, 
                                        double *Transparency = NULL, bool *ToRegen = NULL);
          int     gs_ddsellayers(void);
DllExport presbuf gsc_ddsellayers(TCHAR *current);
DllExport presbuf gsc_ddsellayers(TCHAR *current);
DllExport int     gsc_ddMultiSelectLayers(C_STR_LIST *in_layerList, C_STR_LIST &out_layerList);

DllExport int     gsc_setLayer(ads_name ent, const TCHAR *Layer);
          int     gsc_setLayer(AcDbObjectId &objId, const TCHAR *Layer);
          int     gsc_setLayer(AcDbEntity *pEnt, const TCHAR *Layer);
          int     gsc_getLayerId(const TCHAR *layer, AcDbObjectId &Id, 
                                 AcDbLayerTable *pTable = NULL);
          int     gsc_getLayer(ads_name ent, TCHAR **layer);
DllExport int     gsc_getLayer(ads_name ent, C_STRING &Layer);
          void    gsc_getLayer(AcDbEntity *pEnt, C_STRING &Layer);
          int     gs_getDependeciesOnLayer(void);
          int     gsc_getDependeciesOnLayer(const TCHAR *Layer, C_INT_STR_LIST &ItemList);

DllExport int     gsc_getLayerModelList(int prj, C_LAYER_MODEL_LIST &LayerModelList);
          int     gs_getLayerModelList(void);
          void    gsddLayerModelList(void);
          int     gsc_ddLayerModelList(void);
          int     gsc_getLayerModel(int prj, C_LAYER_MODEL &LayerModel);
DllExport int     gsc_addLayerModel(int prj, C_LAYER_MODEL &LayerModel);
          int     gs_addLayerModel(void);
DllExport int     gsc_setLayerModel(int prj, const TCHAR *ModelName, C_LAYER_MODEL &LayerModel);
          int     gs_setLayerModel(void);
DllExport int     gsc_delLayerModel(int prj, const TCHAR *ModelName);
          int     gs_ddLayerModel(void);
          int     gsc_ddLayerModel(C_LAYER_MODEL &LayerModel);
          int     gsc_ddLayerModel(C_RB_LIST &ModelProperties, bool MultiLayer = false);

///////////////////////////////////////////////////////////////////////////////
// stili quotature
          int gs_getgeosimDimStyle(void);
          int gsc_validDimStyle(const TCHAR *DimStyle);
          int gsc_getgeosimDimStyles(C_STR_LIST &StyleNameList);
          int gsc_set_CurrentDimStyle(C_STRING &DimStyle, C_STRING *old = NULL);
          int gs_ddSelDimStyles(void);
          TCHAR *gsc_ddSelDimStyles(const TCHAR *current);
          int gsc_getDimStyleId(const TCHAR *DimStyleName, AcDbObjectId &DimStyleId,
                                AcDbDimStyleTable *pDimStyleTable = NULL);
          int gsc_set_dimStyle(AcDbEntity *pEnt, AcDbObjectId &dimStyleId);
          int gsc_set_dimStyle(ads_name ent, const TCHAR *DimStyle);
          int gsc_set_dimStyle(ads_name ent, AcDbObjectId &dimStyleId);

///////////////////////////////////////////////////////////////////////////////
// funzioni generiche
          const ACHAR *gsc_getSymbolName(AcDbObjectId objId);
          enum RotationMeasureUnitsEnum gsc_getCurrAcadAUNITS(void);
          int gs_LoadGEOsimThm(void);
          void gsrefreshthm(void);
          int gsc_refresh_thm(void);
DllExport int gsc_LoadThmFromDwg(const TCHAR *DwgPath);
          int gsc_PurgeDwg(const TCHAR *DwgPath);


#endif
