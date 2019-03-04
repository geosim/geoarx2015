/**********************************************************/
/*   GS_CMD.H                                             */
/**********************************************************/


#ifndef _gs_cmd_h
#define _gs_cmd_h 1

#ifndef AD_DBMAIN_H
   #include "dbmain.h"         // per i reattori
#endif


/*********************************************************/
/* TYPEDEFS */
/*********************************************************/


typedef struct 
{
   C_STRING name;       // nome comando
   int      (*fptr)();  // puntatore a funzione
   int      flag;       // controllo 1° Bit operativita su spazio carta no-si
                        // controllo 2° Bit comando disponibile con o senza login
} FunTable;

typedef struct 
{
   C_STRING        GlobalName;
   C_STRING        LocalName;
   AcRxFunctionPtr fptr;
   bool            LoginNeeded;
   Adesk::Int32    flag;
} CmdTable;


//-----------------------------------------------------------------------//   
// Classe derivata da AcDbDatabaseReactor per controllo su:
// Cancellazioni oggetti GEOsim
//-----------------------------------------------------------------------//

class C_GSDBReactor : public AcDbDatabaseReactor
{
   public:
   // reattore per modifica oggetti del database grafico di ACAD
   virtual void objectModified(const AcDbDatabase *dwg, const AcDbObject *dbObj);

   // reattore per inserimento oggetti al database grafico di ACAD
   virtual void objectAppended(const AcDbDatabase *dwg, const AcDbObject *dbObj);

   // reattore per cancellazione oggetti dal database grafico di ACAD
   virtual void objectErased(const AcDbDatabase *dwg, const AcDbObject *dbObj,
                             Adesk::Boolean pEased);
};


class C_GSEditorReactor : public AcEditorReactor2
{
   public:
   ACRX_DECLARE_MEMBERS(C_GSEditorReactor);

   // constructor/destructors
            C_GSEditorReactor();
    virtual ~C_GSEditorReactor();

    virtual void beginSave (AcDbDatabase *dwg, const ACHAR* pIntendedName);
    virtual void commandWillStart(const ACHAR* cmdStr);
};


// classe per gestire lo stack dei comandi e delle funzioni LISP di
// GEOsim (1 sola istanza)
class C_CMDLIST
{
   public:

DllExport C_CMDLIST();
DllExport ~C_CMDLIST();                           

      int GEOsimCmdsLoad(void);
      int UndefineAcadCmds(void);
      int RedefineAcadCmds(void);
      int funcLoad();
      int funcUnload();
      FunTable *get_ptr_fun(int val);
      int doFunc();

      void StartCmd(void);
      void EndCmd(void);
      void ErrorCmd(void);
      void CancelCmd(void);

      bool CmdRunningFromInternalCall;

   private:
      FunTable *exfun;
      int      LenExfun;
      CmdTable *GEOsimCmds;
      int      LenGEOsimCmds;
      CmdTable *AcadCmds;
      int      LenAcadCmds;

      int      PrevReactorAbilit;
};


int gsc_is_acad_main_entity(ads_name ent);
int gsc_is_acad_main_entity(presbuf gigi);
int gsc_is_acad_main_entity(C_STRING &DXFType);
int gsc_is_acad_main_entity(const TCHAR *DXFType);

extern int gs_scal_attr_text(void);

void gsbreak();
void gscopy();
void gswblock(void);
void gscopy2AcadObjs(void);
void gsdbcopy();
void gserase();
void gssetdbdefault(void);
void gsaddmemberstogrp(void);
void gsremovememberstogrp(void);
int gsc_getActualScreeRect(ads_point LeftLowerPt, ads_point RightUpperPt);
int IsVisibleVertexOnCurrentScreen(ads_name polyline, long *nVertex);
void gssaveas(); 
void gssave(); 
void gsqsave(); 
void gsnew();   
void gsopen();   

void gshatch(void);

int gsc_break(ads_name ent, ads_point pt);
int gsc_break(ads_name Link, ads_name Node);

int gs_import(void);

extern int ObjLock();            // Test per lock oggetti

DllExport extern long gsc_align_data_base(void);
DllExport extern int gsc_enable_reactors(void);
DllExport extern int gsc_disable_reactors(void);
DllExport extern int gsc_reactors_off(void);
DllExport extern int gsc_is_enable_reactors(void);
int gsc_hatch(ads_name SelSet, TCHAR *hatch, double scale = 1, double rotation = 0,
              C_COLOR *pColor = NULL, TCHAR *layer = NULL, int CounterToVideo = GS_BAD);
extern int gs_enable_reactors(void);
extern int gs_disable_reactors(void);
extern int gs_is_reactorsoff(void);
extern void gsreactorsoff(void);
void gsc_reactors_ClearObjs(void);
int gs_UndefineAcadCmds(void);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

extern C_GSDBReactor *GSDBReactor;  // reattore al database grafico di ACAD
extern int GS_CHECK_DBREACTOR;
extern C_GSEditorReactor *GSEditorReactor;  // reattore al database grafico di ACAD


// se GS_GOOD -> reattori abilitati,  se GS_BAD -> reattori non abilitati,
// se GS_CAN -> reattori non abilitati e NON è più possibile riattivare i reattori 
// finchè ci si trova in una sessione di lavoro (all'uscita di una sessione di lavoro 
// i reattori vengono automaticamente riabilitati)


#endif