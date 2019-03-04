/**********************************************************
Name: GS_ATTBL
                                   
Module description: File funzioni di base per la gestione dei blocchi
                    degli attributi per le entità GEOSIM. 
            
Author: Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS  S.p.A.

**********************************************************/


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <ctype.h>

#include "rxdefs.h"   
#include "adslib.h"   
#include <dbents.h>
#include <dbapserv.h>     // per acdbHostApplicationServices()

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_init.h"
#include "gs_ase.h"
#include "gs_user.h"
#include "gs_class.h"
#include "gs_prjct.h"
#include "gs_netw.h"
#include "gs_area.h"
#include "gs_lisp.h"
#include "gs_graph.h"
#include "gs_attbl.h"
#include "gs_opcod.h"
#include "gs_setv.h"
#include "gs_query.h"
#include "gs_dwg.h"     // gestione disegni
#include "gs_filtr.h"
#include "gs_thm.h"


static ads_real LAST_ROT = 0, LAST_SCALE = 1;

#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  attbl_t1=0, attbl_t2=0, attbl_t3=0, attbl_t4=0, attbl_t5=0, attbl_t6=0, attbl_t7=0, attbl_t8=0;
#endif


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

//----------------------------------------------------------------------------//
// class C_OVER_ATTR che memorizza un handle di un attributo visibile e
// una lista di handle di attributi che gli si sovrappongono e la lunghezza di questa lista
//----------------------------------------------------------------------------//
class C_OVER_ATTR : public C_STR
{
   friend class C_OVER_ATTR_LIST;

   public :
      ads_name DABlock;          // entità blocco padre
      long  id;                  // conteggio di handle_list
      C_STR_BTREE handle_list;   // lista ordinata di handle

      C_OVER_ATTR() {}
      virtual ~C_OVER_ATTR() {} // chiama ~C_STR

      long get_id() { return id; }
};
//----------------------------------------------------------------------------//
// class C_OVER_ATTR_LIST che memorizza lista di handle di attributi visibili e,
// per ognuno, una lista di handle di attributi che gli si sovrappongono
//----------------------------------------------------------------------------//
class C_OVER_ATTR_LIST : public C_LIST
{
   public :
      C_OVER_ATTR_LIST() {}
      ~C_OVER_ATTR_LIST() {}  // chiama ~C_LIST
      
      int remove_handle(const TCHAR *handle);
};

/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/


int get_attr_fas_from_block(TCHAR *sample_block_name, C_ATTRIB_FAS_LIST *pFasList);
static int gsc_attribinspoint_2_absolute(ads_point pInsBlock, ads_real BlkRotaz,
                                         ads_real BlkScale, ads_point pt,
                                         ads_point ptConverted);

TCHAR *get_str_param_name(TCHAR *str);
TCHAR *convert_str_parametric(const TCHAR *str, TCHAR *param);

int gsc_set_ins_mode_attrib(AcDbAttribute *pAttrib, short *GenFlag = NULL, 
                            short *HorzAlign = NULL, short *VertAlign = NULL);

int gsc_dbOpenAttribute(ads_name DABlock, const TCHAR *AttribName, 
                        enum AcDb::OpenMode Mode,
                        AcDbBlockReference **pBlock, AcDbAttribute **pAttrib);

int gsc_RbList2DAStrFmtRbList(C_ATTRIB_LIST *pAttribList, C_RB_LIST &ColValues, 
                              C_RB_LIST &DAStrFmtRbList, int cls = 0, int sub = 0);

int gsc_add_da_block(C_CLASS *p_class, C_RB_LIST &ColValues, C_SELSET &SelSet);


//-----------------------------------------------------------------------//
//////////////////  C_CLASS         INIZIO  ///////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_CLASS::updfromDA <external> */
/*+                                                                       
  Aggiorna gli attributi di una scheda di un'entità (già esistente) con 
  eventuali valori di attributi visibili.
  Parametri:
  C_RB_LIST &ColValues;   	  Lista ((<nome colonna><valore>) ...) di tutti gli 
     					      	  attributi della classe
  ads_name entity;		     Entità grafica
  C_SELSET *GlobalSelSet;    Gruppo di selezione globale per la scheda corrente
                             (default = ASE_DSC_NULL)
  int *is_torecalc;      	  Flag opzionale: se = GS_GOOD -> viene effettuato
                             il ricalcolo, se = GS_BAD non viene effettuato
                             il ricalcolo. Se = NULL va a controllare se 
                             è stato modificato un attributo visibile o se 
                             esiste almeno un attributo visibile (default = NULL).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::updfromDA(C_RB_LIST &ColValues, ads_name entity,
                       C_SELSET *GlobalSelSet, int *is_torecalc)
{
   C_ATTRIB_LIST *p_attrib_list = ptr_attrib_list();
   C_ATTRIB      *p_attrib;
   presbuf		  p;
   int 			  to_recalc, Modified = GS_BAD, result = GS_BAD;
   C_SELSET      SelSet;
   C_STRING      text, DescrFrom_TAB_REF;
   C_RB_LIST      VisValuesList;
   presbuf        value = NULL, p_attr;
   TCHAR          *attrib_name;
   int            i = 0;
   C_ATTRIB_BLOCK DABlock;
   C_SELSET       DABlockSet;
   C_PROVIDER_TYPE_LIST *pProviderTypeList = ptr_info()->getDBConnection(OLD)->ptr_DataTypeList();
   bool                  UpdateableFromDA;

   // se non ha legame diretto con la grafica o non ha legame con tabella
   if (!ptr_fas() || !p_attrib_list || p_attrib_list->is_visible() == GS_BAD)
      return GS_GOOD;

   do
   {
      if (GlobalSelSet == NULL)  // se non è passato come parametro, lo ricavo
      {
         if (get_SelSet(ColValues, SelSet) == GS_BAD) return GS_BAD;
      }
      else 
         if (GlobalSelSet->copy(SelSet) == GS_BAD) return GS_BAD;

      // numero di oggetti grafici associati al linkset
      if (SelSet.length() <= 0) { GS_ERR_COD = eGSOpNotAble; break; }

      if (id.category == CAT_SIMPLEX && id.type == TYPE_TEXT)
      { 
         if ((p_attrib = (C_ATTRIB *) p_attrib_list->getFirstVisibleAttrib()) == NULL)
            { GS_ERR_COD = eGSInvAttribName; break; }
         
         if ((p = ColValues.CdrAssoc(p_attrib->get_name())) == NULL) break;
         // leggo valore dal text
         if (gsc_getInfoText(entity, &text) == GS_BAD) break;
         if ((VisValuesList << acutBuildList(RTLB, RTLB,
                                                   RTSTR, p_attrib->get_name(),
                                                   RTSTR, text.get_name(),
                                                   RTLE,
                                             RTLE, 0)) == NULL) break;
      }
      else // altrimenti
      {
         C_ATTRIB_BLOCK DABlock;
         C_SELSET       DABlockSet;
         ads_name       ent;

         if (SelSet.copy(DABlockSet) != GS_GOOD || DABlockSet.intersectType(DA_BLOCK) != GS_GOOD)
            break;
         
         if (DABlockSet.entname(0, ent) == GS_BAD) { result = GS_GOOD; break; }
         DABlock.set_ent_name(ent);
         if (DABlock.get_values_attrib(VisValuesList) == GS_BAD) break;
      }

      // ciclo i valori visibili
      while ((p = VisValuesList.nth(i++)) != NULL)
      {
         p           = p->rbnext;
         attrib_name = p->resval.rstring;
         text        = p->rbnext->resval.rstring;

         if ((p_attrib = (C_ATTRIB *) p_attrib_list->search_name(attrib_name, FALSE)) != NULL)
         {  // verifico se esiste l'attributo visibile in ColValues
            // (nel caso in cui il blocco DA sia disallineato alla struttura del DB)
            if ((p_attr = ColValues.CdrAssoc(attrib_name)) == NULL) continue;

            // elimino eventuale prefisso e suffisso
            text.removePrefixSuffix(p_attrib->GphPrefix.get_name(), p_attrib->GphSuffix.get_name(), GS_BAD);

            // Verifico se ci sono dei valori codificati
            UpdateableFromDA = true;

            if (GS_CURRENT_WRK_SESSION)
            {
               // Carico la lista se non era già stata caricata
               DescrFrom_TAB_REF.paste(GS_CURRENT_WRK_SESSION->get_pCacheClsAttribValuesList()->get_Value(p_attrib->name, ColValues, text,
                                                                                                   GS_CURRENT_WRK_SESSION->get_PrjId(),
                                                                                                   id.code, id.sub_code));
               C_CACHE_ATTRIB_VALUES *pCacheAttribValues;

               // verifico se si tratta di una lista di scelta condizionata
               pCacheAttribValues = GS_CURRENT_WRK_SESSION->get_pCacheClsAttribValuesList()->get_pCacheAttribValues(p_attrib->name,
                                                                                                             GS_CURRENT_WRK_SESSION->get_PrjId(),
                                                                                                             id.code, id.sub_code);
               if (pCacheAttribValues)
                  // se non aggiornabile da blocco DA (flag settato da get_pCacheAttribValues)
                  if (pCacheAttribValues->get_UpdateableFromDA() != 1)
                     UpdateableFromDA = false;
                  else
                     // verifico se il suo valore è associato ad una lista di scelta 
                     // a 2 colonne
                     if (DescrFrom_TAB_REF.get_name())
                        // se valore è cambiato
                        if (gsc_strcmp(DescrFrom_TAB_REF.get_name(), p->resval.rstring) != 0)
                        {
                           Modified = TRUE;
                           gsc_RbSubst(p, DescrFrom_TAB_REF.get_name());
                        }
            }

            if (UpdateableFromDA == true) // se attributo aggiornabile da blocco DA
            {
               // se il valore è letto da TAB o REF
               if (DescrFrom_TAB_REF.get_name())
               {
                  if ((value = pProviderTypeList->DataValueStr2Rb(p_attrib->type,
                                                                  DescrFrom_TAB_REF.get_name())) == NULL)
                     break;
               }
               else
               {
                  // converto valore visibile in resbuf
                  // converto gli attributi numerici con lo standard (123.456 e NON con le impostazioni di window)
                  if (gsc_DBIsNumeric(p_attrib->ADOType) == GS_GOOD)
                  {
                     if (text.len() == 0)
                     {
                        if ((value = acutBuildList(RTNIL, 0)) == NULL)
                           { GS_ERR_COD = eGSOutOfMem; break; }
                     }
                     else
                        if ((value = acutBuildList(RTREAL, text.tof(), 0)) == NULL)
                           { GS_ERR_COD = eGSOutOfMem; break; }
                  }
                  else
                     if ((value = pProviderTypeList->DataValueStr2Rb(p_attrib->type,
                                                                     text.get_name())) == NULL)
                        break;
               }

               if (gsc_equal(value, p_attr) == GS_BAD)
               {	// valore cambiato
                  Modified = TRUE;
                  if (gsc_sostitutebuf(value, p_attr) == GS_BAD) break;
               }
               acutRelRb(value); value = NULL;
            }
         }
      }
      if (value) acutRelRb(value);

      // se non si sa se devo effettuare il ricalcolo oppure no
      if (!is_torecalc)
      {  // se è cambiato almeno un attributo visibile ed esiste un attributo
         // calcolato (solo valore) oppure da validare
         if (Modified == GS_GOOD &&
             (p_attrib_list->is_calculated() == GS_GOOD ||
              p_attrib_list->is_validated() == GS_GOOD))
            to_recalc = GS_GOOD;
      }
      else to_recalc = *is_torecalc;
      
      if (to_recalc == GS_GOOD) // c'è almeno un attributo calcolato ed
    							        // era cambiato almeno un attributo visibile
         if (CalcValidData(ColValues, MODIFY, &SelSet) == GS_BAD) break; // validazione e ricalcolo dati

      result = GS_GOOD;
   }
   while (0);
                       
   return result;
}


/*********************************************************/
/*.doc C_CLASS::updtoDA                       <external> */
/*+                                                                       
  Aggiorna i blocchi degli attributi di una scheda di un'entità.
  Parametri:
  C_SELSET &SelSet;	   Gruppo di selezione
  C_RB_LIST &ColValues; lista colonna-valore
  int ToSAVE_SS;        Flag per aggiungere il linkset a GEOsimAppl::SAVE_SS per salvataggio
                        (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::updtoDA(ads_name ent, C_RB_LIST &ColValues, int ToSAVE_SS)
{
   C_SELSET SelSet;

   SelSet.add(ent);

   return updtoDA(SelSet, ColValues, ToSAVE_SS);
}
int C_CLASS::updtoDA(C_SELSET &SelSet, C_RB_LIST &ColValues, int ToSAVE_SS)
{
   C_RB_LIST      StrList;
   C_ATTRIB_LIST *pAttribList = ptr_attrib_list();
   C_ATTRIB      *pAttrib;
   C_ID          *p_id = ptr_id();
   C_STRING      text;

   // se non ha legame diretto con la grafica o non ha legame con tabella
   if (!ptr_fas() || !pAttribList || pAttribList->is_visible() == GS_BAD)
      return GS_GOOD;

   // Formatto una lista di resbuf per le etichette
   if (gsc_RbList2DAStrFmtRbList(pAttribList, ColValues, StrList, id.code, id.sub_code) == GS_BAD)
      return GS_BAD;

   if ((p_id->category == CAT_SIMPLEX || p_id->category == CAT_SUBCLASS) && p_id->type == TYPE_TEXT)
   {
      presbuf  pValue;
      long     i;
      ads_name entity;

      if ((pAttrib = (C_ATTRIB *) pAttribList->getFirstVisibleAttrib()) == NULL)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

      if ((pValue = StrList.CdrAssoc(pAttrib->get_name())) == NULL)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

      // aggiorno n. aggregate su tutti gli oggetti
      i = 0;
      while (SelSet.entname(i++, entity) == GS_GOOD)
         if (gsc_set_text(entity, pValue->resval.rstring) == GS_BAD) break;
   }
   else
   {
      C_ATTRIB_BLOCK DABlock;

      if (DABlock.set_values_attrib(SelSet, StrList) == GS_BAD) return GS_BAD;
   }

   if (ToSAVE_SS == GS_GOOD) // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addSS2savess(SelSet) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::SetVisibDA                    <external> */
/*+                                                                       
  Visualizza o nasconde i blocchi degli attributi di una o più entità.
  Parametri:
  C_SELSET &SelSet;	   Gruppo di selezione
  int Mode;             Modalità di visualizzazione; VISIBLE o INVISIBLE
  int CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::SetVisibDA(ads_name ent, int Mode, int CounterToVideo)
{
   C_SELSET SelSet;

   SelSet.add(ent);

   return SetVisibDA(SelSet, Mode);
}
int C_CLASS::SetVisibDA(C_SELSET &SelSet, int Mode, int CounterToVideo)
{
   long                SelSetLen, key, qty = 0;
   ads_name            entity;
   int                 ToInsDABlk, ToUpd;
   bool                NotSaved = false;
   C_SELSET            PrivateSelSet, entSS;
   C_RB_LIST           ColValues;
   C_SET_VISIB         *pActiveSet = NULL;
   C_ATTRIB_BLOCK      DABlock;
   C_DBCONNECTION      *pTempConn;
   C_STRING            TempTableRef;
   C_PREPARED_CMD_LIST TempOldCmdList;

   C_STRING CompleteName;
   get_CompleteName(CompleteName);
   // "Applicazione dei criteri di visibilità delle etichette della classe <%s>"
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1064), CompleteName.get_name());

   if (gsc_check_op(opAttInvisib) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   // se si tratta di testo o non ha parte grafica o
   // se non ha legame con tabella o non ci sono attributi visibili
   if (id.type == TYPE_TEXT || !ptr_fas() || !ptr_info() ||
       !ptr_attrib_list() || ptr_attrib_list()->is_visible() == GS_BAD) 
      return GS_GOOD;

   // se non ci sono oggetti nel gruppo di selezione
   if ((SelSetLen = SelSet.length()) == 0) return GS_GOOD;
   SelSet.copy(PrivateSelSet);

   if (Mode == VISIBLE && GEOsimAppl::ACTIVE_VIS_ATTRIB_SET)
      // Cerco set di visibilità attivo (se esistente)
      pActiveSet = GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->get_VisibSet(this);

   // preparo istruzione per l'inserimento di record nella tabella temp
   if ((pTempConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;
   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(SelSetLen);

   while (PrivateSelSet.entname(0, entity) == GS_GOOD)
   {
  	   if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set_Perc((int) ((SelSetLen - PrivateSelSet.length()) * 100 / SelSetLen));

      // leggo valore chiave e il gruppo di selezione
      if (get_Key_SelSet(entity, &key, entSS) == GS_BAD)
         { PrivateSelSet.subtract_ent(entity); continue; }
      // se entità non aggiornabile
      if (is_updateableSS(key, entSS) == GS_BAD)
         { PrivateSelSet.subtract_ent(entity); continue; }

      ToUpd = TRUE;
      // se è un blocco attributi
      if (gsc_is_DABlock(entity) == GS_GOOD)
      {
         // accendi/spegni singola scheda
         DABlock.set_ent_name(entity);
         if (DABlock.set_vis_attrib(Mode) == GS_BAD)
            { PrivateSelSet.subtract_ent(entity); continue; }
         qty++;
         PrivateSelSet.subtract_ent(entity);

         // Se si deve accendere una scheda ed è attivo un set
         if (Mode == VISIBLE && pActiveSet)
         {
            DABlock.set_ent_name(entity);
            DABlock.apply_visib_set(pActiveSet->InvAttribList);
         }
      }
      else // se è un oggetto principale
      {
         long ItemNum = 0;

         ToInsDABlk = (Mode == VISIBLE) ? TRUE : FALSE;

         // accendi/spegni tutte le schede
         while (entSS.entname(ItemNum++, entity) == GS_GOOD)
         {
            // se è un blocco attributi
            if (gsc_is_DABlock(entity) == GS_GOOD)
            {
               DABlock.set_ent_name(entity);
               if (DABlock.set_vis_attrib(Mode) == GS_GOOD) qty++;

               // Se si deve visualizzare una scheda
               if (Mode == VISIBLE)
               {
                  ToInsDABlk = FALSE; // C'era già un blocco DA
                  // Se è attivo un set di visualizzazione
                  if (pActiveSet)
                  {
                     DABlock.set_ent_name(entity);
                     DABlock.apply_visib_set(pActiveSet->InvAttribList);
                  }
               }
            }
            PrivateSelSet.subtract_ent(entity);
         }
      
         // Se non c'era già un blocco e lo si vuole visualizzare
         if (ToInsDABlk && 
             (SelSetLen == 1 || GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO))
            // se la selezione riguarda una sola entità oppure se la posizione deve
            // essere calcolata in modo automatico
            if (gsc_add_da_block(entity) == GS_GOOD)
               { qty++; ToUpd = FALSE; }
      }

      // se non è stato inserito un blocco nuovo (che chiama già la ::upd_data)
      if (ToUpd)
         if (query_data(key, ColValues, &TempOldCmdList) == GS_BAD ||
             upd_data(key, ColValues, ((C_PREPARED_CMD *) TempOldCmdList.get_head()), 
                      &entSS) == GS_BAD)
         {
            // Aggiungo gli oggetti nel gruppo degli oggetti rifiutati
            GEOsimAppl::REFUSED_SS.add(entity);
            NotSaved = true;
         }
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      if (Mode == INVISIBLE) 
         acutPrintf(gsc_msg(653), qty); // "\nNascoste %d scheda/e attributi."
      else
         acutPrintf(gsc_msg(377), qty); // "\nVisualizzate %d scheda/e attributi."

      if (NotSaved)
         acutPrintf(_T("\n%s"), gsc_msg(84)); // "Alcune entità di GEOsim non saranno salvate"

      acutPrintf(GS_LFSTR);
   }
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_CLASS         FINE    ///////////////////////////////
//////////////////  C_ATTRIB_BLOCK  INIZIO  ///////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::C_ATTRIB_BLOCK         <internal> */
/*+
  Questa funzione è il costruttore di C_ATTRIB_BLOCK
-*/  
/*********************************************************/
C_ATTRIB_BLOCK::C_ATTRIB_BLOCK()
{
   ads_point_clear(insert_point);
   wcscpy(layer, DEFAULT_LAYER);
   rotation = 0;
   scale	   = 1;
   h_text   = 1;
   wcscpy(style, DEFAULT_TEXTSTYLE);
   color.setByLayer();
   //vis	   = 0;
   ads_name_clear(ent_name);
}
C_ATTRIB_BLOCK::C_ATTRIB_BLOCK(ads_name ent)
{
   ads_point_clear(insert_point);
   wcscpy(layer, DEFAULT_LAYER);
   rotation = 0;
   scale	   = 1;
   h_text   = 1;
   color.setByLayer();
   //vis	   = 0;
   ads_name_set(ent, ent_name);
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::~C_ATTRIB_BLOCK        <internal> */
/*+
  Questa funzione è il distruttore di C_ATTRIB_BLOCK
-*/  
/*********************************************************/
C_ATTRIB_BLOCK::~C_ATTRIB_BLOCK()
{
}


/*****************************************************************************/
/*.doc C_ATTRIB_BLOCK::get_insert_point & set_insert_point        <internal> */
/*+
  Queste funzioni gestiscono il valore del punto di inserimento del blocco.
-*/  
/*****************************************************************************/
int C_ATTRIB_BLOCK::get_insert_point(ads_point out)
{
   ads_point_set(insert_point, out);
   return GS_GOOD;
}
int C_ATTRIB_BLOCK::set_insert_point(ads_point in)
{
   ads_point_set(in, insert_point);
   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_ATTRIB_BLOCK::get_layer & set_layer                      <internal> */
/*+
  Queste funzioni gestiscono il valore del piano del blocco.
-*/  
/*****************************************************************************/
TCHAR *C_ATTRIB_BLOCK::get_layer()
{
   return layer;
}
int C_ATTRIB_BLOCK::set_layer(const TCHAR *in)
{
   if (in != NULL && (wcslen(in) < MAX_LEN_LAYERNAME))
      gsc_strcpy(layer, in, MAX_LEN_LAYERNAME);
   else
      return GS_BAD;
   return GS_GOOD;
}      


/***********************************************************/
/*.doc C_ATTRIB_BLOCK::get_rotation & set_rotation  <internal> */
/*+
  Queste funzioni gestiscono il valore della rotazione del blocco (in gradi).
-*/  
/*********************************************************/
ads_real C_ATTRIB_BLOCK::get_rotation()
{
   return rotation;
}
int C_ATTRIB_BLOCK::set_rotation(ads_real in)
{
   rotation = in;
   return GS_GOOD;
}


/***********************************************************/
/*.doc C_ATTRIB_BLOCK::get_scale & set_scale  <internal> */
/*+
  Queste funzioni gestiscono il valore della scala del blocco.
-*/  
/*********************************************************/
ads_real C_ATTRIB_BLOCK::get_scale()
{
   return scale;
}
int C_ATTRIB_BLOCK::set_scale(ads_real in)
{
   if (gsc_validscale(in) == GS_BAD) return GS_BAD;
   scale = in;
   return GS_GOOD;
}


/***********************************************************/
/*.doc C_ATTRIB_BLOCK::get_h_text & set_h_text  <internal> */
/*+
  Queste funzioni gestiscono il valore dell'altezza testo degli attributi.
-*/  
/*********************************************************/
ads_real C_ATTRIB_BLOCK::get_h_text()
{
   return h_text;
}
int C_ATTRIB_BLOCK::set_h_text(ads_real in)
{
   if (gsc_validhtext(in) == GS_BAD) return GS_BAD;
   h_text = in;
   return GS_GOOD;
}


/***********************************************************/
/*.doc C_ATTRIB_BLOCK::get_text_style & set_text_style <internal> */
/*+
  Queste funzioni gestiscono il valore dello stile testo degli attributi.
-*/  
/*********************************************************/
TCHAR *C_ATTRIB_BLOCK::get_text_style()
{
   return style;
}
int C_ATTRIB_BLOCK::set_text_style(TCHAR *in)
{
   if (gsc_validtextstyle(in) == GS_BAD) return GS_BAD;
   gsc_strcpy(style, in, MAX_LEN_TEXTSTYLENAME);
   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_ATTRIB_BLOCK::get_color & set_color                      <internal> */
/*+
  Queste funzioni gestiscono il valore del colore.
-*/  
/*****************************************************************************/
void C_ATTRIB_BLOCK::get_color(C_COLOR &out)
{
   out = color;
}
int C_ATTRIB_BLOCK::set_color(C_COLOR &in)
{
   if (in.getColorMethod() == C_COLOR::None) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   color = in;
   return GS_GOOD;
}      


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::create                 <internal> */
/*+
  Questa funzione crea l'oggetto blocco $T contenente gli attributi visibili.
  Parametri:
  C_RB_LIST &ColValues;  Lista di valori da visualizzre nel blocco DA
                         es. ((<attr1><valore>)(<attr2><valore>)...)
  ads_point  InsPoint;   Punto di inserimento del blocco DA
  ads_real   Rot;        Rotazione del blocco DA in radianti (0 per default)
  presbuf    pDAParams;  Lista di parametri per la definizione delle
                         caratteristiche grafiche degli attributi del
                         blocco DA. Lista così composta:
                         (40, Height,        // altezza testi (1 per default)
                          51, Oblique,       // angolo obliquo testi (0 per default)
                           7, TextStyle,     // stile testo (opzionale)
                           8, LayerName,     // nome del layer (opzionale)
                          71, GeneratFlag,   // flag di generazione testo (opzionale)
                          72, HorizAlign,    // allineamento orizzontale (opzionale)
                          74, VertAlign,     // allineamento verticale (opzionale)
                          62, Color,         // colore testo
                          0)) == NULL )
                         I seguenti parametri assumono i valori (vedi manuale di ACAD):
                         HorizAlign;    0 a sinistra (per default)
                                        1 in centro
                                        2 a destra
                                        3 allineato
                                        4 centrato
                                        5 adattato
                         VertAlign;     0 linea di base (per default)
                                        1 in basso
                                        2 in centro
                                        3 in alto
                         Oblique;       Angolo obliquo (0 per default)
                         GeneratFlag;   0 normale (per default)
                                        2 testo rovesciato
                                        3 testo capovolto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Il punto di inserimento del blocco è in alto a sinistra e gli attributi
        sono impilati in modo che l'attributo 1 sia quello più in alto.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::create(C_RB_LIST &ColValues, ads_point InsPoint,
                           ads_real Rot, presbuf pDAParams)
{
   AcDbBlockTableRecord *pBlockTableRecord;
   AcDbObjectId         BlockId, AttId, newEntId;
   AcDbBlockTable       *pBlockTable;
   AcGePoint3d          pt(InsPoint[X], InsPoint[Y], InsPoint[Z]);
   AcDbAttribute        *pAttrib;
   presbuf              p_rb;
   int                  index = 0;
   short                HorizAlign = 0, VertAlign = 0, GeneratFlag = 0;
   int                  color = DEFAULT_COLOR;
   TCHAR                *TextStyle = NULL, *LayerName = NULL, *Value;
   ads_point            Point;
   ads_real             Height = 1, Oblique = 0, Dist, OffSet;

   // altezza testi
   if ((p_rb = gsc_rbsearch(40, pDAParams))) Height = p_rb->resval.rreal;
   // angolo obliquo testi
   if ((p_rb = gsc_rbsearch(51, pDAParams))) Oblique = p_rb->resval.rreal;
   // stile testo
   if ((p_rb = gsc_rbsearch(7, pDAParams))) TextStyle = p_rb->resval.rstring;
   // nome del layer
   if ((p_rb = gsc_rbsearch(8, pDAParams))) LayerName = p_rb->resval.rstring;
   // flag di generazione testo
   if ((p_rb = gsc_rbsearch(71, pDAParams))) GeneratFlag = (short) p_rb->resval.rint;
   // flag di allineamento orizzontale
   if ((p_rb = gsc_rbsearch(72, pDAParams))) HorizAlign = (short) p_rb->resval.rint;
   // flag di allineamento verticale
   if ((p_rb = gsc_rbsearch(74, pDAParams))) VertAlign = (short) p_rb->resval.rint;
   // colore
   if ((p_rb = gsc_rbsearch(62, pDAParams))) color = p_rb->resval.rint;

   // Open the block table for read.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->getAt(_T("$T"), BlockId) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }

   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   pBlockTable->close();

   // creazione di una istanza del blocco "$T"
   AcDbBlockReference *pBlkRef = new AcDbBlockReference;
   pBlkRef->setBlockTableRecord(BlockId);
   pBlkRef->setPosition(pt);        // punto di inserimento
   pBlkRef->setRotation(Rot);       // rotazione (in radianti)
   pBlkRef->setColorIndex(color);   // colore
   gsc_setLayer(pBlkRef, (LayerName) ? LayerName : _T("0")); // nome del layer 

   pBlockTableRecord->appendAcDbEntity(newEntId, pBlkRef);
   pBlockTableRecord->close();

   // Offset del 60% dell'altezza
   OffSet = Height * 0.6;
   Dist   = Height + OffSet;

   // calcolo il punto di inserimento del primo attributo (30% in su)
   ads_point_set_from_AcGePoint3d(pt, Point);
   acutPolar(Point, Rot + PI / 2, OffSet / 2, Point);
   AcGePoint3d_set_from_ads_point(Point, pt);

   // ciclo di inserimento degli attributi
   while ((p_rb = ColValues.nth(index++)) != NULL)
   {
      Value = gsc_rb2str(p_rb->rbnext->rbnext);

      pAttrib = new AcDbAttribute();
      pAttrib->setPosition(pt); // punto di inserimento
      gsc_setLayer(pAttrib, _T("0"));  // nome layer, "0" in questo caso significa "DABLOCCO"
      pAttrib->setTextString((Value) ? Value : GS_EMPTYSTR);  // valore
      if (Value) free(Value);
      pAttrib->setHeight(Height);                    // altezza testo
      pAttrib->setTag(p_rb->rbnext->resval.rstring); // etichetta
      pAttrib->setRotation(Rot); // rotazione (in radianti)
      pAttrib->setOblique(Oblique); // angolo obliquo testi
      pAttrib->setColorIndex(0); // colore BYBLOCK

      // stile testo (opzionale)
      if (TextStyle) gsc_set_style_attrib(pAttrib, TextStyle);

      gsc_set_ins_mode_attrib(pAttrib, &GeneratFlag, &HorizAlign, &VertAlign);

      pBlkRef->appendAttribute(AttId, pAttrib);
      pAttrib->close();

      // calcolo il punto di inserimento del prossimo attributo
      ads_point_set_from_AcGePoint3d(pt, Point);
      acutPolar(Point, Rot - PI / 2, Dist, Point);
      AcGePoint3d_set_from_ads_point(Point, pt);
   }
   pBlkRef->close();

   acdbEntLast(ent_name); // non può fallire se "ads_entmake" non ha fallito

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::insert <external> */
/*+                                                                       
  Inserisce un blocco per gli attributi con coordinate di inserimento
  conosciute.
  C_RB_LIST &ColValues;    Lista di valori da visualizzre nel blocco DA
                           es. ((<attr1><valore>)(<attr2><valore>)...)
  int       cls;		      codice classe
  int       sub;		      codice sotto-classe
  long      Key;           Valore chiave
  TCHAR *file_ref_block;   Path completa file dwg contenente il blocco di riferimento
                           (default NULL)
  TCHAR *ref_block;        Nome del blocco di riferimento (default NULL)
  int       modo;          se MANUAL chiede : punto inserimento, scala e rotazione
                           (per default = MANUAL)
  int       num_el;        n. di aggregazioni (per default = 2 -> 1 oggetto +
                           1 scheda attributi)

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD. 
  
  N.B. Il colore di default degli attributi del blocco sono da blocco, il colore del
  blocco per ora viene derivato dal colore del layer.
  Questo, anche se è una incongruenza, torna utile in fase di stampa in cui lo
  spessore del tratto è in funzione del colore e quindi si può cambiare il
  colore degli attributi in modo agevole.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::insert(C_RB_LIST &ColValues, int cls, int sub, long Key,
                           TCHAR *file_ref_block, TCHAR *ref_block,
                           int modo, int num_el)
{
   ads_point  ins_attrib;
   C_EED	     eed;
   int        result = GS_BAD;
   C_LINK     Link;
   C_RB_LIST  Descr;
   C_CLASS    *pClass;
   C_RB_LIST  StrList;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if ((pClass = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;

   // Formatto una lista di resbuf per i blocchi DA
   if (gsc_RbList2DAStrFmtRbList(pClass->ptr_attrib_list(), ColValues, StrList, cls, sub) == GS_BAD)
      return GS_BAD;

   if (get_insert_point(ins_attrib) == GS_BAD) return GS_BAD;
   
   if (modo == MANUAL) // chiedo il punto di inserimento del blocco
   {
  	   acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      // "\nPunto di inserimento della scheda: "
      if ((result = acedGetPoint(NULL, gsc_msg(109), ins_attrib)) == RTERROR)
         return GS_BAD;
	   if (result == RTCAN) return GS_CAN;
      set_insert_point(ins_attrib);
   }

   if (get_insert_point(ins_attrib) == GS_BAD) return GS_BAD;

   do
   {
		// Non considero il colore  62, get_color(),
      if ((Descr << acutBuildList(8, get_layer(),
                                  40, get_h_text(), 
                                  7, get_text_style(),
                                  0)) == GS_BAD)
         break;

      // inserisco il blocco attributi visibili
      if (create(StrList, ins_attrib, 0, Descr.get_head()) == GS_BAD)
         break;

      // lo collego alla tabella
      if (Link.Set(ent_name, cls, sub, Key, INSERT) == GS_BAD) break;

      if (modo == MANUAL)
      {
         if (GEOsimAppl::GLOBALVARS.get_InsXScale() == MANUAL) // impostazione manuale del fattore di scala
         {
            if ((result = gsc_ddscale_block(ent_name, get_scale(), &LAST_SCALE)) != GS_GOOD)
               break;
         }
         else // scalo il blocco   
            if ((result = gsc_set_scale(ent_name, get_scale(), get_scale(), 1)) == GS_BAD) break;

         if (GEOsimAppl::GLOBALVARS.get_InsRotaz() == MANUAL) // impostazione manuale della rotazione
         {
            if ((result = gsc_ddrotate_block(ent_name, get_rotation(), &LAST_ROT)) != GS_GOOD)
               break;
         }
         else // ruoto il blocco senza controllo rotazione
            if ((result = gsc_set_rotation(ent_name, gsc_grd2rad(get_rotation()),
                                           GS_BAD)) == GS_BAD)
               break;
      }
      else
      {
         // scalo il blocco   
         if ((result = gsc_set_scale(ent_name, get_scale(), get_scale(), 1)) == GS_BAD)
            break;
         // ruoto il blocco senza controllo rotazione
         if ((result = gsc_set_rotation(ent_name, gsc_grd2rad(get_rotation()), GS_BAD)) == GS_BAD)
            break;
      }     
      // inserisco relazione all'oggetto grafico
      eed.cls    = cls;
      eed.sub    = sub;
      eed.num_el = num_el;
      eed.gs_id  = Key;

      if (eed.save(ent_name) == GS_BAD) break;
    
      result = GS_BAD;

      // se esiste il blocco di riferimento applico le variazioni impostate nel blocco
      if (setfas_attrib(file_ref_block, ref_block) == GS_BAD) break;

      // Se esiste un set di visibilità lo attivo
      if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET)
      {
         C_CLASS     *pClass;
         C_SET_VISIB *pActiveSet;

         if ((pClass = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) break;
         if ((pActiveSet = GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->get_VisibSet(pClass)) != NULL)
            if (apply_visib_set(pActiveSet->InvAttribList) == GS_BAD) break; 
      }  

      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addEnt2savess(ent_name) == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);

   return result;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::setfas_attrib <external> */
/*+                                                                       
  Questa funzione setta le caratteristiche grafiche di un blocco.
  Parametri:
  TCHAR *file_ref_block;    nome file DWG contenente il blocco fi riferimento
  TCHAR *ref_block;         nome blocco campione per le caratteristiche grafiche
  
  Restituisce GS_GOOD in caso di affermativo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::setfas_attrib(TCHAR *file_ref_block, TCHAR *ref_block)
{
   C_ATTRIB_FAS_LIST FasList;

   // se c'è il blocco di riferimento
   if (ref_block && wcslen(ref_block) > 0)
   { // controllo che sia già stato caricato
      if (gsc_validblock(ref_block) == GS_BAD)
      {
         // se c'è il file DWG contenente il blocco di riferimento
         if (file_ref_block && wcslen(file_ref_block) > 0)
            // carico il file DWG
            if (gsc_LoadThmFromDwg(file_ref_block) != GS_GOOD)
               { GS_ERR_COD = eGSInvalidRefBlock; return GS_BAD; }

         // controllo che sia tato caricato
         if (gsc_validblock(ref_block) == GS_BAD) return GS_BAD;
      }
      // ricavo una lista di C_ATTRIB_FAS con le caratteristiche degli attributi del
      // blocco campione
      if (get_attr_fas_from_block(ref_block, &FasList) == GS_BAD) return GS_BAD;

      if (setfas_attrib(&FasList, file_ref_block) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::setfas_attrib                             <external> */
/*+
  Questa funzione setta le caratteristiche grafiche di un blocco.
  Parametri:
  C_ATTRIB_FAS_LIST *pFasList;   lista delle caratteristiche grafiche degli attributi
  TCHAR *file_ref_block;         inserito questo parametro per un vaco di autocad:
                                 1) viene inserito il blocco di riferimento che definisce tutti i layer
                                 2) viene fatto un undo che però toglie tutti i layer ma non 
                                 la definizione del blocco ! ci si ritrova con il blocco di riferimento 
                                 ma senza la definizione dei layer in esso contenuti... (no comment)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::setfas_attrib(C_ATTRIB_FAS_LIST *pFasList, TCHAR *file_ref_block)
{
   C_RB_LIST    LayerDescr;
   C_ATTRIB_FAS *pFas;
	C_CLASS		 *pCls;
	C_EED			 eed;
   TCHAR        *param_name, *new_layer, *value;
   TCHAR        OldLayer[MAX_LEN_LAYERNAME];
   ads_point    pInsBlock, abs_point;
   ads_real     RotBlock = 0, BlkScale = 1;

   AcDbObjectId       objId;
   AcDbBlockReference *pBlock;
	AcDbAttribute      *pAttrib;
   AcDbObjectIterator *pAttrIter;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (eed.load(ent_name) == GS_BAD) 
      { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return GS_BAD;
	if (!pCls->ptr_fas()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   // punto di inserimento del blocco
   ads_point_set_from_AcGePoint3d(pBlock->position(), pInsBlock);
   // rotazione del blocco
   RotBlock = pBlock->rotation();
   // fattore di scala x del blocco
   BlkScale = pBlock->scaleFactors().sx;

   // Ciclo gli attributi
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         // se ci sono le caratteristiche di questo attributo
         if ((pFas = (C_ATTRIB_FAS *) pFasList->search_name(pAttrib->tagConst(), FALSE)) != NULL)
         {
            // se il layer dell'attributo è parametrico (es. "$class_layer$attr" per
            // concatenare il nome del layer della classe con la stringa "attr"
			   if ((param_name = get_str_param_name(pFas->ptr_fas()->layer)) != NULL)
			   {
				   gsc_toupper(param_name);
				   if (gsc_strcmp(param_name, _T("CLASS_LAYER"), FALSE) == 0)
					   value = pCls->ptr_fas()->layer;
				   else
				   {
                  free(param_name); 
                  GS_ERR_COD = eGSInvalidArg; 
                  pAttrib->close();
                  pBlock->close();
                  return GS_BAD;
               }
               free(param_name);

			      // converte il nome del layer sostituendo la parte parametrica
               if ((new_layer = convert_str_parametric(pFas->ptr_fas()->layer, value)) == NULL)
                  { pAttrib->close(); pBlock->close(); return GS_BAD; }
               gsc_strcpy(OldLayer, pFas->ptr_fas()->layer, MAX_LEN_LAYERNAME);
			      gsc_strcpy(pFas->ptr_fas()->layer, new_layer, MAX_LEN_LAYERNAME);
               free(new_layer);

               if ((LayerDescr << acdbTblSearch(_T("LAYER"), pFas->ptr_fas()->layer, 0)) == NULL)
               {  // se non esisteva il layer 
                  TCHAR   LineType[MAX_LEN_LINETYPENAME];
                  bool    isFrozen, isLocked, isOff;
                  double  Transparency;
                  C_COLOR color;

                  if (gsc_get_charact_layer(OldLayer, LineType, &color, 
                                            &isFrozen, &isLocked, &isOff, &Transparency) == GS_BAD)
                     if (file_ref_block)
                     {
                        // carico il file DWG
                        if (gsc_LoadThmFromDwg(file_ref_block) != GS_GOOD)
                           { pAttrib->close(); pBlock->close(); GS_ERR_COD = eGSInvalidRefBlock; return GS_BAD; }
                        if (gsc_get_charact_layer(OldLayer, LineType, &color, 
                                                  &isFrozen, &isLocked, &isOff, &Transparency) == GS_BAD)
                           { pAttrib->close(); pBlock->close(); return GS_BAD; }
                     }
                     else
                        { pAttrib->close(); pBlock->close(); return GS_BAD; }

                  // viene creato e settato con le caratteristiche ereditate da OldLayer
                  if (gsc_set_charact_layer(pFas->ptr_fas()->layer, LineType, &color,
                                            &isFrozen, &isLocked, &isOff, &Transparency) == GS_BAD)
                     { pAttrib->close(); pBlock->close(); return GS_BAD; }
               }
            }

            if (gsc_attribinspoint_2_absolute(pInsBlock, RotBlock, BlkScale,
                                              pFas->pt, abs_point) == GS_BAD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }

            // setto la fas per l'attributo
            if (pAttrib->upgradeOpen() != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
            if (gsc_set_fas_attrib(pAttrib, pFas->ptr_fas(), abs_point, RotBlock,
                                   pFas->GeneratFlag, pFas->HorzAlign, pFas->VertAlign,
                                   pFas->visib) == GS_BAD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }
         }

         pAttrib->close();
      }
   }
   pBlock->close();

   return GS_GOOD;
}


/***********************************************************/
/*.doc C_ATTRIB_BLOCK::apply_visib_set                     */
/*+
  Funzione della classe C_ATTRIB_BLOCK per l'applicazione del 
  set di visibilità ad un blocco attributi.
  La funzione viene eseguita solo se la scheda non era spenta.
  Parametri:
  C_STR_LIST &InvAttribList;    Lista degli attributi da nascondere

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::apply_visib_set(C_STR_LIST &InvAttribList)
{
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;
   AcDbBlockReference *pBlock;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = ((AcDbBlockReference *) pBlock)->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForWrite) == Acad::eOk)
      {
         if (InvAttribList.search_name(pAttrib->tagConst(), FALSE))
         {
            if (gsc_set_vis_attrib(pAttrib, INVISIBLE) == GS_BAD) continue;  // da nascondere
         }
         else
            if (gsc_set_vis_attrib(pAttrib, VISIBLE) == GS_BAD) continue; // da visualizzare

         pAttrib->close();
      }
   }
   pBlock->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::set_vis_attrib         <external> */
/*+
  Questa funzione setta la visibilita degli attributi del blocco DA
  Parametri:
  int      mode;           se = INVISIBLE rende invisibili tutti gli 
                           attributi del blocco; se = VISIBLE rende 
                           visibili tutti gli attributi del blocco.
  const TCHAR *AttribName; Opzionale; nome dell'attributo su cui agire se = NULL si
                           considerano tutti gli attributi del blocco (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::set_vis_attrib(int mode, const TCHAR *AttribName)
{
   C_EED              eed;
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;

   if (eed.load(ent_name) == GS_BAD) return GS_BAD;

   // Se si vuole spegnere solo un attributo salto i controlli successivi
   if (!AttribName)
   {
      if (mode == INVISIBLE) // se si vuole rendere invisibile il blocco
      { // se il blocco era già invisibile esco
         if (eed.vis == INVISIBLE) return GS_GOOD; 
      }
   }

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         if (AttribName) // se devo considerare solo questo attributo
         {
            if (wcsicmp(AttribName, pAttrib->tagConst()) == 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk)
                  { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
               if (gsc_set_vis_attrib(pAttrib, mode) == GS_BAD)
                  { pAttrib->close(); pBlock->close(); return GS_BAD; }
			      pAttrib->close();
               break;
            }
         }
         else
         {
            if (pAttrib->upgradeOpen() != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
            if (gsc_set_vis_attrib(pAttrib, mode) == GS_BAD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }
         }

			pAttrib->close();
		}
   }
   pBlock->close();

   if (mode == INVISIBLE) // se si vuole rendere invisibile
   {  // Se si vogliono spegnere tutti gli attributi e
      // il blocco non era marcato come invisibile
      if (!AttribName && eed.vis == VISIBLE)
         // Marco il blocco come spento
         eed.save_vis(INVISIBLE); 
   }
   else // Se si vuole accendere qualcosa e
        // il blocco non era marcato come visibile
      if (eed.vis == INVISIBLE)
         // Marco il blocco come visibile
         eed.save_vis(VISIBLE);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::set_values_attrib      <external> */
/*+
  Questa funzione setta i valori degli attributi di un blocco DA.
  Parametri:
  C_RB_LIST &Values;   Lista di attributi ((<nome1><val1>)(<nome2><val2>)...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::set_values_attrib(C_SELSET &SelSet, C_RB_LIST &Values)
{
   ads_name ent;
   long     i = 0;

   // ciclo sui link
   while (SelSet.entname(i++, ent) == GS_GOOD)
   {  // in SelSet potrebbero esserci degli oggetti che non sono blocchi DA
      // scarto ciò che non è blocco DA
      if (gsc_is_DABlock(ent) == GS_BAD) continue;

      set_ent_name(ent);
      // Aggiorno i valori degli attributi
      if (set_values_attrib(Values) != GS_GOOD) return GS_BAD;
   }

   return GS_GOOD;
}
int C_ATTRIB_BLOCK::set_values_attrib(C_RB_LIST &Values)
{
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;
   AcDbBlockReference *pBlock;
   int                Res = GS_GOOD;
   presbuf            prb;
   TCHAR              *Txt;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (acdbOpenObject(pBlock, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = ((AcDbBlockReference *) pBlock)->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) != Acad::eOk)
         { Res = GS_BAD; break; }

      if ((prb = Values.CdrAssoc(pAttrib->tagConst())) != NULL)
      {
         if ((Txt = gsc_rb2str(prb)) == NULL) 
         {
            if (gsc_strcmp(pAttrib->textString(), GS_EMPTYSTR) != 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk) { pAttrib->close(); Res = GS_BAD; break; }
               pAttrib->setTextString(GS_EMPTYSTR);
            }
         }
         else
         {
            if (gsc_strcmp(pAttrib->textString(), Txt) != 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk) { pAttrib->close(); Res = GS_BAD; break; }
               pAttrib->setTextString(Txt);
            }
            free(Txt);
         }
      }

      pAttrib->close();
   }
   pBlock->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::get_values_attrib      <external> */
/*+
  Questa funzione legge i valori di un blocco DA (Displayable Attributes).
  Parametri:
  C_RB_LIST &ColValues;   Lista di attributi visualizzati 
                           ((<nome1><val1>)(<nome2><val2>)...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::get_values_attrib(C_RB_LIST &ColValues)
{
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;
   AcDbBlockReference *pBlock;
   int                Res = GS_GOOD;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = ((AcDbBlockReference *) pBlock)->attributeIterator();

   if ((ColValues << acutBuildList(RTLB, 0)) == NULL) { pBlock->close(); return GS_BAD; }

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) != Acad::eOk)
         { Res = GS_BAD; break; }
      
      if ((ColValues += acutBuildList(RTLB,
                                      RTSTR, pAttrib->tagConst(),
                                      RTSTR, pAttrib->textStringConst(),
                                      RTLE, 0)) == NULL)
         { pAttrib->close(); Res = GS_BAD; break; }

      pAttrib->close();
   }
   pBlock->close();

   if (Res == GS_BAD)
      ColValues.remove_all();
   else
      if ((ColValues += acutBuildList(RTLE, 0)) == NULL) Res = GS_BAD;

   return Res;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::move_attrib            <external> */
/*+
  Questa funzione sposta un attributo di un blocco DA.
  Parametri:
  const TCHAR *AttribName;   nome dell'attributo
  ads_pointp InsPt;          nuova posizione in coordinate assolute (default = NULL)
  ads_real   *Rot;           nuova rotazione (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::move_attrib(const TCHAR *AttribName, ads_pointp InsPt,
                                ads_real *Rot)
{
   AcDbBlockReference *pBlock;
   AcDbAttribute      *pAttrib;
   int                Res;

   if (gsc_dbOpenAttribute(ent_name, AttribName, AcDb::kForRead, &pBlock, &pAttrib) == GS_BAD)
      return GS_BAD;
   Res = gsc_set_firstPoint(pAttrib, InsPt);
   if (Res == GS_GOOD && Rot) 
      // ruoto l'attributo blocco senza controllo rotazione
      Res = gsc_set_rotation(pAttrib, *Rot, GS_BAD);

	pAttrib->close();
   pBlock->close();

   if (pAttrib->drawable()) pAttrib->draw();

   return Res;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::set_h_text_attrib      <external> */
/*+
  Questa funzione setta l'altezza testo di un attributo di un blocco DA.
  Parametri:
  ads_real   h_text;       nuova altezza testo
  const TCHAR *AttribName; Opzionale; nome dell'attributo su cui agire se = NULL si
                           considerano tutti gli attributi del blocco (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::set_h_text_attrib(ads_real h_text, const TCHAR *AttribName)
{
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         if (AttribName) // se devo considerare solo questo attributo
         {
            if (wcsicmp(AttribName, pAttrib->tagConst()) == 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk)
                  { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
               if (pAttrib->setHeight(h_text) != Acad::eOk)
                  { GS_ERR_COD = eGSInvalidH_text; pAttrib->close(); pBlock->close(); return GS_BAD; }
			      pAttrib->close();
               break;
            }
         }
         else
         {
            if (pAttrib->upgradeOpen() != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
            if (pAttrib->setHeight(h_text) != Acad::eOk)
               { GS_ERR_COD = eGSInvalidH_text; pAttrib->close(); pBlock->close(); return GS_BAD; }
         }

			pAttrib->close();
		}
   }
   pBlock->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_ATTRIB_BLOCK::set_style_attrib       <external> */
/*+
  Questa funzione setta lo stile di testo di un attributo di un blocco DA.
  Parametri:
  TCHAR *style;            nuovo stile testo
  const TCHAR *AttribName; Opzionale; nome dell'attributo su cui agire se = NULL si
                           considerano tutti gli attributi del blocco (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_ATTRIB_BLOCK::set_style_attrib(TCHAR *style, const TCHAR *AttribName)
{
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         if (AttribName) // se devo considerare solo questo attributo
         {
            if (wcsicmp(AttribName, pAttrib->tagConst()) == 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk)
                  { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
               if (gsc_set_style_attrib(pAttrib, style) == GS_BAD)
                  { pAttrib->close(); pBlock->close(); return GS_BAD; }
			      pAttrib->close();
               break;
            }
         }
         else
         {
            if (pAttrib->upgradeOpen() != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
            if (gsc_set_style_attrib(pAttrib, style) == GS_BAD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }
         }

			pAttrib->close();
		}
   }
   pBlock->close();

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::set_color_attrib                          <external> */
/*+
  Questa funzione cambia il colore di un attributo di un blocco.
  Parametri:
  C_COLOR &color;          Nuovo colore
  const TCHAR *AttribName; Opzionale; nome dell'attributo su cui agire se = NULL si
                           considerano tutti gli attributi del blocco (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::set_color_attrib(C_COLOR &color, const TCHAR *AttribName)
{
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         if (AttribName) // se devo considerare solo questo attributo
         {
            if (wcsicmp(AttribName, pAttrib->tagConst()) == 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk)
                  { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
               if (gsc_set_color(pAttrib, color) == GS_BAD)
                  { pAttrib->close(); pBlock->close(); return GS_BAD; }
			      pAttrib->close();
               break;
            }
         }
         else
            if (gsc_set_color(pAttrib, color) == GS_BAD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }

			pAttrib->close();
		}
   }
   pBlock->close();

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::set_layer_attrib                          <external> */
/*+
  Questa funzione cambia il layer di un attributo di un blocco.
  Parametri:
  const TCHAR *layer;      nuovo layer
  const TCHAR *AttribName; Opzionale; nome dell'attributo su cui agire se = NULL si
                           considerano tutti gli attributi del blocco (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::set_layer_attrib(const TCHAR *layer, const TCHAR *AttribName)
{
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
	AcDbAttribute      *pAttrib;
   AcDbObjectId       objId;

   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if (pBlock->openAttribute(pAttrib, objId, AcDb::kForRead) == Acad::eOk)
      {
         if (AttribName) // se devo considerare solo questo attributo
         {
            if (wcsicmp(AttribName, pAttrib->tagConst()) == 0)
            {
               if (pAttrib->upgradeOpen() != Acad::eOk)
                  { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
               if (gsc_setLayer(pAttrib, layer) != GS_GOOD)
                  { pAttrib->close(); pBlock->close(); return GS_BAD; }
			      pAttrib->close();
               break;
            }
         }
         else
         {
            if (pAttrib->upgradeOpen() != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; pAttrib->close(); pBlock->close(); return GS_BAD; }
            if (gsc_setLayer(pAttrib, layer) != GS_GOOD)
               { pAttrib->close(); pBlock->close(); return GS_BAD; }
         }

			pAttrib->close();
		}
   }
   pBlock->close();

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::copy_visAttrib                           <external> */
/*+
  Questa funzione copia solo le caratteristiche di visibilità degli attributi 
  che sono contenuti anche in un altro blocco (dest).
  Parametri:
  ads_name destDABlock;       Entità blocco destinazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::copy_visAttrib(ads_name destDABlock)
{
   AcDbBlockReference *pSrcBlock, *pDstBlock;
   AcDbObjectIterator *pSrcAttrIter, *pDstAttrIter;
	AcDbAttribute      *pSrcAttrib, *pDstAttrib;
   AcDbObjectId       objId;
   int                res = GS_GOOD;

   // Apro il blocco sorgente
   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pSrcBlock, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pSrcAttrIter = pSrcBlock->attributeIterator();

   // Apro il blocco destinazione
   if (acdbGetObjectId(objId, destDABlock) != Acad::eOk)
      { pSrcBlock->close(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pDstBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { pSrcBlock->close(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pDstAttrIter = pDstBlock->attributeIterator();

	for (pSrcAttrIter->start(); !pSrcAttrIter->done(); pSrcAttrIter->step())
   {
	   objId = pSrcAttrIter->objectId();

      if (pSrcBlock->openAttribute(pSrcAttrib, objId, AcDb::kForRead) != Acad::eOk)
         { res = GS_BAD; break; }
      // Cerco l'attributo di destinazione
	   for (pDstAttrIter->start(); !pDstAttrIter->done(); pDstAttrIter->step())
      {
	      objId = pDstAttrIter->objectId();

         if (pDstBlock->openAttribute(pDstAttrib, objId, AcDb::kForRead) != Acad::eOk)
            { res = GS_BAD; break; }
         if (wcsicmp(pSrcAttrib->tagConst(), pDstAttrib->tagConst()) == 0)
         {
            if (pDstAttrib->upgradeOpen() != Acad::eOk)
               { pDstAttrib->close(); res = GS_BAD; break; }

            // setto la visibilità
            if (pDstAttrib->setInvisible(pSrcAttrib->isInvisible()) != Acad::eOk)
               { pDstAttrib->close(); res = GS_BAD; break; }
         }

         pDstAttrib->close();
      }

      pSrcAttrib->close();
   }

   pDstBlock->close();
   pSrcBlock->close();

   return res;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::copy_geomAttrib                           <external> */
/*+
  Questa funzione copia solo le caratteristiche geometriche degli attributi 
  che sono contenuti anche in un altro blocco (dest).
  Parametri:
  ads_name destDABlock;       Entità blocco destinazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::copy_geomAttrib(ads_name destDABlock)
{
   AcDbBlockReference *pSrcBlock, *pDstBlock;
   AcDbObjectIterator *pSrcAttrIter, *pDstAttrIter;
	AcDbAttribute      *pSrcAttrib, *pDstAttrib;
   AcDbObjectId       objId;
   int                res = GS_GOOD;

   // Apro il blocco sorgente
   if (acdbGetObjectId(objId, ent_name) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pSrcBlock, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pSrcAttrIter = pSrcBlock->attributeIterator();

   // Apro il blocco destinazione
   if (acdbGetObjectId(objId, destDABlock) != Acad::eOk)
      { pSrcBlock->close(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pDstBlock, objId, AcDb::kForWrite) != Acad::eOk)
      { pSrcBlock->close(); GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   pDstAttrIter = pDstBlock->attributeIterator();

	for (pSrcAttrIter->start(); !pSrcAttrIter->done(); pSrcAttrIter->step())
   {
	   objId = pSrcAttrIter->objectId();

      if (pSrcBlock->openAttribute(pSrcAttrib, objId, AcDb::kForRead) != Acad::eOk)
         { res = GS_BAD; break; }
      // Cerco l'attributo di destinazione
	   for (pDstAttrIter->start(); !pDstAttrIter->done(); pDstAttrIter->step())
      {
	      objId = pDstAttrIter->objectId();

         if (pDstBlock->openAttribute(pDstAttrib, objId, AcDb::kForRead) != Acad::eOk)
            { res = GS_BAD; break; }
         if (wcsicmp(pSrcAttrib->tagConst(), pDstAttrib->tagConst()) == 0)
         {
            if (pDstAttrib->upgradeOpen() != Acad::eOk)
               { pDstAttrib->close(); res = GS_BAD; break; }

            // setto il punto di inserimento
            if (pDstAttrib->setPosition(pSrcAttrib->position()) != Acad::eOk)
               { pDstAttrib->close(); res = GS_BAD; break; }
            // setto la rotazione
            if (pDstAttrib->setRotation(pSrcAttrib->rotation()) != Acad::eOk)
               { pDstAttrib->close(); res = GS_BAD; break; }
         }

         pDstAttrib->close();
      }

      pSrcAttrib->close();
   }

   pDstBlock->close();
   pSrcBlock->close();

   return res;
}


/****************************************************************************/
/*.doc C_ATTRIB_BLOCK::set_ins_mode_attrib                       <external> */
/*+
  Questa funzione cambia la modalità di inserimento di un attributo di un blocco.
  Parametri:
  const TCHAR *AttribName;  nome dell'attributo
  short       *GenFlag;     flag di generazione testo [0,2,4] (default = 0)
                            2 = rovesciato, 4 = capovolto (default = NULL)
  short       *HorzAlign;   allineamento orizzontale [0,1,2,3,4,5] (default = 0)
                            0 = a sinistra, 1 = in centro, 2 = a destra, 3 = allineato,
                            4 = centrato, 5 = adattato  (default = NULL)
  short       *VertAlign;   allineamento verticale [0,1,2,3] (default = 0)
                            0 = linea di base, 1 = in basso, 2 = in centro, 3 = in alto
                            (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int C_ATTRIB_BLOCK::set_ins_mode_attrib(const TCHAR *AttribName, short *GenFlag,
                                        short *HorzAlign, short *VertAlign)
{
   AcDbBlockReference *pBlock;
	AcDbAttribute      *pAttrib;
   int                res;

   if (gsc_dbOpenAttribute(ent_name, AttribName, AcDb::kForWrite, 
                           &pBlock, &pAttrib) == GS_BAD)
      return GS_BAD;
   res = gsc_set_ins_mode_attrib(pAttrib, GenFlag, HorzAlign, VertAlign);

   pAttrib->close();
   pBlock->close();

   return res;
}


//-----------------------------------------------------------------------//
//////////////////   C_ATTRIB_BLOCK  FINE   ///////////////////////////////
//-----------------------------------------------------------------------//


/***********************************************************/
/*.doc int gsc_from_attblock2attribfaslist     */
/*+
   Funzione per il caricamento in una lista C_ATTRIB_FAS_LIST 
   delle caratteristiche di un blocco attributi
   Parametri:
   ads_name             Blk         nome blocco  
   C_ATTRIB_FAS_LIST   *listAttrib  lista di output.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_from_attblock2attribfaslist(ads_name Blk, C_ATTRIB_FAS_LIST *pFasList) 
{
   C_ATTRIB_FAS *pAttribFas;
   C_FAS        *pFas;
   C_RB_LIST    AttribList, BlockDefinition;
   presbuf      prb;
   ads_name     ndx;
   int          result = GS_GOOD;

   ads_name_set(Blk, ndx);
   while (ads_entnext(ndx, ndx) == RTNORM)
   {
      // leggo le caratteristiche dell' attributo
      if ((AttribList << acdbEntGet(ndx)) == NULL) 
         { result = GS_BAD; break; }
      // se è una etichetta di "SEQEND" ho finito
      if ((prb = AttribList.SearchType(kDxfStart)) == NULL || 
          gsc_strcmp(prb->resval.rstring, _T("SEQEND")) == 0)
         break;

      // Se sono a questo punto ho un attributo quindi alloco un oggetto C_ATTRIB_FAS
      if ((pAttribFas = new C_ATTRIB_FAS) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pFas = pAttribFas->ptr_fas();
      pFasList->add_tail(pAttribFas);

      // Memorizzo il nome dell' attributo
      if ((prb = AttribList.SearchType(2)) == NULL)
         { result = GS_BAD; break; }
      pAttribFas->set_name(prb->resval.rstring);
      
      // Punto di ins. attrib. relativo al punto di ins. del blocco
      if (gsc_get_firstPoint(ndx, pAttribFas->pt) == GS_BAD)
         { GS_ERR_COD = eGSInvEntityOp; result = GS_BAD; break; }
      
      // Rotazione attrib. relativa alla rotazione del blocco
      if ((prb = AttribList.SearchType(50)) != NULL)
         pFas->rotation = prb->resval.rreal;
      
      // Altezza attrib.
      if ((prb = AttribList.SearchType(40)) != NULL)     
         pFas->h_text = prb->resval.rreal;
      
      // Colore attrib.
      gsc_get_color(ndx, pFas->color);
      
      // Layer attrib.
      if ((prb = AttribList.SearchType(8)) != NULL)      
         gsc_strcpy(pFas->layer, prb->resval.rstring, MAX_LEN_LAYERNAME);
      
      // Stile testo attrib.
      if ((prb = AttribList.SearchType(7)) != NULL)      
         gsc_strcpy(pFas->style, prb->resval.rstring, MAX_LEN_TEXTSTYLENAME);
      
      // Flag di generazione testo
      if ((prb = AttribList.SearchType(71)) != NULL)     
         pAttribFas->GeneratFlag = prb->resval.rint;
      
      // Allineamento orizzontale
      if ((prb = AttribList.SearchType(72)) != NULL)     
         pAttribFas->HorzAlign = prb->resval.rint;
      
      // Allineamento verticale
      if ((prb = AttribList.SearchType(74)) != NULL)     
         pAttribFas->VertAlign = prb->resval.rint;
      
      // flag visibilità attrib.
      if ((prb = AttribList.SearchType(70)) != NULL)     
         pAttribFas->visib = (prb->resval.rint & 1) ? INVISIBLE : VISIBLE;
   }

   return result;
}


//-----------------------------------------------------------------------//
/////////////////  C_ATTRIB_BLOCK  FINE  FUNZIONI   ///////////////////////
/////////////////  C_ATTRIB_FAS  INIZIO  FUNZIONI   ///////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc C_ATTRIB_FAS::C_ATTRIB_FAS             <internal> */
/*+
  Questa funzione è il costruttore di C_ATTRIB_FAS
-*/  
/*********************************************************/
C_ATTRIB_FAS::C_ATTRIB_FAS()
{
   ads_point_clear(pt);
   GeneratFlag = HorzAlign = VertAlign = 0;
   visib = 0;
   fas.width = 1.0;  // Fattore larghezza testo
}


/*********************************************************/
/*.doc C_ATTRIB_FAS::~C_ATTRIB_FAS            <internal> */
/*+
  Questa funzione è il distruttore di C_ATTRIB_FAS
-*/  
/*********************************************************/
C_ATTRIB_FAS::~C_ATTRIB_FAS()
{}


/*********************************************************/
/*.doc C_ATTRIB_FAS::ptr_fas                  <internal> */
/*+
  Questa funzione restituisce il puntatore alla C_FAS che 
  memorizza l'insieme delle caratteristiche grafiche.
-*/  
/*********************************************************/
C_FAS* C_ATTRIB_FAS::ptr_fas()
   { return &fas; }


//-----------------------------------------------------------------------//
/////////////////  C_ATTRIB_FAS    FINE  FUNZIONI   ///////////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_CountAttributes                    <external> */
/*+
  Parametri:
  ads_name ent;   entità grafica

  Ritorna il numero di attributi di un blocco.
-*/  
/*********************************************************/
int gsc_CountAttributes(ads_name Block)
{
   AcDbObjectId       objId;
   AcDbBlockReference *pBlock;
   AcDbObjectIterator *pAttrIter;
   int                res = 0;

   if (acdbGetObjectId(objId, Block) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return 0; }  
   if (acdbOpenObject(pBlock, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return 0; }

   pAttrIter = pBlock->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step()) res++;

   pBlock->close();

   return res;
}


/*********************************************************/
/*.doc gs_is_DABlock                          <internal> */
/*+
  Parametri:
  ads_name ent;   entità grafica

  Ritorna GS_GOOD se ent e' un blocco degli attributi GEOsim,
  altrimenti GS_BAD;
-*/  
/*********************************************************/
int gs_is_DABlock(void)
{
   presbuf  arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTENAME) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_is_DABlock(arg->resval.rlname) == GS_GOOD) acedRetT();
     
   return RTNORM;
}
int gsc_is_DABlock(AcDbObject *pObj)
{
	TCHAR                *BlockName;  
	AcDbBlockTableRecord *pCurEntBlock;
	AcDbObjectId         BlockId;
   int                  Result = GS_BAD;

	if (pObj->isA() != AcDbBlockReference::desc()) return GS_BAD;

	// get the insert's block table record and compare its name
	// to make sure we've got the right block.
	BlockId = (AcDbBlockReference::cast(pObj))->blockTableRecord();
	if (acdbOpenObject((AcDbObject*&)pCurEntBlock, BlockId, AcDb::kForRead) == Acad::eOk)
	{
		pCurEntBlock->getName(BlockName); 
		if (gsc_strcmp(BlockName, _T("$T")) == 0) Result = GS_GOOD;
		pCurEntBlock->close();
		acdbFree(BlockName);
   }

   return Result;
}
int gsc_is_DABlock(ads_name ent)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   int          Result;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk) return GS_BAD;

   Result = gsc_is_DABlock(pObj);
	
   if (pObj->close() != Acad::eOk) return GS_BAD;
   
   return Result;
}
int gsc_is_DABlock(presbuf list)
{
   presbuf p;

   // se non è un blocco "$T"
   if ((p = gsc_rbsearch(0, list)) == NULL || gsc_strcmp(p->resval.rstring, _T("INSERT")) != 0)
      return GS_BAD;
   if ((p = gsc_rbsearch(2, list)) == NULL || gsc_strcmp(p->resval.rstring, _T("$T")) != 0)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsinvisib                              <external> */
/*+
  Se blocco visibile lo rende invisibile.
-*/  
/*********************************************************/
void gsinvisib(void)
{
   int      ret;
   C_SELSET SelSet;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   do
   {
      if ((ret = gsc_ssget(NULL, NULL, NULL, NULL, SelSet)) != RTNORM)
         if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
         else return GEOsimAppl::CMDLIST.ErrorCmd();

      if (SelSet.length() > 0) break;
   }
   while (1);

   if (gsc_invisib(SelSet, GS_GOOD) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_invisib <external> */
/*+                                                                       
  Funzione Lisp che spegne le schede di un gruppo di selezione.
  Se l'entità nel gruppo di selezione è un blocco scheda allora viene 
  spento il singolo blocco.
  Se l'entità nel gruppo di selezione è una principale allora 
  vengono spente tutte le schede ad essa collegate.

  Parametri:
  ads_name sel_set;	Gruppo di selezione da spegnere.
  
  Restituisce GS_GOOD in caso di affermativo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_invisib(void)
{
   resbuf *arg;
   
   acedRetNil();

   if ((arg = acedGetArgs()) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg->restype != RTPICKS && arg->restype != RTENAME)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   if (arg->restype == RTENAME)
   {  // se singola entità creo un gruppo di selezione
      C_SELSET SelSet;

      if (SelSet.add(arg->resval.rlname) != GS_GOOD) return RTERROR;
      if (gsc_invisib(SelSet) == GS_BAD) return RTERROR;
   }
   else
      if (gsc_invisib(arg->resval.rlname, GS_GOOD) == GS_BAD) return RTERROR; 

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_invisib <external> */
/*+                                                                       
  Spegne le schede di un gruppo di selezione.
  Se l'entità nel gruppo di selezione è un blocco scheda allora viene 
  spento il singolo blocco.
  Se l'entità nel gruppo di selezione è una principale allora 
  vengono spente tutte le schede ad essa collegate.

  Parametri:
  C_SELSET &SelSet;    Gruppo di selezione da spegnere.
  int CounterToVideo;  flag, se = GS_GOOD stampa a video il numero di entità che si 
                       stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di affermativo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_invisib(ads_name SelSet, int CounterToVideo)
{
   C_SELSET internalSelSet;  
   internalSelSet.add_selset(SelSet);

   return gsc_invisib(internalSelSet, CounterToVideo);
}
int gsc_invisib(C_SELSET &SelSet, int CounterToVideo)
{
   ads_name entity;
   C_CLASS  *pCls;
   C_EED    check;
   C_SELSET CpySS, ClsSS;

   if (gsc_check_op(opAttInvisib) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (SelSet.copy(CpySS) == GS_BAD) return GS_BAD;
   GEOsimAppl::REFUSED_SS.clear(); // Pulisco il gruppo degli oggetti rifiutati
   
   gsc_startTransaction();
   while (CpySS.entname(0, entity) == GS_GOOD)
   {
      if (check.load(entity) == GS_BAD)
         CpySS.subtract_ent(entity);
      else
      {
         CpySS.copy(ClsSS);
         ClsSS.intersectClsCode(check.cls, check.sub);
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(check.cls, check.sub)))
            pCls->SetVisibDA(ClsSS, INVISIBLE, CounterToVideo);
         CpySS.subtract(ClsSS);
      }
   }
   gsc_endTransaction();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsvisib <external> */
/*+
  Comando Acad : rende visibili le schede di oggetti grafici GEOsim.
-*/  
/*********************************************************/
void gsvisib(void)
{
   int      ret;
   C_SELSET SelSet;

   GEOsimAppl::CMDLIST.StartCmd();
   
   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   do
   {
      if ((ret = gsc_ssget(NULL, NULL, NULL, NULL, SelSet)) != RTNORM)
         if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
         else return GEOsimAppl::CMDLIST.ErrorCmd();

      if (SelSet.length() > 0) break;
   }
   while (1);

   if (gsc_visib(SelSet, GS_GOOD) == GS_BAD) 
      return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_visib <external> */
/*+                                                                       
  Funzione Lisp che accende le schede di un gruppo di selezione
  contenente oggetti grafici (non blocchi DA).

  Parametri:
  ads_name sel_set;	Gruppo di selezione da accendere.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_visib(void)
{
   resbuf *arg;

   acedRetNil();
   
   if ((arg = acedGetArgs()) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg->restype != RTPICKS && arg->restype != RTENAME)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   if (arg->restype == RTENAME)
   {  // se singola entità creo un gruppo di selezione
      C_SELSET SelSet;

      if (SelSet.add(arg->resval.rlname) != GS_GOOD) return RTERROR;
      if (gsc_visib(SelSet) == GS_BAD) return RTERROR;
   }
   else
      if (gsc_visib(arg->resval.rlname, GS_GOOD) == GS_BAD) return RTERROR; 

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_visib <external> */
/*+                                                                       
  Accende le schede di un gruppo di selezione contenente oggetti 
  grafici (non blocchi DA).

  Parametri:
   ads_name sel_set;	   Gruppo di selezione da accendere.
   int CounterToVideo;  flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_visib(ads_name SelSet, int CounterToVideo)
{
   C_SELSET internalSelSet;  
   internalSelSet.add_selset(SelSet);

   return gsc_visib(internalSelSet, CounterToVideo);
}
int gsc_visib(C_SELSET &SelSet, int CounterToVideo)
{
   ads_name entity;
   C_CLASS  *pCls;
   C_EED    check;
   C_SELSET CpySS, ClsSS;

   if (gsc_check_op(opAttInvisib) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (SelSet.copy(CpySS) == GS_BAD) return GS_BAD;
   GEOsimAppl::REFUSED_SS.clear(); // Pulisco il gruppo degli oggetti rifiutati

   gsc_startTransaction();
   while (CpySS.entname(0, entity) == GS_GOOD)
   {
      if (check.load(entity) == GS_BAD)
         CpySS.subtract_ent(entity);
      else
      {
         CpySS.copy(ClsSS);
         ClsSS.intersectClsCode(check.cls, check.sub);
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(check.cls, check.sub)))
            pCls->SetVisibDA(ClsSS, VISIBLE, CounterToVideo);
         CpySS.subtract(ClsSS);
      }
   }
   gsc_endTransaction();

   return GS_GOOD;
}


/****************************************************************************/
/*.doc get_attr_fas_from_block <internal> */
/*+
  Questa funzione ricava una lista che descrive le FAS degli attributi del
  blocco dato.
  Parametri:
  TCHAR *sample_block_name;       nome del blocco;
  C_ATTRIB_FAS_LIST *pFasList;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int get_attr_fas_from_block(TCHAR *sample_block_name, C_ATTRIB_FAS_LIST *pFasList)
{
   C_ATTRIB_FAS *pAttribFas;
   C_FAS        *pFas;
   C_RB_LIST    AttribList, BlockDefinition;
   presbuf      prb;
   int          result = GS_GOOD;
   ads_name     AttribDefinition;

   // leggo la definizione del blocco
   if ((BlockDefinition << ads_tblsearch(_T("BLOCK"), sample_block_name, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }
   
   if ((prb = BlockDefinition.SearchType(-2)) == NULL)      // definizione primo attributo
      { GS_ERR_COD=eGSInvEntityOp; return GS_BAD; }

   ads_name_set(prb->resval.rlname, AttribDefinition);

   do
   {
      if ((pAttribFas = new C_ATTRIB_FAS) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pFas = pAttribFas->ptr_fas();

      if ((AttribList << acdbEntGet(AttribDefinition)) == NULL)
         { GS_ERR_COD = eGSInvEntityOp; result = GS_BAD; break; }

      if ((prb = AttribList.SearchType(2)) == NULL)      // nome attdef
         { GS_ERR_COD = eGSInvEntityOp; result = GS_BAD; break; }
      pAttribFas->set_name(prb->resval.rstring);

      // rotazione attrib. relativa alla rotazione del blocco
      if ((prb = AttribList.SearchType(50)) != NULL)
         pFas->rotation = prb->resval.rreal;

      if ((prb = AttribList.SearchType(40)) != NULL)     // altezza attrib.
         pFas->h_text = prb->resval.rreal;

      if ((prb = AttribList.SearchType(41)) != NULL)     // Fattore larghezza testo
         pFas->width = prb->resval.rreal;

      gsc_get_color(AttribDefinition, pFas->color);      // colore attrib.

      if ((prb = AttribList.SearchType(8)) != NULL)      // layer attrib.
         gsc_strcpy(pFas->layer, prb->resval.rstring, MAX_LEN_LAYERNAME);

      if ((prb = AttribList.SearchType(7)) != NULL)      // stile testo attrib.
         gsc_strcpy(pFas->style, prb->resval.rstring, MAX_LEN_TEXTSTYLENAME);

      if ((prb = AttribList.SearchType(71)) != NULL)     // Flag di generazione testo
         pAttribFas->GeneratFlag = prb->resval.rint;

      if ((prb = AttribList.SearchType(72)) != NULL)     // Allineamento orizzontale
         pAttribFas->HorzAlign = prb->resval.rint;

      if ((prb = AttribList.SearchType(74)) != NULL)     // Allineamento verticale
         pAttribFas->VertAlign = prb->resval.rint;

      // Se il flag 72 o 74 sono diversi da 0 la coordinata deve essere letta
      // dal codice 11 e non 10 (punto di allineamento)
      if (pAttribFas->HorzAlign != 0 || pAttribFas->VertAlign != 0)
      {
         if ((prb = AttribList.SearchType(11)) != NULL) // Punto di allineamento
            ads_point_set(prb->resval.rpoint, pAttribFas->pt);
      }
      else
         // punto di ins. attrib. relativo al punto di ins. del blocco
         if (gsc_get_firstPoint(AttribDefinition, pAttribFas->pt) == GS_BAD)
            { GS_ERR_COD = eGSInvEntityOp; result = GS_BAD; break; }

      if ((prb = AttribList.SearchType(70)) != NULL)     // flag visibilità attrib.
         pAttribFas->visib = (prb->resval.rint & 1) ? INVISIBLE : VISIBLE;

      pFasList->add_tail(pAttribFas);
   }
   while (ads_entnext(AttribDefinition, AttribDefinition) != RTERROR);

   if (result == GS_BAD) delete pAttribFas;

   return result;
}


/****************************************************************************/
/*.doc gsc_set_fas_attrib                                        <external> */
/*+
  Questa funzione setta le caratteristiche grafiche di un attributo di un
  blocco.
  Parametri:
  AcDbAttribute *pAttrib;  lista di definizione dell'attributo
  C_FAS     *pFas;         lista di caratteristiche grafiche (color, layer)
  ads_point abs_p_ins;     posizione assoluta dell'attributo
  ads_real  abs_rot;       rotazione assoluta del blocco
  short GeneratFlag;       Flag di generazione testo
  short HorzAlign;         Allineamento orizzontale
  short VertAlign;         Allineamento verticale
  int       visib;         flag di visibilità  se = INVISIBLE rende invisibili 
                           tutti gli attributi del blocco; se = VISIBLE rende 
                           visibili tutti gli attributi del blocco.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_set_fas_attrib(AcDbAttribute *pAttrib, C_FAS *pFas, ads_point abs_p_ins,
                       ads_real rotation_block, short GeneratFlag, short HorzAlign,
                       short VertAlign, int visib)
{
   ads_real abs_rot = rotation_block + pFas->rotation;

   // modalità di inserimento testo (prima di impostare posizione e rotazione !!!)
   if (gsc_set_ins_mode_attrib(pAttrib, &GeneratFlag, &HorzAlign, &VertAlign) == GS_BAD)
      return GS_BAD;

   // posizione
   if (gsc_set_firstPoint(pAttrib, abs_p_ins) == GS_BAD) return GS_BAD;

   // ruoto l'attributo blocco senza controllo rotazione
   if (gsc_set_rotation(pAttrib, abs_rot, GS_BAD) == GS_BAD) return GS_BAD;
   // colore
   if (gsc_set_color(pAttrib, pFas->color) == GS_BAD) return GS_BAD;
   // stile testo
   if (gsc_set_style_attrib(pAttrib, pFas->style) == GS_BAD) return GS_BAD;
   // layer
   if (gsc_setLayer(pAttrib, pFas->layer) != GS_GOOD) return GS_BAD;
   // altezza testo
   if (pAttrib->setHeight(pFas->h_text) != Acad::eOk) return GS_BAD;
   // fattore larghezza testo
   if (pAttrib->setWidthFactor(pFas->width) != Acad::eOk) return GS_BAD;
   // visibilità
   if (gsc_set_vis_attrib(pAttrib, visib) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_attribinspoint_2_absolute <external> */
/*+
  Questa funzione trasforma il punto di inserimento di un attributo da
  coordinate relative al punto di inserimento del blocco di appartenenza,
  a coordinate assolute 
  Parametri:
  ads_point pInsBlock;     punto di inserimento blocco
  ads_real  BlkRotaz;      rotazione del blocco
  ads_real  BlkScale;      scala del blocco
  ads_point pt;            posizione attributo in coordinate relative
  ads_point ptConverted    nuova posizione attributo in coordinate assolute

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_attribinspoint_2_absolute(ads_point pInsBlock, ads_real BlkRotaz, ads_real BlkScale,
                                  ads_point pt, ads_point ptConverted)
{
   ads_real  dist;
   ads_point dummy;

   // trasformazione da coordinate relative in coordinate assolute
   dummy[X] = pInsBlock[X] + pt[X] * BlkScale;
   dummy[Y] = pInsBlock[Y] + pt[Y] * BlkScale;
   dummy[Z] = pInsBlock[Z] + pt[Z] * BlkScale;
   dist = gsc_dist(dummy, pInsBlock);
   ads_polar(pInsBlock, ads_angle(pInsBlock, dummy)+BlkRotaz, dist, ptConverted);

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_set_style_attrib <external> */
/*+
  Questa funzione cambia lo stile testo di un attributo di un blocco.
  Parametri:
  AcDbAttribute *pAttrib;      Descrizione dell'attributo
  TCHAR *style;                stile testo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_set_style_attrib(AcDbAttribute *pAttrib, TCHAR *style)
{
   AcDbObjectId TextStyleId;

   if (gsc_getTextStyleId(style, TextStyleId) == GS_BAD) return GS_BAD;
   
   if (pAttrib->setTextStyle(TextStyleId) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_set_ins_mode_attrib                                   <external> */
/*+
  Questa funzione cambia la modalità di inserimento di un attributo di un blocco.
  Parametri:
  AcDbAttribute *pAttrib;  attributo
  short      *GenFlag;     flag di generazione testo [0,2,4] (default = 0)
                           2 = rovesciato, 4 = capovolto (default = NULL)
  short      *HorzAlign;   allineamento orizzontale [0,1,2,3,4,5] (default = 0)
                           0 = a sinistra, 1 = in centro, 2 = a destra, 3 = allineato,
                           4 = centrato, 5 = adattato  (default = NULL)
  short      *VertAlign;   allineamento verticale [0,1,2,3] (default = 0)
                           0 = linea di base, 1 = in basso, 2 = in centro, 3 = in alto
                           (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_set_ins_mode_attrib(AcDbAttribute *pAttrib, short *GenFlag, short *HorzAlign,
                            short *VertAlign)
{
   if (GenFlag) // flag di generazione testo
   {
      if (*GenFlag == 0)
      {
         pAttrib->mirrorInX(Adesk::kFalse);
         pAttrib->mirrorInY(Adesk::kFalse);
      }
      else
      {
         if (*GenFlag & 2) pAttrib->mirrorInX(Adesk::kTrue);
         if (*GenFlag & 4) pAttrib->mirrorInY(Adesk::kTrue);
      }
   }

   if (HorzAlign) // allineamento orizzontale
   {
      AcDb::TextHorzMode TextHorzMode;

      switch (*HorzAlign)
      {
         case 0: // a sinistra
            TextHorzMode = AcDb::kTextLeft;
            break;
         case 1: // in centro
            TextHorzMode = AcDb::kTextCenter;
            break;
         case 2: // a destra
            TextHorzMode = AcDb::kTextRight;
            break;
         case 3: // allineato (se allineamento verticale = 0)
            TextHorzMode = AcDb::kTextAlign;
            break;
         case 4: // centrato (se allineamento verticale = 0)
            TextHorzMode = AcDb::kTextMid;
            break;
         case 5: // adattato (se allineamento verticale = 0)
            TextHorzMode = AcDb::kTextFit;
            break;
         default : 
            GS_ERR_COD = eGSInvalidArg; 
            return GS_BAD;
      }

      if (pAttrib->setHorizontalMode(TextHorzMode) != Acad::eOk) return GS_BAD;
   }

   if (VertAlign) // allineamento verticale
   {
      AcDb::TextVertMode TextVertMode;

      switch (*VertAlign)
      {
         case 0: // a sinistra
            TextVertMode = AcDb::kTextBase;
            break;
         case 1: // in centro
            TextVertMode = AcDb::kTextBottom;
            break;
         case 2: // a destra
            TextVertMode = AcDb::kTextVertMid;
            break;
         case 3: // allineato (se allineamento verticale = 0)
            TextVertMode = AcDb::kTextTop;
            break;
         default : 
            GS_ERR_COD = eGSInvalidArg; 
            return GS_BAD;
      }

      if (pAttrib->setVerticalMode(TextVertMode) != Acad::eOk) return GS_BAD;
   }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_set_vis_attrib <external> */
/*+
  Questa funzione rende invisibile un attributo di un blocco.
  Parametri:
  C_RB_LIST &Attrib;   Descrizione dell'attributo
  int       mode;      se = INVISIBLE rende invisibili tutti gli 
                       attributi del blocco; se = VISIBLE rende 
                       visibili tutti gli attributi del blocco.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_set_vis_attrib(ads_name AttribEnt, int mode)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   int          res;

   if (acdbGetObjectId(objId, AttribEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   res = gsc_set_vis_attrib((AcDbAttribute *) pObj, mode);

   pObj->close();

   return res;
}
int gsc_set_vis_attrib(AcDbAttribute *pAttrib, int mode)
{
   if (mode == INVISIBLE)
   {
      return (pAttrib->setInvisible(Adesk::kTrue) != Acad::eOk) ? GS_BAD : GS_GOOD;
   }
   else
      return (pAttrib->setInvisible(Adesk::kFalse) != Acad::eOk) ? GS_BAD : GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_upd_DA_attrib_list <external> */
/*+
  Questa funzione aggiorna la lista degli attributi di un blocco DA 
  aggiungendo e cancellando gli attributi secondo i nomi contenuti in una lista
  data come parametro. Tutti gli attributi nuovi saranno inseriti in fondo alla
  eventuale lista di attributi.
  Parametri:
  ads_name entity;         oggetto blocco DA
  C_RB_LIST &ColValues;    Lista di valori da visualizzre nel blocco DA
                           es. ((<attr1><valore>)(<attr2><valore>)...)
  long Key;                Valore chiave
  C_FAS    *pFas;          Rappresentazione grafica da applicare
  ads_name res_ent;        nuovo blocco DA (output) (default = NULL)
  int ChangeLayer;         Flag se = GS_BAD non deve essere variato il layer del 
                           blocco DA esistente (default = GS_GOOD)
  int MoveOldVisAttrib;    Flag indicante se si devono mantenere (GS_BAD) o meno (GS_GOOD)
                           le posizioni degli attributi visibili già inseriti;
                           (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. l'oggetto <entity> viene cancellato e inserito un'altro blocco DA.
-*/  
/****************************************************************************/
int gsc_upd_DA_attrib_list(ads_name entity, C_RB_LIST &ColValues,
                           long Key, C_FAS *pFas, ads_name res_ent,
                           int ChangeLayer, int MoveOldVisAttrib)
{
   C_RB_LIST      EntList;
   presbuf        prb;
   ads_real       BlkRotaz = 0, BlkScale = 1;
   C_ATTRIB_BLOCK attrib_block;
   C_EED          eed;
   ads_name       new_DABlock;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // verifico che l'oggetto grafico sia già etichettato "GEOsim"
   if (eed.load(entity) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
   if (pFas == NULL) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((EntList << acdbEntGet(entity)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((prb = EntList.SearchType(0)) == NULL ||
       gsc_strcmp(prb->resval.rstring, _T("INSERT")) != 0)    // tipo di entità
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   // punto di inserimento blocco   
   if ((prb = EntList.SearchType(10)) == NULL)     
      { GS_ERR_COD=eGSInvEntityOp; return GS_BAD; }
   if (attrib_block.set_insert_point(prb->resval.rpoint) == GS_BAD) return GS_BAD;
   // angolo di rotazione del blocco (default=0) da convertire da radianti in gradi
   if ((prb = EntList.SearchType(50)) != NULL) BlkRotaz = prb->resval.rreal;
   if (attrib_block.set_rotation(gsc_rad2grd(BlkRotaz)) == GS_BAD) return GS_BAD;
   // fattore di scala del blocco (default=1)
   if ((prb = EntList.SearchType(41)) != NULL) BlkScale = prb->resval.rreal;
   if (attrib_block.set_scale(BlkScale) == GS_BAD) return GS_BAD;

   // layer del blocco
   if (ChangeLayer == GS_BAD) // se non deve essere variato il layer del blocco DA
   {  // leggo il layer dall'oggetto grafico
      if ((prb = EntList.SearchType(8)) == NULL)
         { if (attrib_block.set_layer(_T("0")) == GS_BAD) return GS_BAD; }
      else
         { if (attrib_block.set_layer(prb->resval.rstring) == GS_BAD) return GS_BAD; }
   }
   else
      if (attrib_block.set_layer(pFas->layer) == GS_BAD) return GS_BAD;

   // altezza testo degli attributi del blocco
   if (attrib_block.set_h_text(pFas->h_text) == GS_BAD) return GS_BAD;
   // stile testo degli attributi del blocco
   if (attrib_block.set_text_style(pFas->style) == GS_BAD) return GS_BAD;
	// Il colore
   if (attrib_block.set_color(pFas->color) == GS_BAD) return GS_BAD;

   if (attrib_block.insert(ColValues, eed.cls, eed.sub, Key,
                           pFas->file_ref_block,
                           pFas->ref_block, AUTO, eed.num_el) != GS_GOOD)
      return GS_BAD;

   // se ci sono degli attributi da ereditare, ne copio solo
   // le caratteristiche geometriche e non grafiche
   if (attrib_block.get_ent_name(new_DABlock) == GS_BAD) return GS_BAD;
   if (MoveOldVisAttrib == GS_BAD)
   {
      C_ATTRIB_BLOCK SrcDABlock(entity);

      if (SrcDABlock.copy_geomAttrib(new_DABlock) == GS_BAD) return GS_BAD;
   }

   // Visibilità attributi
   if (eed.vis == GS_BAD) // Se la scheda era spenta
   {
      if (attrib_block.set_vis_attrib(INVISIBLE) == GS_BAD) return GS_BAD;
   }
   else // Copio la visibilità degli attributi
   {
      C_ATTRIB_BLOCK SrcDABlock(entity);

      if (SrcDABlock.copy_visAttrib(new_DABlock) == GS_BAD) return GS_BAD;
   }

   // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio (prima di cancellarlo)
   if (gsc_addEnt2savess(entity) == GS_BAD) return GS_BAD;

   // cancello il vecchio blocco DA
   if (gsc_EraseEnt(entity) != GS_GOOD) return GS_BAD;

   if (res_ent) ads_name_set(new_DABlock, res_ent);

   return GS_GOOD;
}


/****************************************************************************/
/*.doc is_str_parametric <external> */
/*+
  Questa funzione verifica se il nome del layer indicato contiene una 
  parte parametrica (e quindi da sostituire run-time) oppure se è un nome statico.
  La parte parametrica è individuata da un carattere separatore iniziale = "$" e da
  un carattere separatore finale "$".
  Es. "$class_layer$attr" per concatenare il nome del layer della classe 
  		con la stringa "attr".

  Parametri:
  TCHAR *layer;    nome del layer

  Restituisce GS_GOOD nel caso di layer parametrico altrimenti restituisce GS_BAD.
  N.B. alloca memoria !!!
-*/  
/****************************************************************************/
TCHAR *get_str_param_name(TCHAR *str)
{
	TCHAR sep = _T('$'), *separator_1st, *separator_2nd;

   if ((separator_1st = wcschr(str, sep)) &&
       (separator_2nd = wcschr(++separator_1st, sep)))
   {
      TCHAR *result;
      
      if ((result = (TCHAR *) malloc(sizeof(TCHAR) * (separator_2nd - separator_1st + 1))) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      gsc_strcpy(result, separator_1st, separator_2nd - separator_1st + 1);

      return result;
   }

	return NULL;
}


/****************************************************************************/
/*.doc convert_str_parametric <external> */
/*+
  Questa funzione converte una sringa parametrica sostituendo alla stringa originale
  la parte di stringa ricevuta come secondo parametro
  Parametri:
  TCHAR *str;    nome del layer
  TCHAR *param

  Restituisce NULL in caso di errore altrimenti restituisce un puntatore valido.
-*/  
/****************************************************************************/
TCHAR *convert_str_parametric(const TCHAR *str, TCHAR *param)
{
   TCHAR *dummy = NULL, *str_result = NULL, sep = _T('$'), *separator_1st, *separator_2nd;
   int  result = GS_BAD;

   if ((dummy = gsc_tostring(str)) == NULL) return NULL;

   do
   {
      // leggo separatore iniziale
      if ((separator_1st = wcschr(dummy, sep)) != NULL)
      {
         *separator_1st = _T('\0');
         if ((str_result = gsc_tostring(dummy)) == NULL) break;
         separator_1st++;
         // leggo separatore finale
         if ((separator_2nd = wcschr(separator_1st, sep)) == NULL || ++separator_2nd == NULL)
         	break;
         if ((str_result = gsc_strcat(str_result, param)) == NULL) break;
         if ((str_result = gsc_strcat(str_result, separator_2nd)) == NULL) break; 

         if (gsc_validlayer(str_result) == GS_BAD) break;
         
         result = GS_GOOD;
   	}
   }
   while (0);
   free(dummy);
   if (result == GS_BAD)
      if (str_result) free(str_result);

   return str_result;
}


/************************************************************/
/*.doc gsc_add_da_block                                     */
/*+
  Questa funzione inserisce un blocco di attributi visibili
  relativo alla entità ent.
  Parametri:
  ads_name ent;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_add_da_block(ads_name ent)
{
   C_CLASS       *p_class;
   C_SELSET      SelSet;
   C_RB_LIST     col_values;
   C_ATTRIB_LIST *p_attrib_list;
   int           WhyNotUpd;
   long          key;
   C_EED         eed;

   // verifico che l'oggetto grafico sia già etichettato "GEOsim"
   if (eed.load(ent) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }

   if ((p_class = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return GS_BAD;

   if ((p_attrib_list = p_class->ptr_attrib_list()) == NULL) return GS_GOOD;
   if (p_attrib_list->is_visible() == GS_BAD) return GS_GOOD; // non ci sono attributi visibili

   // leggo valore chiave e il gruppo di selezione
   if (p_class->get_Key_SelSet(ent, &key, SelSet) == GS_BAD) return GS_BAD;
   // verifico che l'elemento sia modificabile
   if (p_class->is_updateableSS(key, SelSet, &WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   if (p_class->query_data(key, col_values) == GS_BAD) return GS_BAD;
   
   return gsc_add_da_block(p_class, col_values, SelSet);
}


/************************************************************/
/*.doc gsc_add_da_block                          <internal> */
/*+
  Questa funzione inserisce un blocco di attributi visibili
  relativo alla entità ent senza controllare che l'entità sia modificabile
  (va controllato prima ed eventualmente va bloccata agli altri utenti).
  Parametri:
  C_CLASS *p_class;     Puntatore alla classe di entità di GEOsim
  C_RB_LIST &ColValues; Lista dei valori alfanumerici dell'entità
  C_SELSET &SelSet;     Gruppo di selezione completo degli oggetti
                        che compongono l'entità (viene modificato)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Il gruppo SelSet viene modificato e aggiornato con il blocco DA inserito.
-*/  
/************************************************************/
int gsc_add_da_block(C_CLASS *p_class, C_RB_LIST &ColValues, C_SELSET &SelSet)
{
   C_ID          *p_id = p_class->ptr_id();
   C_ATTRIB_LIST *p_attrib_list;
   C_FAS         *p_fas;
   int           result, OldOp;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   // verifico che l'elemento abbia una rappresentazione grafica
   if ((p_fas = p_class->ptr_fas()) == NULL) return GS_GOOD;
   if ((p_attrib_list = p_class->ptr_attrib_list()) == NULL) return GS_GOOD;
   if (p_attrib_list->is_visible() == GS_BAD) return GS_GOOD; // non ci sono attributi visibili

   if (p_id->type != TYPE_TEXT)
   {
      C_ATTRIB_BLOCK attrib_block;
      ads_name       ent_blk, ent_name;
      presbuf        p_rb_key;
      C_RB_LIST      VisValues;
      C_STRING       TempTableRef;
      C_SELSET       DAss;
      long           key;
      C_DBCONNECTION *pConn;
      C_EED          eed;
      
      // ricavo il Selset dei blocchi DA
      if (SelSet.copy(DAss) == GS_BAD) return GS_BAD;
      DAss.intersectType(DA_BLOCK);

      // ricavo gli attributi visibili
      if ((VisValues << p_attrib_list->get_vis_values(ColValues)) == GS_BAD)
         return GS_BAD;
      if (attrib_block.set_layer(p_fas->layer) == GS_BAD) return GS_BAD;
      // altezza testo degli attributi del blocco
      if (attrib_block.set_h_text(p_fas->h_text) == GS_BAD) return GS_BAD;
      // stile testo degli attributi del blocco
      if (attrib_block.set_text_style(p_fas->style) == GS_BAD) return GS_BAD;
      // colore
      if (attrib_block.set_color(p_fas->color) == GS_BAD) return GS_BAD;
      // cerco "key_attrib"
      if ((p_rb_key = ColValues.CdrAssoc(p_class->ptr_info()->key_attrib.get_name())) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_rb2Lng(p_rb_key, &key) == GS_BAD) return GS_BAD;

      if (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO)
      {
         ads_point point;
         double    rot;
         ads_name  ent;
         C_SELSET  PrincSS;

         // ricavo il Selset degli oggetti grafici principali
         SelSet.copy(PrincSS);
         PrincSS.intersectType(GRAPHICAL);
         if (PrincSS.entname(0, ent) == GS_BAD) return GS_BAD;

         if ((p_id->category == CAT_SIMPLEX && p_id->type == TYPE_POLYLINE) ||
             (p_id->category == CAT_SUBCLASS && p_id->type == TYPE_POLYLINE))
         { // punto medio tratto + lungo
            if (gsc_getPntRtzOnObj(ent, point, &rot, _T("MML"), 0, 0, GS_GOOD) == GS_BAD) return GS_BAD;
         }
         else if ((p_id->category == CAT_SIMPLEX && p_id->type == TYPE_NODE) ||
                  (p_id->category == CAT_SUBCLASS && p_id->type == TYPE_NODE))
         { // punto di inserimento
            if (gsc_getPntRtzOnObj(ent, point, &rot, _T("S"), 0, 0, GS_GOOD) == GS_BAD) return GS_BAD;
         }
         else if ((p_id->category == CAT_SIMPLEX && p_id->type == TYPE_SURFACE) ||
                  (p_id->category == CAT_SUBCLASS && p_id->type == TYPE_SURFACE))
         { // centroide
            // scarto i riempimenti
            if (PrincSS.subtract(_T("HATCH")) == GS_BAD) return GS_BAD;
            if (gsc_get_centroidpoint(PrincSS, point) == GS_BAD) return GS_BAD;
            rot = 0.0;
         }
         else return GS_BAD;

         attrib_block.set_insert_point(point);
         attrib_block.set_rotation(gsc_rad2grd(rot));
      }

      if ((result = attrib_block.insert(VisValues, p_id->code, p_id->sub_code, key,
                                        p_fas->file_ref_block, p_fas->ref_block,
                                        GEOsimAppl::GLOBALVARS.get_InsPos())) != GS_GOOD)
         return result;

		attrib_block.get_ent_name(ent_name);
		SelSet.add(ent_name); // aggiungo il nuovo blocco DA

      eed.cls    = p_id->code;
      eed.sub    = p_id->sub_code;
      eed.num_el = SelSet.length(); // numero di oggetti grafici incrementato di 1 aggregazione
      eed.gs_id  = key;

      // aggiorno n. aggregate su tutti gli oggetti
      if (eed.save_aggr(SelSet, eed.num_el) == GS_BAD) return GS_BAD;

      attrib_block.get_ent_name(ent_blk);

      if (p_class->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // copio dati su tabella temporanea
      if ((pConn = p_class->ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;
      if (pConn->InsRow(TempTableRef.get_name(), ColValues, ONETEST, GS_BAD) == GS_BAD)
         // se il record era già nel temp non è errore
         if (GS_ERR_COD != eGSIntConstr) return GS_BAD;

      // Setto l'operazione corrente
      OldOp = set_GS_CURRENT_OPERATION((DAss.length() == 0) ? INSERT_FIRST_DABLOCK : MODIFY);

      // se esistono funzioni di calcolo grafico vengono applicate ora
      if (p_class->graph_calc(ColValues, &SelSet) == GS_BAD)
         { set_GS_CURRENT_OPERATION(OldOp); return GS_BAD; }
      
      // Setto l'operazione corrente
      set_GS_CURRENT_OPERATION(OldOp);

      // Lancio l'aggiornamento dell'entità che provvede anche ad
      // aggiungere in GEOsimAppl::SAVE_SS per salvataggio
      if (p_class->upd_data(key, ColValues, NULL, &SelSet) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_DefineDABlock                                    */
/*+
  Questa funzione definisce il blocco per visualizzare gli attributi visibili
  specificati nella lista passata come parametro. Il blocco NON deve già 
  esistere.
  Parametri:
  TCHAR         *BlkName;        Nome del blocco
  C_ATTRIB_LIST *pAttribList;    Lista degli attributi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_DefineDABlock(TCHAR *BlkName, C_ATTRIB_LIST *pAttribList)
{
   C_ATTRIB                *pAttrib = (C_ATTRIB *) pAttribList->get_head();
   int                     Result = GS_GOOD;
   AcDbAttributeDefinition *pAttDef;
   AcGePoint3d             basePoint(0.0, 0.0, 0.0);
   AcDbBlockTable          *pBlockTable;
   AcDbBlockTableRecord    *pBlockRecord;
   ads_real                Height, Dist, OffSet;

   // Open the block table for write.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForWrite) != Acad::eOk)
      return GS_BAD;

   // Se c'era già il blocco
   if (pBlockTable->has(BlkName))
   {
      pBlockTable->close();
      GS_ERR_COD = eGSInvalidBlock;
      return GS_BAD;
   }

   // Lo creo nuovo
   if ((pBlockRecord = new AcDbBlockTableRecord) == NULL)
      { pBlockTable->close(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   pBlockRecord->setName(BlkName);
   pBlockRecord->setOrigin(basePoint);

   if (pBlockTable->add(pBlockRecord) != Acad::eOk)
   {
      delete pBlockRecord;
      pBlockRecord->close();
      pBlockTable->close();
      GS_ERR_COD = eGSOutOfMem;
      return GS_BAD;
   } 

   // Offset del 60% dell'altezza
   Height = 1;
   OffSet = Height * 0.6;
   Dist   = Height + OffSet;

   // calcolo il punto di inserimento del primo attributo (30% in su)
   basePoint.y += (OffSet / 2);

   while (pAttrib)
   {
      if (pAttrib->is_visible() == GS_GOOD)
      {
         if ((pAttDef = new AcDbAttributeDefinition) == NULL)
            { Result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }

         pAttDef->setPosition(basePoint);
         pAttDef->setHeight(1.0);
         pAttDef->setRotation(0.0);
         pAttDef->setHorizontalMode(AcDb::kTextLeft);
         pAttDef->setVerticalMode(AcDb::kTextBase);

         pAttDef->setPrompt(pAttrib->get_name());
         pAttDef->setTextString(pAttrib->get_name());
         pAttDef->setTag(pAttrib->get_name());
         pAttDef->setInvisible(Adesk::kFalse);
         pAttDef->setVerifiable(Adesk::kFalse);
         pAttDef->setPreset(Adesk::kFalse);
         pAttDef->setConstant(Adesk::kFalse);

         if (pBlockRecord->appendAcDbEntity(pAttDef) != Acad::eOk)
           { pAttDef->close(); delete pAttDef; Result = GS_BAD; break; }
         pAttDef->close();

         // calcolo il punto di inserimento del prossimo attributo
         basePoint.y -= Dist;
      }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }

   pBlockRecord->close();
   pBlockTable->close();

   return Result;
}


/*********************************************************************/
/*.doc gsc_isOverlapRect                                  <internal> */
/*+
  Questa funzione verifica se un un rettangolo si sovrappone con quello
  occupato dall'entità ent. Funzione di ausilio a <gsc_isOverlapAttr>.
  Parametri:
  AcDbPolyline &Rect;      Rettangolo
  ads_name ent;            Attributo visibile

  La funzione restituisce true se i rettangoli si sovrappongono altrimenti false.
-*/  
/*********************************************************************/
bool gsc_isOverlapRect(AcDbPolyline &Rect, ads_name ent)
{
   AcDbPolyline RectEnt(4);
   AcGePoint2d  Vertice;
   ads_point    p1, p2, p3, p4;

   // Ricavo il rettangolo di occupazione dell'attributo ent
   gsc_get_txt_rectangle(ent, p1, p2, p3, p4);

   // Creo un' entità polyline per il rettangolo
   Vertice.set(p1[X], p1[Y]);
   RectEnt.addVertexAt(0, Vertice);
   Vertice.set(p2[X], p2[Y]);
   RectEnt.addVertexAt(1, Vertice);
   Vertice.set(p3[X], p3[Y]);
   RectEnt.addVertexAt(2, Vertice);
   Vertice.set(p4[X], p4[Y]);
   RectEnt.addVertexAt(3, Vertice);
   RectEnt.setClosed(Adesk::kTrue); // polilinea chiusa

   // Verifico se il rettangolo è interno o si interseca con quello <Rect>
   if (gsc_IsInternalEnt(&Rect, &RectEnt, CROSSING) == GS_GOOD) return true;
   // o viceversa
   if (gsc_IsInternalEnt(&RectEnt, &Rect, CROSSING) == GS_GOOD) return true;

   return false;
}

/*********************************************************************/
/*.doc gsc_isOverlapAttr                                  <internal> */
/*+
  Questa funzione verifica se un attributo si sovrappone ad altri attributi.
  Parametri:
  ads_name ent;               attributo visibile
  C_STR_BTREE &OverHandleList; lista degli handle degli attributi a cui si sovrappone

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_isOverlapAttr(ads_name attr, C_STR_BTREE &OverHandleList)
{
   C_RB_LIST    Attrib, CoordList;
   ads_point    p1, p2, p3, p4;
   C_SELSET     ssBox;
   long         i = 0;
   TCHAR        handleAttr[MAX_LEN_HANDLE], handle[MAX_LEN_HANDLE];
   ads_name     ent, ndx;
   presbuf      pAttrValue, prb;
   C_STRING     Str;
   AcDbPolyline Rect(4);
   AcGePoint2d  Vertice;

   OverHandleList.remove_all();

   if ((Attrib << acdbEntGet(attr)) == NULL) return GS_BAD;

   // Se l'attributo non era visibile
   if ((prb = Attrib.SearchType(70)) != NULL && prb->resval.rint & 1)
      return GS_GOOD;

   // Se l'attributo non aveva testo
   if ((pAttrValue = Attrib.SearchType(1)) == NULL) return GS_BAD;
   Str = pAttrValue->resval.rstring;
   Str.alltrim();
   if (Str.len() == 0) return GS_GOOD;

   prb = Attrib.SearchType(5);
   gsc_strcpy(handleAttr, prb->resval.rstring, MAX_LEN_HANDLE);

   // Ricavo il rettangolo di occupazione dell'attributo
   gsc_get_txt_rectangle(attr, p1, p2, p3, p4);

   // Creo un' entità polyline per il rettangolo
   Vertice.set(p1[X], p1[Y]);
   Rect.addVertexAt(0, Vertice);
   Vertice.set(p2[X], p2[Y]);
   Rect.addVertexAt(1, Vertice);
   Vertice.set(p3[X], p3[Y]);
   Rect.addVertexAt(2, Vertice);
   Vertice.set(p4[X], p4[Y]);
   Rect.addVertexAt(3, Vertice);
   Rect.setClosed(Adesk::kTrue); // polilinea chiusa

   // Filtro gli oggetti dentro il poligono di tipo DA_BLOCK (blocchi DA)
   CoordList << acutBuildList(RTPOINT, p1, RTPOINT, p2, RTPOINT, p3, RTPOINT, p4, 0);
   //                     CoordList,type     , ss   ,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
   if (gsc_selObjsPolygon(CoordList, CROSSING, ssBox, GS_BAD, GS_GOOD, GS_BAD, 0, 0, DA_BLOCK) == GS_BAD)
      return GS_BAD;

   // Ciclo sul gruppo di selezione
   while (ssBox.entname(i++, ent) == GS_GOOD)
   {
      // leggo gli attributi visibili
      ads_name_set(ent, ndx);
      while (ads_entnext(ndx, ndx) == RTNORM)
      {
         if ((Attrib << acdbEntGet(ndx)) == NULL) return GS_BAD;

         // se è una etichetta di "SEQEND"
         if ((prb = Attrib.SearchType(0)) == NULL || 
             gsc_strcmp(prb->resval.rstring, _T("SEQEND")) == 0)
            break;

         // Salto l'entità Attr
         gsc_enthand(ndx, handle);
         if (gsc_strcmp(handleAttr, handle) == 0) continue;

         // Se l'attributo non era visibile
         if ((prb = Attrib.SearchType(70)) != NULL && prb->resval.rint & 1)
            continue;

         // Se l'attributo non aveva testo
         if ((pAttrValue = Attrib.SearchType(1)) == NULL) return GS_BAD;
         Str = pAttrValue->resval.rstring;
         Str.alltrim();
         if (Str.len() == 0) continue;

         // Verifico se il rettangolo di occupazione di <attr> si interseca
         // con quello dell'attributo ndx
         if (gsc_isOverlapRect(Rect, ndx)) OverHandleList.add(handle);
      }
   }

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_InvOverlapAttr                                 <external> */
/*+
  Questa funzione spegne gli attributi visibili che si sovrappongono
  con altri attributi visibili.
  Parametri:
  C_SELSET &SelSet  : Gruppo di oggetti su cui operare
  int CounterToVideo;   flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
void gsInvOverlapAttr(void)
{
   int      ret;
   C_SELSET SelSet;

   GEOsimAppl::CMDLIST.StartCmd();

   do
   {
      if ((ret = gsc_ssget(NULL, NULL, NULL, NULL, SelSet)) != RTNORM)
         if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
         else return GEOsimAppl::CMDLIST.ErrorCmd();

      if (SelSet.length() > 0) break;
   }
   while (1);

   if (gsc_InvOverlapAttr(SelSet, GS_GOOD) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}
int gsc_InvOverlapAttr(C_SELSET &SelSet, int CounterToVideo)
{
   C_SELSET         DASelSet;
   ads_name         ent, ndx;
   long             i = 0;
   C_RB_LIST        Attrib;
   presbuf          prb;
   C_OVER_ATTR_LIST OverAttrList;
   C_OVER_ATTR      *pOverAttr;
   TCHAR            handle[MAX_LEN_HANDLE];
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(454)); // "Controllo sovrapposizione attributi"

   SelSet.copy(DASelSet);
   DASelSet.intersectType(DA_BLOCK); // Solo blocchi DA

   // se non ci sono oggetti nel gruppo di selezione
   if (DASelSet.length() == 0) return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(DASelSet.length());

   while (DASelSet.entname(i++, ent) == GS_GOOD)
   {
  	   if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      // leggo gli attributi visibili
      ads_name_set(ent, ndx);
      while (ads_entnext(ndx, ndx) == RTNORM)
      {
         if ((Attrib << acdbEntGet(ndx)) == NULL) break;
         // se è una etichetta di "SEQEND"
         if ((prb = Attrib.SearchType(0)) == NULL || 
             gsc_strcmp(prb->resval.rstring, _T("SEQEND")) == 0)
            break;

         if ((pOverAttr = new C_OVER_ATTR) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }

         // Ricavo gli attributi che si sovrappongono a questo attributo
         if (gsc_isOverlapAttr(ndx, pOverAttr->handle_list) == GS_GOOD && 
             pOverAttr->handle_list.get_count() > 0)
         {
            gsc_enthand(ndx, handle);
            ads_name_set(ent, pOverAttr->DABlock);
            pOverAttr->set_name(handle);
            pOverAttr->id = pOverAttr->handle_list.get_count();
            OverAttrList.add_tail(pOverAttr);
         }
         else delete pOverAttr;
      }
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      acutPrintf(gsc_msg(455)); // "\nAttributi spenti:\n"
      i = 0;
   }

   GEOsimAppl::REFUSED_SS.clear(); // Pulisco il gruppo degli oggetti rifiutati 
   gsc_startTransaction();

   C_SELSET ToAlign;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(454)); // "Controllo sovrapposizione attributi"

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

   // Ordino per numero di sovrapposizioni in ordine decrescente
   OverAttrList.sort_id(FALSE);

   while (OverAttrList.get_count() > 0)
   {
      pOverAttr = (C_OVER_ATTR *) OverAttrList.get_head();

      // spengo questo attributo
      if (ads_handent(pOverAttr->get_name(), ent) == RTNORM)
      {
         gsc_set_vis_attrib(ent, INVISIBLE);
         // Lo rimuovo dalla lista degli handle degli attributi
         OverAttrList.remove_handle(pOverAttr->get_name());

         ToAlign.add(pOverAttr->DABlock);

         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."
      }

      OverAttrList.remove(pOverAttr);
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."
      acutPrintf(gsc_msg(111)); // "\nTerminato.\n"
   }
   //               selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
   gsc_class_align(ToAlign, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS),
                   CounterToVideo, FAS_MOD);

   gsc_endTransaction();

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
/// INIZIO FUNZIONI CLASSE C_OVER_ATTR_LIST                           ///
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_OVER_ATTR_LIST::remove_handle           <internal> */
/*+
  Questa funzione cancella un handle da tutte le liste degli attributi 
  che si sovrappongono e se trova una lista di attribtui vuota rimuove 
  quell'elemento dalla lista.
  Parametri:
  const TCHAR *handle;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_OVER_ATTR_LIST::remove_handle(const TCHAR *handle)
{
   C_OVER_ATTR *pOverAttr = (C_OVER_ATTR *) get_head();

   while (pOverAttr)
   {
      if (pOverAttr->handle_list.remove(handle) == GS_GOOD &&
          (pOverAttr->id = pOverAttr->handle_list.get_count()) == 0)
      {
         remove_at();
         pOverAttr = (C_OVER_ATTR *) get_cursor();
      }
      else
         pOverAttr = (C_OVER_ATTR *) get_next();
   }
   
   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
//  FINE  FUNZIONI CLASSE C_OVER_ATTR_LIST                             //
/////////////////////////////////////////////////////////////////////////


/*********************************************************************/
/*.doc gsTopDrawOrderAttr                                 <external> */
/*+
  Questo comando pone in primo piano gli attributi visibili selezionati.

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
void gsTopDrawOrderAttr(void)
{
   int      ret;
   C_SELSET SelSet;
   ads_name dummySS;

   GEOsimAppl::CMDLIST.StartCmd();

   do
   {
      if ((ret = gsc_ssget(NULL, NULL, NULL, NULL, SelSet)) != RTNORM)
         if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();
         else return GEOsimAppl::CMDLIST.ErrorCmd();

      if (SelSet.length() > 0) break;
   }
   while (1);

   SelSet.intersectType(DA_BLOCK); // Solo blocchi DA
   if (SelSet.length() == 0) return GEOsimAppl::CMDLIST.EndCmd();

   SelSet.get_selection(dummySS);
   // Non uso le API perchè non sono ufficialmente documentate perchè cambieranno
   // in futuro (non sono nella versione definitiva). Letto da ADN cercando draworder.
   if (gsc_callCmd(_T("_.DRAWORDER"), RTPICKS, dummySS, 
                   RTSTR, GS_EMPTYSTR, RTSTR, _T("_F"), 0) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   gsc_callCmd(_T("_.REGEN"), 0); // altrimenti non si vede cambiamento

   return GEOsimAppl::CMDLIST.EndCmd();
}


/****************************************************************************/
/*.doc gsc_dbOpenAttribute                                       <internal> */
/*+
  Questa funzione apre gli oggetti AcDb di un blocco DA cercando
  un attributo noto.
  Parametri:
  ads_name DABlock;              oggetto blocco DA
  const TCHAR *AttribName;       nome dell'attributo
  enum AcDb::OpenMode Mode;      modalità di apertura
  AcDbBlockReference **pBlock;   puntatore all'oggetto AcDb blocco (out)
  AcDbAttribute **pAttrib;       puntatore all'oggetto AcDb attributo (out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_dbOpenAttribute(ads_name DABlock, const TCHAR *AttribName, 
                        enum AcDb::OpenMode Mode,
                        AcDbBlockReference **pBlock, AcDbAttribute **pAttrib)
{
   AcDbObjectId       objId;
   AcDbObjectIterator *pAttrIter;
   int                res = GS_BAD;

   if (acdbGetObjectId(objId, DABlock) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }  
   if (acdbOpenObject(*pBlock, objId, Mode) != Acad::eOk)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   pAttrIter = (* pBlock)->attributeIterator();

	for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
   {
	   objId = pAttrIter->objectId();

      if ((* pBlock)->openAttribute(*pAttrib, objId, AcDb::kForRead) != Acad::eOk)
         break;

      if (wcsicmp(AttribName, (* pAttrib)->tagConst()) == 0)
      {
         if (Mode == AcDb::kForWrite)
            if ((* pAttrib)->upgradeOpen() != Acad::eOk)
               { (*pAttrib)->close(); break; }

         res = GS_GOOD;
         break;
      }

      (* pAttrib)->close();
   }

   if (res == GS_BAD) (* pBlock)->close();

   return res;
}


/****************************************************************************/
/*.doc gsc_getAttributeFromBlock                                 <external> */
/*+
  Questa funzione cerca un attributo in un blocco.
  Parametri:
  ads_name Block;              oggetto blocco
  const TCHAR *AttribName;     nome dell'attributo
  ads_name Attrib;             oggetto attributo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_getAttributeFromBlock(ads_name Block, const TCHAR *AttribName, ads_name Attrib)
{
   C_RB_LIST Descr;
   presbuf   pRb;

   // leggo gli attributi visibili
   ads_name_set(Block, Attrib);
   while (ads_entnext(Attrib, Attrib) == RTNORM)
   {
      if ((Descr << acdbEntGet(Attrib)) == NULL) return GS_BAD;
      // se è una etichetta di "SEQEND"
      if ((pRb = Descr.SearchType(0)) == NULL || 
          gsc_strcmp(pRb->resval.rstring, _T("ATTRIB")) != 0)
         return GS_BAD;
      // nome dell'attributo
      if ((pRb = Descr.SearchType(2)) == NULL) return GS_BAD;

      if (gsc_strcmp(pRb->resval.rstring, AttribName, GS_BAD) == 0) break;
   }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_RbList2DAStrFmtRbList                                 <internal> */
/*+
  Converte una lista di resbuf ((<nome attrib> <valore attrib>) ...)
  in una lista in cui i valori degli attributi vengono convertiti
  in stringhe formattate per la loro visualizzazione in blocchi DA.
  Parametri:
  C_ATTRIB_LIST *pAttribList;    Lista degli attributi
  C_RB_LIST &ColValues;          Lista resbuf attributo-valore
  C_RB_LIST &DAStrFmtRbList;     Lista resbuf formattata per blocchi DA
  int cls;                       Codice classe di entità (opzionale per leggere dati 
                                 codificati, default = 0)
  int sub;                       Codice sottoclasse (opzionale per leggere dati 
                                 codificati, default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_RbList2DAStrFmtRbList(C_ATTRIB_LIST *pAttribList, C_RB_LIST &ColValues, 
                              C_RB_LIST &DAStrFmtRbList, int cls, int sub)
{
   C_ATTRIB *pAttrib;
   presbuf  p;
   int      i = 0;
   C_STRING str, text;
   C_STR   *pDescrFrom_TAB_REF;

   if (ColValues.copy(DAStrFmtRbList) == GS_BAD) return GS_BAD;

   while ((p = DAStrFmtRbList.nth(i++)))
   {
      if ((pAttrib = (C_ATTRIB *) pAttribList->search_name(p->rbnext->resval.rstring, FALSE)) == NULL)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

      // Salto gli attributi non visibili
      if (pAttrib->is_visible() == GS_BAD) continue;

      // Verifico se ci sono dei valori codificati
      pDescrFrom_TAB_REF = NULL;
      if (GS_CURRENT_WRK_SESSION)
         pDescrFrom_TAB_REF = GS_CURRENT_WRK_SESSION->get_pCacheClsAttribValuesList()->get_Descr(pAttrib->name, ColValues,
                                                                                          GS_CURRENT_WRK_SESSION->get_PrjId(),
                                                                                          cls, sub);

      // se il valore è letto da TAB o REF
      if (pDescrFrom_TAB_REF)
      {
         text = pAttrib->GphPrefix;
         text += pDescrFrom_TAB_REF->get_name2();
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());
      }
      else
      // converto gli attributi numerici con lo standard (123.456 e NON con le impostazioni di window)
      if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD)
      {  // aggiungo eventuale prefisso e suffisso
         if (p->rbnext->rbnext->restype == RTNIL || p->rbnext->rbnext->restype == RTNONE) continue;
         text = pAttrib->GphPrefix;
         str.paste(gsc_rb2str(p->rbnext->rbnext, pAttrib->len, pAttrib->dec));
         text += str;
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());
      }
      else
      // converto gli attributi data con lo standard (dd/mm/yyyy e NON con le impostazioni di window)
      if (gsc_DBIsDate(pAttrib->ADOType) == GS_GOOD || gsc_DBIsTimestamp(pAttrib->ADOType) == GS_GOOD)
      {
         if (p->rbnext->rbnext->restype == RTNIL || p->rbnext->rbnext->restype == RTNONE) continue;
         // aggiungo eventuale prefisso e suffisso
         text = pAttrib->GphPrefix;
         if (gsc_getGEOsimDateTime(p->rbnext->rbnext->resval.rstring, str) == GS_BAD)
            return GS_BAD;
         text += str;
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());
      }
      else
      // converto gli attributi data con lo standard (hh:mm:ss)
      if (gsc_DBIsTime(pAttrib->ADOType) == GS_GOOD)
      {
         if (p->rbnext->rbnext->restype == RTNIL || p->rbnext->rbnext->restype == RTNONE) continue;
         // aggiungo eventuale prefisso e suffisso
         text = pAttrib->GphPrefix;
         if (gsc_getSQLTime(p->rbnext->rbnext->resval.rstring, str) == GS_BAD)
            return GS_BAD;
         text += str;
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());
      }
      else
      // converto gli attributi logici
      if (gsc_DBIsBoolean(pAttrib->ADOType) == GS_GOOD)
      {
         if (p->rbnext->rbnext->restype == RTSTR) continue;
         // aggiungo eventuale prefisso e suffisso
         text = pAttrib->GphPrefix;
         str  = (p->rbnext->rbnext->restype == RTT) ? gsc_msg(774) : gsc_msg(775); // Sì No
         text += str;
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());         
      }
      else
      {
         if (p->rbnext->rbnext->restype != RTSTR) continue;
         // aggiungo eventuale prefisso e suffisso
         text = pAttrib->GphPrefix;
         text += p->rbnext->rbnext->resval.rstring;
         text += pAttrib->GphSuffix;
         gsc_RbSubst(p->rbnext->rbnext, text.get_name());
      }
   }

   return GS_GOOD;
}