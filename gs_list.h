/******************************************************************************/
/*    GS_LIST.H                                                               */
/******************************************************************************/


#ifndef _gs_list_h
#define _gs_list_h 1

#ifndef _INC_STDIO
   #include <stdio.h>
#endif

#ifndef _adslib_h
   #include "adslib.h"   
#endif

#ifndef ASI_ASIAPPL_H
   #include "asiappl.h"
#endif

#include "dbmain.h" 

#include "..\gs_def.h" // definizioni globali


//----------------------------------------------------------------------------//
//    class C_RB_LIST                                                         //
//----------------------------------------------------------------------------//

class C_RB_LIST
{
   private:
      presbuf head, cursor, tail;
      int     count;                    // n elementi in lista
      int     mReleaseAllAtDistruction; // flag perchè il distruttore rilasci la lista rb

   public :
DllExport C_RB_LIST();
DllExport C_RB_LIST(resbuf *prb);
DllExport virtual ~C_RB_LIST(); 
DllExport int IsEmpty(void);
      
DllExport int remove_all();

DllExport int remove_all_without_dealloc();
DllExport presbuf remove_head();
DllExport presbuf remove_tail();
DllExport presbuf remove(presbuf punt);    // puntatore
DllExport presbuf remove_at();             // cursor position
DllExport presbuf remove_at(int pos);      // ordinal position (1 è il primo)

DllExport int GetCount();
DllExport int GetSubListCount();
DllExport void ReleaseAllAtDistruction(int in);
DllExport presbuf get_head(); 
          presbuf get_tail();
DllExport presbuf get_cursor();
          void set_cursor(presbuf punt);

DllExport presbuf get_next();
DllExport presbuf getptr_at(int n);    // 1 è il primo elemento

DllExport presbuf link_head(resbuf *prb);
DllExport presbuf link_tail(resbuf *prb);
DllExport presbuf link_atCursor(resbuf *prb);

DllExport presbuf SearchType(int restype);
DllExport presbuf SearchNextType(int restype);

DllExport presbuf SearchName(const TCHAR *str, int sensitive = TRUE);
DllExport presbuf SearchName(const char *str, int sensitive = TRUE);
          presbuf SearchNextName(const TCHAR *str, int sensitive = TRUE);
          presbuf SearchNextName(const char *str, int sensitive = TRUE);

DllExport resbuf* operator << (resbuf *prb);
          resbuf* operator << (resbuf mrb);
          resbuf* operator << (C_RB_LIST *mrb);
DllExport resbuf* operator += (resbuf *prb);
DllExport resbuf* operator += (C_RB_LIST *mrb);
DllExport resbuf* operator += (C_RB_LIST &mrb);
          resbuf* operator += (TCHAR *str);
          
          resbuf* operator = (resbuf *prb);

DllExport int copy(C_RB_LIST &out);
DllExport int append(C_RB_LIST &out);

DllExport void LspRetList(void);

          // funzioni da utilizzare se la lista ha una struttura tipo LISP
          // es. ((a b)(c d))
DllExport presbuf Assoc(const TCHAR *str, int sensitive = FALSE);
          presbuf Assoc(const char *str, int sensitive = FALSE);
DllExport presbuf CdrAssoc(const TCHAR *str, int sensitive = FALSE);
DllExport presbuf CdrAssoc(const char *str, int sensitive = FALSE);

DllExport int CdrAssocSubst(const TCHAR *str, long new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(const char *str, long new_value, int sensitive = FALSE);
          int CdrAssocSubst(const TCHAR *str, int new_value, int sensitive = FALSE);
          int CdrAssocSubst(const char *str, int new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(const TCHAR *str, double new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(const char *str, double new_value, int sensitive = FALSE);
          int CdrAssocSubst(const TCHAR *str, TCHAR *new_value, int sensitive = FALSE);
          int CdrAssocSubst(const char *str, char *new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(const TCHAR *str, presbuf new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(const char *str, presbuf new_value, int sensitive = FALSE);
DllExport int CdrAssocSubst(C_RB_LIST &ColValues, int sensitive = FALSE);

DllExport int SubstRTNONEtoDifferent(C_RB_LIST &mrb, int sensitive = FALSE);

DllExport presbuf nth(int i);

DllExport int insert_after(resbuf *prb); // after cursor position
DllExport int insert_after(C_RB_LIST &mrb); // after cursor position
};
 


//----------------------------------------------------------------------------//
//    class C_STRING                                                          //
//----------------------------------------------------------------------------//

class C_STRING
{
   protected :  
      TCHAR  *name;
      size_t dim;

   public :
      DllExport C_STRING();
                C_STRING(int in);
      DllExport C_STRING(const TCHAR *in);
      DllExport C_STRING(const TCHAR *format, va_list args);
      DllExport C_STRING(const char *in);
      DllExport C_STRING(C_STRING &in);
      DllExport virtual ~C_STRING();

                int  alloc(int in); 

      DllExport TCHAR *get_name();
      DllExport int set_name(C_STRING &in); 
      DllExport int set_name(const TCHAR *in); 
      DllExport int set_name(const TCHAR *format, va_list args); 
      DllExport int set_name(const char *in); 
      DllExport int set_name(const TCHAR *in, int start, int end); 
      DllExport int set_name(const char *in, int start, int end); 
      DllExport int set_name(double in); 

      DllExport int set_name_formatted(const TCHAR *format, ...); 
      DllExport int set_name_formatted(const TCHAR *format, va_list args); 
      
      DllExport int cat(const TCHAR *in);
      DllExport int cat(const char *in);

      DllExport size_t len();
      DllExport int comp(const TCHAR *in, int sensitive = TRUE); 
      DllExport int comp(const char *in, int sensitive = TRUE); 
      DllExport int comp(C_STRING &in, int sensitive = TRUE); 
      DllExport int wildcomp(C_STRING &wild, int sensitive = TRUE);
      DllExport int wildcomp(const TCHAR *wild, int sensitive = TRUE);
      DllExport int wildcomp(const char *wild, int sensitive = TRUE);
      DllExport int isAlpha(void);
      DllExport int isAlnum(TCHAR CharToExclude = _T('\0'));
      DllExport int isDigit(void);
      DllExport TCHAR *paste(TCHAR *in);
                TCHAR *cut();
      DllExport void clear();
      DllExport int set_chr(const TCHAR in, size_t pos);
      DllExport int set_chr(const char in, size_t pos);
      DllExport TCHAR get_chr(size_t pos);

      DllExport int find(const TCHAR *in, int sensitive = TRUE);
      DllExport int find(C_STRING &in, int sensitive = TRUE);
      DllExport int find(TCHAR in);
      DllExport int reverseFind(const TCHAR *in, int sensitive = TRUE);
      DllExport int reverseFind(C_STRING &in, int sensitive = TRUE);
      DllExport int reverseFind(TCHAR in);
      DllExport C_STRING right(size_t n);
      DllExport C_STRING left(size_t n);     
      DllExport C_STRING mid(size_t pos, size_t n); // 0-indexed
      DllExport int remove(TCHAR in, int sensitive = TRUE);

      DllExport C_STRING& alltrim();
      DllExport C_STRING& ltrim();
      DllExport C_STRING& rtrim();
      DllExport C_STRING& toupper();
      DllExport C_STRING& tolower();
      DllExport TCHAR *at(TCHAR in);
      DllExport TCHAR *at(const TCHAR *in, int sensitive = TRUE);
      DllExport TCHAR *at(const char *in, int sensitive = TRUE);
      DllExport TCHAR *at(C_STRING &in, int sensitive = TRUE);
      DllExport TCHAR *rat(TCHAR in);
      DllExport TCHAR *rat(const TCHAR *in, int sensitive = TRUE);
      DllExport TCHAR *rat(const char *in, int sensitive = TRUE);
      DllExport TCHAR *rat(C_STRING &in, int sensitive = TRUE);
      DllExport bool startWith(const TCHAR *in, int sensitive = TRUE);
      DllExport bool startWith(C_STRING &in, int sensitive = TRUE);
      DllExport bool endWith(const TCHAR *in, int sensitive = TRUE);
      DllExport bool endWith(C_STRING &in, int sensitive = TRUE);

      DllExport int strtran(TCHAR *sub, TCHAR *con, int sensitive = TRUE);
      DllExport int strtran(char *sub, char *con, int sensitive = TRUE);
      DllExport int removePrefixSuffix(const TCHAR *Prefix = NULL, const TCHAR *Suffix = NULL,
                                       int sensitive = TRUE);
      DllExport int removePrefixSuffix(const char *Prefix = NULL, const char *Suffix = NULL,
                                       int sensitive = TRUE);
      DllExport int removePrefixSuffix(const TCHAR Prefix, const TCHAR Suffix,
                                       int sensitive = TRUE);
      DllExport int addPrefixSuffix(const TCHAR *Prefix = NULL, const TCHAR *Suffix = NULL);
      DllExport int addPrefixSuffix(const char *Prefix = NULL, const char *Suffix = NULL);
      DllExport int toi(void);
      DllExport long tol(void);
      DllExport double tof(void);
      DllExport int toPt(ads_point Pt);
      DllExport void toHTML(void);
      DllExport void toAXML(void);

	   DllExport C_STRING& operator += (const TCHAR *in);
	   DllExport C_STRING& operator += (const char *in);
      DllExport C_STRING& operator += (C_STRING &in);
      DllExport C_STRING& operator += (TCHAR in);
      DllExport C_STRING& operator += (char in);
      DllExport C_STRING& operator += (short in);
      DllExport C_STRING& operator += (int in);
      DllExport C_STRING& operator += (long in);
      DllExport C_STRING& operator += (float in);
      DllExport C_STRING& operator += (double in);
      DllExport C_STRING& operator += (presbuf in);
      DllExport C_STRING& operator += (ads_point in);

	   DllExport C_STRING& operator = (const TCHAR *in);
	   DllExport C_STRING& operator = (const char *in);
      DllExport C_STRING& operator = (C_STRING &in);
      DllExport C_STRING& operator = (TCHAR in);
      DllExport C_STRING& operator = (char in);
      DllExport C_STRING& operator = (short in);
      DllExport C_STRING& operator = (int in);
      DllExport C_STRING& operator = (long in);
      DllExport C_STRING& operator = (float in);
      DllExport C_STRING& operator = (double in);
      DllExport C_STRING& operator = (presbuf in);
      DllExport C_STRING& operator = (ads_point in);

	   DllExport C_STRING operator + (const TCHAR *in);
	   DllExport C_STRING operator + (const char *in);
      DllExport C_STRING operator + (C_STRING &in);
      DllExport C_STRING operator + (TCHAR in);
      DllExport C_STRING operator + (char in);
      DllExport C_STRING operator + (short in);
      DllExport C_STRING operator + (int in);
      DllExport C_STRING operator + (long in);
      DllExport C_STRING operator + (float in);
      DllExport C_STRING operator + (double in);
      DllExport C_STRING operator + (presbuf in);
      DllExport C_STRING operator + (ads_point in);

      DllExport bool operator == (const TCHAR *in);
      DllExport bool operator == (const char *in);
      DllExport bool operator == (C_STRING &in);
      DllExport bool operator != (const TCHAR *in);
      DllExport bool operator != (const char *in);
      DllExport bool operator != (C_STRING &in);
};


 
//----------------------------------------------------------------------------//
//    class C_NODE.                                                           //
//                                                                            //
// CLASSE BASE da cui derivare oggetti da organizzzare in liste di tipo       //
// C_LIST (doppiamente linkate). I dati della classe si limitano a            //
// puntatori ad altri oggetti di tipo C_NODE. Le funzioni get_key() e         //
// get_name() sono virtuali da ridefinire come eventuali chiavi di ricerca    //
// rispettivamente per codice(int) o per nome (char*).                        //
//----------------------------------------------------------------------------//

class C_NODE
{
   friend class C_LIST;
   friend class C_FAS_LIST; // serve per sortByNum di C_FAS_LIST che accede a membri protetti di C_LIST
   
   protected :
      C_NODE *prev;
      C_NODE *next;
      DllExport   virtual int comp(void *in); // usata per il sort   
      
   public :
      DllExport   C_NODE();
                  C_NODE(C_NODE *p,C_NODE *n);
      DllExport   virtual ~C_NODE();

      DllExport   virtual int  get_key();
      DllExport   virtual int  get_type();
      DllExport   virtual int  get_category();
      DllExport   virtual GSDataPermissionTypeEnum get_level();
      DllExport   virtual TCHAR *get_name();
      DllExport   virtual TCHAR *get_name2();
      DllExport   virtual double get_key_double();
      DllExport   virtual long  get_id();

      DllExport   virtual C_NODE *get_next();
      DllExport   virtual C_NODE *get_prev();

      DllExport   virtual int  set_key(int in);
      DllExport   virtual int  set_key(double in);
      DllExport   virtual int  set_type(int in);
      DllExport   virtual int  set_name(const TCHAR *in);
      DllExport   virtual int  set_name(const char *in);
      DllExport   virtual int  set_name2(const TCHAR *in);
      DllExport   virtual int  set_name2(const char *in);                           
};


//----------------------------------------------------------------------------//
//    class C_LIST.                                                           //
//                                                                            //
// Classe per gestire le liste di oggetti derivati da C_NODE.                 //
// prevede comandi di inserimento e cancellazione di nuovi elementi, in       //
// testa, in coda o nella posizione attuale del curcore.                      //
// In particolare e possibile eseguire ricerche per codice(int) o per         //
// nome(char*) ridefinendo le funzioni di ritorno delle chiavi di ricerca     //
// nella classe derivata da C_NODE.                                           //
//----------------------------------------------------------------------------//
class C_LIST
{
   typedef int (CALLBACK *CompareFunction)(C_NODE *p1, C_NODE *p2);

   protected :
      C_NODE *head, *cursor, *tail;
      int  count;
      CompareFunction fCompPtr;  // puntatore a funzione di comparazione

   public :            
      DllExport   C_LIST();
      DllExport   virtual ~C_LIST();
   
                  int print_list();

      // count of elements
      DllExport   int get_count() const;
      DllExport   int is_empty() const;

      // return head,tail,cursor's next or prev
      DllExport   virtual C_NODE *get_head();
      DllExport   virtual C_NODE *get_tail();
      DllExport   virtual C_NODE *get_cursor();
      DllExport   virtual C_NODE *get_next();
      DllExport   virtual C_NODE *get_prev();
      DllExport   virtual C_NODE *getptr_at(int pos);  // ordinal position (1 è il primo)
      DllExport   virtual int getpos(C_NODE *ptr);     // ordinal position (1 è il primo)
      DllExport   virtual int getpos_key(int in);      // ordinal position (1 è il primo)
      DllExport   virtual int getpos_name(const TCHAR *in, int sensitive = TRUE);  // ordinal position (1 è il primo)
      DllExport   virtual int getpos_name(const char *in, int sensitive = TRUE);  // ordinal position (1 è il primo)
      DllExport   virtual int getpos_name2(const TCHAR *in); // ordinal position (1 è il primo)
      DllExport   virtual int getpos_name2(const char *in); // ordinal position (1 è il primo)

      // add before head or after tail node or list
      DllExport   virtual int add_head(C_NODE* newnode);
      DllExport   virtual int add_tail(C_NODE* newnode);
      DllExport   virtual int paste_head(C_LIST &NewList);
      DllExport   virtual int paste_tail(C_LIST &NewList);

      // remove elements
      DllExport   virtual int remove_head();
      DllExport   virtual int remove_tail();
      DllExport   virtual int remove(C_NODE *node); // puntatore
      DllExport   virtual int remove_all();
      DllExport   virtual int remove_at();          // cursor position
      DllExport   virtual int remove_at(int pos);   // ordinal position (1 è il primo)
      DllExport   virtual int remove_key(int key);
      DllExport   virtual int remove_key(long key);
      DllExport   virtual int remove_name(const TCHAR *name, int sensitive = TRUE);
      DllExport   virtual int remove_name(const char *name, int sensitive = TRUE);
      DllExport   virtual int remove_name2(const TCHAR *name);
      DllExport   virtual int remove_name2(const char *name);
      
      // inserting before or after a given position
      DllExport   virtual int insert_before(int pos, C_NODE* newnod);
      DllExport   virtual int paste_before(int pos, C_LIST* pNewList); // ordinal position (1 è il primo)
      DllExport   virtual int insert_after (int pos, C_NODE* newnod);
      DllExport   virtual int insert_before(C_NODE* newnod); // before cursor position
      DllExport   virtual int insert_after (C_NODE* newnod); // after cursor position
      DllExport   virtual int insert_ordered(C_NODE* newnod, bool *inserted);

      // cut a list element or move list//
      DllExport   virtual C_NODE *cut_head(void);
      DllExport   virtual C_NODE *cut_tail(void);
      DllExport   virtual C_NODE *cut_key(int in);
      DllExport   virtual C_NODE *cut_at(void);     // cursor position
      DllExport   virtual int cut(C_NODE *node); // puntatore
      DllExport   virtual int move_all(C_LIST* target);

      // search a key
      DllExport   virtual C_NODE *search_key(int in);        // begin from head
      DllExport   virtual C_NODE *search_next_key(int in);   // begin from cursor
      DllExport   virtual C_NODE *search_key(double in);     // begin from head
      DllExport   virtual C_NODE *search_key(long in);     // begin from head

      DllExport   virtual C_NODE *search_type(int in);        // begin from head
      DllExport   virtual C_NODE *search_next_type(int in);   // begin from cursor

      DllExport   virtual C_NODE *search_name(const TCHAR *in, int sensitive = TRUE); // begin from head
      DllExport   virtual C_NODE *search_name(const char *in, int sensitive = TRUE); // begin from head
      DllExport   virtual C_NODE *search_next_name(const TCHAR *in, int sensitive = TRUE);// begin from cursor
      DllExport   virtual C_NODE *search_next_name(const char *in, int sensitive = TRUE);// begin from cursor

      DllExport   virtual C_NODE *search_name2(const TCHAR *in, int sensitive = TRUE);        // begin from head
      DllExport   virtual C_NODE *search_name2(const char *in, int sensitive = TRUE);        // begin from head
      DllExport   virtual C_NODE *search_next_name2(const TCHAR *in, int sensitive = TRUE);   // begin from cursor
      DllExport   virtual C_NODE *search_next_name2(const char *in, int sensitive = TRUE);   // begin from cursor
      
      DllExport   virtual C_NODE *search_name2_prefix(const TCHAR *in); // begin from head
      DllExport   virtual C_NODE *search_name2_prefix(const char *in); // begin from head

      DllExport   virtual C_NODE *search_category(int in);        // begin from head
      DllExport   virtual C_NODE *search_next_category(int in);   // begin from cursor

                          void sort(void);
      DllExport   virtual void sort_name(int sensitive = FALSE, int ascending = TRUE);
      DllExport   virtual void sort_key(int ascending = TRUE);
      DllExport   virtual void sort_nameByNum(int ascending = TRUE);
      DllExport   virtual void sort_nameByDate(int ascending = TRUE);
      DllExport   virtual void sort_id(int ascending = TRUE);
      DllExport   void Inverse(void);
      
      DllExport   virtual C_NODE *goto_pos(int in); // 1 e' il primo
      DllExport   int set_cursor(C_NODE *ptr);

      // PER TABELLE SECONDARIE
      int set_mother_class(C_NODE *p_class);
};



//----------------------------------------------------------------------------//
//    class C_INT                                                             //
//----------------------------------------------------------------------------//

class C_INT : public C_NODE
{
   friend class C_INT_LIST;

   protected :
      int key;
      DllExport virtual int comp(void *in); // usata per il sort                       

   public :
      DllExport C_INT();
      DllExport C_INT(int in);
      DllExport virtual ~C_INT();

      DllExport int get_key();
      DllExport int set_key(int in);
};
                  


//----------------------------------------------------------------------------//
//    class C_INT_LIST                                                        //
//----------------------------------------------------------------------------//

class C_INT_LIST : public C_LIST
{
   public :
      DllExport C_INT_LIST();
      DllExport virtual ~C_INT_LIST(); // chiama ~C_LIST

      DllExport int copy(C_INT_LIST *out);

      DllExport virtual resbuf *to_rb(void);
      DllExport virtual int from_rb(resbuf *rb);
      DllExport int from_str(const TCHAR *str, TCHAR Sep = _T(','));
      DllExport int from_str(const char *str, char Sep);
      DllExport TCHAR* to_str(TCHAR Sep = _T(','));
      DllExport TCHAR* to_str(char Sep);
      DllExport int add_tail_int(int Val);
      DllExport C_INT *search_min_key(void);
   
      DllExport int save(FILE *file, const TCHAR *sez);
      DllExport int save(FILE *file, const char *sez);
      DllExport int load(FILE *file, const TCHAR *sez);
      DllExport int load(FILE *file, const char *sez);
      DllExport int FromDCLMultiSel(const TCHAR *in);
      DllExport int FromDCLMultiSel(const char *in);
};


//----------------------------------------------------------------------------//
//    class C_LONG                                                            //
//----------------------------------------------------------------------------//

class C_LONG : public C_NODE
{
   friend class C_LONG_LIST;

   protected :
      long  id;
      DllExport virtual int comp(void *in); // usata per il sort                       

   public :
      DllExport C_LONG();
      DllExport C_LONG(long in) { set_id(in); }
      DllExport virtual ~C_LONG();

      DllExport long get_id();
      DllExport int  set_id(long in);
};
                  

//----------------------------------------------------------------------------//
//    class C_LONG_LIST                                                       //
//----------------------------------------------------------------------------//

class C_LONG_LIST : public C_LIST
{
   public :
      DllExport   C_LONG_LIST();
      DllExport   virtual ~C_LONG_LIST(); // chiama ~C_LIST

      DllExport   C_LONG *search(long in);
      DllExport int copy(C_LONG_LIST *out);
      DllExport bool operator == (C_LONG_LIST &in);
      DllExport int add_tail_long(long Val);
      DllExport int from_str(const TCHAR *str, TCHAR Sep = _T(','));
};


//----------------------------------------------------------------------------//
//    class C_INT_INT                                                         //
//----------------------------------------------------------------------------//

class C_INT_INT : public C_INT
{
   friend class C_INT_INT_LIST;

   protected :  
      int  type;

   public :
      DllExport C_INT_INT();
      DllExport C_INT_INT(int in1, int in2);
      DllExport virtual ~C_INT_INT();

      DllExport int get_type();
      DllExport int set_type(int in);
      DllExport int from_rb(presbuf *rb);
};


//----------------------------------------------------------------------------//
//    class C_INT_INT_LIST                                                    //
//----------------------------------------------------------------------------//

class C_INT_INT_LIST : public C_LIST
{
   public :
      DllExport C_INT_INT_LIST();
      DllExport virtual ~C_INT_INT_LIST(); // chiama ~C_LIST

      DllExport int virtual values_add_tail(int _key, int _type);
      DllExport int copy(C_INT_INT_LIST *out);

      DllExport resbuf *to_rb(void);
      DllExport int from_rb(resbuf *rb);
      DllExport int from_str(const TCHAR *str, TCHAR CoupleSep = _T(','), TCHAR Sep = _T(','));
      DllExport int from_str(const char *str, char CoupleSep = ',', char Sep = ',');
      DllExport TCHAR *to_str(TCHAR CoupleSep = _T(','), TCHAR Sep = _T(','));
      DllExport TCHAR *to_str(char CoupleSep, char Sep);

      DllExport C_INT_INT *search(int aaa, int bbb);
      int remove(int aaa, int bbb);
};


//----------------------------------------------------------------------------//
//    class C_2LONG                                                           //
//----------------------------------------------------------------------------//


class C_2LONG : public C_LONG
{
   friend class C_2LONG_LIST;

   protected :
      long  id_2;

   public :
DllExport C_2LONG();
DllExport C_2LONG(long in1, long in2);
DllExport virtual ~C_2LONG();

DllExport long get_id_2();
DllExport int set_id_2(long in);
};
//----------------------------------------------------------------------------//
//    class C_2LONG_LIST                                                  //
//----------------------------------------------------------------------------//

class C_2LONG_LIST : public C_LIST
{
   public :
      C_2LONG_LIST();
      virtual ~C_2LONG_LIST(); // chiama ~C_LIST
};


//----------------------------------------------------------------------------//
//    class C_2LONG_INT                                                       //
//----------------------------------------------------------------------------//

class C_2LONG_INT : public C_2LONG
{
   friend class C_2LONG_INT_LIST;

   protected :
      int class_id;

   public :
DllExport C_2LONG_INT();
DllExport C_2LONG_INT(long in1, long in2, int in3);
DllExport virtual ~C_2LONG_INT();

DllExport int get_class_id();
DllExport int set_class_id(int in);
      int get_key();
};


//----------------------------------------------------------------------------//
//    class C_2LONG_INT_LIST                                                  //
//----------------------------------------------------------------------------//

class C_2LONG_INT_LIST : public C_LIST
{
   public :
DllExport C_2LONG_INT_LIST();
DllExport virtual ~C_2LONG_INT_LIST(); // chiama ~C_LIST
};


//----------------------------------------------------------------------------//
//    class C_2INT_LONG                                                       //
//----------------------------------------------------------------------------//
class C_2INT_LONG : public C_INT_INT
{
   friend class C_2INT_LONG_LIST;

   protected :  
      long id;

   public :
DllExport C_2INT_LONG();
DllExport C_2INT_LONG(int aaa, int bbb, long ccc);
DllExport virtual ~C_2INT_LONG();
DllExport long get_id();
DllExport int set_id(long in);
};


//----------------------------------------------------------------------------//
//    class C_2INT_LONG_LIST                                                  //
//----------------------------------------------------------------------------//

class C_2INT_LONG_LIST : public C_LIST
{
   public :
DllExport C_2INT_LONG_LIST();
DllExport virtual ~C_2INT_LONG_LIST(); // chiama ~C_LIST
DllExport C_2INT_LONG *search(int aaa, int bbb, long ccc);
          C_2INT_LONG *search(int aaa, int bbb);
          C_2INT_LONG *search_next(int aaa, int bbb, long ccc);
};


//----------------------------------------------------------------------------//
//    class C_STR                                                             //
//----------------------------------------------------------------------------//

class C_STR : public C_NODE
{
   friend class C_STR_LIST;

   protected :  
      C_STRING name;
      DllExport virtual int comp(void *in); // usata per il sort                       

   public :
      DllExport C_STR();
      DllExport C_STR(int in);
      DllExport C_STR(const TCHAR *in);
      DllExport C_STR(const char *in);
      DllExport virtual ~C_STR();

      DllExport TCHAR *get_name();
      DllExport int  set_name(const TCHAR *in);
      DllExport int  set_name(const char *in);
                int  set_name(const TCHAR *in, int start, int end); 
                int  set_name(const char *in, int start, int end); 
                int  cat(const TCHAR *in);
                int  cat(const char *in);
                size_t  len();
      DllExport int  comp(const TCHAR *in, int sensitive = TRUE);
      DllExport int  comp(const char *in, int sensitive = TRUE);
                TCHAR *cut();
                TCHAR *paste(TCHAR *in);
            	 
            	 C_STR& operator = (int in);
      DllExport C_STR& operator += (const TCHAR *in);
      DllExport C_STR& operator += (const char *in);
                C_STR& operator += (C_STRING *in);
            	 C_STR& operator += (C_STR *in);
      DllExport virtual void clear();
      DllExport int isDigit();
                int toi();
                long tol();
};


//----------------------------------------------------------------------------//
//    class C_STR_LIST                                                        //
//----------------------------------------------------------------------------//

class C_STR_LIST : public C_LIST
{
   public :
		DllExport C_STR_LIST();
		DllExport virtual ~C_STR_LIST(); // chiama ~C_LIST

      DllExport int copy(C_STR_LIST *out);
      DllExport virtual resbuf *to_rb(void);
      DllExport virtual int from_rb(resbuf *rb);

      DllExport int from_str(const TCHAR *str, TCHAR sep = _T(','));
      DllExport int from_str(const char *str, char sep = ',');
      DllExport TCHAR *to_str(TCHAR sep = _T(','));
      DllExport TCHAR *to_str(char sep);

      DllExport int add_tail_str   (const TCHAR *Value);
                int add_tail_unique(const TCHAR *Value, int sensitive = TRUE);
                int add_tail_unique(C_STR_LIST &in, int sensitive = TRUE);
                void listIntersect(C_STR_LIST &ref_list, int sensitive = TRUE);
                void listUnion(C_STR_LIST &ref_list, int sensitive = TRUE);

      DllExport int save(const TCHAR *Path);
      DllExport int save(const char *Path);
      DllExport int load(const TCHAR *Path);
      DllExport int load(const char *Path);
      DllExport int getRecursiveList(C_STR_LIST &ClsCodeList, const TCHAR *Path = NULL);
      DllExport C_STR *getFirstTrueGEOLispCond(C_RB_LIST &ColValues);
      DllExport C_STR *getFirstTrueGEOLispCond(presbuf ColValues);
};


//----------------------------------------------------------------------------//
//    class C_INT_STR                                                         //
//----------------------------------------------------------------------------//

class C_INT_STR : public C_STR
{
   friend class C_INT_STR_LIST;
   protected:
      int key;
   public :
      DllExport C_INT_STR();
      C_INT_STR(int _key, const TCHAR *_name);
      DllExport virtual ~C_INT_STR(); // chiama ~C_STR e ~C_INT

      DllExport int get_key();
      DllExport int set_key(int in);
};


//----------------------------------------------------------------------------//
//    class C_INT_STR_LIST                                                    //
//----------------------------------------------------------------------------//

class C_INT_STR_LIST : public C_LIST
{
   public :
      DllExport C_INT_STR_LIST();
      DllExport virtual ~C_INT_STR_LIST();  // chiama ~C_LIST

      DllExport int add_tail_int_str(int _val1, const TCHAR *_val2);
      DllExport resbuf *to_rb(void);
                int from_rb(resbuf *rb);
                int from_str(const TCHAR *str, TCHAR Sep = _T(','));
};


//----------------------------------------------------------------------------//
//    class C_INT_INT_STR                                                     //
//    Classe per multi-uso (lista utenti)                                     //
//----------------------------------------------------------------------------//

class C_INT_INT_STR : public C_STR
{
   friend class C_INT_INT_STR_LIST;
   protected:
      int key;
      int type;
   public :
      DllExport C_INT_INT_STR();
      C_INT_INT_STR(int in);
      DllExport virtual ~C_INT_INT_STR(); // chiama ~C_STR

      DllExport int get_key();
      DllExport int set_key(int in);
      DllExport int get_type();
      DllExport int set_type(int in);
};


//----------------------------------------------------------------------------//
//    class C_INT_INT_STR_LIST                                                //
//----------------------------------------------------------------------------//

class C_INT_INT_STR_LIST : public C_LIST
{
   public :
      DllExport C_INT_INT_STR_LIST();
      DllExport virtual ~C_INT_INT_STR_LIST();  // chiama ~C_LIST

      DllExport resbuf *to_rb(void);
      DllExport int from_rb(resbuf *rb);
      DllExport C_INT_INT_STR *search(int aaa, int bbb);
      DllExport int copy(C_INT_INT_STR_LIST *out);
};


//----------------------------------------------------------------------------//
//    class C_2STR                                                            //
//    Classe per multi-uso                                                    //
//----------------------------------------------------------------------------//

class C_2STR : public C_STR
{
   friend class C_2STR_LIST;
   
   protected:
      C_STRING name2;
   
   public :
   DllExport C_2STR();
             C_2STR(int in1, int in2);
   DllExport C_2STR(const TCHAR *in1, const TCHAR *in2);
   DllExport C_2STR(const char *in1, const char *in2);
   DllExport virtual ~C_2STR(); // chiama ~C_STR

   DllExport TCHAR *get_name2();
   DllExport int set_name2(const TCHAR *in); 
   DllExport int set_name2(const char *in); 
   DllExport void clear();
};


//----------------------------------------------------------------------------//
//    class C_2STR_LIST                                                       //
//----------------------------------------------------------------------------//

class C_2STR_LIST : public C_LIST
{
   public :
   DllExport C_2STR_LIST();
   DllExport C_2STR_LIST(const TCHAR *in1, const TCHAR *in2);
   DllExport C_2STR_LIST(const char *in1, const char *in2);
   DllExport virtual ~C_2STR_LIST();  // chiama ~C_LIST

   DllExport int add_tail_2str(const TCHAR *in1, const TCHAR *in2);

   DllExport C_2STR *search_names(TCHAR *first, TCHAR *second, int sensitive = TRUE);
   DllExport C_2STR *search_names(char *first, char *second, int sensitive = TRUE);
   DllExport int how_many_times(TCHAR *first, int *count);
   DllExport int how_many_times(char *first, int *count);

   DllExport resbuf *to_rb(void);
   DllExport int from_rb(resbuf *rb);
   DllExport int copy(C_2STR_LIST &out);
   DllExport int save(const TCHAR *Path, TCHAR Sep = _T(','), const TCHAR *Sez = NULL);
   DllExport int save(const char *Path, char Sep = ',', const char *Sez = NULL);
             int save(FILE *f, TCHAR Sep, bool Unicode = false);
             int save(FILE *f, char Sep, bool Unicode = false);
   DllExport int load(const TCHAR *Path, TCHAR Sep = _T(','), 
                      const TCHAR *Sez = NULL, bool fromDB = false,
                      C_RB_LIST *pParamValues = NULL);
   DllExport int load(const char *Path, char Sep = ',',
                      const char *Sez = NULL, bool fromDB = false,
                      C_RB_LIST *pParamValues = NULL);
   DllExport int load(FILE *f, TCHAR Sep, const TCHAR *Sez, bool fromDB = false,
                      bool Unicode = false, C_RB_LIST *pParamValues = NULL);
   DllExport int load(FILE *f, char Sep, const char *Sez, bool fromDB = false,
                      bool Unicode = false, C_RB_LIST *pParamValues = NULL);
             int load(FILE *f, TCHAR Sep, bool fromDB = false, bool Unicode = false,
                      C_RB_LIST *pParamValues = NULL);
             int load(FILE *f, char Sep, bool fromDB = false, bool Unicode = false,
                      C_RB_LIST *pParamValues = NULL);
};

bool gsc_is_C_2STR_LIST_referred_to_DB(C_2STR_LIST &List, C_STRING &ConnStrUDLFile,
                                       C_STRING &UdlProperties, C_STRING &SelectStm);
DllExport int gsc_C_2STR_LIST_load(C_2STR_LIST &List, C_STRING &ConnStrUDLFile, 
                                   C_STRING &UdlPropertiesStr, C_STRING &SelectStm,
                                   C_RB_LIST *pParamValues = NULL);
DllExport int gsc_C_2STR_LIST_load(C_2STR_LIST &List, C_STRING &ConnStrUDLFile, 
                                   C_STRING &UdlPropertiesStr, C_STRING &SelectStm,
                                   presbuf pParamValues = NULL);
C_2STR *gsc_C_2STR_LIST_load_by_code(FILE *f, const TCHAR *Code,
                                     TCHAR Sep = _T(','), const TCHAR *Sez = NULL,
                                     bool fromDB = false, bool Unicode = false,
                                     C_RB_LIST *pParamValues = NULL);
C_2STR *gsc_C_2STR_LIST_load_by_code(FILE *f, const TCHAR *Code,
                                     TCHAR Sep = _T(','), const TCHAR *Sez = NULL,
                                     bool fromDB = false, bool Unicode = false,
                                     presbuf pParamValues = NULL);
C_2STR *gsc_C_2STR_LIST_load_by_descr(FILE *f, const TCHAR *Descr,
                                      TCHAR Sep = _T(','), const TCHAR *Sez = NULL,
                                      bool fromDB = false, bool Unicode = false,
                                      C_RB_LIST *pParamValues = NULL);


//----------------------------------------------------------------------------//
//    class C_4INT_STR                                                        //
//    Classe per multi-uso (usata in gs_class)                                //
//----------------------------------------------------------------------------//

class C_4INT_STR : public C_INT_INT_STR
{
   friend class C_4INT_STR_LIST;
   protected:
      int category;
      GSDataPermissionTypeEnum level;
      C_LIST sub_list;     // Lista di SUB-CLASS 
            
   public :
      DllExport C_4INT_STR();
      DllExport C_4INT_STR(int in);
      DllExport virtual ~C_4INT_STR(); // chiama ~C_INT_INT_STR

      DllExport int get_category();
      DllExport int set_category(int in);
      DllExport GSDataPermissionTypeEnum get_level();
      DllExport int set_level(GSDataPermissionTypeEnum in);
      DllExport C_LIST *ptr_sub_list();
};


//----------------------------------------------------------------------------//
//    class C_4INT_STR_LIST                                                   //
//----------------------------------------------------------------------------//

class C_4INT_STR_LIST : public C_LIST
{
   public :
      DllExport C_4INT_STR_LIST();
      DllExport virtual ~C_4INT_STR_LIST();  // chiama ~C_LIST

                resbuf *to_rb(void);
                int from_rb(resbuf *rb);
                int count_upd();   // conto quante sono quelle modificabili

      DllExport int copy(C_4INT_STR_LIST *out);

      DllExport C_4INT_STR *search(int aaa, int bbb, int ccc);
};
                                           

class C_FAMILY;
//----------------------------------------------------------------------------//
//    class C_FAMILY_LIST                                                     //
//----------------------------------------------------------------------------//
class C_FAMILY_LIST : public C_INT_LIST
{
   public :
      DllExport C_FAMILY_LIST();
      DllExport virtual ~C_FAMILY_LIST(); // chiama ~C_LIST
      
      DllExport C_FAMILY *search_list(int in);

      resbuf *to_rb(void);
      int from_rb(resbuf *rb);

      DllExport int add_tail_int(int Val);
      DllExport int copy(C_FAMILY_LIST &out);
};


//----------------------------------------------------------------------------//
//    class C_FAMILY                                                          //
//    Classe per memorizzare gruppi di classi collegate                       //
//----------------------------------------------------------------------------//
class C_FAMILY : public C_INT
{
   friend class C_FAMILY_LIST;
   
   public :
      C_FAMILY_LIST relation;
   
      C_FAMILY();
      DllExport C_FAMILY(int in);
      virtual ~C_FAMILY();
};


//---------------------------------------------------------------------------//
//    class C_BIRELATION                                                     //
//    Classe per informazioni relazioni tra classi gruppo                    //
//---------------------------------------------------------------------------//

class C_BIRELATION : public C_INT
{
   friend class C_BIRELATION_LIST;
   
   public :
      C_INT_INT_LIST relation;
   
DllExport C_BIRELATION();
DllExport virtual ~C_BIRELATION();   // chiama ~C_INT //
};


//----------------------------------------------------------------------------//
//    class C_BIRELATION_LIST                                                 //
//----------------------------------------------------------------------------//

class C_BIRELATION_LIST : public C_LIST
{
   public :
DllExport C_BIRELATION_LIST();
DllExport virtual ~C_BIRELATION_LIST(); // chiama ~C_LIST

      resbuf *to_rb(void);
      int from_rb(resbuf *rb);
};


//----------------------------------------------------------------------------//
//    class C_CLS_PUNT                                                        //
//    Classe per multi-uso (lista classi selezionate)                         //
//----------------------------------------------------------------------------//
class C_CLS_PUNT : public C_NODE
{
   friend class C_CLS_PUNT_LIST;
   protected:
   public :
      C_NODE   *cls;
      ads_name ent; 
      long     gs_id;

DllExport C_CLS_PUNT(C_NODE *_cls = NULL, ads_name _ent = NULL, long _gs_id = 0);
DllExport C_CLS_PUNT(ads_name _ent);
DllExport C_CLS_PUNT(C_CLS_PUNT *p);
      int get_key();                 // class code
      TCHAR *get_name();              // class name
DllExport int get_ent(ads_name out); // graphical entity
DllExport long get_gs_id();          // table link key value
      int gs_id_initEnt(void);       // dal nome dell'entità ricava il valore chiave
DllExport int copy(C_CLS_PUNT *out);
      int from_ent(ads_name _ent);
DllExport C_NODE *get_class(void);

      virtual ~C_CLS_PUNT(); // chiama ~C_NODE
};


//----------------------------------------------------------------------------//
//    class C_CLS_PUNT_LIST                                                   //
//----------------------------------------------------------------------------//

class C_CLS_PUNT_LIST : public C_LIST
{
   public :
      DllExport C_CLS_PUNT_LIST();
      DllExport virtual ~C_CLS_PUNT_LIST();  // chiama ~C_LIST

      C_CLS_PUNT *search_ent(ads_name in);
      C_CLS_PUNT *search_next_ent(ads_name in);
      C_CLS_PUNT *search_ClsKey(C_NODE *pCls, long key);
      C_CLS_PUNT *search_next_ClsKey(C_NODE *pCls, long key);
      C_CLS_PUNT *search_Cls(C_NODE *pCls);
      C_CLS_PUNT *search_Cls(int cls, int sub);
      int copy(C_CLS_PUNT_LIST &out);
      int to_ssgroup(ads_name sss);
      int from_ss(ads_name ss);
      int gs_id_initEnt(void);
      int get_TopoLinkInfo(int *LinkSub, long *LinkID,            // La lista deve avere
                           int *InitNodeSub, long *InitNodeID,    // 3 elementi: link,
                           int *FinalNodeSub, long *FinalNodeID); // nodo iniziale, nodo finale
      int to_C_2INT_LONG_LIST(C_2INT_LONG_LIST &out);
};


//----------------------------------------------------------------------------//
//    class C_ENT_FAMILY                                                      //
//    Classe per memorizzare gruppi di entita' collegate                      //
//----------------------------------------------------------------------------//

class C_ENT_FAMILY : public C_NODE
{
   friend class C_ENT_FAMILY_LIST;
   
   public :
      C_CLS_PUNT_LIST family;
	   long            id;
   
      C_ENT_FAMILY();
      virtual ~C_ENT_FAMILY();   // chiama ~C_NODE

      long get_id();
      int set_id(long in);

	   int evid(int mode, int *AllOnVideo=NULL);
      presbuf to_rb(void);
};


//----------------------------------------------------------------------------//
//    class C_ENT_FAMILY_LIST                                                 //
//----------------------------------------------------------------------------//

class C_ENT_FAMILY_LIST : public C_LIST
{
   public :
      C_ENT_FAMILY_LIST();
      virtual ~C_ENT_FAMILY_LIST(); // chiama ~C_LIST

      presbuf to_rb(void);
};


//----------------------------------------------------------------------------//
//    class C_INT_LONG                                                       //
//----------------------------------------------------------------------------//

class C_INT_LONG : public C_INT
{
   friend class C_INT_LONG_LIST;

   protected :
      long n_long;

   public :
      DllExport   C_INT_LONG();
      DllExport   C_INT_LONG(int in1, long in2);
      DllExport   virtual ~C_INT_LONG();

      DllExport   long get_id();
      DllExport   int set_id(long in);
};


//----------------------------------------------------------------------------//
//    class C_INT_LONG_LIST                                                   //
//----------------------------------------------------------------------------//

class C_INT_LONG_LIST : public C_LIST
{
   public :
      DllExport C_INT_LONG_LIST();
      DllExport virtual ~C_INT_LONG_LIST(); // chiama ~C_LIST

      DllExport int copy(C_INT_LONG_LIST *out);
      DllExport int add_tail_int_long(int in1, long in2);
      DllExport C_INT_LONG *search(int in1, long in2);
};


//----------------------------------------------------------------------------//
//    class C_INT_VOIDPTR e C_INT_VOIDPTR_LIST                                //
//    Classe per memorizzare l'ordine di elementi esterni alla lista          //
//    la classe memorizza solo il puntatore a questi elementi e quindi        //
//    non si occupa della loro allocazione/rilascio                           //
//----------------------------------------------------------------------------//

class C_INT_VOIDPTR : public C_INT
{
   friend class C_INT_VOIDPTR_LIST;

   protected :
      void *VoidPtr;

   public :
      DllExport C_INT_VOIDPTR();
      DllExport C_INT_VOIDPTR(int in, void *in2);
      DllExport virtual ~C_INT_VOIDPTR();

      DllExport void *get_VoidPtr();
      DllExport int set_VoidPtr(void *in);
};


//----------------------------------------------------------------------------//
//    class C_INT_VOIDPTR_LIST                                                //
//----------------------------------------------------------------------------//

class C_INT_VOIDPTR_LIST : public C_LIST
{
   public :
      DllExport C_INT_VOIDPTR_LIST();
      DllExport virtual ~C_INT_VOIDPTR_LIST(); // chiama ~C_LIST

      DllExport int copy(C_INT_VOIDPTR_LIST *out);
      DllExport int add_tail_int_voidptr(int in1, void *in2);
};


//----------------------------------------------------------------------------//
//    class C_REAL                                                             //
//----------------------------------------------------------------------------//


class C_REAL : public C_NODE
{
   friend class C_REAL_LIST;

   protected :
      double key;
      DllExport virtual int comp(void *in); // usata per il sort                       

   public :
      DllExport C_REAL();
      DllExport C_REAL(double in);
      DllExport virtual ~C_REAL();

      DllExport double get_key_double();
      DllExport int    set_key(double in);
      DllExport resbuf *to_rb(void);
};


//----------------------------------------------------------------------------//
//    class C_REAL_LIST                                                       //
//----------------------------------------------------------------------------//


class C_REAL_LIST : public C_LIST
{
   public :
      DllExport C_REAL_LIST();
      DllExport virtual ~C_REAL_LIST(); // chiama ~C_LIST

      DllExport void sort_key(int ascending = TRUE);
      DllExport int copy(C_REAL_LIST *out);
      DllExport int insert_ordered_double(double Val, bool *inserted = NULL);
      DllExport resbuf *to_rb(void);
      DllExport int from_rb(resbuf *rb);
      DllExport int getpos_key_double(double in);      // ordinal position (1 è il primo)
      DllExport int add_tail_double(double Val);
};


//----------------------------------------------------------------------------//
//    class C_REAL_VOIDPTR e C_REAL_VOIDPTR_LIST                              //
//    Classe per memorizzare l'ordine di elementi esterni alla lista          //
//    la classe memorizza solo il puntatore a questi elementi e quindi        //
//    non si occupa della loro allocazione/rilascio                           //
//----------------------------------------------------------------------------//

class C_REAL_VOIDPTR : public C_REAL
{
   friend class C_REAL_VOIDPTR_LIST;

   protected :
      void *VoidPtr;

   public :
      DllExport C_REAL_VOIDPTR();
      DllExport C_REAL_VOIDPTR(double in, void *in2);
      DllExport virtual ~C_REAL_VOIDPTR();

      DllExport void *get_VoidPtr();
      DllExport int set_VoidPtr(void *in);
};


//----------------------------------------------------------------------------//
//    class C_REAL_VOIDPTR_LIST                                               //
//----------------------------------------------------------------------------//

class C_REAL_VOIDPTR_LIST : public C_LIST
{
   public :
      DllExport C_REAL_VOIDPTR_LIST();
      DllExport virtual ~C_REAL_VOIDPTR_LIST(); // chiama ~C_LIST

      DllExport int copy(C_REAL_VOIDPTR_LIST *out);
      DllExport int add_tail_double_voidptr(double in1, void *in2);
      DllExport int insert_ordered_double_voidptr(double in1, void *in2, bool *inserted = NULL);
};


//----------------------------------------------------------------------------//
//    class C_2REAL                                                           //
//----------------------------------------------------------------------------//


class C_2REAL : public C_REAL
{
   friend class C_2REAL_LIST;

   protected :
      double key_2;

   public :
      C_2REAL();
      C_2REAL(double in1, double in2);
      virtual ~C_2REAL();

      double get_key_2_double();
      int set_key_2(double in);
      void copy(C_2REAL *out);
};


//----------------------------------------------------------------------------//
//    class C_2REAL_LIST                                                      //
//----------------------------------------------------------------------------//


class C_2REAL_LIST : public C_LIST
{
   public :
      C_2REAL_LIST();
      virtual ~C_2REAL_LIST(); // chiama ~C_LIST

      void sort_key(int ascending = TRUE);
      int add_tail_2double(double in1, double in2);
      int insert_ordered_2double(double Val1, double Val2, bool *inserted = NULL);
};


//----------------------------------------------------------------------------//
//    class C_2STR_INT                                                        //
//    Classe per multi-uso                                                    //
//----------------------------------------------------------------------------//
class C_2STR_INT : public C_2STR
{
   friend class C_2STR_INT_LIST;
   
   protected:
      int	 type;
   
   public :
      DllExport C_2STR_INT();
      DllExport C_2STR_INT(const TCHAR *in1, const TCHAR *in2, int in3);
      DllExport C_2STR_INT(const char *in1, const char *in2, int in3);
      DllExport virtual ~C_2STR_INT(); // chiama ~C_STR

      DllExport int  get_type();
      DllExport int  set_type(int in);
};


//----------------------------------------------------------------------------//
//    class C_2STR_LIST                                                       //
//----------------------------------------------------------------------------//

class C_2STR_INT_LIST : public C_LIST
{
   public :
      DllExport C_2STR_INT_LIST();
      DllExport virtual ~C_2STR_INT_LIST();  // chiama ~C_LIST
      
      DllExport int copy(C_2STR_INT_LIST *out);
      DllExport int save(C_STRING &Path, TCHAR Sep = _T(','), const TCHAR *Sez = NULL);
      DllExport int load(C_STRING &Path, TCHAR Sep = _T(','), const TCHAR *Sez = NULL);
      DllExport resbuf *to_rb(void);
      DllExport int from_rb(resbuf *rb);
};


//----------------------------------------------------------------------------//
//    class C_2LONG_STR                                                     //
//    Classe per multi-uso                                                    //
//----------------------------------------------------------------------------//
class C_2LONG_STR : public C_2LONG
{
   friend class C_2LONG_STR_LIST;
   
   protected:
      C_STRING name;
   
   public :
      DllExport C_2LONG_STR();
      DllExport virtual ~C_2LONG_STR();

      DllExport TCHAR *get_name();
      DllExport int set_name(const TCHAR *in);
      DllExport int set_name(const char *in);
};


//----------------------------------------------------------------------------//
//    class C_2LONG_STR_LIST                                                  //
//----------------------------------------------------------------------------//
class C_2LONG_STR_LIST : public C_LIST
{
   public :
      DllExport C_2LONG_STR_LIST();
      DllExport virtual ~C_2LONG_STR_LIST();  // chiama ~C_LIST
};

//----------------------------------------------------------------------------//
//    class C_LONG_REAL                                                       //
//----------------------------------------------------------------------------//
class C_LONG_REAL : public C_LONG
{
   friend class C_LONG_REAL_LIST;

   protected :
      double Num;

   public :
      DllExport C_LONG_REAL();
      DllExport C_LONG_REAL(long in1, double in2) { set_id(in1); set_real(in2); }
      DllExport virtual ~C_LONG_REAL();

      DllExport double get_real();
      DllExport int  set_real(double in);
};
                  

//----------------------------------------------------------------------------//
//    class C_LONG_REAL_LIST                                                  //
//----------------------------------------------------------------------------//
class C_LONG_REAL_LIST : public C_LIST
{
   public :
      DllExport C_LONG_REAL_LIST();
      DllExport virtual ~C_LONG_REAL_LIST(); // chiama ~C_LIST

      DllExport C_LONG_REAL *search(long in);
};


//----------------------------------------------------------------------------//
// class C_BNODE.                                                             //
//                                                                            //
// CLASSE BASE da cui derivare oggetti da organizzzare in liste di tipo       //
// C_BTREE (albero binario). I dati della classe si limitano a                //
// puntatori ad altri oggetti di tipo C_BNODE.                                //
//----------------------------------------------------------------------------//
class C_BNODE
{
   friend class C_BTREE;
   
   protected :
      // Puntatore al padre
      C_BNODE *prev;
      // Puntatori ai figli destro (maggiore) e sinistro (minore)
      C_BNODE *next_sx, *next_dx;
      // Coefficiente di bilanciamento definito come:
      // altezza del sottoalbero destro meno quella del sottoalbero sinistro
      short   BalanceCoeff; 
   public :
      DllExport C_BNODE();
                C_BNODE(C_BNODE *p, C_BNODE *n_sx,  C_BNODE *n_dx);
      DllExport virtual ~C_BNODE();
      // funzione che stabilisce l'ordine degli elementi (come gsc_strcmp)
      virtual int compare(const void *in);
      // funzione che stampa
      virtual void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_BTREE.                                                             //
//                                                                            //
// Classe per gestire alberi binari di oggetti derivati da C_BNODE.           //
//----------------------------------------------------------------------------//
class C_BTREE
{
   protected :
      C_BNODE *root, *cursor;
      long    count;

   private :
      // funzione che cancella l'albero a partire da un elemento
      void clear(C_BNODE *p);
      // funzione che stampa
      void print(C_BNODE *p, int *level = NULL);
      // calcolo della massima profondità (per bilanciamento)
      long max_deep(C_BNODE *p, long i = 0);
      // rotazione semplice a dx (per bilanciamento)
      void rot_dx(C_BNODE *p);
      // rotazione semplice a sx (per bilanciamento)
      void rot_sx(C_BNODE *p);
      // Inversione della posizione di 2 nodi
      void invert_nodes(C_BNODE *p1, C_BNODE *p2);
      // Aggiorna i fattori di bilanciamento partendo dal nodo p
      void upd_balance_coeff(C_BNODE *p, bool OnAdd, C_BNODE **pMaxDeepUnbalanceNode = NULL);
      // bilanciamento partendo dal nodo p
      bool balance(C_BNODE *p, bool *IsHeightUpdated = NULL);

   public :            
      DllExport C_BTREE();
      DllExport virtual ~C_BTREE();
   
      // conteggio elementi
      DllExport long get_count();

      // return min, max, next, prev or cursor
      DllExport C_BNODE *go_top(C_BNODE *p = NULL);
      DllExport C_BNODE *go_bottom(C_BNODE *p = NULL);
      DllExport C_BNODE *go_next(void);
      DllExport C_BNODE *go_prev(void);
      DllExport C_BNODE *get_cursor(void);
      int set_cursor(C_BNODE *ptr);

      // aggiunge elemento allocando memoria (viene fatta una copia di "in")
      DllExport int add(const void *in);

      // cerca un elemento
      DllExport C_BNODE* search(const void *in);

      // cancella elemento
      DllExport int remove(const void *in);
      // cancella elemento corrente
      DllExport int remove_at(void);
      // cancella tutti gli elementi
      DllExport void remove_all(void);

      // alloca un elemento
      DllExport virtual C_BNODE* alloc_item(const void *in);

      void print(void);
};


//----------------------------------------------------------------------------//
// class C_BSTR.                                                              //
//----------------------------------------------------------------------------//
class C_BSTR : public C_BNODE
{
   protected :  
      C_STRING name;

   public :
      DllExport C_BSTR();
      DllExport C_BSTR(const TCHAR *in);
      DllExport C_BSTR(const char *in);
      DllExport virtual ~C_BSTR();

      DllExport TCHAR *get_name();
      DllExport int  set_name(const TCHAR *in);
      DllExport int  set_name(const char *in);

      // funzione che stabilisce l'ordine degli elementi (come gsc_strcmp)
      DllExport int compare(const void *in);
     
      // funzione che stampa
      virtual void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_STR_BTREE.                                                         //
//                                                                            //
// Classe per gestire alberi binari di stringhe.                              //
//----------------------------------------------------------------------------//
class C_STR_BTREE : public C_BTREE
{
   public :
      DllExport C_STR_BTREE();
      DllExport virtual ~C_STR_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//----------------------------------------------------------------------------//
// class C_B2STR  coppie di stringhe.                                         //
//----------------------------------------------------------------------------//
class C_B2STR : public C_BSTR
{
   protected :  
      C_STRING name2;

   public :
      DllExport C_B2STR();
      DllExport C_B2STR(const TCHAR *in, const TCHAR *in2);
      DllExport virtual ~C_B2STR();

      DllExport TCHAR *get_name2();
      DllExport int  set_name2(const TCHAR *in);
    
      // funzione che stampa
      void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_2STR_BTREE.                                                        //
//                                                                            //
// Classe per gestire alberi binari di coppie di stringhe.                    //
//----------------------------------------------------------------------------//
class C_2STR_BTREE : public C_BTREE
{
   public :
      DllExport C_2STR_BTREE();
      DllExport virtual ~C_2STR_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//----------------------------------------------------------------------------//
// class C_PROFILE_SECTION                                                    //
//----------------------------------------------------------------------------//
class C_BPROFILE_SECTION : public C_BSTR
{
   protected :  
      C_2STR_BTREE EntryList;

   public :
      DllExport C_BPROFILE_SECTION();
      DllExport C_BPROFILE_SECTION(const TCHAR *in);
      DllExport virtual ~C_BPROFILE_SECTION();

      DllExport C_2STR_BTREE *get_ptr_EntryList(void);
      // setta una entry
      DllExport int set_entry(const TCHAR *entry, const TCHAR *value);
      DllExport int set_entry(const TCHAR *entry, double value);
      DllExport int set_entry(const TCHAR *entry, int value);
      DllExport int set_entry(const TCHAR *entry, long value);
      DllExport int set_entry(const TCHAR *entry, bool value);
      // legge una entry
      DllExport int get_entry(const TCHAR *entry, C_STRING &value);
      DllExport int get_entry(const TCHAR *entry, double *value);
      DllExport int get_entry(const TCHAR *entry, int *value);
      DllExport int get_entry(const TCHAR *entry, long *value);
      DllExport int get_entry(const TCHAR *entry, bool *value);
      // setta una lista di entries
      DllExport int set_entryList(C_2STR_LIST &EntryValueList);

      // funzione che stampa
      void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_PROFILE_SECTION_BTREE.                                             //
//                                                                            //
// Classe per gestire alberi binari di coppie di stringhe.                    //
//----------------------------------------------------------------------------//
class C_PROFILE_SECTION_BTREE : public C_BTREE
{
   public :
      DllExport C_PROFILE_SECTION_BTREE();
      DllExport virtual ~C_PROFILE_SECTION_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
      // cerca una entry
      DllExport C_B2STR* get_entry(const void *section, const void *entry);
      // setta una entry
      DllExport int set_entry(const TCHAR *section, const TCHAR *entry, const TCHAR *value);
      DllExport int set_entry(const TCHAR *section, const TCHAR *entry, double value);
      DllExport int set_entry(const TCHAR *section, const TCHAR *entry, int value);
      DllExport int set_entry(const TCHAR *section, const TCHAR *entry, long value);
      DllExport int set_entry(const TCHAR *section, const TCHAR *entry, bool value);
      // setta una lista di entries
      DllExport int set_entryList(const TCHAR *section, C_2STR_LIST &EntryValueList);
};

//----------------------------------------------------------------------------//
// class C_BLONG.                                                             //
//----------------------------------------------------------------------------//
class C_BLONG : public C_BNODE
{
   protected :  
      long key;

   public :
      DllExport C_BLONG();
      DllExport C_BLONG(long in);
      DllExport virtual ~C_BLONG();

      DllExport long get_key();
      DllExport int  set_key(long in);

      // funzione che stabilisce l'ordine degli elementi (come gsc_strcmp)
      DllExport int compare(const void *in);
     
      // funzione che stampa
      void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_LONG_BTREE.                                                        //
//                                                                            //
// Classe per gestire alberi binari di numeri interi long.                    //
//----------------------------------------------------------------------------//
class C_LONG_BTREE : public C_BTREE
{
   public :
      DllExport C_LONG_BTREE();
      DllExport virtual ~C_LONG_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
      void subtract(C_LONG_BTREE &in);
      void intersect(C_LONG_BTREE &in);
      DllExport int add_list(C_LONG_BTREE &in);
      DllExport int C_LONG_BTREE::from_rb(presbuf p);
      resbuf *to_rb(void);
};


//----------------------------------------------------------------------------//
// class C_BREAL.                                                             //
//----------------------------------------------------------------------------//
class C_BREAL : public C_BNODE
{
   protected :  
      double key;

   public :
      DllExport C_BREAL();
      DllExport C_BREAL(double in);
      DllExport virtual ~C_BREAL();

      DllExport double get_key();
      DllExport int  set_key(double in);

      // funzione che stabilisce l'ordine degli elementi (come gsc_strcmp)
      DllExport int compare(const void *in);
     
      // funzione che stampa
      void print(int *level = NULL);
};


//----------------------------------------------------------------------------//
// class C_REAL_BTREE.                                                        //
//                                                                            //
// Classe per gestire alberi binari di numeri reali.                          //
//----------------------------------------------------------------------------//
class C_REAL_BTREE : public C_BTREE
{
   public :
      DllExport C_REAL_BTREE();
      DllExport virtual ~C_REAL_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//---------------------------------------------------------------------------//
// MEMORIZZA UN B-ALBERO DEGLI HANDLE DEGLI OGGETTI CREATI DA DB E,          //
// PER CIASCUNO DI QUESTI, UN CODICE NUMERICO CHIAVE DELLA TABELLA DB        //
//---------------------------------------------------------------------------//
class C_BSTR_REAL : public C_BSTR
{
   protected :  
      double key;

   public :
      DllExport C_BSTR_REAL();
      DllExport C_BSTR_REAL(const TCHAR *in1);
      DllExport virtual ~C_BSTR_REAL();

      DllExport double get_key(void);
      DllExport int set_key(double in);

      // funzione che stampa
      void print(int *level = NULL);
};


class C_STR_REAL_BTREE : public C_STR_BTREE
{
   public :
      DllExport C_STR_REAL_BTREE();
      DllExport virtual ~C_STR_REAL_BTREE();  // chiama ~C_STR_REAL_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//---------------------------------------------------------------------------//
// MEMORIZZA UN B-ALBERO DI OGGETTI INDICIZZATI PER LONG                     //
// CONTENENTI ANCHE UN INT                                                   //
//---------------------------------------------------------------------------//
class C_BLONG_INT : public C_BLONG
{
   protected :  
      int m_int;

   public :
      DllExport C_BLONG_INT();
      DllExport C_BLONG_INT(long in1);
      DllExport virtual ~C_BLONG_INT();

      DllExport int get_int(void);
      DllExport int set_int(int in);

      // funzione che stampa
      void print(int *level = NULL);
};


class C_LONG_INT_BTREE : public C_LONG_BTREE
{
   public :
      DllExport C_LONG_INT_BTREE();
      DllExport virtual ~C_LONG_INT_BTREE();  // chiama ~C_LONG_INT_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
};


//----------------------------------------------------------------------------//
// class C_POINT.                                                             //
//                                                                            //
// Classe per gestire punti.                                                  //
//----------------------------------------------------------------------------//
class C_POINT : public C_NODE
{
   friend class C_POINT_LIST;
   protected :

   public :
   ads_point point;
   double    Bulge;  // tang di 1/4 angolo interno (negativo se il senso è orario)
                     // vale 0.0 se segmento rettilineo

   DllExport C_POINT();
   DllExport C_POINT(ads_point in, double _bulge = 0.0);
   C_POINT(double _X, double _Y, double _Z = 0.0, double _bulge = 0.0, int Prec = -1);
   C_POINT(const TCHAR *Str);
   C_POINT(const char *Str);
   C_POINT(C_POINT &in);
   DllExport virtual ~C_POINT();
   
   DllExport void Set(double _X, double _Y, double _Z = 0.0, double _bulge = 0.0, int Prec = -1);
   DllExport void Set(C_POINT &in);
   DllExport void Set(ads_point in, double _bulge = 0.0);

   void toStr(C_STRING &Str, GeomDimensionEnum geom_dim = GS_2D,
              TCHAR CoordSep = _T(','));
   int fromStr(const TCHAR *Str, TCHAR Sep = _T(','), int *nRead = NULL);
   int fromStr(const char *Str);

   DllExport double x(void);
   DllExport double y(void);
   DllExport double z(void);

   DllExport bool operator == (C_POINT &in);
   DllExport bool operator != (C_POINT &in);
};


#ifndef _gs_thm_h
   #include "gs_thm.h" 
#endif


//-------------------------------------------------------------------------//
// class C_POINT.                                                             //
//                                                                            //
// Classe per gestire una lista di punti.                                     //
//----------------------------------------------------------------------------//
class C_POINT_LIST : public C_LIST
{
   public :
   DllExport C_POINT_LIST() : C_LIST() { } 
   DllExport virtual ~C_POINT_LIST(){ }  // chiama ~C_LIST

   // cerca un elemento
   C_POINT *search(ads_point point, double Tolerance = 0.0);
   C_POINT *get_nearest(ads_point point);

   DllExport int copy(C_POINT_LIST &out);
   int add_point(ads_point point, double bulge = 0.0);
   DllExport int add_point(double _X, double _Y, double _Z, double bulge = 0.0);
   int add_connect_point_list(ads_name ent, int type, int Cls = 0, int Sub = 0);
   int add_intersectWith(ads_name ent1, ads_name ent2, int Dec = -1);
   int add_intersectWith(AcDbEntity *pEnt1, AcDbEntity *pEnt2, int Dec = -1);
   int add_intersectWithArc(ads_name ent, ads_point center,
                            double radius, double startAngle, double endAngle, int Dec = -1);
   int add_intersectWithArc(AcDbEntity *pEnt, ads_point center,
                            double radius, double startAngle, double endAngle, int Dec = -1);
   int add_intersectWithLine(ads_name ent, ads_point startPt, ads_point endPt, int Dec = -1);
   int add_intersectWithLine(AcDbEntity *pEnt, ads_point startPt, ads_point endPt, int Dec = -1);
   int add_ins_point(ads_name ent);
   int add_vertex_point(ads_name ent, int IncludeFirstLast = GS_BAD,
                        double MaxTolerance2ApproxCurve = 1.0, int MaxSegmentNum2ApproxCurve = 50);
   int add_vertex_point(AcDbEntity *pEnt, int IncludeFirstLast = GS_BAD,
                        double MaxTolerance2ApproxCurve = 1.0, int MaxSegmentNum2ApproxCurve = 50);
   int add_vertex_point(presbuf *pRb, int IncludeFirstLast = GS_BAD,
                        double MaxTolerance2ApproxCurve = 1.0, int MaxSegmentNum2ApproxCurve = 50);
      int add_vertex_point(AcGePoint2dArray &vertices, AcGeDoubleArray &bulges);
   int add_vertex_point_from_hatch(AcDbHatch *pHatch, int iLoop,
                                   double MaxTolerance2ApproxCurve = 1.0, int MaxSegmentNum2ApproxCurve = 50);
   int add_sten_point(ads_name ent);
   int add_middle_points(ads_name ent);
   int add_centroid_point(ads_name ent);
   resbuf *to_rb(bool bulge = false);
   int getElevation(double *Elev);
   double getLineLen(void);
   DllExport int getWindow(ads_point ptBottomLeft, ads_point ptTopRight);
   DllExport int fromStr(const TCHAR *Str, GeomDimensionEnum geom_dim = GS_2D,
                         bool BulgeWith = false, TCHAR PtSep = _T(';'), TCHAR CoordSep = _T(','),
                         int *nRead = NULL);
   int toStr(C_STRING &Str, GeomDimensionEnum geom_dim = GS_2D,
             bool BulgeWith = false, TCHAR PtSep = _T(';'), TCHAR CoordSep = _T(','),
             double MaxTolerance2ApproxCurve = 1.0, int MaxSegmentNum2ApproxCurve = 50);
   int toAcGeArray(AcGePoint2dArray &vertices, AcGeDoubleArray &bulges);
   AcDbCurve *toPolyline(const TCHAR *Layer = NULL, C_COLOR *color = NULL, 
                         const TCHAR *LineType = NULL, double Scale = -1, 
                         double Width = -1, double Thickness = -1, bool *Plinegen = NULL);
   void translation(double OffSet_X, double OffSet_Y, double OffSet_Z = 0.0);
   int OffSet(double in);

   DllExport int Draw(const TCHAR *Layer = NULL, C_COLOR *Color = NULL, int Mode = EXTRACTION);
};


//----------------------------------------------------------------------------//
// class C_RECT                                                               //
// Classe per gestire un rettangolo.                                          //
//----------------------------------------------------------------------------//
class C_RECT : public C_NODE
{
   friend class C_RECT_LIST;

   public :
   C_POINT BottomLeft;
   C_POINT TopRight;

	// from ads_point
	DllExport C_RECT();
	DllExport C_RECT(double Xmin, double Ymin, double Xmax, double Ymax);
	DllExport C_RECT(ads_point pt1, ads_point pt2);
	DllExport C_RECT(const char *Str);
	DllExport C_RECT(const TCHAR *Str);

   DllExport void Set(ads_point pt1, ads_point pt2);
   DllExport void toStr(C_STRING &Str);
   DllExport void fromStr(const char *Str);
   DllExport void fromStr(const TCHAR *Str);
   void Not(C_RECT &in, C_RECT_LIST &out);
   DllExport void Not(C_RECT_LIST &in, C_RECT_LIST &out);
   DllExport int Intersect(C_RECT &in);
   DllExport int In(C_RECT &in);
   DllExport int Contains(ads_point _pt, int Mode = CROSSING);
   DllExport double Bottom();
   DllExport double Left();
   DllExport double Top();
   DllExport double Right();
   DllExport void Center(C_POINT &pt);
   DllExport double Width();
   DllExport double Height();
   DllExport bool operator == (C_RECT &in);
   DllExport bool operator != (C_RECT &in);

   presbuf toAdeQryCondRB(void);
   DllExport int Draw(const TCHAR *Layer = NULL, C_COLOR *Color = NULL,
                      int Mode = EXTRACTION, const TCHAR *Lbl = NULL,
                      AcDbBlockTableRecord *pBlockTableRecord = NULL);

   DllExport int get_ExtractedGridClsCodeList(C_INT_LIST &ClsList);
};

//----------------------------------------------------------------------------//
//    class C_RECT_LIST                                                        //
//----------------------------------------------------------------------------//
class C_RECT_LIST : public C_LIST
{
   public :

   DllExport int AdeQryDefine(void);
};


//----------------------------------------------------------------------------//
//  class C_BOOL_MATRIX.                                                      //
//                                                                            //
// Classe per gestire le matrici di booleni allocate dinamicamente.           //
//----------------------------------------------------------------------------//
class C_BOOL_MATRIX
{
   protected :
      bool *Matrix;     // Matrice di booleni
      long nx;          // numero di colonne della matrice
      long ny;          // numero di righe della matrice

   public :            
      C_BOOL_MATRIX();
      C_BOOL_MATRIX(long nCols, long nRows);
      virtual ~C_BOOL_MATRIX();

      long get_nCols();
      long get_nRows();
      int alloc(long nCols, long nRows);
      bool get(long Col, long Row); // get value     
      void set(long Col, long Row, bool val); // set value
      void setColumn(long Col, bool val); // set all column values
      void setRow(long Row, bool val); // set all row values
      void init(bool val); // set all values

      void set1DArray(long Pos, bool val); // set value in the Array
      bool get1DArray(long Pos);           // get value in the Array
      void getColRowFrom1DArray(long Pos, long *Col, long *Row); // get matrix cell from a position in the Array
};


//----------------------------------------------------------------------------//
//  class C_DBL_MATRIX.                                                       //
//                                                                            //
// Classe per gestire le matrici di numeri double allocate dinamicamente.     //
//----------------------------------------------------------------------------//
class C_DBL_MATRIX
{
   protected :
      double *Matrix;   // Matrice di numeri double
      long nx;          // numero di colonne della matrice
      long ny;          // numero di righe della matrice

   public :            
      C_DBL_MATRIX();
      C_DBL_MATRIX(long nCols, long nRows);
      virtual ~C_DBL_MATRIX();

      long get_nCols();
      long get_nRows();
      int alloc(long nCols, long nRows);
      double get(long Col, long Row); // get value     
      void set(long Col, long Row, double val); // set value
      void setColumn(long Col, double val); // set all column values
      void setRow(long Row, double val); // set all row values
      void init(double val); // set all values

      void set1DArray(long Pos, double val); // set value in the Array
      double get1DArray(long Pos);           // get value in the Array
      void getColRowFrom1DArray(long Pos, long *Col, long *Row); // get matrix cell from a position in the Array
};


//----------------------------------------------------------------------------//
// class C_STR_STRLIST                                                        //
//                                                                            //
// Classe per il parsing del LISP.                                            //
//----------------------------------------------------------------------------//
class C_STR_STRLIST_LIST;

class C_STR_STRLIST : public C_STR
{
   friend class C_STR_STRLIST_LIST;
   protected:
      C_STR_STRLIST_LIST *pStr_StrList;
   public :
      C_STR_STRLIST();
      C_STR_STRLIST(const char *in);
      C_STR_STRLIST(const TCHAR *in);
      virtual ~C_STR_STRLIST(); // chiama ~C_STR

      C_STR_STRLIST_LIST *ptr_Str_StrList(void);
      int add_tail(C_STR_STRLIST *newnode);
      int add_tail_str(const TCHAR *Value);
};


//----------------------------------------------------------------------------//
//    class C_STR_STRLIST_LIST                                                //
//----------------------------------------------------------------------------//


class C_STR_STRLIST_LIST : public C_STR_LIST
{
   public :
      DllExport C_STR_STRLIST_LIST();
      DllExport virtual ~C_STR_STRLIST_LIST();  // chiama ~C_LIST
};


//----------------------------------------------------------------------------//
//    GLOBAL FUNCTIONS                                                        //
//----------------------------------------------------------------------------//
extern int     gsc_lspforrb    (C_RB_LIST *rb);

                                                                           
#endif