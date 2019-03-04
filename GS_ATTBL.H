/*************************************************************************/
/*   GS_ATTBL.H                                                          */
/*************************************************************************/


#ifndef _gs_attbl_h
#define _gs_attbl_h 1
                                                                           

//----------------------------------------------------------------------------//
//    class C_ATTRIB_FAS usata per settare le caratteristiche grafiche di un  //
//                       attributo di un blocco DA.                           //
//----------------------------------------------------------------------------//
class C_ATTRIB_FAS : public C_STR
{
   friend class C_ATTRIB_FAS_LIST;

   public :
      C_ATTRIB_FAS();
      ~C_ATTRIB_FAS();

      C_FAS     fas;

      C_FAS     *ptr_fas();
      ads_point pt;           // punto di inserimento attributo
      short     GeneratFlag;  // Flag di generazione testo
      short     HorzAlign;    // Allineamento orizzontale
      short     VertAlign;    // Allineamento verticale
      int       visib;        // flag di visibilità (1 = invisibile, 0 = visibile)
      double    WidthFactor;  // Fattore larghezza testo
};


//----------------------------------------------------------------------------//
//    class C_ATTRIB_FAS_LIST usata per settare le caratteristiche grafiche   //
//                            degli attributi di un blocco DA.                //
//----------------------------------------------------------------------------//
class C_ATTRIB_FAS_LIST : public C_STR_LIST
{
   public :
      C_ATTRIB_FAS_LIST() : C_STR_LIST() {};
      virtual ~C_ATTRIB_FAS_LIST() {}; // chiama ~C_LIST
};


//----------------------------------------------------------------------------//
//    class C_ATTRIB_BLOCK usata per gestire i blocchi di attributo visibili  //
//----------------------------------------------------------------------------//
class C_ATTRIB_BLOCK
{
   protected :
      ads_point  insert_point;
      TCHAR		  layer[MAX_LEN_LAYERNAME];
      ads_real	  rotation; // in gradi   
      ads_real	  scale;    
      ads_real	  h_text;   // altezza testo degli attributi
      TCHAR      style[MAX_LEN_TEXTSTYLENAME]; // stile testo degli attributi
      C_COLOR    color;    // colore, per ora non considerato: 
									// il colore viene derivato dal layer del blocco
      ads_name   ent_name;

      int create(C_RB_LIST &ColValues, ads_point InsPoint,
                 ads_real Rot, presbuf pDAParams);

   public :
DllExport C_ATTRIB_BLOCK();
DllExport C_ATTRIB_BLOCK(ads_name ent);
DllExport ~C_ATTRIB_BLOCK();

      int  get_insert_point(ads_point out);
DllExport int  set_insert_point(ads_point in);
      TCHAR *get_layer();
DllExport int set_layer(const TCHAR *in);
      ads_real get_rotation();
DllExport int set_rotation(ads_real in);
      ads_real get_scale();
DllExport int set_scale(ads_real in);
      ads_real get_h_text();
      int  set_h_text(ads_real in);
      TCHAR *get_text_style();
      int  set_text_style(TCHAR *in);
      void get_color(C_COLOR &out);
DllExport int set_color(C_COLOR &in);
      //int  get_vis() { return vis; }
      int  get_ent_name(ads_name entity) { ads_name_set(ent_name, entity); return GS_GOOD; }
      int  set_ent_name(ads_name in) { ads_name_set(in, ent_name); return GS_GOOD; }      
      int  apply_visib_set(C_STR_LIST &InvAttribList);     
      
DllExport int insert(C_RB_LIST &ColValues, int cls, int sub, long Key,
                     TCHAR *file_ref_block = NULL, TCHAR *ref_block = NULL,
                     int modo = MANUAL, int num_el = 2);

DllExport int setfas_attrib(TCHAR *file_ref_block, TCHAR *ref_block);
      int setfas_attrib(C_ATTRIB_FAS_LIST *pFasList, TCHAR *file_ref_block = NULL);
      int set_vis_attrib(int mode, const TCHAR *AttrName = NULL);
      int set_values_attrib(C_SELSET &SelSet, C_RB_LIST &Values);
      int set_values_attrib(C_RB_LIST &Values);
      int get_values_attrib(C_RB_LIST &Values);
      int move_attrib(const TCHAR *AttribName, ads_pointp InsPt = NULL, ads_real *Rot = NULL);
      int set_h_text_attrib(ads_real h_text, const TCHAR *AttribName = NULL);
      int set_style_attrib(TCHAR *style, const TCHAR *AttribName = NULL);
      int set_color_attrib(C_COLOR &color, const TCHAR *AttribName = NULL);
      int set_layer_attrib(const TCHAR *layer, const TCHAR *AttribName = NULL);

      int copy_visAttrib(ads_name destDABlock);
      int copy_geomAttrib(ads_name destDABlock);

      int set_ins_mode_attrib(const TCHAR *AttribName, short *GenFlag = NULL,
                              short *HorzAlign = NULL, short *VertAlign = NULL);

};

int gsc_CountAttributes(ads_name Block);
int gsc_is_DABlock(AcDbObject *pObj);
DllExport int gsc_is_DABlock(ads_name ent);
int gs_is_DABlock(void);
DllExport int gsc_is_DABlock(presbuf list);
void gsinvisib(void);
int gs_invisib(void);
int gsc_invisib(ads_name SelSet, int CounterToVideo = GS_BAD);
int gsc_invisib(C_SELSET &SelSet, int CounterToVideo = GS_BAD);
void gsvisib(void);
int gs_visib(void);
int gsc_visib(ads_name SelSet, int CounterToVideo = GS_BAD);
int gsc_visib(C_SELSET &SelSet, int CounterToVideo = GS_BAD);

void gsInvOverlapAttr(void);
int gsc_InvOverlapAttr(C_SELSET &SelSet, int CounterToVideo = GS_BAD);

void gsTopDrawOrderAttr(void);

int gsc_set_vis_attrib(ads_name AttribEnt, int mode);
int gsc_set_vis_attrib(AcDbAttribute *pAttrib, int mode);
int gsc_set_style_attrib(AcDbAttribute *pAttrib, TCHAR *style);

int gsc_set_ins_mode_attrib(AcDbBlockReference *pBlock, const TCHAR *AttribName,
                            short *GenFlag = NULL, short *HorzAlign = NULL, 
                            short *VertAlign = NULL);

int gsc_set_fas_attrib(AcDbAttribute *pAttrib, C_FAS *pFas, ads_point abs_p_ins,
                       ads_real rotation_block, short GeneratFlag, short HorzAlign,
                       short VertAlign, int visib);

int gsc_set_fas_attrib(AcDbAttribute *pAttrib, C_FAS *pFas,
                       ads_point abs_p_ins, ads_real abs_rot, int visib);
extern int gsc_get_DA_attrib_name_list(ads_name ent, C_STR_LIST *attrib_name_list,
                                       C_RB_LIST *EntList = NULL);
int gsc_upd_DA_attrib_list(ads_name entity, C_RB_LIST &ColValues,
                           long Key, C_FAS *pFas, ads_name res_ent = NULL,
                           int ChangeLayer = GS_BAD, int MoveOldVisAttrib = GS_BAD);
extern int gsc_from_attblock2attribfaslist(ads_name Blk, C_ATTRIB_FAS_LIST *pFasList); 

int gsc_add_da_block(ads_name ent);
//int gsc_add_da_block(C_CLASS *p_class, C_RB_LIST &ColValues, C_SELSET &SelSet);

int gsc_getAttributeFromBlock(ads_name Block, const TCHAR *AttribName, ads_name Attrib);

int gsc_DefineDABlock(TCHAR *BlkName, C_ATTRIB_LIST *pAttribList);


#endif
