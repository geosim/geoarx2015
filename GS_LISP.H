/******************************************************************************/
/*    GS_LISP.H                                                               */
/******************************************************************************/


#ifndef _gs_lisp_h
#define _gs_lisp_h 1


class C_VAR : public C_NODE
{
   friend class C_VAR_LIST;

   public :

   C_STR *func_stream;      // Puntatore corpo della funzione
   TCHAR  *value;            // Valore della variabile
   TCHAR  *proc_name;        /* nome della procedura di dichiarazione */
   TCHAR  *name;             /* nome della variabile */
   int   ttype;             /* tipo della variabile n:numerico,c:carattere,l:logico */

   C_VAR() : C_NODE() { value=name=proc_name=NULL; func_stream=NULL; ttype=0; }
   C_VAR(const TCHAR *pr_na,const TCHAR *na);
   C_VAR (C_STR *func_str, const TCHAR *va, const TCHAR* pr_na, const TCHAR *na, int ty);
   ~C_VAR();

   int set (C_STR *func_str, const TCHAR *va, int ty);
   TCHAR *get_name();
}; 
 

class C_VAR_LIST : public C_LIST
   {
     public :
      C_VAR_LIST();
      virtual ~C_VAR_LIST();

      C_VAR *search_var(const TCHAR *pr_na, const TCHAR *na);
   };
                                                                           

// Struttura che definisce una funzione di calcolo GEOLISP
typedef struct
{
   TCHAR *Name;    // Nome funzione
   TCHAR *Type;    // Tipo in formato stringa ("real", "short", "point", "long", "string")
   TCHAR *DefVal;  // Il valore è sempre in formato stringa che verrà 
                   // convertito a seconda di restype 
   TCHAR *Descr;   // Descrizione funzione
} GEOLispFunction;


///////////////////////////////////////////////////////////////////////////


/**********************************************************************/
/*   FUNCTION'S PROTOTYPES */
/**********************************************************************/

int            gs_calc_attr(void);
int            gsc_chk_attr(const TCHAR *attr, presbuf NewValue, C_RB_LIST &ColValues, 
                            const TCHAR *stream, const TCHAR *file);
int            gsc_calc_attr(int prj,int cls,int sub, const TCHAR *attr, C_RB_LIST &ColValues);
DllExport resbuf *gsc_exec_lisp(const TCHAR *call, const TCHAR *stream, const TCHAR *name_file,
                                int screen = TRUE);
long           gsc_WhatIsGraphModified(const TCHAR *fun_graph_calc, const TCHAR *file_graph_calc);
long           gsc_WhatIsGraphModified(C_NODE *pCls);

int            gsc_HowManyToken(const TCHAR *fun_graph_calc, const TCHAR *file_graph_calc,
                                const TCHAR *Token);

DllExport int  gsc_is_param(const TCHAR *calc_file, const TCHAR *calc_func, const TCHAR *par);

void           gslisp(void);

TCHAR          *what_is(int screen, const TCHAR *curr_fun, const TCHAR *stream, size_t *ptr,
                         const TCHAR *file_name, int flag, long *WhatIsGraphModified = NULL,
                         const TCHAR *TokenToSearch = NULL, int *QtyToken = NULL); // Alloca stringa in uscita
TCHAR          *exec_proc(int screen, const TCHAR *proc, const TCHAR *call_proc, 
                          const TCHAR *stream, size_t *ptr,const TCHAR *file_name,
                          long *WhatIsGraphModified = NULL, const TCHAR *TokenToSearch = NULL,
                          int *QtyToken = NULL); // Alloca stringa in uscita 
TCHAR          *load_lisp(int screen, const TCHAR *stream, size_t *ptr, const TCHAR *name_file,
                          const TCHAR *other_file, long *WhatIsGraphModified = NULL,
                          const TCHAR *TokenToSearch = NULL, int *QtyToken = NULL); // Alloca stringa in uscita

TCHAR          *reco(int screen, const TCHAR *stream, size_t *ptr); // Non alloca stringa in uscita
TCHAR          OK_string(TCHAR lett);
TCHAR          *find_var(int screen, const TCHAR *stream, size_t *ptr,const TCHAR *proc, 
                         const TCHAR *name); // Alloca stringa in uscita //

TCHAR          *tostring(TCHAR *num);       // Lavora su stessa stringa in ingresso //
TCHAR          *stringto(const TCHAR *num); // Alloca stringa in uscita //
TCHAR          *error(int screen, const TCHAR *stream, size_t *ptr, int err, const TCHAR *what);

///////////////////////////////////////////////////////////////////////////

C_STR          *copy_function(int screen, const TCHAR *stream, size_t *ptr, long pt1, long pt2);
C_VAR          *is_var(const TCHAR *proc, const TCHAR *name);

///////////////////////////////////////////////////////////////////////////

void           del_comment(const TCHAR *stream, size_t *ptr);
DllExport int  isnumber(const TCHAR *num);
int            isstring(const TCHAR *num);
DllExport int  rb2lspstr(resbuf *rb, TCHAR *value, int len = -1, int dec = -1); // usata in S2E

int            jump_fun(int screen, const TCHAR *stream, size_t *ptr);
//int            readscreen(int screen);
int            readscreen(int screen, C_STR *Input_string);
int            count_par(const TCHAR *stream,int *flag);
int            del_var(void);
DllExport int  alloc_var(int screen, const TCHAR *stream, size_t *ptr, int pub, const TCHAR *proc, 
                         const TCHAR *name, const TCHAR *val, C_STR* func_str);
DllExport int  release_var(const TCHAR *proc);
int            conv_stream(TCHAR *tmp_stream);

///////////////////////////////////////////////////////////////////////////

void           gsGslReload(void);
int            gs_gsl_reload(void);
void           gsGslGsReload(void);
int            gs_gsl_gs_reload(void);
void           gsGslPrjReload(void);
int            gs_gsl_prj_reload(void);
int            gsc_load_gsl(TCHAR *dove);

int            gs_LoadFuncFromGraphList(void);
int            gs_LoadFuncToGraphList(void);
int            gs_LoadFuncNameRemList(void);
int            gsc_LoadFuncNameRemList(const TCHAR *Path, C_2STR_LIST &FuncNameRemList);

///////////////////////////////////////////////////////////////////////////

DllExport int gsc_getPntRtzOnObj(ads_name ent, ads_point point, double *angle, const TCHAR *mode, 
                                 double offset1=0, double offset2=0, int check_rotz = GS_GOOD);
int gsc_getPntRtzOnObj(AcDbEntity *pEnt, ads_point point, double *angle, const TCHAR *mode, 
                                 double offset1=0, double offset2=0, int check_rotz = GS_GOOD);

/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

extern C_NODE *GS_CALC_CLASS;  // puntatore pubblico dedicato al calcolo dei campi calcolati
                                // vedi funzioni di calcolo di dati da grafica.

extern GEOLispFunction VectorFromGraphGEOLispFunctions[];
long VectorFromGraphGEOLispFunctionsLen();

#endif