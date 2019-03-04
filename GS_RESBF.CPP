/**********************************************************
Name: GS_RESBF

Module description: File funzioni di base per la gestione
                    delle liste (resbuf). 
            
Author: Roberto Poltini

(c) Copyright 1995-2012 by IREN ACQUA GAS S.p.A.

**********************************************************/

/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/

#include "stdafx.h"         // MFC core and standard components
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "adslib.h"    
#include "asiappl.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_init.h"

#include "gs_utily.h"
#include "gs_resbf.h"

/****************************************************************************/
/* GLOBALS */
/****************************************************************************/


presbuf GS_RESBUF = NULL;       // resbuf pubblico


/*************************************************************************/
/*.doc gsc_copybuf (internal)*/  
/*+
  Copia il contenuto del 'resbuf' puntato parametro in ingresso in un 
  'resbuf' allocato all'interno della funzione e ne ritorna il puntatore .
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/
struct resbuf *gsc_copybuf(struct resbuf *sorc)
{
   struct resbuf *dest;                    
   
   if (!sorc) { GS_ERR_COD = eGSInvalidArg; return NULL; }
    
   switch (sorc->restype)
   {
      case RTLB :
           if ((dest = ads_newrb(RTLB)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;
         
      case RTPOINT :
           if ((dest = ads_newrb(RTPOINT)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           ads_point_set(sorc->resval.rpoint, dest->resval.rpoint);
           break;

      case RT3DPOINT :
           if ((dest = ads_newrb(RT3DPOINT)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           ads_point_set(sorc->resval.rpoint, dest->resval.rpoint);
           break;

      case RTREAL :
           if ((dest = ads_newrb(RTREAL)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
         dest->resval.rreal = sorc->resval.rreal;
         break;

      case RTSHORT:
           if ((dest = ads_newrb(RTSHORT)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           dest->resval.rint = sorc->resval.rint;
           break;

      case RTANG:
           if ((dest = ads_newrb(RTANG)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           dest->resval.rreal = sorc->resval.rreal;
           break;

      case RTSTR:
           if ((dest = acutBuildList(RTSTR,sorc->resval.rstring,0))==NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;     
         
      case RTENAME :
           if ((dest = ads_newrb(RTENAME)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           dest->resval.rlname[0] = sorc->resval.rlname[0];
           dest->resval.rlname[1] = sorc->resval.rlname[1];
           break;

      case RTPICKS :
           if ((dest = ads_newrb(RTPICKS)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }

           dest->resval.rlname[0] = sorc->resval.rlname[0];
           dest->resval.rlname[1] = sorc->resval.rlname[1];
           break;
            
      case RTORINT :
           if ((dest = ads_newrb(RTORINT)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           dest->resval.rreal = sorc->resval.rreal;
           break;

      case RTLONG :
           if ((dest = ads_newrb(RTLONG))==NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           dest->resval.rlong = sorc->resval.rlong;
           break; 

      case RTLE :
           if ((dest = ads_newrb(RTLE)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTDOTE :
           if ((dest = ads_newrb(RTDOTE)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTNIL :
           if ((dest = ads_newrb(RTNIL)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTVOID :
           if ((dest = ads_newrb(RTVOID)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTNONE :
           if ((dest = ads_newrb(RTNONE)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTDXF0 :
           if ((dest = acutBuildList(RTDXF0,sorc->resval.rstring,0))==NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;

      case RTT :
           if ((dest = ads_newrb(RTT)) == NULL)
              { GS_ERR_COD = eGSOutOfMem; return NULL; }
           break;
   }

   return dest;
}


/*************************************************************************/
/*.doc gsc_rblistcopy (internal)*/  
/*+
  Riceve in input il puntatore ad un 'resbuf' di una lista e restituisce il
  puntatore ad una lista contenente la copia della prima.
  Alloca memoria !!.
-*/
/*************************************************************************/
presbuf gsc_rblistcopy(presbuf lista)
{
   presbuf pindex=lista, result=NULL, prbcopia;
      
   while (pindex != NULL)
   {
      if (result==NULL) // prima volta
      {
         if ((result = prbcopia = gsc_copybuf(pindex)) == NULL) return NULL;
      }
      else
      {
         if ((prbcopia->rbnext = gsc_copybuf(pindex)) == NULL)
            { acutRelRb(result); return NULL; }
         prbcopia = prbcopia->rbnext;
      }
      pindex = pindex->rbnext;
   }

   return (result);
}
      

/*************************************************************************/
/*.doc gsc_sostitutebuf (internal)*/  
/*+
  Sostituisce il contenuto del 'resbuf' puntato dal parametro in ingresso con 
  quello di un altro resbuf (dest già allocato).
-*/
/*************************************************************************/
int gsc_sostitutebuf(presbuf sorc, presbuf dest)
{
   if (sorc == NULL || dest == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (dest->restype == RTSTR || dest->restype == RTDXF0)
   {
      if (dest->resval.rstring != NULL) free(dest->resval.rstring);
   }
   else
      if (dest->restype == RTPICKS)
         if (!ads_name_nil(dest->resval.rlname)) ads_ssfree(dest->resval.rlname);

   switch (sorc->restype)
   {
      case RTLB :
         dest->restype = RTLB;
         break;
      case RTPOINT :
         dest->restype = RTPOINT;
         ads_point_set(sorc->resval.rpoint, dest->resval.rpoint);
         break;
      case RT3DPOINT :
         dest->restype = RT3DPOINT;
         ads_point_set(sorc->resval.rpoint, dest->resval.rpoint);
         break;
      case RTREAL :
         dest->restype = RTREAL;
         dest->resval.rreal = sorc->resval.rreal;
         break;
      case RTSHORT:
         dest->restype = RTSHORT;
         dest->resval.rint = sorc->resval.rint;
         break;
      case RTANG:
         dest->restype = RTANG;
         dest->resval.rreal = sorc->resval.rreal;
         break;
      case RTSTR:
         dest->restype = RTSTR;
         if (sorc->resval.rstring != NULL)
         {
            if ((dest->resval.rstring = gsc_tostring(sorc->resval.rstring)) == NULL)
               { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
         }
         else dest->resval.rstring = NULL;
         break;     
      case RTENAME :
         dest->restype = RTENAME;
         dest->resval.rlname[0] = sorc->resval.rlname[0];
         dest->resval.rlname[1] = sorc->resval.rlname[1];
         break;

      case RTPICKS :
         dest->restype = RTPICKS;
         dest->resval.rlname[0] = sorc->resval.rlname[0];
         dest->resval.rlname[1] = sorc->resval.rlname[1];
         break;
            
      case RTORINT :
         dest->restype = RTORINT;
         dest->resval.rreal = sorc->resval.rreal;
         break;

      case RTLONG :
         dest->restype = RTLONG;
         dest->resval.rlong = sorc->resval.rlong;
         break; 

      case RTLE :
         dest->restype = RTLE;
         break;

      case RTDOTE :
         dest->restype = RTDOTE;
         break;

      case RTNIL :
         dest->restype = RTNIL;
         break;

      case RTVOID :
         dest->restype = RTVOID;
         break;

      case RTDXF0 :
         dest->restype = RTDXF0;
         if (sorc->resval.rstring != NULL)
         {
            if ((dest->resval.rstring = gsc_tostring(sorc->resval.rstring)) == NULL)
               { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
         }
         else dest->resval.rstring = NULL;
         break;

      case RTT :
         dest->restype = RTT;
         break;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_scorri (internal)*/  
/*+
  Riceve in input il puntatore ad un 'resbuf' di una lista e restituisce il
  puntatore al termine della lista corrispondente, definito normalmente da un
  elemento RTLE o RTDOTE (coppie puntate).
  La funzione e' ricorsiva.
-*/
/*************************************************************************/
presbuf gsc_scorri(presbuf lista)
{
   if (!lista) return NULL;

   // Se l'elemento è un punto allora è una lista che inizia e finisce 
   // nello stesso elemento
   switch (lista->restype)
   {
      case RTPOINT: case RT3DPOINT:
      case 10: case 11: case 12: case 13: case 14: case 15:
      case 16: case 17: case 18: case 19:
      case 1010: case 1011: case 1012: case 1013: case 1014: case 1015:
      case 1016: case 1017: case 1018: case 1019:
         return lista;
   }

   while ((lista = lista->rbnext) != NULL)
   {
      switch (lista->restype)
      {
         case RTLE: case RTDOTE:
            return lista;

         case RTLB :
            lista = gsc_scorri(lista);
            break ;
      }
   }

   return (lista);
}


/*************************************************************************/
/*.doc gsc_listcopy                                                      */  
/*+
  La funzione copia una lista di resbuf partendo da RTLB (a cui ci si
  deve trovare) fino al relativo RTLE.
  Parametri:
  presbuf lista;     Lista di resbuf
  presbuf *p;        opzionale; puntatore alla fine della lista (RTLE)
                     (default = NULL)

  Ritorna il puntatore alla lista copiata in caso di successo altrimenti NULL.
-*/
/*************************************************************************/
presbuf gsc_listcopy(presbuf lista, presbuf *p)
{
   C_RB_LIST new_list;
   presbuf   p_ndx = lista;
   int       count = 0;

   if (!lista || lista->restype != RTLB) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   while (p_ndx)
   {
      if ((new_list += gsc_copybuf(p_ndx)) == NULL) return NULL;

      switch (p_ndx->restype)
      {
         case RTLB :
            count++; 
            break;
         case RTLE :
         case RTDOTE :
            count--;
            break;
      }
      if (count == 0) break;
      p_ndx = p_ndx->rbnext;
   }

   if (count != 0) { GS_ERR_COD = eGSListError; return NULL; }

   new_list.ReleaseAllAtDistruction(GS_BAD);
   if (p) *p = p_ndx;

   return new_list.get_head();
}


/*************************************************************************/
/*.doc gsc_scorcopy (internal)*/  
/*+
  Scorre il contenuto di una lista analogamente a gsc_scorri()
  ricopiandolo in coda ad un'altra.
  Il primo parametro e` il puntatore al primo 'resbuf' della lista da copiare.
  (NB: il primo elemento non viene copiato, si parte dal successivo).
  Il secondo parametro e` il puntatore all'ultimo 'resbuf' della lista a cui 
  appendere la copia della lista sorgente.  La funzione e' ricorsiva. 
  Ritorna il puntatore all'ultimo 'resbuf' della lista di destinazione.
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/
struct resbuf *gsc_scorcopy(struct resbuf *aaa, struct resbuf *bbb)
{
   int error = 0;

   while ((aaa = aaa->rbnext) != NULL)
   {
      if (bbb==NULL)
         return NULL;

      switch(aaa->restype)
         {
         case RTLE :
         case RTDOTE :
              bbb=bbb->rbnext=gsc_copybuf(aaa);
              return bbb;

         case RTLB :
              if ( (bbb=bbb->rbnext=gsc_copybuf(aaa))==NULL )
                 return NULL;
              if ( (bbb = gsc_scorcopy(aaa,bbb))==NULL )
                 return NULL;
              aaa = gsc_scorri(aaa);
              break;
    
         default :
              if ( (bbb=bbb->rbnext=gsc_copybuf(aaa))==NULL)
                 return NULL;
              break;
         }           /* END SWITCH */
      }        /* END WHILE */

   GS_ERR_COD=eGSListError;
   return NULL;
}


/*************************************************************************/
/*.doc gsc_length (external)*/  
/*+
  Riceve in input il puntatore ad una lista e restituisce il
  numero di elementi contenuti nella lista (-1 se non e' una lista).
  Siamo in presenza di una lista se il tipo dell'elemento passato
  e` RTLB, RTPOINT o RT3DPOINT (in questi due ultimi casi ritorna
  sempre rispettivamente 2 e 3).  
-*/
/*************************************************************************/

int gsc_length(struct resbuf *lista)
{
   int lungh = 0;
   
   if (lista == NULL) return -1; 

   switch (lista->restype)
   {
      case RTPOINT :
         return 2;  
           
      case RT3DPOINT :
         return 3;
           
      case RTLB :
         while ((lista = lista->rbnext) != NULL && 
                lista->restype != RTLE && lista->restype != RTDOTE)
         {
            if (lista->restype == RTLB) lista = gsc_scorri(lista);
            lungh++;
         }
         return (lungh);

      default :
         GS_ERR_COD = eGSListError;
         return -2;
   }
}


/*************************************************************************/
/*.doc gsc_nth (external)*/  
/*+
  Restituisce il putatore all'ennesimo elemento della lista.
  La funzione ha due parametri. 
  Il primo e` l'indice (n) dell'elemento da cercare. Questo parametro deve 
  essere >= 0 (NB: 0 indica il primo elemento).
  Il secondo parametro e` il puntatore all'inizio della lista in cui
  cercare.
  I tipi permessi per il primo elemento della lista sono RTLB, RTPOINT e 
  RT3DPOINT.
  La funzione ritorna NULL in caso di errore. 
-*/
/*************************************************************************/

struct resbuf *gsc_nth(int indice,struct resbuf *lista)
{ 
   int cont = 0;

   if (indice < 0 || indice >= gsc_length(lista)) return NULL;
   
   while ((lista = lista->rbnext) != NULL)
   {
      if (cont == indice) break;
      if (lista->restype == RTLB) lista = gsc_scorri(lista);
      cont++;
   }       
   
   return lista;
}


/*************************************************************************/
/*.doc gsc_nthcopy (external)*/  
/*+
  Restituisce il puntatore alla copia dell'n-esimo elemento della lista.
  La funzione ha due parametri. 
  Il primo e` l'indice (n) dell'elemento da copiare. Questo parametro deve 
  essere >= 0 (NB: 0 indica il primo elemento).
  Il secondo parametro e` il puntatore all'inizio della lista da cui copiare.
  I tipi permessi per il primo elemento della lista sono RTLB, RTPOINT e 
  RT3DPOINT.
  La funzione restituisce NULL in caso di errore.
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/
struct resbuf *gsc_nthcopy(int indice,struct resbuf *lista)
{ 
   int cont = 0;
   struct resbuf *target;
   
   if ( (indice<0) || (indice>=gsc_length(lista)) )
      { GS_ERR_COD=eGSInvalidArg; return NULL; }
   
   while ( (lista=lista->rbnext)!=NULL)
      {
      if (cont==indice)
         break;
      if (lista->restype==RTLB)
         lista=gsc_scorri(lista);
      cont++;
      }       

   if ((target = gsc_copybuf(lista)) == NULL)
      { GS_ERR_COD=eGSOutOfMem; return NULL; }
   if (lista->restype==RTLB)
      {
      if (gsc_scorcopy(lista,target)==NULL)
         return NULL;
      }

return target; 
}



/*************************************************************************/
/*.doc gsc_car (external)*/  
/*+
  Restituisce il puntatore alla copia del primo elemento della lista 
  puntata da puntatore passato come parametro.
  I tipo accettati per il primo elemento della lista sono RTLB, RTPOINT e
  RT3DPOINT.
  In caso di errore nei parametri la funzione ritorna NULL.
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/

struct resbuf *gsc_car(struct resbuf *rb)
{
   struct resbuf *temp;

   if (rb == NULL)
      { GS_ERR_COD=eGSListError; return NULL; }

   switch (rb->restype)
   {
      case RTPOINT :
      case RT3DPOINT :
         if ( (temp=acutBuildList(RTREAL,rb->resval.rpoint[0],0))==NULL)
            { GS_ERR_COD=eGSOutOfMem; return NULL; }
         return(temp);
     
      case RTLB :
         if ( (temp=gsc_nthcopy(0,rb))==NULL)
            return NULL;
         return temp;
           
      default :
         GS_ERR_COD=eGSListError;
         return NULL;
      }
}


/*************************************************************************/
/*.doc gsc_cdr (external)*/  
/*+
  Restituisce il puntatore alla lista contenente la copia di tutti gli
  elementi della lista passata come parametro escluso il primo.
  I tipo accettati per il primo elemento della lista passata come parametro
  sono RTLB, RTPOINT e RT3DPOINT.
  In caso di errore nei parametri la funzione ritorna NULL.
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/
struct resbuf *gsc_cdr(struct resbuf *lista)
{
   struct resbuf *temp;

   switch (lista->restype)
      {
      case RTPOINT :
           if ((temp=acutBuildList(RTLB,
                                   RTREAL,lista->resval.rpoint[1],
                                   RTLE,0)) == NULL)
          GS_ERR_COD=eGSOutOfMem;
           return(temp);

      case RT3DPOINT :
           if ((temp=acutBuildList(RTLB,
                                   RTREAL,lista->resval.rpoint[1],
                                   RTREAL,lista->resval.rpoint[2],
                                   RTLE,0)) == NULL)
           GS_ERR_COD=eGSOutOfMem;
           return(temp);

      case RTLB :
                       /*** Vai alla fine del primo elemto ***/       
           lista=lista->rbnext;
           if (lista->restype==RTLB)
              lista=gsc_scorri(lista);
                       /*** Copia la lista da qui' in poi ***/
           if ((temp = ads_newrb(RTLB)) == NULL)
              { GS_ERR_COD=eGSOutOfMem; return NULL; }
           if (gsc_scorcopy(lista,temp)==NULL)
              return NULL;
           return temp;

      default :
           GS_ERR_COD=eGSListError;
           return NULL;
      }
}


/*************************************************************************/
/*.doc gsc_equal (external)*/  
/*+
  Confronta il contenuto di due elementi di un resbuf.
  Parametri:
  presbuf a;      primo resbuf
  presbuf b;      secondo resbuf
  int sensitive;  usato solo per stringhe; sensibile al maiuscolo (default = TRUE)

  Restituisce TRUE se sono uguali altrimenti FALSE.  
-*/
/*************************************************************************/
int gsc_equal(presbuf a, presbuf b, int sensitive)
{
   if (a == NULL && b == NULL) return TRUE;
   if (a == NULL || b == NULL) return FALSE;

   switch (a->restype)
   {
      case RTNONE: return (b->restype == RTNONE) ? TRUE : FALSE;
      case RTLB  : return (b->restype == RTLB) ? TRUE : FALSE;
      case RTLE  : return (b->restype == RTLE) ? TRUE : FALSE;
      case RTDOTE: return (b->restype == RTDOTE) ? TRUE : FALSE;
      case RTVOID: return (b->restype == RTVOID) ? TRUE : FALSE;
      case RTNIL : return (b->restype == RTNIL) ? TRUE : FALSE;
      case RTT   : return (b->restype == RTT) ? TRUE : FALSE;
             
      case RTPOINT:
         if (b->restype == RTPOINT &&
             ads_2Dpoint_equal(a->resval.rpoint, b->resval.rpoint))
            return TRUE;
         else return FALSE;    
                
      case RT3DPOINT:
         if (b->restype == RT3DPOINT &&
             gsc_point_equal(a->resval.rpoint, b->resval.rpoint))
            return TRUE;
         else return FALSE;  

      case RTREAL : 
      case RTANG  : 
      case RTORINT:
         switch (b->restype)
         {
            case RTREAL :
            case RTANG  :
            case RTORINT:
               return (a->resval.rreal == b->resval.rreal) ? TRUE : FALSE;
            case RTSHORT:
               return (a->resval.rreal == b->resval.rint) ? TRUE : FALSE;
            case RTLONG:
               return (a->resval.rreal == b->resval.rlong) ? TRUE : FALSE;
            default: return FALSE;
         }

      case RTSHORT:
         switch (b->restype)
         {
            case RTREAL :
            case RTANG  :
            case RTORINT:
               return (a->resval.rint == b->resval.rreal) ? TRUE : FALSE;
            case RTSHORT:
               return (a->resval.rint == b->resval.rint) ? TRUE : FALSE;
            case RTLONG:
               return (a->resval.rint == b->resval.rlong) ? TRUE : FALSE;
            default: return FALSE;
         }

      case RTLONG:
         switch (b->restype)
         {
            case RTREAL :
            case RTANG  :
            case RTORINT:
               return (a->resval.rlong == b->resval.rreal) ? TRUE : FALSE;
            case RTSHORT:
               return (a->resval.rlong == b->resval.rint) ? TRUE : FALSE;
            case RTLONG:
               return (a->resval.rlong == b->resval.rlong) ? TRUE : FALSE;
            default: return FALSE;
         }

      case RTSTR : 
      case RTDXF0:
          if ((b->restype == RTSTR || b->restype == RTDXF0) &&
             gsc_strcmp(a->resval.rstring, b->resval.rstring, sensitive) == 0)
             return TRUE;
          else return FALSE;

      case RTENAME: 
      case RTPICKS:
          if ((b->restype == RTENAME || b->restype == RTPICKS) &&
              (a->resval.rlname[0] == b->resval.rlname[0]) &&
              (a->resval.rlname[1] == b->resval.rlname[1]))
             return TRUE;
          else return FALSE; 

      default : return FALSE;
   }
}


/*************************************************************************/
/*.doc gsc_assoccopy (external)*/  
/*+
  Riceve due parametri una lista di ricerca (lista) ed un puntatore ad 
  una chiave da ricercare (indice).
  La lista deve a sua volta contenere delle sottoliste.
  La funzione restituisce la copia della sottolista (di lista) il cui primo 
  elemento sia uguale a (indice).
  La funzione ritorna il puntatore alla copia della sottolista o NULL nel caso
  di errore nei parametri o nel caso non trovi la chiave di ricerca.
  (WARNING: Alloca memoria)
-*/
/*************************************************************************/
struct resbuf *gsc_assoccopy(struct resbuf *indice, struct resbuf *lista)
{
   int i;
   struct resbuf *buf,*first;
   
   if ((lista  == NULL) || (indice == NULL))
      return (NULL);

   for (i=0;i<gsc_length(lista);i++)
       {
       if ( (buf=gsc_nthcopy(i,lista))==NULL)  return NULL;

       if ( buf->restype==RTPOINT || buf->restype==RT3DPOINT ||
                     buf->restype==RTLB )
          {
          if ( (first=gsc_car(buf))==NULL)
             { acutRelRb(buf); return NULL; }
          
          if (gsc_equal(first,indice)) 
             {
             acutRelRb(first);
             return buf;
             }
          acutRelRb(first);
          }
       acutRelRb(buf);
       }
   return NULL;
}
struct resbuf *gsc_assoccopy(char *stringa, struct resbuf *lista)
{
   TCHAR         *UnicodeString = gsc_CharToUnicode(stringa);  
   struct resbuf *res = gsc_assoccopy(UnicodeString, lista);
   if (UnicodeString) free(UnicodeString);

   return res;
}
struct resbuf *gsc_assoccopy(TCHAR *stringa, struct resbuf *lista)
{
   presbuf p_key, p_ret;

   if ((p_key = acutBuildList(RTSTR, stringa, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      
   p_ret = gsc_assoccopy(p_key, lista);
      
   acutRelRb(p_key);
   
   return p_ret;
}


/*************************************************************************/
/*.doc gsc_assoc (external)*/  
/*+
  Riceve due parametri una lista di ricerca (lista) ed un puntatore ad 
  una chiave da ricercare (Key).
  La lista deve a sua volta contenere delle sottoliste.
  Parametri:
  presbuf Key;       resbuf da cercare 
  presbuf lista;     lista in cui cercare
  int sensitive;     usato solo per stringhe; sensibile al maiuscolo (default = TRUE)

  La funzione restituisce la sottolista (di lista) il cui primo 
  elemento sia uguale a (Key) o NULL nel caso
  di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
struct resbuf *gsc_assoc(presbuf Key, presbuf lista, int sensitive)
{
   presbuf p, pItem = lista;

   if (!lista || !Key) return NULL;

   while ((pItem = pItem->rbnext))
   {
      if (pItem->restype == RTLB)
      {
         if ((p = pItem->rbnext) == NULL) return NULL;
         if (gsc_equal(p, Key, sensitive)) return pItem;
         pItem = gsc_scorri(pItem); // vado all'elemento successivo
      }
      else if (pItem->restype != RTNIL)
         return NULL;
   }

   return NULL;
}
struct resbuf *gsc_assoc(const char *stringa, presbuf lista, int sensitive)
{
   TCHAR         *UnicodeString = gsc_CharToUnicode(stringa);  
   struct resbuf *res = gsc_assoc(UnicodeString, lista, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
struct resbuf *gsc_assoc(const TCHAR *stringa, presbuf lista, int sensitive)
{
   presbuf p_key, p_ret;

   if (!stringa) return NULL;
   if ((p_key = acutBuildList(RTSTR, stringa, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
      
   p_ret = gsc_assoc(p_key, lista, sensitive);
      
   acutRelRb(p_key);
   
   return p_ret;
}
struct resbuf *gsc_CdrAssoc(const TCHAR *stringa, presbuf lista, int sensitive)
   { return gsc_nth(1, gsc_assoc(stringa, lista, sensitive)); }


/*************************************************************************/
/*.doc gsc_RbSubst (external)*/  
/*+
  Riceve due parametri: un puntatore a resbuf e il nuovo valore che verrà inserito
  sostituendo quello vecchio
  Parametri:
  presbuf p_rb;      resbuf
  long    new_value;  nuovo valore da inserire

  La funzione ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_RbSubst(presbuf p_rb, long new_value)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTLONG;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTLONG;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTLONG;
         break;
      case RTLONG: break;
      default:
         p_rb->restype = RTLONG;
   }                  

   p_rb->resval.rlong = new_value;

   return GS_GOOD;
}
int gsc_RbSubst(presbuf p_rb, int new_value)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTSHORT;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTSHORT;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTSHORT;
         break;
      case RTSHORT: break;
      default:
         p_rb->restype = RTSHORT;
   }                  

   p_rb->resval.rint = new_value;

   return GS_GOOD;
}
int gsc_RbSubst(presbuf p_rb, double new_value)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTREAL;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTREAL;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTREAL;
         break;
      case RTREAL: break;
      default:
         p_rb->restype = RTREAL;
   }                  

   p_rb->resval.rreal = new_value;

   return GS_GOOD;
}
int gsc_RbSubst(presbuf p_rb, const char *new_value)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(new_value);  
   int   res = gsc_RbSubst(p_rb, UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int gsc_RbSubst(presbuf p_rb, const TCHAR *new_value)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTSTR;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTSTR;
         break;
      default:
         p_rb->restype = RTSTR;
   }                  

   if (!new_value) p_rb->restype = RTNIL;
   else
      if ((p_rb->resval.rstring = gsc_tostring(new_value)) == NULL) return GS_BAD;

   return GS_GOOD;
}
int gsc_RbSubstNIL(presbuf p_rb)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTNIL;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTNIL;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTNIL;
         break;
      case RTNIL: break;
      default:
         p_rb->restype = RTNIL;
   }                  

   return GS_GOOD;
}
int gsc_RbSubstT(presbuf p_rb)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTT;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTT;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTT;
         break;
      case RTT: break;
      default:
         p_rb->restype = RTT;
   }                  

   return GS_GOOD;
}
int gsc_RbSubstNONE(presbuf p_rb)
{
   if (!p_rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (p_rb->restype)
   {
      case RTPICKS: // gruppo di selezione
         ads_ssfree(p_rb->resval.rlname);
         p_rb->restype = RTNONE;
         break;
      case RTSTR: // stringa
         if (p_rb->resval.rstring) free(p_rb->resval.rstring);
         p_rb->restype = RTNONE;
         break;
      case 1004: // dato binario
         if (p_rb->resval.rbinary.buf) free(p_rb->resval.rbinary.buf);
         p_rb->restype = RTNONE;
         break;
      case RTNONE: break;
      default:
         p_rb->restype = RTNONE;
   }                  

   return GS_GOOD;
}
int gsc_RbSubst(presbuf p_rb, presbuf new_value)
{
   if (!new_value) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   switch (new_value->restype)
   {
      case RTLB: case RTLE:
         gsc_RbSubstNIL(p_rb); // rilascio memoria
         p_rb->restype = new_value->restype;
         return GS_GOOD;
      case RTLONG:
         return gsc_RbSubst(p_rb, new_value->resval.rlong);
      case RTSHORT:
         return gsc_RbSubst(p_rb, new_value->resval.rint);
      case RTREAL:
         return gsc_RbSubst(p_rb, new_value->resval.rreal);
      case RTSTR:
         return gsc_RbSubst(p_rb, new_value->resval.rstring);
      case RTNIL:
         return gsc_RbSubstNIL(p_rb);
      case RTNONE:
         return gsc_RbSubstNONE(p_rb);
      case RTT:
         return gsc_RbSubstT(p_rb);
   }

   return GS_BAD;
}


/*************************************************************************/
/*.doc gsc_CdrAssocSum                                        (external) */  
/*+
  Riceve tre parametri una lista di ricerca (lista) ed una chiave (stringa)
  da ricercare (indice) e il nuovo valore che verrà sommato al
  secondo elemento della coppia puntata (il primo=indice)
  Parametri:
  char    *stringa;   chiave di ricerca
  presbuf lista;      lista in cui cercare
  presbuf new_value;  puntatore resbuf nuovo valore da sommare

  La funzione ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_CdrAssocSum(char *stringa, presbuf lista, presbuf new_value)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(stringa);  
   int   res = gsc_CdrAssocSum(UnicodeString, lista, new_value);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int gsc_CdrAssocSum(TCHAR *stringa, presbuf lista, presbuf new_value)
{
   presbuf p;
   double  source;

   if (!stringa || !lista) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if ((p = gsc_assoc(stringa, lista)) == NULL) return GS_BAD;
   if ((p = gsc_nth(1, p)) == NULL) return GS_BAD;

   switch (new_value->restype)
   {
      case RTREAL: case RTANG:
         source = new_value->resval.rreal;
         break;
      case RTSHORT:
         source = (double) new_value->resval.rint;
         break;
      case RTLONG:
         source = (double) new_value->resval.rlong;
         break;
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }

   switch (p->restype)
   {
      case RTREAL: case RTANG:
         p->resval.rreal += source;
         break;
      case RTSHORT:
         p->resval.rint += (int) source;
         break;
      case RTLONG:
         p->resval.rlong += (long) source;
         break;
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_CdrAssocSub                                        (external) */  
/*+
  Riceve tre parametri una lista di ricerca (lista) ed una chiave (stringa)
  da ricercare (indice) e il nuovo valore che verrà sottratto al
  secondo elemento della coppia puntata (il primo=indice)
  Parametri:
  char    *stringa;   chiave di ricerca
  presbuf lista;      lista in cui cercare
  presbuf new_value;  puntatore resbuf nuovo valore da sottrarre

  La funzione ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_CdrAssocSub(char *stringa, presbuf lista, presbuf new_value)
{
   TCHAR *UnicodeString = gsc_CharToUnicode(stringa);  
   int   res = gsc_CdrAssocSub(UnicodeString, lista, new_value);
   if (UnicodeString) free(UnicodeString);

   return res;
}
int gsc_CdrAssocSub(TCHAR *stringa, presbuf lista, presbuf new_value)
{
   presbuf p;
   double  source;

   if (!stringa || !lista) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if ((p = gsc_assoc(stringa, lista)) == NULL) return GS_BAD;
   if ((p = gsc_nth(1, p)) == NULL) return GS_BAD;

   switch (new_value->restype)
   {
      case RTREAL: case RTANG:
         source = new_value->resval.rreal;
         break;
      case RTSHORT:
         source = (double) new_value->resval.rint;
         break;
      case RTLONG:
         source = (double) new_value->resval.rlong;
         break;
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }

   switch (p->restype)
   {
      case RTREAL: case RTANG:
         p->resval.rreal -= source;
         break;
      case RTSHORT:
         p->resval.rint -= (int) source;
         break;
      case RTLONG:
         p->resval.rlong -= (long) source;
         break;
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rbsearch (external)*/  
/*+
  Riceve due parametri una lista di ricerca (lista) ed un puntatore ad 
  una chiave da ricercare (indice) o resbuf o stringa o tipo.
  Parametri:
  presbuf indice;
  presbuf lista;
  int sensitive;  usato solo per stringhe; sensibile al maiuscolo (default = TRUE)

  La funzione ritorna il puntatore al primo elemento trovato o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf gsc_rbsearch(presbuf indice, presbuf lista, int sensitive)
{
   presbuf buf = lista;
   
   if (!indice) return NULL;

   while (buf)
   {
      if (gsc_equal(buf, indice, sensitive) == GS_GOOD) return buf;
      buf = buf->rbnext;
   }

   return NULL;
}
presbuf gsc_rbsearch(char *stringa, presbuf lista, int sensitive)
{
   TCHAR         *UnicodeString = gsc_CharToUnicode(stringa);  
   struct resbuf *res = gsc_rbsearch(UnicodeString, lista, sensitive);
   if (UnicodeString) free(UnicodeString);

   return res;
}
presbuf gsc_rbsearch(TCHAR *stringa, presbuf lista, int sensitive)
{
   presbuf p_key, p_ret;

   if ((p_key = acutBuildList(RTSTR, stringa, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
      
   p_ret = gsc_rbsearch(p_key, lista, sensitive);
      
   acutRelRb(p_key);

   return p_ret;
}


/*************************************************************************/
/*.doc gsc_rbsearch (external)*/  
/*+
  Riceve due parametri una lista di ricerca (lista) ed un numero intero
  indicante il tipo di valore nel resbuf (da usare su liste ottenute con ads_entget)
  La funzione ritorna il puntatore al primo elemento trovato o NULL nel 
  caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf gsc_rbsearch(int type, presbuf lista)
{
   presbuf buf = lista;
   
   if (buf == NULL) return NULL;
   do
   {
      if (buf->restype==type) return buf;
   }
   while ((buf = buf->rbnext) != NULL);

   return NULL;
}

/*************************************************************************/
/*.doc gsc_EEDsearch (external)*/  
/*+
  Riceve due parametri una lista di ricerca (lista) ed una stringa contenente
  il nome di una applicazione. 
  La funzione ritorna il puntatore all'inizio dell'EED corrispondente o NULL 
  nel caso di errore nei parametri o nel caso non trovi la chiave di ricerca.
-*/
/*************************************************************************/
presbuf gsc_EEDsearch(char* str, presbuf lista)
{
   TCHAR         *UnicodeString = gsc_CharToUnicode(str);  
   struct resbuf *res = gsc_EEDsearch(UnicodeString, lista);
   if (UnicodeString) free(UnicodeString);

   return res;
}
presbuf gsc_EEDsearch(TCHAR *str, presbuf lista)
{
   presbuf buf;
   
   if ((buf = lista) == NULL || str == NULL) return NULL;

   do
   {
      if (buf->restype == 1001)
         if (buf->resval.rstring != NULL && gsc_strcmp(buf->resval.rstring, str) == 0)
            return buf;
   }
   while ((buf = buf->rbnext) != NULL);

   return NULL;
}


/*************************************************************************/
/*.doc gsc_rbforlsp (external)*/  
/*+
  Riceve un resbuf e lo cambia togliendo il primo e l'ultimo elemento se 
  questi sono rispettivamente RTLB (aperta tonda) e RTLE (chiusa tonda).
-*/
/*************************************************************************/
presbuf gsc_rbforlsp(presbuf *rb)
{       
   if (rb == NULL) return NULL;
   
   if ((*rb) != NULL && (*rb)->restype == RTLB)  // tolgo prima e ultima tonda
   {
      presbuf p1, p2;

      p1 = *rb;              
      *rb = (*rb)->rbnext;
      p1->rbnext = NULL;
      acutRelRb(p1); // tolta prima aperta tonda
      p2 = *rb;
      p1 = NULL;
      if (p2 != NULL)
      {
         while (p2->rbnext != NULL) { p1 = p2; p2 = p2->rbnext; }
         if (p2->restype == RTLE)
         {  
            acutRelRb(p2); // tolta ultima chiusa tonda
            if (p1 == NULL) *rb = NULL;
            else p1->rbnext = NULL;
         }
      }
   }
   
   return *rb;
}


/*************************************************************************/
/*.doc gsc_lspforrb (external)*/  
/*+
  Riceve un resbuf e lo cambia aggiungendo al primo e l'ultimo elemento
  rispettivamente RTLB (aperta tonda) e RTLE (chiusa tonda).
-*/
/*************************************************************************/
presbuf gsc_lspforrb(presbuf *rb)
{       
   if (rb == NULL) return NULL;
   
   if ((*rb) != NULL)  // aggiungo aperta tonda
   {
      presbuf p;

      if ((p = acutBuildList(RTLB, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      p->rbnext = *rb;
      (*rb) = p;              
      while (p->rbnext != NULL) p = p->rbnext;

      if ((p->rbnext = acutBuildList(RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   }
   
   return *rb;
}


/*************************************************************************/
/*.doc gsc_str2rb                                             <internal> */  
/*+
  La funzione copia una stringa in un resbuf. Se la stringa è nulla il resbuf 
  sarà di tipo RTNIL altrimenti sarà di tipo RTSTR.
  N.B.: Alloca memoria
-*/
/*************************************************************************/
presbuf gsc_str2rb(const char *str)
{
   TCHAR         *UnicodeString = gsc_CharToUnicode(str);  
   struct resbuf *res = gsc_str2rb(UnicodeString);
   if (UnicodeString) free(UnicodeString);

   return res;
}
presbuf gsc_str2rb(C_STRING &str)
{
   return gsc_str2rb(str.get_name());
}
presbuf gsc_str2rb(const TCHAR *str)
{
   presbuf p;

   if (!str)
   {
      if ((p = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   }
   else
      if ((p = acutBuildList(RTSTR, str, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

   return p;
}


/*************************************************************************/
/*.doc gsc_rb2str                                             <internal> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in stringa.
  Per elementi di tipo : RTSHORT, RTREAL, RTLONG e RTSTR la funzione 
  ritorna il puntatore ad una stringa contenente la conversione,
  per i restanti tipi ritorna NULL.
  (WARNING: Alloca memoria!!)
-*/
/*************************************************************************/
TCHAR *gsc_rb2str(presbuf rb)
{
   TCHAR *str;

   if (rb==NULL) return NULL;

   switch(rb->restype)
   {
      case RTREAL :
         str = gsc_tostring(rb->resval.rreal);
         break;
      case RTSHORT :
         str = gsc_tostring(rb->resval.rint);
         break;
      case RTLONG :
         str = gsc_tostring(rb->resval.rlong);
         break;
      case RTSTR :
         str = gsc_tostring(rb->resval.rstring);
         break;      
      case RTPOINT :
      {
         TCHAR *dummy;

         if ((str = gsc_tostring(rb->resval.rpoint[X])) == NULL) break;
         if ((str = gsc_strcat(str, _T(","))) == NULL) break;
         if ((dummy = gsc_tostring(rb->resval.rpoint[Y])) == NULL) break;
         if ((str = gsc_strcat(str, dummy)) == NULL) { free(dummy); break; }
         free(dummy);
         if ((str = gsc_strcat(str, _T(",0"))) == NULL) break;

         break;
      }
      case RT3DPOINT :
         str = gsc_tostring(rb->resval.rpoint);
         break;
      case RTT :
         str = gsc_tostring(_T("1"));
         break;
      case RTNIL: case RTNONE:
         str = gsc_tostring(GS_EMPTYSTR);
         break;
      default :
         return NULL;
   }                              
   return str;
}
TCHAR *gsc_rb2str(presbuf rb, int digits, int dec)
{
   TCHAR *str;

   if (rb==NULL) return NULL;

   switch(rb->restype)
   {
      case RTREAL :
         str=gsc_tostring(rb->resval.rreal, digits, dec);
         break;
      case RTSHORT :
         str=gsc_tostring(rb->resval.rint);
         break;
      case RTLONG :
         str=gsc_tostring(rb->resval.rlong);
         break;
      case RTSTR :
         str=gsc_tostring(rb->resval.rstring);
         break;      
      case RTPOINT :
      case RT3DPOINT :
         if ((str = gsc_tostring(rb->resval.rpoint, digits, dec)) == NULL) break;
         break;
      default :
         return NULL;
   }

   return str;
}


/*************************************************************************/
/*.doc gsc_rb2Int                                             <external> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in numero intero.
  Per elementi di tipo : RTSHORT, RTREAL, RTLONG, RTANG, RTORINT.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rb2Int(presbuf rb, int *n)
{
   if (!rb || !n) return GS_BAD;

   switch(rb->restype)
   {
      case RTREAL:
      case RTANG:
      case RTORINT:
         *n = (int) rb->resval.rreal;
         break;
      case RTLONG:
         *n = (int) rb->resval.rlong;
         break;
      case RTSHORT:
         *n = rb->resval.rint;
         break;
      case RTSTR:
      {
         if ((*n = _wtoi(rb->resval.rstring)) == 0) // potrebbe essere un errore
            if (gsc_strcmp(rb->resval.rstring, _T("0")) != 0 &&
                gsc_strcmp(rb->resval.rstring, _T("0.0")) != 0)
               return GS_BAD;
         break;
      }
      case RTT:
         *n = 1;
         break;
      case RTNIL:
         *n = 0;
         break;
      default :
         return GS_BAD;
   }                              
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rb2Lng                                             <external> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in numero long.
  Per elementi di tipo : RTSHORT, RTREAL, RTLONG, RTANG, RTORINT.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rb2Lng(presbuf rb, long *n)
{
   if (!rb || !n) return GS_BAD;

   switch(rb->restype)
   {
      case RTREAL:
      case RTANG:
      case RTORINT:
         *n = (long) rb->resval.rreal;
         break;
      case RTLONG:
         *n = rb->resval.rlong;
         break;
      case RTSHORT:
         *n = (long) rb->resval.rint;
         break;
      case RTSTR:
      {
         if ((*n = _wtol(rb->resval.rstring)) == 0) // potrebbe essere un errore
            if (gsc_strcmp(rb->resval.rstring, _T("0")) != 0 &&
                gsc_strcmp(rb->resval.rstring, _T("0.0")) != 0)
               return GS_BAD;
         break;
      }
      case RTT:
         *n = 1;
         break;
      case RTNIL:
         *n = 0;
         break;
      default :
         return GS_BAD;
   }                              
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rb2Dbl                                             <external> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in numero double.
  Per elementi di tipo : RTSHORT, RTREAL, RTLONG, RTANG, RTORINT.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rb2Dbl(presbuf rb, double *n)
{
   if (!rb || !n) return GS_BAD;

   switch(rb->restype)
   {
      case RTREAL: case RTANG: case RTORINT:
         *n = rb->resval.rreal;
         break;
      case RTLONG:
         *n = rb->resval.rlong;
         break;
      case RTSHORT:
         *n = rb->resval.rint;
         break;
      case RTSTR:
      {          
         if ((*n = _wtof(rb->resval.rstring)) == 0.0) // potrebbe essere un errore
            if (gsc_strcmp(rb->resval.rstring, _T("0")) != 0 &&
                gsc_strcmp(rb->resval.rstring, _T("0.0")) != 0)
               return GS_BAD;
         break;
      }
      case RTT: // true
         *n = 1;
         break;
      case RTNIL: // false
         *n = 0;
         break;
      default :
         return GS_BAD;
   }                              
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rb2Pt                                              <external> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in ads_point.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rb2Pt(presbuf rb, ads_point pt)
{
   if (!rb || !pt) return GS_BAD;

   switch (rb->restype)
   {
      case RTPOINT:
         ads_2Dpoint_set(rb->resval.rpoint, pt);
         break;
      case RT3DPOINT:
         ads_point_set(rb->resval.rpoint, pt);
         break;
      default :
         return GS_BAD;
   }                              
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rb2Bool                                             <external> */  
/*+
  La funzione riceve un puntatore ad un elemento 'resbuf' ed opera la 
  conversione del suo contenuto in boolean.
  Per elementi di tipo : RTT, RTNIL, RTNONE, RTVOID, RTSHORT, RTREAL, RTLONG, RTANG, RTORINT.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rb2Bool(presbuf rb, bool *n)
{
   if (!rb || !n) return GS_BAD;

   switch (rb->restype)
   {
      case RTT:
         *n = true;
         break;
      case RTNIL: case RTNONE: case RTVOID:
         *n = false;
         break;
      case RTSHORT:
         *n = (rb->resval.rint) ? true : false;
         break;
      case RTLONG:
         *n = (rb->resval.rlong) ? true : false;
         break;
      case RTREAL: case RTANG: case RTORINT:
         *n = (rb->resval.rreal) ? true : false;
         break;
      case RTSTR:
      {
         int IsTrue;

         gsc_conv_Bool(rb->resval.rstring, &IsTrue);
         *n = (IsTrue) ? true : false;
         break;
      }
      default:
         GS_ERR_COD = eGSInvRBType;
         return GS_BAD;
   }
   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_rbExeMathOp                                             <external> */  
/*+
  La funzione esegue un calcolo matematico modificando il valore numerico
  rbVal sommandolo ("+"), sottraendolo ("_"), moltiplicandolo ("*") o
  dividendolo ("/") di una quantità che puo' essere assoluta o
  in percentuale del valore originale di rbVal.
  Per elementi di tipo : RTREAL, RTANG, RTORINT, RTLONG, RTSHORT, RTNIL, RTVOID.
  Parametri:
  presbuf     rbVal;  Valore da modificare
  presbuf     rbQty;  Quantità da sommare, sottrarre, moltiplicare o dividere
  const TCHAR *Op;    Operazione da eseguire "+", "-", "*", "/"
  bool        Perc;   Se TRUE la quantità indicata da rbQty è in percentuale di rbVal
                     (default = FALSE)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*************************************************************************/
int gsc_rbExeMathOp(presbuf rbVal, presbuf rbQty, const TCHAR *Op, bool Perc)
{
   double NumVal = 0, NumQty = 0;

   if (!rbVal || !rbQty) return GS_BAD;

   if (rbVal->restype != RTNIL && rbVal->restype != RTVOID && rbVal->restype != RTNONE)
      if (gsc_rb2Dbl(rbVal, &NumVal) == GS_BAD) return GS_BAD;
   
   if (rbQty->restype != RTNIL && rbQty->restype != RTVOID && rbQty->restype != RTNONE)
      if (gsc_rb2Dbl(rbQty, &NumQty) == GS_BAD) return GS_BAD;

   // Se deve essere una quantità in percentuale del valore originario
   if (Perc) NumQty = NumVal * NumQty / 100;
   
   if      (gsc_strcmp(Op, _T("+")) == 0) NumVal += NumQty;
   else if (gsc_strcmp(Op, _T("-")) == 0) NumVal -= NumQty;
   else if (gsc_strcmp(Op, _T("*")) == 0) NumVal *= NumQty;
   else if (gsc_strcmp(Op, _T("/")) == 0) NumVal /= NumQty;

   return gsc_RbSubst(rbVal, NumVal);
}


/*************************************************************************/
/*.doc gsc_printlist(external)
/*+
   Stampa il contenuto di una lista di 'resbuf'.
-*/
/*************************************************************************/

void gsc_printlist(const struct resbuf *a)
{
   acutPrintf(_T("\nLISTA ->   "));
   while(a!=NULL)
   {
      switch (a->restype)
      {
         case RTLB:
            acutPrintf(_T("("));
            break;
         case RTLE: 
            acutPrintf(_T(")\n"));
            break;
         case RTDOTE: 
            acutPrintf(_T(" ."));
            break;
         case RTT:
            acutPrintf(_T(" T"));
            break;
         case RTVOID: case RTNIL: case RTNONE:
            acutPrintf(_T(" NIL"));
            break;                 
         case RTPOINT:
            acutPrintf(_T(" (%.1lf|%.1lf)"), a->resval.rpoint[X], a->resval.rpoint[Y]);
            break;                    
         case RT3DPOINT:
            acutPrintf(_T(" (%.1lf|%.1lf|%.1lf)"), a->resval.rpoint[X], 
                       a->resval.rpoint[Y], a->resval.rpoint[Z]);
            break;          
         case RTREAL: case RTANG: case RTORINT:
            acutPrintf(_T(" %.1lf"), a->resval.rreal);
            break;
         case RTSHORT:
            acutPrintf(_T(" %d"), a->resval.rint);
            break;
         case RTSTR: case RTDXF0:
            acutPrintf(_T(" \"%s\""), a->resval.rstring);
            break;
         case RTLONG:
            acutPrintf(_T(" %ld"), a->resval.rlong);
            break;
         case RTENAME: case RTPICKS:
         default :
            acutPrintf(_T(" ERR(%d) "), a->restype);
            break;
      }
      a = a->rbnext;
   }

   acutPrintf(_T(" END\n"));
}


/*************************************************************************/
 
