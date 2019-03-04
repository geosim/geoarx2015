/**********************************************************
Name: GS_LIST

Module description: File funzioni di base per la gestione
                    delle classi di entita' 
            
Author: Poltini Roberto

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.a.

              
Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/*****************************************************************************/
/* INCLUDE                                                                   */
/*****************************************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <math.h>

#include "rxdefs.h"   
#include "adslib.h"   

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_utily.h"
#include "gs_resbf.h"

#include "gs_list.h"
#include "gs_ase.h"
#include "gs_init.h"
#include "gs_class.h"
#include "gs_area.h"
#include "gs_lisp.h"
#include "gs_graph.h"
#include "gs_topo.h"      // per "gsc_OverlapValidation"


//---------------------------------------------------------------------------//
// class C_NODE                                                              //
//---------------------------------------------------------------------------//


C_NODE::C_NODE() 
{ 
   next=prev=NULL; 
}
      
C_NODE::C_NODE(C_NODE *p,C_NODE *n) 
{ 
   prev=p; 
   next=n; 
}

C_NODE::~C_NODE() {}

int C_NODE::get_key()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::set_key(int in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

double C_NODE::get_key_double()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

long C_NODE::get_id()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::set_key(double in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::get_type()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::set_type(int in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

TCHAR* C_NODE::get_name()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return _T(""); 
}

int C_NODE::set_name(const TCHAR *in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::set_name(const char *in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

TCHAR* C_NODE::get_name2()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return _T(""); 
}

int C_NODE::set_name2(const TCHAR *in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_NODE::set_name2(const char *in)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

C_NODE* C_NODE::get_next() 
{ 
   return next; 
}

C_NODE* C_NODE::get_prev() 
{  
   return prev; 
}

int C_NODE::get_category()
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

GSDataPermissionTypeEnum C_NODE::get_level()
{ 
   GS_ERR_COD = eGSInvClassType; 
   return GSNonePermission; 
}

int C_NODE::comp(void *in)
{ 
   return 0; 
}


//---------------------------------------------------------------------------//
// class C_LIST.                                                             //
//---------------------------------------------------------------------------//

C_LIST::C_LIST() 
{ 
   head=tail=cursor=NULL; 
   count=0; 
   fCompPtr = NULL;
}

C_LIST::~C_LIST() 
{ 
   remove_all(); 
}
   
int C_LIST::get_count() const 
{ 
   return count; 
}
      
int C_LIST::is_empty() const
{ 
   if (count==0) 
      return TRUE; 
   else 
      return FALSE; 
}

C_NODE* C_LIST::get_head()  
{ 
   cursor=head; 
   return head; 
} 

C_NODE* C_LIST::get_tail()  
{ 
   cursor=tail; 
   return tail; 
}

C_NODE* C_LIST::get_cursor() 
{ 
   return cursor; 
}

C_NODE* C_LIST::get_next()
{ 
   if (cursor != NULL) 
      cursor = cursor->next; 
   return cursor; 
}

C_NODE* C_LIST::get_prev()
{ 
   if (cursor != NULL) 
      cursor = cursor->prev; 
   return cursor; 
}

int C_LIST::set_mother_class(C_NODE *p_class)
{ 
   GS_ERR_COD=eGSInvClassType; 
   return GS_BAD; 
}

int C_LIST::remove_head()
{
   if (head==NULL) return GS_BAD;
   if ((cursor=head->next)==NULL) tail=NULL;
   else cursor->prev=NULL;
   delete head; head=cursor;
   count--;return GS_GOOD;
}

int C_LIST::remove_tail()
{
   if (tail==NULL) return GS_BAD;
   if ((cursor=tail->prev)==NULL) head=NULL;
   else cursor->next=NULL;
   delete tail; tail=cursor;
   count--; return GS_GOOD;
}

int C_LIST::add_head(C_NODE* new_node)
{
   if (new_node==NULL) return GS_BAD;
   if (count==0) tail=new_node;
   else head->prev=new_node;
   new_node->next=head; new_node->prev=NULL; head=new_node;
   count++; cursor=head; return GS_GOOD;
}

int C_LIST::add_tail(C_NODE* new_node)
{
   if (new_node==NULL) return GS_BAD;
   if (count==0) head=new_node;
   else tail->next=new_node;
   new_node->next=NULL; new_node->prev=tail; tail=new_node;
   count++; cursor=tail; 
   return GS_GOOD;
}

int C_LIST::paste_head(C_LIST &new_list)
{
   C_NODE *pnode;
   int    result = GS_GOOD;

   if (new_list.is_empty() == TRUE) return GS_GOOD;

   while ((pnode = new_list.cut_tail()) != NULL)
      if (add_head(pnode)==GS_BAD) { result=GS_BAD; break; }

   return result;
}

int C_LIST::paste_tail(C_LIST &new_list)
{  
   C_NODE *pnode;
   int    result = GS_GOOD;

   if (new_list.is_empty() == TRUE) return GS_GOOD;

   while ((pnode = new_list.cut_head()) != NULL)
      if (add_tail(pnode)==GS_BAD) { result=GS_BAD; break; }

   return result;
}

int C_LIST::remove(C_NODE *punt)
{  // il cursore punterà all'elemento successivo quello cancellato
   C_NODE *ptr;

   if (!punt) return GS_BAD;
   if (punt == head) return remove_head();
   if (punt == tail) return remove_tail();

   ptr = head;
   while (ptr != NULL)
   {
      if (ptr == punt)
      {      
         cursor=punt->prev->next=punt->next;
         punt->next->prev=punt->prev;
         count--; delete punt;
         return GS_GOOD;
      }
      ptr = ptr->next;
   }

   return GS_BAD; // Il nodo non appartiene alla lista
}  

int C_LIST::remove_all()
{ 
   while(head!=NULL)
   {
      cursor=head->next; 
      delete head; 
      head=cursor;
   }
   count=0; 
   tail=NULL; 
   return GS_GOOD;
}   

C_NODE *C_LIST::getptr_at(int pos)    // 1 e' il primo //
{
   int i;
   if (pos>count || pos<1) return NULL;
   if (pos<count/2)
      { cursor=head; for (i=1;i<pos;i++) cursor=cursor->next; }
   else
      { cursor=tail; for (i=count;i>pos;i--) cursor=cursor->prev; }
   return cursor;
}

int C_LIST::getpos(C_NODE *ptr)        // 1 e' il primo //
{                                      // 0 non trovato //
   int cc=0;

   cursor=head;
   while(cursor!=NULL)
   {
      cc++; 
      if (cursor==ptr) return cc;
      cursor = cursor->get_next();
   }
   return 0;
}

int C_LIST::getpos_key(int in)        // 1 e' il primo //
{                                     // 0 non trovato //
   int cc=0;

   cursor=head;
   while(cursor!=NULL)
   {
      cc++; 
      if (cursor->get_key()==in) return cc;
      cursor = cursor->get_next();
   }
   return 0;
}

int C_LIST::getpos_name(const char *in, int sensitive)   // 1 e' il primo
{                                                        // 0 non trovato
   TCHAR *UnicodeString = gsc_CharToUnicode(in);  
   int   res = getpos_name(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_LIST::getpos_name(const TCHAR *in, int sensitive)  // 1 e' il primo
{                                                        // 0 non trovato
   int cc = 0;

   if (!in) return 0;
   cursor = head;
   while (cursor)
   {
      cc++; 
      if (gsc_strcmp(cursor->get_name(), in, sensitive) == 0) return cc;
      cursor = cursor->get_next();
   }
   return 0;
}

int C_LIST::getpos_name2(const char *in)     // 1 e' il primo
{                                            // 0 non trovato
   TCHAR *UnicodeString = gsc_CharToUnicode(in);  
   int res = getpos_name2(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_LIST::getpos_name2(const TCHAR *in)     // 1 e' il primo
{                                            // 0 non trovato
   int cc=0;

   if (!in) return 0;
   cursor = head;
   while (cursor)
   {
      cc++; 
      if (gsc_strcmp(cursor->get_name2(), in) == 0) return cc;
      cursor = cursor->get_next();
   }
   return 0;
}

int C_LIST::remove_at(int pos) // 1 e' il primo
{  
   getptr_at(pos);
   return remove_at();
}

// se viene rimosso un elemento il cursore va all'elemento successivo
int C_LIST::remove_at()
{  
   C_NODE *punt;

   if (!cursor) return GS_BAD;
   punt = cursor;

   if (cursor->prev == NULL) // rimozione in testa
   { 
      head = cursor = punt->next;  
      if (cursor) cursor->prev = NULL;
      else tail = NULL;
   }
   else
      if (cursor->next == NULL) // rimozione in coda
      { 
         tail = punt->prev;  
         if (tail) tail->next = NULL;
         else head = NULL;
         cursor = NULL;  
      }
      else
      {
         cursor = punt->prev->next = punt->next;
         punt->next->prev = punt->prev;
      }
   
   delete punt; 
   count--;
   
   return GS_GOOD;
}

int C_LIST::remove_key(int key)
{
   C_NODE *punt;

   if ((punt=C_LIST::search_key(key))==NULL) return GS_BAD;
   return remove(punt);  
}

int C_LIST::remove_key(long key)
{
   C_NODE *punt;

   if ((punt=C_LIST::search_key(key))==NULL) return GS_BAD;
   return remove(punt);  
}

int C_LIST::remove_name(const char *name, int sensitive)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(name);  
   int res = remove_name(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_LIST::remove_name(const TCHAR *name, int sensitive)
{
   C_NODE *punt;

   if ((punt = search_name(name, sensitive)) == NULL) return GS_BAD;
   return remove(punt);  
}

int C_LIST::remove_name2(const char *name)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(name);  
   int res = remove_name2(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_LIST::remove_name2(const TCHAR *name)
{
   C_NODE *punt;

   if ((punt = search_name2(name)) == NULL) return GS_BAD;
   return remove(punt);  
}

int C_LIST::insert_before(int pos, C_NODE* newnod)
{
   C_NODE *punt;
   if (newnod==NULL) return GS_BAD;
   if(pos==1) return add_head(newnod);
   if((punt=getptr_at(pos))==NULL) return GS_BAD;
   punt->prev->next=newnod; newnod->prev=punt->prev;
   punt->prev=newnod; newnod->next=punt;
   cursor=newnod; count++; return GS_GOOD;
}

int C_LIST::paste_before(int pos, C_LIST* pNewList)
{
   C_NODE *pnode;
   int    ndx = pos;
   
   if (!pNewList) return GS_BAD;
   if (pos == 1) return paste_head(*pNewList);

   while ((pnode = pNewList->cut_head()) != NULL)
      if (insert_before(ndx++, pnode) == NULL) return GS_BAD;

   return GS_GOOD;
}

int C_LIST::insert_before(C_NODE* newnod) // prima del cursore
{
   C_NODE *punt;
   if (newnod==NULL) return GS_BAD;
   if(cursor==NULL) return GS_BAD;
   if(cursor->prev==NULL) return add_head(newnod);
   punt=cursor;
   punt->prev->next=newnod; newnod->prev=punt->prev;
   punt->prev=newnod; newnod->next=punt;
   cursor=newnod; count++; return GS_GOOD;
}

int C_LIST::insert_after (int pos, C_NODE* newnod)
{
   C_NODE *punt;
   if (newnod==NULL) return GS_BAD;
   if(pos==count) return add_tail(newnod);
   if((punt=getptr_at(pos))==NULL) return GS_BAD;
   punt->next->prev=newnod; newnod->next=punt->next;
   punt->next=newnod; newnod->prev=punt;
   cursor=newnod; count++; return GS_GOOD;
}

int C_LIST::insert_after (C_NODE* newnod)
{
   C_NODE *punt;
   if (newnod==NULL) return GS_BAD;
   if(cursor==NULL) return GS_BAD;
   if(cursor->next==NULL) return add_tail(newnod);
   punt=cursor;
   punt->next->prev=newnod; newnod->next=punt->next;
   punt->next=newnod; newnod->prev=punt;
   cursor=newnod; count++; return GS_GOOD;
}

int C_LIST::insert_ordered(C_NODE *pNewNod, bool *inserted)
{
   C_NODE *p = get_head();
   int    res;

   while (p)
   {
      res = p->comp(pNewNod);
      if (res < 0) // p minore di pNewNod
         p = get_next();
      else if (res == 0) // c'è già
      {
         *inserted = false;
         return GS_GOOD;
      }
      else // p maggiore di pNewNod
         break;
   }

   *inserted = true;
   if (!p) 
		return add_tail(pNewNod);
	else
      return insert_before(pNewNod);
}

C_NODE *C_LIST::cut_head()
{
   C_NODE *punt;

   if (head==NULL) return NULL;
   punt=head;
   if ((cursor=head->next)==NULL) tail=NULL;
   else cursor->prev=NULL;
   head=cursor;
   count--;return punt;
}

C_NODE *C_LIST::cut_tail()
{
   C_NODE *punt;

   if (tail==NULL) return NULL;
   punt=tail;
   if ((cursor=tail->prev)==NULL) head=NULL;
   else cursor->next=NULL;
   tail=cursor;
   count--; return punt;
}

C_NODE *C_LIST::cut_key(int in)
{
   search_key(in);
   return cut_at();
}

C_NODE *C_LIST::cut_at(void)
{
   C_NODE *punt = cursor;
   return (cut(cursor) == GS_GOOD) ? punt : NULL;
}

int C_LIST::cut(C_NODE *node)
{
   C_NODE *punt;
   
   if ((punt = node) == NULL) return GS_BAD;
   if (punt == head) { cut_head(); return GS_GOOD; }
   if (punt == tail) { cut_tail(); return GS_GOOD; }

   cursor = punt->prev->next=punt->next;
   punt->next->prev = punt->prev;
   count--;
   return GS_GOOD;
}

int C_LIST::move_all(C_LIST *target)
{
   if (target==NULL) return GS_BAD;
   target->remove_all();

   target->head=head;       head=NULL;
   target->tail=tail;       tail=NULL;
   target->cursor=cursor;   cursor=NULL;
   target->count=count;     count=0;

   return GS_GOOD;
}

C_NODE *C_LIST::search_key(int in)
{
   cursor = head;
   while (cursor)
   {
      if (cursor->get_key() == in) break;
      get_next();
   }

   return cursor;
}   

C_NODE *C_LIST::search_key(double in)
{
   cursor = head;
   while (cursor)
   {
      if (cursor->get_key_double() == in) break;
      get_next();
   }

   return cursor;
}   

C_NODE *C_LIST::search_key(long in)
{
   cursor = head;
   while (cursor)
   {
      if (cursor->get_id() == in) break;
      get_next();
   }

   return cursor;
}   

C_NODE *C_LIST::search_next_key(int in)
{
   get_next();
   while(cursor!=NULL)
      { 
      if (cursor->get_key()==in) break;
      get_next();
      }
   return cursor;
}   

C_NODE *C_LIST::search_type(int in)
{
   cursor=head;
   while(cursor!=NULL)
      { 
      if (cursor->get_type()==in) break;
      get_next();
      }
   return cursor;
}   

C_NODE *C_LIST::search_next_type(int in)
{
   get_next();
   while(cursor!=NULL)
      { 
      if (cursor->get_type()==in) break;
      get_next();
      }
   return cursor;
}   

/*********************************************************/
/*.doc search_name                            <internal> */
/*+
  Questa funzione cerca un elemento nella lista.
  Parametri:
  const char *in;    stringa da cercare
  int sensitive;     sensibile al maiuscolo (default = TRUE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
C_NODE *C_LIST::search_name(const char *in, int sensitive)
{
   TCHAR  *UnicodeString = gsc_CharToUnicode(in);  
   C_NODE *res = search_name(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
C_NODE *C_LIST::search_name(const TCHAR *in, int sensitive)
{
   cursor = head;
   while (cursor)
   { 
      if (gsc_strcmp(cursor->get_name(), in, sensitive) == 0) break;
      get_next();
   }
   return cursor;
}   

C_NODE *C_LIST::search_next_name(const char *in, int sensitive)
{
   TCHAR  *UnicodeString = gsc_CharToUnicode(in);  
   C_NODE *res = search_next_name(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
C_NODE *C_LIST::search_next_name(const TCHAR *in, int sensitive)
{
   get_next();
   while (cursor)
   { 
      if (gsc_strcmp(cursor->get_name(), in, sensitive) == 0) break;
      get_next();
   }
   return cursor;
}   

C_NODE *C_LIST::search_name2(const char *in, int sensitive)
{
   TCHAR  *UnicodeString = gsc_CharToUnicode(in);  
   C_NODE *res = search_name2(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
C_NODE *C_LIST::search_name2(const TCHAR *in, int sensitive)
{
   cursor = head;
   while (cursor)
   { 
      if (gsc_strcmp(cursor->get_name2(), in, sensitive) == 0) break;
      get_next();
   }
   return cursor;
}   

C_NODE *C_LIST::search_next_name2(const char *in, int sensitive)
{
   TCHAR  *UnicodeString = gsc_CharToUnicode(in);  
   C_NODE *res = search_next_name2(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
C_NODE *C_LIST::search_next_name2(const TCHAR *in, int sensitive)
{
   get_next();
   while (cursor)
   { 
      if (gsc_strcmp(cursor->get_name2(), in, sensitive) == 0) break;
      get_next();
   }
   return cursor;
}   

// ricerca la stringa che inizia per ...
C_NODE *C_LIST::search_name2_prefix(const char *in)
{
   TCHAR  *UnicodeString = gsc_CharToUnicode(in);  
   C_NODE *res = search_name2_prefix(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
C_NODE *C_LIST::search_name2_prefix(const TCHAR *in)
{
   cursor = head;
   while (cursor)
   { 
      if (wcsstr(cursor->get_name2(), in) == cursor->get_name2()) break;
      get_next();
   }
   return cursor;
}   

C_NODE *C_LIST::search_category(int in)
{
   cursor=head;
   while(cursor!=NULL)
      { 
      if (cursor->get_category()==in) break;
      get_next();
      }
   return cursor;
}   

C_NODE *C_LIST::search_next_category(int in)
{
   get_next();
   while(cursor!=NULL)
      { 
      if (cursor->get_category()==in) break;
      get_next();
      }
   return cursor;
}   

int C_LIST::print_list()
{
   C_NODE *punt;

   int i=1;
   acutPrintf(_T("LEN : %d\n"), count);
   punt = head;
   while(punt!=NULL)
   {
     acutPrintf(_T("%d->\t%d\t%d\t%s"), i,
                punt->get_key(), punt->get_type(), punt->get_name());
     if(punt==head) acutPrintf(_T(" HEAD"));
     if(punt==tail) acutPrintf(_T(" TAIL"));
     if(punt==cursor) acutPrintf(_T(" CURS"));
     acutPrintf(GS_LFSTR);

      punt=punt->next;i++;
   }
   return GS_GOOD;
}

C_NODE *C_LIST::goto_pos(int in) // 1 e' il primo
{
   int i = 1;
   
   if (in > count || in == 0) return NULL;
   cursor = head;
   while (cursor && i < in) 
   {
      get_next();
      i++;
   }
   return cursor;
}   

// questa funzione è estremamente pericolosa perchè si fida che ptr
// sia il puntatore di un elemento della C_LIST corrente
int C_LIST::set_cursor(C_NODE *ptr)
{
   cursor = ptr;

   return GS_GOOD;
}   


/*********************************************************/
/*.doc C_LIST::sort                           <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base alla
  funzione fCompPtr impostato per la lista.
  Parametri:
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort(void)
{
   bool   stop = false;
   int    i, j = 0;
   C_NODE *p_item1, *p_item2, *p_next;

   if (!head || !fCompPtr) return;

   while (!stop)
   {
      stop    = true;
      i       = 1;
      p_item1 = head;
      p_item2 = p_item1->next;
      
      while (p_item2 && i < count - j)
      {
         if ((*fCompPtr)(p_item1, p_item2) > 0) // richiamo la funzione di comparazione
         { // inverto la posizione dei 2 C_NODE
            stop = false;
            // precedente di p_item1
            p_item2->prev = p_item1->prev;
            p_next = p_item2->next;
            p_item2->next = p_item1;
            // successivo di p_item2
            p_item1->next = p_next;
            p_item1->prev = p_item2;
            
            if (p_item1->next) p_item1->next->prev = p_item1; // collego l'elemento successivo
            else tail = p_item1; // non c'era il successivo quindi è in coda

            if (p_item2->prev) p_item2->prev->next = p_item2; // collego l'elemento precedente
            else head = p_item2; // non c'era il precedente quindi è in testa
         }
         else p_item1 = p_item2;

         p_item2 = p_item1->next;
         i++;
      }
      j++;
   }
}

int CALLBACK gsc_C_NODE_compare_by_name_asc_sensitive(C_NODE *p1, C_NODE *p2)
   { return gsc_strcmp(p1->get_name(), p2->get_name(), TRUE); }
int CALLBACK gsc_C_NODE_compare_by_name_asc_insensitive(C_NODE *p1, C_NODE *p2)
   { return gsc_strcmp(p1->get_name(), p2->get_name(), FALSE); }
int CALLBACK gsc_C_NODE_compare_by_name_desc_sensitive(C_NODE *p1, C_NODE *p2)
   { return gsc_strcmp(p1->get_name(), p2->get_name(), TRUE); }
int CALLBACK gsc_C_NODE_compare_by_name_desc_insensitive(C_NODE *p1, C_NODE *p2)
   { return gsc_strcmp(p1->get_name(), p2->get_name(), FALSE); }

int CALLBACK gsc_C_NODE_compare_by_key_asc(C_NODE *p1, C_NODE *p2)
{
   if (p1->get_key() == p2->get_key())
      return 0;
   else
      return (p1->get_key() > p2->get_key()) ? 1 : -1; 
}
int CALLBACK gsc_C_NODE_compare_by_key_desc(C_NODE *p1, C_NODE *p2)
   { return gsc_C_NODE_compare_by_key_asc(p2, p1); }

int CALLBACK gsc_C_REAL_compare_by_key_asc(C_NODE *p1, C_NODE *p2)
{
   double n1 = ((C_REAL *) p1)->get_key_double();
   double n2 = ((C_REAL *) p2)->get_key_double();

   if (n1 == n2)
      return 0;
   else
      return (n1 > n2) ? 1 : -1; 
}
int CALLBACK gsc_C_REAL_compare_by_key_desc(C_NODE *p1, C_NODE *p2)
   { return gsc_C_REAL_compare_by_key_asc(p2, p1); }

int CALLBACK gsc_C_NODE_compare_by_nameAsNum_asc(C_NODE *p1, C_NODE *p2)
{
   double n1 = (p1->get_name()) ? _wtof(p1->get_name()) : 0;
   double n2 = (p2->get_name()) ? _wtof(p2->get_name()) : 0;

   if (n1 == n2)
      return 0;
   else
      return (n1 > n2) ? 1 : -1; 
}
int CALLBACK gsc_C_NODE_compare_by_nameAsNum_desc(C_NODE *p1, C_NODE *p2)
   { return gsc_C_NODE_compare_by_nameAsNum_asc(p2, p1); }

int CALLBACK gsc_C_NODE_compare_by_nameAsDate_asc(C_NODE *p1, C_NODE *p2)
{
   C_STRING Date1, Date2;

   // Conversione in formato di MAP = "yyyy-mm-dd hh:mm:ss"
   if (gsc_getSQLDateTime(p1->get_name(), Date1) == GS_BAD) Date1 = GS_EMPTYSTR;
   if (gsc_getSQLDateTime(p2->get_name(), Date2) == GS_BAD) Date2 = GS_EMPTYSTR;

   return Date1.comp(Date2);
}
int CALLBACK gsc_C_NODE_compare_by_nameAsDate_desc(C_NODE *p1, C_NODE *p2)
   { return gsc_C_NODE_compare_by_nameAsDate_asc(p2, p1); }

int CALLBACK gsc_C_NODE_compare_by_id_asc(C_NODE *p1, C_NODE *p2)
{
   if (p1->get_id() == p2->get_id())
      return 0;
   else
      return (p1->get_id() > p2->get_id()) ? 1 : -1; 
}
int CALLBACK gsc_C_NODE_compare_by_id_desc(C_NODE *p1, C_NODE *p2)
   { return gsc_C_NODE_compare_by_id_asc(p2, p1); }


/*********************************************************/
/*.doc C_LIST::sort_name                      <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base alla stringa
  restituita da get_name().
  int sensitive;  se = TRUE è sensibile al maiuscolo/minuscolo (default = FALSE)
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort_name(int sensitive, int ascending)
{
   if (sensitive == TRUE)
   {
      if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_name_asc_sensitive;
      else                   fCompPtr = &gsc_C_NODE_compare_by_name_desc_sensitive;
   }
   else
      if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_name_asc_insensitive;
      else                   fCompPtr = &gsc_C_NODE_compare_by_name_desc_insensitive;

   sort();
}


/*********************************************************/
/*.doc C_LIST::sort_key                       <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base alla stringa
  restituita da get_name().
  int sensitive;  se = TRUE è sensibile al maiuscolo/minuscolo (default = FALSE)
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort_key(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_key_asc;
   else                   fCompPtr = &gsc_C_NODE_compare_by_key_desc;

   sort();
}


/*********************************************************/
/*.doc C_LIST::sort_nameByNum <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base alla stringa
  restituita da get_name() considerata come numero.
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort_nameByNum(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_nameAsNum_asc;
   else                   fCompPtr = &gsc_C_NODE_compare_by_nameAsNum_desc;

   sort();
}


/*********************************************************/
/*.doc C_LIST::sort_nameByDate <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base alla stringa
  restituita da get_name().
  int sensitive;  se = TRUE è sensibile al maiuscolo/minuscolo (default = FALSE)
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort_nameByDate(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_nameAsDate_asc;
   else                   fCompPtr = &gsc_C_NODE_compare_by_nameAsDate_desc;

   sort();
}


/*********************************************************/
/*.doc C_LIST::sort_id                        <external> */
/*+
  Questa funzione ordina una lista C_LIST o derivate in base al numero
  lungo id restituito da get_id().
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_LIST::sort_id(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_NODE_compare_by_id_asc;
   else                   fCompPtr = &gsc_C_NODE_compare_by_id_desc;

   sort();
}


/*********************************************************/
/*.doc C_LIST::Inverse                        <external> */
/*+
  Questa funzione inverte l'ordine degli elementi della lista.
-*/  
/*********************************************************/
void C_LIST::Inverse(void)
{
   int    i, Tot = get_count();
   C_NODE *p = cursor, *pMoved = NULL, *dummy;

   for (i = 1; i < Tot; i++)
   {
      if (pMoved == NULL) // prima volta 
      {
         pMoved = cut_tail();
         add_head(pMoved);
      }
      else
      {
         dummy  = pMoved;
         pMoved = cut_tail();
         cursor = dummy;
         insert_after(pMoved);
      }
   }

   cursor = p; // Ripristino il cursore
}

//---------------------------------------------------------------------------//
// class C_INT                                                               //
//---------------------------------------------------------------------------//

C_INT::C_INT() : C_NODE() { key = 0; }

C_INT::C_INT(int in) : C_NODE() { set_key(in); }
      
C_INT::~C_INT() {}

int C_INT::get_key()  
{ 
   return key; 
}

int C_INT::set_key(int in)  
{ 
   key=in; 
   return GS_GOOD; 
}

int C_INT::comp(void *in)
{
   if (key == ((C_INT *) in)->key)
      return 0;
   else
      return (key > ((C_INT *) in)->key) ? 1 : -1; 
}


//---------------------------------------------------------------------------//
// INIZIO CLASSE C_INT_LIST                                                  //
//---------------------------------------------------------------------------//

C_INT_LIST::C_INT_LIST():C_LIST() {}

C_INT_LIST::~C_INT_LIST() {}

int C_INT_LIST::copy(C_INT_LIST *out)
{
   C_INT *new_node,*punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   punt=(C_INT*)get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_INT(punt->key)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      out->add_tail(new_node);
      punt = (C_INT*) get_next();
   }
   return GS_GOOD;
}

resbuf *C_INT_LIST::to_rb(void)
{
   resbuf *list=NULL,*rb;
   C_INT *punt;

   punt = (C_INT *) get_tail();
   if (punt == NULL) 
      if ((list = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

   while (punt)
   {                       
      if ((rb = acutBuildList(RTSHORT, punt->get_key(), 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext = list;
      list = rb; punt = (C_INT *) get_prev();
   }

   return list;      
}

int C_INT_LIST::from_rb(resbuf *rb)
{
   int _number;

   remove_all();

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   switch (rb->restype)
   {
      case RTANG:
      case RTLONG:
      case RTORINT:
      case RTREAL:
      case RTSHORT:
         if (gsc_rb2Int(rb, &_number) == GS_BAD) 
            { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         if (add_tail_int(_number) == GS_BAD)
            { remove_all(); return GS_BAD; }
         return GS_GOOD;

      case RTNONE: case RTNIL:
         return GS_GOOD;

      case RTPOINT:
         if (add_tail_int((int) rb->resval.rpoint[X]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rpoint[Y]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         return GS_GOOD;

      case RT3DPOINT:
         if (add_tail_int((int) rb->resval.rpoint[X]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rpoint[Y]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rpoint[Z]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         return GS_GOOD;

      case RTLB:
         while ((rb = rb->rbnext) != NULL && rb->restype != RTLE && rb->restype != RTDOTE)
         {
            if (gsc_rb2Int(rb, &_number) == GS_BAD) 
               { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
            if (add_tail_int(_number) == GS_BAD)
               { remove_all(); return GS_BAD; }
         }
         if (!rb) { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         return GS_GOOD;

      case 10: case 11: case 12: case 13: case 14: case 15:
      case 16: case 17: case 18: case 19:
      case 1010: case 1011: case 1012: case 1013: case 1014: case 1015:
      case 1016: case 1017: case 1018: case 1019:
         if (add_tail_int(rb->restype) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rpoint[X]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rpoint[Y]) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (rb->resval.rpoint[Z] != 0)
            if (add_tail_int((int) rb->resval.rpoint[Z]) == GS_BAD)
               { remove_all(); return GS_BAD; }
         return GS_GOOD;

      case kDxfUCSOriX:
         if (add_tail_int(kDxfUCSOriX) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rreal) == GS_BAD)
            { remove_all(); return GS_BAD; }
         return GS_GOOD;

      case kDxfUCSOriY:
         if (add_tail_int(kDxfUCSOriY) == GS_BAD)
            { remove_all(); return GS_BAD; }
         if (add_tail_int((int) rb->resval.rreal) == GS_BAD)
            { remove_all(); return GS_BAD; }
         return GS_GOOD;
   }

   GS_ERR_COD = eGSInvRBType;

   return GS_BAD; 
}

//-----------------------------------------------------------------------//
int C_INT_LIST::save(FILE *file, const char *sez)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(sez);  
   int   res = save(file, UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_INT_LIST::save(FILE *file, const TCHAR *sez)
{
   C_INT    *p, *pPrev;
   C_STRING str_ndx, str_num;
   int      ndx = 1;

   pPrev = (C_INT *) get_cursor();
   p     = (C_INT *) get_head();
   while (p)
   {
      str_ndx = ndx++;
      str_num = p->get_key();
      if (gsc_set_profile(file, sez, str_ndx.get_name(), str_num.get_name(), 0) == GS_BAD)
         return GS_BAD;
      p = (C_INT *) p->get_next();
   }
   set_cursor(pPrev);

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int C_INT_LIST::load(FILE *file, const char *sez)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(sez);  
   int   res = load(file, UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_INT_LIST::load(FILE *file, const TCHAR *sez)
{
   C_INT    *p;
   C_STRING str_ndx;
   int      ndx = 1;
   TCHAR    buf[ID_PROFILE_LEN], *str;

   str = buf;
   remove_all();
   str_ndx = ndx++;
   while (gsc_get_profile(file, sez, str_ndx.get_name(), &str, ID_PROFILE_LEN - 1, 0) == GS_GOOD)
   {
      if ((p = new C_INT(_wtoi(str))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(p);
      str_ndx = ndx++;
   }

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int C_INT_LIST::FromDCLMultiSel(const char *in)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in);  
   int   res = FromDCLMultiSel(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_INT_LIST::FromDCLMultiSel(const TCHAR *in)
{
   C_STRING buffer;
   C_INT    *pint;
   int      cont, ii;
   TCHAR    *punt = NULL;
   
   remove_all();
   buffer = in;
   buffer.alltrim();
   if (buffer.len() == 0) return GS_GOOD;
   // Devo stare attento perchè in "in" ci possono essere più valori,
   // perchè la selezione è multipla il formato di restituzione è "2 3 7 9 11"
   // Sostituisco gli spazi con \0 così ricavo il numero di elementi selezionati
   cont = gsc_strsep(buffer.get_name(), _T('\0'), _T(' '));
   // Incremento di uno perchè se gli spazi sono due i valori sono tre
   cont++; 
   punt = buffer.get_name();

   if ((pint = new C_INT(_wtoi(punt))) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
   add_tail(pint);

   for (ii = 2; ii <= cont; ii++)
   {
      while (*punt != _T('\0')) punt++; punt++; 

      if ((pint = new C_INT(_wtoi(punt))) == NULL) 
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      add_tail(pint);
   }

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
// La stringa ha gli elementi separati da <sep>
int C_INT_LIST::from_str(const char *str, char sep)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(str); 
   TCHAR UnicodeChar = gsc_CharToUnicode(sep);
   int   res = from_str(UnicodeString, UnicodeChar);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_INT_LIST::from_str(const TCHAR *str, TCHAR sep)
{
   TCHAR    *p;
   C_STRING Buffer(str);
   C_INT    *pInt;
   int      n, i;

   remove_all();
   if (Buffer.len() == 0) return GS_GOOD;

   p = Buffer.get_name();
   n = gsc_strsep(p, _T('\0'), sep) + 1;

   for (i = 0; i < n; i++)
   {
      if ((pInt = new C_INT(_wtoi(p))) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(pInt);
      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
// La stringa restituita ha gli elementi separati da <sep> (default = ',')
TCHAR *C_INT_LIST::to_str(char sep)
{
   TCHAR UnicodeChar = gsc_CharToUnicode(sep);

   return to_str(UnicodeChar);
}
TCHAR *C_INT_LIST::to_str(TCHAR sep)
{
   C_STRING Buffer;
   C_INT    *p;

   p = (C_INT *) get_head();
   while (p)
   {
      Buffer += p->get_key();
      if ((p = (C_INT *) p->get_next()) != NULL) Buffer += sep;
   }

   return Buffer.cut();
}

int C_INT_LIST::add_tail_int(int Val)
{
   C_INT *p;

   if ((p = new C_INT(Val)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

C_INT *C_INT_LIST::search_min_key(void)
{
   get_head();
   if (cursor)
   {
      C_NODE *pMin = cursor;

      get_next();
      while (cursor)
      {
         if (cursor->get_key() < pMin->get_key()) pMin = cursor;
         get_next();
      }
      cursor = pMin;
   }

   return (C_INT *) cursor;
}   


//---------------------------------------------------------------------------//
// FINE CLASSE C_INT_LIST                                                    //
// INIZIO CLASSE C_LONG                                                      //
//---------------------------------------------------------------------------//


C_LONG::C_LONG() : C_NODE() 
{ 
   id=0;
}
      
C_LONG::~C_LONG() {}

long C_LONG::get_id()  
{ 
   return id; 
}

int C_LONG::set_id(long in)  
{ 
   id=in; 
   return GS_GOOD; 
}


int C_LONG::comp(void *in)
{
   if (id == ((C_LONG *) in)->id)
      return 0;
   else
      return (id > ((C_LONG *) in)->id) ? 1 : -1; 
}


//---------------------------------------------------------------------------//
// class C_LONG_LIST                                                         //
//---------------------------------------------------------------------------//

C_LONG_LIST::C_LONG_LIST() : C_LIST() {}

C_LONG_LIST::~C_LONG_LIST(){} 

C_LONG *C_LONG_LIST::search(long in)
{
   C_LONG *punt;

   punt=(C_LONG*)get_head();
   while(punt!=NULL)
   {
      if (in==punt->get_id()) break;
      punt=(C_LONG*)get_next();
   }
   return punt;
}

int C_LONG_LIST::copy(C_LONG_LIST *out)
{
   C_LONG *new_node, *punt;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   punt = (C_LONG *) get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_LONG(punt->id)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      out->add_tail(new_node);
      punt = (C_LONG *) get_next();
   }

   return GS_GOOD;
}

// Compara le due liste per verificare se il contenuto
// dipendentemente dall'ordine sia =
bool C_LONG_LIST::operator == (C_LONG_LIST &in)
{
   C_LONG *p    = (C_LONG *) get_head();
   C_LONG *p_in = (C_LONG *) in.get_head();

   while (p && p_in)
   {
      if (p->get_id() != p_in->get_id()) return false;
      p    = (C_LONG *) get_next();
      p_in = (C_LONG *) in.get_next();
   }

   return (!p && !p_in) ? true : false;
}

int C_LONG_LIST::add_tail_long(long Val)
{
   C_LONG *p;

   if ((p = new C_LONG(Val)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

int C_LONG_LIST::from_str(const TCHAR *str, TCHAR sep)
{
   TCHAR    *p;
   C_STRING Buffer(str);
   C_LONG   *pLong;
   int      n, i;

   remove_all();
   if (Buffer.len() == 0) return GS_GOOD;

   p = Buffer.get_name();
   n = gsc_strsep(p, _T('\0'), sep) + 1;

   for (i = 0; i < n; i++)
   {
      if ((pLong = new C_LONG(_wtol(p))) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(pLong);
      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_INT_INT                                                           //
//---------------------------------------------------------------------------//

C_INT_INT::C_INT_INT() : C_INT() { type = 0; }
C_INT_INT::C_INT_INT(int in1, int in2) { set_key(in1); set_type(in2); }

C_INT_INT::~C_INT_INT() {}

int C_INT_INT::get_type() 
{ 
   return type; 
}

int C_INT_INT::set_type(int in) 
{ 
   type=in; 
   return GS_GOOD; 
}

int C_INT_INT::from_rb(presbuf *rb)
{
   presbuf p = *rb;
   if (!p) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      
   switch (p->restype)
   {
      case RTPOINT:
         set_key((int)p->resval.rpoint[0]);
         set_type((int)p->resval.rpoint[1]);
         break;
      case RTLB:
         if ((p = p->rbnext) == NULL || p->restype != RTSHORT)
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         set_key(p->resval.rint);
         if ((p = p->rbnext) == NULL || p->restype != RTSHORT)
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         set_type(p->resval.rint);
         if ((p = p->rbnext) == NULL || (p->restype != RTLE && p->restype != RTDOTE))
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         break;
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }
   *rb = p->rbnext; // vado al successivo
     
   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_INT_INT_LIST                                                      //
//---------------------------------------------------------------------------//

C_INT_INT_LIST::C_INT_INT_LIST() : C_LIST() {}

C_INT_INT_LIST::~C_INT_INT_LIST() {}

int C_INT_INT_LIST::values_add_tail(int _key, int _type)
{
   C_INT_INT *new_node;

   if ((new_node = new C_INT_INT(_key, _type)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return add_tail(new_node);
 }

int C_INT_INT_LIST::copy(C_INT_INT_LIST *out)
{
C_INT_INT *new_node,*punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   punt=(C_INT_INT*)get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_INT_INT)==NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      new_node->key  = punt->key;
      new_node->type = punt->type;
      out->add_tail(new_node);
      punt = (C_INT_INT*) get_next();
   }
   
   return GS_GOOD;
}

resbuf *C_INT_INT_LIST::to_rb(void)
{
   resbuf *list=NULL,*rb;
   C_INT_INT *punt;
            
   punt=(C_INT_INT*)get_tail();
   if (punt==NULL) 
      if ((list=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }
      
   while(punt!=NULL)
      {                                
      if ((rb=acutBuildList(RTLB,
                               RTSHORT, punt->get_key(),
                               RTSHORT, punt->get_type(),
                            RTLE, 0)) == NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext->rbnext->rbnext=list;
      list=rb; punt=(C_INT_INT*)get_prev();
      }                             
   return list;      
}

int C_INT_INT_LIST::from_rb(resbuf *rb)
{
   C_INT_INT *nod;

   remove_all();
   if (!rb) return GS_GOOD;
   if (rb->restype == RTNIL) return GS_GOOD;
   if (rb->restype != RTLB)  { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   rb = rb->rbnext; // salto la parentesi aperta
   while (rb && rb->restype != RTLE)
   {      
      if ((nod =new C_INT_INT) == NULL)
         { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      add_tail(nod);
      // legge dal resbuf e si sposta all'elemento successivo
      if (nod->from_rb(&rb) == GS_BAD) { remove_all(); return GS_BAD; }
   }
     
   return GS_GOOD;
}

C_INT_INT *C_INT_INT_LIST::search(int aaa,int bbb)
{
   C_INT_INT *punt;

   punt = (C_INT_INT *) get_head();
   while (punt != NULL)
   {
      if (aaa == punt->get_key())
         if (bbb == punt->get_type()) break;
      punt = (C_INT_INT *) get_next();
   }

   return punt;
}

int C_INT_INT_LIST::remove(int aaa,int bbb)
{
   C_NODE *punt;

   if ((punt = search(aaa, bbb)) == NULL) return GS_BAD;
   return C_LIST::remove((C_NODE *) punt);  
}


//-----------------------------------------------------------------------//
// La stringa ha le coppie separate da <CoupleSep> (default = ',' e 
// gli elementi separati da <sep> (default = ',')
int C_INT_INT_LIST::from_str(const char *str, char CoupleSep, char sep)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(str);  
   TCHAR UnicodeCharCoupleSep = gsc_CharToUnicode(CoupleSep);
   TCHAR UnicodeCharSep = gsc_CharToUnicode(sep);
   int   res = from_str(UnicodeString, UnicodeCharCoupleSep, UnicodeCharSep);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_INT_INT_LIST::from_str(const TCHAR *str, TCHAR CoupleSep, TCHAR sep)
{
   TCHAR     *p, *pPartial;
   C_STRING  Buffer(str), Partial;
   C_INT_INT *pIntInt;
   int       n, i, num, nPartial;

   remove_all();
   if (Buffer.len() == 0) return GS_GOOD;

   p = Buffer.get_name();
   n = gsc_strsep(p, _T('\0'), CoupleSep) + 1;

   for (i = 0; i < n; i++)
   {
      if (CoupleSep == sep) // Se i separatori sono uguali
      {
         num = _wtoi(p);
         i++;
         if (i < n)
         {
            while (*p != _T('\0')) p++; p++;
            if ((pIntInt = new C_INT_INT(num, _wtoi(p))) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         }
         else
            if ((pIntInt = new C_INT_INT(num, 0)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      }
      else // Se i separatori sono diversi
      {
         Partial = p;
         pPartial = Partial.get_name();
         nPartial = gsc_strsep(pPartial, _T('\0'), sep) + 1;
         num = _wtoi(pPartial);
         if (nPartial == 2)
         {
            while (*pPartial != _T('\0')) pPartial++; 
            pPartial++;
            if ((pIntInt = new C_INT_INT(num, _wtoi(pPartial))) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         }
         else
            if ((pIntInt = new C_INT_INT(num, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      }


      add_tail(pIntInt);
      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}

//-----------------------------------------------------------------------//
// La stringa restituita ha gli elementi separati da <sep> (default = ',')
TCHAR *C_INT_INT_LIST::to_str(char CoupleSep, char sep)
{
   TCHAR UnicodeCharCoupleSep = gsc_CharToUnicode(CoupleSep);
   TCHAR UnicodeCharSep = gsc_CharToUnicode(sep);
   
   return to_str(UnicodeCharCoupleSep, UnicodeCharSep);
}
TCHAR *C_INT_INT_LIST::to_str(TCHAR CoupleSep, TCHAR sep)
{
   C_STRING Buffer;
   C_INT_INT    *p;

   p = (C_INT_INT *) get_head();
   while (p)
   {
      Buffer += p->get_key();
      Buffer += sep;
      Buffer += p->get_type();
      if ((p = (C_INT_INT *) p->get_next()) != NULL) Buffer += CoupleSep;
   }

   return Buffer.cut();
}



//---------------------------------------------------------------------------//
// class C_2LONG                                                             //
//---------------------------------------------------------------------------//


C_2LONG::C_2LONG() : C_LONG()
{ 
   id_2 = 0;
}
C_2LONG::C_2LONG(long in1, long in2) : C_LONG(in1)
{ 
   set_id_2(in2);
}

C_2LONG::~C_2LONG() {}

long C_2LONG::get_id_2()        
{ 
   return id_2; 
}

int C_2LONG::set_id_2(long in) 
{ 
   id_2=in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_2LONG_LIST                                                        //
//---------------------------------------------------------------------------//


C_2LONG_LIST::C_2LONG_LIST() : C_LIST() {}

C_2LONG_LIST::~C_2LONG_LIST() {} 


//---------------------------------------------------------------------------//
// class C_2LONG_INT                                                         //
//---------------------------------------------------------------------------//

C_2LONG_INT::C_2LONG_INT() : C_2LONG() 
{ 
   class_id=0;
}
C_2LONG_INT::C_2LONG_INT(long in1, long in2, int in3)
{ 
   set_id(in1); set_id_2(in2); set_class_id(in3);
}


C_2LONG_INT::~C_2LONG_INT() {}

int C_2LONG_INT::get_class_id()       
{ 
   return class_id; 
}

int C_2LONG_INT::set_class_id(int in) 
{ 
   class_id=in; 
   return GS_GOOD; 
}

int C_2LONG_INT::get_key()  
{ 
   return class_id; 
}


//---------------------------------------------------------------------------//
// class C_2LONG_INT_LIST                                                    //
//---------------------------------------------------------------------------//

C_2LONG_INT_LIST::C_2LONG_INT_LIST() : C_LIST() {}

C_2LONG_INT_LIST::~C_2LONG_INT_LIST() {} 


//---------------------------------------------------------------------------//
// class C_2INT_LONG                                                         //
//---------------------------------------------------------------------------//

C_2INT_LONG::C_2INT_LONG() : C_INT_INT() 
{ 
   id=0;
}

C_2INT_LONG::C_2INT_LONG(int aaa,int bbb, long ccc) : C_INT_INT()
{
   set_key(aaa);
   set_type(bbb);
   set_id(ccc);
}

C_2INT_LONG::~C_2INT_LONG() {}

long C_2INT_LONG::get_id() 
{ 
   return id; 
}

int C_2INT_LONG::set_id(long in) 
{ 
   id=in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_2INT_LONG_LIST                                                    //
//---------------------------------------------------------------------------//

C_2INT_LONG_LIST::C_2INT_LONG_LIST() : C_LIST() {}

C_2INT_LONG_LIST::~C_2INT_LONG_LIST() {}

C_2INT_LONG *C_2INT_LONG_LIST::search(int aaa, int bbb, long ccc)
{
   C_2INT_LONG *punt = (C_2INT_LONG *) get_head();

   while (punt)
   {
      if (aaa == punt->get_key())
         if (bbb == punt->get_type())
            if (ccc == punt->get_id()) break;
      punt = (C_2INT_LONG *) get_next();
   }
   return punt;
}
C_2INT_LONG *C_2INT_LONG_LIST::search(int aaa, int bbb)
{
   C_2INT_LONG *punt = (C_2INT_LONG *) get_head();

   while (punt)
   {
      if (aaa == punt->get_key())
         if (bbb == punt->get_type()) break;
      punt = (C_2INT_LONG *) get_next();
   }
   return punt;
}

C_2INT_LONG *C_2INT_LONG_LIST::search_next(int aaa, int bbb, long ccc)
{
   C_2INT_LONG *punt = (C_2INT_LONG *) get_next();

   while (punt)
   { 
      if (aaa == punt->get_key())
         if (bbb == punt->get_type())
            if (ccc == punt->get_id()) break;
      punt = (C_2INT_LONG *) get_next();
   }
   
   return punt;
}   


//---------------------------------------------------------------------------//
// class C_STRING                                                            //
//---------------------------------------------------------------------------//

C_STRING::C_STRING() 
{ 
   name = NULL; 
   dim  = 0; 
}

C_STRING::C_STRING(int in)
   { alloc(in); }


C_STRING::C_STRING(C_STRING &in)
{
   name = NULL; 
   dim  = 0; 
   set_name(in.get_name());
}
C_STRING::C_STRING(const char *in)
{
   name = NULL; 
   dim  = 0; 
   set_name(in);
}
C_STRING::C_STRING(const TCHAR *in)
{
   name = NULL; 
   dim  = 0;
   set_name(in);
}
C_STRING::C_STRING(const TCHAR *format, va_list args)
{
   name = NULL; 
   dim  = 0;
   set_name(format, args);
}

C_STRING::~C_STRING()
{ 
   clear(); 
}

int C_STRING::alloc(int in) 
{ 
   clear(); 
   if ((name = (TCHAR *) calloc(in, sizeof(TCHAR))) == NULL)
   { 
      dim = 0, GS_ERR_COD = eGSOutOfMem; 
      return GS_BAD; 
   }
   else 
      dim = in;

   return GS_GOOD; 
}

TCHAR* C_STRING::get_name() 
{ 
   return name; 
}

size_t C_STRING::len() 
{ 
   if (name) 
      return wcslen(name);
   else 
      return 0; 
}

int C_STRING::comp(C_STRING &in, int sensitive)
   { return comp(in.get_name(), sensitive); } 
int C_STRING::comp(const TCHAR *in, int sensitive)
	{ return gsc_strcmp(name, in, sensitive); }
int C_STRING::comp(const char *in, int sensitive)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in);  
   int res = gsc_strcmp(name, UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}


/*********************************************************/
/*.doc C_STRING::wildcomp                     <external> */
/*+
  Questo operatore effettua la comparazione supportando
  i caratteri jolly * e ?.
  Parametri:
  const char *in;    stringa da confrontare

  Se le stringhe corrispondono restituisce un valore diverso da 0 altrimenti 0.
-*/  
/*********************************************************/
int C_STRING::wildcomp(C_STRING &wild, int sensitive)
   { return wildcomp(wild.get_name(), sensitive); }
int C_STRING::wildcomp(const char *wild, int sensitive)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(wild);  
   int res = wildcomp(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STRING::wildcomp(const TCHAR *wild, int sensitive)
{
	TCHAR       *cp, *pName;
   const TCHAR *mp;
   C_STRING    InternalName(name), InternalWild;

   if (!sensitive)
   {
      InternalName.toupper();
      InternalWild = wild;
      InternalWild.toupper();
      wild = InternalWild.get_name();
   }

   pName = InternalName.get_name();
	
	while ((*pName) && (*wild != _T('*')))
   {
      if ((*wild != *pName) && (*wild != _T('?'))) return 0;

      wild++;
		pName++;
	}
		
	while (*pName)
   {
		if (*wild == _T('*'))
      {
			if (!*++wild) return 1; // Fine stringa

         mp = wild;
			cp = pName + 1;
		}
      else if ((*wild == *pName) || (*wild == _T('?')))
      {
			wild++;
			pName++;
		}
      else 
      {
			wild = mp;
			pName = cp++;
		}
	}
		
	while (*wild == _T('*')) wild++;

   return !*wild;
}

int C_STRING::isAlnum(TCHAR CharToExclude)
{ 
	TCHAR ch;
	int   i = 0;

   if (!name) return GS_GOOD;

	while ((ch = get_chr(i++)) != _T('\0'))
      if (ch != CharToExclude)
		   if (iswalnum(ch) == 0) return GS_BAD;

   return GS_GOOD; 
}

int C_STRING::isAlpha(void)
{ 
	TCHAR ch;
	int   i = 0;

   if (!name) return GS_GOOD;

	while ((ch = get_chr(i++)) != _T('\0'))
		if (iswalpha(ch) == 0) return GS_BAD;

	return GS_GOOD; 
}

int C_STRING::isDigit(void)
{ 
	TCHAR ch;
	int   i = 0;

   if (!name) return GS_GOOD;

	while ((ch = get_chr(i++)) != _T('\0'))
		if (iswdigit(ch) == 0) return GS_BAD;

	return GS_GOOD; 
}

TCHAR* C_STRING::cut() 
{ 
   TCHAR *ret;

   ret  = name; 
   name = NULL; 
   dim  = 0; 

   return ret; 
} 

TCHAR *C_STRING::paste(TCHAR *in) 
{ 
   if (name) free(name);
   name = in; 
   if (in) dim = wcslen(in) + 1;
   else dim = 0;
   
   return in; 
}


/*********************************************************/
/*.doc C_STRING::+= <external> */
/*+
  Questo operatore effettua la concatenazione fra stringhe.
  Parametri:
  const char *in;    stringa da accodare (viene copiata e accodata)

  Restituisce codice classe in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
C_STRING& C_STRING::operator += (const char *in)
{
   cat(in);
   return (*this);
}
C_STRING& C_STRING::operator += (const TCHAR *in) 
{ 
   cat(in); 
   return (*this);
}
C_STRING& C_STRING::operator += (C_STRING &in) 
{ 
   cat(in.get_name()); 
   return (*this);
}
C_STRING& C_STRING::operator += (char in)
{
   char appoggio[2];

   appoggio[0] = in;
   appoggio[1] = '\0';
   
   cat(appoggio);
   return (*this);
}
C_STRING& C_STRING::operator += (TCHAR in)
{
   TCHAR appoggio[2];

   appoggio[0] = in;
   appoggio[1] = _T('\0');
   
   cat(appoggio); 
   return (*this);
}
C_STRING& C_STRING::operator += (short in)
{
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (int in)
{
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (long in)
{
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (float in)
{
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (double in)
{
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (presbuf in)
{
   TCHAR *appoggio = gsc_rb2str(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator += (ads_point in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      cat(appoggio);
      free(appoggio);
   }

   return (*this);
}


/*********************************************************/
/*.doc C_STRING::= <external> */
/*+
  Questo operatore effettua l'assegnazione della stringa.
  Parametri:
  const char *in;    stringa da assegnare (viene copiata)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
C_STRING& C_STRING::operator = (const char *in) 
{ 
   set_name(in);
   return (*this);
}
C_STRING& C_STRING::operator = (const TCHAR *in) 
{ 
   set_name(in); 
   return (*this);
}
C_STRING& C_STRING::operator = (C_STRING &in) 
{ 
   set_name(in.get_name()); 
   return (*this);
}
C_STRING& C_STRING::operator = (TCHAR in) 
{
   TCHAR appoggio[2];

   appoggio[0] = in;
   appoggio[1] = _T('\0');
   
   set_name(appoggio);
   return (*this);
}
C_STRING& C_STRING::operator = (char in) 
{ 
   char appoggio[2];

   appoggio[0] = in;
   appoggio[1] = '\0';
   
   set_name(appoggio); 
   return (*this);
}
C_STRING& C_STRING::operator = (short in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      set_name(appoggio);
      free(appoggio);
   }

   return (*this);
}
C_STRING& C_STRING::operator = (int in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      set_name(appoggio);
      free(appoggio);
   }
   
   return (*this);
}
C_STRING& C_STRING::operator = (long in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      set_name(appoggio);
      free(appoggio);
   }
   
   return (*this);
}
C_STRING& C_STRING::operator = (float in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      set_name(appoggio);
      free(appoggio);
   }
   
   return (*this);
}
C_STRING& C_STRING::operator = (double in) 
{ 
   TCHAR *appoggio = gsc_tostring(in);

   if (appoggio)
   {
      set_name(appoggio);
      free(appoggio);
   }
   
   return (*this);
}
C_STRING& C_STRING::operator = (presbuf in) 
{ 
   paste(gsc_rb2str(in));

   return (*this);
}
C_STRING& C_STRING::operator = (ads_point in) 
{ 
   paste(gsc_tostring(in));

   return (*this);
}


/*********************************************************/
/*.doc C_STRING::+                            <external> */
/*+
  Questo operatore effettua la concatenazione di due stringhe.
  Parametri:
  const char *in;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
C_STRING C_STRING::operator + (const TCHAR *in) 
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (const char *in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (C_STRING &in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (TCHAR in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (char in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (short in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (int in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (long in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (float in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (double in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (presbuf in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}
C_STRING C_STRING::operator + (ads_point in)
{
   C_STRING dummy;
   
   dummy = name;
   dummy += in;

   return dummy;
}


void C_STRING::clear() 
{ 
   if (name) 
   { 
      free(name); 
      name=NULL; 
      dim=0; 
   } 
}

int C_STRING::cat(const char *in)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in);  
   int   res = cat(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STRING::cat(const TCHAR *in)
{
   size_t len;

   if (in == NULL) return GS_GOOD;
   len = wcslen(in);
   if (name != NULL)
   {
      size_t name_len = wcslen(name);

      len += name_len;
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      wcscat(name + name_len, in);
   }
   else 
   {
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      name[0] = _T('\0');
      wcscat(name, in);
   }

   dim = len + 1;

   return GS_GOOD;   
}
    

int C_STRING::set_name(C_STRING &in)
   { return set_name(in.get_name()); }
int C_STRING::set_name(const char *in)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   int   res = set_name(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STRING::set_name(const TCHAR *in)
{
   if (in == NULL)
   { 
      if (name != NULL) free(name); 
      dim = 0; name = NULL; 
      return GS_GOOD;
   }
   
   size_t len = wcslen(in);
   if (len >= dim)
   {
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      dim = len + 1;
   }  
   wcscpy(name, in);
   
   return GS_GOOD;
}

int C_STRING::set_name(const TCHAR *format, va_list args)
{
   TCHAR buffer[1024];

   if (format == NULL)
   { 
      if (name != NULL) free(name); 
      dim = 0; name = NULL; 
      return GS_GOOD;
   }

   vswprintf(buffer, 1024, format, args);
   size_t len = wcslen(buffer);
   if (len >= dim)
   {
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      dim = len + 1;
   }  
   wcscpy(name, buffer);

   return GS_GOOD;
}

int C_STRING::set_name(const char *in, int start, int end) 
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   int   res = set_name(UnicodeString, start, end);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STRING::set_name(const TCHAR *in, int start, int end) 
{
   size_t len;
   int    i, j = 0;

   if (!in) { clear(); return GS_GOOD; }

   if (start > end) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   len = wcslen(in);
   if (end >= (int) len) end = (int) len - 1;
   len = end - start + 1; 
   
   if (len >= dim)
   {
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      dim = len + 1;
   }  

   for (i = start; i <= end; i++)
   {               
      name[j] = in[i];
      if (name[j] == _T('\0')) break;
      j++;
   } 
   name[j] = _T('\0');
   
   return GS_GOOD;
}

int C_STRING::set_name(double in) 
{
   return (paste(gsc_tostring(in)) != NULL) ? GS_GOOD : GS_BAD;
}

int C_STRING::set_name_formatted(const TCHAR *format, ...)
{
   va_list args;

   va_start(args, format);
   int result = set_name(format, args);
   va_end(args);
  
   return result;
}

int C_STRING::set_name_formatted(const TCHAR *format, va_list args)
{
   TCHAR buffer[1024];

   if (format == NULL)
   { 
      if (name != NULL) free(name); 
      dim = 0; name = NULL; 
      return GS_GOOD;
   }

   vswprintf(buffer, 1024, format, args);
   size_t len = wcslen(buffer);
   if (len >= dim)
   {
      if ((name = (TCHAR *) realloc(name, (len + 1) * sizeof(TCHAR))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      dim = len + 1;
   }  
   wcscpy(name, buffer);

   return GS_GOOD;
}

int C_STRING::set_chr(const char in, size_t pos)
{
   TCHAR UnicodeChar = gsc_CharToUnicode(in);
   int   res = set_chr(UnicodeChar, pos);

   return res;
}
int C_STRING::set_chr(const TCHAR in, size_t pos)
{
   if (pos < 0 || pos >= dim) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   name[pos] = in;

   return GS_GOOD;
}
TCHAR C_STRING::get_chr(size_t pos)
{
   if (pos < 0 || pos >= dim) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   return name[pos];
}

int C_STRING::find(const TCHAR *in, int sensitive) // 1 = primo carattere
{
   TCHAR *p;

	if ((p = at(in, sensitive)) == NULL) return -1;
   else return (int) (p - name + 1);
}
int C_STRING::find(C_STRING &in, int sensitive) // 1 = primo carattere
   { return find(in.get_name(), sensitive); }
int C_STRING::find(TCHAR in) // 1 = primo carattere
{
   TCHAR *p;

	if ((p = at(in)) == NULL) return -1;
   else return (int) (p - name + 1);
}

int C_STRING::reverseFind(const TCHAR *in, int sensitive) // 1 = primo carattere
{
   TCHAR *p;

	if ((p = rat(in, sensitive)) == NULL) return -1;
   else return (int) (p - name + 1);
}
int C_STRING::reverseFind(C_STRING &in, int sensitive) // 1 = primo carattere
   { return reverseFind(in.get_name(), sensitive); }
int C_STRING::reverseFind(TCHAR in) // 1 = primo carattere
{
   TCHAR *p;

	if ((p = rat((TCHAR) in)) == NULL) return -1;
   else return (int) (p - name + 1);
}

C_STRING C_STRING::right(size_t n)
{
   C_STRING dummy;

   if (!name || n < 0 || n > wcslen(name))
      { GS_ERR_COD = eGSInvalidArg; return NULL; }
   dummy.paste(gsc_tostring(name + wcslen(name) - n)); // Alloca memoria

   return dummy;
}
C_STRING C_STRING::left(size_t n)
{
   C_STRING dummy;

   if (!name || n < 0 || n > wcslen(name))
      { GS_ERR_COD = eGSInvalidArg; return NULL; }   
   dummy.set_name(name, 0, (int) (n - 1));

   return dummy;
}
C_STRING C_STRING::mid(size_t pos, size_t n) // 0-indexed
{
   C_STRING dummy;
   size_t   _len = wcslen(name);

   if (!name || n < 0) { GS_ERR_COD = eGSInvalidArg; return NULL; }
   if (pos >= _len) return NULL;
   if (pos + n > _len)
      dummy.set_name(name, (int) pos, (int) (_len - pos - 1));
   else
      dummy.set_name(name, (int) pos, (int) (pos + n - 1));

   return dummy;
}
int C_STRING::remove(TCHAR in, int sensitive)
{
   C_STRING dummy;

   dummy = in;
   return strtran(dummy.get_name(), _T(""), sensitive);
}


bool C_STRING::operator == (const char *in)
{ 
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   bool   res = (gsc_strcmp(name, UnicodeString) == 0) ? true : false;
   if (UnicodeString) free(UnicodeString);

   return res;
}
bool C_STRING::operator == (const TCHAR *in)
   { return (gsc_strcmp(name, in) == 0) ? true : false; }
bool C_STRING::operator == (C_STRING &in)
   { return (gsc_strcmp(name, in.get_name()) == 0) ? true : false; }

bool C_STRING::operator != (const char *in)
{ 
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   bool   res = (gsc_strcmp(name, UnicodeString) == 0) ? false : true ;
   if (UnicodeString) free(UnicodeString);

   return res;
}
bool C_STRING::operator != (const TCHAR *in)
   { return (gsc_strcmp(name, in) == 0) ? false : true; }
bool C_STRING::operator != (C_STRING &in)
   { return (gsc_strcmp(name, in.get_name()) == 0) ? false : true; }

C_STRING& C_STRING::alltrim()
{
   if (name) { gsc_alltrim(name); dim = wcslen(name) + 1; }
   return (*this);
}
C_STRING& C_STRING::ltrim()
{
   if (name) { gsc_ltrim(name); dim = wcslen(name) + 1; }
   return (*this);
}
C_STRING& C_STRING::rtrim()
{
   if (name) { gsc_rtrim(name); dim = wcslen(name) + 1; }
   return (*this);
}

C_STRING& C_STRING::toupper() 
{
   gsc_toupper(name);
   return (*this);
}

C_STRING& C_STRING::tolower()
{
   gsc_tolower(name);
   return (*this);
}

TCHAR *C_STRING::at(TCHAR in)
{
   if (name && in) return wcschr(name, in);
   else return NULL;
}
TCHAR *C_STRING::at(const char *in, int sensitive)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   TCHAR *res = at(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
TCHAR *C_STRING::at(const TCHAR *in, int sensitive)
{
   if (sensitive)
   {
      if (name && in) return wcsstr(name, in);
      else return NULL;
   }
   else
   {
      C_STRING _name(get_name()), _in(in);

      _name.toupper();
      _in.toupper();
      if (_name.get_name() && _in.get_name())
      {
         TCHAR *p = wcsstr(_name.get_name(), _in.get_name());
         
         return (p) ? name + (p - _name.get_name()) : NULL;
      }
      else return NULL;
   }
}
TCHAR *C_STRING::at(C_STRING &in, int sensitive)
   { return at(in.get_name(), sensitive); }

TCHAR *C_STRING::rat(TCHAR in)
{
   if (name) return wcsrchr(name, in);
   else return NULL;
}
TCHAR *C_STRING::rat(const char *in, int sensitive)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(in); 
   TCHAR *res = rat(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
TCHAR *C_STRING::rat(const TCHAR *in, int sensitive)
{
   TCHAR *pPrev, *p;

   if (sensitive)
   {
      pPrev = wcsstr(name, in);

      if (!pPrev) return NULL;

      while (pPrev[1] != _T('\0') && (p = wcsstr(pPrev + 1, in)) != NULL)
         pPrev = p;

      return pPrev;
   }
   else
   {
      C_STRING _name(get_name()), _in(in);

      _name.toupper();
      _in.toupper();

      pPrev = wcsstr(_name.get_name(), _in.get_name());

      if (!pPrev) return NULL;

      while (pPrev[1] != _T('\0') && (p = wcsstr(pPrev + 1, _in.get_name())) != NULL)
         pPrev = p;

      return (pPrev) ? name + (pPrev - _name.get_name()) : NULL;
   }
}
TCHAR *C_STRING::rat(C_STRING &in, int sensitive)
   { return rat(in.get_name(), sensitive); }


/*************************************************************************/
/*.doc  C_STRING::startWith                                             */  
/*+
  La funzione verifica se la stringa inizia per ...
  Parametri:
  const TCHAR *in
  int  sensitive;      sensibile al maiuscolo (default = TRUE)
-*/
/*************************************************************************/
bool C_STRING::startWith(const TCHAR *in, int sensitive)
{
   C_STRING dummy(in);
   return startWith(dummy, sensitive);
}
bool C_STRING::startWith(C_STRING &in, int sensitive)
{
   TCHAR *p = at(in, sensitive);
   C_STRING dummy(p);

   return (get_name() == p) ? true : false;
}


/*************************************************************************/
/*.doc  C_STRING::endWith                                             */  
/*+
  La funzione verifica se la stringa finisce per ...
  Parametri:
  const TCHAR *in
  int  sensitive;      sensibile al maiuscolo (default = TRUE)
-*/
/*************************************************************************/
bool C_STRING::endWith(const TCHAR *in, int sensitive)
{
   C_STRING dummy(in);
   return endWith(dummy, sensitive);
}
bool C_STRING::endWith(C_STRING &in, int sensitive)
{
   TCHAR *p = rat(in, sensitive);
   C_STRING dummy(p);

   return (p == get_name() + len() - in.len()) ? true : false;
}


/*************************************************************************/
/*.doc  C_STRING::strtran                                             */  
/*+
  La funzione sostituisce tutte le occorrenze della prima stringa con la seconda.
  Parametri:
  char *sub;
  char *con;
  int  sensitive;      sensibile al maiuscolo (default = TRUE)

  Ritorna il numero di sostituzioni avvenute.
-*/
/*************************************************************************/
int C_STRING::strtran(char *sub, char *con, int sensitive)
{
   TCHAR *UnicodeStringSub = gsc_CharToUnicode(sub); 
   TCHAR *UnicodeStringCon = gsc_CharToUnicode(con); 
   int   res = strtran(UnicodeStringSub, UnicodeStringCon, sensitive);
   if (UnicodeStringSub) free(UnicodeStringSub);
   if (UnicodeStringCon) free(UnicodeStringCon);

   return res;
}
int C_STRING::strtran(TCHAR *sub, TCHAR *con, int sensitive)
{
   TCHAR    *next, *end, *source = name;
   C_STRING buffer;                                  
   size_t   so;
   int      count = 0;
  
   if (name == NULL) return 0;

   so  = wcslen(sub);
   end = source + wcslen(source);

   while ((next = gsc_strstr(source, sub, sensitive)) != NULL)   // cerca next 'sub'
   {
      while (source < next)   // Ricopia in 'buffer' fino a next 'sub'
         { buffer += *source; source++; }
      buffer += con;          // Ricopia in 'buffer' 'con'
      source += so;           // Salta alla fine di next 'sub'
      count++;
   }
   
   while (source <= end)  // Ricopia fino alla fine (compreso '\0')
      { buffer += *source; source++; }

   set_name(buffer.get_name());

   return count;
}   


/*************************************************************************/
/*.doc  C_STRING::toi                                                    */  
/*+
  La funzione restituisce la conversione in un numero intero.
-*/
/*************************************************************************/
int C_STRING::toi(void)
{
   return (name) ? _wtoi(name) : 0;
}


/*************************************************************************/
/*.doc  C_STRING::tol                                                    */  
/*+
  La funzione restituisce la conversione in un numero lungo.
-*/
/*************************************************************************/
long C_STRING::tol(void)
{
   return (name) ? _wtol(name) : 0;
}


/*************************************************************************/
/*.doc  C_STRING::tof                                                    */  
/*+
  La funzione restituisce la conversione in un numero float.
-*/
/*************************************************************************/
double C_STRING::tof(void)
{
   return (name) ? _wtof(name) : 0;
}


/*************************************************************************/
/*.doc  C_STRING::toPt                                                    */  
/*+
  La funzione restituisce la conversione in un punto.
-*/
/*************************************************************************/
int C_STRING::toPt(ads_point Pt)
{
   return gsc_Str2Pt(name, Pt);
}


/*************************************************************************/
/*.doc  C_STRING::toHTML                                                 */  
/*+
  La funzione converte un testo perchè possa essere usato come messaggio 
  HTML.
-*/
/*************************************************************************/
void C_STRING::toHTML(void)
{
   C_STRING dummy(get_name());

   dummy.alltrim();
   if (dummy.len() == 0)
   {
      set_name(_T("&nbsp;"));
      return;
   }

   // per primo la &
   strtran(_T("&"), _T("&amp;")); // il simbolo "&" si scrive "&amp";
   // poi gli altri
   strtran(_T("<"), _T("&lt;")); // il simbolo "<" si scrive "&lt";
   strtran(_T(">"), _T("&gt;")); // il simbolo ">" si scrive "&gt";
   strtran(_T("\""), _T("&quot;")); // il simbolo " si scrive "&quot";
   strtran(_T("\'"), _T("&apos;")); // il simbolo " si scrive "&apos";
   strtran(_T("ì"), _T("&#236;")); // il simbolo " si scrive "&#236";
   strtran(_T("è"), _T("&#232;")); // il simbolo " si scrive "&#232";
   strtran(_T("é"), _T("&#233;")); // il simbolo " si scrive "&#233";
   strtran(_T("ò"), _T("&#242;")); // il simbolo " si scrive "&#242";
   strtran(_T("à"), _T("&#224;")); // il simbolo " si scrive "&#224";
   strtran(_T("ù"), _T("&#249;")); // il simbolo " si scrive "&#249";
}


/*************************************************************************/
/*.doc  C_STRING::toAXML                                                 */  
/*+
  La funzione converte un testo perchè possa essere usato come messaggio 
  AXML.
-*/
/*************************************************************************/
void C_STRING::toAXML(void)
{
   C_STRING dummy(get_name());

   dummy.alltrim();
   if (dummy.len() == 0)
   {
      set_name(_T(""));
      return;
   }

   // per primo la &
   strtran(_T("&"), _T("&amp;")); // il simbolo "&" si scrive "&amp;"
   // poi gli altri
   strtran(_T("<"), _T("&lt;")); // il simbolo "<" si scrive "&lt;"
   strtran(_T(">"), _T("&gt;")); // il simbolo ">" si scrive "&gt;"
   strtran(_T("'"), _T("&#x2019;")); // il simbolo "'" si scrive "&#x2019;"
   strtran(_T("\""), _T("&#x201D;")); // il simbolo "\" si scrive "&#x201D;"
   strtran(_T("^"), _T("&circ;")); // il simbolo "^" si scrive "&circ;"
   strtran(_T("~"), _T("&tilde;")); // il simbolo "~" si scrive "&tilde;"
   strtran(_T("ò"), _T("&#xF2;")); // il simbolo "ò" si scrive "&#xF2;"
   strtran(_T("ø"), _T("&#xD8;")); // il simbolo "ø" si scrive "&#xD8;" "
   strtran(_T("°"), _T("&#xB0;")); // il simbolo "°" si scrive "&#xB0;" "
   strtran(_T("µ"), _T("&#xB5;")); // il simbolo micro "µ" si scrive "&#xB5;
   strtran(_T("±"), _T("&#xB1;")); // il simbolo "±" si scrive "&#xB1;

   strtran(_T("À"), _T("&#xC0;")); // il simbolo "À" si scrive "&#xC0;"
   strtran(_T("Á"), _T("&#xC1;")); // il simbolo "Á" si scrive "&#xC1;"
   strtran(_T("È"), _T("&#xC8;")); // il simbolo "È" si scrive "&#xC8;"
   strtran(_T("É"), _T("&#xC9;")); // il simbolo "É" si scrive "&#xC9;"
   strtran(_T("Ì"), _T("&#xCC;")); // il simbolo "Ì" si scrive "&#xCC;"
   strtran(_T("Í"), _T("&#xCD;")); // il simbolo "É" si scrive "&#xCD;"
   strtran(_T("Ò"), _T("&#xD2;")); // il simbolo "Ò" si scrive "&#xD2;"
   strtran(_T("Ó"), _T("&#xD3;")); // il simbolo "Ó" si scrive "&#xD3;"
   strtran(_T("Ù"), _T("&#xD9;")); // il simbolo "Ù" si scrive "&#xD9;"
   strtran(_T("Ú"), _T("&#xDA;")); // il simbolo "Ú" si scrive "&#xDA;"
   strtran(_T("à"), _T("&#xE0;")); // il simbolo "à" si scrive "&#xE0;"
   strtran(_T("á"), _T("&#xE1;")); // il simbolo "á" si scrive "&#xE1;"
   strtran(_T("è"), _T("&#xE8;")); // il simbolo "è" si scrive "&#xE8;"
   strtran(_T("é"), _T("&#xE9;")); // il simbolo "é" si scrive "&#xE9;"
   strtran(_T("ì"), _T("&#xEC;")); // il simbolo "ì" si scrive "&#xE9;"
   strtran(_T("í"), _T("&#xED;")); // il simbolo "í" si scrive "&#xE9;"
   strtran(_T("ò"), _T("&#xF2;")); // il simbolo "ò" si scrive "&#xF2;"
   strtran(_T("ó"), _T("&#xF3;")); // il simbolo "ó" si scrive "&#xF3;"
   strtran(_T("ù"), _T("&#xF9;")); // il simbolo "ù" si scrive "&#xF9;"
   strtran(_T("ú"), _T("&#xFA;")); // il simbolo "ú" si scrive "&#xFA;"  
}


/************************************************************/
/*.doc C_STRING::removePrefixSuffix                         */
/*+
  Questa funzione rimuove, se esistente, il prefisso e il suffisso
  da un testo.
  Parametri:
  const char *Prefix;      prefisso (default = NULL)
  const char *Suffix;      suffisso (default = NULL)
  int        sensitive;    sensibile al maiuscolo (default = TRUE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int C_STRING::removePrefixSuffix(const char *Prefix, const char *Suffix, int sensitive)
{
   TCHAR *UnicodeStringPrefix = gsc_CharToUnicode(Prefix); 
   TCHAR *UnicodeStringSuffix = gsc_CharToUnicode(Suffix); 
   int   res = removePrefixSuffix(UnicodeStringPrefix, UnicodeStringSuffix, sensitive);
   if (UnicodeStringPrefix) free(UnicodeStringPrefix);
   if (UnicodeStringSuffix) free(UnicodeStringSuffix);

   return res;
}
int C_STRING::removePrefixSuffix(const TCHAR *Prefix, const TCHAR *Suffix, int sensitive)
{
   size_t _len;

   if (len() == 0) return GS_GOOD;
   
   // prefisso
   if ((_len = gsc_strlen(Prefix)) > 0)
      if (at(Prefix, FALSE) == get_name()) // esiste ed è all'inizio
         set_name(right(len() - _len));

   // suffisso
   if ((_len = gsc_strlen(Suffix)) > 0)
      if (rat(Suffix, FALSE) == get_name() + (len() - _len)) // esiste ed è alla fine
         set_name(left(len() - _len));

   return GS_GOOD;
}
int C_STRING::removePrefixSuffix(const TCHAR Prefix, const TCHAR Suffix, int sensitive)
{
   C_STRING PrefixStr, SuffixStr;
   
   PrefixStr = Prefix;
   SuffixStr = Suffix;
   int   res = removePrefixSuffix(PrefixStr.get_name(), SuffixStr.get_name(), sensitive);

   return res;
}


/************************************************************/
/*.doc C_STRING::addPrefixSuffix                         */
/*+
  Questa funzione aggiunge il prefisso e il suffisso a un testo.
  Parametri:
  const char *Prefix;      prefisso (default = NULL)
  const char *Suffix;      suffisso (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int C_STRING::addPrefixSuffix(const char *Prefix, const char *Suffix)
{
   TCHAR *UnicodeStringPrefix = gsc_CharToUnicode(Prefix); 
   TCHAR *UnicodeStringSuffix = gsc_CharToUnicode(Suffix); 
   int   res = addPrefixSuffix(UnicodeStringPrefix, UnicodeStringSuffix);
   if (UnicodeStringPrefix) free(UnicodeStringPrefix);
   if (UnicodeStringSuffix) free(UnicodeStringSuffix);

   return res;
}
int C_STRING::addPrefixSuffix(const TCHAR *Prefix, const TCHAR *Suffix)
{
   C_STRING Tmp(Prefix);

   Tmp += get_name();
   Tmp += Suffix;
   
   return set_name(Tmp.get_name());
}


//---------------------------------------------------------------------------//
// class C_STR                                                               //
//---------------------------------------------------------------------------//


C_STR::C_STR() : C_NODE() {}

C_STR::C_STR(int in) : C_NODE() 
{ 
   name.alloc(in); 
}
C_STR::C_STR(const char *in) : C_NODE() 
{ 
   name.set_name(in); 
}
C_STR::C_STR(const TCHAR *in) : C_NODE() 
{ 
   name.set_name(in); 
}

C_STR::~C_STR() {}

TCHAR* C_STR::get_name() 
{ 
   return name.get_name(); 
}

int C_STR::set_name(const char *in) 
{
   return name.set_name(in); 
}
int C_STR::set_name(const TCHAR *in) 
{ 
   return name.set_name(in); 
}

int C_STR::set_name(const char *in,int start,int end) 
{ 
   return name.set_name(in, start, end); 
} 
int C_STR::set_name(const TCHAR *in,int start,int end) 
{ 
   return name.set_name(in, start, end); 
} 

int C_STR::cat(const char *in) 
{ 
   return name.cat(in); 
}
int C_STR::cat(const TCHAR *in) 
{ 
   return name.cat(in); 
}

size_t C_STR::len() 
{ 
   return name.len(); 
}

int C_STR::comp(const char *in, int sensitive) 
{ 
   return name.comp(in, sensitive); 
}
int C_STR::comp(const TCHAR *in, int sensitive) 
{ 
   return name.comp(in, sensitive); 
}
int C_STR::comp(void *in) 
{ 
   return name.comp(((C_STR *) in)->get_name()); 
}

TCHAR *C_STR::cut() 
{ 
   return name.cut(); 
}

TCHAR *C_STR::paste(TCHAR *in) 
{ 
   return name.paste(in); 
}

C_STR& C_STR::operator += (const char *in) 
{ 
   cat(in);
   return (*this);
}
C_STR& C_STR::operator += (const TCHAR *in) 
{ 
   cat(in);
   return (*this);
}
C_STR& C_STR::operator += (C_STRING *in) 
{ 
   cat(in->get_name()); 
   return (*this);
}
C_STR& C_STR::operator += (C_STR *in) 
{ 
   cat(in->get_name()); 
   return (*this);
}

C_STR& C_STR::operator = (int in)
{
   name = in; 
   return (*this);
}

void C_STR::clear() 
{ 
   name.clear(); 
}

int C_STR::isDigit(void) 
{ 
   return name.isDigit(); 
}

int C_STR::toi(void) 
{ 
   return name.toi(); 
}

long C_STR::tol(void) 
{ 
   return name.tol(); 
}


//---------------------------------------------------------------------------//
// class C_STR_LIST                                                          //
//---------------------------------------------------------------------------//


C_STR_LIST::C_STR_LIST() : C_LIST() {}

C_STR_LIST::~C_STR_LIST() {}

int C_STR_LIST::copy(C_STR_LIST *out)
{
   C_STR *new_node,*punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   punt=(C_STR*)get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node= new C_STR)==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }      
      if (new_node->set_name(punt->get_name()) == GS_BAD) return GS_BAD;

      out->add_tail(new_node);
      punt=(C_STR*)get_next();
   }
   
   return GS_GOOD;
}

resbuf *C_STR_LIST::to_rb(void)
{
   resbuf *list=NULL,*rb;
   C_STR *punt;
   TCHAR str[1] = GS_EMPTYSTR, *string;

   punt=(C_STR*)get_tail();
   if (punt == NULL) 
      if ((list = acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }

   while (punt!=NULL)
   {                                
      if ((string=punt->get_name())==NULL) string=str;

      if ((rb=acutBuildList(RTSTR,string,0))==NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext=list;
      list=rb; punt=(C_STR*)get_prev();
   }                             
   return list;      
}

int C_STR_LIST::from_rb(resbuf *rb)
{
   C_STR *nod;
   TCHAR *str;

   remove_all();

   if (rb==NULL)           { GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   if (rb->restype==RTNIL) { return GS_GOOD; }

   if (rb->restype==RTLB)
      {
      while((rb=rb->rbnext)!=NULL && rb->restype!=RTLE)
      {
         if (rb->restype==RTSTR)
         {
            str=rb->resval.rstring;
            if ((nod = new C_STR((int) wcslen(str)+1))==NULL)
               { GS_ERR_COD=eGSOutOfMem; remove_all(); return GS_BAD; }
            add_tail(nod);
            if (nod->set_name(str)==GS_BAD) { remove_all(); return GS_BAD; }
            }
         else { GS_ERR_COD=eGSInvRBType; remove_all(); return GS_BAD; }
         }
      if (rb==NULL)
           { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      return GS_GOOD;
   }

   GS_ERR_COD=eGSInvRBType;
   return GS_BAD; 
}
//-----------------------------------------------------------------------//
// La stringa ha gli elementi separati da <sep> (default = ',')
int C_STR_LIST::from_str(const char *str, char sep)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(str); 
   TCHAR UnicodeChar = gsc_CharToUnicode(sep); 
   int   res = from_str(UnicodeString, sep);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STR_LIST::from_str(const TCHAR *str, TCHAR sep)
{
   TCHAR    *p;
   C_STRING Buffer(str);
   C_STR    *pStr;
   int      n, i;

   remove_all();
   if (Buffer.len() == 0) return GS_GOOD;

   p = Buffer.get_name();
   n = gsc_strsep(p, _T('\0'), sep) + 1;

   for (i = 0; i < n; i++)
   {
      if ((pStr = new C_STR(p)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(pStr);
      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}
// La stringa ha gli elementi separati <sep> (default = ',')
// N.B.: Alloca memoria
TCHAR* C_STR_LIST::to_str(char sep)
{
   TCHAR UnicodeChar = gsc_CharToUnicode(sep);

   return to_str(UnicodeChar);
}
TCHAR* C_STR_LIST::to_str(TCHAR sep)
{
   C_STRING Buffer;
   C_STR    *pStr;

   pStr = (C_STR *) get_head();
   while (pStr)
   {
      Buffer += pStr->get_name();
      Buffer += sep;
      pStr = (C_STR *) get_next();
   }
   
   if (Buffer.len() > 0) Buffer.set_chr(_T('\0'), Buffer.len() - 1);

   return gsc_tostring(Buffer.get_name());
}


/*********************************************************/
/*.doc C_STR_LIST::save                       <external> */
/*+
  Questa funzione salva la lista nel file indicato.
  Parametri:
  const char *Path; Path di un file (se esiste verrà distrutto)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_STR_LIST::save(const char *Path)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(Path); 
   int   res = save(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STR_LIST::save(const TCHAR *Path)
{
   FILE  *f;
   C_STR *p;

   if ((f = gsc_fopen(Path, _T("w"))) == NULL) return GS_BAD;
   p = (C_STR *) get_head();
   while (p)
   {      
      if (fwprintf(f, _T("%s\n"), p->get_name()) < 0) { gsc_fclose(f); return GS_BAD; }
      p = (C_STR *) get_next();
   }
   if (gsc_fclose(f) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_STR_LIST::load                       <external> */
/*+
  Questa funzione carica il set dal file indicato.
  Parametri:
  const char *Path;           Path del file del set

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_STR_LIST::load(const char *Path)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(Path); 
   int   res = load(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_STR_LIST::load(const TCHAR *Path)
{
   FILE     *f;
   C_STR    *p;
   C_STRING Buffer;
   bool     Unicode;

   if ((f = gsc_fopen(Path, _T("r"), MORETESTS, &Unicode)) == NULL) return GS_BAD;
   remove_all();
   while (gsc_readline(f, Buffer, Unicode) == GS_GOOD)
   {      
      if ((p = new C_STR(Buffer.get_name())) == NULL) return GS_BAD;
      add_tail(p);
   }
   if (gsc_fclose(f) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_STR_LIST::getRecursiveList           <external> */
/*+
  Questa funzione restituisce la lista esaminando che ciascun 
  elemento non si riferisca ad una path di un file. In quel caso
  verrà aperto quel file e caricata la lista anche dal quel file.
  La ricorsività infinita non viene controllata.
  Parametri:
  C_STR_LIST &RecursiveList;  Lista
  const char *Path;           Opzionale; path del file del set (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_STR_LIST::getRecursiveList(C_STR_LIST &RecursiveList, const TCHAR *Path)
{
   C_STR    *pItem, *p;
   C_STRING Buffer;

   RecursiveList.remove_all();
   if (Path) // carico la lista utilizzando il file indicato
      if (load(Path) == GS_BAD) return GS_BAD;

   pItem = (C_STR *) get_head();
   while (pItem)
   {
      // se non è una path di file esistente
      if (gsc_path_exist(pItem->get_name(), GS_BAD) == GS_BAD)
      {
         if ((p = new C_STR(pItem->get_name())) == NULL) return GS_BAD;
         RecursiveList.add_tail(p);
      }
      else // si tratta di un sotto set
      {
         C_STR_LIST SubList, dummy;

         if (dummy.getRecursiveList(SubList, pItem->get_name()) == GS_BAD) return GS_BAD;
         RecursiveList.paste_tail(SubList);
      }

      pItem = (C_STR *) get_next();
   }
   
   return GS_GOOD;
}


/**************************************************************/
/*.doc C_STR_LIST::getFirstTrueGEOLispCond                    */
/*+
  Funzione che, leggendo una lista di valori, restituisce la prima condizione vera.
  La funzione è applicata per decidere quale lista di valori associare ad un
  attributo in fase di editing (es. se materiale acciaio allora si propone 
  una certa lista di diametri relativi a quel materiale)
  Parametri:
  C_RB_LIST &ColValues;   	  Lista ((<nome colonna><valore>) ...) di tutti gli 
     					      	  attributi della classe

  Ritorna il puntatore alla prima condizione vera in caso di successo altrimenti NULL.
-*/                                             
/**************************************************************/
C_STR* C_STR_LIST::getFirstTrueGEOLispCond(C_RB_LIST &ColValues)
   { return getFirstTrueGEOLispCond(ColValues.get_head()); }
C_STR* C_STR_LIST::getFirstTrueGEOLispCond(presbuf ColValues)
{
   C_STR   *pCond = (C_STR *) get_head();
   int     i = 0, result = GS_GOOD;
   presbuf pRbAttr, pRb;
   TCHAR   *attrib_name, value[2 * MAX_LEN_FIELD];
   size_t  ptr;

   if (!pCond) return NULL;

   // carico le variabili locali a "GEOsim"
   while ((pRbAttr = gsc_nth(i++, ColValues)) != NULL)
   {
      pRb = gsc_nth(0, pRbAttr);

      if (pRb->restype != RTSTR) 
         { GS_ERR_COD = eGSInvRBType; result = GS_BAD; break; }
      attrib_name = pRb->resval.rstring;   // Nome variabile = attributo     

      pRb = gsc_nth(1, pRbAttr);
      // Conversione resbuf in stringa lisp
      if (rb2lspstr(pRb, value) == GS_BAD) { result = GS_BAD; break; }
      // senza messaggi di errore
      if (alloc_var(FALSE, NULL, &ptr, _T('L'), _T("GEOsim"), attrib_name, value, NULL) == GS_BAD) 
         { result = GS_BAD; break; }
   }

   if (result == GS_BAD)
      { release_var(_T("GEOsim")); GS_ERR_COD = eGSInvRBType; return NULL; }

   while (pCond)
   {
      // senza messaggi di errore
      if ((pRb = gsc_exec_lisp(_T("GEOsim"), pCond->get_name(), NULL, FALSE)) != NULL)
      {
         if (pRb->restype != RTNIL) { acutRelRb(pRb); break; }
         acutRelRb(pRb);
      }   
      pCond = (C_STR *) get_next();
   }
   release_var(_T("GEOsim")); // Scarico le variabili locali "GEOsim"

   return pCond;
}


/**************************************************************/
/*.doc C_STR_LIST::add_tail_str                               */
/*+
  Funzione che aggiunge in coda una stringa
  nella lista.
  Parametri:
  const TCHAR *Value;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_STR_LIST::add_tail_str(const TCHAR *Value)
{
   C_STR *p;

   if ((p = new C_STR(Value)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   return add_tail(p);
}


/**************************************************************/
/*.doc C_STR_LIST::add_tail_unique                    */
/*+
  Funzione che aggiunge in coda una stringa solo se non già presente
  nella lista.
  Parametri:
  const TCHAR *Value;
  int sensitive;

  oppure
  C_STR_LIST &in;
  int sensitive;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_STR_LIST::add_tail_unique(const TCHAR *Value, int sensitive)
{
   C_STR *p = (C_STR *) search_name(Value, sensitive);
 
   if (p) return GS_GOOD;

   if ((p = new C_STR(Value)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   return add_tail(p);
}
int C_STR_LIST::add_tail_unique(C_STR_LIST &in, int sensitive)
{
   C_STR *p = (C_STR *) in.get_head();
 
   while (p)
   {
      if (add_tail_unique(p->get_name(), sensitive) == GS_BAD) return GS_BAD;
      p = (C_STR *) in.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_STR_LIST::listIntersect              <external> */
/*+
  Questa funzione filtra solo gli elementi che sono presenti 
  anche nella lista indicata dal parametro <in>.
  Parametri:
  C_STR_LIST &ref_list;    lista di confronto
  int        sensitive;    sensibile al maiuscolo (default = TRUE)

-*/  
/*********************************************************/
void C_STR_LIST::listIntersect(C_STR_LIST &ref_list, int sensitive)
{
   C_STR *p = (C_STR *) get_head();
 
   while (p)
      if (ref_list.search_name(p->get_name(), sensitive) == NULL)
      {
         remove_at(); // cancella e va al successivo
         p = (C_STR *) get_cursor();
      }
      else      
         p = (C_STR *) get_next();
}


/*********************************************************/
/*.doc C_STR_LIST::listUnion                  <external> */
/*+
  Questa funzione unisce (senza duplicazione) gli elementi della lista 
  indicata dal parametro <in>.
  Parametri:
  C_STR_LIST &ref_list;    lista di confronto
  int        sensitive;    sensibile al maiuscolo (default = TRUE)

-*/  
/*********************************************************/
void C_STR_LIST::listUnion(C_STR_LIST &ref_list, int sensitive)
{
   C_STR *p_ref = (C_STR *) ref_list.get_head();
 
   while (p_ref)
   {
      add_tail_unique(p_ref->get_name(), sensitive);
      
      p_ref = (C_STR *) ref_list.get_next();
   }
}


//---------------------------------------------------------------------------//
// class C_INT_STR                                                           //
//---------------------------------------------------------------------------//

C_INT_STR::C_INT_STR() : C_STR()
{
   key = 0; 
}

C_INT_STR::C_INT_STR(int _key, const TCHAR *_name) : C_STR()
{
   key = _key;
   set_name(_name);
}

C_INT_STR::~C_INT_STR() {}

int C_INT_STR::get_key()  
{ 
   return key; 
}

int C_INT_STR::set_key(int in)  
{ 
   key = in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_INT_STR_LIST                                                      //
//---------------------------------------------------------------------------//

C_INT_STR_LIST::C_INT_STR_LIST() : C_LIST() {}

C_INT_STR_LIST::~C_INT_STR_LIST() {}


/**************************************************************/
/*.doc C_INT_STR_LIST::add_tail_int_str                       */
/*+
  Funzione che aggiunge in coda una intero-stringa
  nella lista.
  Parametri:
  int _val1;
  const TCHAR *_val2;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_INT_STR_LIST::add_tail_int_str(int _val1, const TCHAR *_val2)
{
   C_INT_STR *p;

   if ((p = new C_INT_STR(_val1, _val2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   return add_tail(p);
}


resbuf *C_INT_STR_LIST::to_rb(void)
{
   presbuf   list = NULL, rb;
   C_INT_STR *punt;
   TCHAR     str[1] = GS_EMPTYSTR, *string;
            
   punt = (C_INT_STR *) get_tail();
   if (!punt) 
      if ((list = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   while (punt)
   {
      if ((string = punt->get_name()) == NULL) string = str;
                                         
      if ((rb = acutBuildList(RTLB, RTSHORT, punt->get_key(),
                              RTSTR, string, RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      rb->rbnext->rbnext->rbnext->rbnext = list;
      list = rb;
      punt = (C_INT_STR *) get_prev();
   }                             
   
   return list;      
}

int C_INT_STR_LIST::from_rb(resbuf *rb)
{
   C_INT_STR *nod;
   int       val_1;
   TCHAR     *str;

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if (rb->restype == RTNIL) { remove_all(); return GS_GOOD; }
   if (rb->restype != RTLB)  { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   remove_all();
   while ((rb = rb->rbnext) != NULL && rb->restype != RTLE)
   {      
      if (rb->restype != RTLB)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      if ((rb = rb->rbnext) == NULL || rb->restype != RTSHORT)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      val_1 = rb->resval.rint;

      if ((rb = rb->rbnext) == NULL || rb->restype != RTSTR)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      str = rb->resval.rstring;

      if ((rb=rb->rbnext)==NULL || rb->restype != RTLE)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }        

      if ((nod = new C_INT_STR(val_1, str)) == NULL)
         { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(nod);
   }

   return GS_GOOD;
}

int C_INT_STR_LIST::from_str(const TCHAR *str, TCHAR Sep)
{
   TCHAR     *p;
   C_STRING  Buffer(str);
   C_INT_STR *pIntStr;
   int       n, i;

   remove_all();
   if (Buffer.len() == 0) return GS_GOOD;

   p = Buffer.get_name();
   n = gsc_strsep(p, _T('\0'), Sep) + 1;

   for (i = 0; i < n; i++)
   {
      if (i % 2 == 0)
      {
         if ((pIntStr = new C_INT_STR()) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pIntStr->set_key(_wtoi(p));
         add_tail(pIntStr);
      }
      else pIntStr->set_name(p);

      while (*p != _T('\0')) p++; p++;
   }

   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_INT_INT_STR                                                       //
//---------------------------------------------------------------------------//

C_INT_INT_STR::C_INT_INT_STR() : C_STR() 
{ 
   key=type=0; 
}

C_INT_INT_STR::C_INT_INT_STR(int in) : C_STR() 
{ 
   key=type=0; 
   name.alloc(in); 
}

C_INT_INT_STR::~C_INT_INT_STR() {}

int C_INT_INT_STR::get_key()  
{ 
   return key; 
}

int C_INT_INT_STR::set_key(int in)  
{ 
   key=in; 
   return GS_GOOD; 
}

int C_INT_INT_STR::get_type() 
{ 
   return type; 
}

int C_INT_INT_STR::set_type(int in) 
{ 
   type = in; 
   return GS_GOOD; 
}



//---------------------------------------------------------------------------//
// class C_INT_INT_STR_LIST                                                  //
//---------------------------------------------------------------------------//

C_INT_INT_STR_LIST::C_INT_INT_STR_LIST() : C_LIST() {}

C_INT_INT_STR_LIST::~C_INT_INT_STR_LIST() {}

resbuf *C_INT_INT_STR_LIST::to_rb(void)
{
   resbuf        *list = NULL,*rb;
   C_INT_INT_STR *punt;
   TCHAR         str[1] = GS_EMPTYSTR, *string;
            
   punt = (C_INT_INT_STR*)get_tail();
   if (punt == NULL) 
      if ((list = acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }

   while (punt != NULL)
   {
      if ((string=punt->get_name())==NULL) string=str;
                                         
      if ((rb=acutBuildList(RTLB,RTSHORT,punt->get_key(),
                            RTSTR,string,
                            RTSHORT,punt->get_type(),RTLE,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext->rbnext->rbnext->rbnext=list;
      list=rb; punt=(C_INT_INT_STR*)get_prev();
   }                             
   return list;      
}

int C_INT_INT_STR_LIST::from_rb(resbuf *rb)
{
   C_INT_INT_STR *nod;
   int           val_1,val_2;
   TCHAR         *str;

   if (rb==NULL)           { GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   if (rb->restype==RTNIL) { remove_all(); return GS_GOOD; }
   if (rb->restype!=RTLB)  { GS_ERR_COD=eGSInvRBType; return GS_BAD; }

   remove_all();
   while((rb=rb->rbnext)!=NULL && rb->restype!=RTLE)
   {      
      if (rb->restype!=RTLB)
               { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      if ((rb=rb->rbnext)==NULL || rb->restype!=RTSHORT)
               { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      val_1=rb->resval.rint;

      if ((rb=rb->rbnext)==NULL || rb->restype!=RTSTR)
               { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      str=rb->resval.rstring;

      if((rb=rb->rbnext)==NULL || rb->restype!=RTSHORT)
               { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      val_2=rb->resval.rint;
      
      if((rb=rb->rbnext)==NULL || rb->restype!=RTLE)
               { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }        

      if ((nod=new C_INT_INT_STR((int) wcslen(str)+1))==NULL)
         { remove_all(); GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      add_tail(nod);
      nod->set_key(val_1);
      nod->set_type(val_2);
      if (nod->set_name(str)==GS_BAD) { remove_all(); return GS_BAD; }
   }

   return GS_GOOD;
}

C_INT_INT_STR *C_INT_INT_STR_LIST::search(int aaa, int bbb)
{
   C_INT_INT_STR *punt;

   punt = (C_INT_INT_STR *) get_head();
   while (punt != NULL)
   {
      if (aaa == punt->get_key())
         if (bbb == punt->get_type()) break;
      punt = (C_INT_INT_STR *) get_next();
   }

   return punt;
}

int C_INT_INT_STR_LIST::copy(C_INT_INT_STR_LIST *out)
{
   C_INT_INT_STR *new_node,*punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   punt=(C_INT_INT_STR*)get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node= new C_INT_INT_STR)==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }      
      new_node->key  = punt->key;
      new_node->type = punt->type;
      if (new_node->set_name(punt->get_name()) == GS_BAD) return GS_BAD;

      out->add_tail(new_node);
      punt=(C_INT_INT_STR*)get_next();
   }
   
   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_2STR                                                              //
//---------------------------------------------------------------------------//

C_2STR::C_2STR() : C_STR() {}
      
C_2STR::C_2STR(int in1, int in2) : C_STR(in1) 
{ 
   name2.alloc(in2); 
}
C_2STR::C_2STR(const char *in1, const char *in2) : C_STR() 
{
   name.set_name(in1); 
   name2.set_name(in2); 
}
C_2STR::C_2STR(const TCHAR *in1, const TCHAR *in2) : C_STR() 
{ 
   name.set_name(in1); 
   name2.set_name(in2); 
}

C_2STR::~C_2STR() {}

TCHAR* C_2STR::get_name2() 
{ 
   return name2.get_name(); 
}

int C_2STR::set_name2(const char *in) 
{ 
   return name2.set_name(in); 
} 
int C_2STR::set_name2(const TCHAR *in) 
{ 
   return name2.set_name(in); 
} 

void C_2STR::clear() 
{ 
   C_STR::clear(); 
   name2.clear(); 
}


//---------------------------------------------------------------------------//
// class C_2STR_LIST                                                         //
//---------------------------------------------------------------------------//

C_2STR_LIST::C_2STR_LIST() : C_LIST() {}

C_2STR_LIST::C_2STR_LIST(const char *in1, const char *in2) : C_LIST()
{
   C_2STR *pItem;

   if ((pItem = new C_2STR(in1, in2)) != NULL)
      add_tail(pItem);
}
C_2STR_LIST::C_2STR_LIST(const TCHAR *in1, const TCHAR *in2) : C_LIST()
{
   add_tail_2str(in1, in2);
}

C_2STR_LIST::~C_2STR_LIST() {}

int C_2STR_LIST::add_tail_2str(const TCHAR *in1, const TCHAR *in2)
{
   C_2STR *pItem;

   if ((pItem = new C_2STR(in1, in2)) == NULL) return GS_BAD;
   add_tail(pItem);

   return GS_GOOD;
}

C_2STR *C_2STR_LIST::search_names(char *first, char* second, int sensitive)
{
   TCHAR *UnicodeStringFirst  = gsc_CharToUnicode(first); 
   TCHAR *UnicodeStringSecond = gsc_CharToUnicode(second); 
   C_2STR *res = search_names(UnicodeStringFirst, UnicodeStringSecond, sensitive);
   if (UnicodeStringFirst) free(UnicodeStringFirst);
   if (UnicodeStringSecond) free(UnicodeStringSecond);

   return res;
}
C_2STR *C_2STR_LIST::search_names(TCHAR *first, TCHAR *second, int sensitive)
{
   cursor = head;
   while (cursor)
   { 
      if (gsc_strcmp(cursor->get_name(), first, sensitive) == 0 &&
          gsc_strcmp(cursor->get_name2(), second, sensitive) == 0)
         return (C_2STR *) cursor;
      get_next();
   }

   return NULL;
}

int C_2STR_LIST::how_many_times(char *first, int *num)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(first); 
   int   res = how_many_times(UnicodeString, num);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_2STR_LIST::how_many_times(TCHAR *first, int *num)
{
   cursor=head;
   while(cursor!=NULL)
   { 
      if (gsc_strcmp(cursor->get_name(),first)==0)
         (*num)++;      
      get_next();
   }

   return GS_GOOD;
}

int C_2STR_LIST::from_rb(resbuf *rb)
{
   C_2STR *nod;
   TCHAR  *str;

   remove_all();

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   
   if (rb->restype == RTLB)
   {
      while ((rb = rb->rbnext) != NULL && rb->restype != RTLE)
      {
         if (rb->restype != RTLB || !(rb = rb->rbnext) || rb->restype != RTSTR)
            { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }
         str = rb->resval.rstring;
         if (!(rb = rb->rbnext) || rb->restype != RTSTR)
            { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }

         if ((nod = new C_2STR(str, rb->resval.rstring)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
         add_tail(nod);

         if (!(rb = rb->rbnext) || rb->restype != RTLE)
            { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }
      }
      if (!rb) { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      
      return GS_GOOD;
   }

   GS_ERR_COD = eGSInvRBType;
   
   return GS_BAD; 
}

resbuf *C_2STR_LIST::to_rb(void)
{
   resbuf *list = NULL, *rb;
   C_2STR *punt;
   TCHAR  str[] = GS_EMPTYSTR, *string;

   punt = (C_2STR *)get_tail();
   if (!punt) return NULL;

   while (punt)
   {                                
      if ((string = punt->get_name2()) == NULL) string = str;
      if ((rb = acutBuildList(RTSTR, string, RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext = list;
      list = rb;

      if ((string = punt->get_name()) == NULL) string = str;
      if ((rb = acutBuildList(RTLB, RTSTR, string, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext = list;
      list = rb;

      punt = (C_2STR *) get_prev();
   }                             
   
   return list;      
}

int C_2STR_LIST::copy(C_2STR_LIST &out)
{
   C_2STR *new_node,*punt;

   punt = (C_2STR *) get_head();
   out.remove_all();
   
   while (punt)
   {
      if ((new_node = new C_2STR(punt->get_name(), punt->get_name2())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      

      out.add_tail(new_node);
      punt = (C_2STR *) get_next();
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_is_C_2STR_LIST_referred_to_DB        <external> */
/*+
  Questa funzione restituisce vero se la lista contiene informazioni 
  che si riferiscono a collegamento a DB. In caso afferamtivo la funzione 
  inizializza le proprietà di connessione a DB.
  Usata per la gestione TAB, REF, FDF, DEF.
  Parametri:
  C_2STR_LIST &List;          Lista di valori (input)
  C_STRING &ConnStrUDLFile;   Proprietà di connessione (output)
  C_STRING &UdlProperties;    Proprietà di connessione (output)
  C_STRING &SelectStm;        Proprietà di connessione (output)

  Restituisce true in caso di successo altrimenti restituisce false.
-*/  
/*********************************************************/
bool gsc_is_C_2STR_LIST_referred_to_DB(C_2STR_LIST &List, C_STRING &ConnStrUDLFile,
                                     C_STRING &UdlProperties, C_STRING &SelectStm)
{
   C_2STR *p;

   // La lista deve essere con 3 elementi
   if (List.get_count() != 3) return false;

   if ((p = (C_2STR *) List.search_name(_T("CONNSTRUDLFILE"))) == NULL) return false;
   ConnStrUDLFile = p->get_name2();
   ConnStrUDLFile.alltrim();

   if ((p = (C_2STR *) List.search_name(_T("UDLPROPERTIES"))) == NULL) return false;
   UdlProperties = p->get_name2();
   UdlProperties.alltrim();

   // (per compatibilità con versione passate verifico anche SELECTSTAMENT)
   if ((p = (C_2STR *) List.search_name(_T("SELECTSTATEMENT"))) == NULL)
      if ((p = (C_2STR *) List.search_name(_T("SELECTSTAMENT"))) == NULL) return false;
   SelectStm = p->get_name2();
   SelectStm.alltrim();

   return true;
}


/*********************************************************/
/*.doc C_2STR_LIST::load                       <external> */
/*+
  Questa funzione carica la lista dal file indicato.
  Usata per la gestione TAB, REF, FDF, DEF.
  Parametri:
  const char *Path;  Path del file
  char Sep;          Carattere separatore (Default = ',')
  const char *Sez;   Sezione nel file (Default = NULL)
                     Se Sez = NULL viene caricato solo il testo fuori dalle sezioni
                     altrimenti viene caricato il contenuto della sezione indicata
  bool fromDB;       Flag, se = true e la lista di valori letta dal
                     file rappresenta una connessione a DB allora vengono
                     letti i valori dall'istruzione SQL specificata
                     (default =  false).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_2STR_LIST::load(const char *Path, char Sep, const char *Sez, bool fromDB,
                      C_RB_LIST *pParamValues)
{
   TCHAR *UnicodeStringPath = gsc_CharToUnicode(Path); 
   TCHAR UnicodeChar = gsc_CharToUnicode(Sep);
   TCHAR *UnicodeStringSez  = gsc_CharToUnicode(Sez); 
   int  Res = load(UnicodeStringPath, UnicodeChar, UnicodeStringSez, fromDB,
                   pParamValues);
   if (UnicodeStringPath) free(UnicodeStringPath);
   if (UnicodeStringSez) free(UnicodeStringSez);

   return Res;
}
int C_2STR_LIST::load(const TCHAR *Path, TCHAR Sep, const TCHAR *Sez, bool fromDB,
                      C_RB_LIST *pParamValues)
{
   FILE *f;
   int  Res = GS_GOOD;
   bool Unicode = false;

   if ((f = gsc_fopen(Path, _T("rb"), MORETESTS, &Unicode)) == NULL) return GS_BAD;
   if (load(f, Sep, Sez, fromDB, Unicode, pParamValues) == GS_BAD) Res = GS_BAD;
   if (gsc_fclose(f) == GS_BAD) return GS_BAD;

   return Res;
}
int C_2STR_LIST::load(FILE *f, char Sep, const char *Sez, bool fromDB, bool Unicode,
                      C_RB_LIST *pParamValues)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(Sez); 
   TCHAR UnicodeChar = gsc_CharToUnicode(Sep);
   int   res = load(f, UnicodeChar, UnicodeString, fromDB, Unicode, pParamValues);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int C_2STR_LIST::load(FILE *f, TCHAR Sep, const TCHAR *Sez, bool fromDB, bool Unicode,
                      C_RB_LIST *pParamValues)
{
   C_STRING Buffer, BuffSez;
   bool     InSez = false;
   long     pos, Prev;
   int      Res = GS_GOOD;

   if (gsc_strlen(Sez) > 0)
   {
      BuffSez = _T('[');
      BuffSez += Sez;
      BuffSez += _T(']');
   }

   remove_all();
   
   pos = ftell(f);                 // posizione attuale
   if (fseek(f, 0, SEEK_SET) != 0) // vado a inizio file
      { GS_ERR_COD = eGSReadFile; return GS_BAD; }

   // Cerco la sezione o la lista di default (senza sezione)
   Prev = 0;
   while (gsc_readline(f, Buffer, Unicode) == GS_GOOD)
   {
      Buffer.alltrim();
      if (Buffer.len() == 0)
      {
         InSez = false;    // fine lista quando trova una riga vuota
         Prev = ftell(f);  // posizione attuale
         continue;
      }

      if (!InSez && Buffer.get_chr(0) == _T('[') && Buffer.get_chr(Buffer.len() - 1) == _T(']'))
         InSez = true; // trovato inizio sezione

      // Se si sta cercando una sezione e si è dentro una sezione
      if (BuffSez.len() && InSez)
      {
         if (Buffer.comp(BuffSez) == 0) // trovata inizio sezione
            { Res = load(f, Sep, fromDB, Unicode, pParamValues); break; }
      }
      else
         // Se non si sta cercando una sezione e non si è dentro una sezione
         if (BuffSez.len() == 0 && !InSez)
         {
            if (fseek(f, Prev, SEEK_SET) != 0) // torno indietro di una riga
               { GS_ERR_COD = eGSReadFile; Res = GS_BAD; break; }
            Res = load(f, Sep, fromDB, Unicode, pParamValues);
            break;
         }

      Prev = ftell(f);                 // posizione attuale
   }

   fseek(f, pos, SEEK_SET);  // Ritorna posizione iniziale

   return Res;
}
int C_2STR_LIST::load(FILE *f, char Sep, bool fromDB, bool Unicode, 
                      C_RB_LIST *pParamValues)
{
   TCHAR UnicodeChar = gsc_CharToUnicode(Sep); 
   return load(f, UnicodeChar, fromDB, Unicode);
}
int C_2STR_LIST::load(FILE *f, TCHAR Sep, bool fromDB, bool Unicode,
                      C_RB_LIST *pParamValues)
{
   C_2STR   *p;
   TCHAR    *str;
   C_STRING Buffer;

   while (gsc_readline(f, Buffer, Unicode) == GS_GOOD)
   {
      Buffer.alltrim();
      if (Buffer.len() == 0) break; // fine lista quando trova una riga vuota

      if ((str = Buffer.at(Sep)) != NULL)
      {
         *str = _T('\0');
         str++;
      }
      if ((p = new C_2STR(Buffer.get_name(), str)) == NULL) return GS_BAD;

      add_tail(p);
   }

   // Se non si vuole leggere da DB
   if (!fromDB) return GS_GOOD;

   C_STRING ConnStrUDLFile, UdlProperties, SelectStm;

   // Se le righe lette contengono il collegamento ad un DB
   // allora la lista dei valori va letta dal DB
   if (gsc_is_C_2STR_LIST_referred_to_DB(*(this), ConnStrUDLFile, 
                                       UdlProperties, SelectStm))
      if (gsc_C_2STR_LIST_load(*this, ConnStrUDLFile, UdlProperties, 
                               SelectStm, pParamValues) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_C_2STR_LIST_load_by_code           <external> */
/*+
  Questa funzione carica la lista dal file indicato cercando
  l'elemento con codice noto.
  Usata per la gestione TAB, REF.
  Parametri:
  FILE *f;           File
  const TCHAR *Code; Codice di ricerca
  TCHAR Sep;         Carattere separatore (Default = ',')
  const TCHAR *Sez;  Sezione nel file (Default = NULL)
                     Se Sez = NULL viene caricato solo il testo fuori dalle sezioni
                     altrimenti viene caricato il contenuto della sezione indicata
  bool fromDB;       Flag, se = true e la lista di valori letta dal
                     file rappresenta una connessione a DB allora vengono
                     letti i valori dall'istruzione SQL specificata
                     (default =  false).
   bool  Unicode;    Flag che determina se il contenuto del file è in 
                     formato UNICODE o ANSI (default = false)

  Restituisce un puntatore a C_2STR in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria
-*/  
/*********************************************************/
C_2STR *gsc_C_2STR_LIST_load_by_code(FILE *f, const TCHAR *Code, TCHAR Sep, 
                                     const TCHAR *Sez, bool fromDB, bool Unicode,
                                     C_RB_LIST *pParamValues)
{
   return gsc_C_2STR_LIST_load_by_code(f, Code, Sep, Sez, fromDB, Unicode, 
                                       (pParamValues) ? pParamValues->get_head() : NULL);
}
C_2STR *gsc_C_2STR_LIST_load_by_code(FILE *f, const TCHAR *Code, TCHAR Sep, 
                                     const TCHAR *Sez, bool fromDB, bool Unicode,
                                     presbuf pParamValues)
{
   C_2STR      *p;
   C_2STR_LIST ValueList;
   C_STRING    Buffer;

   // Leggo le righe della sezione (senza leggere da DB)
   if (ValueList.load(f, Sep, Sez, false, Unicode) == GS_BAD) return NULL;

   // Se si vuole leggere da DB e
   // Se le righe lette contengono il collegamento ad un DB
   // allora la lista dei valori va letta dal DB
   if (fromDB)
   {
      C_STRING ConnStrUDLFile, UdlProperties, OldSelectStm;

      if (gsc_is_C_2STR_LIST_referred_to_DB(ValueList, ConnStrUDLFile, 
                                          UdlProperties, OldSelectStm))
      {
         C_STRING NewSelectStm, NewSelectStm2;
         TCHAR    *pStr1, *pStr2, CodeExpr[100];

         // modifico l'istruzione SQL
         // se l'istruzione è nella forma "SELECT DISTINCTROW A,B FROM TAB" (solo ACCESS)
         if ((pStr1 = OldSelectStm.at(_T("SELECT DISTINCTROW "), GS_BAD)) != NULL)
            pStr1 += gsc_strlen(_T("SELECT DISTINCTROW "));
         else
            // se l'istruzione è nella forma "SELECT DISTINCT A,B FROM TAB"
            if ((pStr1 = OldSelectStm.at(_T("SELECT DISTINCT "), GS_BAD)) != NULL)
               pStr1 += gsc_strlen(_T("SELECT DISTINCT "));
            else
               // se l'istruzione è nella forma "SELECT A,B FROM TAB"
               if ((pStr1 = OldSelectStm.at(_T("SELECT "), GS_BAD)) != NULL)
                  pStr1 += gsc_strlen(_T("SELECT "));
               else 
                  return NULL;

         if ((pStr2 = OldSelectStm.at(_T(","), GS_BAD)) == NULL) return NULL;
         if ((pStr2 - pStr1) > 99) return NULL; // 100 -1
         wcsncpy(CodeExpr, pStr1, pStr2 - pStr1);
         CodeExpr[pStr2 - pStr1] = _T('\0'); // aggiungo un fine stringa

         // Verifico esistenza di " ORDER BY " e lo elimino
         if ((pStr1 = OldSelectStm.rat(_T(" ORDER BY "), GS_BAD)) != NULL)
            NewSelectStm.set_name(OldSelectStm.get_name(), 0, (int) (pStr1 - OldSelectStm.get_name()));
         else
            NewSelectStm = OldSelectStm;

         if (NewSelectStm.rat(_T(" WHERE "), GS_BAD) == NULL)
            NewSelectStm += _T(" WHERE (");
         else
         {
            NewSelectStm.strtran(_T(" WHERE "), _T(" WHERE ("), GS_BAD);
            NewSelectStm += _T(") AND (");
         }

         NewSelectStm += CodeExpr;
         NewSelectStm += _T("=");
         NewSelectStm2 = NewSelectStm;
         NewSelectStm += Code;
         NewSelectStm += _T(")");
         
         // suppongo che CodeExpr abbia valore numerico
         if (gsc_C_2STR_LIST_load(ValueList, ConnStrUDLFile, UdlProperties, 
                                  NewSelectStm, pParamValues) == GS_BAD)
         {
            NewSelectStm2 += _T("'"); // attenzione: non è detto che sia giusto
            NewSelectStm2 += Code;    // dipende dal tipo di DB
            NewSelectStm2 += _T("')");
            // suppongo che CodeExpr abbia valore carattere
            if (gsc_C_2STR_LIST_load(ValueList, ConnStrUDLFile, UdlProperties, 
                                     NewSelectStm2, pParamValues) == GS_BAD)
               return NULL;
         }
      }
   }

   if ((p = (C_2STR *) ValueList.search_name(Code)))
      return new C_2STR(p->get_name(), p->get_name2());
   else 
      return NULL;
}


/*********************************************************/
/*.doc gsc_C_2STR_LIST_load_by_descr           <external> */
/*+
  Questa funzione carica la lista dal file indicato cercando
  l'elemento con descrizione nota.
  Usata per la gestione TAB, REF.
  Parametri:
  FILE *f;            File
  const TCHAR *Descr; Codice di ricerca
  TCHAR Sep;          Carattere separatore (Default = ',')
  const TCHAR *Sez;   Sezione nel file (Default = NULL)
                      Se Sez = NULL viene caricato solo il testo fuori dalle sezioni
                      altrimenti viene caricato il contenuto della sezione indicata
  bool fromDB;        Flag, se = true e la lista di valori letta dal
                      file rappresenta una connessione a DB allora vengono
                      letti i valori dall'istruzione SQL specificata
                      (default =  false).
   bool  Unicode;     Flag che determina se il contenuto del file è in 
                      formato UNICODE o ANSI (default = false)

  Restituisce un puntatore a C_2STR in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria
-*/  
/*********************************************************/
C_2STR *gsc_C_2STR_LIST_load_by_descr(FILE *f, const TCHAR *Descr, TCHAR Sep, 
                                      const TCHAR *Sez, bool fromDB, bool Unicode,
                                      C_RB_LIST *pParamValues)
{
   C_2STR      *p;
   C_2STR_LIST ValueList;
   C_STRING    Buffer;

   // Leggo le righe della sezione (senza leggere da DB)
   if (ValueList.load(f, Sep, Sez, false, Unicode) == GS_BAD) return NULL;

   // Se si vuole leggere da DB e
   // Se le righe lette contengono il collegamento ad un DB
   // allora la lista dei valori va letta dal DB
   if (fromDB)
   {
      C_STRING ConnStrUDLFile, UdlProperties, OldSelectStm;

      if (gsc_is_C_2STR_LIST_referred_to_DB(ValueList, ConnStrUDLFile, 
                                          UdlProperties, OldSelectStm))
      {
         C_STRING NewSelectStm, NewSelectStm2;
         TCHAR    *pStr1, *pStr2, CodeExpr[100];

         // modifico l'istruzione SQL
         // mi aspetto "SELECT A,B FROM TAB"
         // e converto in "SELECT A,B FROM TAB WHERE B=..."
         if ((pStr1 = OldSelectStm.at(_T(","), GS_BAD)) == NULL) return NULL;
         pStr1 += gsc_strlen(_T(","));
         if ((pStr2 = OldSelectStm.at(_T("FROM "), GS_BAD)) == NULL) return NULL;
         if ((pStr2 - pStr1) > 99) return NULL; // 100 -1
         wcsncpy(CodeExpr, pStr1, pStr2 - pStr1);
         CodeExpr[pStr2 - pStr1] = _T('\0'); // aggiungo un fine stringa

         NewSelectStm = OldSelectStm;
         if (NewSelectStm.rat(_T(" WHERE "), GS_BAD) == NULL)
            NewSelectStm += _T(" WHERE (");
         else
         {
            NewSelectStm.strtran(_T(" WHERE "), _T(" WHERE ("), GS_BAD);
            NewSelectStm += _T(") AND (");
         }

         NewSelectStm += CodeExpr;
         NewSelectStm += _T("=");
         NewSelectStm2 = NewSelectStm;
         NewSelectStm += _T("'"); // attenzione: non è detto che sia giusto
         NewSelectStm += Descr;   // dipende dal tipo di DB
         NewSelectStm += _T("')");
         
         // suppongo che CodeExpr abbia valore carattere
         if (gsc_C_2STR_LIST_load(ValueList, ConnStrUDLFile, UdlProperties, 
                                  NewSelectStm, pParamValues) == GS_BAD)
         {
            NewSelectStm2 += Descr;
            NewSelectStm2 += _T(")");
            // suppongo che CodeExpr abbia valore numerico
            if (gsc_C_2STR_LIST_load(ValueList, ConnStrUDLFile, UdlProperties, 
                                     NewSelectStm2, pParamValues) == GS_BAD)
               return NULL;
         }
      }
   }

   if ((p = (C_2STR *) ValueList.search_name2(Descr)))
      return new C_2STR(p->get_name(), p->get_name2());
   else 
      return NULL;
}


/*********************************************************/
/*.doc gsc_C_2STR_LIST_load                   <internal> */
/*+
  Questa funzione carica la lista da un DB specificato nel file indicato.
  Usata in ausilio alla C_2STR_LIST::load(FILE *f, char Sep)
  Parametri:
  C_2STR_LIST &List;
  C_STRING &ConnStrUDLFile;
  C_STRING &UdlProperties;
  C_STRING &SelectStm;
  C_RB_LIST *pParamValues;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_C_2STR_LIST_load(C_2STR_LIST &List, C_STRING &ConnStrUDLFile, 
                         C_STRING &UdlPropertiesStr, C_STRING &SelectStm,
                         C_RB_LIST *pParamValues)
{
   return gsc_C_2STR_LIST_load(List, ConnStrUDLFile, UdlPropertiesStr, SelectStm,
                               (pParamValues) ? pParamValues->get_head() : NULL);

}
int gsc_C_2STR_LIST_load(C_2STR_LIST &List, C_STRING &ConnStrUDLFile, 
                         C_STRING &UdlPropertiesStr, C_STRING &SelectStm,
                         presbuf pParamValues)
{
   C_2STR_LIST    UDLProperties;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        p1, p2;
   C_2STR         *pItem;
   int            res = GS_GOOD;
   C_STRING       msg, MySelectStm(SelectStm);

   List.remove_all();

   ConnStrUDLFile.alltrim();
   if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
      // se si tratta di un file e NON di una stringa di connessione
      if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) // lo converto
         return GS_BAD;

   UdlPropertiesStr.alltrim();
   if (UdlPropertiesStr.len() > 0)
   {
      // Conversione path UDLProperties da assoluto in dir relativo
      if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(),
                                          UdlPropertiesStr) == GS_BAD)
         return GS_BAD;
      if (gsc_PropListFromConnStr(UdlPropertiesStr.get_name(), UDLProperties) == GS_BAD)
         return GS_BAD;
   }

   if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), 
                                                             &UDLProperties)) == NULL)
      return GS_BAD;

   // Verifico presenza di una query parametrica e risolvo i valori dei 
   // parametri leggendoli da ColValues
   if (pConn->SetParamValues(MySelectStm, pParamValues) == GS_BAD)
      return GS_BAD;

   // Un tentativo senza visualizzare eventuali messaggi di errore
   if (pConn->OpenRecSet(MySelectStm, pRs, adOpenForwardOnly, adLockReadOnly, ONETEST, GS_BAD) == GS_BAD) 
      return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (!(p1 = ColValues.nth(0))) return GS_BAD;
   if (!(p1 = gsc_nth(1, p1))) return GS_BAD;
   if ((p2 = ColValues.nth(1))) p2 = gsc_nth(1, p2);

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // leggo riga
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }

      if ((pItem = new C_2STR()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; res = GS_BAD; break; }

      msg = p1;
      pItem->set_name(msg.get_name());
      if (p2)
      {
         msg = p2;
         pItem->set_name2(msg.get_name());
      }

      List.add_tail(pItem);

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   return res;
}


/*********************************************************/
/*.doc C_2STR_LIST::save                       <external> */
/*+
  Questa funzione salva la lista nel file indicato.
  Usata per la gestione TAB, REF, FDF, DEF.
  N.B. Se il file è vuoto viene cancellato.
  Parametri:
  const char *Path;           Path di un file
  char Sep;                   Carattere separatore (Default = ',')
  const char *Sez;            Sezione nel file (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_2STR_LIST::save(const char *Path, char Sep, const char *Sez)
{
   TCHAR *UnicodeStringPath = gsc_CharToUnicode(Path); 
   TCHAR UnicodeCharSep = gsc_CharToUnicode(Sep);
   TCHAR *UnicodeStringSez = gsc_CharToUnicode(Sez); 
   int   res = save(UnicodeStringPath, UnicodeCharSep, UnicodeStringSez);
   if (UnicodeStringPath) free(UnicodeStringPath);
   if (UnicodeStringSez) free(UnicodeStringSez);

   return res;
}
int C_2STR_LIST::save(const TCHAR *Path, TCHAR Sep, const TCHAR *Sez)
{
   FILE        *f = NULL, *fTemp;
   C_STR_LIST  SezList;
   C_STR       *pSez;
   C_STRING    Buffer, Dir, TempFile;
   int         res = GS_GOOD;
   C_2STR_LIST dummy;

   if (gsc_path_exist(Path) == GS_GOOD)
   {
      bool ToErase = true, Unicode;

      if ((f = gsc_open_profile(Path, READONLY, MORETESTS, &Unicode)) == NULL)
         return GS_BAD;

      // Copio tutte le sezioni in un file temporaneo ad eccezione di quella interessata
      if (gsc_get_profile(f, SezList, Unicode) == GS_BAD)
         { gsc_close_profile(f); return GS_BAD; }

      Dir = GEOsimAppl::WORKDIR;
      Dir += _T("\\TEMP");
      if (gsc_get_tmp_filename(Dir.get_name(), _T("temp"), _T(".ini"), TempFile) == GS_BAD)
         { gsc_fclose(f); return GS_BAD; }
      if ((fTemp = gsc_fopen(TempFile, _T("w"))) == NULL) { gsc_close_profile(f); return GS_BAD; }

      pSez = (C_STR *) SezList.get_head();
      while (pSez)
      {
         if (gsc_strcmp(pSez->get_name(), Sez) != 0) 
         {
            if (dummy.load(f, Sep, pSez->get_name()) == GS_BAD) 
               { res = GS_BAD; break; }
            if (dummy.get_count() > 0)
            {
               if (fwprintf(fTemp, _T("\n[%s]\n"), pSez->get_name()) < 0) { res = GS_BAD; break; }
               if (dummy.save(fTemp, Sep) == GS_BAD) { res = GS_BAD; break; }
               ToErase = false;
            }
         }

         pSez = (C_STR *) SezList.get_next();
      }

      if (res == GS_BAD)
         { gsc_fclose(fTemp); gsc_close_profile(f); gsc_delfile(TempFile); return GS_BAD; }

      // Aggiungo la sezione interessata
      if (gsc_strlen(Sez) > 0)
      {
         // Aggiungo i valori di default (senza sezione)
         if (dummy.load(f, Sep, (TCHAR *)NULL) == GS_BAD)
            { gsc_fclose(fTemp); gsc_fclose(f); gsc_delfile(TempFile); return GS_BAD; }
         if (dummy.get_count() > 0)
         {
            if (fwprintf(fTemp, GS_LFSTR) < 0)
               { gsc_fclose(fTemp); gsc_fclose(f); gsc_delfile(TempFile); return GS_BAD; }
            if (dummy.save(fTemp, Sep) == GS_BAD)
               { gsc_fclose(fTemp); gsc_fclose(f); gsc_delfile(TempFile); return GS_BAD; }
            ToErase = false;
         }
         if (get_count() > 0)
         {
            if (fwprintf(fTemp, _T("\n[%s]\n"), Sez) < 0)
               { gsc_fclose(fTemp); gsc_fclose(f); gsc_delfile(TempFile); return GS_BAD; }
            ToErase = false;
         }
      }
      else
         if (get_count() > 0)
         {
            if (fwprintf(fTemp, GS_LFSTR) < 0)
               { gsc_fclose(fTemp); gsc_close_profile(f); gsc_delfile(TempFile); return GS_BAD; }
            ToErase = false;
         }

      if (save(fTemp, Sep) == GS_BAD) 
         { gsc_fclose(fTemp); gsc_close_profile(f); gsc_delfile(TempFile); return GS_BAD; }

      gsc_fclose(fTemp);
      gsc_close_profile(f);
      gsc_delfile(Path);
      gsc_copyfile(TempFile.get_name(), Path);
      gsc_delfile(TempFile);

      if (ToErase) gsc_delfile(Path);
   }
   else if (get_count() > 0) // Se non esiste il file e c'è qualcosa da scrivere
   {
      if ((f = gsc_fopen(Path, _T("w"))) == NULL) return GS_BAD;

      // Aggiungo la sezione interessata
      if (gsc_strlen(Sez) > 0)
      {
         if (fwprintf(f, _T("\n[%s]\n"), Sez) < 0) { gsc_fclose(f); return GS_BAD; }
      }
      else
         if (fwprintf(f, GS_LFSTR) < 0) { gsc_fclose(f); return GS_BAD; }

      if (save(f, Sep) == GS_BAD) { gsc_fclose(f); return GS_BAD; }

      gsc_fclose(f);
   }

   return GS_GOOD;
}
int C_2STR_LIST::save(FILE *f, char Sep, bool Unicode)
{
   TCHAR UnicodeChar = gsc_CharToUnicode(Sep);
   return save(f, UnicodeChar, Unicode);
}
int C_2STR_LIST::save(FILE *f, TCHAR Sep, bool Unicode)
{
   C_2STR   *p;
   C_STRING Buffer;
 
   p = (C_2STR *) get_head();
   while (p)
   {
      Buffer = p->get_name();
      if (gsc_strlen(p->get_name2()) > 0)
      {
         Buffer += Sep;
         Buffer += p->get_name2();
      }

      if (Unicode)
      {
         if (fwprintf(f, _T("%s\n"), Buffer.get_name()) < 0) return GS_BAD;
      }
      else
      {
         char *strBuff = gsc_UnicodeToChar(Buffer.get_name());

         if (fprintf(f, "%s\n", strBuff) < 0)
            { if (strBuff) free(strBuff); return GS_BAD; }
         if (strBuff) free(strBuff);
      }

      p = (C_2STR *) get_next();
   }

   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_4INT_STR                                                          //
//---------------------------------------------------------------------------//

C_4INT_STR::C_4INT_STR() : C_INT_INT_STR() 
{ 
   category = 0;
   level    = GSInvisibleData;
}

C_4INT_STR::C_4INT_STR(int in) : C_INT_INT_STR(in) 
{ 
   category = 0;
   level    = GSInvisibleData;
}

C_4INT_STR::~C_4INT_STR() {} 

int C_4INT_STR::get_category()  
{ 
   return category; 
}

int C_4INT_STR::set_category(int in)  
{ 
   category = in; 
   return GS_GOOD; 
}

GSDataPermissionTypeEnum C_4INT_STR::get_level() 
{ 
   return level; 
}

int C_4INT_STR::set_level(GSDataPermissionTypeEnum in) 
{ 
   level = in; 
   return GS_GOOD; 
}

C_LIST *C_4INT_STR::ptr_sub_list() 
{  
   return &sub_list; 
}



//---------------------------------------------------------------------------//
// class C_4INT_STR_LIST                                                     //
//---------------------------------------------------------------------------//

C_4INT_STR_LIST::C_4INT_STR_LIST() : C_LIST() {}
      
C_4INT_STR_LIST::~C_4INT_STR_LIST() {}

int C_4INT_STR_LIST::count_upd()
{ 
   int n_mod_cls = 0;
   C_4INT_STR *p_class;
           
   p_class = (C_4INT_STR *) get_head();
   while (p_class != NULL)
   {
      if (p_class->get_level() == GSUpdateableData) n_mod_cls++;
         p_class = (C_4INT_STR *) get_next();
   }
   return n_mod_cls;
};

resbuf *C_4INT_STR_LIST::to_rb(void)
{
   resbuf          *list=NULL, *rb, *p;
   C_4INT_STR      *punt;
   C_4INT_STR_LIST *sub_list;
   TCHAR            str[1] = GS_EMPTYSTR, *string;
            
   punt = (C_4INT_STR*) get_tail();
   if (punt == NULL)
      if ((list = acutBuildList(RTNIL,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
         
   while (punt != NULL)
   {
      if ((string = punt->get_name())==NULL) string = str;
      if ((rb = acutBuildList(RTLB,
                                  RTSHORT, punt->get_key(),
                                  RTSTR, string,
                                  RTSHORT, (int) punt->get_level(),
                                  RTSHORT, punt->get_category(),
                                  RTSHORT, punt->get_type(), 0)) == NULL)
         { acutRelRb(list); GS_ERR_COD=eGSOutOfMem; return NULL; }
      p=rb;
      while (p->rbnext != NULL) p = p->rbnext;
      // sub_list
      sub_list = (C_4INT_STR_LIST *) punt->ptr_sub_list();
      punt     = (C_4INT_STR *) sub_list->get_head();
      if (punt == NULL)
      {
         if ((p->rbnext = acutBuildList(RTNIL,0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
         p = p->rbnext;   
      }
      else
      {
         if ((p->rbnext = acutBuildList(RTLB, 0)) == NULL)
            { acutRelRb(list); GS_ERR_COD=eGSOutOfMem; return NULL; }
         p = p->rbnext;   
         while (punt != NULL)
         {
            if ((string = punt->get_name())==NULL) string = str;
            if ((p->rbnext = acutBuildList(RTLB,
                                           RTSHORT, punt->get_key(),
                                           RTSTR, string,
                                           RTSHORT, (int) punt->get_level(),
                                           RTSHORT, punt->get_category(),
                                           RTSHORT, punt->get_type(),
                                           RTLE, 0)) == NULL)
               { acutRelRb(list); GS_ERR_COD=eGSOutOfMem; return NULL; }
            while (p->rbnext != NULL) p = p->rbnext;
            punt = (C_4INT_STR *) sub_list->get_next();
         }
         if ((p->rbnext = acutBuildList(RTLE, 0)) == NULL)
            { acutRelRb(list); GS_ERR_COD=eGSOutOfMem; return NULL; }
         p = p->rbnext;   
      }
      if ((p->rbnext = acutBuildList(RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      p = p->rbnext;   
      p->rbnext = list;
      list = rb;
      punt = (C_4INT_STR*) get_prev();
   }
                                
   return list;      
}

int C_4INT_STR_LIST::from_rb(resbuf *rb)   // da aggiornare (se serve)
{
   C_4INT_STR *nod;
   int   val_1, val_3, val_4;
   GSDataPermissionTypeEnum Permission;
   TCHAR *str;

   if(rb == NULL)           { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if(rb->restype == RTNIL) { remove_all(); return GS_GOOD; }
   if(rb->restype != RTLB)  { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   remove_all();
   while((rb = rb->rbnext) != NULL && rb->restype != RTLE)
   {      
      if (rb->restype != RTLB)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      if ((rb = rb->rbnext) == NULL || rb->restype != RTSHORT)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      val_1 = rb->resval.rint;

      if ((rb = rb->rbnext) == NULL || rb->restype != RTSTR)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      str = rb->resval.rstring;

      if ((rb = rb->rbnext) == NULL || rb->restype != RTSHORT)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      Permission = (GSDataPermissionTypeEnum) rb->resval.rint;

      if ((rb = rb->rbnext) == NULL || rb->restype != RTSHORT)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      val_3 = rb->resval.rint;

      if ((rb = rb->rbnext) == NULL || rb->restype != RTSHORT)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      val_4 = rb->resval.rint;
      
      if ((rb = rb->rbnext) == NULL || rb->restype != RTLE)
         { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }        

      if ((nod = new C_4INT_STR((int) wcslen(str) + 1))==NULL)
         { remove_all(); GS_ERR_COD=eGSOutOfMem; return GS_BAD; }

      add_tail(nod);
      nod->set_key(val_1);
      nod->set_level(Permission);
      nod->set_category(val_3);
      nod->set_type(val_4);
      if (nod->set_name(str) == GS_BAD) { remove_all(); return GS_BAD; }
   }
   
   return GS_GOOD;
}

C_4INT_STR* C_4INT_STR_LIST::search(int aaa, int bbb, int ccc)
{
   C_4INT_STR *punt;

   punt = (C_4INT_STR *) get_head();
   while (punt != NULL)
   {
      if (aaa == punt->get_key())
         if (bbb == punt->get_type())
            if (ccc == punt->get_category()) 
               break;
      punt = (C_4INT_STR *) get_next();
   }
   return punt;
}

int C_4INT_STR_LIST::copy(C_4INT_STR_LIST *out)
{
   C_4INT_STR *new_node, *punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }
   out->remove_all();

   punt=(C_4INT_STR*)get_head();
   
   while (punt)
   {
      if ((new_node= new C_4INT_STR)==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }      
      new_node->key  = punt->key;
      new_node->type = punt->type;
      new_node->category = punt->category;
      new_node->level = punt->level;
      if (new_node->set_name(punt->get_name()) == GS_BAD) return GS_BAD;

      out->add_tail(new_node);
      punt=(C_4INT_STR*)get_next();
   }
   
   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_FAMILY                                                            //
//---------------------------------------------------------------------------//

C_FAMILY::C_FAMILY() : C_INT() {}

C_FAMILY::C_FAMILY(int in) : C_INT() { key = in; }

C_FAMILY::~C_FAMILY()
{ 
   relation.remove_all(); 
}



//---------------------------------------------------------------------------//
// class C_FAMILY_LIST                                                       //
//---------------------------------------------------------------------------//

C_FAMILY_LIST::C_FAMILY_LIST() : C_INT_LIST() {}

C_FAMILY_LIST::~C_FAMILY_LIST() {} 

C_FAMILY *C_FAMILY_LIST::search_list(int in)
{
   C_FAMILY *punt;
   
   punt = (C_FAMILY *) get_head();
   while (punt)
   {
      if (punt->relation.search_key(in) != NULL) break;
      punt = (C_FAMILY *) get_next();
   }
 
   return punt;      
}

resbuf *C_FAMILY_LIST::to_rb(void)
{
   resbuf *list=NULL,*rb,*buf;
   C_FAMILY *punt;
            
   punt=(C_FAMILY*)get_tail();
   if (punt==NULL) 
      if ((list=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }
   while(punt!=NULL)
      {                                
      if ((rb = buf = acutBuildList(RTLB,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext = punt->relation.to_rb();
      while(rb->rbnext != NULL) { rb = rb->rbnext; }
      if ((rb->rbnext = acutBuildList(RTLE,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext = list;
      list = buf; punt = (C_FAMILY *) get_prev();
      }                             
   return list;      
}

int C_FAMILY_LIST::from_rb(resbuf *rb)
{
   C_FAMILY *nod;

   if(rb==NULL)           { GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   if(rb->restype==RTNIL) { remove_all(); return GS_GOOD; }
   if(rb->restype!=RTLB)  { GS_ERR_COD=eGSInvRBType; return GS_BAD; }

   remove_all();
   while((rb=rb->rbnext)!=NULL && rb->restype!=RTLE)
   {      
      if ((nod=new C_FAMILY)==NULL)
         { remove_all(); GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      
      add_tail(nod);
      
      if (rb->restype!=RTLB)
         { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }

      if (nod->relation.from_rb(rb->rbnext)==GS_BAD)
                  { remove_all(); return GS_BAD; }
      rb=gsc_scorri(rb->rbnext);              
      if ((rb=rb->rbnext)==NULL || rb->restype!=RTLE)
         { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   }
   if ((rb=rb->rbnext)==NULL)
              { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   return GS_GOOD;
}

int C_FAMILY_LIST::add_tail_int(int Val)
{
   C_FAMILY *p;

   if ((p = new C_FAMILY(Val)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

int C_FAMILY_LIST::copy(C_FAMILY_LIST &out)
{
   C_FAMILY *new_node, *punt;

   punt = (C_FAMILY * )get_head();
   out.remove_all();
   
   while (punt)
   {
      if ((new_node = new C_FAMILY(punt->key)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      out.add_tail(new_node);
      punt->relation.copy(new_node->relation);
      
      punt = (C_FAMILY *) get_next();
   }
   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_BIRELATION                                                        //
//---------------------------------------------------------------------------//
      
C_BIRELATION::C_BIRELATION() : C_INT() {}

C_BIRELATION::~C_BIRELATION()
{ 
   relation.remove_all(); 
} 



//---------------------------------------------------------------------------//
// class C_BIRELATION_LIST                                                   //
//---------------------------------------------------------------------------//
      
C_BIRELATION_LIST::C_BIRELATION_LIST() : C_LIST() {}
      
C_BIRELATION_LIST::~C_BIRELATION_LIST() {}

resbuf *C_BIRELATION_LIST::to_rb(void)
{
   resbuf *list=NULL,*rb,*buf;
   C_BIRELATION *punt;
            
   punt=(C_BIRELATION*)get_tail();
   if (punt==NULL) 
      if ((list=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return NULL; }

   while(punt!=NULL)
   {                                
      if ((buf=acutBuildList(RTLB,RTSHORT,punt->get_key(),
                       RTLB,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb = buf->rbnext->rbnext;
      rb->rbnext = punt->relation.to_rb();
      while(rb->rbnext != NULL) {rb = rb->rbnext;}
      if ((rb->rbnext = acutBuildList(RTLE,RTLE,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext->rbnext = list;
      list = buf; punt = (C_BIRELATION *) get_prev();
   }

   return list;      
}

int C_BIRELATION_LIST::from_rb(resbuf *rb)
{
   C_BIRELATION *nod;

   if(rb==NULL)           { GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   if(rb->restype==RTNIL) { remove_all(); return GS_GOOD; }
   if(rb->restype!=RTLB)  { GS_ERR_COD=eGSInvRBType; return GS_BAD; }

   remove_all();
   while((rb=rb->rbnext)!=NULL && rb->restype!=RTLE)
   {      
      if ((nod=new C_BIRELATION)==NULL)
         { remove_all(); GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      
      add_tail(nod);
                                                  
      if (rb->restype!=RTLB)
                  { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      if ((rb=rb->rbnext)==NULL || rb->restype!=RTSHORT)
                  { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
      nod->set_key(rb->resval.rint);

      if (nod->relation.from_rb(rb->rbnext)==GS_BAD)
                  { remove_all(); return GS_BAD; }
      rb=gsc_scorri(rb->rbnext);              
      if ((rb=rb->rbnext)==NULL || rb->restype!=RTLE)
              { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   }
   if ((rb=rb->rbnext)==NULL)
              { remove_all(); GS_ERR_COD=eGSInvRBType; return GS_BAD; }
   return GS_GOOD;
}



//---------------------------------------------------------------------------//
// class C_CLS_PUNT                                                          //
//---------------------------------------------------------------------------//

// Costruttori
C_CLS_PUNT::C_CLS_PUNT(C_CLS_PUNT *p) : C_NODE()
{
   p->copy(this);
}
C_CLS_PUNT::C_CLS_PUNT(C_NODE *_cls, ads_name _ent, long _gs_id) : C_NODE() 
{ 
   cls = _cls; 
   if (!_ent) ads_name_clear(ent);
   else ads_name_set(_ent, ent);
   gs_id = _gs_id;
}
C_CLS_PUNT::C_CLS_PUNT(ads_name _ent) : C_NODE() 
{ 
   cls = NULL; 
   ads_name_set(_ent, ent);
   gs_id = 0;
}

C_CLS_PUNT::~C_CLS_PUNT() {} 

int C_CLS_PUNT::get_key()  
{ 
   if (cls==NULL) 
      return 0;
   else 
      return cls->get_key(); 
}

TCHAR *C_CLS_PUNT::get_name() 
{ 
   if (cls==NULL) 
      return NULL;
   else 
      return cls->get_name(); 
}

int C_CLS_PUNT::get_ent(ads_name out)
{
   if (out==NULL) 
      { GS_ERR_COD=eGSInvalidArg; return GS_BAD; }
   ads_name_set(ent,out); 
   return GS_GOOD;
}        

long C_CLS_PUNT::get_gs_id()
{
   if (gs_id) return gs_id; // se è già stato inizializzato
   // altrimenti lo ricavo
   if (((C_CLASS *) cls)->getKeyValue(ent, &gs_id) == GS_BAD) return 0;
   return gs_id;
}


/*********************************************************/
/*.doc C_CLS_PUNT::gs_id_initEnt
/*+                                                                       
  Dal membro <ent> già inizializzato viene calcolato il valore chiave
  ad esso associato (se non ancora ricavato).
-*/  
/*********************************************************/
int C_CLS_PUNT::gs_id_initEnt(void)
{
   if (gs_id) return GS_GOOD; // se è già stato ricavato
   return ((C_CLASS *) cls)->getKeyValue(ent, &gs_id);
}

int C_CLS_PUNT::copy(C_CLS_PUNT *out)
{
   if (!out) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   out->cls = cls;
   ads_name_set(ent, out->ent);
   out->gs_id = gs_id;

   return GS_GOOD;
}

int C_CLS_PUNT::from_ent(ads_name _ent)
{
   C_EED eed;

   if (ads_name_nil(_ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (eed.load(_ent) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if ((cls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
      return GS_BAD;
   ads_name_set(_ent, ent);
   gs_id = eed.gs_id;

   return GS_GOOD;
}

C_NODE *C_CLS_PUNT::get_class(void)
{
   if (!cls)
   {
      C_EED eed;

      if (ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
      if (eed.load(ent) == GS_BAD) return NULL;
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return NULL; }
      cls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub);
   }

   return cls;
}


//---------------------------------------------------------------------------//
// class C_CLS_PUNT_LIST                                                     //
//---------------------------------------------------------------------------//


C_CLS_PUNT_LIST::C_CLS_PUNT_LIST() : C_LIST() {}
      
C_CLS_PUNT_LIST::~C_CLS_PUNT_LIST() {}  

int C_CLS_PUNT_LIST::to_ssgroup(ads_name sss)
{
   ads_name   tmp;
   C_CLS_PUNT *punt;

   if (!sss) { eGSInvalidArg; return GS_BAD; }
   
   if (acedSSAdd(NULL, NULL, sss) != RTNORM)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   punt = (C_CLS_PUNT *) get_head();
   while(punt)
   { 
      if (punt->get_ent(tmp) == GS_BAD) return GS_BAD;
      if (acedSSAdd(tmp, sss, sss) != RTNORM)
         { ads_ssfree(sss); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      punt = (C_CLS_PUNT *) get_next();
   }

   return GS_GOOD;
}

// Converto una lista di ename in una lista C_CLS_PUNT_LIST
int C_CLS_PUNT_LIST::from_ss(ads_name ss)
{
   long       len;
   C_CLS_PUNT *p;

   remove_all();

   if (ads_sslength(ss, &len) != RTNORM)
   {  // è una sola entità
      if ((p = new C_CLS_PUNT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (p->from_ent(ss) == GS_BAD) { delete p; return GS_BAD; }
      add_tail(p);
   }
   else
   {  // è un gruppo di selezione
      ads_name entity;

      for (long i = 0; i < len; i++)
      {
         if (acedSSName(ss, i, entity) != RTNORM)
   	      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         if ((p = new C_CLS_PUNT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         if (p->from_ent(entity) == GS_BAD) { delete p; return GS_BAD; }
         add_tail(p);
      }
   }

   return GS_GOOD;
}

C_CLS_PUNT *C_CLS_PUNT_LIST::search_ent(ads_name in)
{
   ads_name tmp;
   C_CLS_PUNT *punt;
   
   if (!in) { GS_ERR_COD =  eGSInvalidArg; return NULL; }
      
   punt = (C_CLS_PUNT *) get_head();
   while (punt)
   { 
      if (punt->get_ent(tmp) == GS_BAD) return NULL;
      if (ads_name_equal(tmp, in)) break;
      punt = (C_CLS_PUNT *) get_next();
   }

   return punt;
}   

C_CLS_PUNT *C_CLS_PUNT_LIST::search_next_ent(ads_name in)
{
   ads_name tmp;
   C_CLS_PUNT *punt;
   
   if (in == NULL) { eGSInvalidArg; return NULL; }
      
   punt=(C_CLS_PUNT*)get_next();
   while (punt)
   { 
      if (punt->get_ent(tmp)==GS_BAD) return NULL;
      if (ads_name_equal(tmp,in)) break;
      punt=(C_CLS_PUNT*)get_next();
   }

   return punt;
}   

int C_CLS_PUNT_LIST::copy(C_CLS_PUNT_LIST &out)
{
   C_CLS_PUNT *pNew, *p = (C_CLS_PUNT *) get_head();

   while (p)
   {
      if (!(pNew = new C_CLS_PUNT)) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (p->copy(pNew) == GS_BAD) { out.remove_all(); return GS_BAD; }      
      out.add_tail(pNew);
      p = (C_CLS_PUNT *) get_next();
   }
   
   return GS_GOOD;
}

int C_CLS_PUNT_LIST::gs_id_initEnt()
{
   C_CLS_PUNT *punt = (C_CLS_PUNT *) get_head();
   
   while (punt)
   { 
      if (punt->gs_id_initEnt()==GS_BAD) return GS_BAD;
      punt = (C_CLS_PUNT *) get_next();
   }
   
   return GS_GOOD;
}   


/*********************************************************/
/*.doc C_CLS_PUNT_LIST::search_ClsKey
/*+                                                                       
  Dato un puntatore ad una classe GEOsim e un codice 
  chiave, cerca l'elemento nella lista.
  Parametri:
  C_NODE *pCls;   Puntatore a classe di GEOsim
  long gs_id;     Codice entità
-*/  
/*********************************************************/
C_CLS_PUNT *C_CLS_PUNT_LIST::search_ClsKey(C_NODE *pCls, long key)
{
   C_CLS_PUNT *punt;
      
   punt = (C_CLS_PUNT *) get_head();
   while (punt)
   { 
      if (pCls == punt->get_class() && key == punt->gs_id) break;
      punt = (C_CLS_PUNT *) get_next();
   }
   
   return punt;
}   


/*********************************************************/
/*.doc C_CLS_PUNT_LIST::search_next_ClsKey
/*+                                                                       
  Dato un puntatore ad una classe GEOsim e un codice chiave,
  cerca l'elemento nella lista iniziando la ricerca da punto successivo 
  a quello attuale.
  Parametri:
  C_NODE *pCls;   Puntatore a classe di GEOsim
  long gs_id;     Codice entità
-*/  
/*********************************************************/
C_CLS_PUNT *C_CLS_PUNT_LIST::search_next_ClsKey(C_NODE *pCls, long key)
{
   C_CLS_PUNT *punt;
      
   punt = (C_CLS_PUNT *) get_next();
   while (punt)
   { 
      if (pCls == punt->get_class() && key == punt->gs_id) break;
      punt = (C_CLS_PUNT *) get_next();
   }
   
   return punt;
}   


/*********************************************************/
/*.doc C_CLS_PUNT_LIST::search_Cls
/*+                                                                       
  Dato un puntatore ad una classe GEOsim, cerca l'elemento nella lista.
  Parametri:
  C_NODE *pCls;   Puntatore a classe di GEOsim
  oppure
  int cls;        Codice classe di GEOsim
-*/  
/*********************************************************/
C_CLS_PUNT *C_CLS_PUNT_LIST::search_Cls(C_NODE *pCls)
{   
   return search_Cls(((C_CLASS *) pCls)->ptr_id()->code, 
                     ((C_CLASS *) pCls)->ptr_id()->sub_code);
}   
C_CLS_PUNT *C_CLS_PUNT_LIST::search_Cls(int cls, int sub)
{
   C_CLS_PUNT *punt;
   C_CLASS    *pCls;
      
   punt = (C_CLS_PUNT *) get_head();
   while (punt)
   { 
      pCls = (C_CLASS *) punt->get_class();
      if (pCls && 
          pCls->ptr_id()->code == cls && pCls->ptr_id()->sub_code == sub) break;
      punt = (C_CLS_PUNT *) get_next();
   }
   
   return punt;
}   


/*********************************************************/
/*.doc C_CLS_PUNT_LIST::get_TopoLinkInfo
/*+                                                                       
  La funzione richiede che la lista sia composta da 3 elementi:
  link (primo elemento della lista)
  nodo iniziale (secondo elemento della lista)
  nodo finale (terzo elemento della lista)
  Parametri:
  int *LinkSub;      Codice sottoclasse Link
  long *LinkID;      Codice entità Link
  int *InitNodeSub;  Codice sottoclasse Nodo iniziale
  long *InitNodeID;  Codice entità Nodo iniziale
  int *FinalNodeSub; Codice sottoclasse Nodo finale
  long *FinalNodeID; Codice entità Nodo finale

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLS_PUNT_LIST::get_TopoLinkInfo(int *LinkSub, long *LinkID,
                                      int *InitNodeSub, long *InitNodeID,
                                      int *FinalNodeSub, long *FinalNodeID)
{
   C_CLS_PUNT *pClsParam;

   // link (primo elemento di <lista_cls>)
   if ((pClsParam = (C_CLS_PUNT *) goto_pos(1)) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (LinkSub) *LinkSub = ((C_CLASS *) pClsParam->get_class())->ptr_id()->sub_code;
   if (LinkID) *LinkID  = pClsParam->get_gs_id();
   // nodo iniziale (secondo elemento di <lista_cls>)
   if ((pClsParam = (C_CLS_PUNT *) goto_pos(2)) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (InitNodeSub) *InitNodeSub = ((C_CLASS *) pClsParam->get_class())->ptr_id()->sub_code;
   if (InitNodeID) *InitNodeID  = pClsParam->get_gs_id();
   // nodo finale (terzo elemento di <lista_cls>)
   if ((pClsParam = (C_CLS_PUNT *) goto_pos(3)) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (FinalNodeSub) *FinalNodeSub = ((C_CLASS *) pClsParam->get_class())->ptr_id()->sub_code;
   if (FinalNodeID) *FinalNodeID  = pClsParam->get_gs_id();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLS_PUNT_LIST::to_C_2INT_LONG_LIST
/*+                                                                       
  Conversione della lista in una C_2INT_LONG_LIST.
  Parametri:
  C_2INT_LONG_LIST &out;        Lista in uscita
-*/  
/*********************************************************/
int C_CLS_PUNT_LIST::to_C_2INT_LONG_LIST(C_2INT_LONG_LIST &out)
{
   C_CLS_PUNT  *punt = (C_CLS_PUNT *) get_head();
   C_CLASS     *pCls;
   C_2INT_LONG *p;
   
   out.remove_all();
   while (punt)
   { 
      pCls = (C_CLASS *) punt->get_class();
      if (pCls)
         // evito i duplicati
         if (out.search(pCls->ptr_id()->code, pCls->ptr_id()->sub_code, punt->get_gs_id()) == NULL)
         {
            if ((p = new C_2INT_LONG(pCls->ptr_id()->code, pCls->ptr_id()->sub_code, punt->get_gs_id())) == NULL)
               { out.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
            out.add_tail(p);
         }
      punt = (C_CLS_PUNT *) get_next();
   }
   
   return GS_GOOD;
}   


//---------------------------------------------------------------------------//
// class C_CLS_PUNT_LIST - fine                                              //
// class C_RB_LIST - inizio                                                  //
//---------------------------------------------------------------------------//


C_RB_LIST::C_RB_LIST()
{
   head = tail = cursor = NULL; 
   count = 0; 
   mReleaseAllAtDistruction = GS_GOOD;
}
C_RB_LIST::C_RB_LIST(resbuf *prb)
{
   head = tail = cursor = NULL; 
   count = 0; 
   mReleaseAllAtDistruction = GS_GOOD;
   (*this) << prb;
}

C_RB_LIST::~C_RB_LIST()
{ 
   if (mReleaseAllAtDistruction == GS_GOOD) remove_all();
}

int C_RB_LIST::IsEmpty(void) 
{ 
   return (head) ? FALSE : TRUE; 
}
      
int C_RB_LIST::remove_all() 
{ 
   if (head) 
   {
      cursor = head;

      while (cursor) // acutRelRb non rilascia i gruppi di selezione (noi si)
      {
         switch (cursor->restype)
         {
            case RTPICKS: // gruppo di selezione
               ads_ssfree(cursor->resval.rlname);
               break;
            case 1004: // dato binario
               if (cursor->resval.rbinary.buf) free(cursor->resval.rbinary.buf);
               cursor->restype = RTNONE; // perchè acutRelRb altrimenti si pianterebbe
               break;
         }
         cursor = cursor->rbnext;
      }

      acutRelRb(head);
   }

   head = tail = cursor = NULL; 
   count = 0;
    
   return GS_GOOD; 
}

int C_RB_LIST::remove_all_without_dealloc() 
{ 
   head = tail = cursor = NULL; 
   count = 0; 

   return GS_GOOD; 
}

int C_RB_LIST::GetCount() { return count; }

int C_RB_LIST::GetSubListCount()
{
   int     i = 0;
   presbuf pPrevCsr = cursor;

   while (nth(i)) i++;
   cursor = pPrevCsr;

   return i;
}

void C_RB_LIST::ReleaseAllAtDistruction(int in) 
{ 
   mReleaseAllAtDistruction = in;
}

presbuf C_RB_LIST::get_head()  
{ 
   cursor=head; 
   return head; 
} 

presbuf C_RB_LIST::get_tail()  
{ 
   cursor=tail; 
   return tail; 
}

presbuf C_RB_LIST::get_cursor() 
{ 
   return cursor; 
}

// questa funzione è estremamente pericolosa perchè si fida che punt
// sia il puntatore di un elemento della C_RB_LIST corrente
void C_RB_LIST::set_cursor(presbuf punt) 
   { cursor = punt; }

presbuf C_RB_LIST::get_next() 
{ 
   if (cursor) 
      cursor = cursor->rbnext; 
   return cursor; 
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::operator << <external> */
/*+
  Data una lista resbuf associo la lista all'oggetto resbuf
  senza copiarla ma puntando alla stessa.
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore all'inizio della lista resbuf.
-*/  
/*********************************************************/
presbuf C_RB_LIST::operator << (resbuf *prb) 
{
   remove_all();
   head = tail = prb;

   if (prb) 
   {
      count++;
      while (tail->rbnext) { tail = tail->rbnext; count++; }
   }
   // else GS_ERR_COD = eGSOutOfMem; // commentato perchè nasconde i messaggi di errore veri

   return head;
}
resbuf* C_RB_LIST::operator << (resbuf mrb) 
{ 
   return (*this << &mrb); 
}
resbuf* C_RB_LIST::operator << (C_RB_LIST *mrb) 
{ 
   return (*this << mrb->get_head()); 
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::operator += <external> */
/*+
  Data una lista resbuf aggiungo la lista all'oggetto resbuf
  senza copiarla ma puntando alla stessa.
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore all'inizio della lista resbuf da aggiungere.
-*/  
/*********************************************************/
presbuf C_RB_LIST::operator += (resbuf *prb)
{
   if (!prb) GS_ERR_COD = eGSOutOfMem;
   else
   {
      if (IsEmpty()) (*this) << prb;
      else 
      {
         tail->rbnext = prb;
         while (tail->rbnext) { tail = tail->rbnext; count++; }
      }
   }

   return prb;
}
resbuf* C_RB_LIST::operator += (C_RB_LIST *mrb) 
{ 
   return (*this += mrb->get_head()); 
}
resbuf* C_RB_LIST::operator += (C_RB_LIST &mrb) 
{ 
   return (*this += mrb.get_head()); 
}

resbuf* C_RB_LIST::operator += (TCHAR *str) 
{ 
   presbuf p;

   if (str)
   {
      if ((p = acutBuildList(RTSTR, str, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   }
   else
      if ((p = acutBuildList(RTSTR, GS_EMPTYSTR, 0)) == NULL) // stringa vuota
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

   return (*this += p); 
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::operator          = <external> */
/*+
  Copia una lista resbuf nella lista dell'oggetto,
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore all'inizio della lista resbuf.
-*/  
/*********************************************************/
resbuf* C_RB_LIST::operator = (presbuf prb) 
{ 
   presbuf q;
   int     result = GS_GOOD;

   remove_all();

   while (prb)
   {
      if ((q = gsc_copybuf(prb)) == NULL) { result = GS_BAD; break; }
      if (((*this) += q) == NULL) { result = GS_BAD; break; }

      prb = prb->rbnext;
   }

   if (result == GS_BAD) remove_all();

   return get_head();
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::remove_head         <external> */
/*+
  Data una lista resbuf cancello il primo elemento

  Restituisce il puntatore all'inizio della lista resbuf.
-*/  
/*********************************************************/
presbuf C_RB_LIST::remove_head() 
{
   presbuf p;

   if ((p = head) != NULL) 
   {
      head = head->rbnext;
      if (cursor==p) cursor = head;
      p->rbnext = NULL;
      
      // acutRelRb non rilascia i gruppi di selezione !
      if (p->restype == RTPICKS) ads_ssfree(p->resval.rlname);

      acutRelRb(p);
      count--;
   }

   return head;
}
/*********************************************************/
/*.doc presbuf C_RB_LIST::remove_tail <external> */
/*+
  Data una lista resbuf cancello l'ultimo elemento

  Restituisce il puntatore alla fine della lista resbuf.
-*/  
/*********************************************************/
presbuf C_RB_LIST::remove_tail() 
{
   presbuf p, p1;

   if ((p = tail) != NULL) 
   {
      p1 = tail = getptr_at(GetCount()-1);
      if (cursor == p1) cursor = tail;
      p1->rbnext = NULL;

      // acutRelRb non rilascia i gruppi di selezione !
      if (p->restype == RTPICKS) ads_ssfree(p->resval.rlname);

      acutRelRb(p);
      count--;
   }

   return tail;
}
/*********************************************************/
/*.doc presbuf C_RB_LIST::remove              <external> */
/*+
  Data una lista resbuf cancello l'elemento puntato.
  Parametri:
  presbuf punt;

  Restituisce il puntatore all'elemento successivo della lista tranne
  cancellazione della coda in cui viene restituito il nuovo puntatore alla coda.
-*/  
/*********************************************************/
presbuf C_RB_LIST::remove(presbuf punt) 
{
   if (!punt) return NULL;
   if (punt == head) return remove_tail();
   if (punt == tail) return remove_tail();

   presbuf ptr = head->rbnext, ptrPrev = head;

   while (ptr != NULL)
   {
      if (ptr == punt)
      {
         ptrPrev->rbnext = punt->rbnext;
         punt->rbnext    = NULL;
         count--;
         // acutRelRb non rilascia i gruppi di selezione !
         if (punt->restype == RTPICKS) ads_ssfree(punt->resval.rlname);
         acutRelRb(punt);
         cursor = ptrPrev->rbnext;
         
         return cursor;
      }

      ptrPrev = ptr;
      ptr     = ptr->rbnext;
   }

   return NULL; // Il puntatore non appartiene alla lista
}
presbuf C_RB_LIST::remove_at(int pos) // 1 e' il primo
{  
   getptr_at(pos);
   return remove_at();
}
// se viene rimosso un elemento il cursore va all'elemento successivo
presbuf C_RB_LIST::remove_at()
{
   return remove(cursor); 
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::link_head           <external> */
/*+
  Data una lista resbuf aggiungo la lista all'inizio della lista 
  dell'oggetto senza copiarla ma puntando alla stessa.
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore al primo elemento.
-*/  
/*********************************************************/
presbuf C_RB_LIST::link_head(resbuf *prb) 
{
   if (IsEmpty()) (*this) << prb;
   else if (prb)
   {
      presbuf p = head, ind;

      head = ind = prb;
      count++;
      while (ind->rbnext) { ind = ind->rbnext; count++; }
      ind->rbnext = p;
   }

   return head;
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::link_tail           <external> */
/*+
  Data una lista resbuf aggiungo la lista alla fine della lista 
  dell'oggetto senza copiarla ma puntando alla stessa.
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore all'ultimo elemento.
-*/  
/*********************************************************/
presbuf C_RB_LIST::link_tail(resbuf *prb) 
{
   if (IsEmpty()) (*this) << prb;
   else if (prb)
   {
      tail->rbnext = prb;
      while (tail->rbnext) { tail = tail->rbnext; count++; }
   }

   return tail;
}


/*********************************************************/
/*.doc presbuf C_RB_LIST::link_atCursor       <external> */
/*+
  Data una lista resbuf aggiungo la lista dopo il resbuf
  del cursore dell'oggetto senza copiarla ma puntando alla stessa.
  Parametri:
  resbuf *prb;       lista resbuf

  Restituisce il puntatore del cursore.
-*/  
/*********************************************************/
presbuf C_RB_LIST::link_atCursor(resbuf *prb) 
{
   if (IsEmpty()) (*this) << prb;
   else if (prb)
   {
      presbuf p = cursor, pNext = cursor->rbnext;

      p->rbnext = prb;
      while (p->rbnext) { p = p->rbnext; count++; }
      p->rbnext = pNext;
   }

   return cursor;
}


/*************************************************************************/
/*.doc C_RB_LIST::SearchType (external)*/  
/*+
  Ricerca il primo resbuf avente restype = a quello del parametro
  Parametri:
  int restype;    restype da cercare (vedi struttura resbuf)

  La funzione ritorna il puntatore al primo elemento trovato o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::SearchType(int restype)
{
   cursor = head;

   while (cursor && cursor->restype != restype)
      cursor = cursor->rbnext;

   return cursor;

}


/*************************************************************************/
/*.doc C_RB_LIST::SearchNextType (external)*/  
/*+
  Ricerca il primo resbuf avente restype = a quello del parametro partendo dalla
  posizione successiva a quella del cursore.
  Parametri:
  int restype;    restype da cercare (vedi struttura resbuf)

  La funzione ritorna il puntatore al RSBUF o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::SearchNextType(int restype)
{
   if (!cursor) return NULL;
   cursor = cursor->rbnext;

   while (cursor && cursor->restype != restype)
      cursor = cursor->rbnext;

   return cursor;
}


/*************************************************************************/
/*.doc C_RB_LIST::getptr_at (external)*/  
/*+
  Ricerca l'n-esimo resbuf.
  Parametri:
  int n;    posizione nella lista; (1 è il primo)

  La funzione ritorna il puntatore all'n-esimo elemento altrimenti NULL.
   
/*************************************************************************/
presbuf C_RB_LIST::getptr_at(int n)
{
   int i;

   if (IsEmpty() || GetCount()<n) return NULL;

   get_head();

   for (i=0; i<n-1; i++)
      if (!get_next()) break;

   return cursor;
}


/*************************************************************************/
/*.doc C_RB_LIST::copy                                         (external)*/  
/*+
  Copia la lista in un'altra C_RB_LIST.
  Parametri:
  C_RB_LIST &out;    lista destinazione

  La funzione ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
/*************************************************************************/
int C_RB_LIST::copy(C_RB_LIST &out)
{
   presbuf p, q;
   int     result = GS_GOOD;

   out.remove_all();
   p = get_head();

   while (p)
   {
      if ((q = gsc_copybuf(p)) == NULL) { result = GS_BAD; break; }
      if ((out += q) == NULL) { result = GS_BAD; break; }

      p = get_next();
   }

   if (result == GS_BAD) out.remove_all();

   return result;
}


/*************************************************************************/
/*.doc C_RB_LIST::append                                       (external)*/  
/*+
  Copia la lista in fondo ad un'altra C_RB_LIST.
  Parametri:
  C_RB_LIST &out;    lista destinazione

  La funzione ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
/*************************************************************************/
int C_RB_LIST::append(C_RB_LIST &out)
{
   presbuf p, q;
   int     result = GS_GOOD;

   p = get_head();

   while (p)
   {
      if ((q = gsc_copybuf(p)) == NULL) { result = GS_BAD; break; }
      if ((out += q) == NULL) { result = GS_BAD; break; }

      p = get_next();
   }

   if (result == GS_BAD) out.remove_all();

   return result;
}


/*************************************************************************/
/*.doc gsc_lspforrb (external)*/  
/*+
  Riceve una C_RB_LIST la cambia aggiungendo al primo e l'ultimo elemento
  rispettivamente RTLB (aperta tonda) e RTLE (chiusa tonda).
-*/
/*************************************************************************/
int gsc_lspforrb(C_RB_LIST *rb)
{       
   if (rb == NULL) return GS_BAD;
   
   if (rb)  // aggiungo aperta tonda
   {
   
      presbuf p;

      if ((p = acutBuildList(RTLB, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      rb->link_head(p); // collego all'inizio della lista

      if ((p = acutBuildList(RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      *rb += p;   // collego alla fine della lista
   }
   
   return GS_GOOD;
}


/*************************************************************************/
/*.doc C_RB_LIST::SearchName           (external)*/  
/*+
  Ricerca il primo resbuf avente restype = RTSTR con stringa uguale
  a quella del parametro.
  Parametri:
  const char *str;    stringa da cercare
  int sensitive;      sensibile al maiuscolo (default = TRUE)

  La funzione ritorna il puntatore al primo elemento trovato o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::SearchName(const char *str, int sensitive)
{
   cursor = head;
   return SearchNextName(str, sensitive);
}
presbuf C_RB_LIST::SearchName(const TCHAR *str, int sensitive)
{
   cursor = head;
   return SearchNextName(str, sensitive);
}


/*************************************************************************/
/*.doc C_RB_LIST::SearchNextName (external)*/  
/*+
  Ricerca il primo resbuf avente restype = RTSTR con stringa uguale
  a quella del parametro partendo dalla posizione del cursore
  Parametri:
  const char *str;    stringa da cercare
  int sensitive;      sensibile al maiuscolo (default = TRUE)

  La funzione ritorna il puntatore al RESBUF o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::SearchNextName(const char *str, int sensitive)
{
   TCHAR   *UnicodeString = gsc_CharToUnicode(str); 
   presbuf res = SearchNextName(UnicodeString, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
presbuf C_RB_LIST::SearchNextName(const TCHAR *str, int sensitive)
{
   if (IsEmpty()) return NULL;

   while (cursor)
      if (cursor->restype == RTSTR && 
          gsc_strcmp(cursor->resval.rstring, str, sensitive) == 0) break;
      else cursor = cursor->rbnext;

   return cursor;
}


/*************************************************************************/
/*.doc C_RB_LIST::Assoc                                       <external> */  
/*+
  Se una lista di resbuf è strutturata come una lista LISP la funzione 
  ricerca la sottolista il cui primo elemento è una stringa nota.
  (vedi funzioni assoc del LISP).
  Parametri:
  const char *str;    stringa da cercare
  int sensitive;      sensibile al maiuscolo (default = FALSE)

  La funzione ritorna il puntatore al RESBUF o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::Assoc(const char *str, int sensitive)
{
   return cursor = gsc_assoc(str, head, sensitive);
}
presbuf C_RB_LIST::Assoc(const TCHAR *str, int sensitive)
{
   return cursor = gsc_assoc(str, head, sensitive);
}


/*************************************************************************/
/*.doc C_RB_LIST::CdrAssoc                                    <external> */  
/*+
  Se una lista di resbuf è strutturata come una lista LISP la funzione 
  ricerca il secondo elemento di una sottolista cercata sul primo della stessa
  (vedi funzioni cdr e assoc del LISP).
  Parametri:
  const char *str;    stringa da cercare
  int sensitive;      sensibile al maiuscolo (default = FALSE)

  La funzione ritorna il puntatore al RESBUF o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf C_RB_LIST::CdrAssoc(const char *str, int sensitive)
{
   return gsc_nth(1, gsc_assoc(str, head, sensitive));
}
presbuf C_RB_LIST::CdrAssoc(const TCHAR *str, int sensitive)
{
   return gsc_nth(1, gsc_assoc(str, head, sensitive));
}


/*************************************************************************/
/*.doc C_RB_LIST::CdrAssocSubst                               <external> */  
/*+
  Se una lista di resbuf è strutturata come una lista LISP la funzione 
  ricerca il secondo elemento di una sottolista cercata sul primo della stessa
  (vedi funzioni cdr e assoc del LISP) e sostituisce il suo valore con uno nuovo.
  Parametri:
  const char *str;                              stringa da cercare
  long, int, double, char *, presbuf new_value; nuovo valore    
  int sensitive;                                sensibile al maiuscolo (default = FALSE)

  La funzione ritorna GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/
/*************************************************************************/
int C_RB_LIST::CdrAssocSubst(const char *str, long new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }
int C_RB_LIST::CdrAssocSubst(const TCHAR *str, long new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }

int C_RB_LIST::CdrAssocSubst(const char *str, int new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }
int C_RB_LIST::CdrAssocSubst(const TCHAR *str, int new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }

int C_RB_LIST::CdrAssocSubst(const char *str, double new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }
int C_RB_LIST::CdrAssocSubst(const TCHAR *str, double new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }

int C_RB_LIST::CdrAssocSubst(const char *str, char *new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }
int C_RB_LIST::CdrAssocSubst(const TCHAR *str, TCHAR *new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }

int C_RB_LIST::CdrAssocSubst(const char *str, presbuf new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }
int C_RB_LIST::CdrAssocSubst(const TCHAR *str, presbuf new_value, int sensitive)
   { return gsc_RbSubst(CdrAssoc(str, sensitive), new_value); }

int C_RB_LIST::CdrAssocSubst(C_RB_LIST &ColValues, int sensitive)
{
   presbuf pSrc;
   int     i = 0;

   while ((pSrc = ColValues.nth(i++)))
      CdrAssocSubst(pSrc->rbnext->resval.rstring, 
                    pSrc->rbnext->rbnext, sensitive);

   return GS_GOOD; 
}


/*************************************************************************/
/*.doc C_RB_LIST::SubstRTNONEtoDifferent                       <external> */  
/*+
  Sostituisce i resbuf della lista che differiscono dalla lista in input con
  RTNONE.
  Parametri:
  C_RB_LIST &mrb;       Lista da confrontare
  int sensitive;        sensibile al maiuscolo (default = FALSE)

  La funzione ritorna GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/
/*************************************************************************/
int C_RB_LIST::SubstRTNONEtoDifferent(C_RB_LIST &mrb, int sensitive)
{
   int ndx = 0;
   presbuf pRB1, pRB2;

   pRB1 = mrb.get_head();
   pRB2 = get_head();
   // Confronto i campi annullando quelli diversi
   while (pRB1 && pRB2)
   {
      if (pRB2->restype != RTNIL)
         if (gsc_equal(pRB2, pRB1, sensitive) == GS_BAD)
            gsc_RbSubstNONE(pRB2);

      pRB1 = pRB1->rbnext;
      pRB2 = pRB2->rbnext;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc C_RB_LIST::nth                                         <external> */  
/*+
  Se una lista di resbuf è strutturata come una lista LISP la funzione 
  ricerca l'i-esimo elemento (vedi funzioni nth del LISP).
  Parametri:
  int i;      indice della lista

  La funzione ritorna il puntatore al RESBUF o NULL nel 
  caso di errore nei parametri o nel caso non trovi l'elemento.
-*/
/*************************************************************************/
presbuf C_RB_LIST::nth(int i)
{
   return (cursor = gsc_nth(i, head));
}


/*********************************************************/
/*.doc void C_RB_LIST::LspRetList             <external> */
/*+                                                                       
   Funzione che, se è necessario (se la lista contiene una
   sola sottolista) aggiunge una parentesi aperta ed una 
   chiusa all' inizio e alla fine della lista passata in input.
   Tutto questo perchè in Map 2000 l' acedRetList non funziona
   più come prima.
     
   N.B. Non restituisce alcun valore ma modifica la lista.
-*/  
/*********************************************************/
void C_RB_LIST::LspRetList(void)
{
   presbuf p;

   // correggo la lista per il lisp
   // A list returned to AutoLISP by acedRetList() can include only the following result type codes:
   // RTREAL, RTPOINT, RTSHORT, RTANG, RTSTR, RTENAME, RTPICKS, RTORINT, RT3DPOINT, RTT, RTNIL, RTLB, RTLE, and RTDOTE. 
   p = get_head();
   while (p)
   {
      switch (p->restype)
      {
         case RTNONE: // No result
         case RTVOID: // Blank symbol
            gsc_RbSubstNIL(p);
            break;
         case RTLONG: // Long integer
         {
            double n = p->resval.rlong;
            gsc_RbSubst(p, n); 
            break;
         }
         case RTDXF0: // DXF code 0 for ads_buildlist only
         case RTRESBUF: // resbuf
         case RTMODELESS: // interrupted by modeless dialog
            break;
      }
      p = p->rbnext;
   }

   // se inizia per una parentesi aperta e non c'è la corrispondente chiusa
   if (get_head() && get_head()->restype == RTLB && (!(p = gsc_scorri(get_head())) || 
      !(p = p->rbnext))) // oppure dopo la parentesi chiusa la lista continua
   {
      link_head(acutBuildList(RTLB, 0)); // aggiungo una parentesa aperta all'inizio
      *this += acutBuildList(RTLE, 0); // aggiungo una parentesa chiusa alla fine
   }
   acedRetList(get_head());
}


/*************************************************************************/
/*.doc C_RB_LIST::SubstRTNONEtoDifferent                       <external> */  
/*+
  inserisce una lista dopo la posizione attuale del cursore.
  Parametri:
  resbuf *prb;       Lista da inserire

  La funzione ritorna GS_GOOD in caso di successo altrimenti ritorna GS_BAD.
-*/
/*************************************************************************/
int C_RB_LIST::insert_after(C_RB_LIST &mrb)
{
   return insert_after(mrb.get_head());
}
int C_RB_LIST::insert_after(resbuf *prb)
{
   presbuf p, pInit, pFinal;
   int     prb_length = 1;

   if (!prb) return GS_GOOD;
   if (!cursor && head) return GS_BAD;

   if ((pInit = gsc_copybuf(prb)) == NULL) return GS_BAD;
   pFinal = pInit;
   prb = prb->rbnext;
   while (prb)
   {
      if ((pFinal->rbnext = gsc_copybuf(prb)) == NULL) { acutRelRb(pInit); return GS_BAD; }
      pFinal = pFinal->rbnext;
      prb = prb->rbnext;
      prb_length++;
   }

   if (!head)
      head = cursor = pInit;
   else
   {
      p = cursor->rbnext;
      cursor->rbnext = pInit;
      pFinal->rbnext = p;
   }
   if (cursor == tail) tail = pFinal;

   count += prb_length;

   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// fine funzioni class C_RB_LIST                                             //
// class C_ENT_FAMILY                                                        //
//---------------------------------------------------------------------------//

C_ENT_FAMILY::C_ENT_FAMILY() : C_NODE() {}
      
C_ENT_FAMILY::~C_ENT_FAMILY()
{ 
   family.remove_all(); 
}  

long C_ENT_FAMILY::get_id() 
{ 
   return id; 
}

int C_ENT_FAMILY::set_id(long in) 
{ 
   id=in; 
   return GS_GOOD; 
}

int evid(int mode) 
{
   return GS_GOOD;
};



//---------------------------------------------------------------------------//
// class C_ENT_FAMILY_LIST                                                   //
//---------------------------------------------------------------------------//

C_ENT_FAMILY_LIST::C_ENT_FAMILY_LIST() : C_LIST() {}

C_ENT_FAMILY_LIST::~C_ENT_FAMILY_LIST() {} 


//---------------------------------------------------------------------------//
// class C_INT_LONG                                                         //
//---------------------------------------------------------------------------//

C_INT_LONG::C_INT_LONG() : C_INT() 
{ 
   n_long = 0;
}

C_INT_LONG::C_INT_LONG(int in1, long in2)
{
   set_key(in1);
   set_id(in2);
}

C_INT_LONG::~C_INT_LONG() {}

long C_INT_LONG::get_id() 
{ 
   return n_long; 
}

int C_INT_LONG::set_id(long in) 
{ 
   n_long = in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_INT_LONG_LIST                                                    //
//---------------------------------------------------------------------------//

C_INT_LONG_LIST::C_INT_LONG_LIST() : C_LIST() {}

C_INT_LONG_LIST::~C_INT_LONG_LIST() {}

int C_INT_LONG_LIST::copy(C_INT_LONG_LIST *out)
{
   C_INT_LONG *new_node, *punt;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   punt = (C_INT_LONG *) get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_INT_LONG) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      new_node->key = punt->key;
      new_node->set_id(punt->get_id());
      out->add_tail(new_node);
      punt = (C_INT_LONG*) get_next();
   }
   return GS_GOOD;
}

int C_INT_LONG_LIST::add_tail_int_long(int in1, long in2)
{
   C_INT_LONG *p;

   if ((p = new C_INT_LONG(in1, in2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

C_INT_LONG* C_INT_LONG_LIST::search(int in1, long in2)
{
   C_INT_LONG *punt = (C_INT_LONG *) get_head();

   while (punt)
   {
      if (in1 == punt->get_key() && in2 == punt->get_id()) break;
      punt = (C_INT_LONG *) get_next();
   }

   return punt;
}


//---------------------------------------------------------------------------//
// class C_INT_VOIDPTR                                                       //
//---------------------------------------------------------------------------//

C_INT_VOIDPTR::C_INT_VOIDPTR() : C_INT() 
{ 
   VoidPtr = NULL;
}

C_INT_VOIDPTR::C_INT_VOIDPTR(int in1, void *in2)
{
   set_key(in1);
   set_VoidPtr(in2);
}

C_INT_VOIDPTR::~C_INT_VOIDPTR() {}

void *C_INT_VOIDPTR::get_VoidPtr() 
{ 
   return VoidPtr; 
}

int C_INT_VOIDPTR::set_VoidPtr(void *in) 
{ 
   VoidPtr = in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_INT_VOIDPTR_LIST                                                  //
//---------------------------------------------------------------------------//

C_INT_VOIDPTR_LIST::C_INT_VOIDPTR_LIST() : C_LIST() {}

C_INT_VOIDPTR_LIST::~C_INT_VOIDPTR_LIST() {}

int C_INT_VOIDPTR_LIST::copy(C_INT_VOIDPTR_LIST *out)
{
   C_INT_VOIDPTR *new_node, *punt;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   punt = (C_INT_VOIDPTR *) get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_INT_VOIDPTR) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      new_node->key = punt->key;
      new_node->set_VoidPtr(punt->get_VoidPtr());
      out->add_tail(new_node);
      punt = (C_INT_VOIDPTR*) get_next();
   }
   return GS_GOOD;
}

int C_INT_VOIDPTR_LIST::add_tail_int_voidptr(int in1, void *in2)
{
   C_INT_VOIDPTR *p;

   if ((p = new C_INT_VOIDPTR(in1, in2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}


//---------------------------------------------------------------------------//
// class C_REAL                                                              //
//---------------------------------------------------------------------------//


C_REAL::C_REAL() : C_NODE() 
   { key = 0.0; }
C_REAL::C_REAL(double in) : C_NODE() 
   { set_key(in); }
      
C_REAL::~C_REAL() {}

double C_REAL::get_key_double()  
{ 
   return key; 
}

int C_REAL::set_key(double in)  
{ 
   key=in; 
   return GS_GOOD; 
}

resbuf* C_REAL::to_rb(void)  
{ 
   return acutBuildList(RTREAL, key, 0);
}

int C_REAL::comp(void *in)
{
   if (key == ((C_REAL *) in)->key)
      return 0;
   else
      return (key > ((C_REAL *) in)->key) ? 1 : -1; 
}


//---------------------------------------------------------------------------//
// class C_REAL_LIST                                                          //
//---------------------------------------------------------------------------//

C_REAL_LIST::C_REAL_LIST():C_LIST() {}

C_REAL_LIST::~C_REAL_LIST() {}

/*********************************************************/
/*.doc C_REAL_LIST::sort_name <external> */
/*+
  Questa funzione ordina una lista C_REAL_LIST o derivate in base al valore
  di key.
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_REAL_LIST::sort_key(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_REAL_compare_by_key_asc;
   else                   fCompPtr = &gsc_C_REAL_compare_by_key_desc;

   sort();
}


int C_REAL_LIST::copy(C_REAL_LIST *out)
{
   C_REAL *new_node,*punt;

   if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }

   punt=(C_REAL*)get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_REAL)==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }      
      new_node->key = punt->key;
      out->add_tail(new_node);
      punt = (C_REAL*) get_next();
   }
   return GS_GOOD;
}

int C_REAL_LIST::insert_ordered_double(double Val, bool *inserted)
{
   C_REAL *pReal = new C_REAL(Val);
   bool   _inserted;

   if (C_LIST::insert_ordered(pReal, &_inserted) == GS_BAD)
      { delete pReal; return GS_BAD; }
   if (_inserted == false) delete pReal;
   if (inserted) *inserted = _inserted;

   return GS_GOOD;
}

resbuf *C_REAL_LIST::to_rb(void)
{
   resbuf *list = NULL,*rb;
   C_REAL *punt;

   punt = (C_REAL *) get_tail();
   if (!punt) 
      if ((list = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   while (punt)
   {                                
      if ((rb = acutBuildList(RTREAL, punt->key, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext = list;
      list = rb;
      punt = (C_REAL *) get_prev();
   }                             
   
   return list;      
}

int C_REAL_LIST::from_rb(resbuf *rb)
{
   C_REAL *nod;

   remove_all();

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   switch (rb->restype)
   {
      case RTNONE: case RTNIL:
         return GS_GOOD;

      case RTPOINT:
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[0]);
         add_tail(nod);
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[1]);
         add_tail(nod);
         return GS_GOOD;

      case RT3DPOINT:
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[0]);
         add_tail(nod);
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[1]);
         add_tail(nod);
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[2]);
         add_tail(nod);
         return GS_GOOD;

      case RTLB:
         while ((rb=rb->rbnext) != NULL && rb->restype != RTLE)
         {
            if ((nod = new C_REAL) == NULL)
               { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            add_tail(nod);
            switch (rb->restype)
            {
               case RTSHORT:
                  nod->set_key(rb->resval.rint);
                  break;
               case RTREAL:
               case RTANG:
               case RTORINT:
                  nod->set_key(rb->resval.rreal);
                  break;
               case RTLONG:
                  nod->set_key(rb->resval.rlong);
                  break;
               default:
                  remove_all();
                  GS_ERR_COD = eGSInvRBType;
                  return GS_BAD;
            }
         }
         if (!rb) { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }    
         return GS_GOOD;

      case 10: case 11: case 12: case 13: case 14: case 15:
      case 16: case 17: case 18: case 19:
      case 1010: case 1011: case 1012: case 1013: case 1014: case 1015:
      case 1016: case 1017: case 1018: case 1019:
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->restype);
         add_tail(nod);
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[X]);
         add_tail(nod);
         if ((nod = new C_REAL) == NULL)
            { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         nod->set_key(rb->resval.rpoint[Y]);
         add_tail(nod);
         if (rb->resval.rpoint[Z] != 0)
         {
            if ((nod = new C_REAL) == NULL)
               { remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            nod->set_key(rb->resval.rpoint[Z]);
            add_tail(nod);
         }
         return GS_GOOD;
   }

   GS_ERR_COD = eGSInvRBType;
   
   return GS_BAD; 
}

int C_REAL_LIST::getpos_key_double(double in) // 1 e' il primo
{                                          // 0 non trovato
   int cc=0;

   cursor=head;
   while(cursor!=NULL)
   {
      cc++; 
      if (cursor->get_key_double()==in) return cc;
      cursor = cursor->get_next();
   }
   return 0;
}

int C_REAL_LIST::add_tail_double(double Val)
{
   C_REAL *p;

   if ((p = new C_REAL(Val)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}


//---------------------------------------------------------------------------//
// class C_REAL_VOIDPTR                                                      //
//---------------------------------------------------------------------------//

C_REAL_VOIDPTR::C_REAL_VOIDPTR() : C_REAL() 
{ 
   VoidPtr = NULL;
}

C_REAL_VOIDPTR::C_REAL_VOIDPTR(double in1, void *in2)
{
   set_key(in1);
   set_VoidPtr(in2);
}

C_REAL_VOIDPTR::~C_REAL_VOIDPTR() {}

void *C_REAL_VOIDPTR::get_VoidPtr() 
{ 
   return VoidPtr; 
}

int C_REAL_VOIDPTR::set_VoidPtr(void *in) 
{ 
   VoidPtr = in; 
   return GS_GOOD; 
}


//---------------------------------------------------------------------------//
// class C_REAL_VOIDPTR_LIST                                                 //
//---------------------------------------------------------------------------//

C_REAL_VOIDPTR_LIST::C_REAL_VOIDPTR_LIST() : C_LIST() {}

C_REAL_VOIDPTR_LIST::~C_REAL_VOIDPTR_LIST() {}

int C_REAL_VOIDPTR_LIST::copy(C_REAL_VOIDPTR_LIST *out)
{
   C_REAL_VOIDPTR *new_node, *punt;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   punt = (C_REAL_VOIDPTR *) get_head();
   out->remove_all();
   
   while (punt)
   {
      if ((new_node = new C_REAL_VOIDPTR) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      new_node->key = punt->key;
      new_node->set_VoidPtr(punt->get_VoidPtr());
      out->add_tail(new_node);
      punt = (C_REAL_VOIDPTR*) get_next();
   }
   return GS_GOOD;
}

int C_REAL_VOIDPTR_LIST::add_tail_double_voidptr(double in1, void *in2)
{
   C_REAL_VOIDPTR *p;

   if ((p = new C_REAL_VOIDPTR(in1, in2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

int C_REAL_VOIDPTR_LIST::insert_ordered_double_voidptr(double in1, void *in2, bool *inserted)
{
   C_REAL_VOIDPTR *p = new C_REAL_VOIDPTR(in1, in2);
   bool   _inserted;

   if (C_LIST::insert_ordered(p, &_inserted) == GS_BAD)
      { delete p; return GS_BAD; }
   if (_inserted == false) delete p;
   if (inserted) *inserted = _inserted;

   return GS_GOOD;
}



//---------------------------------------------------------------------------//
// class C_2REAL                                                              //
//---------------------------------------------------------------------------//


C_2REAL::C_2REAL() : C_REAL() 
   { key_2 = 0.0; }
C_2REAL::C_2REAL(double in1, double in2) : C_REAL() 
{
   set_key(in1);
   set_key_2(in2);
}
      
C_2REAL::~C_2REAL() {}

double C_2REAL::get_key_2_double()  
   { return key_2; }

int C_2REAL::set_key_2(double in)  
{ 
   key_2 = in; 
   return GS_GOOD; 
}

void C_2REAL::copy(C_2REAL *out)
{
   out->set_key(get_key_double());
   out->set_key_2(get_key_2_double());
}


//---------------------------------------------------------------------------//
// class C_2REAL_LIST                                                        //
//---------------------------------------------------------------------------//

C_2REAL_LIST::C_2REAL_LIST():C_LIST() {}

C_2REAL_LIST::~C_2REAL_LIST() {}


/*********************************************************/
/*.doc C_REAL_LIST::sort_name <external> */
/*+
  Questa funzione ordina una lista C_2REAL_LIST o derivate in base al valore
  di key.
  int ascending;  se = TRUE ordina in modo crescente (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_2REAL_LIST::sort_key(int ascending)
{
   if (ascending == TRUE) fCompPtr = &gsc_C_REAL_compare_by_key_asc;
   else                   fCompPtr = &gsc_C_REAL_compare_by_key_desc;

   sort();
}

int C_2REAL_LIST::add_tail_2double(double in1, double in2)
{
   C_2REAL *p;

   if ((p = new C_2REAL(in1, in2)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return add_tail(p);
}

int C_2REAL_LIST::insert_ordered_2double(double Val1, double Val2, bool *inserted)
{
   C_2REAL *p2Real = new C_2REAL(Val1, Val2);
   bool    _inserted;

   if (C_LIST::insert_ordered(p2Real, &_inserted) == GS_BAD)
      { delete p2Real; return GS_BAD; }
   if (_inserted == false) delete p2Real;
   if (inserted) *inserted = _inserted;

   return GS_GOOD;
}


//---------------------------------------------------------------------------//
// class C_2STR_INT                                                          //
//---------------------------------------------------------------------------//

C_2STR_INT::C_2STR_INT() : C_2STR() {}
C_2STR_INT::C_2STR_INT(const TCHAR *in1, const TCHAR *in2, int in3) : C_2STR()
{
   set_name(in1); set_name2(in2); set_type(in3);
}
C_2STR_INT::C_2STR_INT(const char *in1, const char *in2, int in3) : C_2STR()
{
   set_name(in1); set_name2(in2); set_type(in3);
}
      
C_2STR_INT::~C_2STR_INT() {}

int C_2STR_INT::get_type() 
{ 
   return type; 
}

int C_2STR_INT::set_type(int in) 
{ 
   type = in;
   return GS_GOOD; 
} 


//---------------------------------------------------------------------------//
// class C_2STR_INT       -  fine                                            //
// class C_2STR_INT_LIST  -  inizio                                          //
//---------------------------------------------------------------------------//


C_2STR_INT_LIST::C_2STR_INT_LIST() : C_LIST() {}

C_2STR_INT_LIST::~C_2STR_INT_LIST() {}

int C_2STR_INT_LIST::copy(C_2STR_INT_LIST *out)
{
   C_2STR_INT *new_node, *punt;

   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   out->remove_all();
   
   punt = (C_2STR_INT *) get_head();
   while (punt)
   {
      if ((new_node = new C_2STR_INT(punt->get_name(), punt->get_name2(), punt->get_type())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; } 
      out->add_tail(new_node);

      punt = (C_2STR_INT *) get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_2STR_INT_LIST::save                  <external> */
/*+
  Questa funzione salva la lista nel file indicato.
  Parametri:
  C_STRING &Path;          Path di un file
  TCHAR Sep;               Carattere separatore (Default = ',')
  TCHAR char *Sez;         Sezione nel file (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. LA sezione deve essere eslcusivamente dedicata a questa lista.
-*/  
/*********************************************************/
int C_2STR_INT_LIST::save(C_STRING &Path, TCHAR Sep, const TCHAR *Sez)
{
   int        ndx = 1;
   C_STRING   str_ndx, Buffer;
   C_2STR_INT *p;
   bool       Unicode = false;
   FILE       *file;

   if (Sez && gsc_path_exist(Path) == GS_GOOD) // Cancello la sezione
      gsc_delete_sez(Path.get_name(), Sez);

   if ((file = gsc_open_profile(Path, UPDATEABLE, MORETESTS, &Unicode)) == NULL) return GS_BAD;

   p = (C_2STR_INT *) get_head();
   while (p)
   {
      str_ndx = ndx;
      Buffer = p->get_name();
      Buffer += Sep;
      Buffer += p->get_name2();
      Buffer += Sep;
      Buffer += p->get_type();
      if (gsc_set_profile(file, Sez, str_ndx.get_name(), Buffer.get_name(), 0, Unicode) == GS_BAD)
         { gsc_close_profile(file); return GS_BAD; }
      ndx++;
      p = (C_2STR_INT *) get_next();
   }

   return gsc_close_profile(file);
}

//-----------------------------------------------------------------------//
int C_2STR_INT_LIST::load(C_STRING &Path, TCHAR Sep, const TCHAR *Sez)
{
   C_2STR_INT *p;
   C_STRING   str_ndx, Buffer;
   int        ndx = 1;
   bool       Unicode = false;
   FILE       *file;
   TCHAR      *str, *name, *name2;
   int        n_campi;

   remove_all();
   if ((file = gsc_open_profile(Path, READONLY, MORETESTS, &Unicode)) == NULL) return GS_BAD;

   str_ndx = ndx;
   while (gsc_get_profile(file, Sez, str_ndx.get_name(), Buffer) == GS_GOOD)
   {
      str     = Buffer.get_name();
      n_campi = gsc_strsep(str, _T('\0')) + 1;
      if (n_campi < 3) // numero insufficiente di informazioni
         { gsc_close_profile(file); GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      name = str;
      while (*str != _T('\0')) str++; str++;
      name2 = str;
      while (*str != _T('\0')) str++; str++;

      if ((p = new C_2STR_INT(name, name2, _wtoi(str))) == NULL)
         { gsc_close_profile(file); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      add_tail(p);
      str_ndx = ndx++;
   }

   return gsc_close_profile(file);
}


int C_2STR_INT_LIST::from_rb(resbuf *rb)
{
   C_2STR_INT *p;
   TCHAR      *str, *str2;
   int        num;

   remove_all();

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   
   if (rb->restype != RTLB) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   while ((rb = rb->rbnext) != NULL && rb->restype != RTLE)
   {
      if (rb->restype != RTLB || !(rb = rb->rbnext) || rb->restype != RTSTR)
         { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }
      str = rb->resval.rstring;
      if (!(rb = rb->rbnext) || rb->restype != RTSTR)
         { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }
      str2 = rb->resval.rstring;
      if (!(rb = rb->rbnext) || gsc_rb2Int(rb, &num) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }

      if ((p = new C_2STR_INT(str, str2, num)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; remove_all(); return GS_BAD; }
      add_tail(p);

      if (!(rb = rb->rbnext) || rb->restype != RTLE)
         { GS_ERR_COD = eGSInvRBType; remove_all(); return GS_BAD; }
   }
   if (!rb) { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      
   return GS_GOOD;
}

resbuf *C_2STR_INT_LIST::to_rb(void)
{
   resbuf     *list = NULL, *rb;
   C_2STR_INT *p;
   TCHAR      str[] = GS_EMPTYSTR, *string;

   p = (C_2STR_INT *) get_tail();
   while (p)
   {                                
      if ((rb = acutBuildList(RTSHORT, p->get_type(), RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext = list;
      list = rb;

      if ((string = p->get_name2()) == NULL) string = str;
      if ((rb = acutBuildList(RTSTR, string, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext = list;
      list = rb;

      if ((string = p->get_name()) == NULL) string = str;
      if ((rb = acutBuildList(RTLB, RTSTR, string, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext->rbnext = list;
      list = rb;

      p = (C_2STR_INT *) get_prev();
   }                             
   
   return list;      
}


//---------------------------------------------------------------------------//
// class C_2STR_INT_LIST  -  fine                                            //
// class C_2LONG_STR      -  inizio                                          //
//---------------------------------------------------------------------------//


C_2LONG_STR::C_2LONG_STR() : C_2LONG() {}
      
C_2LONG_STR::~C_2LONG_STR() {}

TCHAR *C_2LONG_STR::get_name() 
{ 
   return name.get_name(); 
}

int C_2LONG_STR::set_name(const char *in) 
{ 
   return name.set_name(in); 
}
int C_2LONG_STR::set_name(const TCHAR *in) 
{ 
   return name.set_name(in); 
}


//---------------------------------------------------------------------------//
// class C_2LONG_STR_LIST                                                     //
//---------------------------------------------------------------------------//

C_2LONG_STR_LIST::C_2LONG_STR_LIST() : C_LIST() {}

C_2LONG_STR_LIST::~C_2LONG_STR_LIST() {}


//---------------------------------------------------------------------------//
// class C_LONG_REAL                                                         //
//---------------------------------------------------------------------------//

C_LONG_REAL::C_LONG_REAL() : C_LONG() {}
      
C_LONG_REAL::~C_LONG_REAL() {}

double C_LONG_REAL::get_real() { return Num; }
int C_LONG_REAL::set_real(double in) { Num = in; return GS_GOOD; }


//---------------------------------------------------------------------------//
// class C_LONG_REAL_LIST                                                    //
//---------------------------------------------------------------------------//

C_LONG_REAL_LIST::C_LONG_REAL_LIST() : C_LIST() {}

C_LONG_REAL_LIST::~C_LONG_REAL_LIST() {}

C_LONG_REAL *C_LONG_REAL_LIST::search(long in)
{
   C_LONG_REAL *punt = (C_LONG_REAL*)get_head();

   while (punt)
   {
      if (in == punt->get_id()) break;
      punt = (C_LONG_REAL*) get_next();
   }

   return punt;
}


//---------------------------------------------------------------------------//
// class C_BNODE per gestire alberi binari                                   //
//---------------------------------------------------------------------------//


C_BNODE::C_BNODE() 
{ 
   next_sx = next_dx = prev = NULL;
   BalanceCoeff = 0;
}
      
C_BNODE::C_BNODE(C_BNODE *p, C_BNODE *n_sx, C_BNODE *n_dx) 
{ 
   prev    = p;
   next_sx = n_sx;
   next_dx = n_dx;
   BalanceCoeff = 0;
}

C_BNODE::~C_BNODE() {}

int C_BNODE::compare(const void *in)
   { return 0; }

void C_BNODE::print(int *level)
   { return; }


//---------------------------------------------------------------------------//
// class C_BTREE per gestire alberi binari                                   //
//---------------------------------------------------------------------------//


C_BTREE::C_BTREE() 
{ 
   root  = cursor = NULL; 
   count = 0;
}

C_BTREE::~C_BTREE() 
   { remove_all(); }


/*********************************************************/
/*.doc C_BTREE::max_deep                      <internal> */
/*+
  Calcola la massima profondità dell'albero partendo dal nodo p.
  Parametri:
  C_BNODE *p;      Nodo di inizio
  long    i;       Uso interno contatore (default = 0)
-*/  
/*********************************************************/
long C_BTREE::max_deep(C_BNODE *p, long i)
{
   long MaxDx, MaxSx;

   if (!p) return i - 1;
   i++;
   MaxSx = max_deep(p->next_sx, i);
   MaxDx = max_deep(p->next_dx, i);

   return max(MaxSx, MaxDx);
}


/*********************************************************/
/*.doc C_BTREE::rot_dx                        <internal> */
/*+
  Effettua una rotazione semplice a dx per bilanciare l'albero.
  Parametri:
  C_BNODE *p;       Nodo di inizio
-*/  
/*********************************************************/
void C_BTREE::rot_dx(C_BNODE *p)
{
   C_BNODE *pFather = p->prev, *pChildSx = p->next_sx, *pChildDx = p->next_dx;

   // Il padre del nodo punta lo figlio sx
   if (!pFather) // il nodo è la root dell'albero
      root = pChildSx;
   else
   {
      if (pFather->next_sx == p) pFather->next_sx = pChildSx;
      else pFather->next_dx = pChildSx;
   }
   // Il figlio sx ha come precedente il padre del nodo
   pChildSx->prev = pFather;

   // Il successivo sx del nodo diventa il figlio dx di suo figlio sx
   p->next_sx = pChildSx->next_dx;
   // Il figlio dx di suo figlio sx ha come precedente il nodo
   if (pChildSx->next_dx) pChildSx->next_dx->prev = p;

   // Il precedente del nodo diventa il figlio sx
   p->prev = pChildSx;
   // il successivo dx del figlio sx del nodo diventa il nodo
   pChildSx->next_dx = p;

   // Aggiorno il fattore di bilanciamento di p e di suo figlio sx
   p->BalanceCoeff        = (short) (max_deep(p->next_dx) - max_deep(p->next_sx));
   pChildSx->BalanceCoeff = (short) (max_deep(pChildSx->next_dx) - max_deep(pChildSx->next_sx));
}


/*********************************************************/
/*.doc C_BTREE::rot_sx                        <internal> */
/*+
  Effettua una rotazione semplice a sx per bilanciare l'albero.
  Parametri:
  C_BNODE *p;       Nodo di inizio
-*/  
/*********************************************************/
void C_BTREE::rot_sx(C_BNODE *p)
{
   C_BNODE *pFather = p->prev, *pChildSx = p->next_sx, *pChildDx = p->next_dx;

   // Il padre del nodo punta al figlio dx
   if (!pFather) // il nodo è la root dell'albero
      root = pChildDx;
   else
   {
      if (pFather->next_sx == p) pFather->next_sx = pChildDx;
      else pFather->next_dx = pChildDx;
   }
   // Il figlio dx ha come precedente il padre del nodo
   pChildDx->prev = pFather;

   // Il successivo dx del nodo diventa il figlio sx di suo figlio dx
   p->next_dx = pChildDx->next_sx;
   // Il figlio sx di suo figlio dx ha come precedente il nodo
   if (pChildDx->next_sx) pChildDx->next_sx->prev = p;

   // Il precedente del nodo diventa il figlio dx
   p->prev = pChildDx;
   // Il successivo sx del figlio dx del nodo diventa il nodo
   pChildDx->next_sx = p;

   // Aggiorno il fattore di bilanciamento di p e di suo figlio dx
   p->BalanceCoeff        = (short) (max_deep(p->next_dx) - max_deep(p->next_sx));
   pChildDx->BalanceCoeff = (short) (max_deep(pChildDx->next_dx) - max_deep(pChildDx->next_sx));
}


/*********************************************************/
/*.doc C_BTREE::go_top                        <internal> */
/*+
  Cerca il minimo partendo dal nodo p. Se p non viene passato si
  parte dalla radice cercando il minimo di tutto l'albero.
  Parametri:
  C_BNODE *p;       Nodo di inizio ricerca, opzionale (default = NULL)
-*/  
/*********************************************************/
C_BNODE* C_BTREE::go_top(C_BNODE *p)
{
   if (p == NULL) p = root;
   cursor = p;
   if (!cursor) return NULL;

   while (cursor->next_sx)
      cursor = cursor->next_sx;

   return cursor;
}
      
/*********************************************************/
/*.doc C_BTREE::go_bottom                     <internal> */
/*+
  Cerca il massimo partendo dal nodo p. Se p non viene passato si
  parte dalla radice cercando il massimo di tutto l'albero.
  Parametri:
  C_BNODE *p;       Nodo di inizio ricerca, opzionale (default = NULL)
-*/  
/*********************************************************/
C_BNODE* C_BTREE::go_bottom(C_BNODE *p)
{
   if (p == NULL) p = root;
   cursor = p;
   if (!cursor) return NULL;

   while (cursor->next_dx)
      cursor = cursor->next_dx;

   return cursor;
}

C_BNODE* C_BTREE::search(const void *in)
{
   cursor = root;

   while (cursor)
   {
      switch (cursor->compare(in))
      {
         case 0: // uguali
            return cursor;
         case -1: // se elemento > del padre  
            cursor = cursor->next_dx;
            break;
         case 1: // se elemento < del padre
            cursor = cursor->next_sx;
            break;
      }
   }

   return NULL;
}

void C_BTREE::clear(C_BNODE *p)
{
   if (p->next_sx) clear(p->next_sx);
   if (p->next_dx) clear(p->next_dx);
   delete p;
}

long C_BTREE::get_count() 
   { return count; }
      
C_BNODE* C_BTREE::get_cursor(void) 
   { return cursor; }
// questa funzione è estremamente pericolosa perchè si fida che ptr
// sia il puntatore di un elemento del C_BTREE corrente
int C_BTREE::set_cursor(C_BNODE *ptr)
{
   cursor = ptr;

   return GS_GOOD;
}   


C_BNODE* C_BTREE::go_next(void) 
{
   if (!cursor) return NULL;

   if (cursor->next_dx)
      cursor = go_top(cursor->next_dx); // il minimo del ramo destro
   else
   {
      C_BNODE* p = cursor;

      while (cursor = p->prev)
         if (cursor->next_sx == p) break;
         else p = cursor;
   }

   return cursor;
}

C_BNODE* C_BTREE::go_prev(void) 
{
   if (!cursor) return NULL;

   if (cursor->next_sx)
      cursor = go_bottom(cursor->next_sx); // il massimo del ramo sinistro
   else
   {
      C_BNODE* p = cursor;

      while (cursor = p->prev)
         if (cursor->next_dx == p) break;
         else p = cursor;
   }

   return cursor;   
}


/*********************************************************/
/*.doc C_BTREE::upd_balance_coeff             <internal> /*
/*+
  Questa funzione, aggiorna i fattori di bilanciamento 
  dei nodi che vanno dal padre del nodo p fino alla root
  Nel caso si sia aggiunto un nodo il ciclo si ferma
  appena si trova uno sbilanciamento di 2 o -2.
  Parametri:
  C_BNODE *p;
  bool OnAdd;                       Se = true si è già aggiunto il nodo p altrimenti
                                    si ha intenzione di cancellare il nodo p
  C_BNODE **pMaxDeepUnbalanceNode;  Puntatore al nodo più profondo che ha fattore
                                    di bilanciamento 2 o -2 
-*/  
/*********************************************************/
void C_BTREE::upd_balance_coeff(C_BNODE *p, bool OnAdd, 
                                C_BNODE **pMaxDeepUnbalanceNode)
{
   C_BNODE *pFather = p->prev, *pChild = p;

   *pMaxDeepUnbalanceNode = NULL;

   // Ricalcola i fattori di bilanciamento dei nodi dal padre di p alla root
   if (OnAdd) // aggiunta di un nodo
   {
      while (pFather)
      {         
         if (pFather->next_dx == pChild) (pFather->BalanceCoeff)++; // Se figlio dx
         else (pFather->BalanceCoeff)--; // Se figlio sx

         // Se il coefficiente di bilanciamento diventa 0 vuol dire che 
         // non è variata la profondità del sottoalbero di pFather
         if (pFather->BalanceCoeff == 0) break; // ci si può fermare

         if (*pMaxDeepUnbalanceNode == NULL)
            // Memorizzo il primo nodo che ha fattore di bilanciamento 2 o -2
            if (pFather->BalanceCoeff == 2 || pFather->BalanceCoeff == -2)
            {
               *pMaxDeepUnbalanceNode = pFather; // mi fermo
               break; 
            }

         // mi sposto al nodo superiore
         pChild  = pFather;
         pFather = pFather->prev;
      }
   }
   else // cancellazione di un nodo
   {
      while (pFather)
      {         
         if (pFather->next_dx == pChild)
         {
            (pFather->BalanceCoeff)--; // Se figlio dx

            if (*pMaxDeepUnbalanceNode == NULL)
               // Memorizzo il primo nodo che ha fattore di bilanciamento -2
               if (pFather->BalanceCoeff == -2)
               {
                  *pMaxDeepUnbalanceNode = pFather; // mi fermo
                  break; 
               }

            // Se il coefficiente di bilanciamento è < 0 vuol dire che 
            // non varierà la profondità del sottoalbero di pFather
            if (pFather->BalanceCoeff < 0) break; // ci si può fermare
         }
         else
         {
            (pFather->BalanceCoeff)++; // Se figlio sx

            if (*pMaxDeepUnbalanceNode == NULL)
               // Memorizzo il primo nodo che ha fattore di bilanciamento 2
               if (pFather->BalanceCoeff == 2)
               {
                  *pMaxDeepUnbalanceNode = pFather; // mi fermo
                  break; 
               }

            // Se il coefficiente di bilanciamento è > 0 vuol dire che 
            // non varierà la profondità del sottoalbero di pFather
            if (pFather->BalanceCoeff > 0) break; // ci si può fermare
         }

         // mi sposto al nodo superiore
         pChild  = pFather;
         pFather = pFather->prev;
      }
   }
}


/*********************************************************/
/*.doc C_BTREE::balance                       <internal> /*
/*+
  Questa funzione, bilancia partendo da un nodo noto.
  Parametri:
  C_BNODE *p;
  bool *IsHeightUpdated; Opzionale; Se passato contiene se l'albero 
                         che aveva radice in p ha variato la sua altezza
                         (default = NULL)

  Ritorna true se è stato necessario bilanciare il nodo e false se
  era già bilanciato.
-*/  
/*********************************************************/
bool C_BTREE::balance(C_BNODE *p, bool *IsHeightUpdated)
{
   long PrevHeight;

   if (IsHeightUpdated) *IsHeightUpdated = false;

   // Se il nodo ha coefficiente di bilanciamento = +2
   if (p->BalanceCoeff == 2)
   {
      if (IsHeightUpdated) PrevHeight = max_deep(p);

      // Se suo figlio dx ha un coefficiente = -1
      if (p->next_dx->BalanceCoeff == -1)
      {
         // effettuo una rotazione a destra-sinistra
         rot_dx(p->next_dx);
         rot_sx(p);
      }
      else // Altrimenti suo figlio dx ha un coefficiente = +1 o = 0
      {
         // effettuo una rotazione a sinistra
         rot_sx(p);
      }

      if (IsHeightUpdated) 
         *IsHeightUpdated = (PrevHeight != max_deep(p->prev)) ? true : false;

      return true;
   }
   else
      // Se il nodo ha coefficiente di bilanciamento = -2
      if (p->BalanceCoeff == -2)               
      {
         if (IsHeightUpdated) PrevHeight = max_deep(p);

         // Se suo figlio sx ha un coefficiente = 1
         if (p->next_sx->BalanceCoeff == 1)
         {
            // effettuo una rotazione a sinistra-destra
            rot_sx(p->next_sx);
            rot_dx(p);
         }
         else // Altrimenti suo figlio sx un coefficiente = -1 o 0
         {
            // effettuo una rotazione a destra
            rot_dx(p);
         }

         if (IsHeightUpdated) 
            *IsHeightUpdated = (PrevHeight != max_deep(p->prev)) ? true : false;

         return true;
      }

   return false;
}


/*********************************************************/
/*.doc C_BTREE::add                           <external> /*
/*+
  Questa funzione, inserisce un nuovo elemento nell'albero.
  Cerca la posizione esatta e se la chiave dell'elemento non è
  ancora presente lo inserisce allocando un nuovo elemento.
  In questo modo vengono vietati elementi doppi.
  La funzione mantiene l'albero bilanciato.
  L'elemento inserito sarà puntato dal cursore dell'albero.
  Se si desidera sapere se l'elemento è stato inserito o se
  esisteva già bisogna verificare il n. di elementi prima e dopo
  la chiamata a questa funzione.
  Parametri:
  const void *in;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_BTREE::add(const void *in)
{
   C_BNODE *p;
   int     Compare;
   bool    Stop = false;

   // Se l'albero è vuoto
   if (!root)
   {
      if ((cursor = root = (C_BNODE *) alloc_item(in)) == NULL) return GS_BAD;
      count = 1;
      
      return GS_GOOD;
   }

   // Cerco la posizione giusta
   p = root;
   do
   {
      switch ((Compare = p->compare(in)))
      {
         case 0: // elemento già presente
            cursor = p;
            return GS_GOOD;
         case -1: // se elemento > del padre  
            if (p->next_dx == NULL) { Stop = true; break; }
            p = p->next_dx;
            break;
         case 1: // se elemento < del padre
            if (p->next_sx == NULL) { Stop = true; break; }
            p = p->next_sx;
            break;
      }
   }
   while (!Stop);

   if ((cursor = alloc_item(in)) == NULL) return NULL;

   if (Compare == -1) // Se elemento > del padre  
      p->next_dx = cursor;
   else // Se elemento < del padre
      p->next_sx = cursor;

   cursor->prev = p;
   count++;

   // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
   // su inserimento nuovo nodo cercando il nodo più profondo con
   // coefficiente di bilanciamento = 2 o = -2
   upd_balance_coeff(cursor, true, &p);

   // Se esiste un nodo da bilanciare
   if (p)
      balance(p); // lo bilancio con le rotazioni

   return GS_GOOD;
}

     
int C_BTREE::remove(const void *in)
{
   search(in);
   return remove_at();
}


/*********************************************************/
/*.doc C_BTREE::invert_nodes                   <internal> /*
/*+
  Questa funzione inverte le posizioni dei 2 nodi nell'albero.
  Parametri:
  C_BNODE *p1;    Puntatore al primo nodo
  C_BNODE *p2;    Puntatore al secondo nodo
-*/  
/*********************************************************/
void C_BTREE::invert_nodes(C_BNODE *p1, C_BNODE *p2)
{
   C_BNODE *p1Father = p1->prev, *p1ChildSx = p1->next_sx, *p1ChildDx = p1->next_dx;
   C_BNODE *p2Father = p2->prev, *p2ChildSx = p2->next_sx, *p2ChildDx = p2->next_dx;
   
   if (p2Father == p1) // se p1 è padre di p2
   {
      // imposto il nuovo padre di p2
      if (p1Father)
      {
         if (p1Father->next_dx == p1) p1Father->next_dx = p2;
         else p1Father->next_sx = p2;
      }
      else
         root = p2;
      p2->prev = p1Father;

      // imposto i nuovi figli di p2
      if (p1ChildSx == p2) // se p2 era figlio sx
      {
         p2->next_sx = p1;
         p2->next_dx = p1ChildDx;
         if (p1ChildDx) p1ChildDx->prev = p2;
      }
      else
      {
         p2->next_dx = p1;
         p2->next_sx = p1ChildSx;
         if (p1ChildSx) p1ChildSx->prev = p2;
      }
      p1->prev = p2;

      // imposto i nuovi figli di p1
      p1->next_dx = p2ChildDx;
      if (p2ChildDx) p2ChildDx->prev = p1;
      p1->next_sx = p2ChildSx;
      if (p2ChildSx) p2ChildSx->prev = p1;
   }
   else
   if (p1Father == p2) // se p2 è padre di p1
   {
      // imposto il nuovo padre di p1
      if (p2Father)
      {
         if (p2Father->next_dx == p2) p2Father->next_dx = p1;
         else p2Father->next_sx = p1;
      }
      else
         root = p1;
      p1->prev = p2Father;

      // imposto i nuovi figli di p1
      if (p2ChildSx == p1) // se p1 era figlio sx
      {
         p1->next_sx     = p2;
         p1->next_dx     = p2ChildDx;
         if (p2ChildDx) p2ChildDx->prev = p1;
      }
      else
      {
         p1->next_dx     = p2;
         p1->next_sx     = p2ChildSx;
         if (p2ChildSx) p2ChildSx->prev = p1;
      }
      p2->prev = p1;

      // imposto i nuovi figli di p2
      p2->next_dx = p1ChildDx;
      if (p1ChildDx) p1ChildDx->prev = p2;
      p2->next_sx = p1ChildSx;
      if (p1ChildSx) p1ChildSx->prev = p2;
   }
   else // se p2 e p1 non hanno legami diretti
   {
      // imposto il nuovo padre di p2
      if (p1Father)
      {
         if (p1Father->next_dx == p1) p1Father->next_dx = p2;
         else p1Father->next_sx = p2;
      }
      else
         root = p2;
      p2->prev = p1Father;

      // imposto i nuovi figli di p2
      p2->next_sx = p1ChildSx;
      if (p1ChildSx) p1ChildSx->prev = p2;
      p2->next_dx = p1ChildDx;
      if (p1ChildDx) p1ChildDx->prev = p2;

      // imposto il nuovo padre di p1
      if (p2Father)
      {
         if (p2Father->next_dx == p2) p2Father->next_dx = p1;
         else p2Father->next_sx = p1;
      }
      else
         root = p1;
      p1->prev = p2Father;

      // imposto i nuovi figli di p1
      p1->next_sx = p2ChildSx;
      if (p2ChildSx) p2ChildSx->prev = p1;
      p1->next_dx = p2ChildDx;
      if (p2ChildDx) p2ChildDx->prev = p1;

      // imposto i nuovi figli di p2
      p2->next_sx = p1ChildSx;
      if (p1ChildSx) p1ChildSx->prev = p2;
      p2->next_dx = p1ChildDx;
      if (p1ChildDx) p1ChildDx->prev = p2;
   }

   // sostituisco i fattori di bilanciamento
   short dummy = p1->BalanceCoeff;
   p1->BalanceCoeff = p2->BalanceCoeff;
   p2->BalanceCoeff = dummy;
}


/*********************************************************/
/*.doc C_BTREE::remove_at                     <external> /*
/*+
  Questa funzione, cancella l'elemento puntato dal cursore dell'albero.
  Il cursore sarà spostato sull'elemento successivo.
  La funzione mantiene l'albero bilanciato.
  Se l'elemento è una foglia la rimuove.
  Se il nodo ha un solo figlio quest'ultimo viene collegato al padre
  del nodo da cancellare.
  Se il nodo ha 2 figli sostituisco il nodo con il suo predecessore
  (considerando l'ordine) rimuovendolo dalla sua posizione (quest'ultimo
  avrà un solo figlio).
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_BTREE::remove_at(void)
{
   if (!cursor) return GS_BAD;

   C_BNODE *pFather = cursor->prev, *pChildSx = cursor->next_sx, *pChildDx = cursor->next_dx;
   C_BNODE *p, *pNext, *pStartBalance = NULL;

   p      = cursor;
   pNext  = go_next(); // cerco il successivo
   cursor = p;

   // Se l'elemento ha almeno un figlio
   if (pChildDx || pChildSx)
   {
      if (!pChildDx) // Se ha solo il figlio sinistro
      {  
         // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
         // su cancellazione nodo
         upd_balance_coeff(cursor, false, &pStartBalance);
         
         // Collego il padre del nodo con suo figlio sinistro
         if (pFather)
         {
            if (pFather->next_dx == cursor) pFather->next_dx = pChildSx;
            else pFather->next_sx = pChildSx;
         }
         else
            root = pChildSx;

         pChildSx->prev = pFather;
      }
      else if (!pChildSx) // Se ha solo il figlio destro
      {
         // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
         // su cancellazione nodo
         upd_balance_coeff(cursor, false, &pStartBalance);
         
         // Collego il padre del nodo con suo figlio destro
         if (pFather)
         {
            if (pFather->next_dx == cursor) pFather->next_dx = pChildDx;
            else pFather->next_sx = pChildDx;
         }
         else
            root = pChildDx;

         pChildDx->prev = pFather;
      }
      else // Se ha due figli
      {
         C_BNODE *pPrev;

         p      = cursor;
         pPrev  = go_prev(); // cerco il precedente
         cursor = p;

         // Inverto le posizioni del nodo da cancellare con quella del precedente
         invert_nodes(cursor, pPrev);
         pFather = cursor->prev; // leggo il nuovo padre di cursor
         pChildSx = cursor->next_sx; // leggo il nuovo figlio sx di cursor
         // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
         // su cancellazione nodo
         upd_balance_coeff(cursor, false, &pStartBalance);
         
         // Collego il padre del nodo al nuovo figlio sx
         if (pFather->next_dx == cursor) pFather->next_dx = pChildSx;
         else pFather->next_sx = pChildSx;
         
         if (pChildSx) pChildSx->prev = pFather;
      }
   }
   else // Se l'elemento è una foglia
   {
      if (pFather) // Se ha un padre
      {
         // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
         // su cancellazione nodo
         upd_balance_coeff(cursor, false, &pStartBalance);

         if (pFather->next_dx == cursor)
            pFather->next_dx = NULL;
         else
            pFather->next_sx = NULL;
      }
      else
         root = NULL;
   }

   delete cursor;
   cursor = pNext; // mi posiziono sull'elemento successivo
   count--;

   bool IsChildSx, IsHeightUpdated;

   // Partendo da pStartBalance alla root eseguo le opportune rotazioni
   // sui nodi sbilanciati
   while (pStartBalance)
   {
      p = pStartBalance->prev;
      IsChildSx = (p && p->next_sx == pStartBalance) ? true : false;

      // lo bilancio con le rotazioni
      balance(pStartBalance, &IsHeightUpdated);

      // Se non esiste un padre del nodo bilanciato oppure
      // se il bilanciamento non ha variato l'altezza dell'albero 
      // allora mi fermo
      if (!p || !IsHeightUpdated) break; 

      // Ricalcola i fattori di bilanciamento dei nodi da cursore alla root
      // su cancellazione nodo
      if (IsChildSx)
         upd_balance_coeff(p->next_sx, false, &pStartBalance);
      else
         upd_balance_coeff(p->next_dx, false, &pStartBalance);
   }

   return GS_GOOD;
}

void C_BTREE::remove_all()
{
   if (!root) return;
   clear(root);
   root  = cursor = NULL;
   count = 0;
}

C_BNODE* C_BTREE::alloc_item(const void *in)
{
   C_BNODE *p = new C_BNODE;

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return p;
}

void C_BTREE::print(void)
{
   if (!root) return;
   int level = 0;
   print(root, &level);
}

void C_BTREE::print(C_BNODE *p, int *level)
{
   if (p)
   {
      if (level) (*level)++;
      if (p->next_dx) print(p->next_dx, level);
      p->print(level);
      if (p->next_sx) print(p->next_sx, level);
      if (level) (*level)--;
   }
}


//---------------------------------------------------------------------------//
// class C_BSTR                                                               //
//---------------------------------------------------------------------------//


// costruttore
C_BSTR::C_BSTR() : C_BNODE() {}

C_BSTR::C_BSTR(const char *in) : C_BNODE() 
   { name.set_name(in); }
C_BSTR::C_BSTR(const TCHAR *in) : C_BNODE() 
   { name.set_name(in); }

// distruttore
C_BSTR::~C_BSTR() {}

TCHAR* C_BSTR::get_name() 
   { return name.get_name(); }

int C_BSTR::set_name(const char *in) 
   { return name.set_name(in); }
int C_BSTR::set_name(const TCHAR *in) 
   { return name.set_name(in); }

int C_BSTR::compare(const void *in) // <in> deve essere una stringa unicode
{
   int res = gsc_strcmp(get_name(), (const TCHAR *) in);

   if (res == 0) return 0;
   else return (res > 0) ? 1 : -1;
}

void C_BSTR::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%s"), get_name());
}


//---------------------------------------------------------------------------//
// class C_STR_BTREE                                                         //
//---------------------------------------------------------------------------//


C_STR_BTREE::C_STR_BTREE():C_BTREE() {}

C_STR_BTREE::~C_STR_BTREE() {}

C_BNODE* C_STR_BTREE::alloc_item(const void *in) // <in> deve essere una stringa unicode
{
   C_BSTR *p = new C_BSTR((const TCHAR *) in);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


//---------------------------------------------------------------------------//
// class C_B2STR                                                             //
//---------------------------------------------------------------------------//


// costruttore
C_B2STR::C_B2STR() : C_BSTR() {}

C_B2STR::C_B2STR(const TCHAR *in, const TCHAR *in2) : C_BSTR() 
   { name = in; name2 = in2; }

// distruttore
C_B2STR::~C_B2STR() {}

TCHAR* C_B2STR::get_name2() 
   { return name2.get_name(); }

int C_B2STR::set_name2(const TCHAR *in) 
   { name2 = in; return GS_GOOD; }

void C_B2STR::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%s, %s"), get_name(), get_name2());
}


//---------------------------------------------------------------------------//
// class C_2STR_BTREE                                                        //
//---------------------------------------------------------------------------//


C_2STR_BTREE::C_2STR_BTREE():C_BTREE() {}

C_2STR_BTREE::~C_2STR_BTREE() {}

C_BNODE* C_2STR_BTREE::alloc_item(const void *in) // <in> deve essere una stringa unicode
{
   C_B2STR *p = new C_B2STR((const TCHAR *) in, NULL);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


//---------------------------------------------------------------------------//
// class C_BPROFILE_SECTION                                                  //
//---------------------------------------------------------------------------//


// costruttore
C_BPROFILE_SECTION::C_BPROFILE_SECTION() : C_BSTR() {}

C_BPROFILE_SECTION::C_BPROFILE_SECTION(const TCHAR *in) : C_BSTR() 
   { name = in; }

// distruttore
C_BPROFILE_SECTION::~C_BPROFILE_SECTION() {}

C_2STR_BTREE* C_BPROFILE_SECTION::get_ptr_EntryList(void) 
   { return &EntryList; }

int C_BPROFILE_SECTION::set_entry(const TCHAR *entry, const TCHAR *value)
{
   if (!value) // si vuole cancellare
   {
      get_ptr_EntryList()->remove(entry);
      return GS_GOOD;
   }
   else
   {
      C_B2STR *pEntry;

      if (!(pEntry = (C_B2STR *) get_ptr_EntryList()->search(entry)))
      {
         if (get_ptr_EntryList()->add(entry) == GS_BAD) return GS_BAD;
         pEntry = (C_B2STR *) get_ptr_EntryList()->get_cursor();
      }
      pEntry->set_name2(value);
   }

   return GS_GOOD;
}
int C_BPROFILE_SECTION::set_entry(const TCHAR *entry, double value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(entry, Buffer.get_name());
}
int C_BPROFILE_SECTION::set_entry(const TCHAR *entry, int value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(entry, Buffer.get_name());
}
int C_BPROFILE_SECTION::set_entry(const TCHAR *entry, long value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(entry, Buffer.get_name());
}
int C_BPROFILE_SECTION::set_entry(const TCHAR *entry, bool value)
{
   return set_entry(entry, (value) ? _T("1") : _T("0"));
}

int C_BPROFILE_SECTION::get_entry(const TCHAR *entry, C_STRING &value)
{
   C_B2STR *pProfileEntry = (C_B2STR *) get_ptr_EntryList()->search(entry);

   if (!pProfileEntry) { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   value = pProfileEntry->get_name();

   return GS_GOOD;
}
int C_BPROFILE_SECTION::get_entry(const TCHAR *entry, double *value)
{
   C_B2STR *pProfileEntry = (C_B2STR *) get_ptr_EntryList()->search(entry);

   if (!pProfileEntry) { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   *value = _wtof(pProfileEntry->get_name());

   return GS_GOOD;
}
int C_BPROFILE_SECTION::get_entry(const TCHAR *entry, int *value)
{
   C_B2STR *pProfileEntry = (C_B2STR *) get_ptr_EntryList()->search(entry);

   if (!pProfileEntry) { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   *value = _wtoi(pProfileEntry->get_name());

   return GS_GOOD;
}
int C_BPROFILE_SECTION::get_entry(const TCHAR *entry, long *value)
{
   C_B2STR *pProfileEntry = (C_B2STR *) get_ptr_EntryList()->search(entry);

   if (!pProfileEntry) { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   *value = _wtol(pProfileEntry->get_name());

   return GS_GOOD;
}
int C_BPROFILE_SECTION::get_entry(const TCHAR *entry, bool *value)
{
   C_B2STR *pProfileEntry = (C_B2STR *) get_ptr_EntryList()->search(entry);

   if (!pProfileEntry) { GS_ERR_COD = eGSEntryNotFound; return GS_BAD; }

   *value = (gsc_strcmp(pProfileEntry->get_name(), _T("0")) == 0) ? FALSE : TRUE;

   return GS_GOOD;
}

int C_BPROFILE_SECTION::set_entryList(C_2STR_LIST &EntryValueList)
{
   C_2STR *p = (C_2STR *) EntryValueList.get_head();
   while (p)
   {
      if (set_entry(p->get_name(), p->get_name2()) != GS_GOOD) return GS_BAD;
      p = (C_2STR *) EntryValueList.get_next();
   }
   return GS_GOOD;
}

void C_BPROFILE_SECTION::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%s"), get_name());
   get_ptr_EntryList()->print();
}


//---------------------------------------------------------------------------//
// class C_PROFILE_SECTION_BTREE                                             //
//---------------------------------------------------------------------------//


C_PROFILE_SECTION_BTREE::C_PROFILE_SECTION_BTREE():C_BTREE() {}

C_PROFILE_SECTION_BTREE::~C_PROFILE_SECTION_BTREE() {}

C_BNODE* C_PROFILE_SECTION_BTREE::alloc_item(const void *in) // <in> deve essere una stringa unicode
{
   C_BPROFILE_SECTION *p = new C_BPROFILE_SECTION((const TCHAR *) in);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}

C_B2STR* C_PROFILE_SECTION_BTREE::get_entry(const void *section, const void *entry)
{
   C_BPROFILE_SECTION *pSection;
   C_B2STR            *pEntry;

   if ((pSection = (C_BPROFILE_SECTION *) search(section)) == NULL) return NULL;
   if ((pEntry   = (C_B2STR *) pSection->get_ptr_EntryList()->search(entry)) == NULL) return NULL;
   
   return pEntry;
}

int C_PROFILE_SECTION_BTREE::set_entry(const TCHAR *section, const TCHAR *entry, const TCHAR *value)
{
   C_BPROFILE_SECTION *pSection = (C_BPROFILE_SECTION *) search(section);

   if (!value) // si vuole cancellare
   {
      if (!pSection) return GS_GOOD;

      if (!entry) // si vuole cancellare la section
         remove_at();
      else // si vuole cancellare l'entry
         pSection->set_entry(entry, value);

      return GS_GOOD;
   }
   else
   {
      if (!pSection)
      {
         if (add(section) == GS_BAD) return GS_BAD;
         pSection = (C_BPROFILE_SECTION *) get_cursor();
      }
      pSection->set_entry(entry, value);
   }

   return GS_GOOD;
}
int C_PROFILE_SECTION_BTREE::set_entry(const TCHAR *section, const TCHAR *entry, double value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(section, entry, Buffer.get_name());
}
int C_PROFILE_SECTION_BTREE::set_entry(const TCHAR *section, const TCHAR *entry, int value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(section, entry, Buffer.get_name());
}
int C_PROFILE_SECTION_BTREE::set_entry(const TCHAR *section, const TCHAR *entry, long value)
{
   C_STRING Buffer;
   Buffer = value;
   return set_entry(section, entry, Buffer.get_name());
}
int C_PROFILE_SECTION_BTREE::set_entry(const TCHAR *section, const TCHAR *entry, bool value)
{
   return set_entry(section, entry, (value) ? _T("1") : _T("0"));
}
int C_PROFILE_SECTION_BTREE::set_entryList(const TCHAR *section, C_2STR_LIST &EntryValueList)
{
   C_BPROFILE_SECTION *pSection = (C_BPROFILE_SECTION *) search(section);

   if (!pSection)
   {
      if (add(section) == GS_BAD) return GS_BAD;
      pSection = (C_BPROFILE_SECTION *) get_cursor();
   }
   return pSection->set_entryList(EntryValueList);
}

//---------------------------------------------------------------------------//
// class C_BLONG                                                             //
//---------------------------------------------------------------------------//


// costruttore
C_BLONG::C_BLONG() : C_BNODE() { key = 0; }

C_BLONG::C_BLONG(long in) : C_BNODE() 
   { key = in; }

// distruttore
C_BLONG::~C_BLONG() {}

long C_BLONG::get_key() 
   { return key; }

int C_BLONG::set_key(long in) 
   { key = in; return GS_GOOD; }

int C_BLONG::compare(const void *in) // <in> deve essere un puntatore a long
{
   long *pDummy = (long *) in;

   if (key == (long) (*pDummy)) return 0;
   return (key > (long) (*pDummy)) ? 1 : -1;
}

void C_BLONG::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%ld(%d)"), get_key(), BalanceCoeff);
}


//---------------------------------------------------------------------------//
// class C_LONG_BTREE                                                        //
//---------------------------------------------------------------------------//


C_LONG_BTREE::C_LONG_BTREE():C_BTREE() {}

C_LONG_BTREE::~C_LONG_BTREE() {}

C_BNODE* C_LONG_BTREE::alloc_item(const void *in) // <in> deve essere un puntatore a long
{   
   const long *dummy = (long *) in;
   C_BLONG *p = new C_BLONG(*dummy);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


/*********************************************************/
/*.doc C_LONG_BTREE::subtract                 <external> */
/*+
  Questa funzione filtra solo i codici che non sono presenti 
  nella lista indicata dal parametro <in>.
  Parametri:
  C_LONG_BTREE &in;   lista di codici
-*/  
/*********************************************************/
void C_LONG_BTREE::subtract(C_LONG_BTREE &in)
{
   C_BLONG *pItem = (C_BLONG *) in.go_top();
   long    Value;

   while (pItem)
   {
      Value = pItem->get_key();
      if (search(&Value) != NULL) remove_at();

      pItem = (C_BLONG *) in.go_next();
   }
}


/*********************************************************/
/*.doc C_LONG_BTREE::intersect                <external> */
/*+
  Questa funzione filtra solo i codici che sono presenti 
  anche nella lista indicata dal parametro <in>.
  Parametri:
  C_LONG_BTREE &in;   lista di codici
-*/  
/*********************************************************/
void C_LONG_BTREE::intersect(C_LONG_BTREE &in)
{
   C_BLONG *pItem = (C_BLONG *) go_top();
   long    Value;

   while (pItem)
   {
      Value = pItem->get_key();
      if (in.search(&Value) == NULL)
      {
         remove_at();
         pItem = (C_BLONG *) in.get_cursor();
      }
      else
         pItem = (C_BLONG *) in.go_next();
   }
}

int C_LONG_BTREE::add_list(C_LONG_BTREE &in)
{
   C_BLONG *pItem = (C_BLONG *) in.go_top();
   long    Value;

   while (pItem)
   {
      Value = pItem->get_key();
      if (add(&Value) != GS_GOOD) return GS_BAD;

      pItem = (C_BLONG *) in.go_next();
   }
   
   return GS_GOOD;
}

int C_LONG_BTREE::from_rb(resbuf *rb)
{
   long    Value;

   remove_all();

   if (!rb) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   switch (rb->restype)
   {
      case RTNONE: case RTNIL:
         return GS_GOOD;

      case RTPOINT:
         Value = (long) rb->resval.rpoint[0];
         if (add(&Value) != GS_GOOD) return GS_BAD;
         Value = (long) rb->resval.rpoint[1];
         if (add(&Value) != GS_GOOD) return GS_BAD;
         return GS_GOOD;

      case RT3DPOINT:
         Value = (long) rb->resval.rpoint[0];
         if (add(&Value) != GS_GOOD) return GS_BAD;
         Value = (long) rb->resval.rpoint[1];
         if (add(&Value) != GS_GOOD) return GS_BAD;
         Value = (long) rb->resval.rpoint[2];
         if (add(&Value) != GS_GOOD) return GS_BAD;
         return GS_GOOD;

      case RTLB:
         while ((rb=rb->rbnext) != NULL && rb->restype != RTLE)
         {
            switch (rb->restype)
            {
               case RTSHORT:
                  Value = (long) rb->resval.rint;
                  break;
               case RTREAL:
               case RTANG:
               case RTORINT:
                  Value = (long) rb->resval.rreal;
                  break;
               case RTLONG:
                  Value = rb->resval.rlong;
                  break;
               default:
                  remove_all();
                  GS_ERR_COD = eGSInvRBType;
                  return GS_BAD;
            }
            if (add(&Value) != GS_GOOD) return GS_BAD;
         }
         if (!rb) { remove_all(); GS_ERR_COD = eGSInvRBType; return GS_BAD; }    
         return GS_GOOD;

      case 10: case 11: case 12: case 13: case 14: case 15:
      case 16: case 17: case 18: case 19:
      case 1010: case 1011: case 1012: case 1013: case 1014: case 1015:
      case 1016: case 1017: case 1018: case 1019:
         Value = (long) rb->restype;
         if (add(&Value) != GS_GOOD) return GS_BAD;

         Value = (long) rb->resval.rpoint[X];
         if (add(&Value) != GS_GOOD) return GS_BAD;

         Value = (long) rb->resval.rpoint[Y];
         if (add(&Value) != GS_GOOD) return GS_BAD;

         if (rb->resval.rpoint[Z] != 0)
         {
            Value = (long) rb->resval.rpoint[Z];
            if (add(&Value) != GS_GOOD) return GS_BAD;
         }

         return GS_GOOD;
   }

   GS_ERR_COD = eGSInvRBType;
   
   return GS_BAD; 
}

resbuf *C_LONG_BTREE::to_rb(void)
{
   resbuf *list = NULL,*rb;
   C_BLONG *punt;

   punt = (C_BLONG *) go_bottom();
   if (!punt) 
      if ((list = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   while (punt)
   {                                
      if ((rb = acutBuildList(RTREAL, punt->get_key(), 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      rb->rbnext = list;
      list = rb;
      punt = (C_BLONG *) go_prev();
   }                             
   
   return list;      
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_LONG_BTREE
// INIZIO FUNZIONI C_BREAL
///////////////////////////////////////////////////////////////////////////


// costruttore
C_BREAL::C_BREAL() : C_BNODE() { key = 0; }

C_BREAL::C_BREAL(double in) : C_BNODE() 
   { key = in; }

// distruttore
C_BREAL::~C_BREAL() {}

double C_BREAL::get_key() 
   { return key; }

int C_BREAL::set_key(double in) 
   { key = in; return GS_GOOD; }

int C_BREAL::compare(const void *in) // <in> deve essere un puntatore a long
{
   double *pDummy = (double *) in;

   if (key == (double) (*pDummy)) return 0;
   return (key > (double) (*pDummy)) ? 1 : -1;
}

void C_BREAL::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%f(%d)"), get_key(), BalanceCoeff);
}


//---------------------------------------------------------------------------//
// class C_REAL_BTREE                                                        //
//---------------------------------------------------------------------------//


C_REAL_BTREE::C_REAL_BTREE():C_BTREE() {}

C_REAL_BTREE::~C_REAL_BTREE() {}

C_BNODE* C_REAL_BTREE::alloc_item(const void *in) // <in> deve essere un puntatore a double
{   
   const double *dummy = (double *) in;
   C_BREAL *p = new C_BREAL(*dummy);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_REAL_BTREE
// INIZIO FUNZIONI C_BSTR_REAL
///////////////////////////////////////////////////////////////////////////


// costruttore
C_BSTR_REAL::C_BSTR_REAL() : C_BSTR() { key = 0; }

C_BSTR_REAL::C_BSTR_REAL(const TCHAR *in1) // costruttore che riceve solo la chiave
   { name.set_name(in1); key = 0; }

// distruttore
C_BSTR_REAL::~C_BSTR_REAL() {}

double C_BSTR_REAL::get_key() 
   { return key; }

int C_BSTR_REAL::set_key(double in) 
   { key = in; return GS_GOOD; }

void C_BSTR_REAL::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%s,%f(%d)"), get_name(), get_key(), BalanceCoeff);
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BSTR_REAL
// INIZIO FUNZIONI C_STR_REAL_BTREE
///////////////////////////////////////////////////////////////////////////


C_STR_REAL_BTREE::C_STR_REAL_BTREE():C_STR_BTREE() {}

C_STR_REAL_BTREE::~C_STR_REAL_BTREE() {}

C_BNODE* C_STR_REAL_BTREE::alloc_item(const void *in) // <in> deve essere una stringa unicode
{   
   C_BSTR_REAL *p = new C_BSTR_REAL((const TCHAR *) in);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_STR_REAL_BTREE
// INIZIO FUNZIONI C_BLONG_INT
///////////////////////////////////////////////////////////////////////////


// costruttore
C_BLONG_INT::C_BLONG_INT() : C_BLONG() { m_int = 0; }

C_BLONG_INT::C_BLONG_INT(long in1) // costruttore che riceve solo la chiave
   { key = in1; m_int = 0; }

// distruttore
C_BLONG_INT::~C_BLONG_INT() {}

int C_BLONG_INT::get_int() 
   { return m_int; }

int C_BLONG_INT::set_int(int in) 
   { m_int = in; return GS_GOOD; }

void C_BLONG_INT::print(int *level)
{
   acutPrintf(GS_LFSTR);
   if (level)
      for (int i = 1; i < *level; i++) acutPrintf(_T("  "));
   acutPrintf(_T("%ld,%d(%d)"), get_key(), get_int(), BalanceCoeff);
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BLONG_INT
// INIZIO FUNZIONI C_LONG_INT_BTREE
///////////////////////////////////////////////////////////////////////////


C_LONG_INT_BTREE::C_LONG_INT_BTREE():C_LONG_BTREE() {}

C_LONG_INT_BTREE::~C_LONG_INT_BTREE() {}

C_BNODE* C_LONG_INT_BTREE::alloc_item(const void *in) // <in> deve essere un puntatore a long
{   
   const long *dummy = (long *) in;
   C_BLONG_INT *p = new C_BLONG_INT(*dummy);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_LONG_INT_BTREE
// INIZIO FUNZIONI C_POINT
///////////////////////////////////////////////////////////////////////////


// costruttori
C_POINT::C_POINT() : C_NODE()
   { Bulge = 0.0; ads_point_clear(point); }
C_POINT::C_POINT(ads_point in, double _bulge) : C_NODE()
   { Bulge = _bulge; ads_point_set(in, point); }
C_POINT::C_POINT(double _X, double _Y, double _Z, double _bulge, int Prec) : C_NODE()
   { Set(_X, _Y, _Z, _bulge, Prec); }
C_POINT::C_POINT(const TCHAR *Str) : C_NODE()
   { Bulge = 0.0; fromStr(Str); }
C_POINT::C_POINT(const char *Str) : C_NODE()
   { Bulge = 0.0; fromStr(Str); }
C_POINT::C_POINT(C_POINT &in) : C_NODE()
   { Bulge = in.Bulge; ads_point_set(in.point, point); }

// distruttore
C_POINT::~C_POINT() {}

void C_POINT::Set(double _X, double _Y, double _Z, double _bulge, int Prec)
{
   if (Prec < 0)
      { point[X] = _X; point[Y] = _Y; point[Z] = _Z; }
   else
   {
      TCHAR buf[50 + 1], frm[10];

      swprintf(frm, 50 + 1, _T("%%.%df"), Prec);

      _snwprintf_s(buf, 50, frm, _X); buf[50] = _T('\0');
      point[X] = _wtof(buf);
      _snwprintf_s(buf, 50, frm, _Y); buf[50] = _T('\0');
      point[Y] = _wtof(buf);
      _snwprintf_s(buf, 50, frm, _Z); buf[50] = _T('\0');
      point[Z] = _wtof(buf);
   }

   Bulge = _bulge;
}
void C_POINT::Set(C_POINT &in)
   { Bulge = in.Bulge; ads_point_set(in.point, point); }
void C_POINT::Set(ads_point in, double _bulge)
   { Bulge = _bulge; ads_point_set(in, point); }

void C_POINT::toStr(C_STRING &Str, GeomDimensionEnum geom_dim, TCHAR CoordSep)
{
   Str = point[X];
   Str += CoordSep;
   Str += point[Y];
   if (geom_dim == GS_3D)
   {
      Str += CoordSep;
      Str += point[Z];
   }
}


/*************************************************************************/
/*.doc C_POINT::fromStr                                                  */  
/*+
  La funzione inizializza il punto da una stringa espressa nella forma
  "X,Y[,Z[,bulge]]". Tra quadre ci sono i valori opzionali.
  Parametri:
  const char *Str;  stringa da elaborare
  oppure
  const TCHAR *Str;  stringa da elaborare
  TCHAR Sep;         Opzionale, Carattere separatore tra coordinate (default = ',')
  int *nRead;        Opzionale, ritorna il numero di caratteri letti (default = NULL)

  Restituisce il GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT::fromStr(const char *Str)
{
   C_STRING Buffer(Str);

   return fromStr(Buffer.get_name());
}
int C_POINT::fromStr(const TCHAR *Str, TCHAR Sep, int *nRead)
{
   C_STRING Buffer(Str);
   TCHAR    *p;
   int      nCoord;

   Buffer.alltrim();
   p      = Buffer.get_name();
   nCoord = gsc_strsep(p, _T('\0'), Sep) + 1;
   if (nCoord < 2) // numero insufficiente di informazioni
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   point[X] = _wtof(p);

   while (*p!= _T('\0')) p++; p++;
   point[Y] = _wtof(p);

   point[Z] = 0.0;
   Bulge = 0.0;
   if (nCoord > 2)
   {
      // Coordinata Z
      while (*p!= _T('\0')) p++; p++;
      point[Z] = _wtof(p);

      if (nCoord > 3)
      {
         // Bulge
         while (*p!= _T('\0')) p++; p++;
         Bulge = _wtof(p);
      }
   }

   if (nRead)
   {
       while (*p != _T('\0')) p++;
       *nRead = (int) (p -  Buffer.get_name());
   }

   return GS_GOOD;
}

double C_POINT::x (void) 
   { return point[X]; }
double C_POINT::y (void) 
   { return point[Y]; }
double C_POINT::z (void) 
   { return point[Z]; }

bool C_POINT::operator == (C_POINT &in)
{
   return gsc_point_equal(point, in.point);
}
bool C_POINT::operator != (C_POINT &in)
{
   return !gsc_point_equal(point, in.point);
}

///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_POINT
// INIZIO FUNZIONI C_POINT_LIST
///////////////////////////////////////////////////////////////////////////


int C_POINT_LIST::copy(C_POINT_LIST &out)
{
   C_POINT *punt = (C_POINT *) get_head(), *pNewPt;

   out.remove_all();

   while (punt)
   {
      if ((pNewPt = new C_POINT(*punt)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      out.add_tail(pNewPt);

      punt = (C_POINT *) get_next();
   }
   
   return GS_GOOD;
}

int C_POINT_LIST::add_point(ads_point p, double bulge)
{
   C_POINT *punt;

   if (!p) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if ((punt = new C_POINT(p, bulge)) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   add_tail(punt);

   return GS_GOOD;
}
int C_POINT_LIST::add_point(double _X, double _Y, double _Z, double _bulge)
{
   C_POINT *punt;

   if ((punt = new C_POINT(_X, _Y, _Z, _bulge)) == NULL)
     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   add_tail(punt);

   return GS_GOOD;
}


/*********************************************************/
/*.doc add_connect_point_list <internal> */
/*+
  Questa funzione aggiunge alla lista tutti i punti di connessione
  relativi all'entita' ent, nel caso si tratti di una polilinea,
  specificati dai flag di type: CONCT_START_END, CONCT_MIDDLE, CONCT_VERTEX, CONCT_POINT.
  Per gli altri tipi di entita' aggiunge solamente il punto di inserimento.
  La funzione verifica anche la possibilità di sovrapposizione con altri 
  oggetti GEOsim.
  Parametri:
  ads_name ent;       Entità a cui collegarsi
  int      type;      Flag composto per identificare il tipo di connessione
  int      Cls;       Codice classe dell'oggetto che si vuole 
                      inserire per eventuale controllo di sovrapposizione
                      (default = 0)
  int      Sub;       Codice sotto-classe dell'oggetto che si vuole 
                      inserire per eventuale controllo di sovrapposizione
                      (default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_POINT_LIST::add_connect_point_list(ads_name ent, int type, int Cls, int Sub)
{
   C_RB_LIST Descr;
   presbuf   rb;
   C_POINT   *p;
  
   if ((Descr << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   if (!(rb = Descr.SearchType(kDxfStart)))
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (gsc_strcmp(_T("POLYLINE"), rb->resval.rstring) == 0 ||
       gsc_strcmp(_T("LWPOLYLINE"), rb->resval.rstring) == 0)
   {
      if (type & CONCT_START_END)
         if (add_sten_point(ent) == GS_BAD) return GS_BAD;
      if (type & CONCT_MIDDLE)
         if (add_middle_points(ent) == GS_BAD) return GS_BAD;
      if (type & CONCT_VERTEX)
         if (add_vertex_point(ent) == GS_BAD) return GS_BAD;
      if (type & CONCT_CENTROID)
         if (add_centroid_point(ent) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(_T("TEXT"), rb->resval.rstring) == 0 ||
            gsc_strcmp(_T("MTEXT"), rb->resval.rstring) == 0 ||
            gsc_strcmp(_T("INSERT"), rb->resval.rstring) == 0)
   {
      if (add_ins_point(ent) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(_T("POINT"), rb->resval.rstring) == 0)
   {
      if (add_ins_point(ent) == GS_BAD) return GS_BAD;
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
  
   if (Cls > 0)
   {
      C_CLASS *p_class;

      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      if ((p_class = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub)) == NULL)
         return GS_BAD;

      p = (C_POINT *) get_head();
      while (p)
      {
         // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
         if (gsc_OverlapValidation(p->point, p_class) == GS_BAD)
         {
            remove_at();
            p = (C_POINT *) get_cursor();
         }
         else p = (C_POINT *) get_next();
      }
   }

  return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_ins_point <internal> */
/*+
  Questa funzione aggiunge il punto di inserimento dell'entità ent alla
  lista dei punti notevoli.
  Parametri:
  ads_name ent;      entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_ins_point(ads_name ent)
{
   ads_point point;

   if (ent == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (gsc_get_firstPoint(ent, point) == GS_BAD) return GS_BAD;
   if (add_point(point) == GS_BAD) return GS_BAD; 
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_vertex_point <internal> */
/*+
  Questa funzione aggiunge i punti corrispondenti ai vertici dell'entita' ent
  alla lista dei punti notevoli.
  Parametri:
  ads_name ent;            entità
  int IncludeFirstLast;    flag, se vuole includere il primo e l'ultimo vertice
                           (default = GS_BAD)
  double MaxTolerance2ApproxCurve;  Opzionale, Distanza massima ammessa tra la curva
                                    originale e i segmenti retti che la approssimano
                                    (usata per le ellissi)
  double MaxSegmentNum2ApproxCurve; Opzionale, Numero massimo di segmenti retti
                                    usati per approssimare una curva (usata per le ellissi)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_vertex_point(ads_name ent, int IncludeFirstLast,
                                   double MaxTolerance2ApproxCurve, int MaxSegmentNum2ApproxCurve)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (add_vertex_point((AcDbEntity *) pObj, IncludeFirstLast,
                        MaxTolerance2ApproxCurve, MaxSegmentNum2ApproxCurve) == GS_BAD)
      { pObj->close(); return GS_BAD; }

   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int C_POINT_LIST::add_vertex_point(AcDbEntity *pEnt, int IncludeFirstLast,
                                   double MaxTolerance2ApproxCurve, int MaxSegmentNum2ApproxCurve)
{
   C_POINT *pPt;

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();
      AcDbObjectId       vertexObjId;
      AcDb2dVertex       *pVertex;

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertici
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         {  // se NON è un punto di controllo della cornice di spline
            if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex)
               if (add_point(pVertex->position().x, pVertex->position().y, pVertex->position().z,
                             pVertex->bulge()) == GS_BAD)
                  { pVertex->close(); return GS_BAD; }

            pVertex->close();
         }
      }
      delete pVertIter;

      // Se la linea è chiusa aggiungo il vertice iniziale se esistente e non uguale a quello finale
      if (((AcDb2dPolyline *) pEnt)->isClosed() && (pPt = (C_POINT *) get_head()))
         if (*pPt != *((C_POINT *) get_tail()))
            if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      AcDbObjectIterator   *pVertIter = ((AcDb3dPolyline *) pEnt)->vertexIterator();
      AcDbObjectId         vertexObjId;
      AcDb3dPolylineVertex *pVertex;

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertici
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         {
            if (add_point(pVertex->position().x, pVertex->position().y, pVertex->position().z) == GS_BAD)
               { pVertex->close(); return GS_BAD; }

            pVertex->close();
         }
      }
      delete pVertIter;

      // Se la linea è chiusa aggiungo il vertice iniziale se esistente e non uguale a quello finale
      if (((AcDb3dPolyline *) pEnt)->isClosed() && (pPt = (C_POINT *) get_head()))
         if (*pPt != *((C_POINT *) get_tail()))
            if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point center, pt1, pt2;
      double    Angle, Bulge;

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), center);

      acutPolar(center, ((AcDbArc *) pEnt)->startAngle(), ((AcDbArc *) pEnt)->radius(), pt1);
      acutPolar(center, ((AcDbArc *) pEnt)->endAngle(), ((AcDbArc *) pEnt)->radius(), pt2);

      if (ads_point_equal(pt1, pt2)) // va considerato come un cerchio
      {
         AcDbCircle Circle;

         Circle.setCenter(((AcDbArc *) pEnt)->center());
         Circle.setRadius(((AcDbArc *) pEnt)->radius());
         return add_vertex_point(&Circle, IncludeFirstLast, MaxTolerance2ApproxCurve, MaxSegmentNum2ApproxCurve);
      }
      else // è un arco normale
      {
         // Calcolo l'angolo interno
         if (((AcDbArc *) pEnt)->endAngle() < ((AcDbArc *) pEnt)->startAngle())
            Angle = (((AcDbArc *) pEnt)->endAngle() + 2 * PI) - ((AcDbArc *) pEnt)->startAngle();
         else
            Angle = ((AcDbArc *) pEnt)->endAngle() - ((AcDbArc *) pEnt)->startAngle();

         Bulge = tan(Angle / 4);

         if (add_point(pt1, Bulge) == GS_BAD) return GS_BAD;
         if (add_point(pt2) == GS_BAD) return GS_BAD;
      }
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      AcGePoint3d Pt;
      int numVerts = ((AcDbLeader *) pEnt)->numVertices();

      for (int i = 0; i < numVerts; i++)
      {
         Pt = ((AcDbLeader *) pEnt)->vertexAt(i);
         if (add_point(Pt.x, Pt.y, Pt.z, 0.0) == GS_BAD)
            return GS_BAD;
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      if (add_point(((AcDbLine *) pEnt)->startPoint().x,
                    ((AcDbLine *) pEnt)->startPoint().y,
                    ((AcDbLine *) pEnt)->startPoint().z,
                    0.0) == GS_BAD)
         return GS_BAD;

      if (add_point(((AcDbLine *) pEnt)->endPoint().x,
                    ((AcDbLine *) pEnt)->endPoint().y,
                    ((AcDbLine *) pEnt)->endPoint().z,
                    0.0) == GS_BAD)
         return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d pt;
      double      Bulge;

      for (unsigned int i = 0; i < ((AcDbPolyline *) pEnt)->numVerts(); i++)
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, pt);
         ((AcDbPolyline *) pEnt)->getBulgeAt(i, Bulge);       

         if (add_point(pt.x, pt.y, pt.z, Bulge) == GS_BAD) return GS_BAD;
      }  

      // Se la linea è chiusa aggiungo il vertice iniziale se esistente e non uguale a quello finale
      if (((AcDbPolyline *) pEnt)->isClosed() && (pPt = (C_POINT *) get_head()))
         if (*pPt != *((C_POINT *) get_tail()))
            if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt;
      int         NumVerts = ((AcDbMline *) pEnt)->numVertices();

      for (int i = 0; i < NumVerts; i++)
      {
         pt = ((AcDbMline *) pEnt)->vertexAt(i);

         if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;
      }  

      // Se la linea è chiusa aggiungo il vertice iniziale se esistente
      if (((AcDbMline *) pEnt)->closedMline() && (pPt = (C_POINT *) get_head()))
         if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbHatch::desc()))
   {
      GS_ERR_COD = eGSInvEntityOp; 
      return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
   {
      AcGePoint3d pt, LastPt;

      ((AcDbSolid *) pEnt)->getPointAt(0, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      ((AcDbSolid *) pEnt)->getPointAt(1, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      ((AcDbSolid *) pEnt)->getPointAt(2, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      // Se il terzo punto è diverso dal quarto
      ((AcDbSolid *) pEnt)->getPointAt(3, LastPt);
      if (pt.isEqualTo(LastPt) == false)
      {
         ((AcDbSolid *) pEnt)->getPointAt(3, pt);
         if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;
      }

      // Chiudo la linea aggiungendo il vertice iniziale
      if ((pPt = (C_POINT *) get_head()))
         if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbFace::desc()))
   {
      AcGePoint3d pt, LastPt;

      ((AcDbFace *) pEnt)->getVertexAt(0, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      ((AcDbFace *) pEnt)->getVertexAt(1, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      ((AcDbFace *) pEnt)->getVertexAt(2, pt);
      if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;

      // Se il terzo punto è diverso dal quarto
      ((AcDbFace *) pEnt)->getVertexAt(3, LastPt);
      if (pt.isEqualTo(LastPt) == false)
      {
         ((AcDbFace *) pEnt)->getVertexAt(3, pt);
         if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;
      }

      // Chiudo la linea aggiungendo il vertice iniziale
      if ((pPt = (C_POINT *) get_head()))
         if (add_point(pPt->x(), pPt->y(), pPt->z()) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {  // Lo simulo con 2 archi
      ads_point pt1, pt2;
      double    Bulge;

      pt1[X] = ((AcDbCircle *) pEnt)->center().x + ((AcDbCircle *) pEnt)->radius();
      pt1[Y] = ((AcDbCircle *) pEnt)->center().y;
      pt1[Z] = ((AcDbCircle *) pEnt)->center().z;
      pt2[X] = ((AcDbCircle *) pEnt)->center().x - ((AcDbCircle *) pEnt)->radius();
      pt2[Y] = pt1[Y];
      pt2[Z] = pt1[Z];

      // Angolo interno = PI (180 gradi)
      Bulge = tan(PI / 4);

      if (add_point(pt1, Bulge) == GS_BAD) return GS_BAD;
      if (add_point(pt2, Bulge) == GS_BAD) return GS_BAD;
      if (add_point(pt1) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      double      EndParam, EllipseLength, Dist, OffSet, SamplePts, ArcRadius, dummy, SegmentLen;
      AcGePoint3d pt, pt1;
      
      ((AcDbEllipse *) pEnt)->getEndParam(EndParam);
      ((AcDbEllipse *) pEnt)->getDistAtParam(EndParam, EllipseLength);

      // primo punto
      ((AcDbEllipse *) pEnt)->getPointAtDist(0.0, pt1);
      if (add_point(pt1.x, pt1.y, pt1.z) == GS_BAD) return GS_BAD;

      // Considero un raggio = all'asse minore / 4 (approssimo)
      ArcRadius =  ((AcDbEllipse *) pEnt)->minorAxis().length() / 4;

      if (MaxTolerance2ApproxCurve >= ArcRadius)
         // almeno 6 punti (3 da una parte e 3 dall'altra dell'asse)
         SamplePts = 6;
      else
      {
         // Calcolo la lunghezza del segmento con pitagora
         dummy      = ArcRadius - MaxTolerance2ApproxCurve;
         SegmentLen = pow((ArcRadius * ArcRadius) - (dummy * dummy), (double) 1/2); // radice quadrata
         SegmentLen = SegmentLen * 2;     

         // calcolo quanti segmenti ci vogliono
         if ((SamplePts = ceil(EllipseLength / SegmentLen)) > MaxSegmentNum2ApproxCurve)
            SamplePts = MaxSegmentNum2ApproxCurve;
         else // almeno 6 punti (3 da una parte e 3 dall'altra dell'asse)
            if (SamplePts < 6) SamplePts = 6;
      }

      // punti successivi fino al penultimo compreso
      OffSet = EllipseLength / SamplePts;
      Dist   = OffSet;
      while (Dist < EllipseLength)
      {
         ((AcDbEllipse *) pEnt)->getPointAtDist(Dist, pt);
         if (add_point(pt.x, pt.y, pt.z) == GS_BAD) return GS_BAD;
         Dist += OffSet;
      }

      // ultimo punto = al primo punto
      if (add_point(pt1.x, pt1.y, pt1.z) == GS_BAD) return GS_BAD;
   }
   else
   if (pEnt->isKindOf(AcDbPolygonMesh::desc()))
   {
      AcDbObjectIterator    *pVertIter = ((AcDbPolygonMesh *) pEnt)->vertexIterator();
      AcDbObjectId          vertexObjId;
      AcDbPolygonMeshVertex *pVertex;

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertici
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         {
            if (add_point(pVertex->position().x, pVertex->position().y, pVertex->position().z) == GS_BAD)
               { pVertex->close(); return GS_BAD; }

            pVertex->close();
         }
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcDbCurve *pCurve = NULL;

      if (((AcDbSpline *) pEnt)->toPolyline(pCurve) != Acad::eOk || pCurve == NULL)
         { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
      if (add_vertex_point(pCurve, IncludeFirstLast, 
                           MaxTolerance2ApproxCurve, MaxSegmentNum2ApproxCurve) == GS_BAD)
         { delete pCurve; return GS_BAD; }
      delete pCurve;
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (IncludeFirstLast == GS_BAD) // Esclude punto iniziale e finale
   {
      remove_head();
      remove_tail();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_vertex_point <internal> */
/*+
  Questa funzione aggiunge la lista di punti contenuti in una lista di resbuf
  con la seguente struttura:
  1                             ; 1 = Polyline open, 0 = Closed
  (0 0 1)                       ; Direction of normal vector: (0 0 1) identifies the Z axis,
                                ; i.e., this polygon is parallel to the XY plane
  0.000000                      ; Bulge factor of a vertex
  (4.426217 7.991379 0.000000)  ; Coordinates of a vertex
  0.000000                      ; Bulge factor of a vertex
  (2.385054 5.530788 0.000000)  ; Coordinates of a vertex
  0.000000                      ; Bulge factor of a vertex
  (7.754200 3.823892 0.000000)  ; Coordinates of a vertex
  0.000000                      ; Bulge factor of a vertex
  (8.020439 3.646552 0.000000)) ; Coordinates of a vertex

  Parametri:
  presbuf *pRb;            Puntatore a lista di resbuf (viene spostato alla fine della lista)
  int IncludeFirstLast;    flag, se vuole includere il primo e l'ultimo vertice
                           (default = GS_BAD)
  double MaxTolerance2ApproxCurve;  Opzionale, Distanza massima ammessa tra la curva
                                    originale e i segmenti retti che la approssimano
                                    (usata per le ellissi)
  double MaxSegmentNum2ApproxCurve; Opzionale, Numero massimo di segmenti retti
                                    usati per approssimare una curva (usata per le ellissi)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_vertex_point(presbuf *pRb, int IncludeFirstLast,
                                   double MaxTolerance2ApproxCurve, int MaxSegmentNum2ApproxCurve)
{
   presbuf      p = *pRb;
   double       Bulge;
   int          ClosedPolyline;
   AcGePoint2d  PolygonPt;
   AcDbPolyline Polygon;

   // 1 = Polyline open, 0 = Polyline closed
   if (gsc_rb2Int(p, &ClosedPolyline) != GS_GOOD) return GS_BAD;

   // Direction of normal vector:
   // (0 0 1) identifies the Z axis, i.e., this polygon is parallel to the XY plane
   if (!(p = p->rbnext)) return GS_BAD;

   if (!(p = p->rbnext)) return GS_BAD;

   // Primo bulge
   if (p->restype == RTREAL)
   {
      gsc_rb2Dbl(p, &Bulge);
      p = p->rbnext;
   }
   else
      Bulge = 0;

   // Primo punto 
   if (p->restype != RTPOINT && p->restype != RT3DPOINT)
      return GS_BAD;

   PolygonPt.set(p->resval.rpoint[X], p->resval.rpoint[Y]);
   Polygon.addVertexAt(0, PolygonPt, Bulge); 

   if (ClosedPolyline == 0) Polygon.setClosed(kTrue);
   else Polygon.setClosed(kFalse);

   while ((p = p->rbnext) != NULL)
   {
      // bulge
      if (p->restype == RTREAL)
      {
         gsc_rb2Dbl(p, &Bulge);
         p = p->rbnext;
      }
      else
         Bulge = 0;

      // punto 
      if (p->restype != RTPOINT && p->restype != RT3DPOINT)
         break;

      PolygonPt.set(p->resval.rpoint[X], p->resval.rpoint[Y]);
      Polygon.addVertexAt(0, PolygonPt, Bulge); 
   }

   if (add_vertex_point(&Polygon, GS_GOOD,
                        MaxTolerance2ApproxCurve, MaxSegmentNum2ApproxCurve) != GS_GOOD)
      return GS_BAD;

   *pRb = p;

   return GS_GOOD;
}


int C_POINT_LIST::add_vertex_point(AcGePoint2dArray &vertices, AcGeDoubleArray &bulges)
{
   for (int i = 0; i < vertices.length(); i++)
      if (add_point(((AcGePoint2d) vertices[i]).x, ((AcGePoint2d) vertices[i]).y, 0.0,
                    bulges[i]) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int C_POINT_LIST::add_vertex_point_from_hatch(AcDbHatch *pHatch, int iLoop,
                                              double MaxTolerance2ApproxCurve, int MaxSegmentNum2ApproxCurve)
{
   Adesk::Int32 LoopType;
   int          Len;

   if (iLoop >= pHatch->numLoops()) return GS_BAD;

   LoopType = pHatch->loopTypeAt(iLoop);
   if (LoopType & AcDbHatch::kPolyline)
   {
      AcGePoint2dArray Vertices;
      AcGeDoubleArray  Bulges;
      int              BulgesLen;

      if (pHatch->getLoopAt(iLoop, LoopType, Vertices, Bulges) != Acad::eOk) return GS_BAD;
      Len       = Vertices.length();
      BulgesLen = Bulges.length();
      for (int i = 0; i < Len; i++)
         if (add_point(((AcGePoint2d) Vertices[i]).x, 
                       ((AcGePoint2d) Vertices[i]).y,
                       0.0, 
                       (BulgesLen == 0) ? 0.0 : Bulges[i]) == GS_BAD) return GS_BAD;
   }
   else
   {
      AcGeVoidPointerArray edgePtrs;
      AcGeIntArray         edgeTypes;
      const AcGeCurve2d    *Curve;
      C_POINT              *pFirstPt = NULL;

      if (pHatch->getLoopAt(iLoop, LoopType, edgePtrs, edgeTypes) != Acad::eOk) return GS_BAD;

      Len = edgePtrs.length();
      for (int i = 0; i < Len; i++)
      {
         Curve = static_cast<AcGeCurve2d*>(edgePtrs[i]);

         if (Curve->isKindOf(AcGe::kLineSeg2d))
         {
            const AcGeLineSeg2d *line = static_cast<const AcGeLineSeg2d*>(Curve);

            if (add_point(line->startPoint().x, line->startPoint().y, 0.0) == GS_BAD) return GS_BAD;
            if (i == 0) pFirstPt = (C_POINT *) get_cursor();

            if (i == (Len - 1)) // Se è l'ultimo punto dell'anello
               if (add_point(line->endPoint().x, line->endPoint().y, 0.0) == GS_BAD) return GS_BAD;
          }
          else if (Curve->isKindOf(AcGe::kCircArc2d))
          {
            const AcGeCircArc2d* arc = static_cast<const AcGeCircArc2d*>(Curve);

            if (arc->isClosed()) // Cerchio
            {  // Lo simulo con 2 archi
               ads_point pt1, pt2;
               double    Bulge;

               pt1[X] = arc->center().x + arc->radius();
               pt1[Y] = arc->center().y;
               pt1[Z] = 0.0;
               pt2[X] = arc->center().x - arc->radius();
               pt2[Y] = pt1[Y];
               pt2[Z] = pt1[Z];

               // Angolo interno = PI (180 gradi)
               Bulge = tan(PI / 4);

               if (add_point(pt1, Bulge) == GS_BAD) return GS_BAD;
               if (i == 0) pFirstPt = (C_POINT *) get_cursor();
               if (add_point(pt2, Bulge) == GS_BAD) return GS_BAD;
               if (add_point(pt1) == GS_BAD) return GS_BAD;
            }
            else // arco
            {
               ads_point center, pt1, pt2;
               double    Angle, Bulge;

               ads_point_set_from_AcGePoint2d(arc->center(), center);

               acutPolar(center, arc->startAng(), arc->radius(), pt1);
               acutPolar(center, arc->endAng(), arc->radius(), pt2);

               // Calcolo l'angolo interno
               if (arc->endAng() < arc->startAng())
                  Angle = (arc->endAng() + 2 * PI) - arc->startAng();
               else
                  Angle = arc->endAng() - arc->startAng();

               Bulge = tan(Angle / 4);

               if (add_point(pt1, Bulge) == GS_BAD) return GS_BAD;
               if (i == 0) pFirstPt = (C_POINT *) get_cursor();

               if (i == (Len - 1)) // Se è l'ultimo punto dell'anello
                  if (add_point(pt2) == GS_BAD) return GS_BAD;
            }
         }
         else if (Curve->isKindOf(AcGe::kSplineEnt2d))
         {
            const AcGeSplineEnt2d* spline = static_cast<const AcGeSplineEnt2d*>(Curve);

            for (int j=0; j < spline->numControlPoints(); j++)
            {
               if (add_point(spline->controlPointAt(i).x, 
                             spline->controlPointAt(i).y, 
                             0.0) == GS_BAD) return GS_BAD;
               if (i == 0) pFirstPt = (C_POINT *) get_cursor();
            }
            if (!spline->isClosed()) // se la spline non è chiusa
               if (i == (Len - 1)) // Se NOn è l'ultimo punto dell'anello
                  remove_tail(); // tolgo l'ultimo punto
         }
         else if (Curve->isKindOf(AcGe::kEllipArc2d))
         {
            const AcGeEllipArc2d* ellip_arc = static_cast<const AcGeEllipArc2d*>(Curve);

            AcGePoint3d  Center(ellip_arc->center().x, ellip_arc->center().y, 0.0);
            // Il vettore majorAxis è con lunghezza 1 quindi devo ricalcolare le
            // lunghezze reali
            AcGeVector3d majorAxis(ellip_arc->majorAxis().x * ellip_arc->majorRadius(),
                                   ellip_arc->majorAxis().y * ellip_arc->majorRadius(), 0.0);

            AcDbEllipse Ellipse(Center, AcGeVector3d(0.0, 0.0, 1.0), majorAxis, 
                                ellip_arc->minorRadius() / ellip_arc->majorRadius(),
                                ellip_arc->startAng(), ellip_arc->endAng());
            add_vertex_point(&Ellipse, GS_GOOD, MaxTolerance2ApproxCurve, MaxSegmentNum2ApproxCurve);
         }
         else
            return GS_BAD;
      }

      if (pFirstPt)
         if (!ads_point_equal(pFirstPt->point, ((C_POINT *) get_cursor())->point))
            add_point(pFirstPt->point); // aggiungo punto iniziale
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_sten_point <internal> */
/*+
  Questa funzione aggiunge i punti di inizio e fine dell'entita' ent alla
  lista dei punti notevoli.
  Parametri:
  ads_name ent;      entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_sten_point(ads_name ent)
{
   ads_point pnt;

   if (gsc_get_firstPoint(ent, pnt) == GS_BAD) return GS_BAD;
   if (add_point(pnt) == GS_BAD) return GS_BAD; 
   if (gsc_get_lastPoint(ent, pnt) == GS_BAD) return GS_BAD;
   if (add_point(pnt) == GS_BAD) return GS_BAD; 
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_middle_points <internal> */
/*+
  Questa funzione aggiunge i punti medi delle varie tratte dell'entita' ent
  alla lista dei punti notevoli.
  Parametri:
  ads_name ent;      entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_middle_points(ads_name ent)
{
   resbuf *rb, *head;

   if ((head = rb = gsc_getMiddlePts(ent)) == NULL)
      { GS_ERR_COD = 1; return GS_BAD; }
   
   do
   {
      if (rb->restype != RT3DPOINT)
         { GS_ERR_COD = eGSInvRBType; acutRelRb(head); return GS_BAD; }
      if (add_point(rb->resval.rpoint) == GS_BAD) 
         { acutRelRb(head); return GS_BAD; }
      }
   while((rb = rb->rbnext) != NULL);
   acutRelRb(head);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::add_centroid_point <internal> */
/*+
  Questa funzione aggiunge il punto centroide della superficie ent alla
  lista dei punti notevoli.
  Parametri:
  ads_name ent;      entità superficie
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_POINT_LIST::add_centroid_point(ads_name ent)
{
   ads_point point;

   if (gsc_get_centroidpoint(ent, point) == GS_BAD) return GS_BAD;
   if (add_point(point) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_POINT_LIST::search                   <internal> */
/*+
  Questa funzione cerca un punto nella lista.
  Parametri:
  ads_point _point;     punto noto
  double Tolerance;     tolleranza

  Ritorna il puntatore del punto in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
C_POINT *C_POINT_LIST::search(ads_point _point, double Tolerance)
{
   C_POINT *p;

   p = (C_POINT *) get_head();
   while (p)
   {
      if (((p->point[0] > _point[0]) ? p->point[0] - _point[0] : _point[0] - p->point[0]) <= Tolerance && 
          ((p->point[1] > _point[1]) ? p->point[1] - _point[1] : _point[1] - p->point[1]) <= Tolerance && 
          ((p->point[2] > _point[2]) ? p->point[2] - _point[2] : _point[2] - p->point[2]) <= Tolerance)
         break;
      p = (C_POINT *) p->get_next();
   }
   
   return p;
}


/*********************************************************/
/*.doc C_POINT_LIST::get_nearest              <internal> */
/*+
  Questa funzione ritorna il puntatore corrispondente al punto 
  piu' vicino ad un punto noto.
  Parametri:
  ads_point center;      punto noto
-*/  
/*********************************************************/
C_POINT *C_POINT_LIST::get_nearest(ads_point center)
{
   C_POINT *punt, *nearp = NULL;
   double  dist, min = -1;

   punt = (C_POINT *) get_head();
   while (punt)
   {
      if ((dist = gsc_dist(center, punt->point)) < min || min == -1)
      {
         min = dist;
         nearp = punt;
      }
      punt = (C_POINT *) punt->get_next();
   }
   
   return nearp;
}


/*********************************************************/
/*.doc C_POINT_LIST::to_rb                    <internal> */
/*+
  Questa funzione scrive una lista di resbuf contenente i punti.
  Parametri:
  bool bulge;     Flag, se = true significa che la lista sarà composta da
                  bulge seguito da punto (bulge1 punto1 bulg2 punto2 ...),
                  altrimenti sarà una lista di soli punti 
  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
resbuf* C_POINT_LIST::to_rb(bool bulge)
{
   presbuf list=NULL, prb;
   C_POINT *punt;

   punt = (C_POINT *) get_tail();
   if (!punt)
      if ((list = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   while (punt)
   {                                
      if (bulge)
      {
         if ((prb = acutBuildList(RTREAL, punt->Bulge, RTPOINT, punt->point, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
         prb->rbnext->rbnext = list;
      }
      else
      {
         if ((prb = acutBuildList(RTPOINT, punt->point, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
         prb->rbnext = list;
      }

      list        = prb;
      punt        = (C_POINT *) get_prev();
   }
   
   return list;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::add_intersectWith                               */
/*+
  La funzione calcola i punti di intersezione tra 2 oggetti grafici.
  Parametri:
  ads_name ent1;
  ads_name ent2;
  int Dec;          Opzionale, se diverso da >=0 le coordinate vengono 
                    troncate al n di decimali specificato (default = -1)

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/
/*************************************************************************/
int C_POINT_LIST::add_intersectWith(ads_name ent1, ads_name ent2, int Dec)
{
   AcDbObject   *pObj1, *pObj2;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent1) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj1, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (acdbGetObjectId(objId, ent2) != Acad::eOk)
      { pObj1->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj2, objId, AcDb::kForRead) != Acad::eOk)
      { pObj1->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (add_intersectWith((AcDbEntity *) pObj1, (AcDbEntity *) pObj2, Dec) == GS_BAD)
      { pObj1->close(); pObj2->close(); return GS_BAD; }

   if (pObj1->close() != Acad::eOk) { pObj2->close(); return GS_BAD; }
   if (pObj2->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int C_POINT_LIST::add_intersectWith(AcDbEntity *pEnt1, AcDbEntity *pEnt2, int Dec)
{
   AcGePoint3dArray Pts;
   int              len, i;
   ads_point        Ent1Corner1, Ent1Corner2, Ent2Corner1, Ent2Corner2;
   double           OffSetX, OffSetY;
   AcDbEntity       *pInternalEnt1, *pInternalEnt2;

   // se le coordinate delle entità sono molto grandi la funzione intersectWith
   // non lavora correttamente quindi devo traslare gli oggetti il più vicino
   // possibile all'origine, calcolare le intersezioni, e successivamente traslare
   // i punti di intersezione per riportarli alle coordinate originali
   if (gsc_get_ent_window(pEnt1, Ent1Corner1, Ent1Corner2) != GS_GOOD ||
       gsc_get_ent_window(pEnt2, Ent2Corner1, Ent2Corner2) != GS_GOOD)
      return GS_BAD;
   OffSetX = (min(Ent1Corner1[X], Ent2Corner1[X]) + max(Ent1Corner2[X], Ent2Corner2[X])) / 2;
   OffSetY = (min(Ent1Corner1[Y], Ent2Corner1[Y]) + max(Ent1Corner2[Y], Ent2Corner2[Y])) / 2;
   // Creo entità copia
   if ((pInternalEnt1 = (AcDbEntity *) pEnt1->isA()->create()) == NULL)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if ((pInternalEnt2 = (AcDbEntity *) pEnt2->isA()->create()) == NULL)
      { delete pInternalEnt1; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (pInternalEnt1->copyFrom(pEnt1) != Acad::eOk ||
       pInternalEnt2->copyFrom(pEnt2) != Acad::eOk)
   {
      gsc_ReleaseSubEnts(pInternalEnt1); delete pInternalEnt1;
      gsc_ReleaseSubEnts(pInternalEnt2); delete pInternalEnt2;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   // Traslo le entità
   if (gsc_MoveEntOnXY(pInternalEnt1, -1 * OffSetX, -1 * OffSetY, Dec) != GS_GOOD ||
       gsc_MoveEntOnXY(pInternalEnt2, -1 * OffSetX, -1 * OffSetY, Dec) != GS_GOOD)
   {
      gsc_ReleaseSubEnts(pInternalEnt1); delete pInternalEnt1;
      gsc_ReleaseSubEnts(pInternalEnt2); delete pInternalEnt2;
      return GS_BAD;
   }

   // Provo prima a valutare le intersezione di ent1 con ent2
   if (pInternalEnt1->intersectWith(pInternalEnt2, AcDb::kOnBothOperands, Pts) != Acad::eOk)
      // se fallisce o se non trova niente 
      // provo a valutare le intersezione di ent2 con ent1
      // (forse per ent1 è stata implementata la funzione "intersectWith")
      if (pInternalEnt2->intersectWith(pInternalEnt1, AcDb::kOnBothOperands, Pts) != Acad::eOk)
      {
         gsc_ReleaseSubEnts(pInternalEnt1); delete pInternalEnt1;
         gsc_ReleaseSubEnts(pInternalEnt2); delete pInternalEnt2;
         GS_ERR_COD = eGSInvGraphObjct;
         return GS_BAD;
      }

   if ((len = Pts.length()) == 0 && 
       gsc_isLinearEntity(pInternalEnt2) && gsc_isLinearEntity(pInternalEnt1))
   {
      // se 2 linee hanno la stessa pendenza e si toccano con i punti finali
      // la funzione intersectWith non trova l'intersezione
      ads_point ptInit1, ptInit2, ptFinal1, ptFinal2;
      AcGePoint3d pt;

      if (gsc_get_firstPoint(pInternalEnt1, ptInit1) == GS_GOOD &&
          gsc_get_firstPoint(pInternalEnt2, ptInit2) == GS_GOOD &&
          gsc_get_lastPoint(pInternalEnt1, ptFinal1) == GS_GOOD &&
          gsc_get_lastPoint(pInternalEnt2, ptFinal2) == GS_GOOD)
      {
         if (gsc_point_equal(ptInit1, ptInit2))
         {
            AcGePoint3d_set_from_ads_point(ptInit1, pt);
            Pts.append(pt);
         }
         else
            if (gsc_point_equal(ptInit1, ptFinal2))
            {
               AcGePoint3d_set_from_ads_point(ptInit1, pt);
               Pts.append(pt);
            }

         if (gsc_point_equal(ptFinal1, ptInit2))
         {
            AcGePoint3d_set_from_ads_point(ptFinal1, pt);
            Pts.append(pt);
         }
         else
            if (gsc_point_equal(ptFinal1, ptFinal2))
            {
               AcGePoint3d_set_from_ads_point(ptFinal1, pt);
               Pts.append(pt);
            }

         len = Pts.length();
      }
   }

   gsc_ReleaseSubEnts(pInternalEnt1); delete pInternalEnt1;
   gsc_ReleaseSubEnts(pInternalEnt2); delete pInternalEnt2;

   for (i = 0; i < len; i++)
      // Sommo l'OffSet per gli assi X e Y
      if (add_point(Pts[i].x + OffSetX, Pts[i].y + OffSetY, Pts[i].z) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::add_intersectWithArc                            */
/*+
  La funzione calcola i punti di intersezione tra 1 oggetto grafico e un arco.
  Parametri:
  ads_name ent;
  ads_point center;  Centro dell'arco
  double radius;     Raggio Arco
  double startAngle; Angolo iniziale
  double endAngle;   Angolo finale
  int Dec;           Opzionale, se diverso da >=0 le coordinate vengono 
                     troncate al n di decimali specificato (default = -1)

  oppure

  AcDbEntity *pEnt;
  ads_point center;  Centro dell'arco
  double radius;     Raggio Arco
  double startAngle; Angolo iniziale
  double endAngle;   Angolo finale
  int Dec;           Opzionale, se diverso da >=0 le coordinate vengono 
                     troncate al n di decimali specificato (default = -1)

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/
/*************************************************************************/
int C_POINT_LIST::add_intersectWithArc(ads_name ent, ads_point center,
                                       double radius, double startAngle, double endAngle, int Dec)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (add_intersectWithArc((AcDbEntity *) pObj, center, radius, startAngle, endAngle, Dec) == GS_BAD)
      { pObj->close(); return GS_BAD; }

   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int C_POINT_LIST::add_intersectWithArc(AcDbEntity *pEnt, ads_point center,
                                       double radius, double startAngle, double endAngle, int Dec)
{
   AcGePoint3d pt(center[X], center[Y], center[Z]);

   AcDbArc Arc(pt, radius, startAngle, endAngle);

   if (add_intersectWith(pEnt, &Arc, Dec) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::add_intersectWithLine                           */
/*+
  La funzione calcola i punti di intersezione tra 1 oggetto grafico e una linea.
  Parametri:
  ads_name ent1;
  ads_point startPt; Punto iniziale
  ads_point endPt2;  Punto finale
  int Dec;           Opzionale, se diverso da >=0 le coordinate vengono 
                     troncate al n di decimali specificato (default = -1)

  oppure

  ads_name ent1;
  ads_point startPt; Punto iniziale
  ads_point endPt2;  Punto finale
  int Dec;           Opzionale, se diverso da >=0 le coordinate vengono 
                     troncate al n di decimali specificato (default = -1)

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/
/*************************************************************************/
int C_POINT_LIST::add_intersectWithLine(ads_name ent, ads_point startPt, ads_point endPt, int Dec)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (add_intersectWithLine((AcDbEntity *) pObj, startPt, endPt, Dec) == GS_BAD)
      { pObj->close(); return GS_BAD; }

   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int C_POINT_LIST::add_intersectWithLine(AcDbEntity *pEnt, ads_point startPt, ads_point endPt, int Dec)
{
   AcGePoint3d  Pt1(startPt[X], startPt[Y], startPt[Z]);
   AcGePoint3d  Pt2(endPt[X], endPt[Y], endPt[Z]);
   AcDbLine Line(Pt1, Pt2);

   if (add_intersectWith(pEnt, &Line, Dec) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::getElevation                                    */
/*+
  La funzione legge l'elevazione comune a tutti i punti della lista (Z).
  Parametri:
  double *Elev;

  Qualora i punti abbiano valori Z diversi la funzione ritorna GS_CAN
  altrimenti GS_GOOD.
-*/
/*************************************************************************/
int C_POINT_LIST::getElevation(double *Elev)
{
   C_POINT *pPoint;

   pPoint = (C_POINT *) get_head();
   if (!pPoint) return GS_CAN;
   *Elev = pPoint->point[Z];
   pPoint = (C_POINT *) get_next();
   while (pPoint)
   {
      if (*Elev != pPoint->point[Z]) return GS_CAN;

      pPoint = (C_POINT *) get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::getLineLen                                      */
/*+
  La funzione calcola la lunghezza della linea risultante dalla lista dei vertici.
  Parametri:

  Restituisce la lunghezza in caso di successo altrimenti restituisce -1. 
-*/
/*************************************************************************/
double C_POINT_LIST::getLineLen(void)
{
   C_POINT   *pPoint;
   ads_point Pt1;
   double    LineLen = 0, Bulge;

   if ((pPoint = (C_POINT *) get_head()) == NULL) return -1;
   
   ads_point_set(pPoint->point, Pt1);
   Bulge = pPoint->Bulge;

   pPoint = (C_POINT *) get_next();
   while (pPoint)
   {
      if (Bulge == 0)
         LineLen += gsc_dist(Pt1, pPoint->point);
      else
      {
         double    ArcAngle, Radius;
         ads_point Center;

         ArcAngle = atan(Bulge) * 4;
         gsc_getArcRayCenter(Pt1, pPoint->point, ArcAngle, &Radius, Center);
         LineLen += gsc_getArcLen(Radius, ArcAngle);
      }

      ads_point_set(pPoint->point, Pt1);
      Bulge = pPoint->Bulge;
      pPoint = (C_POINT *) get_next();
   }

   return LineLen;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::getWindow                                       */
/*+
  La funzione ricava la finestra spaziale contenente tutti i punti.
  Parametri:
  ads_point ptBottomLeft;  Punto in basso a sinistra
  ads_point ptTopRight;    Punto in altro a destra

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT_LIST::getWindow(ads_point ptBottomLeft, ads_point ptTopRight)
{
   C_POINT *pPoint = (C_POINT *) get_head();

   if (!pPoint) return GS_BAD;

   ads_point_set(pPoint->point, ptBottomLeft);
   ads_point_set(pPoint->point, ptTopRight);

   pPoint = (C_POINT *) get_next();
   while (pPoint)
   {
      if (pPoint->point[X] > ptTopRight[X]) ptTopRight[X] = pPoint->point[X];
      if (pPoint->point[Y] > ptTopRight[Y]) ptTopRight[Y] = pPoint->point[Y];
      if (pPoint->point[X] < ptBottomLeft[X]) ptBottomLeft[X] = pPoint->point[X];
      if (pPoint->point[Y] < ptBottomLeft[Y]) ptBottomLeft[Y] = pPoint->point[Y];

      pPoint = (C_POINT *) get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::fromStr                                       */
/*+
  La funzione inizializza una lista di punti da una stringa espressa nella forma
  "X1,Y1[,Z1[,bulge1]],X2,Y2[,Z2[,bulge2]]". Tra quadre ci sono i valori opzionali.
  Parametri:
  const TCHAR *Str; stringa da elaborare
  GeomDimensionEnum geom_dim; Opzionale; Dato geometrico 2D o 3D (default = 2D)
  bool BulgeWith;             Opzionale; Esistenza del bulge tra i punti 
                              (default = false)
  TCHAR PtSep;                Opzionale, Carattere separatore tra punti
                              (default = ';')
  TCHAR CoordSep;             Opzionale, Carattere separatore tra coordinate
                              (default = ',')
  int *nRead;                 Opzionale, ritorna il numero di caratteri letti (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT_LIST::fromStr(const TCHAR *Str, GeomDimensionEnum geom_dim,
                          bool BulgeWith, TCHAR PtSep, TCHAR CoordSep, int *nRead)
{
   C_STRING Buffer(Str);
   TCHAR    *p;
   int      nPts, i, nPartialRead;
   C_POINT  *pPt;

   remove_all();
   if (nRead) *nRead = 0;
   p      = Buffer.get_name();
   nPts = gsc_strsep(p, _T('\0'), PtSep);

   for (i = 0; i <= nPts; i++)
   {
      if ((pPt = new C_POINT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      if (pPt->fromStr(p, CoordSep, &nPartialRead) == GS_BAD) return GS_BAD;
      // Se dato in 2D con bulge allora fromStr ha memorizzato il bulge nella Z
      // perchè crede sia nel formato x,y,z
      if (geom_dim == GS_2D && BulgeWith)
      {
         pPt->Bulge    = pPt->z();
         pPt->point[Z] = 0.0;
      }
      add_tail(pPt);

      if (i < nPts)
         p += (nPartialRead + 1); // vado al punto successivo
      if (nRead)
         *nRead += nPartialRead;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::toStr                                           */
/*+
  La funzione inizializza una stringa contenente una lista di punti 
  "X1,Y1[,Z1[,bulge1]] X2,Y2[,Z2[,bulge2]]" se BulgeWith ) true
  Parametri:
  C_STRING &Str;              stringa di output
  GeomDimensionEnum geom_dim; Opzionale; Dato geometrico 2D o 3D (default = 2D)
  bool BulgeWith;             Opzionale; Se true scrive il bulge dopo ogni coordinata
                              altrimenti se viene incontrato un arco questo viene spezzato
                              in segmenti (default = false)
  TCHAR PtSep;                Opzionale, Carattere separatore tra punti
                              (default = ';')
  TCHAR CoordSep;             Opzionale, Carattere separatore tra coordinate
                              (default = ',')
  double MaxTolerance2ApproxCurve;  Opzionale, Distanza massima ammessa tra la curva
                                    originale e i segmenti retti che la approssimano
  double MaxSegmentNum2ApproxCurve; Opzionale, Numero massimo di segmenti retti
                                    usati per approssimare una curva

  Ritorna il numero di punti che ha elaborato (compresi quelli interpolati)
  in caso di successo altrimenti -1.
-*/
/*************************************************************************/
int C_POINT_LIST::toStr(C_STRING &Str, GeomDimensionEnum geom_dim,
                        bool BulgeWith, TCHAR PtSep, TCHAR CoordSep,
                        double MaxTolerance2ApproxCurve, int MaxSegmentNum2ApproxCurve)
{
   C_STRING    Buffer;
   C_POINT     *pPt = (C_POINT *) get_head(), Pt;
   double      Bulge;
   AcGePoint2d StartPoint, EndPoint;
   int         nPts = 0;

   Str.clear();
   while (pPt)
   {
      pPt->toStr(Buffer, geom_dim, CoordSep);
      Str += Buffer;
      nPts++;

      if (BulgeWith)
      {
         Str += CoordSep;
         Str += pPt->Bulge;
      }
      else if ((Bulge = pPt->Bulge) != 0)
         AcGePoint2d_set_from_ads_point(pPt->point, StartPoint);

      if ((pPt = (C_POINT *) get_next()) != NULL) // se esiste il punto successivo
      {
         if (!BulgeWith && Bulge != 0)
         {
            int              Point2dArrayLen, SamplePts;
      	   AcGePoint2dArray Point2dArray;
            double           ArcLength, ArcRadius, dummy, SegmentLen;

            AcGePoint2d_set_from_ads_point(pPt->point, EndPoint);

            AcGeCircArc2d Arc2d(StartPoint, EndPoint, Bulge, Adesk::kFalse);

            // Calcolo la lunghezza dell'arco
            ArcLength = Arc2d.length(Arc2d.paramOf(StartPoint), Arc2d.paramOf(EndPoint));

            ArcRadius  = Arc2d.radius();

            if (MaxTolerance2ApproxCurve >= ArcRadius)
               // almeno 3
               SamplePts = 3;
            else
            {
               // Calcolo la lunghezza del segmento con pitagora
               dummy      = ArcRadius - MaxTolerance2ApproxCurve;
               SegmentLen = pow((ArcRadius * ArcRadius) - (dummy * dummy), (double) 1/2); // radice quadrata
               SegmentLen = SegmentLen * 2;

               // calcolo quanti segmenti ci vogliono
               if ((dummy = ceil(ArcLength / SegmentLen)) > MaxSegmentNum2ApproxCurve)
                  SamplePts = MaxSegmentNum2ApproxCurve;
               else // almeno 3 
                  SamplePts = (dummy < 3) ? 3 : (int) dummy;
            }

            SamplePts++; // il numero dei punti è il numero dei segmenti + 1

            // approssimazione di un arco in n segmenti aventi 
            // lunghezza costante = MAX_SEGMENTLEN_TO_APPROX_CURVE
			   Arc2d.getSamplePoints(SamplePts, Point2dArray);

            // il ciclo salta il primo e l'ultimo punto
            Point2dArrayLen = Point2dArray.length() - 1;
			   for (int i = 1; i < Point2dArrayLen; i++)
			   {
				   Pt.Set(Point2dArray.at(i).x, Point2dArray.at(i).y);

               Str += PtSep; // Aggiungo separatore tra punti
               Pt.toStr(Buffer, geom_dim, CoordSep);
               Str += Buffer;
               nPts++;
			   }
         }

         Str += PtSep; // Aggiungo separatore tra punti
      }
   }

   return nPts;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::toAcGeArray                                     */
/*+
  La funzione converte la lista dei punti e dei bluge in un array di punti 2d
  e un array di double.
  Parametri:
  AcGePoint2dArray &vertices; Array di punti 2D
  AcGeDoubleArray &bulges;    Array di double

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT_LIST::toAcGeArray(AcGePoint2dArray &vertices, AcGeDoubleArray &bulges)
{
   C_POINT     *pPt = (C_POINT *) get_head();
   AcGePoint2d Pt;

   vertices.setLogicalLength(0);
   bulges.setLogicalLength(0);
   while (pPt)
   {
      AcGePoint2d_set_from_ads_point(pPt->point, Pt);
      vertices.append(Pt);
      bulges.append(pPt->Bulge);
      
      pPt = (C_POINT *) get_next();
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::toPolyline                                      */
/*+
  La funzione converte la lista dei punti in un oggetto polilinea (2D o 3D a seconda 
  delle coordinate) senza inserirlo nel DB grafico di AutoCAD.
  Parametri:
  const TCHAR *Layer;       (default = NULL)
  C_COLOR *Color;           (default = NULL)
  const TCHAR *LineType;    (default = NULL)
  double Scale;             (default -1)
  double Width;             (default -1)
  double Thickness;         (default -1)
  bool *Plinegen;           (default = NULL), usato solo per linee 2D

  Restituisce il puntatore all'oggetto grafico non ancora chiuso
  in caso di successo altrimenti restituisce NULL. 
-*/
/*************************************************************************/
AcDbCurve *C_POINT_LIST::toPolyline(const TCHAR *Layer, C_COLOR *Color,
                                    const TCHAR *LineType, double Scale, double Width, 
                                    double Thickness, bool *Plinegen)
{
   double  Elev;
   C_POINT *pPoint;

   if (get_count() < 2) return GS_BAD;

   if (getElevation(&Elev) == GS_CAN) // i punti hanno elevazioni diverse
   {
      AcGePoint3dArray Vertices;
      AcGePoint3d      pt;

      pPoint = (C_POINT *) get_head();
      while (pPoint)
      {
         pt.set(pPoint->point[X], pPoint->point[Y], pPoint->point[Z]);
         Vertices.append(pt);

         pPoint = (C_POINT *) get_next();
      }

      // Devo costruire una 3D polyline semplice non chiusa
      AcDb3dPolyline *pLine = new AcDb3dPolyline(AcDb::k3dSimplePoly,
                                                 Vertices, Adesk::kFalse);
      if (Layer)
         if (gsc_setLayer(pLine, Layer) != GS_GOOD)
            { delete pLine; return NULL; }

      if (Color)
         if (gsc_set_color(pLine, *Color) != GS_GOOD)
            { delete pLine; return NULL; }

      if (LineType)
      {
         if (gsc_set_lineType(pLine, LineType)!= GS_GOOD)
            { delete pLine; return NULL; }
         if (Scale > 0) pLine->setLinetypeScale(Scale);
      }

      return pLine;
   }
   else // Devo costruire una lwpolyline
   {
      AcDbPolyline *pLine = new AcDbPolyline(get_count());
      AcGePoint2d  pt;
      int          i = 0;

      pPoint = (C_POINT *) get_head();
      while (pPoint)
      {
         pt.set(pPoint->point[X], pPoint->point[Y]);
         pLine->addVertexAt(i++, pt, pPoint->Bulge);

         pPoint = (C_POINT *) get_next();
      }

      if (Layer)
         if (gsc_setLayer(pLine, Layer) != GS_GOOD)
            { delete pLine; return NULL; }

      if (Color)
         if (gsc_set_color(pLine, *Color) != GS_GOOD)
            { delete pLine; return NULL; }

      pLine->setElevation(Elev);

      if (LineType)
      {
         if (gsc_set_lineType(pLine, LineType)!= GS_GOOD)
            { delete pLine; return NULL; }
         if (Scale > 0) pLine->setLinetypeScale(Scale);
      }

      if (Width >= 0) pLine->setConstantWidth(Width);

      if (Thickness >= 0) pLine->setThickness(Thickness);

      if (Plinegen) pLine->setPlinegen(*Plinegen);

      return pLine;
   }

   return NULL;
}


/*************************************************************************/
/*.doc int C_POINT_LIST::translation                                     */
/*+
  La funzione effettua una traslazione in X, Y e Z dei punti della lista.
  Parametri:
  double OffSet_X;   Offset di traslazione X
  double OffSet_Y;   Offset di traslazione Y
  double OffSet_Z;   Offset di traslazione Z (default = 0)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
void C_POINT_LIST::translation(double OffSet_X, double OffSet_Y, double OffSet_Z)
{
   C_POINT *pPt = (C_POINT *) get_head();

   while (pPt)
   {
      pPt->Set(pPt->x() + OffSet_X, pPt->y() + OffSet_Y, pPt->z() + OffSet_Z);    
      pPt = (C_POINT *) get_next();
   }
}


/*************************************************************************/
/*.doc int C_POINT_LIST::OffSet                                          */
/*+
  La funzione effettua l'offset dei punti della lista.
  Parametri:
  double in;   valore di Offset

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT_LIST::OffSet(double in)
{
   AcDbCurve    *pEnt;
   double       Dist, Rot;
   AcGePoint3d  ReferencePt;
   C_POINT      *pPt;
   ads_point    NewPt;
   C_POINT_LIST NewPtList;

    // Creo la polilinea
   if (!(pEnt = toPolyline())) return GS_BAD;

   pPt = (C_POINT *) get_head();
   while (pPt)
   {
      AcGePoint3d_set_from_ads_point(pPt->point, ReferencePt);
      if (pEnt->getDistAtPoint(ReferencePt, Dist) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; delete pEnt; return GS_BAD; }
      if (gsc_getPntRtzOnObj(pEnt, NewPt, &Rot, _T("S"), Dist, in) == GS_BAD)
         { delete pEnt; return GS_BAD; }
      NewPtList.add_point(NewPt, pPt->Bulge);

      pPt = (C_POINT *) get_next();
   }
   delete pEnt;
   
   return NewPtList.copy(*this);
}


/*************************************************************************/
/*.doc int C_POINT_LIST::Draw                                            */
/*+
  La funzione disegna la lista dei punti.
  Parametri:
  const TCHAR *Layer; Nome del piano
  C_COLOR *Color;     Codice colore (se = NULL -> "colore corrente")
  int Mode;           Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                      (= EXTRACTION) crea oggetti grafici (default = EXTRACTION)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_POINT_LIST::Draw(const TCHAR *Layer, C_COLOR *Color, int Mode)
{
   C_POINT *pPoint = (C_POINT *) get_head();

   // Se non esiste il layer lo creo
   if (Layer) if (gsc_crea_layer(Layer) == GS_BAD) return GS_BAD;

   while (pPoint)
   {
      // Visualizza punto
      if (Mode == PREVIEW)
      {
         int AutoCADColorIndex = COLOR_BYLAYER;

         if (Color)
            if (Color->getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

         if (acedGrDraw(pPoint->point, pPoint->point, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
      }
      else
         if (gsc_insert_point(pPoint->point, Layer, Color, 0.0) == GS_BAD)
            AfxThrowUserException();

      pPoint = (C_POINT *) get_next();
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_POINT_LIST
// INIZIO FUNZIONI C_RECT
///////////////////////////////////////////////////////////////////////////


// costruttori
C_RECT::C_RECT() : C_NODE() {}
C_RECT::C_RECT(double Xmin, double Ymin, double Xmax, double Ymax) : C_NODE()
{
   // Mi assicuro che Xmin sia minore di Xmax e lo stesso per le Y
   BottomLeft.Set(min(Xmin, Xmax), min(Ymin, Ymax));
   TopRight.Set(max(Xmin, Xmax), max(Ymin, Ymax));
}
C_RECT::C_RECT(ads_point pt1, ads_point pt2) : C_NODE()
   { Set(pt1, pt2); }
C_RECT::C_RECT(const char *Str) : C_NODE()
   { fromStr(Str); }
C_RECT::C_RECT(const TCHAR *Str) : C_NODE()
   { fromStr(Str); }

void C_RECT::Set(ads_point pt1, ads_point pt2)
{
   // Mi assicuro che pt1 sia il punto in basso-destra e pt2 in alto-sinistra
   BottomLeft.Set(min(pt1[X], pt2[X]), min(pt1[Y], pt2[Y]), min(pt1[Z], pt2[Z]));
   TopRight.Set(max(pt1[X], pt2[X]), max(pt1[Y], pt2[Y]), max(pt1[Z], pt2[Z]));
}

void C_RECT::toStr(C_STRING &Str)
{
   C_STRING dummy;

   BottomLeft.toStr(Str, GS_3D);
   TopRight.toStr(dummy, GS_3D);
   Str += _T(';');
   Str += dummy;
}

void C_RECT::fromStr(const char *Str)
{
   double _X1, _Y1, _Z1, _X2, _Y2, _Z2;

   sscanf(Str, "%lf,%lf,%lf;%lf,%lf,%lf", &_X1, &_Y1, &_Z1, &_X2, &_Y2, &_Z2);

   BottomLeft.Set(_X1, _Y1, _Z1);
   TopRight.Set(_X2, _Y2, _Z2);
}
void C_RECT::fromStr(const TCHAR *Str)
{
   double _X1, _Y1, _Z1, _X2, _Y2, _Z2;

   swscanf(Str, _T("%lf,%lf,%lf;%lf,%lf,%lf"), &_X1, &_Y1, &_Z1, &_X2, &_Y2, &_Z2);

   BottomLeft.Set(_X1, _Y1, _Z1);
   TopRight.Set(_X2, _Y2, _Z2);
}

double C_RECT::Bottom(void) { return BottomLeft.y(); }
double C_RECT::Left(void) { return BottomLeft.x(); }
double C_RECT::Top(void) { return TopRight.y(); }
double C_RECT::Right(void) { return TopRight.x(); }

void C_RECT::Center(C_POINT &pt)
{
   pt.Set((Right() + Left()) / 2, (Top() + Bottom()) / 2);
   return;
}

double C_RECT::Width(void) { return Right() - Left(); }
double C_RECT::Height(void) { return Top() - Bottom(); }

bool C_RECT::operator == (C_RECT &in) 
{
   return (Bottom() == in.Bottom() && Left() == in.Left() &&
           Top() == in.Top() && Right() == in.Right()) ? 1 : 0; 
}

bool C_RECT::operator != (C_RECT &in) 
{
   return (Bottom() == in.Bottom() && Left() == in.Left() &&
           Top() == in.Top() && Right() == in.Right()) ? 0 : 1; 
}


/*********************************************************/
/*.doc C_RECT::Intersect                      <external> */
/*+                                                                       
/*+
   Questa funzione ritorna una zona in comune con con un altro rettangolo.
   Parametri:
   C_RECT &in;       Rettangolo in cui esaminare l'area in comune

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_RECT::Intersect(C_RECT &in)
{
   // Se non c'è intersezione
   if (Left() > in.Right() || Right() < in.Left() ||
       Bottom() > in.Top() || Top() < in.Bottom())
      return GS_BAD;

   BottomLeft.point[X] = max(Left(), in.Left());
   BottomLeft.point[Y] = max(Bottom(), in.Bottom());
   TopRight.point[X]   = min(Right(), in.Right());
   TopRight.point[Y]   = min(Top(), in.Top());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_RECT::Not                            <external> */
/*+                                                                       
/*+
   Questa funzione ritorna una zona (composta anche da più di un rettangolo)
   ottenuta eliminando l'area in comune con un altro rettangolo.
   Parametri:
   C_RECT &in;          Rettangolo la cui area in comune è da eliminare
   C_RECT_LIST &out;    Zona risultante (composta anche da più di un rettangolo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
void C_RECT::Not(C_RECT &in, C_RECT_LIST &out)
{
   C_RECT *pTopArea = NULL, *pBottomArea = NULL, *p;

   out.remove_all();

   // Se non hanno intersezioni
   if (in.Top() <= Bottom() || in.Bottom() >= Top() ||
       in.Right() <= Left() || in.Left() >= Right())
   {
      if ((p = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }
      p->BottomLeft.Set(Left(), Bottom());
      p->TopRight.Set(Right(), Top());
      out.add_tail(p);
      
      return;
   }

   // se esiste la parte alta
   if (!gsc_num_equal(in.Top(), Top()) && in.Top() < Top()) // a volte sono molto vicini da essere considerati uguali
   {
      if ((pTopArea = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }
      pTopArea->BottomLeft.Set(Left(), max(in.Top(), Bottom()));
      pTopArea->TopRight.Set(Right(), Top());
      out.add_tail(pTopArea);
   }

   // se esiste la parte bassa
   if (!gsc_num_equal(in.Bottom(), Bottom()) && in.Bottom() > Bottom()) // a volte sono molto vicini da essere considerati uguali
   {
      if ((pBottomArea = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }
      pBottomArea->BottomLeft.Set(Left(), Bottom());
      pBottomArea->TopRight.Set(Right(), min(in.Bottom(), Top()));
      out.add_tail(pBottomArea);
   }

   // se esiste la parte destra
   if (!gsc_num_equal(in.Right(), Right()) && in.Right() < Right()) // a volte sono molto vicini da essere considerati uguali
   {
      if ((p = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }

      // se è già stata considerata la parte bassa
      if (pBottomArea)
         p->BottomLeft.Set(max(in.Right(), Left()), pBottomArea->Top());
      else
         p->BottomLeft.Set(max(in.Right(), Left()), Bottom());

      // se è già stata considerata la parte alta
      if (pTopArea)
         p->TopRight.Set(Right(), pTopArea->Bottom());
      else
         p->TopRight.Set(Right(), Top());

      out.add_tail(p);
   }

   // se esiste la parte sinistra
   if (!gsc_num_equal(in.Left(), Left()) && in.Left() > Left()) // a volte sono molto vicini da essere considerati uguali
   {
      if ((p = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }

      // se è già stata considerata la parte bassa
      if (pBottomArea)
         p->BottomLeft.Set(Left(), pBottomArea->Top());
      else
         p->BottomLeft.Set(Left(), Bottom());

      // se è già stata considerata la parte alta
      if (pTopArea)
         p->TopRight.Set(min(in.Left(), Right()), pTopArea->Bottom());
      else
         p->TopRight.Set(min(in.Left(), Right()), Top());

      out.add_tail(p);
   }
}


/*********************************************************/
/*.doc C_RECT::Not                            <external> */
/*+                                                                       
/*+
   Questa funzione ritorna una zona (composta anche da più di un rettangolo)
   ottenuta eliminando le area in comune con altri rettangoli.
   Parametri:
   C_RECT_LIST &in;     Rettangoli le cui aree in comune sono da eliminare
   C_RECT_LIST &out;    Zona risultante (composta anche da più di un rettangolo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
void C_RECT::Not(C_RECT_LIST &in, C_RECT_LIST &out)
{
   C_RECT_LIST Partial, dummy;
   C_RECT      *p_in = (C_RECT *) in.get_head(), *p_out;

   out.remove_all();
   if (p_in)
   {
      Not(*p_in, out);

      p_in = (C_RECT *) p_in->get_next();
      while (p_in)
      {
         p_out = (C_RECT *) out.get_head();
         while (p_out)
         {
            p_out->Not(*p_in, dummy);
            Partial.paste_tail(dummy);

            p_out = (C_RECT *) p_out->get_next();
         }

         out.remove_all();
         out.paste_head(Partial);

         p_in = (C_RECT *) p_in->get_next();
      }
   }
   else
   {
      if ((p_out = new C_RECT()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return; }
      p_out->BottomLeft.Set(Left(), Bottom());
      p_out->TopRight.Set(Right(), Top());
      out.add_tail(p_out);
   }
}


/*********************************************************/
/*.doc C_RECT::In                             <external> */
/*+                                                                       
   Questa funzione ritorna GS_GOOD se il rettangolo è interno al
   rettangolo passato come parametro.
   Parametri:
   C_RECT &in;     Rettangolo contenitore
-*/  
/*********************************************************/
int C_RECT::In(C_RECT &in)
{
   return (Bottom() < in.Bottom() || Left() < in.Left() || 
           Top() > in.Top() || Right() > in.Right()) ? GS_BAD : GS_GOOD;
       
}


/*********************************************************/
/*.doc C_RECT::Contains                       <external> */
/*+                                                                       
   Questa funzione ritorna GS_GOOD se il rettangolo contiene punto è interno al
   rettangolo.
   Parametri:
   ads_point _pt; Punto da verificare
   int Mode;      Modalità di controllo. Se = CROSSING il punto è considerato
                  contenuto anche se si trova sul bordo, se = INSIDE deve
                  essere dentro (default = CROSSING)
-*/  
/*********************************************************/
int C_RECT::Contains(ads_point _pt, int Mode)
{
   if (Mode == CROSSING)
      return (_pt[X] >= Left() && _pt[X] <= Right() &&
              _pt[Y] >= Bottom() && _pt[Y] <= Top()) ? GS_GOOD : GS_BAD;
   else
      return (_pt[X] > Left() && _pt[X] < Right() &&
              _pt[Y] > Bottom() && _pt[Y] < Top()) ? GS_GOOD : GS_BAD;      
}


/*********************************************************/
/*.doc C_RECT::toAdeQryCondRB                 <external> */
/*+                                                                       
/*+
   Questa funzione ritorna una lista di resbuf per impostare
   una condizione di query ADE per "window" "crossing" nella zona.

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
presbuf C_RECT::toAdeQryCondRB(void)
{
  return acutBuildList(RTLB, 
                          RTSTR, WINDOW_SPATIAL_COND, // window
                          RTSTR, _T("crossing"),
                          RTPOINT, BottomLeft.point,
                          RTPOINT, TopRight.point,
                       RTLE, RTNONE);
}


/*********************************************************/
/*.doc C_RECT::Draw                           <external> */
/*+
  Questa funzione disegna il rettangolo in forma di polilinea.
  Parametri:
  const TCHAR *Layer; Nome piano esistente (Default = NULL "piano corrente")
  C_COLOR *Color;     Codice colore (se = NULL -> "colore corrente")
  int Mode;           Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                      (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
  const TCHAR *Lbl;   etichetta da inserire nel centro del rettangolo
                      (usato se Mode=EXTRACTION) (default = NULL)
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di successo o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int C_RECT::Draw(const TCHAR *Layer, C_COLOR *Color,
                 int Mode, const TCHAR *Lbl, AcDbBlockTableRecord *pBlockTableRecord)
{
   if (gsc_insert_rectangle(BottomLeft.point, TopRight.point,
                            Layer, Color, Mode, pBlockTableRecord) == GS_BAD)
      return GS_BAD;

   size_t LblLen = gsc_strlen(Lbl);

   if (Mode == EXTRACTION && LblLen > 0)
   {
      AcDbBlockTable       *pBlockTable;
      AcDbBlockTableRecord *pInternalBlockTableRecord;

      if (!pBlockTableRecord)
      {
         // Open the block table for read.
         if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                              AcDb::kForRead) != Acad::eOk)
            return GS_BAD;
         if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
            { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         pBlockTable->close();
      }
      else
         pInternalBlockTableRecord = pBlockTableRecord;

      // disegno l'etichetta
      C_POINT p;
      AcDbMText  *pLbl = new AcDbMText();
      
      Center(p); // calcolo il centro del rettangolo
      
      AcGePoint3d Location(p.x(), p.y(), 0.0);
      pLbl->setLocation(Location);
      pLbl->setTextHeight(gsc_min(Height(), Width()) / LblLen);
      pLbl->setWidth(Width());
      pLbl->setContents(Lbl);
      pLbl->setAttachment(AcDbMText::kMiddleCenter);
      if (Color) gsc_set_color(pLbl, *Color);
      pInternalBlockTableRecord->appendAcDbEntity(pLbl);
      pLbl->close();
      if (!pBlockTableRecord) pInternalBlockTableRecord->close();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_RECT::get_ExtractedGridClsCodeList   <external> */
/*+
  Questa funzione crea una lista delle classi griglia estratte
  che hanno delle celle che intersecano il rettangolo.
  Parametri:
  C_INT_LIST &ClsList; Lista codici classe

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
/*********************************************************/
int C_RECT::get_ExtractedGridClsCodeList(C_INT_LIST &ClsList)
{
   C_RECT          GridRect;
   C_INT           *pCodeCls;
   C_CLS_PUNT_LIST ExtractedClsList;
   C_CLS_PUNT      *pExtractedCls;
   C_CGRID         *pCls;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   ClsList.remove_all();

   // verifico se tra le classi estratte vi sono delle classi tipo griglia
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(ExtractedClsList, CAT_GRID) == GS_BAD)
      return GS_BAD;

   if (ExtractedClsList.get_count() == 0) return GS_GOOD;

   pExtractedCls = (C_CLS_PUNT *) ExtractedClsList.get_head();
   while (pExtractedCls)
   {
      pCls = (C_CGRID *) pExtractedCls->get_class();
      GridRect.BottomLeft.Set(pCls->ptr_grid()->x, pCls->ptr_grid()->y);
      GridRect.TopRight.Set(pCls->ptr_grid()->x + (pCls->ptr_grid()->dx * pCls->ptr_grid()->nx),
                            pCls->ptr_grid()->y + (pCls->ptr_grid()->dy * pCls->ptr_grid()->ny));

      if (Intersect(GridRect) == GS_GOOD)
      {
         if ((pCodeCls = new C_INT(pCls->ptr_id()->code)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         ClsList.add_tail(pCodeCls);
      }

      pExtractedCls = (C_CLS_PUNT *) ExtractedClsList.get_next();
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_RECT
// INIZIO FUNZIONI C_RECT_LIST
///////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_RECT_LIST::AdeQryDefine              <external> */
/*+                                                                       
/*+
   Questa funzione imposta una condizione di query ADE per tante
   "window" in "OR" modalità "crossing".

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_RECT_LIST::AdeQryDefine(void)
{
   C_RB_LIST AdeQryCondList;
   C_RECT    *p = (C_RECT *) get_head();

   if (!p) return NULL;
      
   if ((AdeQryCondList << p->toAdeQryCondRB()) == NULL) return GS_BAD;
   if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("location"), AdeQryCondList.get_head(), GS_EMPTYSTR) == ADE_NULLID)
      return GS_BAD;

   p = (C_RECT *) get_next();
   while (p)
   {
      if ((AdeQryCondList << p->toAdeQryCondRB()) == NULL) return GS_BAD;
      // dalla seconda condizione in poi impongo l'operatore OR
      if (ade_qrydefine(_T("or"), GS_EMPTYSTR, GS_EMPTYSTR, _T("location"), AdeQryCondList.get_head(), GS_EMPTYSTR) == ADE_NULLID)
         return GS_BAD;

      p = (C_RECT *) get_next();
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_RECT_LIST
// INIZIO FUNZIONI C_BOOL_MATRIX
///////////////////////////////////////////////////////////////////////////


C_BOOL_MATRIX::C_BOOL_MATRIX() 
{ 
   Matrix = NULL; 
   nx = ny = 0;
}
      
C_BOOL_MATRIX::C_BOOL_MATRIX(long nCols, long nRows) 
{ 
   Matrix = NULL;
   alloc(nCols, nRows);
}

C_BOOL_MATRIX::~C_BOOL_MATRIX()
{
   if (Matrix) free(Matrix);
}

long C_BOOL_MATRIX::get_nCols() { return nx; }
long C_BOOL_MATRIX::get_nRows() { return ny; }


/*************************************************************************/
/*.doc int C_BOOL_MATRIX::alloc                                           */
/*+
  La funzione alloca memoria per la matrice.
  Parametri:
  long nCols;     Numero di colonne
  long nRows;     Numero di righe

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_BOOL_MATRIX::alloc(long nCols, long nRows)
{
   if (Matrix) free(Matrix);
   if ((Matrix = (bool *) malloc(sizeof(bool) * nCols * nRows)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   nx = nCols;
   ny = nRows;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc bool C_BOOL_MATRIX::get                                            */
/*+
  La funzione restituisce il valore di una cella della matrice.
  Parametri:
  long Col;     Numero di colonna
  long Row;     Numero di riga

  Ritorna il valore di una cella della matrice.
-*/
/*************************************************************************/
bool C_BOOL_MATRIX::get(long Col, long Row)
{
   // NOTA:  per gestire  matrici  bidimensionali  con allocazione  dinamica
   // occorre  ricordare che  non  posso accedervi  usando  A[y][x] ma  devo
   // calcolare la posizione di ciascun elemento con A[y*larghezza+x]
   return Matrix[Row * nx + Col];
}


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::set                                           */
/*+
  La funzione imposta il valore di una cella della matrice.
  Parametri:
  long Col;     Numero di colonna
  long Row;     Numero di riga
  bool val;     Valore
-*/
/*************************************************************************/
void C_BOOL_MATRIX::set(long Col, long Row, bool val)
{
   // NOTA:  per gestire  matrici  bidimensionali  con allocazione  dinamica
   // occorre  ricordare che  non  posso accedervi  usando  A[y][x] ma  devo
   // calcolare la posizione di ciascun elemento con A[y*larghezza+x]
   Matrix[Row * nx + Col] = val;
}


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::set1DArray                                    */
/*+
  La funzione imposta il valore di una cella della matrice considerata come 
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;    Posizione della cella nel vettore
  bool val;    Valore
-*/
/*************************************************************************/
void C_BOOL_MATRIX::set1DArray(long Pos, bool val) { Matrix[Pos] = val; }


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::get1DArray                                     */
/*+
  La funzione legge il valore di una cella della matrice considerata come 
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;       Posizione della cella nel vettore
-*/
/*************************************************************************/
bool C_BOOL_MATRIX::get1DArray(long Pos) { return Matrix[Pos]; }


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::getColRowFrom1DArray                          */
/*+
  La funzione calcola la colonna e la riga data la posizione in un
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;       Posizione della cella nel vettore (0-based)
  long *Col;      Colonna
  long *Row;      Riga
-*/
/*************************************************************************/
void C_BOOL_MATRIX::getColRowFrom1DArray(long Pos, long *Col, long *Row)
{
   *Col = Pos % nx;
   *Row = (long) floor((double)Pos / nx);
}


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::setColumn                                     */
/*+
  La funzione imposta i valori delle celle di una colonna della matrice.
  Parametri:
  long Col;     Numero di colonna
  bool val;     Valore
-*/
/*************************************************************************/
void C_BOOL_MATRIX::setColumn(long Col, bool val)
{
   for (long i = 0; i < ny; i++) set(Col, i, val);
}


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::setRow                                        */
/*+
  La funzione imposta i valori delle celle di una riga della matrice.
  Parametri:
  long Row;     Numero di riga
  bool val;     Valore
-*/
/*************************************************************************/
void C_BOOL_MATRIX::setRow(long Row, bool val)
{
   for (long i = 0; i < nx; i++) set(i, Row, val);
}


/*************************************************************************/
/*.doc void C_BOOL_MATRIX::init                                            */
/*+
  La funzione inizializza tutte le celle della matrice.
  Parametri:
  bool val;   Valore
-*/
/*************************************************************************/
void C_BOOL_MATRIX::init(bool val)
{
   long Tot = nx * ny;

   for (long i = 0; i < Tot; i++) Matrix[i] = val;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BOOL_MATRIX
// INIZIO FUNZIONI C_DBL_MATRIX
///////////////////////////////////////////////////////////////////////////


C_DBL_MATRIX::C_DBL_MATRIX() 
{ 
   Matrix = NULL; 
   nx = ny = 0;
}
      
C_DBL_MATRIX::C_DBL_MATRIX(long nCols, long nRows) 
{ 
   Matrix = NULL;
   alloc(nCols, nRows);
}

C_DBL_MATRIX::~C_DBL_MATRIX()
{
   if (Matrix) free(Matrix);
}

long C_DBL_MATRIX::get_nCols() { return nx; }
long C_DBL_MATRIX::get_nRows() { return ny; }


/*************************************************************************/
/*.doc int C_DBL_MATRIX::alloc                                           */
/*+
  La funzione alloca memoria per la matrice.
  Parametri:
  long nCols;     Numero di colonne
  long nRows;     Numero di righe

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int C_DBL_MATRIX::alloc(long nCols, long nRows)
{
   if (Matrix) free(Matrix);
   if ((Matrix = (double *) malloc(sizeof(double) * nCols * nRows)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   nx = nCols;
   ny = nRows;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc double C_DBL_MATRIX::get                                          */
/*+
  La funzione restituisce il valore di una cella della matrice.
  Parametri:
  long Col;     Numero di colonna
  long Row;     Numero di riga

  Ritorna il valore di una cella della matrice.
-*/
/*************************************************************************/
double C_DBL_MATRIX::get(long Col, long Row)
{
   // NOTA:  per gestire  matrici  bidimensionali  con allocazione  dinamica
   // occorre  ricordare che  non  posso accedervi  usando  A[y][x] ma  devo
   // calcolare la posizione di ciascun elemento con A[y*larghezza+x]
   return Matrix[Row * nx + Col];
}


/*************************************************************************/
/*.doc void C_DBL_MATRIX::set                                             */
/*+
  La funzione imposta il valore di una cella della matrice.
  Parametri:
  long Col;     Numero di colonna
  long Row;     Numero di riga
  double val;   Valore
-*/
/*************************************************************************/
void C_DBL_MATRIX::set(long Col, long Row, double val)
{
   // NOTA:  per gestire  matrici  bidimensionali  con allocazione  dinamica
   // occorre  ricordare che  non  posso accedervi  usando  A[y][x] ma  devo
   // calcolare la posizione di ciascun elemento con A[y*larghezza+x]
   Matrix[Row * nx + Col] = val;
}


/*************************************************************************/
/*.doc void C_DBL_MATRIX::set1DArray                                     */
/*+
  La funzione imposta il valore di una cella della matrice considerata come 
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;       Posizione della cella nel vettore
  double val;     Valore
-*/
/*************************************************************************/
void C_DBL_MATRIX::set1DArray(long Pos, double val) { Matrix[Pos] = val; }


/*************************************************************************/
/*.doc void C_DBL_MATRIX::get1DArray                                     */
/*+
  La funzione legge il valore di una cella della matrice considerata come 
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;       Posizione della cella nel vettore
-*/
/*************************************************************************/
double C_DBL_MATRIX::get1DArray(long Pos) { return Matrix[Pos]; }


/*************************************************************************/
/*.doc void C_DBL_MATRIX::getColRowFrom1DArray                           */
/*+
  La funzione calcola la colonna e la riga data la posizione in un
  vettore ad una sola dimensione (come effettivamente era stata allocata).
  Parametri:
  long Pos;       Posizione della cella nel vettore (0-based)
  long *Col;      Colonna
  long *Row;      Riga
-*/
/*************************************************************************/
void C_DBL_MATRIX::getColRowFrom1DArray(long Pos, long *Col, long *Row)
{
   *Col = Pos % nx;
   *Row = (long) floor((double)Pos / nx);
}


/*************************************************************************/
/*.doc void C_DBL_MATRIX::setColumn                                      */
/*+
  La funzione imposta i valori delle celle di una colonna della matrice.
  Parametri:
  long Col;     Numero di colonna
  double val;   Valore
-*/
/*************************************************************************/
void C_DBL_MATRIX::setColumn(long Col, double val)
{
   for (long i = 0; i < ny; i++) set(Col, i, val);
}


/*************************************************************************/
/*.doc void C_DBL_MATRIX::setRow                                         */
/*+
  La funzione imposta i valori delle celle di una riga della matrice.
  Parametri:
  long Row;     Numero di riga
  double val;   Valore
-*/
/*************************************************************************/
void C_DBL_MATRIX::setRow(long Row, double val)
{
   for (long i = 0; i < nx; i++) set(i, Row, val);
}


/*************************************************************************/
/*.doc void C_DBL_MATRIX::init                                           */
/*+
  La funzione inizializza tutte le celle della matrice.
  Parametri:
  double val;   Valore
-*/
/*************************************************************************/
void C_DBL_MATRIX::init(double val)
{
   long Tot = nx * ny;

   for (long i = 0; i < Tot; i++) Matrix[i] = val;
}


//---------------------------------------------------------------------------//
// FINE FUNZIONI C_DBL_MATRIX
// INIZIO CLASSE C_STR_STRLIST 
//---------------------------------------------------------------------------//


C_STR_STRLIST::C_STR_STRLIST() : C_STR() { pStr_StrList = NULL; }

C_STR_STRLIST::C_STR_STRLIST(const char *in) : C_STR() 
{ 
   name.set_name(in);
   pStr_StrList = NULL;
}
C_STR_STRLIST::C_STR_STRLIST(const TCHAR *in) : C_STR() 
{ 
   name.set_name(in);
   pStr_StrList = NULL;
}

C_STR_STRLIST::~C_STR_STRLIST() 
   { if (pStr_StrList) delete pStr_StrList; }

C_STR_STRLIST_LIST* C_STR_STRLIST::ptr_Str_StrList(void)
   { return pStr_StrList; }

/**************************************************************/
/*.doc C_STR_STRLIST::add_tail_str                            */
/*+
  Funzione che aggiunge in coda una stringa
  nella lista.
  Parametri:
  const TCHAR *Value;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_STR_STRLIST::add_tail(C_STR_STRLIST *newnode)
{
   if (!pStr_StrList) 
      if ((pStr_StrList = new C_STR_STRLIST_LIST()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return pStr_StrList->add_tail(newnode);
}

int C_STR_STRLIST::add_tail_str(const TCHAR *Value)
{
   C_STR_STRLIST *p;

   if ((p = new C_STR_STRLIST(Value)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (add_tail(p) == GS_BAD)
      { delete p; return GS_BAD; }
   else
      return GS_GOOD;
}

//---------------------------------------------------------------------------//
// FINE CLASSE C_STR_STRLIST
// INIZIO CLASSE C_STR_STRLIST_LIST
//---------------------------------------------------------------------------//


//---------------------------------------------------------------------------//
// FINE CLASSE C_STR_STRLIST_LIST
//---------------------------------------------------------------------------//


C_STR_STRLIST_LIST::C_STR_STRLIST_LIST() : C_STR_LIST() {}

C_STR_STRLIST_LIST::~C_STR_STRLIST_LIST() {}
