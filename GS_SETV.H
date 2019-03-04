/*************************************************************************/
/*   GS_SETV.H                                                          */
/*************************************************************************/


#ifndef _gs_setv_h
#define _gs_setv_h 1


/****************************************************************/
/* Define                                                       */
/****************************************************************/
#define MAX_LEN_NAMESETVISIB  30 + 1


class C_SET_VISIB: public C_NODE
{
   friend class C_SET_VISIB_LIST;

   public :
   C_CLASS    *pclass;        // Puntatore alla classe.
   C_STR_LIST InvAttribList;  // Lista nomi attributi visibili da nascondere
   
   C_SET_VISIB();
   virtual ~C_SET_VISIB();

   int AddInvAttrib(const TCHAR *AttrName);
   int DelInvAttrib(const TCHAR *AttrName);
   int load(FILE *file, bool Unicode = false);
   int save(FILE *file);
   int del(FILE *file);
   int activate(C_SELSET &SelSet, int AddObject2SaveSet);
};


class C_SET_VISIB_LIST: public C_LIST
{
   friend class C_SET_VISIB;

   private :
      int get_Path(C_STRING &Path);

   public :
      TCHAR name[MAX_LEN_NAMESETVISIB]; // nome del set di visibilità
      C_PROJECT *pPrj;

      int  load(void);
      int  save(void);
      int  del(void);
      int  activate(C_SELSET &SelSet, int AddObject2SaveSet = GS_BAD);
      
      C_SET_VISIB *get_VisibSet(C_CLASS *pCls);
      C_SET_VISIB *get_VisibSet(int cls, int sub = 0);

   C_SET_VISIB_LIST();
   virtual ~C_SET_VISIB_LIST();
};


extern void gsddsetvisib(void);
extern int gs_rsetv(void);
extern int gs_activesetv(void);
extern int gs_current_setv(void);


/**************************************************************/
/* Global Variables                                           */
/**************************************************************/
extern C_SET_VISIB_LIST *GS_ACTIVE_SET;


#endif
