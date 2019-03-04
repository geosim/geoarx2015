/*************************************************************************/
/*   GS_GRID.H                                                           */
/*************************************************************************/


#ifndef _gs_grid_h
#define _gs_grid_h 1

#ifndef _gs_list_h
   #include "gs_list.h" 
#endif

// Griglia
int gs_DisplayGridBorder(void);
int gs_DisplayGridPts(void);
int gs_DisplayGridCells(void);
int gs_DisplayGridContours(void);
int gs_GridSpatialInterpolationIDW(void);
int gs_ObjectsOnGrid(void);
int gs_SetAttribEntityFromGrid(void);
int gs_InitIterpolateDisplayContoursGrid(void);
int gs_DisplayValleyOrRidge(void);

DllExport int gsc_getFASList4ContoursGrid(int nIntervals, double MinValue, double MaxValue,
                                          C_COLOR &StartColor, C_COLOR &EndColor,
                                          const TCHAR *LayerPrefix,
                                          C_FAS_LIST &FAS_list);
DllExport int gsc_getFASList4ContoursGrid(double EveryStep, double MinValue, double MaxValue,
                                          C_COLOR &StartColor, C_COLOR &EndColor,
                                          const TCHAR *LayerPrefix,
                                          C_FAS_LIST &FAS_list);

int gsc_GridEvid(C_CGRID *pCls, C_LINK_SET &ResultLS,
                 bool ByPoints, C_STRING &AttribZ,
                 C_FAS *pFAS = NULL, int Mode = PREVIEW);


#endif
