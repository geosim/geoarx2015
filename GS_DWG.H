/*************************************************************************/
/*   GS_DWG.H                                                          */
/*************************************************************************/


#ifndef _gs_dwg_h
#define _gs_dwg_h 1

#include "adeads.h"

#ifndef _gs_selst_h
   #include "gs_selst.h" 
#endif

class C_FAS;

#include "MapProj.h"
#include "MapArxApi.h"

//-----------------------------------------------------------------------//
//  CLASSE GESTIONE LISTA DWG PER IL SALVATAGGIO //
//-----------------------------------------------------------------------//   

class C_DWG : public C_STR
{
   friend class C_DWG_LIST;

   protected :
      long size;             // dimensione del file DWG in byte
      double Xmin;           // estensione minima asse X del disegno
      double Ymin;           // estensione minima asse Y del disegno
      double Xmax;           // estensione massima asse X del disegno
      double Ymax;           // estensione massima asse Y del disegno
      AcDbObjectIdArray Ids; // oggetti risultanti dalla lettura del disegno

      AcMapAttachedDrawing *pMapAttachedDwg; // puntatore a dwg attaccato

   private:
      void get_extents_file_path(C_STRING &ExtFilePath);

   public :
DllExport C_DWG();
      C_DWG(C_STRING &_Path, long _Size, bool DateSensitive = TRUE);
      virtual ~C_DWG();

      int copy(C_DWG &out);
      long get_size();
      int  set_size(long in);
      int  resize();

      int  set_extents(double _Xmin, double _Ymin, double _Xmax, double _Ymax);
DllExport void get_extents(double *_Xmin, double *_Ymin, double *_Xmax, double *_Ymax);
DllExport int  regen_extents_from_dwg(int MessageToVideo = GS_GOOD);
      int  save_extents(void);
DllExport int load_extents(bool DateSensitive = TRUE);
      int  intersect(double _Xmin, double _Ymin, double _Xmax, double _Ymax);

      AcDbObjectIdArray *ptr_ObjectIdArray(void);
      void              clear_ObjectIdArray(void);

      int  is_empty(bool *_Empty);

      TCHAR *get_ADEAliasPath(void);

      ade_id GetAdeId(void);
      int is_attached(void);
      int is_activated(void);
      int activate(void);
      int deactivate(void);
      int attach(void);
      int detach(void);
      int ApplyQuery(int retest = MORETESTS);
      int QueryIn(C_SELSET *pSelSet = NULL, long flag_set = GSNoneSetting, 
                  C_FAS *pFAS = NULL, int retest = MORETESTS);
      int Report(C_STRING &Path, const TCHAR *Mode,
                 int retest = MORETESTS);
      int Preview(long flag_set = GSNoneSetting, C_FAS *pFAS = NULL, int retest = MORETESTS);
      int unlockObjs(void);
      bool HasLocks(void);
      int GetTableList(C_STR_LIST &TableList, AcMap::ETableType kType);
      int index(int prj = 0, int cls = 0);
      int setCrdSys(const TCHAR *CrdSys);
      int Save(C_SELSET &SelSet, int retest = MORETESTS);
      int Save(ads_name SelSet, int retest = MORETESTS);
};
                  
//-----------------------------------------------------------------------//   

class C_DWG_LIST : public C_LIST
{
   public :
DllExport C_DWG_LIST() : C_LIST() { }
DllExport virtual ~C_DWG_LIST(){ } // chiama ~C_LIST

      C_DWG *search_min(void);
DllExport int load(int prj, int cls, bool DateSensitive = TRUE);
      int load(int prj, int cls, TCHAR *dir_dwg, bool DateSensitive = TRUE);
      resbuf *to_rb();
      int copy(C_DWG_LIST &out);

      int update_extents(C_SELSET &SS, int OnlyUpdate = GS_GOOD);
      int regen_extents_from_dwg(int OnlyUpdate = GS_GOOD);
      C_DWG* SelectDwg(int cls);
DllExport int intersect(double Xmin, double Ymin, double Xmax, double Ymax);
      int activate(void);
      int deactivate(void);
      int attach(void);
      int detach(void);
      int unlockObjs(void);
      int index(int prj = 0, int cls = 0);
      int setCrdSys(const TCHAR *CrdSys);
      long get_size();
      int SaveObjs(void);

      int ApplyQuery(int retest = MORETESTS, int CounterToVideo = GS_BAD);
      int QueryIn(C_SELSET *pSelSet = NULL, long flag_set = GSNoneSetting,
                  C_FAS *pFAS = NULL, int retest = MORETESTS, int CounterToVideo = GS_BAD);
      int Report(C_STRING &Path, const TCHAR *Mode, int retest = MORETESTS);
      int Preview(long flag_set = GSNoneSetting, C_FAS *pFAS = NULL,
                  int retest = MORETESTS);
};


//----------------------------------------------------------------------------//
//    class C_INT_DWGS                                                        //
//----------------------------------------------------------------------------//

class C_INT_DWGS : public C_INT
{
   friend class C_INT_DWGS_LIST;

   public :
      C_DWG_LIST DwgList;

      DllExport  C_INT_DWGS();
      DllExport  virtual ~C_INT_DWGS();

      DllExport int LoadFromClsCode(int cls);
};


class C_STR_INT : public C_STR
{
   friend class C_STR_INT_LIST;

   protected :
      long  size;

   public :
      C_STR_INT() : C_STR() { size=0; }
      virtual ~C_STR_INT(){ }

      int  get_size() { return size; }
      int  resize();
      int  set_size(long in)  { size=in; return GS_GOOD; }
};
                  
//-----------------------------------------------------------------------//   

class C_STR_INT_LIST : public C_LIST
{
   public :
      C_STR_INT_LIST() : C_LIST() { }
      virtual ~C_STR_INT_LIST(){ } // chiama ~C_LIST
 
      int load(TCHAR *stringa, int intero);
};


int gsc_getClsPrefixDWGName(int prj, int cls, C_STRING &Prefix);
DllExport int gsc_CreaFirstDWG(int prj, int cls, TCHAR *dir_dwg,
                               const TCHAR *coord = NULL, TCHAR **path_dwg = NULL);

int gsc_optimizeDWG(const TCHAR *PrefixDWGName, C_STRING &ODTableName,
                    ads_point pMin, ads_point pMax, C_SELSET &SS,
                    long ObjsMaxLimit, bool HorizSplit = FALSE,
                    C_HANDLERECT_BTREE *pHandleRectTree = NULL,
                    int DepthLevel = 1, int Level = 1);

int gs_DisplayDWGExt(void);
DllExport int gsc_DisplayDWGExt(int Prj, int Cls);

int gsc_PrepareObjects4AdeSave(int Cls, int Sub, C_SELSET &SelSet);
int gsc_SaveNewObjs(int cls, C_SELSET &NewObjs, C_DWG_LIST &DwgList);

int gs_get_dwg_list(void);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


#endif
