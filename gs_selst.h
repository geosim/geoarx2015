/*************************************************************************/
/*   GS_SELST.H                                                          */
/*************************************************************************/


#ifndef _gs_selst_h
#define _gs_selst_h 1

#ifndef AD_DBENTS_H
   #include <dbents.h>
#endif



//----------------------------------------------------------------------------//
// class C_BHANDLERECT.                                                       //
//----------------------------------------------------------------------------//
class C_BHANDLERECT : public C_BSTR
{
   public :
      ads_point corner1, corner2;

   public :
      C_BHANDLERECT();
      C_BHANDLERECT(const TCHAR *in);
      virtual ~C_BHANDLERECT();
};


//----------------------------------------------------------------------------//
// class C_HANDLERECT_BTREE                                                   //
//                                                                            //
// Classe per gestire alberi binari di handle con il rettangolo della sessione     //
// occupata dall'oggtto grafico indicato dall'handle.                         //
//----------------------------------------------------------------------------//
class C_HANDLERECT_BTREE : public C_STR_BTREE
{
   public :
      C_HANDLERECT_BTREE();
      virtual ~C_HANDLERECT_BTREE();  // chiama ~C_BTREE

      // alloc item
      C_BNODE* alloc_item(const void *in);
};


//-----------------------------------------------------------------------//
//  CLASSE GESTIONE GRUPPO DI SELEZIONE ENTITA'
//-----------------------------------------------------------------------//   
class C_SELSET : public C_NODE
{
   friend class C_SELSET_LIST;

   private :
   ads_name ss;
   int      mReleaseAllAtDistruction; // flag perchè il distruttore rilasci il gruppo di selezione

   public :

DllExport C_SELSET();
DllExport virtual ~C_SELSET();

DllExport void ReleaseAllAtDistruction(int in);

DllExport int get_selection(ads_name sel);
          int get_AcDbObjectIdArray(AcDbObjectIdArray &ObjIds, bool Append = false);
          int get_AcDbEntityPtrArray(AcDbEntityPtrArray &EntitySet, bool Append = false, AcDb::OpenMode mode = AcDb::kForRead);
          int set_AcDbEntityPtrArray(AcDbEntityPtrArray &EntitySet);
DllExport int clear(void);
          int save(const TCHAR *name = NULL);
          int load(const TCHAR *name = NULL);
DllExport int operator << (ads_name selset);
DllExport long length();
DllExport int entname(long i, ads_name ent);
DllExport int is_member(ads_name ent);
          int is_presentGraphicalObject(long *Qty);

DllExport int add_selset(ads_name Group, int OnFile = GS_BAD, const TCHAR *name = NULL); 
DllExport int add_selset(C_SELSET &selset, int OnFile = GS_BAD, const TCHAR *name = NULL);
DllExport int add(ads_name ent, int OnFile = GS_BAD, FILE *fhandle = NULL);
          int add_DA();

DllExport int subtract(C_SELSET &SelSet);
DllExport int subtract(ads_name selset);
          int subtract(const TCHAR *GraphType);
DllExport int subtract_ent(ads_name ent);
          int copy(ads_name out_selset);
DllExport int copy(C_SELSET &out_selset);
DllExport int copyIntersectType(C_SELSET &out_selset, int What);
DllExport int copyIntersectClsCode(C_SELSET &out_selset, int cls, int sub = 0,
                                   bool ErasedObject = false);

  presbuf to_rb(void);

DllExport int intersectType(int What);
DllExport int intersectVisibile(void);
DllExport int intersect(ads_name selset);
DllExport int intersect(C_SELSET &SelSet);
DllExport int intersectUpdateable(int State = UPDATEABLE, int set_lock = GS_GOOD,
                                  int CounterToVideo = GS_BAD);
DllExport int intersectClsCode(int cls, int sub = 0, int What = ALL);
DllExport int intersectClsCodeList(C_INT_INT_LIST &ClsSubList, int What = ALL);
DllExport int get_ClsCodeList(C_INT_INT_LIST &ClsSubList, bool *UnknownObjs = NULL);
          int SpatialIntersectWindow(ads_point p1, ads_point p2, int CheckOn2D = GS_GOOD,
                                     C_HANDLERECT_BTREE *pHandleRectTree = NULL,
                                     int CounterToVideo = GS_BAD);
          int get_SpatialIntersectWindowSS(ads_point p1, ads_point p2, C_SELSET &out, int CheckOn2D = GS_GOOD,
                                           C_HANDLERECT_BTREE *pHandleRectTree = NULL,
                                           int CounterToVideo = GS_BAD);
          long get_SpatialIntersectWindowNumObjs(ads_point p1, ads_point p2, int CheckOn2D = GS_GOOD,
                                                 C_HANDLERECT_BTREE *pHandleRectTree = NULL,
                                                 int CounterToVideo = GS_BAD);
DllExport int SpatialIntersectEnt(ads_name IntersEnt, int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                                  int CounterToVideo = GS_GOOD, bool IncludeFirstLast = true);
          int SpatialIntersectEnt(AcDbEntity *pIntersEnt, int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                                  int CounterToVideo = GS_GOOD, bool IncludeFirstLast = true);
          int SpatialInternalEnt(ads_name ContainerEnt, int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                                 int Mode = INSIDE, int CounterToVideo = GS_GOOD);
          int SpatialInternalEnt(AcDbEntity *pContainerEnt, int CheckOn2D = GS_GOOD, int OnlyInsPt = GS_GOOD,
                                 int Mode = INSIDE, int CounterToVideo = GS_GOOD);

DllExport int redraw(int mode);
DllExport int DrawCross(void);
          int getWindow(ads_point corner1, ads_point corner2);
DllExport int zoom(ads_real min_dim_x = 0.0, ads_real min_dim_y = 0.0);
DllExport int Erase(void);
          int UnErase(void);

          int PolylineJoin(void);
          int PolylineFit(void);

          int ClosestToEnt(ads_name Ent, int Qty = 1);
          int ClosestTo(C_SELSET &SelSet, int Qty = 1);
DllExport bool operator == (C_SELSET &in);
DllExport bool operator != (C_SELSET &in);
};


//----------------------------------------------------------------------------//
//    class C_SELSET_LIST                                                     //
//----------------------------------------------------------------------------//
class C_SELSET_LIST : public C_LIST
{
   public :
      DllExport C_SELSET_LIST();
      DllExport virtual ~C_SELSET_LIST(); // chiama ~C_LIST

      int PolylineJoin(void);
      int PolylineFit(void);
};


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


extern C_SELSET GS_ALTER_FAS_OBJS; // gruppo di selezione degli oggetti a cui è stata
                                   // variata la FAS (tramite evidenziazione risultato
                                   // filtro o estrazione con FAS default/utente)
extern C_SELSET GS_SELSET; // Gruppo di selezione di uso pubblico


#endif
