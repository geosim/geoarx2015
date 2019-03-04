/*************************************************************************/
/*   GS_EVID.H                                                           */
/*************************************************************************/


#ifndef _gs_evid_h
#define _gs_evid_h 1

#ifndef _gs_class_h
   #include "gs_class.h" 
#endif


// struttura usata per scambiare dati nella dcl "display_grid"
struct Common_Dcl_display_grid_Struct
{
   C_CGRID  *pGrid;    // puntatore alla classe griglia
   bool     ByPoints;  // se = Vero visualizzazione tramite punti
                       // altrimenti tramite celle
   bool     TwoDim;    // se = Vero visualizzazione in 2D
                       // altrimenti 3D
   C_STRING AttribZ;   // Se vuoto viualizzaione in 2D altrimenti
                       // 3D usando l'attributo come valore di quota
   int      Mode;      // Se = PREVIEW non crea oggetti ACAD (più veloce)
                       // se = EXTRACTION) crea oggetti grafici
   C_FAS    FAS;       // Caratteristiche grafiche degli oggetti da visualizzare
};


DllExport int gsc_ddChooseGraphSettings(C_CLASS *p_class, bool highlightOption,
                                        long *flag_set, C_FAS &GraphSettings, 
                                        double *MinZoomWindow, int *what, bool DefaultInit);
DllExport int gsc_ddChooseGraphSettings(long EnabledFas, bool highlightOption, 
                                        long *flag_set, C_FAS &GraphSettings, 
                                        double *MinZoomWindow, int *What);

int gs_ddChooseGraphSettings(void);

DllExport int gsc_ddevid(C_CLASS *p_class, C_SELSET &selset);
DllExport int gsc_ddGridEvid(C_CGRID *p_class, C_LONG_BTREE &KeyList);

int gsc_ddgraphBrowse(C_SELSET &selset, int DBOpCode = QUERY);

long gsc_WhatFas4Evid(C_CLASS *p_class);
void gsc_BlinkObjs(C_SELSET &ss);

int gsc_SaveEvidOptions(int ObjType, long flag_set, C_FAS &GraphSettings, C_STRING *pPath = NULL);
int gsc_LoadEvidOptions(int ObjType, long *flag_set, C_FAS &GraphSettings, C_STRING *pPath = NULL);
int gsc_load_last_DisplayGrid(Common_Dcl_display_grid_Struct *pStruct);
int gsc_save_last_DisplayGrid(Common_Dcl_display_grid_Struct *pStruct);

int gsc_dd_display_grid(Common_Dcl_display_grid_Struct &CommonStruct,
                        bool FASselection = TRUE);

#endif
