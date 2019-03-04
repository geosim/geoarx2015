/**********************************************************
Name: GS_SELST.CPP
                                   
Module description. File funzioni di base per la gestione
                    dei gruppi di selezione 
            
Author: Roberto Poltini

(c) Copyright 1995-2012 by IREN ACQUA GAS S.p.A.

**********************************************************/


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdio.h>
#include <string.h>

#include "adslib.h"   
#include <dbents.h>
#include "aced.h"       // per acedUpdateDisplay
#include "acestext.h"   // per acadErrorStatusText
#include "acedCmdNF.h" // per acedCommandS e acedCmdS

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_selst.h"
#include "gs_list.h"
#include "gs_init.h" 
#include "gs_area.h"      // gestione sessioni di lavoro
#include "gs_graph.h" 
#include "gs_attbl.h"


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


C_SELSET GS_ALTER_FAS_OBJS; // gruppo di selezione degli oggetti a cui è stata
                            // variata la FAS (tramite evidenziazione risultato
                            // filtro o estrazione con FAS default/utente)
C_SELSET GS_SELSET;         // Gruppo di selezione di uso pubblico


//#if defined(GSDEBUG) // se versione per debugging
//   #include <sys/timeb.h>  // Solo per debug
//   #include <time.h>       // Solo per debug
//   double  tempo=0, tempo1=0, tempo2=0, tempo3=0, tempo4=0, tempo5=0, tempo6=0, tempo7=0;
//   double  tempo8=0, tempo9=0, tempo10=0, tempo11=0, tempo12=0, tempo13=0, tempo14=0;
//#endif



///////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI C_SELSET
///////////////////////////////////////////////////////////////////////////


C_SELSET::C_SELSET() : C_NODE()
{
   ads_name_clear(ss);
   mReleaseAllAtDistruction = GS_GOOD;
}


//-----------------------------------------------------------------------//
   
C_SELSET::~C_SELSET()
{
   if (mReleaseAllAtDistruction == GS_GOOD) clear();
}

//-----------------------------------------------------------------------//

void C_SELSET::ReleaseAllAtDistruction(int in) 
{ 
   mReleaseAllAtDistruction = in;
}

//-----------------------------------------------------------------------//

int C_SELSET::get_selection(ads_name sel)
{
   ads_name_set(ss, sel);

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::get_AcDbObjectIdArray(AcDbObjectIdArray &ObjIds, bool Append)
{
   AcDbObjectId ObjId;
   long         i = 0;
   ads_name     ent;

   // Se si vuole pulire ObjIds
   if (!Append) ObjIds.removeAll();

   // Ciclo sugli oggetti
   while (entname(i++, ent) == GS_GOOD)
   {
      if (acdbGetObjectId(ObjId, ent) != Acad::eOk) 
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      ObjIds.append(ObjId);
   }

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::get_AcDbEntityPtrArray(AcDbEntityPtrArray &EntitySet, bool Append, AcDb::OpenMode mode)
{
   AcDbObjectId ObjId;
   AcDbEntity   *pObj;
   long         i = 0;
   ads_name     ent;

   // Se si vuole pulire EntitySet
   if (!Append) EntitySet.removeAll();

   // Ciclo sugli oggetti
   while (entname(i++, ent) == GS_GOOD)
      if (acdbGetObjectId(ObjId, ent) == Acad::eOk &&
          acdbOpenObject(pObj, ObjId, mode) == Acad::eOk)
         EntitySet.append(pObj);

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::set_AcDbEntityPtrArray(AcDbEntityPtrArray &EntitySet)
{
   long     i;
   ads_name ent;

   clear();
   for (i = 0; i < EntitySet.length(); i++)
      if (acdbGetAdsName(ent, EntitySet[i]->id()) != Acad::eOk) return GS_BAD;
      else add(ent);

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::clear(void)
{
   if (!ads_name_nil(ss)) { acedSSFree(ss); ads_name_clear(ss); }

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::operator << (ads_name selset)
{
   if (!ads_name_nil(ss)) acedSSFree(ss);
   ads_name_set(selset, ss);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//

long C_SELSET::length()
{ 
	long len_sel_set; 

   if (ads_name_nil(ss)) return 0;
   if (acedSSLength(ss, &len_sel_set) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return 0; }

	return len_sel_set;
}

//-----------------------------------------------------------------------//

int C_SELSET::entname(long i, ads_name ent)
{ 
   if (ads_name_nil(ss) || acedSSName(ss, i, ent) != RTNORM) return GS_BAD;
	
	return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::is_member(ads_name ent)
{ 
   if (ads_name_nil(ss) || acedSSMemb(ent, ss) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
	
	return GS_GOOD;
}



/************************************************************************/
/*.doc (new 2) C_SELSET::is_presentGraphicalObject                      */
/*+                                                                       
  Questa funzione verifica che, nel gruppo di selezione, esistano oggetti grafici
  che non siano blocchi di attributi visibili o riempimenti.
  Nel caso affermativo setta il parametro graphObj al numero di 
  entità grafiche trovate.
  Parametri:
  long     graphObj;   numero di entità grafiche.

  Restituisce GS_GOOD se la funzione è andata a buon fine.
-*/  
/************************************************************************/
int C_SELSET::is_presentGraphicalObject(long *Qty)
{
   C_SELSET GraphObjs;   
   ads_name ent;
   long     ItemNum = 0;

   if (copy(GraphObjs) == GS_BAD || GraphObjs.intersectType(GRAPHICAL) == GS_BAD)
      return GS_BAD; 

   *Qty = 0;
   // scarto i riempimenti
   while (GraphObjs.entname(ItemNum++, ent) == GS_GOOD)
      if (gsc_ishatch(ent) == GS_BAD) (*Qty)++;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::add_DA                       <internal> */
/*+
  Questa funzione aggiunge i blocchi di attributi visibili per
  tutte le entità principali presenti nel gruppo di selezione.
  Parametri:

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::add_DA() 
{
   long     i = 0;
   ads_name ent;
   C_CLASS  *pCls;
   C_SELSET entSS;

   while (entname(i++, ent) == GS_GOOD)
      // Se la classe ha attributi visibili
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) != NULL &&
          pCls->ptr_attrib_list() && pCls->ptr_attrib_list()->is_visible())
      {
         // se non è un blocco attributi
         if (gsc_is_DABlock(ent) != GS_GOOD &&
            pCls->get_SelSet(ent, entSS, DA_BLOCK) == GS_GOOD)
            if (add_selset(entSS) != GS_GOOD) return GS_BAD;
      }
   return GS_GOOD;
}


int C_SELSET::save(const TCHAR *name)
{
   C_STRING path;
   TCHAR    hand[MAX_LEN_HANDLE];
   FILE     *file;
   long     len,i;
   ads_name nn;

   if (ads_name_nil(ss)) return GS_GOOD;

   if (!name)
   {
      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T('\\');
      path += GEOTEMPSESSIONDIR;
      path += _T('\\');
      path += GEO_SAVESS_FILE;
   }
   else path = name;

   if (ads_sslength(ss, &len) != RTNORM)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((file = gsc_fopen(path, _T("w"))) == NULL) return GS_BAD; 

   for (i = 0; i < len; i++)
   {
      if (acedSSName(ss, i, nn) != RTNORM)
         { gsc_fclose(file); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
      
      if (gsc_enthand(nn, hand) == GS_GOOD)
         if (fwprintf(file, _T(" %s"), hand) < 0)
            { gsc_fclose(file); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   gsc_fclose(file);
   
   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::load(const TCHAR *name)
{
   TCHAR     hand[MAX_LEN_HANDLE], format[20];
   C_STRING  path;
   FILE      *file;
   ads_name  nn;

   if (clear() == GS_BAD) return GS_BAD;

   if (acedSSAdd(NULL, NULL, ss) != RTNORM)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if (!name)
   {
      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T('\\');
      path += GEOTEMPSESSIONDIR;
      path += _T('\\');
      path += GEO_SAVESS_FILE;
   }
   else path = name;

   if (gsc_path_exist(path) == GS_BAD) return GS_GOOD;

   if ((file = gsc_fopen(path, _T("r"))) == NULL) return GS_BAD;

   swprintf(format, 20, _T("%%%ds"), MAX_LEN_HANDLE);
   while (fwscanf(file, format, hand) != EOF)
      if (acdbHandEnt(hand, nn) == RTNORM) acedSSAdd(nn, ss, ss);

   gsc_fclose(file);
   
   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::add(ads_name ent, int OnFile, FILE *fhandle)
{
   int result = GS_GOOD;

   if (ads_name_nil(ss))
      if (acedSSAdd(NULL, NULL, ss) != RTNORM)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if (acedSSMemb(ent, ss) == RTNORM) return GS_GOOD;
   if (acedSSAdd(ent, ss, ss) != RTNORM) 
   {
      GS_ERR_COD = eGSInvalidSelectionSet;
      return GS_BAD;
   } 

   if (OnFile == GS_GOOD)
   {
      C_STRING path;
      TCHAR    hand[MAX_LEN_HANDLE];
      FILE     *file;
      
      result = GS_BAD;

      if (fhandle) file = fhandle;
      else
      {
         path = GS_CURRENT_WRK_SESSION->get_dir();
         path += _T('\\');
         path += GEOTEMPSESSIONDIR;
         path += _T('\\');
         path += GEO_SAVESS_FILE;
         if ((file = gsc_fopen(path, _T("a"))) == NULL) return GS_BAD;
      }

      do
      {
         if (gsc_enthand(ent, hand) == GS_GOOD)
            if (fwprintf(file, _T(" %s"), hand) < 0) { GS_ERR_COD = eGSWriteFile; break; }

         result = GS_GOOD;
      }
      while (0);

      if (!fhandle)
         if (gsc_fclose(file) == GS_BAD) return GS_BAD;
   }

   return result;
}

//-----------------------------------------------------------------------//

int C_SELSET::add_selset(C_SELSET &selset, int OnFile, const TCHAR *name)
{
   ads_name dummy_ss;
   selset.get_selection(dummy_ss);
   return add_selset(dummy_ss, OnFile, name);
}
int C_SELSET::add_selset(ads_name selset, int OnFile, const TCHAR *name)
{
   long     i = 0;
   ads_name ent;

   if (OnFile == GS_GOOD)
   {
      C_STRING path;
      FILE     *file;

      if (!name)
      {
         path = GS_CURRENT_WRK_SESSION->get_dir();
         path += _T('\\');
         path += GEOTEMPSESSIONDIR;
         path += _T('\\');
         path += GEO_SAVESS_FILE;
      }
      else path = name;

      if ((file = gsc_fopen(path, _T("a"))) == NULL) return GS_BAD;

      while (acedSSName(selset, i++, ent) == RTNORM)
         if (add(ent, OnFile, file) == GS_BAD)
            { gsc_fclose(file); return GS_BAD; }

      if (gsc_fclose(file) == GS_BAD) return GS_BAD;
   }
   else
      while (acedSSName(selset, i++, ent) == RTNORM)
         if (add(ent) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

//-----------------------------------------------------------------------//

int C_SELSET::subtract(C_SELSET &SelSet) 
{
   ads_name dummy_ss;
   SelSet.get_selection(dummy_ss);
   return subtract(dummy_ss);
}
int C_SELSET::subtract(ads_name selset) 
{
   ads_name ent;
   long     i = 0;
   
   if (ads_name_nil(ss) || ads_name_nil(selset)) return GS_GOOD;
   while (acedSSName(selset, i++, ent) == RTNORM)
      acedSSDel(ent, ss);

   return GS_GOOD; 
} 

//-----------------------------------------------------------------------//

int C_SELSET::subtract(const TCHAR *GraphType) 
{
   long      ndx = 0;
   ads_name  ename;
   C_RB_LIST List;
   presbuf   p;

   while (acedSSName(ss, ndx, ename) == RTNORM)
   {
      if ((List << acdbEntGet(ename)) == NULL ||
          ((p = List.SearchType(0)) && wcsicmp(p->resval.rstring, GraphType) == 0))
         acedSSDel(ename, ss);
      else
         ndx++;
   }

   return GS_GOOD;
} 

//-----------------------------------------------------------------------//

int C_SELSET::subtract_ent(ads_name ent) 
{
   if (!ads_name_nil(ss)) acedSSDel(ent, ss);

   return GS_GOOD; 
} 

//-----------------------------------------------------------------------//

int C_SELSET::copy(ads_name out_selset) 
{
   ads_name ent;
   long     i = 0;

   acedSSAdd(NULL, NULL, out_selset);

   if (ads_name_nil(ss)) return GS_GOOD;

   while (acedSSName(ss, i++, ent) == RTNORM)
      acedSSAdd(ent, out_selset, out_selset);

   return GS_GOOD; 
} 

//-----------------------------------------------------------------------//

int C_SELSET::copy(C_SELSET &out_selset) 
{
   ads_name ent;
   long     i = 0;

   out_selset.clear();
   acedSSAdd(NULL, NULL, out_selset.ss);

   if (ads_name_nil(ss)) return GS_GOOD;

   while (acedSSName(ss, i++, ent) == RTNORM)
      acedSSAdd(ent, out_selset.ss, out_selset.ss);

   return GS_GOOD; 
} 


/*********************************************************/
/*.doc C_SELSET::copyIntersectType            <internal> */
/*+
  Questa funzione copia in un C_SELSET solo le entità specificate
  dal parametro <What>.
  Parametri:
  C_SELSET &out_selset;
  int What;             GRAPHICAL per gli oggetti grafici
                        DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::copyIntersectType(C_SELSET &out_selset, int What) 
{
   ads_name ent;
   long     i = 0;
   C_RB_LIST List;

   out_selset.clear();
   acedSSAdd(NULL, NULL, out_selset.ss);

   if (ads_name_nil(ss)) return GS_GOOD;

   switch (What)
   {
      case GRAPHICAL: // Filtro il SelSet
      {
         while (acedSSName(ss, i++, ent) == RTNORM)
            if ((List << acdbEntGet(ent)) == NULL ||
                gsc_is_DABlock(List.get_head()) != GS_GOOD)
               acedSSAdd(ent, out_selset.ss, out_selset.ss);
         break;
      }
      case DA_BLOCK: // Filtro il SelSet
      {
         while (acedSSName(ss, i++, ent) == RTNORM)
            if ((List << acdbEntGet(ent)) == NULL ||
                gsc_is_DABlock(List.get_head()) == GS_GOOD)
               acedSSAdd(ent, out_selset.ss, out_selset.ss);
         break;
      }
   }

   return GS_GOOD; 
} 


/*********************************************************/
/*.doc C_SELSET::copyIntersectClsCode          <internal> */
/*+
  Questa funzione copia in un C_SELSET gli oggetti che appartengono
  ad una certa classe di GEOsim.
  Parametri:
  C_SELSET &out_selset; Gruppo di selezione in output
  int cls;              Codice classe
  int sub;              Codice sottoclasse
  bool ErasedObject;    Opzionale, Flag per considerare o meno gli
                        oggetti cancellati (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::copyIntersectClsCode(C_SELSET &out_selset, int cls, int sub,
                                   bool ErasedObject) 
{
   ads_name ent;
   long     i = 0;
   C_EED    eed;

   out_selset.clear();
   acedSSAdd(NULL, NULL, out_selset.ss);

   if (ads_name_nil(ss)) return GS_GOOD;

   if (!ErasedObject) // se non devono essere considerati gli oggetti cancellati
   {
      while (acedSSName(ss, i++, ent) == RTNORM)
         if (eed.load(ent) == GS_GOOD && eed.cls == cls && eed.sub == sub)
            acedSSAdd(ent, out_selset.ss, out_selset.ss);
   }
   else
      // Creo il selection set filtrando da SelSet tutti gli oggetti della classe
      // considerando anche quelli cancellati (se non sono nuovi)
      while (acedSSName(ss, i++, ent) == RTNORM)
		   if (gsc_IsErasedEnt(ent) == GS_GOOD)  	// se l'oggetto non esiste
         {
            if (gsc_UnEraseEnt(ent) != GS_GOOD) return GS_BAD; // lo rispristino

            if (eed.load(ent) == GS_GOOD && eed.cls == cls && eed.sub == sub)
               acedSSAdd(ent, out_selset.ss, out_selset.ss);

            if (gsc_EraseEnt(ent) != GS_GOOD) return GS_BAD; // lo ricancello
         }
         else
            if (eed.load(ent) == GS_GOOD && eed.cls == cls && eed.sub == sub)
               acedSSAdd(ent, out_selset.ss, out_selset.ss);

   return GS_GOOD; 
} 


/*********************************************************/
/*.doc C_SELSET::intersectType                <internal> */
/*+
  Questa funzione filtra dal SelSet solo le entità specificate
  dal parametro <What>.
  Parametri:
  int What;   GRAPHICAL per gli oggetti grafici
              DA_BLOCK  per i blocchi degli attributi visibili

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersectType(int What) 
{
   long      ndx = 0;
   ads_name  ename;
   C_RB_LIST List;

   switch (What)
   {
      case GRAPHICAL: // Filtro il SelSet
      {
         while (acedSSName(ss, ndx, ename) == RTNORM)
         {
            if ((List << acdbEntGet(ename)) == NULL ||
                gsc_is_DABlock(List.get_head()) == GS_GOOD)
               acedSSDel(ename, ss);
            else
               ndx++;
         }
         break;
      }
      case DA_BLOCK: // Filtro il SelSet
      {
         while (acedSSName(ss, ndx, ename) == RTNORM)
         {
            if ((List << acdbEntGet(ename)) == NULL ||
                gsc_is_DABlock(List.get_head()) != GS_GOOD)
               acedSSDel(ename, ss);
            else
               ndx++;
         }
         break;
      }
   }
   
   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::intersectVisibile            <internal> */
/*+
  Questa funzione filtra dal SelSet solo le entità che sono su layer visibili.
  Parametri:

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersectVisibile(void) 
{
   long     ndx = 0;
   ads_name ename;
   C_STRING Layer;

   while (acedSSName(ss, ndx, ename) == RTNORM)
      if (gsc_getLayer(ename, Layer) == GS_GOOD &&
          (gsc_is_offLayer(Layer.get_name()) == GS_GOOD || gsc_is_freezeLayer(Layer.get_name()) == GS_GOOD))
         acedSSDel(ename, ss);
      else
         ndx++;
   
   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::intersectClsCode             <internal> */
/*+
  Questa funzione filtra dal SelSet solo le entità di una data classe 
  e sottoclasse.
  Parametri:
  int cls;        Codice classe
  int sub;        Codice sottoclasse opzionale (default = 0)
  int What;       Flag di filtro (default = ALL)
                  GRAPHICAL per gli oggetti grafici
                  DA_BLOCK  per i blocchi degli attributi visibili

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersectClsCode(int cls, int sub, int What) 
{
   long      ndx = 0;
   ads_name  ename;
   C_RB_LIST List;
   presbuf   p;
   C_EED     eed;

   while (acedSSName(ss, ndx, ename) == RTNORM)
   {
      if ((List << ads_entgetx(ename, GEOsimAppl::APP_ID_LIST.get_head())) == NULL ||
          (p = List.SearchType(-3)) == NULL ||           // cerco intestazione EED
          (p = gsc_EEDsearch(GEO_APP_ID, p)) == NULL ||  // cerco l'etichetta di GEOsim
          eed.load(p) != GS_GOOD || eed.cls != cls)      // carico i dati
         acedSSDel(ename, ss); 
      else       
         if (sub != 0 && eed.sub != sub)
            acedSSDel(ename, ss);
         else
            switch (What)
            {
               case GRAPHICAL:
                  if (gsc_is_DABlock(List.get_head()) == GS_GOOD) acedSSDel(ename, ss);
                  else ndx++;
                  break;
               case DA_BLOCK:
                  if (gsc_is_DABlock(List.get_head()) != GS_GOOD) acedSSDel(ename, ss);
                  else ndx++;
                  break;
               default: // ALL
                  ndx++;
            }
   }
   
   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::intersectClsCodeList             <internal> */
/*+
  Questa funzione filtra dal SelSet solo le entità di una lista di classi
  e sottoclassi.
  Parametri:
  C_INT_INT_LIST &ClsSubList; Lista codici classe e sottoclasse
  int What;                   Flag di filtro (default = ALL)
                              GRAPHICAL per gli oggetti grafici
                              DA_BLOCK  per i blocchi degli attributi visibili

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersectClsCodeList(C_INT_INT_LIST &ClsSubList, int What) 
{
   long      ndx = 0;
   ads_name  ename;
   C_RB_LIST List;
   presbuf   p;
   C_EED     eed;

   while (acedSSName(ss, ndx, ename) == RTNORM)
   {
      if ((List << ads_entgetx(ename, GEOsimAppl::APP_ID_LIST.get_head())) == NULL ||
          (p = List.SearchType(-3)) == NULL ||           // cerco intestazione EED
          (p = gsc_EEDsearch(GEO_APP_ID, p)) == NULL ||  // cerco l'etichetta di GEOsim
          eed.load(p) != GS_GOOD || !ClsSubList.search(eed.cls, eed.sub)) // carico i dati
         acedSSDel(ename, ss); 
      else       
         switch (What)
         {
            case GRAPHICAL:
               if (gsc_is_DABlock(List.get_head()) == GS_GOOD) acedSSDel(ename, ss);
               else ndx++;
               break;
            case DA_BLOCK:
               if (gsc_is_DABlock(List.get_head()) != GS_GOOD) acedSSDel(ename, ss);
               else ndx++;
               break;
            default: // ALL
               ndx++;
         }
   }
   
   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::get_ClsCodeList              <external> */
/*+
  Questa funzione crea una lista delle classi e sottoclassi 
  contenute nel gruppo di selezione.
  Parametri:
  C_INT_INT_LIST &ClsSubList; Lista codici classe e sottoclasse
  bool *UnknownObjs;          Flag opzionale per sapere se nel gruppo di
                              selezione sono presenti oggetti che NON
                              appartengono a GEOsim (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::get_ClsCodeList(C_INT_INT_LIST &ClsSubList, bool *UnknownObjs) 
{
   long      ndx = 0;
   ads_name  ename;
   C_EED     eed;
   C_INT_INT *pClsSub;

   if (UnknownObjs) *UnknownObjs = false;

   ClsSubList.remove_all();
   while (acedSSName(ss, ndx++, ename) == RTNORM)
      if (eed.load(ename) == GS_GOOD)
      {
         if (!ClsSubList.search(eed.cls, eed.sub))
         {
            if ((pClsSub = new C_INT_INT(eed.cls, eed.sub)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            ClsSubList.add_tail(pClsSub);
         }
      }
      else if (UnknownObjs) *UnknownObjs = true;
   
   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::intersect                    <internal> */
/*+
  Questa funzione filtra dal SelSet solo le entità presenti
  nel gruppo di selezione indicato dal parametro <selset>.
  Parametri:
  int selset;   gruppo di selezione

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersect(ads_name selset) 
{
   long     ndx = 0;
   ads_name ename;

   while (acedSSName(ss, ndx, ename) == RTNORM)
   {
      if (acedSSMemb(ename, selset) == RTERROR)
         acedSSDel(ename, ss);
      else
         ndx++;
   }
   
   return GS_GOOD;
} 
int C_SELSET::intersect(C_SELSET &SelSet) 
{
   ads_name dummy_ss;
   SelSet.get_selection(dummy_ss);
   return intersect(dummy_ss);
}


/*********************************************************/
/*.doc C_SELSET::intersectUpdateable          <internal> */
/*+
  Questa funzione filtra dal gruppo di selezione gli oggetti
  che hanno come caratteristica lo stato passato in input
  come parametro.
  Parametri:
  int State;          se = UPDATEABLE filtra solo gli oggetti aggiornabili
                      altrimenti solo gli oggetti bloccati default (UPDATEABLE)
  int set_lock;       flag; se = GS_GOOD la funzione prova a bloccare le entità
                      aggiornabili (se <State> = UPDATEABLE; default = GS_GOOD)
                      estraendo totalmente le entità estratte parzialmente.
  int CounterToVideo; Flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  N.B. Gli oggetti che non sono si GEOsim sono sempre aggiornabili !!

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::intersectUpdateable(int State, int set_lock, int CounterToVideo)
{
   C_SELSET SelSet, ObjConsidered, ObjToKeep;
   C_CLASS  *pCls = NULL;
   C_EED    eed;
   long     ndx = 0,Key, i = 0;
   int      ToLock, TryToExtractPartialEnt;
   ads_name ent;
   C_STRING Title;

   TryToExtractPartialEnt = ToLock = (State == UPDATEABLE && set_lock == GS_GOOD) ? GS_GOOD : GS_BAD;

   if (ToLock == GS_GOOD)
      Title = gsc_msg(850); // "Verifica modificabilità e blocco delle entità"
   else
      Title = gsc_msg(850); // "Verifica modificabilità delle entità"

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(length());

   // Scorro il gruppo di selezione oggetto per oggetto
   while (acedSSName(ss, ndx, ent) == RTNORM)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i++);

      // Oggetto NON di GEOsim (quindi aggiornabile)
      if (eed.load(ent) == GS_BAD)
      {
         if (State != UPDATEABLE) subtract_ent(ent);
         else ndx++;
      }
      else
      {
         // Ricavo il puntatore alla classe dell' oggetto in questione
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return GS_BAD;
         if (pCls->ptr_info()) // l'oggetto ha collegamento con DB
         {
            
//#if defined(GSDEBUG) // se versione per debugging
//   struct _timeb t1, t2, t3;
//   _ftime(&t1);
//#endif

            if (pCls->get_Key_SelSet(ent, &Key, SelSet) == GS_BAD) return GS_BAD;

//#if defined(GSDEBUG) // se versione per debugging
//   _ftime(&t2);
//   tempo1 += (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
//#endif

            // Entità GEOsim aggiornabile
            if (pCls->is_updateableSS(Key, SelSet, NULL, ToLock, TryToExtractPartialEnt) == GS_GOOD)
            {
               if (State != UPDATEABLE) { subtract(SelSet); i -= (SelSet.length() - 1); }
               else ndx++;
            }
            else
               if (State == UPDATEABLE) { subtract(SelSet); i -= (SelSet.length() - 1); }
               else ndx++;

//#if defined(GSDEBUG) // se versione per debugging
//   _ftime(&t3);
//   tempo2 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
//#endif

         }
         else // spaghetti
            if (pCls->is_updateable(ent, NULL, ToLock) == GS_GOOD)
            {
               if (State != UPDATEABLE) subtract_ent(ent);
               else ndx++;
            }
            else
               if (State == UPDATEABLE) subtract_ent(ent);
               else ndx++;
      }
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::to_rb                    <internal> */
/*+
  Questa funzione restituisce il gruppo di selezione in formato
  di lista di resbuf;

  Ritorna la lista di resbuf in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
presbuf C_SELSET::to_rb(void) 
{
   long      ndx = 0;
   ads_name  ename;
   C_RB_LIST EntList;

   while (acedSSName(ss, ndx++, ename) == RTNORM)
      if ((EntList += acutBuildList(RTENAME, ename, 0)) == NULL) 
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   EntList.ReleaseAllAtDistruction(GS_BAD);

   return EntList.get_head();
} 


/*********************************************************/
/*.doc C_SELSET::redraw                       <internal> */
/*+
  Questa funzione ridisegna il gruppo di selezione a seconda della modalità
  scelta.
  Parametri:
  int mode;    Modalità di ridisegno (1 = ridisegna, 2 = invisibile,
                                      3 = evidenzia, 4 = toglie evidenziazione)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::redraw(int mode) 
{
   long     ndx = 0;
   ads_name ent;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      acedRedraw(ent, mode);
      
   acedUpdateDisplay();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::DrawCross                    <internal> */
/*+
  Questa funzione disegna una crocetta in modalità grdraw
  su ciascun oggetto.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::DrawCross(void) 
{
   long      ndx = 0;
   ads_name  ent;
   ads_point pt;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      if (gsc_get_firstPoint(ent, pt) == GS_GOOD)
         gsc_GrDrawCross(pt);

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::getWindow                    <internal> */
/*+
  Questa funzione ricava la finestra contenente tutte le entità 
  del gruppo di selezione.
  Parametri:
  ads_real min_dim_x;   dimensione minima della finestra sull'asse x (default = 0)
  ads_real min_dim_y;   dimensione minima della finestra sull'asse y (default = 0)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::getWindow(ads_point corner1, ads_point corner2) 
{
   ads_name _selset;
   get_selection(_selset);
   return gsc_get_window(_selset, corner1, corner2);
} 


/*********************************************************/
/*.doc C_SELSET::zoom                       <internal> */
/*+
  Questa funzione effettua lo zoom su tutte le entità del gruppo di selezione.
  Parametri:
  ads_real min_dim_x;   dimensione minima della finestra sull'asse x (default = 0)
  ads_real min_dim_y;   dimensione minima della finestra sull'asse y (default = 0)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::zoom(ads_real min_dim_x, ads_real min_dim_y) 
{
   ads_point corner1, corner2;

   if (gsc_get_window(ss, corner1, corner2) == GS_GOOD)
   {
      gsc_zoom(corner1, corner2, min_dim_x, min_dim_y);
      return GS_GOOD;
   }
   else
      return GS_BAD;
} 


/*********************************************************/
/*.doc C_SELSET::UnErase                      <internal> */
/*+
  Questa funzione ripristina il gruppo di selezione.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::UnErase(void) 
{
   long     ndx = 0;
   ads_name ent;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      if (gsc_UnEraseEnt(ent) != GS_GOOD) return GS_BAD; // cancello\ripristino oggetto

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::Erase                        <internal> */
/*+
  Questa funzione cancella il gruppo di selezione.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::Erase(void) 
{
   long     ndx = 0;
   ads_name ent;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      if (gsc_EraseEnt(ent) != GS_GOOD) return GS_BAD; // cancello\ripristino oggetto

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::get_SpatialIntersectWindowSS <external> */
/*+
  Questa funzione ricava un nuovo gruppo di selezione di oggetti
  che intersecano la finestra indicata dal punto p1 (angolo in basso a sinistra)
  e p2 (angolo in alto a destra).
  Parametri:
  ads_point p1;       punto inf. sx.
  ads_point p2;       punto inf. dx.
  C_SELSET &out;      Gruppo di oggetti intersecanti la finestra
  int CheckOn2D;      Controllo oggetti senza usare la Z (default = GS_GOOD)
  C_HANDLERECT_BTREE *pHandleRectTree; se <> NULL viene usato per cercare le estensioni
                                       degli oggetti e se non trovati vengono aggiunti
                                       (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::get_SpatialIntersectWindowSS(ads_point p1, ads_point p2, C_SELSET &out, int CheckOn2D,
                                           C_HANDLERECT_BTREE *pHandleRectTree,
                                           int CounterToVideo) 
{
   long      ndx = 0, i = 0;
   ads_name  ent;
   ads_point corner1, corner2;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1071)); // "Calcolo delle intersezioni spaziali"

   out.clear();

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(length());

   if (pHandleRectTree)
   {
      TCHAR         Handle[MAX_LEN_HANDLE];
      C_BHANDLERECT *pHandleRect;

      while (acedSSName(ss, ndx++, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++i);

         gsc_enthand(ent, Handle);
         if (!(pHandleRect = (C_BHANDLERECT *) pHandleRectTree->search(Handle)))
         {
            pHandleRectTree->add(Handle);
            pHandleRect = (C_BHANDLERECT *) pHandleRectTree->get_cursor();
         }

         if (pHandleRect)
         {
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y])
                  out.add(ent);
            }
            else  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y] &&
                   pHandleRect->corner1[Z] <= p2[Z] && pHandleRect->corner2[Z] >= p1[Z])
                  out.add(ent);
         }
      }
   }
   else
      while (acedSSName(ss, ndx++, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++i);

         // Se è possibile determinare l'estensione dell'oggetto e 
         // se questa interseca la regione
         if (gsc_get_ent_window(ent, corner1, corner2) == GS_GOOD)
         {
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y])
                  out.add(ent);
            }
            else  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y] &&
                   corner1[Z] <= p2[Z] && corner2[Z] >= p1[Z])
                  out.add(ent);
         }
      }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::SpatialIntersectWindow       <internal> */
/*+
  Questa funzione filtra gli oggetti del gruppo di selezione
  mantenendo solo gli oggetti che intersecano la finestra indicata
  dal punto p1 (angolo in basso a sinistra) e p2 (angolo in alto a destra).
  Parametri:
  ads_point p1;       punto inf. sx.
  ads_point p2;       punto inf. dx.
  int CheckOn2D;      Controllo oggetti senza usare la Z (default = GS_GOOD)
  C_HANDLERECT_BTREE *pHandleRectTree; se <> NULL viene usato per cercare le estensioni
                                       degli oggetti e se non trovati vengono aggiunti
                                       (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::SpatialIntersectWindow(ads_point p1, ads_point p2, int CheckOn2D,
                                     C_HANDLERECT_BTREE *pHandleRectTree,
                                     int CounterToVideo) 
{
   long      ndx = 0, i = 0;
   ads_name  ent;
   ads_point corner1, corner2;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1071)); // "Calcolo delle intersezioni spaziali"

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(GS_LFSTR);
      StatusBarProgressMeter.Init(length());
   }

   if (pHandleRectTree)
   {
      TCHAR         Handle[MAX_LEN_HANDLE];
      C_BHANDLERECT *pHandleRect;

      while (acedSSName(ss, ndx, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++i);

         gsc_enthand(ent, Handle);
         if (!(pHandleRect = (C_BHANDLERECT *) pHandleRectTree->search(Handle)))
         {
            pHandleRectTree->add(Handle);
            pHandleRect = (C_BHANDLERECT *) pHandleRectTree->get_cursor();
         }

         if (pHandleRect)
         {
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y])
                   ndx++;
               else
                  acedSSDel(ent, ss);
            }
            else  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y] &&
                   pHandleRect->corner1[Z] <= p2[Z] && pHandleRect->corner2[Z] >= p1[Z])
                   ndx++;
               else
                  acedSSDel(ent, ss);
         }
         else
            acedSSDel(ent, ss);
      }
   }
   else
      while (acedSSName(ss, ndx, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++i);

         // Se è possibile determinare l'estensione dell'oggetto e 
         // se questa interseca la regione
         if (gsc_get_ent_window(ent, corner1, corner2) == GS_GOOD)
         {
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y])
                   ndx++;
               else
                  acedSSDel(ent, ss);
            }
            else  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y] &&
                   corner1[Z] <= p2[Z] && corner2[Z] >= p1[Z])
                   ndx++;
               else
                  acedSSDel(ent, ss);
         }
         else
            acedSSDel(ent, ss);
      }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::get_SpatialIntersectWindowNumObjs  <internal> */
/*+
  Questa funzione conta gli oggetti del gruppo di selezione
  che intersecano la finestra indicata
  dal punto p1 (angolo in basso a sinistra) e p2 (angolo in alto a destra).
  Parametri:
  ads_point p1;       punto inf. sx.
  ads_point p2;       punto inf. dx.
  int CheckOn2D;      Controllo oggetti senza usare la Z (default = GS_GOOD)
  C_HANDLERECT_BTREE *pHandleRectTree; se <> NULL viene usato per cercare le estensioni
                                       degli oggetti e se non trovati vengono aggiunti
                                       (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Ritorna il numero di oggetti che intersecano l'area.
-*/  
/*********************************************************/
long C_SELSET::get_SpatialIntersectWindowNumObjs(ads_point p1, ads_point p2, int CheckOn2D,
                                                 C_HANDLERECT_BTREE *pHandleRectTree,
                                                 int CounterToVideo) 
{
   long      ndx = 0, qty = 0;
   ads_name  ent;
   ads_point corner1, corner2;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1071)); // "Calcolo delle intersezioni spaziali"

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(length());

   if (pHandleRectTree)
   {
      TCHAR         Handle[MAX_LEN_HANDLE];
      C_BHANDLERECT *pHandleRect;

      while (acedSSName(ss, ndx++, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(ndx);

         gsc_enthand(ent, Handle);
         if (!(pHandleRect = (C_BHANDLERECT *) pHandleRectTree->search(Handle)))
         {
            pHandleRectTree->add(Handle);
            pHandleRect = (C_BHANDLERECT *) pHandleRectTree->get_cursor();
         }

         if (pHandleRect)
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y])
                  qty++;
            }
            else  // se interseca la regione
               if (pHandleRect->corner1[X] <= p2[X] && pHandleRect->corner2[X] >= p1[X] &&
                   pHandleRect->corner1[Y] <= p2[Y] && pHandleRect->corner2[Y] >= p1[Y] &&
                   pHandleRect->corner1[Z] <= p2[Z] && pHandleRect->corner2[Z] >= p1[Z])
                  qty++;
      }
   }
   else
      while (acedSSName(ss, ndx++, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(ndx);

         // Se è possibile determinare l'estensione dell'oggetto e 
         // se questo interseca la regione
         if (gsc_get_ent_window(ent, corner1, corner2) == GS_GOOD)
            if (CheckOn2D == GS_GOOD)
            {  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y])
                  qty++;
            }
            else  // se interseca la regione
               if (corner1[X] <= p2[X] && corner2[X] >= p1[X] &&
                   corner1[Y] <= p2[Y] && corner2[Y] >= p1[Y] &&
                   corner1[Z] <= p2[Z] && corner2[Z] >= p1[Z])
                  qty++;
      }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return qty;
} 


/*********************************************************/
/*.doc C_SELSET::SpatialIntersectEnt          <internal> */
/*+
  Questa funzione filtra gli oggetti del gruppo di selezione
  mantenendo solo gli oggetti che intersecano l'entità indicata.
  Parametri:
  ads_name ent;       entità su cui verificare il gruppo di selezione
  int CheckOn2D;      Controllo oggetti senza usare la Z (default = GS_GOOD)
  int OnlyInsPt;      Per i blocchi e testi usare solo il punto di inserimento 
                      (default = GS_GOOD)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)
  bool IncludeFirstLast; Nel controllo considera anche se l'intersezione è 
                         esattamente il punto iniziale o finale della linea
                         se IntersEnt è una linea (default = true)

  Ritorna il numero di oggetti che intersecano l'area.
-*/  
/*********************************************************/
int C_SELSET::SpatialIntersectEnt(ads_name IntersEnt, int CheckOn2D,
                                  int OnlyInsPt, int CounterToVideo, bool IncludeFirstLast) 
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, IntersEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (SpatialIntersectEnt((AcDbEntity *) pObj, CheckOn2D, OnlyInsPt, CounterToVideo,
                           IncludeFirstLast) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int C_SELSET::SpatialIntersectEnt(AcDbEntity *pIntersEnt, int CheckOn2D,
                                  int OnlyInsPt, int CounterToVideo, bool IncludeFirstLast) 
{
   AcGePoint3dArray Vertices;
   long             ndx = 0, i = 0;
   int              Result, Dec = -1;
   C_POINT_LIST     IntersectList;
   AcDbObjectId     objId;
   AcDbObject       *pObj;
   C_SELSET         VerifiedSS;
   AcDbEntity       *pDummyEnt, *pDummyIntersEnt;
   bool             Delete_pDummyIntersEnt;
   AcDbPoint        dummyPt;
   ads_point        InsPt, ptStart, ptEnd;
   ads_name         ent;
   C_STRING         dummyStr;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1071)); // "Calcolo delle intersezioni spaziali"

   if (CheckOn2D == GS_GOOD)
   {
      if (OnlyInsPt == GS_GOOD && gsc_isPunctualEntity(pIntersEnt))
      {
         if (gsc_get_firstPoint(pIntersEnt, InsPt) == GS_BAD) return GS_BAD;
         AcGePoint3d AcGePt(InsPt[X], InsPt[Y], 0.0);
         if ((pDummyIntersEnt = new AcDbPoint(AcGePt)) == NULL) return GS_BAD;
      }
      else
         // ottengo una nuova entità con tutte le Z = 0.0
         if (gsc_getEntOnXY(pIntersEnt, &pDummyIntersEnt) != GS_GOOD) return GS_BAD;

      Delete_pDummyIntersEnt = true;
   }
   else
      if (OnlyInsPt == GS_GOOD && gsc_isPunctualEntity(pIntersEnt))
      {
         if (gsc_get_firstPoint(pIntersEnt, InsPt) == GS_BAD) return GS_BAD;
         AcGePoint3d AcGePt(InsPt[X], InsPt[Y], InsPt[Z]);
         if ((pDummyIntersEnt = new AcDbPoint(AcGePt)) == NULL) return GS_BAD;
         Delete_pDummyIntersEnt = true;
      }
      else
      {
         pDummyIntersEnt = pIntersEnt;
         Delete_pDummyIntersEnt = false;
      }

   try
   {
      if (IncludeFirstLast == false)
      { // calcolo punto iniziale e finale
         if (gsc_get_firstPoint(pDummyIntersEnt, ptStart) == GS_BAD) AfxThrowUserException();
         if (gsc_get_lastPoint(pDummyIntersEnt, ptEnd) == GS_BAD) AfxThrowUserException();
      }

      // ricavo quanti sono i decimali di approssimazione
      dummyStr = fmod(GEOsimAppl::TOLERANCE, 1.0);
      if ((Dec = (int) dummyStr.len()) > 2) Dec -= 2; // elimino lo "0." in "0.01"

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Init(length());

      while (acedSSName(ss, ndx, ent) == RTNORM)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++i);

         IntersectList.remove_all();

         // Se si tratta di oggetto puntuale e si vuole considerare solo il 
         // punto di inserimento
         if (gsc_isPunctualEntity(ent) && OnlyInsPt)
         {
            gsc_get_firstPoint(ent, InsPt);
            AcGePoint3d AcGePt(InsPt[X], InsPt[Y], (CheckOn2D == GS_GOOD) ? 0.0 : InsPt[Z]);
            dummyPt.setPosition(AcGePt);
            Result = IntersectList.add_intersectWith(pDummyIntersEnt, &dummyPt, Dec);
         }
         else // per altri tipi di entità
            if (CheckOn2D == GS_GOOD)
            {
               // ottengo una nuova entità con tutte le Z = 0.0
               if (gsc_getEntOnXY(ent, &pDummyEnt) != GS_GOOD) AfxThrowUserException();
               Result = IntersectList.add_intersectWith(pDummyIntersEnt, pDummyEnt, Dec);
               gsc_ReleaseSubEnts(pDummyEnt);
               delete pDummyEnt;
            }
            else // 3D
            {
               if (acdbGetObjectId(objId, ent) != Acad::eOk)
                  { GS_ERR_COD = eGSInvGraphObjct; AfxThrowUserException(); }
               if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
                  { GS_ERR_COD = eGSInvGraphObjct; AfxThrowUserException(); }
               Result = IntersectList.add_intersectWith(pDummyIntersEnt, (AcDbEntity *) pObj, Dec);
               pObj->close();
            }

         // se non devono essere inclusi i punti iniziali e finali
         if (IncludeFirstLast == false)
         {
            C_POINT *p;
            
            if ((p = IntersectList.search(ptStart, GEOsimAppl::TOLERANCE)))
               IntersectList.remove(p);
            if ((p = IntersectList.search(ptEnd, GEOsimAppl::TOLERANCE)))
               IntersectList.remove(p);
         }

         // Se non è possibile calcolare le intersezioni dell'oggetto o 
         // se questo non interseca
         if (Result == GS_BAD || IntersectList.get_count() == 0)
            acedSSDel(ent, ss);
         else
            ndx++;
      }

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   }

   catch (...)
   {
      if (Delete_pDummyIntersEnt)
      {
         gsc_ReleaseSubEnts(pDummyIntersEnt);
         delete pDummyIntersEnt;
      }

      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::SpatialInternalEnt           <internal> */
/*+
  Questa funzione filtra gli oggetti del gruppo di selezione
  mantenendo solo gli oggetti che sono interni o (interni/intersecano)
  l'entità indicata.
  Parametri:
  ads_name ContainerEnt;   Entità su cui verificare il gruppo di selezione
  int CheckOn2D;           Controllo oggetti senza usare la Z (default = GS_GOOD)
  int OnlyInsPt;           Per i blocchi e testi usare solo il punto di inserimento 
                           (default = GS_GOOD)
  int    Mode;             Se Mode = INSIDE l'oggetto ent deve essere 
                           completamente interno (i punti tangenti sono 
                           considerati interni). Se Mode = CROSSING 
                           l'oggetto può anche essere intersecare (default = INSIDE).
  int CounterToVideo;      flag, se = GS_GOOD stampa a video il numero di entità che si 
                           stanno elaborando (default = GS_BAD)

  Ritorna il numero di oggetti che intersecano l'area.
-*/  
/*********************************************************/
int C_SELSET::SpatialInternalEnt(ads_name ContainerEnt, int CheckOn2D,
                                 int OnlyInsPt, int Mode, int CounterToVideo) 
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ContainerEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (SpatialInternalEnt((AcDbEntity *) pObj, CheckOn2D, OnlyInsPt, Mode, CounterToVideo) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int C_SELSET::SpatialInternalEnt(AcDbEntity *pContainerEnt, int CheckOn2D,
                                 int OnlyInsPt, int Mode, int CounterToVideo) 
{
   AcGePoint3dArray Vertices;
   long             ndx = 0, i = 0;
   int              Result;
   AcDbObjectId     objId;
   AcDbObject       *pObj;
   C_SELSET         VerifiedSS;
   AcDbEntity       *pDummyEnt, *pDummyContainerEnt;
   AcDbPoint        dummyPt;
   ads_point        InsPt;
   ads_name         ent;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1071)); // "Calcolo delle intersezioni spaziali"

   if (CheckOn2D == GS_GOOD)
   {
      // ottengo una nuova entità con tutte le Z = 0.0
      if (gsc_getEntOnXY(pContainerEnt, &pDummyContainerEnt) != GS_GOOD) return GS_BAD;
   }
   else pDummyContainerEnt = pContainerEnt;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(length());

   while (acedSSName(ss, ndx, ent) == RTNORM)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(++i);

      // Se si tratta di oggetto puntuale e si vuole considerare solo il 
      // punto di inserimento
      if (gsc_isPunctualEntity(ent) && OnlyInsPt)
      {
         gsc_get_firstPoint(ent, InsPt);
         AcGePoint3d AcGePt(InsPt[X], InsPt[Y], (CheckOn2D == GS_GOOD) ? 0.0 : InsPt[Z]);
         dummyPt.setPosition(AcGePt);
         Result = gsc_IsInternalEnt(pDummyContainerEnt, &dummyPt, Mode);
      }
      else // per altri tipi di entità
         if (CheckOn2D == GS_GOOD)
         {
            // ottengo una nuova entità con tutte le Z = 0.0
            if (gsc_getEntOnXY(ent, &pDummyEnt) != GS_GOOD)
               { gsc_ReleaseSubEnts(pDummyContainerEnt); delete pDummyContainerEnt; return GS_BAD; }
            Result = gsc_IsInternalEnt(pDummyContainerEnt, pDummyEnt, Mode);
            gsc_ReleaseSubEnts(pDummyEnt);
            delete pDummyEnt;
         }
         else // 3D
         {
            if (acdbGetObjectId(objId, ent) != Acad::eOk)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            Result = gsc_IsInternalEnt(pDummyContainerEnt, (AcDbEntity *) pObj, Mode);
            pObj->close();
         }

      // Se non è interno
      if (Result == GS_BAD)
         acedSSDel(ent, ss);
      else
         ndx++;
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (CheckOn2D == GS_GOOD)
   {
      gsc_ReleaseSubEnts(pDummyContainerEnt);
      delete pDummyContainerEnt;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SELSET::PolylineJoin                 <internal> */
/*+
  Questa funzione effettua il Join tra le poliline del gruppo di selezione.
  N.B. Tutti gli oggetti non polyline saranno rimossi dal gruppo.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::PolylineJoin(void) 
{
   int      last_echo;
   ads_name ent;
   long     ndx = 0;

   if (gsc_set_echo(0, &last_echo) == GS_BAD) return GS_BAD;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      if (gsc_ispline(ent) == GS_GOOD)
      {
         if (acedCommandS(RTSTR, _T("_.PEDIT"), RTENAME, ent, RTSTR, _T("_J"), RTPICKS, ss,
                         RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
            { gsc_set_echo(last_echo); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      }
      else
         subtract_ent(ent);

   if (gsc_set_echo(last_echo) == GS_BAD) return GS_BAD;

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::PolylineFit                  <internal> */
/*+
  Questa funzione effettua il Fit delle poliline del gruppo di selezione.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::PolylineFit(void) 
{
   int      last_echo;
   ads_name ent;
   long     ndx = 0;

   if (gsc_set_echo(0, &last_echo) == GS_BAD) return GS_BAD;

   while (acedSSName(ss, ndx++, ent) == RTNORM)
      if (gsc_ispline(ent) == GS_GOOD)
         if (acedCommandS(RTSTR, _T("_.PEDIT"), RTENAME, ent, 
                         RTSTR, _T("_F"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
            { gsc_set_echo(last_echo); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (gsc_set_echo(last_echo) == GS_BAD) return GS_BAD;

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SELSET::operator ==                  <internal> */
/*+
  Questa funzione ritorna true se i due gruppi contengono gli 
  stessi oggetti grafici.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
bool C_SELSET::operator == (C_SELSET &in)
{
   ads_name ent;
   long     ndx = 0;

   if (length() != in.length()) return false;
   while (entname(ndx++, ent) == GS_GOOD)
      if (in.is_member(ent) != GS_GOOD) return false;

   return true;
}


/*********************************************************/
/*.doc C_SELSET::operator !=                  <internal> */
/*+
  Questa funzione ritorna true se i due gruppi NON contengono gli 
  stessi oggetti grafici.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
bool C_SELSET::operator != (C_SELSET &in)
{
   return !((*this) == in);
}


/*********************************************************/
/*.doc C_SELSET::ClosestToEnt                 <external> */
/*+
  Questa funzione mantiene nel gruppo solo le Qty entità più vicine a Ent
  cancellando le più distanti.
  Parametri:
  ads_name Ent;   Entità a cui bisogna calcolare la distanza
  int Qty;        Quante entità mantenere tra le più vicine (default = 1)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET::ClosestToEnt(ads_name Ent, int Qty) 
{
   C_SELSET SelSet;

   if (SelSet.add(Ent) == GS_BAD) return GS_BAD;

   return ClosestTo(SelSet, Qty);
}
int C_SELSET::ClosestTo(C_SELSET &SelSet, int Qty) 
{
   AcDbEntityPtrArray ObjEntitySet, ReferenceEntitySet, ResultEntitySet;

   if (get_AcDbEntityPtrArray(ObjEntitySet) == GS_BAD) return GS_BAD;
   if (SelSet.get_AcDbEntityPtrArray(ReferenceEntitySet) == GS_BAD)
   {
      gsc_close_AcDbEntities(ObjEntitySet);
      return GS_BAD;
   }
   if (gsc_getClosestTo(ObjEntitySet, ReferenceEntitySet, ResultEntitySet, Qty) == GS_BAD)
   {
      gsc_close_AcDbEntities(ObjEntitySet);
      gsc_close_AcDbEntities(ReferenceEntitySet);
      return GS_BAD;
   }
   if (gsc_close_AcDbEntities(ObjEntitySet) == GS_BAD)
      { gsc_close_AcDbEntities(ReferenceEntitySet); return GS_BAD; }
   if (gsc_close_AcDbEntities(ReferenceEntitySet) == GS_BAD) return GS_BAD;

   return set_AcDbEntityPtrArray(ResultEntitySet);
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_SELSET
// INIZIO FUNZIONI C_SELSET_LIST
///////////////////////////////////////////////////////////////////////////


C_SELSET_LIST::C_SELSET_LIST() : C_LIST() {}

C_SELSET_LIST::~C_SELSET_LIST() {}


/*********************************************************/
/*.doc C_SELSET_LIST::PolylineJoin            <internal> */
/*+
  Questa funzione effettua il Join tra le poliline dei gruppi di selezione.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET_LIST::PolylineJoin(void) 
{
   C_SELSET *pSS = (C_SELSET *) get_head();

   gsc_startTransaction();

   while (pSS)
   {
      pSS->PolylineJoin();
      pSS = (C_SELSET *) pSS->get_next();
   }

   gsc_endTransaction();

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_SELSET_LIST::PolylineFit             <internal> */
/*+
  Questa funzione effettua il Fit delle poliline dei gruppi di selezione.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SELSET_LIST::PolylineFit(void) 
{
   C_SELSET *pSS = (C_SELSET *) get_head();

   gsc_startTransaction();

   while (pSS)
   {
      pSS->PolylineFit();
      pSS = (C_SELSET *) pSS->get_next();
   }

   gsc_endTransaction();

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_SELSET_LIST
// INIZIO FUNZIONI C_BHANDLERECT
///////////////////////////////////////////////////////////////////////////


// costruttore
C_BHANDLERECT::C_BHANDLERECT() : C_BSTR()
{
   ads_point_clear(corner1);
   ads_point_clear(corner2);
}

C_BHANDLERECT::C_BHANDLERECT(const TCHAR * in)
{
   ads_name ent;

   set_name(in);
   if (acdbHandEnt(in, ent) == RTNORM)
       gsc_get_ent_window(ent, corner1, corner2);
}

// distruttore
C_BHANDLERECT::~C_BHANDLERECT() {}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BHANDLERECT
// INIZIO FUNZIONI C_HANDLERECT_BTREE
///////////////////////////////////////////////////////////////////////////


C_HANDLERECT_BTREE::C_HANDLERECT_BTREE():C_STR_BTREE() {}

C_HANDLERECT_BTREE::~C_HANDLERECT_BTREE() {}

C_BNODE* C_HANDLERECT_BTREE::alloc_item(const void *in)
{
   C_BHANDLERECT *p = new C_BHANDLERECT((const TCHAR *) in);

   if (!p)
      GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_HANDLERECT_BTREE
///////////////////////////////////////////////////////////////////////////