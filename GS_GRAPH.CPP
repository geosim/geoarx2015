/**********************************************************
Name: GS_GRAPH.CPP

Module description: File per interfaccia alle operazioni con la grafica
            
Author: Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS  S.p.A.

              
Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/

#include "stdafx.h" 

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")
   
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <math.h>
#include <ctype.h>       /*  per isdigit() */
#include <fcntl.h>
#include <string.h>      /*  per strcat() strcmp()  */
#include <limits>

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>
#include "adsdlg.h"   
#include "acutmem.h"
#include <actrans.h>

#include <aced.h>
#include <acdb.h>
#include <dbents.h>
#include <dbelipse.h>
#include <dblead.h>
#include <dbpl.h>
#include <dbray.h>
#include <dbsol3d.h>
#include <dbspline.h>
#include <dbxline.h>
#include <dbhatch.h>
#include <dbsymtb.h>
#include <dbidmap.h>
#include <dbapserv.h>
#include <dbMPolygon.h>         // per centroidi
#include <AcMapUtilities.h>     // per centroidi
#include "topoads.h"      // per funzionalità topologiche
#include "acedCmdNF.h" // per acedCommandS

#include "gs_opcod.h"     // codici delle operazioni
#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori

#include "gs_resbf.h"     // gestione resbuf
#include "gs_list.h"      // gestione liste C++ 
#include "gs_dbref.h"     // prototipi funzioni gestione riferimenti a db
#include "gs_init.h" 
#include "gs_graph.h" 
#include "gs_query.h" 
#include "gs_attbl.h"     // gestione blocchi attributi visibili
#include "gs_lisp.h"      // per validita' e calcolo attributi
#include "gs_utily.h" 
#include "gs_topo.h"      // per "gsc_OverlapValidation"

#include "d2hMap.h" // doc to help


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
#endif


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

double _SCALE, XSCALE, YSCALE, ROTA;                                 
ads_point  BASE, GS_POINT_CURSOR;

int GS_LAST_NODE_CLS=0;  // CLASSE ULTIMO NODO INSERITO PER CLASSI simulazioni
int GS_LAST_NODE_SUB=0;  // SOTTOCLASSE ULTIMO NODO INSERITO PER CLASSI simulazioni


C_CLASS  *SEL_CLS;     // PUNT. CLASSE O SOTTOCLASSE SCELTA //
C_CLASS  *SEL_EXT;     // PUNT. CLASSE-SIMULAZIONE SCELTA //


// struttura usata per scambiare dati nelle dcl "per inserimento dati"
struct Common_Dcl_DataIns_Struct
{
   int mode;
   C_CLS_PUNT_LIST SelClsList; // LISTA PUNT. CLASSI SELEZ.
   C_CLS_PUNT_LIST SelSubList; // LISTA PUNT. SOTTOCLASSI SELEZ.
};

// INSERIMENTO ATTRIBUTI ENTITA'
///////////////////////////////////////
static ads_matrix ads_identmat =  {
                              {1.0, 0.0, 0.0, 0.0},
                              {0.0, 1.0, 0.0, 0.0},
                              {0.0, 0.0, 1.0, 0.0},
                              {0.0, 0.0, 0.0, 1.0}
                           };

// VECCHIO VALORE VARIBILI GLOBALI EXPERT E CMDECHO
static int GS_LAST_ECHO, GS_LAST_EXPERT; // gsc_set_env_cmd() gsc_rest_env_cmd()


///////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS

const TCHAR *gsc_get_graphical_data_type(TCHAR *what);
double gsc_get_graphical_area(C_SELSET &SelSet);
int SelectGrid(ads_point pt, long *gs_id, C_CLS_PUNT_LIST &SelClsList);
int gsc_getSplitMode(ads_point pt, ads_name ent, C_INT_INT *punt);
int gsc_getPtList_BetweenPts(AcGePoint3d &ptStart, AcGePoint3d &ptEnd, double Bulge, 
                             double DistanceFromStart, double Length, 
                             C_POINT_LIST &PtList, double *MyLength = NULL);


/*********************************************************/
/*.doc gsc_zoom                                 <extern> */
/*+
   Effettua la funzione di ZOOM WINDOW.
   Parametri:
   AcGePoint2d min_2d;  coordinate in basso a sx
   AcGePoint2d max_2d;  coordinate in alto a dx
   ads_real min_dim_x;   dimensione minima della finestra sull'asse x (default = 0)
   ads_real min_dim_y;   dimensione minima della finestra sull'asse y (default = 0)
-*/
/*********************************************************/
void gsc_zoom(ads_point min_2d, ads_point max_2d,
              ads_real min_dim_x, ads_real min_dim_y)
{
   AcGePoint2d _min_2d(min_2d[X], min_2d[Y]);
   AcGePoint2d _max_2d(max_2d[X], max_2d[Y]);

   gsc_zoom(_min_2d, _max_2d, min_dim_x, min_dim_y);
}
void gsc_zoom(AcGePoint2d min_2d, AcGePoint2d max_2d,
              ads_real min_dim_x, ads_real min_dim_y)
{
   AcDbViewTableRecord view;

   if (min_dim_x > 0 && min_dim_y > 0)
   {
      double offset;

      // correggo l'estensione della finestra se è troppo piccola
      if ((max_2d.x - min_2d.x) < min_dim_x)
      {
         offset = (min_dim_x - (max_2d.x - min_2d.x)) / 2;
         min_2d.x -= offset;
         max_2d.x += offset;
      }
      if ((max_2d.y - min_2d.y) < min_dim_y)
      {
         offset = (min_dim_y - (max_2d.y - min_2d.y)) / 2;
         min_2d.y -= offset;
         max_2d.y += offset;
      }
   }

   // now set the view centre point
   view.setCenterPoint (min_2d + (max_2d - min_2d) / 2.0); 
   // now height and width of view
   view.setHeight(max_2d[Y] - min_2d[Y]); 
   view.setWidth (max_2d[X] - min_2d[X]); 
   // set the view 
   acedSetCurrentView (&view, NULL);
   // updates the extents  
   acdbHostApplicationServices()->workingDatabase()->updateExt(TRUE);  
}


/*********************************************************/
/*.doc gsc_zoom_extents                         <extern> */
/*+
   Effettua la funzione di ZOOM EXTENTS.
   Parametri:
   ads_real min_dim_x;   dimensione minima della finestra sull'asse x (default = 0)
   ads_real min_dim_y;   dimensione minima della finestra sull'asse y (default = 0)
-*/
/*********************************************************/
void gsc_zoom_extents(ads_real min_dim_x, ads_real min_dim_y)
{
   // get the extents of the drawing
   AcGePoint3d         max = acdbHostApplicationServices()->workingDatabase()->extmax();
   AcGePoint3d         min = acdbHostApplicationServices()->workingDatabase()->extmin();
   AcGePoint2d         max_2d(max[X], max[Y]);
   AcGePoint2d         min_2d(min[X], min[Y]);

   return gsc_zoom(min_2d, max_2d);
}


/****************************************************************************/
/*.doc gsc_GrDrawCross                                                      */
/*+
  Questa funzione disegna in modalità grdraw una croce nel punto indicato.
  Parametri:
  ads_point pt;

  La funzione restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/****************************************************************************/
int gs_GrDrawCross(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();
   if (!arg || (arg->restype != RTPOINT && arg->restype != RT3DPOINT))
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   if (gsc_GrDrawCross(arg->resval.rpoint) == GS_BAD)
      return RTERROR;
   acedRetT();
   return RTNORM;
}
int gsc_GrDrawCross(ads_point pt)
{
   double    factor = 50, offset;
   resbuf    res;
   ads_point p1, p2;

   // leggo il valore di VIEWSIZE (altezza della finestra) 
   // e parametrizzo il valore della croce
   if (acedGetVar(_T("VIEWSIZE"), &res) != RTNORM) return GS_BAD;

   offset = res.resval.rreal / factor;
   p1[Z] = p2[Z] = 0.0;

   p1[X] = pt[X] - offset;
   p1[Y] = pt[Y] + offset;
   p2[X] = pt[X] + offset;
   p2[Y] = pt[Y] - offset;
   // disegno un ramo della croce 
   if (acedGrDraw(p1, p2, -1, 0) != RTNORM) return GS_BAD;

   p1[X] = pt[X] - offset;
   p1[Y] = pt[Y] - offset;
   p2[X] = pt[X] + offset;
   p2[Y] = pt[Y] + offset;
   // disegno l'altro ramo della croce 
   if (acedGrDraw(p1, p2, -1, 0) != RTNORM) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_IsErasedEnt                          <extern> */
/*+
   Verifica se l'oggetto è stato cancellato o ancora esistente.
   Parametri:
   ads_name ent;     oggetto grafico

   Ritorna GS_GOOD se l'oggetto è stato cancellato altrimenti GS_BAD
-*/
/*********************************************************/
int gsc_IsErasedEnt(ads_name ent)
{
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   return (objId.isEffectivelyErased()) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_UnEraseEnt                           <extern> */
/*+
   Se l'oggetto è stato cancellato lo ripristina.
   Parametri:
   ads_name ent;     oggetto grafico

   Ritorna GS_GOOD se l'oggetto è stato cancellato altrimenti GS_BAD
-*/
/*********************************************************/
int gsc_UnEraseEnt(ads_name ent)
{
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return gsc_UnEraseEnt(objId);
}
int gsc_UnEraseEnt(AcDbObjectId objId)
{
   if (objId.isErased())
   {
      AcDbObject *pObj;

      if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      // ripristino l'oggetto grafico
      if (pObj->erase(Adesk::kFalse) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (pObj->close() != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_EraseEnt                              <external>*/
/*+
   Cancella l'oggetto grafico solo se non è già stato cancellato.
   (a differenza di acdbEntDel che in questo caso ripristina l'oggetto)
   Parametri:
   ads_name ent;     oggetto grafico

   Ritorna GS_GOOD se l'oggetto è stato cancellato altrimenti GS_BAD.
-*/
/*********************************************************/
int gsc_EraseEnt(ads_name ent)
{
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   
   return gsc_EraseEnt(objId);
}
int gsc_EraseEnt(AcDbObjectId &objId)
{
   AcDbObject *pObj;

   if (!objId.isErased())
   {
      if (acdbOpenObject(pObj, objId, AcDb::kForWrite, false) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      // cancello l'oggetto grafico
      if (pObj->erase(Adesk::kTrue) != Acad::eOk )
         { pObj->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (pObj->close() != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }

   return GS_GOOD;
}
int gsc_EraseEnt(AcDbObjectIdArray &objIds)
{
   int i, len = objIds.length();

	for (i = 0; i < len; i++)
      if (gsc_EraseEnt(objIds.at(i)) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


//////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI DI C_CLASS
//////////////////////////////////////////////////////////////////////////


/*******************************************************/
/*.doc int gs_insert_ent()                                 */
/*+                                                                   
  Funzione lisp per disegnare in modo guidato degli oggetti in GEOsim.
  Parametri:
  (<cls><check_con>)
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gs_insert_ent(void)
{
   presbuf arg = acedGetArgs();
   int     cls, check_con = 0;
   C_CLASS *pCls;
   C_SELSET SSOut;
   ads_name ss;

   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // codice classe
   if (!arg  || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   cls = (int) arg->resval.rint;

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return RTERROR;

   if ((arg = arg->rbnext) != NULL)
      if (arg->restype == RTT)
         check_con = 1;

   if (pCls->InsertEnt(NULL, check_con, NULL, &SSOut) != GS_GOOD) return RTERROR;

   SSOut.get_selection(ss);
   acedRetName(ss, RTPICKS);
   SSOut.ReleaseAllAtDistruction(GS_BAD);

   return RTNORM;
}


int C_CLASS::InsertEnt(ads_point start, int check_con, ads_name ent,
                       C_SELSET *pSSOut, long *gs_id)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::InsertEnt(C_SUB *cls, C_SELSET *pSSOut, bool Undo)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DI C_CLASS
// INIZIO FUNZIONI DI C_SIMPLEX
//////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_SIMPLEX::InsertEnt                   <external> */
/*+
   Funzioni per inserimento di un'entita di GEOsim.
   L'utente viene guidato per l'inserimento della sola parte grafica 
   a cui viene successivamente associata la scheda di default.
   Parametri:
   ads_point start;     Se <> NULL viene usato come punto iniziale di riferimento
                        (es. per oggetti lineari)
   int check_con;       Flag di controllo, se = FALSE non effettua alcun
                        controllo sulle connessioni.
   ads_name con_ent;    Usato se check_con = TRUE. La funzione cerca
                        (se possibile) la connessione con questa entità
   C_SELSET *pSSOut;    Se <> NULL gruppo di selezione con l'oggetto grafico
                        principale dell'entità inserita
   long *gs_id;         Codice nuova entità (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::InsertEnt(ads_point start, int check_con, ads_name con_ent,
                         C_SELSET *pSSOut, long *gs_id)
{
   C_FAS           OldGraphEnv;
   int             ret = GS_GOOD, SplitMode;
   ads_point       ins;
   ads_pointp      point = NULL;
   ads_name        ent;
   C_SELSET        entSS;
   C_RB_LIST       ColValues;
   C_CLS_PUNT      *pEntCls;
   C_CLS_PUNT_LIST EntClsList;
   C_STRING        TextValue;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (pSSOut) pSSOut->clear();

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GS_BAD;

   // Controlla l'esistenza di eventuali connessioni
   if (check_con && ptr_connect_list()->is_to_be_connected() == GS_GOOD)
   {
      ads_name EntToConnectTo;

      if (con_ent) ads_name_set(con_ent, EntToConnectTo);
      else ads_name_clear(EntToConnectTo);

      if ((ret = gsc_get_connect_point(this, start, EntToConnectTo, 
                                       &point, &SplitMode)) == GS_CAN)
         { gsc_abortTransaction(); return GS_CAN; }
      if (ret == GS_BAD) { gsc_abortTransaction(); return GS_BAD; }

      if (point)
      {  // Elimina allocazioni dinamiche
         ads_point_set(point, ins); free(point); point = ins; 

         // Se si deve dividere l'entità di connessione
         if (SplitMode == SOFT_SPLIT || SplitMode == HARD_SPLIT)
         {  // La spezzo
            ads_name EntLast;

            // Memorizzo ultima entita.
            if (acdbEntLast(EntLast) != RTNORM) 
               { gsc_abortTransaction(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

            if (gsc_break(EntToConnectTo, ins) == GS_GOOD && SplitMode == SOFT_SPLIT)
            {
               C_SELSET NewSS;
               ads_name sel;

               // Aggrego i due pezzi

               // creo gruppo di selezione delle entità nuove generate da "break"
               while (gsc_mainentnext(EntLast, EntLast) == GS_GOOD)
                  NewSS.add(EntLast);
               // Scarto eventuali blocchi DA
               NewSS.intersectType(GRAPHICAL);
               // Se il "break" ha generato 2 nuovi oggetti grafici
               // prendo il primo come oggetto a cui aggregare il secondo
               if (NewSS.length() > 1)
               {
                  NewSS.entname(0, EntToConnectTo);
                  NewSS.subtract_ent(EntToConnectTo);
               }
               NewSS.get_selection(sel);
               aggr_data(sel, EntToConnectTo);
            }
         }
      }
   }
   else 
   {
      point = start;
      if (point && check_con) // se esiste il punto e si devono controllare le connessioni
         // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
         if (gsc_OverlapValidation(start, this) == GS_BAD)
            { gsc_abortTransaction(); GS_ERR_COD = eGSOverlapValidation; return GS_BAD; }
   }

   if (gsc_setenv_graph(id.category, id.type, fas, &OldGraphEnv) == GS_BAD)
      { gsc_abortTransaction(); return GS_BAD; }

   do
   {
      // A seconda del tipo inserisce l'entita' grafica corrispondente
      switch (id.type)
      {
         case TYPE_SURFACE :
         case TYPE_POLYLINE :
            if ((ret = gsc_insert_pline(point, (check_con) ? this : NULL)) != GS_GOOD)
               break;
            acdbEntLast(ent);
            if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
            EntClsList.add_tail(pEntCls);
            // Setto la scala del tipo linea
            if (gsc_set_lineTypeScale(ent, fas.line_scale) == GS_BAD) { ret = GS_BAD; break; }

            // Se SURFACE chiude polilinea
            if (id.type == TYPE_SURFACE)
            {
               if (gsc_close_pline(ent) == GS_BAD) { ret = GS_BAD; break; }
               // se impostato un riempimento lo applica
               if (fas.hatch && wcslen(fas.hatch) > 0)
               {
                  // se ritorna GS_CAN la superficie era troppo piccola per
                  // contenere il riempimento
                  if ((ret = gsc_setHatchEnt(ent, fas.hatch, fas.hatch_scale,
                                             fas.hatch_rotation, &fas.hatch_color,
                                             fas.hatch_layer)) == GS_BAD)
                     break;
            
                  if (ret == GS_GOOD)
                  {
                     acdbEntLast(ent); // aggiungo anche il riempimento
                     if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
                        { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
                     EntClsList.add_tail(pEntCls);
                  }
                  ret = GS_GOOD;
               }
            }
            break;
                                   
         case TYPE_TEXT :
         {
            C_ATTRIB *pTextAttr;

            // cerco il valore di default dell'attributo visibile
            pTextAttr = ptr_attrib_list()->getFirstVisibleAttrib();
            if ((ret = gsc_insert_text(fas.style, point, fas.h_text, fas.rotation,
                                       NULL, 
                                       (pTextAttr->def && pTextAttr->def->restype == RTSTR) ? pTextAttr->def->resval.rstring : _T("?"),
                                       (check_con) ? this : NULL)) != GS_GOOD)
               break;
            acdbEntLast(ent);
            gsc_getInfoText(ent, &TextValue);
            if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
            EntClsList.add_tail(pEntCls);

            break;
         }

         case TYPE_NODE :
            if ((ret = gsc_insert_block(fas.block, point, fas.block_scale, fas.rotation,
                                       (check_con) ? this : NULL)) != GS_GOOD)
               break;
            acdbEntLast(ent);
            if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
            EntClsList.add_tail(pEntCls);
            break;

         default : 
            acutPrintf(gsc_msg(217)); // "\nNON DISPONIBILE IN QUESTA VERSIONE...\n"
            ret = GS_BAD; 
            break;
      }

      if (ret == GS_BAD || ret == GS_CAN) break;

      // Inserimento di entità GEOsim usando i valori attributi di default
      if ((ret = get_default_values(ColValues)) == GS_BAD) break;

      // Se si tratta di testo devo modificare il valore del testo
      if (id.type == TYPE_TEXT)
         if (ColValues.CdrAssocSubst(ptr_attrib_list()->getFirstVisibleAttrib()->get_name(),
                                     TextValue.get_name()) == GS_BAD)
            break;

      if ((ret = ins_data(&EntClsList, ColValues, gs_id)) != GS_GOOD)
         break;
      // la funzione ins_data potrebbe variare la prima entità di EntClsList
      pEntCls = (C_CLS_PUNT *) EntClsList.get_head();
      ads_name_set(pEntCls->ent, ent);
      if (pSSOut) pSSOut->add(ent);

      GEOsimAppl::LAST_CLS = id.code;
      GEOsimAppl::LAST_SUB = id.sub_code;
   }
   while (0);

   // Ripristina situazione FAS precedente
   gsc_setenv_graph(id.category, id.type, OldGraphEnv);

   if (ret != GS_GOOD) { gsc_abortTransaction(); return ret; }

   // End Undo
   gsc_endTransaction();

   return ret;   
}


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DI C_SIMPLEX
// INIZIO FUNZIONI DI C_GROUP
//////////////////////////////////////////////////////////////////////////


presbuf C_GROUP::get_graphical_data(long code, TCHAR *what, TCHAR *type)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/*********************************************************/
/*.doc C_GROUP::InsertEnt                    <external> */
/*+
   Funzioni per inserimento di un'entita di GEOsim.
   L'utente viene guidato per la selezione della sola parte grafica 
   a cui viene successivamente associata la scheda di default.
   Parametri:
   ads_point start;     Usato solo per compatibilità
   int check_con;       Usato solo per compatibilità
   ads_name con_ent;    Usato solo per compatibilità
   C_SELSET *pSSOut;    Se <> NULL gruppo di selezione con gli oggetti grafici
                        membri dell'entità inserita
   long *gs_id;         Codice nuova entità (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GROUP::InsertEnt(ads_point start, int check_con,  ads_name con_ent,
                       C_SELSET *pSSOut, long *gs_id)
{
   C_SELSET        ss;
   long            i;
   ads_name        ent;
   C_CLASS         *pCls;
   C_EED           eed;
   C_GROUP_LIST    group_list;
   C_INT_INT       *pMember;
   C_CLS_PUNT_LIST ent_list;
   C_CLS_PUNT      *punt;
   int             ret;
   long            Key, NotMemberObjs;
   C_RB_LIST       ColValues;
   bool            InvalidGroup;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (pSSOut) pSSOut->clear();

   do
   {
      NotMemberObjs = 0;
      InvalidGroup  = false;
      ent_list.remove_all();
      // Copio la lista che descrive la composizione del gruppo
      ptr_group_list()->copy(&group_list);

      do
      {
         if (gsc_ssget(NULL, NULL, NULL, NULL, ss) != RTNORM) return GS_CAN;
         if (ss.length() > 0) break;
      }
      while (1);

      // Controllo che il gruppo di selezione sia compatibie con il gruppo
      // Scarto gli oggetti che non sono delle classi componenti il gruppo
      i = 0;
      while (ss.entname(i++, ent) == GS_GOOD)
      {
         if (eed.load(ent) != GS_GOOD) // solo entità di GEOsim
            { NotMemberObjs++; continue; }
         // solo entità membri del gruppo   
         if ((pMember = (C_INT_INT *) ptr_group_list()->search_key(eed.cls)) == NULL)
            { NotMemberObjs++; continue; }
         // Cerco caratteristiche classe
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
            { NotMemberObjs++; continue; }
         if (pCls->getKeyValue(ent, &Key) == GS_BAD)
            { NotMemberObjs++; continue; }
         // Se l'entità non era ancora stata selezionata
         if (ent_list.search_ClsKey(pCls, Key) == NULL)
         {
            // Verifico la sua modificabilità ed eventualmente la estraggo totalmente
            if (pCls->is_updateable(Key, NULL, GS_GOOD, GS_GOOD) != GS_GOOD)
            {
               acutPrintf(gsc_msg(770), 1); // "\n%ld entità GEOsim bloccata/e da un' altro utente."
               InvalidGroup = true; 
               break; 
            }
            // Se si è definito un numero di entità per la classe
            if (pMember->get_type() > 0)
            {
               pMember = (C_INT_INT *) group_list.search_key(eed.cls);
               if (pMember->get_type() == 0) // se non si devono aggiungere altre entità
               {
                  pMember = (C_INT_INT *) ptr_group_list()->search_key(eed.cls);
                  // "\nSono state selezionate troppe entità della classe %s (il gruppo ne deve contenere %d)."
                  acutPrintf(gsc_msg(211), pCls->get_name(), pMember->get_type());                 
                  InvalidGroup = true; 
                  break; 
               }
               pMember->set_type(pMember->get_type() - 1); // decremento il n. di entità
            }

            // La aggiungo alla lista
            if ((punt = new C_CLS_PUNT(pCls, ent, Key)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            ent_list.add_tail(punt);

            if (pSSOut) pSSOut->add(ent);
         }
         else // se l'entità era già stata selezionata
            if (pSSOut) pSSOut->add(ent);
      }

      if (InvalidGroup) continue;

      acutPrintf(gsc_msg(227), ss.length(), NotMemberObjs); // "\nOggetti grafici elaborati %ld, scartati %ld."

      // Valuto se il gruppo è completo o se deve essere selezionata qualche altra entità
      pMember = (C_INT_INT *) group_list.get_head();
      while (pMember)
      {
         if (pMember->get_type() > 0) // Se è rimasta ancora qualche entità da aggiungere
         {
            int Tot = ptr_group_list()->search_key(pMember->get_key())->get_type();
            pCls = GS_CURRENT_WRK_SESSION->find_class(pMember->get_key());
            // "\nSono state selezionate solo %d entità della classe %s (il gruppo ne deve contenere %d)."
            acutPrintf(gsc_msg(212), Tot - pMember->get_type(), pCls->get_name(), Tot);
            InvalidGroup = true;
         }

         pMember = (C_INT_INT *) pMember->get_next();
      }

      if (InvalidGroup) continue;

      break;
   }
   while (1);

   // Inserimento di entità GEOsim usando i valori attributi di default
   if (get_default_values(ColValues) == GS_BAD) return GS_BAD;

   if ((ret = ins_data(&ent_list, ColValues, gs_id)) == GS_GOOD)
   {
      GEOsimAppl::LAST_CLS = id.code;
      GEOsimAppl::LAST_SUB = id.sub_code;
   }

   return ret;
}


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DI C_GROUP
// INIZIO FUNZIONI DI C_EXTERN
//////////////////////////////////////////////////////////////////////////


presbuf C_EXTERN::get_graphical_data(long code, TCHAR *what, TCHAR *type)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/*********************************************************/
/*.doc C_EXTERN::InsertEnt                    <external> */
/*+
   Funzioni per inserimento di una o più entita di GEOsim.
   L'utente viene guidato per l'inserimento della sola parte grafica 
   a cui viene successivamente associata la/e scheda/e di default.
   Parametri:
   C_SUB *pSub;       Puntatore alla sottoclasse che si vuole inserire
   C_SELSET *pSSOut;  Se <> NULL puntatore a gruppo di selezione degli 
                      oggetti grafici principali delle entità inserite
   bool  Undo;        Se = TRUE la procedura gestisce gli undo (default = TRUE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
int C_EXTERN::InsertEnt(C_SUB *pSub, C_SELSET *pSSOut, bool Undo)
{
   C_EED          ent_EED;
   C_CONNECT_LIST *con_list, *con_tmp;
   TCHAR          str[60];
   int            direct; // flag usato per inserimento di polilinee:
                          // 1 =  Unisce i due nodi con un solo tratto
                          // 2 =  Unisce i due nodi con più tratti
   int            rc, ret = GS_GOOD, flag = 1, type = 0;
   int            inserted_initial_node = FALSE, inserted_final_node = FALSE, inserted_pline = FALSE;
   ads_point      point,drag;
   ads_name       last, ent, nodo1, middle, nodo2;
   C_SELSET       entSS;
   C_INT_INT      *punt;
   C_CLASS        *pCls, *cls_initial_nodo, *cls_final_nodo;
   long           gs_id_initial_node = 0, gs_id_final_node = 0, gs_id_pline = 0;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (!pSub || (con_list = pSub->ptr_connect_list()) == NULL && con_list->get_count() > 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   ads_name_clear(nodo1);
   ads_name_clear(nodo2);
   SEL_EXT = this;

   if (pSSOut) pSSOut->clear();

   // Start Undo
   if (Undo)
      if (gsc_startTransaction() == GS_BAD) return GS_BAD;

   // INSERIMENTO NODO INIZIALE
   if ((type = pSub->get_type()) == TYPE_POLYLINE)
   {
      wcscpy(str, gsc_msg(100));     // "Esistente"                                        
      acedInitGet(0, gsc_msg(101));  // "Nuova Esistente"                                       
      if ((rc = acedGetKword(gsc_msg(102), str)) == RTERROR) // "\nEntita' iniziale [Nuova/<Esistente>]: "
         { GS_ERR_COD = eGSAdsCommandErr; if (Undo) gsc_abortTransaction(); return GS_CAN; }
      if (rc == RTCAN) { gsc_abortTransaction(); return GS_CAN; }

      if (gsc_strcmp(str, gsc_msg(103)) == 0) // "Nuova" 
      {
         gsc_ddselect_subnode(con_list, _T("StartNode"));
         if ((cls_initial_nodo = SEL_CLS) == NULL)
            { if (Undo) gsc_abortTransaction(); return GS_CAN; }

         // inserimento senza controllo di connessione ma solo di sovrapposizione
         ret = cls_initial_nodo->InsertEnt(NULL, NO_OVERLAP, NULL, &entSS, 
                                           &gs_id_initial_node);
         if (ret == GS_CAN || ret == GS_BAD)
            { if (Undo) gsc_abortTransaction(); return ret; }
         entSS.entname(0, nodo1);
         if (pSSOut) pSSOut->add(nodo1); // aggiungo l'entità inserita
         GS_LAST_NODE_CLS = cls_initial_nodo->ptr_id()->code;
         GS_LAST_NODE_SUB = cls_initial_nodo->ptr_id()->sub_code;
         inserted_initial_node = TRUE;
      }
      else  // NODO ESISTENTE
      {
         int    Flag = 1;
         LinkID ID = LINKID_NULL;

         ret = GS_BAD;
         do
         {
            if (Flag == 0) acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
            Flag = 0;
            acutPrintf(gsc_msg(29));    // "\nSeleziona entita' a cui connettersi :"
            acedInitGet(RSG_NONULL, GS_EMPTYSTR);
            while ((ret = acedEntSel(GS_EMPTYSTR, nodo1, point)) == RTERROR);
            if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; break; }
            if (ret == RTCAN) { ret = GS_CAN; break; }

            // verifico che l'oggetto grafico sia già etichettato "GEOsim"
            if (ent_EED.load(nodo1) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; continue; }
            // Verifico che si tratti di un elemento della simulazione corrente
            if (ent_EED.cls != id.code) continue;
            // Deve essere nella connect_list
            if ((punt=(C_INT_INT*)con_list->search_key(ent_EED.sub))==NULL) continue;
            // Cerco caratteristiche classe
            if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL)
               continue;
            // Deve avere la connect_list non vuota
            if ((con_tmp = pCls->ptr_connect_list()) == NULL || con_tmp->is_empty()) continue;

            // leggo valore chiave e il gruppo di selezione
            if (pCls->get_Key_SelSet(nodo1, &gs_id_initial_node, entSS) == GS_BAD)
               continue;

            if (gsc_is_DABlock(nodo1) == GS_GOOD) // Selezione su scheda degli attributi
            {
               // Ricavo elemento grafico del nodo di partenza
               if (entSS.intersectType(GRAPHICAL) == GS_BAD) continue;
               // get entity name
               if (entSS.entname(0, nodo1) != GS_GOOD) continue;
            }

            // se si è abilitati a inserire in GEOsimAppl::SAVE_SS
            if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD)
               // se l'entità è nuova e non è ancora stata inserita in GEOsimAppl::SAVE_SS 
               // NON può essere selezionata a meno che non abbia regole di connessione
               // con altri oggetti.
               // Questo controlla la seguente casistica:
               // Con "AddEntityToSaveSet" = OFF si inserisce un lato di una simulazione 
               // acqua, successivamente con "AddEntityToSaveSet" = ON si inserisce un 
               // altro lato utilizzando un nodo del lato precedente e si salva.
               if (pCls->is_NewEntity(gs_id_initial_node) == GS_GOOD) // entità nuova
               {
                  if (GEOsimAppl::SAVE_SS.is_member(nodo1) == GS_BAD)
                     if (is_IndipendentSub(pCls->ptr_id()->sub_code) == GS_BAD)
                        // ha regole di connessione con altri oggetti
                        continue;
               }

            ret = GS_GOOD;
            break;
         }
         while (1);

         if (ret != GS_GOOD)
            { if (Undo) gsc_abortTransaction(); return ret; }
         
         cls_initial_nodo = pCls;
      }
   }

   // INSERIMENTO ENTITA' SELEZIONATA
   if (acdbEntLast(last) != RTNORM) ads_name_clear(last);
 
   do
   {
      // se è polilinea non inserisce la scheda
      // inserimento con controllo di connessione
      ret = pSub->InsertEnt(NULL, TRUE, nodo1, &entSS);
      entSS.entname(0, ent);

      if (ret == GS_CAN)
      {
         // per le polilinee la funzione restituisce GS_CAN se non si 
         // è scelto di inserire la polilinea ma si vuole unire due nodi
         // con un unico tratto
         if (type != TYPE_POLYLINE) break;
      }
      else if (ret == GS_BAD)
         break;

      // INSERIMENTO NODO FINALE
      if (type == TYPE_POLYLINE)
      {
         C_RB_LIST       ColValues;
         C_CLS_PUNT_LIST ent_list;
         C_CLS_PUNT      *p_ent;
         long            gs_id;

         ads_name_set(ent, middle);

         // "direct" serve da flag, in questo caso indica che la polilinea non e'
         // stata ancora creata perche deve unire i due nodi estremi con un solo tratto
         if (ads_name_equal(last, middle))
         {
            direct = 1;
            if (gsc_get_lastPoint(nodo1, drag) == GS_BAD) break;
         }
         else   
         {
            direct = 2;
            if (gsc_get_lastPoint(middle, drag) == GS_BAD) break; 
         }

         wcscpy(str, gsc_msg(100));     // "Esistente"                                        
         acedInitGet(0, gsc_msg(101));  // "Nuova Esistente"
         if ((rc = acedGetKword(gsc_msg(108), str)) == RTERROR)  // "\nEntità finale [Nuova/<Esistente>]: "
            { GS_ERR_COD = eGSAdsCommandErr; ret = GS_CAN; break; }
         if (rc == RTCAN) { ret = GS_CAN; break; }

         if (gsc_strcmp(str, gsc_msg(103)) == 0) // "Nuova" 
         {
            gsc_ddselect_subnode(con_list, _T("EndNode"));
            if ((cls_final_nodo = SEL_CLS) == NULL) { ret = GS_CAN; break; }

            acedInitGet(RSG_NONULL, GS_EMPTYSTR);
            do
            {  // "\nPunto di inserimento: "
               while ((ret = acedGetPoint(drag, gsc_msg(20), point)) == RTNONE)
                  acutPrintf(gsc_msg(107)); // "\nPunto non valido."
               if (ret == RTERROR) { ret = GS_BAD; GS_ERR_COD = eGSAdsCommandErr; break; }
               if (ret == RTCAN) { ret = GS_CAN; break; }
               ret = GS_GOOD;
               // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
               if (gsc_OverlapValidation(point, cls_final_nodo) == GS_GOOD) break;
               acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
            }
            while (1);
            if (ret != GS_GOOD) break;

            if (direct == 1)  // Unisce i due nodi con un solo tratto
            {
               if (gsc_join_point(drag, point, middle, pSub)==GS_BAD) break;
            }
            else if (direct==2)
            {  // aggiunge un altro vertice alla fine
               if (gsc_addvertex(middle, point)==GS_BAD) break;
            }
            direct = 0;

            if (pSSOut) pSSOut->add(middle);

            // inserimento senza controllo di connessione
            if ((ret = cls_final_nodo->InsertEnt(point, FALSE, NULL, &entSS, &gs_id_final_node)) != GS_GOOD)
               break;
            entSS.entname(0, nodo2);
            if (pSSOut) pSSOut->add(nodo2);
            ads_name_set(nodo2, ent);
            inserted_final_node = TRUE;
         }
         else  // NODO ESISTENTE
         {
            int        Flag=1;
            LinkID     ID = LINKID_NULL;

            ret = GS_BAD;

            do
            {
               if (Flag == 0) acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
               Flag = 0;
               acutPrintf(gsc_msg(29));    // "\nSeleziona entita' a cui connettersi :"
               acedInitGet(RSG_NONULL, GS_EMPTYSTR);
               while ((ret = acedEntSel(GS_EMPTYSTR, nodo2, point)) == RTERROR);
               if (ret == RTREJ) { ret = GS_BAD; GS_ERR_COD = eGSAdsCommandErr; break; }
               if (ret == RTCAN) { ret = GS_CAN; break; }

               ret = GS_BAD;
               // verifico che l'oggetto grafico sia già etichettato "GEOsim"
               if (ent_EED.load(nodo2) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; continue; }
               // Verifico che si tratti di un elemento della simulazione corrente
               if (ent_EED.cls != id.code) continue;
               // Deve essere nella connect_list
               if ((punt = (C_INT_INT *) con_list->search_key(ent_EED.sub))==NULL) continue;
               // Cerco caratteristiche classe
               if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls,ent_EED.sub))==NULL)
                  continue;
               // Deve avere la connect_list non vuota
               if ((con_tmp = pCls->ptr_connect_list()) == NULL || con_tmp->is_empty()) continue;

               // leggo valore chiave e il gruppo di selezione
               if (pCls->get_Key_SelSet(nodo2, &gs_id_final_node, entSS) == GS_BAD)
                  continue;

               if (gsc_is_DABlock(nodo2) == GS_GOOD) // Selezione su scheda degli attributi
               {
                  // Ricavo elemento grafico del nodo di arrivo
                  if (entSS.intersectType(GRAPHICAL) == GS_BAD) continue;
                  // get entity name
                  if (entSS.entname(0, nodo2) != GS_GOOD) continue;
               }

               // se si è abilitati a inserire in GEOsimAppl::SAVE_SS
               if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD)
                  // se l'entità è nuova e non è ancora stata inserita in GEOsimAppl::SAVE_SS 
                  // NON può essere selezionata a meno che non abbia regole di connessione
                  // con altri oggetti.
                  // Questo controlla la seguente casistica:
                  // Con "AddEntityToSaveSet" = OFF si inserisce un lato di una simulazione 
                  // acqua, successivamente con "AddEntityToSaveSet" = ON si inserisce un 
                  // altro lato utilizzando un nodo del lato precedente e si salva.
                  if (pCls->is_NewEntity(gs_id_final_node) == GS_GOOD) // entità nuova
                  {
                     if (GEOsimAppl::SAVE_SS.is_member(nodo2) == GS_BAD)
                        if (is_IndipendentSub(pCls->ptr_id()->sub_code) == GS_BAD)
                           // ha regole di connessione con altri oggetti
                            continue;
                  }

               ret = GS_GOOD;
               break;
            }
            while (1);

            if (ret != GS_GOOD) break;
            
            cls_final_nodo = pCls;
         }
         ret = GS_BAD;

         if (gsc_get_firstPoint(nodo2, point) == GS_BAD) break; 

         if (direct==1)  // Unisce i due nodi con un solo tratto
         {
            if (gsc_join_point(drag, point, middle, pSub) == GS_BAD) break;
            if (pSSOut) pSSOut->add(middle);
         }
         else if (direct==2)
         { // aggiunge un altro vertice alla fine
            if (gsc_addvertex(middle, point) == GS_BAD) break;
            if (pSSOut) pSSOut->add(middle);
         }

         // Inserimento di entità GEOsim usando i valori attributi di default
         if (pSub->get_default_values(ColValues, middle) == GS_BAD) break;

         // aggiorno, se è il caso, il codice del nodo iniziale e finale
         if (gsc_upd_initfinal_node(pSub->ptr_attrib_list(), ColValues,
                                    gs_id_initial_node, gs_id_final_node) == GS_BAD)
            break;

         // lato
         if ((p_ent = new C_CLS_PUNT(this, middle)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
         ent_list.add_tail(p_ent);
         // nodo iniziale
         if ((p_ent = new C_CLS_PUNT(cls_initial_nodo, nodo1, gs_id_initial_node)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
         ent_list.add_tail(p_ent);
         // nodo finale
         if ((p_ent = new C_CLS_PUNT(cls_final_nodo, nodo2, gs_id_final_node)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
         ent_list.add_tail(p_ent);

         if ((ret = pSub->ins_data(&ent_list, ColValues, &gs_id)) != GS_GOOD)
            break;
      
         // la funzione C_SUB::ins_data varia la prima entità di ent_list
         p_ent = (C_CLS_PUNT *) ent_list.get_head();
         if (pSSOut) pSSOut->add(p_ent->ent);
         if (pSSOut) pSSOut->subtract_ent(middle);

         inserted_pline = TRUE;

         if (gsc_strcmp(str, gsc_msg(103)) == 0) // "Nuova" 
         {
            GS_LAST_NODE_CLS = cls_final_nodo->ptr_id()->code;
            GS_LAST_NODE_SUB = cls_final_nodo->ptr_id()->sub_code;
         }

         ret = GS_GOOD;
      }
      else
         if (pSSOut) pSSOut->add(ent);
   }
   while (0);

   // in caso di errore o di annullamento comando, se ho inserito il nodo iniziale
   // della polilinea e/o la polilinea le inserisco in GS_DELETE
   if (ret != GS_GOOD)
   {
      if (Undo)
      {
         // Riallineo la topologia riabilitando temporaneamente i reattori
         gsc_enable_reactors();
         gsc_abortTransaction();
         gsc_disable_reactors();
      }
   }
   else
   {
      GEOsimAppl::LAST_CLS = id.code;
      GEOsimAppl::LAST_SUB = pSub->ptr_id()->sub_code;
      if (Undo) gsc_endTransaction(); // End Undo         
   }

   return ret;
}


/*********************************************************/
/*.doc C_SUB:InsertEnt                        <external> */
/*+                                                                       
   Funzioni per inserimento di un'entita di GEOsim.
   L'utente viene guidato per l'inserimento della sola parte grafica 
   a cui viene successivamente associata la scheda di default.
   Parametri:
   ads_point start;     Se <> NULL viene usato come punto iniziale di riferimento
                        (es. per oggetti lineari)
   int check_con;       Flag di controllo, se = FALSE non effettua alcun
                        controllo sulle connessioni.
   ads_name con_ent;    Usato se check_con = TRUE. La funzione cerca
                        (se possibile) la connessione con questa entità
   C_SELSET *pSSOut;    Se <> NULL gruppo di selezione con l'oggetto grafico
                        principale dell'entità inserita.
                        Caso particolare: se si deve inserire una polilinea e
                        l'utente interrompe l'inserimento (ad esempio perchè
                        vuole collegare direttamente 2 nodi) il gruppo contiene
                        l'ultimo oggetto inserito (entlast)
   long *gs_id;         Codice nuova entità (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_SUB::InsertEnt(ads_point start, int check_con, ads_name con_ent,
                     C_SELSET *pSSOut, long *gs_id)
{
   C_ATTRIB_LIST   *p_attrib_list = ptr_attrib_list();
   C_CONNECT_LIST  *con_list = ptr_connect_list();
   C_FAS           OldGraphEnv;
   int             ret;
   ads_point       ins;
   ads_pointp      point = NULL;
   ads_name        ent, last;
   C_EED           tmp_EED;
   long            gs_id_initial_node = 0, gs_id_final_node = 0;
   C_RB_LIST       ColValues;
   C_CLS_PUNT_LIST EntClsList;
   C_CLS_PUNT      *pEntCls;
   C_STRING        TextValue;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (pSSOut) pSSOut->clear();

   // Controlla l'esistenza di eventuali connessioni
   if (check_con == TRUE && con_list && con_list->is_to_be_connected() == GS_GOOD)
   {
      if ((ret = gsc_get_connect_point(this, start, con_ent, &point)) == GS_CAN)
         return GS_CAN;
      if (ret == GS_BAD) return GS_BAD;

      // se si è abilitati a inserire in GEOsimAppl::SAVE_SS
      if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD)
      {
         C_CLASS *pCls;
         long    gs_id;

         if (tmp_EED.load(con_ent) == GS_BAD)
            { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
         // Cerco caratteristiche sottoclasse
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(tmp_EED.cls, tmp_EED.sub)) == NULL)
            return GS_BAD;
         if (pCls->getKeyValue(con_ent, &gs_id) == GS_BAD) return GS_BAD;

         // se l'entità è nuova e non è ancora stata inserita in GEOsimAppl::SAVE_SS 
         // NON può essere selezionata a meno che non abbia regole di connessione
         // con altri oggetti.
         // Questo controlla la seguente casistica:
         // Con "AddEntityToSaveSet" = OFF si inserisce un lato di una simulazione 
         // acqua, successivamente con "AddEntityToSaveSet" = ON si inserisce una 
         // valvola utilizzando il lato precedente e si salva.
         if (pCls->is_NewEntity(gs_id) == GS_GOOD) // entità nuova
         {
            if (GEOsimAppl::SAVE_SS.is_member(con_ent) == GS_BAD)
            {  // Cerco caratteristiche classe madre
               if ((pCls = GS_CURRENT_WRK_SESSION->find_class(tmp_EED.cls)) == NULL)
                  return GS_BAD;

               if (((C_EXTERN *)pCls)->is_IndipendentSub(pCls->ptr_id()->sub_code) == GS_BAD)
                  // ha regole di connessione con altri oggetti
                  { GS_ERR_COD = eGSReferenceToTemporaryEnts; return GS_BAD; }
            }
         }
      }

      // Elimina allocazioni dinamiche
      if (point != NULL) { ads_point_set(point, ins); free(point); point = ins; }
   }
   else
   {
      point = start;

      if (id.type == TYPE_NODE || id.type == TYPE_TEXT)
      {
         if (point) 
         {
            if (check_con) // se e si devono controllare le connessioni
               // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
               if (gsc_OverlapValidation(start, this) == GS_BAD)
                  { GS_ERR_COD = eGSOverlapValidation; return GS_BAD; }
         }
         else
         {
            int result;
            
            do
            {  // "\nPunto di inserimento: "
               while ((result = acedGetPoint(NULL, gsc_msg(20), ins)) == RTNONE)
                  acutPrintf(gsc_msg(107)); // "\nPunto non valido."
               if (result == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
               if (result == RTCAN) return GS_CAN;
               point = ins;
               if (check_con) // se e si devono controllare le connessioni
               {
                  // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
                  if (gsc_OverlapValidation(ins, this) == GS_GOOD) break;
                  acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
               }
               else break;
            }
            while (1);
         }
      }
   }

   // Effettua settaggi FAS di default
   if (gsc_setenv_graph(id.category, id.type, fas, &OldGraphEnv) == GS_BAD)
      return GS_BAD;

   // A seconda del tipo inserisce l'entita' grafica corrispondente
   if (acdbEntLast(last) != RTNORM) ads_name_clear(last);

   switch (id.type)
   {
      case TYPE_SURFACE :
      case TYPE_POLYLINE :
         if ((ret = gsc_insert_pline(point, (check_con) ? this : NULL)) != GS_GOOD)
         {
            if (pSSOut && id.type == TYPE_POLYLINE && ret == GS_CAN)
            {
               acdbEntLast(ent);
               pSSOut->add(ent);
            }
            break;
         }
         acdbEntLast(ent);
         // Setto la scala del tipo linea
         if (gsc_set_lineTypeScale(ent, fas.line_scale) == GS_BAD) { ret = GS_BAD; break; }
         if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
         EntClsList.add_tail(pEntCls);

         // Se SURFACE chiude polilinea
         if (id.type == TYPE_SURFACE)
         {
            if (gsc_close_pline(ent) == GS_BAD) { ret = GS_BAD; break; }
            // se impostato un riempimento lo applica
            if (fas.hatch && wcslen(fas.hatch) > 0)
            {
               // se ritorna GS_CAN la superficie era troppo piccola per
               // contenere il riempimento
               if ((ret = gsc_setHatchEnt(ent, fas.hatch, fas.hatch_scale,
                                          fas.hatch_rotation, &fas.hatch_color,
                                          fas.hatch_layer)) == GS_BAD)
                  break;
         
               if (ret == GS_GOOD)
               {
                  acdbEntLast(ent); // aggiungo anche il riempimento
                  if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
                  EntClsList.add_tail(pEntCls);
               }
            }
         }
         else
            // se TYPE_POLYLINE riapre eventuali polilinee chiuse
            if (id.type == TYPE_POLYLINE) gsc_open_pline(ent);        

         break;
                                 
      case TYPE_TEXT :
      {
         C_ATTRIB *pTextAttr;

         // cerco il valore di default
         pTextAttr = ptr_attrib_list()->getFirstVisibleAttrib();
         if ((ret = gsc_insert_text(fas.style, point, fas.h_text, fas.rotation,
                                    NULL,
                                    (pTextAttr->def && pTextAttr->def->restype == RTSTR) ? pTextAttr->def->resval.rstring : _T("?"),
                                    (check_con) ? this : NULL)) != GS_GOOD)
            break;
         acdbEntLast(ent);
         gsc_getInfoText(ent, &TextValue);
         if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
         EntClsList.add_tail(pEntCls);

         break;
      }

      case TYPE_NODE :
         if ((ret = gsc_insert_block(fas.block, point, fas.block_scale, fas.rotation,
                                    (check_con) ? this : NULL)) != GS_GOOD)
            break;
         acdbEntLast(ent);
         if ((pEntCls = new C_CLS_PUNT(this, ent)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; ret = GS_BAD; break; }
         EntClsList.add_tail(pEntCls);
         break;

      default :
         acutPrintf(gsc_msg(217)); // "\nNON DISPONIBILE IN QUESTA VERSIONE..."
         ret = GS_BAD;
         break;
   }

   // Ripristina situazione FAS precedente
   gsc_setenv_graph(id.category, id.type, OldGraphEnv);

   if (ret == GS_BAD || ret == GS_CAN) return ret;

   // se inserimento su polilinea
   if (gsc_isLinearEntity(con_ent) == GS_GOOD)
   {
      int       connect_mode;
      C_INT_INT *punt;

      tmp_EED.load(con_ent);
      if ((punt = (C_INT_INT*) con_list->search_key(tmp_EED.sub)) == NULL)
         return GS_BAD;
      connect_mode = punt->get_type();
      // connessione su punto intermedio, su vertici, su un punto qualsiasi della polilinea
      if (connect_mode & CONCT_MIDDLE || connect_mode & CONCT_VERTEX ||
         connect_mode & CONCT_ANY_POINT)
      {
         ads_point pnt1, pnt2;

         // Non posso spezzare una polilinea nei suoi estremi
         if (gsc_get_firstPoint(con_ent, pnt1) == GS_BAD ||
            gsc_get_lastPoint(con_ent, pnt2) == GS_BAD)
            return GS_BAD;
         
         if (gsc_point_equal(point, pnt1) || gsc_point_equal(point, pnt2))
         {
         }
         else
         {  // inizializzo  gs_id_initial_node e gs_id_final_node 
            gs_id_initial_node = tmp_EED.initial_node;
            gs_id_final_node   = tmp_EED.final_node;
         }
      }
   }

   if (id.type != TYPE_POLYLINE && id.type != TYPE_SURFACE)
   {
      // è stato inserito un oggetto puntuale su una poly spezzandola in 2
      // verifico che si possa modificare la polilinea
      if (gs_id_initial_node && gs_id_final_node)
         // se NON si è abilitati a inserire in GEOsimAppl::SAVE_SS
         if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD)
         {
            C_CLASS *pCls;
            long    gs_id;

            if (tmp_EED.load(con_ent) == GS_BAD)
               { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
            // Cerco caratteristiche sottoclasse
            if ((pCls = GS_CURRENT_WRK_SESSION->find_class(tmp_EED.cls, tmp_EED.sub)) == NULL)
               return GS_BAD;
            if (pCls->getKeyValue(con_ent, &gs_id) == GS_BAD) return GS_BAD;

            // se l'entità a cui ci si collega NON è nuova oppure è già stata 
            // inserita in GEOsimAppl::SAVE_SS allora non si può modificare
            // Questo controlla la seguente casistica:
            // Con "AddEntityToSaveSet" = ON si inserisce un lato di una simulazione 
            // acqua, successivamente con "AddEntityToSaveSet" = OFF si inserisce una 
            // valvola utilizzando il lato precedente e si salva.
            if (pCls->is_NewEntity(gs_id) == GS_BAD || // entità esistente
                  GEOsimAppl::SAVE_SS.is_member(con_ent) == GS_GOOD)
            {  // Cerco caratteristiche classe madre
               if ((pCls = GS_CURRENT_WRK_SESSION->find_class(tmp_EED.cls)) == NULL)
                  return GS_BAD;

               if (((C_EXTERN *)pCls)->is_IndipendentSub(pCls->ptr_id()->sub_code) == GS_BAD)
                  // ha regole di connessione con altri oggetti
                  { GS_ERR_COD = eGSReferenceToTemporaryEnts; return GS_BAD; }
            }
         }

      // Inserimento di entità GEOsim usando i valori attributi di default
      if (get_default_values(ColValues) == GS_BAD) 
         return GS_BAD;

      // Se si tratta di testo devo modificare il valore del testo
      if (id.type == TYPE_TEXT)
         if (ColValues.CdrAssocSubst(ptr_attrib_list()->getFirstVisibleAttrib()->get_name(),
                                     TextValue.get_name()) == GS_BAD)
            return GS_BAD;

      if ((ret = ins_data(&EntClsList, ColValues, gs_id)) != GS_GOOD)
         return GS_BAD;
      
      // la funzione C_SUB::ins_data varia la prima entità di EntClsList
      pEntCls = (C_CLS_PUNT *) EntClsList.get_head();
      ads_name_set(pEntCls->ent, ent);
      if (pSSOut) pSSOut->add(ent);

      // è stato inserito un oggetto nodale su una poly spezzandola in 2
      if (gs_id_initial_node && gs_id_final_node)           
         // la spezzo con il break
         if (gsc_break(con_ent, ent) == GS_BAD)
            return GS_BAD;
   }
   else // se è una polilinea la scheda verrà inserita in seguito
      if (pSSOut) pSSOut->add(ent);

   return ret;   
}


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DI C_EXTERN
// INIZIO FUNZIONI DI C_SPAGHETTI
//////////////////////////////////////////////////////////////////////////


presbuf C_SPAGHETTI::get_graphical_data(long code, TCHAR *what, TCHAR *type)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/*********************************************************/
/*.doc C_SPAGHETTI::InsertEnt                  external> */
/*+
   Funzioni per inserimento di entita'  della sottoclasse GEOsim 
   con interfaccia grafica.
   Parametri:
   ads_point start;     Se <> NULL viene usato come punto iniziale di riferimento
                        (es. per oggetti lineari)
   int check_con;       Flag di controllo, se = FALSE non effettua alcun
                        controllo sulle connessioni.
   ads_name con_ent;    Usato se check_con = TRUE. La funzione cerca
                        (se possibile) la connessione con questa entità
   C_SELSET *pSSOut;    Se <> NULL gruppo di selezione con gli oggetti grafici
                        delle entità inserite
   long *gs_id;         Codice nuova entità (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_SPAGHETTI::InsertEnt(ads_point start, int check_con, ads_name con_ent,
                           C_SELSET *pSSOut, long *gs_id)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
//{
//   C_FAS           OldGraphEnv;
//   C_COLOR         OldColor;
//   int             ret, OldDimAssoc, OldCmdEcho;
//   resbuf          rb;
//   static TCHAR    prev_com[135] = GS_EMPTYSTR;
//   TCHAR           com[135] = GS_EMPTYSTR, OldLayer[MAX_LEN_LAYERNAME];
//   ads_name        last;
//   C_CLS_PUNT      *pEntCls;
//   C_CLS_PUNT_LIST EntClsList;
//   C_STRING        ActualCmd, CmdToCheck;
//   C_RB_LIST       ColValues; // usato solo per compatibilità
//   bool            Hatching;
//
//   // Comandi di inserimento resi disponibili per l'inserimento di entità
//   // spaghetti con ACAD 15 (rivedere questa lista per le versioni successive di ACAD)
//   // Qualora si dovessero inserire dei comandi non nativi mettere il comando in tutte
//   // le lingue da supportare (es. RECTANGLE RECTANGLE)
//   TCHAR           enabled_commands[] = _T(" 3DMESH 3DPOLY ARC BHATCH -BHATCH BOUNDARY BOX \
//CIRCLE CONE CYLINDER DIMALIGNED DIMANGULAR DIMBASELINE \
//DIMCENTER DIMCONTINUE DIMDIAMETER DIMLINEAR DIMORDINATE DIMRADIUS DONUT DTEXT \
//EDGESURF ELLIPSE HATCH -HATCH \
//INSERT -INSERT \
//LEADER LINE \
//MLINE MPOLYGON MTEXT \
//PLINE POLYGON QLEADER RAY REGION \
//RECTANG RECTANGLE RETTANGOLO \
//REVSURF RULESURF \
//SETVAR SHAPE SOLID SPHERE SPLINE TEXT TOLERANCE TORUS TRACE XLINE \
//RECTANGLE RECTANGLE "); 
//
//                                     // inglese italiano  
//TCHAR        disabled_commands[] = _T(" ADEDRAWINGS ADEDRAWINGS \
//                                        CLOSE CHIUDI \
//                                        MAPOPTIONS MAPOPTIONS \
//                                        NEW NUOVO \
//                                        NEWSHEETSET NEWSHEETSET \
//                                        OPEN APRI \
//                                        OPTIONS OPZIONI \
//                                        QSAVE QSAVE \
//                                        QUIT ESCI \
//                                        SAVE SALVA "); 
//
//   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
//   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;
//
//   // verifico l'abilitazione dell' utente;
//   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;
//   if (id.abilit != GSUpdateableData)
//   {
//      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
//      return GS_BAD;
//   }
//
//   if ((pEntCls = new C_CLS_PUNT(this)) == NULL) // preparo la lista per "ins_data"
//      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
//   EntClsList.add_tail(pEntCls);
//
//   // Effettua settaggi FAS di default
//   if (gsc_setenv_graph(id.category, id.type, *ptr_fas(), &OldGraphEnv) == GS_BAD) return GS_BAD;
//
//   // se la geometria è in tabella DB
//   if (ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
//   {  // La quotatura deve essere inserita esplosa
//      struct resbuf rb;
//
//      rb.restype = RTSHORT;
//      acedGetVar(_T("DIMASSOC"), &rb); // memorizzo il tipo di quotatura precedente
//      gsc_rb2Int(&rb, &OldDimAssoc);
//      // Setto la modalità di quotatura esplosa
//      rb.resval.rint = 0; // se = 0 -> Esplosa
//      acedSetVar(_T("DIMASSOC"), &rb);
//   }
//
//   gsc_set_echo(1, &OldCmdEcho); // attivo echo comandi
//
//   if (pSSOut) pSSOut->clear();
//
//   acutPrintf(gsc_msg(104)); // "\nComando: " 
//   while (1)
//   {
//      ret = GS_GOOD;
//      while (1)
//      {  // "(<ESC> termina inserimento spaghetti): "
//         if (ads_getstring(0, gsc_msg(218), com) != RTNORM) continue;
//
//         gsc_alltrim(com);
//         gsc_toupper(com);
//
//         if (com[0] == _T('\0'))
//         {  // se c'era un comando precedente
//            if (prev_com[0] != _T('\0')) wcscpy(com, prev_com); // uso il comando precedente
//         }
//         else // setto il comando che diventerà quello precedente la prossima volta
//         {           
//            // se è stato digitato solo il "." si termina l'inserimento
//            if (gsc_strcmp(com, _T(".")) == 0) { ret = GS_CAN; break; }
//
//            // controllo i comandi vietati
//            CmdToCheck = com;
//            if (CmdToCheck.get_chr(0) == _T('.') || CmdToCheck.get_chr(0) == _T('_'))
//            {
//               CmdToCheck.set_chr(_T(' '), 0);
//               CmdToCheck.alltrim();
//               if (CmdToCheck.get_chr(0) == _T('.') || CmdToCheck.get_chr(0) == _T('_'))
//               {
//                  CmdToCheck.set_chr(_T(' '), 0);
//                  CmdToCheck.alltrim();
//               }
//            }
//            if (gsc_strstr(disabled_commands, CmdToCheck.get_name()) != 0)
//            {
//               acutPrintf(gsc_msg(452)); // "\nComando non disponibile."
//               GS_ERR_COD = eGSOpNotAble;
//               com[0] = _T('\0');
//               continue;
//            }
//
//
//            if (com[0] != _T('.') && (wcslen(com) > 1 && com[1] != _T('.')))
//            {  // devo lanciare un comando nativo (che inizia con .)
//               ActualCmd = _T(".");
//               ActualCmd += com;
//               wcscpy(com, ActualCmd.get_name()); // shift di 1 a destra
//            }
//         }
//
//         if (com[0] == _T('\0')) acutPrintf(gsc_msg(104));  // "\nComando: "      
//         else break;
//      }
//
//      if (ret != GS_GOOD)
//      {  // l'utente ha interrotto l'inserimento di spaghetti
//         if (ret == GS_CAN) ret = GS_GOOD; 
//         break;
//      }
//
//      if (acdbEntLast(last) != RTNORM) ads_name_clear(last); // primo oggetto 
//
//      acutPrintf(GS_LFSTR);
//      ret = GS_BAD;
//      do
//      {
//         if (acedCommand(RTSTR, com, 0) != RTNORM)
//            { GS_ERR_COD = eGSAdsCommandErr; break; }
//         if (acedGetVar(_T("CMDNAMES"), &rb) != RTNORM ||
//             rb.restype != RTSTR || rb.resval.rstring == NULL)
//            { GS_ERR_COD = eGSVarNotDef; break; }
//
//         // Se si tratta di comandi che inseriscono riempimenti
//         if (gsc_strcmp(rb.resval.rstring, _T("BHATCH")) == 0 ||
//             gsc_strcmp(rb.resval.rstring, _T("-BHATCH")) == 0 ||
//             gsc_strcmp(rb.resval.rstring, _T("HATCH")) == 0 ||
//             gsc_strcmp(rb.resval.rstring, _T("-HATCH")) == 0)
//             Hatching = true;
//         else
//             Hatching = false;
//
//         ActualCmd = _T(" ");
//         ActualCmd += rb.resval.rstring;
//         ActualCmd += _T(" ");
//
//         // sono validi solamente alcuni comandi di inserimento
//         if (gsc_strstr(enabled_commands, ActualCmd.get_name()) == 0)
//         {
//            acutPrintf(gsc_msg(452)); // "\nComando non disponibile."
//            acedCommand(RTNONE);
//            GS_ERR_COD = eGSOpNotAble;
//            break;
//         }
//
//         if (Hatching) // Se si sta inserendo un riempimento
//         {  // setto caratteristiche di colore e layer proprie del riempimento
//            if (gsc_set_CurrentLayer(ptr_fas()->hatch_layer, OldLayer) == GS_BAD) break;
//            if (gsc_set_CurrentColor(ptr_fas()->hatch_color, &OldColor) == GS_BAD) break;
//         }
//
//         if ((ret = gsc_end_command(rb.resval.rstring)) == GS_BAD) break;
//
//         if (Hatching) // Se si stava inserendo un riempimento
//         {  // risetto le caratteristiche di colore e layer precedenti
//            if (gsc_set_CurrentLayer(OldLayer) == GS_BAD) break;
//            if (gsc_set_CurrentColor(OldColor) == GS_BAD) break;
//         }
//
//         C_SELSET NewSS;
//         long     i = 0;
//
//         // nel caso siano stati inseriti dei nuovi nuovi oggetti grafici
//         while (gsc_mainentnext(last, last) == GS_GOOD)
//            if (NewSS.add(last) == GS_BAD) break;
//
//         while (NewSS.entname(i++, pEntCls->ent) == GS_GOOD)
//         {
//            if ((ret = ins_data(&EntClsList, ColValues)) == GS_BAD) break;
//            if (pSSOut)
//               // la funzione ins_data potrebbe variare la prima entità di EntClsList
//               pSSOut->add(pEntCls->ent);
//         }
//
//         if (ret == GS_BAD) break;
//
//         wcscpy(prev_com , com);
//         ret = GS_GOOD;
//      }
//      while (0);
//
//      com[0] = _T('\0');
//   }
//
//   // Ripristina situazione FAS precedente
//   gsc_setenv_graph(id.category, id.type, OldGraphEnv);
//
//   // se la geometria è in tabella DB
//   if (ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
//   {  // La quotatura deve essere rispristinata come prima
//      struct resbuf rb;
//
//      rb.restype = RTSHORT;
//      rb.resval.rint = OldDimAssoc;
//      acedSetVar(_T("DIMASSOC"), &rb); // rispristino il tipo di quotatura precedente
//   }
//
//   gsc_set_echo(OldCmdEcho); // ripristino stato precedente echo comandi
//
//   GEOsimAppl::LAST_CLS = id.code;
//   GEOsimAppl::LAST_SUB = id.sub_code;
//
//   return ret;   
//}


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI DI C_SPAGHETTI
//////////////////////////////////////////////////////////////////////////


/*************************************************************************/
/*.doc gsc_getArcLen                                          (external) */  
/*+
  La funzione ricava la lunghezza di una arco.
  Parametri:
  ads_real radius;   raggio dell'arco
  ads_real angle;    angolo dell'arco (in radianti)
  oppure
  ads_real radius;      raggio dell'arco
  ads_real InitAngle;   angolo iniziale dell'arco (in radianti)
  ads_real FinalAngle;  angolo finale dell'arco (in radianti)

-*/
/*************************************************************************/
ads_real gsc_getArcLen(ads_real radius, ads_real angle)
   { return (2.0 * PI * radius) / (2.0 * PI) * fabs(angle); }
ads_real gsc_getArcLen(ads_real radius, ads_real InitAngle, ads_real FinalAngle)
{
   if (InitAngle > FinalAngle)
      return gsc_getArcLen(radius, (2.0 * PI) - (InitAngle - FinalAngle));
   else
      return gsc_getArcLen(radius, InitAngle - FinalAngle);
}


/*************************************************************************/
/*.doc gsc_getArcAngle                                          (external) */  
/*+
  La funzione ricava l'angolo di una arco (in radianti).
  Parametri:
  ads_real radius;      raggio dell'arco
  ads_real len;      lunghezza dell'arco
-*/
/*************************************************************************/
ads_real gsc_getArcAngle(ads_real radius, ads_real len)
   { return (2.0 * PI) / (2.0 * PI * radius) * len; }


/*************************************************************************/
/*.doc gsc_getArcRayCenter                                    (external) */  
/*+
  La funzione ricava il raggio e il centro di una arco.
  Parametri:
  ads_point p1;       punto iniziale dell'arco
  ads_point p2;       punto finale dell'arco
  ads_real  angle;    angolo dell'arco (in radianti)
  ads_real  ray;      raggio dell'arco (out)
  ads_point center;   centro dell'arco (out)
  ads_real *startAngle; Angolo iniziale (out)
  ads_real *endAngle;   Angolo finale (out)
-*/
/*************************************************************************/
void gsc_getArcRayCenter(AcGePoint3d &p1, AcGePoint3d &p2, ads_real angle, 
                         ads_real *radius, AcGePoint3d &center,
                         ads_real *startAngle, ads_real *endAngle)
{
   ads_point _p1, _p2, _center;

   ads_point_set_from_AcGePoint3d(p1, _p1);
   ads_point_set_from_AcGePoint3d(p2, _p2);
   gsc_getArcRayCenter(_p1, _p2, angle, radius, _center, startAngle, endAngle);
   center.set(_center[X], _center[Y], _center[Z]);
}
void gsc_getArcRayCenter(AcGePoint2d &p1, AcGePoint2d &p2, ads_real angle, 
                         ads_real *radius, AcGePoint2d &center,
                         ads_real *startAngle, ads_real *endAngle)
{
   ads_point _p1, _p2, _center;

   ads_point_set_from_AcGePoint2d(p1, _p1);
   ads_point_set_from_AcGePoint2d(p2, _p2);
   gsc_getArcRayCenter(_p1, _p2, angle, radius, _center, startAngle, endAngle);
   center.set(_center[X], _center[Y]);
}
void gsc_getArcRayCenter(ads_point p1, ads_point p2, ads_real angle, 
                         ads_real *radius, ads_point center,
                         ads_real *startAngle, ads_real *endAngle)
{
   ads_point MiddlePt;
   ads_real  MetaCorda, AngleCorda, IncludedAngle;
   
   if (fabs(angle) < PI) IncludedAngle = angle;
   else IncludedAngle = ((angle > 0) ? 1 : -1) * ((2 * PI) - fabs(angle));

   gsc_getMidLine(p1, p2, MiddlePt); // punto medio
   MetaCorda = gsc_dist(p1, MiddlePt); // metà corda
   *radius = fabs(MetaCorda / sin(IncludedAngle / 2.0));
   AngleCorda = acutAngle(p1, p2);

   if (IncludedAngle != angle) // se angolo > di 180
      acutPolar(MiddlePt, 
                (angle > 0) ? AngleCorda - PI / 2 : AngleCorda + PI / 2, // senso orario
                fabs(*radius * cos(angle / 2.0)),
                center); // + o - 90 gradi
   else
      acutPolar(MiddlePt, 
                (angle > 0) ? AngleCorda + PI / 2 : AngleCorda - PI / 2,
                fabs(*radius * cos(angle / 2.0)),
                center); // + o - 90 gradi

   if (startAngle) *startAngle = acutAngle(center, p1);
   if (endAngle) *endAngle = acutAngle(center, p2);
}


/*************************************************************************/
/*.doc gsc_getMidLine                                         (external) */  
/*+
  La funzione ricava il punto medio di una linea.
  Parametri:
  ads_point pt1;     primo punto della linea
  ads_point pt2;     secondo punto della linea
  ads_point pt;      risultato
-*/
/*************************************************************************/
void gsc_getMidLine(ads_point pt1, ads_point pt2, ads_point pt) 
{
   AcGePoint3d _pt1, _pt2, _pt;

   AcGePoint3d_set_from_ads_point(pt1, _pt1);
   AcGePoint3d_set_from_ads_point(pt2, _pt2);
   gsc_getMidLine(_pt1, _pt2, _pt);
   ads_point_set_from_AcGePoint3d(_pt, pt);
}
void gsc_getMidLine(AcGePoint3d &pt1, AcGePoint3d &pt2, AcGePoint3d &pt)
{
   pt.x = (pt1.x + pt2.x) / 2.0;
   pt.y = (pt1.y + pt2.y) / 2.0;
   pt.z = (pt1.z + pt2.z) / 2.0;
}
void gsc_getMid2DLine(ads_point pt1, ads_point pt2, ads_point pt) 
{
   pt[X] = (pt1[X] + pt2[X]) / 2.0;
   pt[Y] = (pt1[Y] + pt2[Y]) / 2.0;
   pt[Z] = 0.0;
}


/*************************************************************************/
/*.doc gsc_retMidArc (internal)*/  
/*+
  La funzione riceve come parametri il punto centrale di un arco e il suo
  vertice iniziale e finale.

  Restituisce il punto medio dell'arco.
-*/
/*************************************************************************/
void gsc_retMidArc(AcGePoint3d &center, AcGePoint3d &iniz, AcGePoint3d &fine, AcGePoint3d &dato)
{
   ads_point _center, _iniz, _fine, _dato;

   ads_point_set_from_AcGePoint3d(center, _center);
   ads_point_set_from_AcGePoint3d(iniz, _iniz);
   ads_point_set_from_AcGePoint3d(fine, _fine);
   gsc_retMidArc(_center, _iniz, _fine, _dato);
   AcGePoint3d_set_from_ads_point(_dato, dato);
}
void gsc_retMidArc(ads_point center, ads_point iniz, ads_point fine, ads_point dato)
{
  ads_real uno, due, medio;
  ads_point dummy;

  uno = acutAngle(center, iniz);
  due = acutAngle(center, fine);
  
  if (uno > due)
     due = due + (2.0 * PI);

  medio = (uno + due) / 2.0;
  uno   =  gsc_dist(iniz, center); // raggio
  acutPolar(center, medio, uno, dummy);
  ads_point_set(dummy, dato);
}


/*************************************************************************/
/*.doc gsc_getLenParts                                                   */
/*+
  La funzione riceve come parametro il nome di un oggetto e restituisce
  una lista delle lunghezze di tutti i tratti che lo compongo.
  Parametri:
  ads_name    Ent;         Oggetto grafico
  C_REAL_LIST &LenParts;   Lista delle lunghezze dei tratti

  La funzione puo' essere utilizzata con oggetti rispettivamente di tipo:
  POLYLINE,LWPOLYLINE,LINE,ARC
  
  La funzione in caso di errore restituisce GS_BAD altrimenti GS_GOOD.  
-*/
/*************************************************************************/
int gsc_getLenParts(ads_name Ent, C_REAL_LIST &LenParts)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   
   if (acdbGetObjectId(objId, Ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   int Res = gsc_getLenParts((AcDbEntity *) pObj, LenParts);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return Res;
}
int gsc_getLenParts(AcDbEntity *pEnt, C_REAL_LIST &LenParts)
{
   C_RB_LIST EntList;
   ads_real  Len;
   C_REAL    *pLenPart;
   int       result = GS_GOOD;

   LenParts.remove_all();

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      bool               Spline, Closed = (((AcDb2dPolyline *) pEnt)->isClosed() == 0) ? false : true;
      AcGePoint3d        p1, p2, center, PrevPt, StartPt;
      ads_real           radius, ArcAngle; // angolo inscritto

      if (((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dQuadSplinePoly ||
          ((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         p1 = pVertex->position();
         if (pVertex->bulge() != 0)
            // vertice curva di adattamento (se <> 0 segue un arco)
            ArcAngle = atan(pVertex->bulge()) * 4; // tang di 1/4 angolo incluso
         else
            ArcAngle = 0;

         pVertex->close();

         if (Closed) StartPt = p1;

         break;
      }
      pVertIter->step();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         p2 = pVertex->position();

         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, p2, ArcAngle, &radius, center);
            Len = gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len = gsc_dist(p1, p2);

         if ((pLenPart = new C_REAL) == NULL)
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);

         if (pVertex->bulge() != 0)
            // vertice curva di adattamento (se <> 0 segue un arco)
            ArcAngle = atan(pVertex->bulge()) * 4; // tang di 1/4 angolo incluso
         else
            ArcAngle = 0;

         p1 = p2;
      }

      if (Closed)
      {
         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, p2, ArcAngle, &radius, center);
            Len = gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len = gsc_dist(p1, p2);

         if ((pLenPart = new C_REAL) == NULL)
            { LenParts.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      ads_real    radius, ArcAngle; // angolo inscritto
      AcGePoint2d p1, p2, center, StartPt;
      int         NumVerts = ((AcDbPolyline *) pEnt)->numVerts();
      double      Bulge;   
      bool        Closed = (((AcDb2dPolyline *) pEnt)->isClosed() == 0) ? false : true;

      ((AcDbPolyline *) pEnt)->getPointAt(0, p1);
      ((AcDbPolyline *) pEnt)->getBulgeAt(0, Bulge);       
      ArcAngle = atan(Bulge) * 4; // vertice curva di adattamento (segue un arco)
      if (Closed) StartPt = p1; // primo punto
      
      for (int i = 1; i < NumVerts; i++) // punti successivi
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, p2);

         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, p2, ArcAngle, &radius, center);
            Len = gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len = gsc_dist(p1, p2);

         if ((pLenPart = new C_REAL) == NULL)
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);

         ((AcDbPolyline *) pEnt)->getBulgeAt(i, Bulge); // tang di 1/4 angolo incluso     
         ArcAngle = atan(Bulge) * 4; // vertice curva di adattamento (segue un arco)

         p1 = p2;
      }

      if (Closed)
      {
         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, StartPt, ArcAngle, &radius, center);
            Len = gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len = gsc_dist(p1, StartPt);

         if ((pLenPart = new C_REAL) == NULL)
            { LenParts.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d p1, p2, StartPt;
      bool        Closed = ((AcDbMline *) pEnt)->closedMline();
      int         NumVerts = ((AcDbMline *) pEnt)->numVertices();

      p1 = ((AcDbMline *) pEnt)->vertexAt(0);
      if (Closed) StartPt = p1; // primo punto

      for (int i = 1; i < NumVerts; i++) // punti successivi
      {
         p2 = ((AcDbMline *) pEnt)->vertexAt(i);
         Len = gsc_dist(p1, p2);

         if ((pLenPart = new C_REAL) == NULL)
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);

         p1 = p2;
      }

      if (Closed)
      {
         Len = gsc_dist(p1, StartPt);

         if ((pLenPart = new C_REAL) == NULL)
            { LenParts.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pLenPart->set_key(Len);
         LenParts.add_tail(pLenPart);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      AcGePoint3d p1, p2;

      p1 = ((AcDbLine *) pEnt)->startPoint(); // primo punto
      p2 = ((AcDbLine *) pEnt)->endPoint(); // primo punto
      Len = gsc_dist(p1, p2);

      if ((pLenPart = new C_REAL) == NULL)
         { LenParts.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pLenPart->set_key(Len);
      LenParts.add_tail(pLenPart);
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_real    radius, init_angle, final_angle;
      AcGePoint3d center;

      center = ((AcDbArc *) pEnt)->center();
      radius = ((AcDbArc *) pEnt)->radius();
      init_angle = ((AcDbArc *) pEnt)->startAngle();
      final_angle = ((AcDbArc *) pEnt)->endAngle();

      Len = gsc_getArcLen(radius, init_angle, final_angle);

      if ((pLenPart = new C_REAL) == NULL)
         { LenParts.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pLenPart->set_key(Len);
      LenParts.add_tail(pLenPart);
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (result == GS_BAD) LenParts.remove_all();

   return result;
}


/*************************************************************************/
/*.doc gsc_getPtOnLinearObj                                              */
/*+
  La funzione riceve come parametro il nome di un oggetto e un offset che
  rappresenta la distanza partendo dal vertice iniziale e lungo l'oggetto
  lineare.
  Viene restituito il punto e la rotazione.
  Parametri:
  ads_name ent;            Oggetto grafico
  ads_real offset;         OffSet
  ads_point pt;            Punto    (out)
  ads_real *Angle;         Angolo di rotazione in radianti  (out)
  
  La funzione puo' essere utilizzata con oggetti rispettivamente di tipo:
  POLYLINE,LWPOLYLINE,LINE,ARC
  
  La funzione in caso di errore restituisce GS_BAD altrimenti GS_GOOD.  
-*/
/*************************************************************************/
int gsc_getPtOnLinearObj(ads_name ent, ads_real offset, ads_point pt, ads_real *Angle)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   int          Result = GS_GOOD;
   AcGePoint3d  ResultPt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return -1; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return -1; }
   Result = gsc_getPtOnLinearObj((AcDbEntity *) pObj, offset, ResultPt, Angle);
   pObj->close();

   if (Result == GS_GOOD)
   {
      pt[X] = ResultPt.x;
      pt[Y] = ResultPt.y;
      pt[Z] = ResultPt.z;
   }

   return Result;
}
int gsc_getPtOnLinearObj(AcDbEntity *pEnt, ads_real offset, AcGePoint3d &pt, ads_real *Angle)
{
   ads_real  PrevLen = 0, Len = 0;
   int       result = GS_GOOD;

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      bool               Spline;
      AcGePoint3d        p1, p2, center, PrevPt;
      ads_real           radius, ArcAngle, PrevArcAngle; // angolo inscritto

      if (((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dQuadSplinePoly ||
          ((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         p1 = pVertex->position();
         if (pVertex->bulge() != 0)
            // vertice curva di adattamento (se <> 0 segue un arco)
            ArcAngle = atan(pVertex->bulge()) * 4; // tang di 1/4 angolo incluso
         else
            ArcAngle = 0;

         pVertex->close();
         break;
      }
      pVertIter->step();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         p2 = pVertex->position();

         PrevLen = Len;
         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, p2, ArcAngle, &radius, center);
            Len += gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len += gsc_dist(p1, p2);

         if (offset <= Len) break;

         PrevArcAngle = ArcAngle;

         if (pVertex->bulge() != 0)
            // vertice curva di adattamento (se <> 0 segue un arco)
            ArcAngle = atan(pVertex->bulge()) * 4; // tang di 1/4 angolo incluso
         else
            ArcAngle = 0;

         PrevPt = p1;
         p1     = p2;
      }

      if (offset > Len) // offset è maggiore della lunghezza totale
      {
         if (PrevArcAngle != 0) // arco
         {
            if (PrevArcAngle > 0) // senso antiorario
               *Angle = gsc_angle(center, p2) + (PI / 2);
            else // senso orario
               *Angle = gsc_angle(center, p2) - (PI / 2);
         }
         else
            *Angle = gsc_angle(PrevPt, p2);

         gsc_polar(p2, *Angle, offset - Len, pt);
      }
      else
      {
         if (ArcAngle != 0) // arco
         {
            *Angle = gsc_angle(center, p1);
            if (ArcAngle > 0) // senso antiorario
            {
               *Angle += gsc_getArcAngle(radius, offset - PrevLen);
               gsc_polar(center, *Angle, radius, pt);
               *Angle += (PI / 2);
            }
            else // senso orario
            {
               *Angle -= gsc_getArcAngle(radius, offset - PrevLen);
               gsc_polar(center, *Angle, radius, pt);
               *Angle -= (PI / 2);
            }
         }
         else
         {
            *Angle = gsc_angle(p1, p2);
            gsc_polar(p1, *Angle, offset - PrevLen, pt);
         }      
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      ads_real    radius, ArcAngle, PrevArcAngle; // angolo inscritto
      AcGePoint2d p1, p2, center, PrevPt;
      int         NumVerts = ((AcDbPolyline *) pEnt)->numVerts();
      double      Bulge;

      ((AcDbPolyline *) pEnt)->getPointAt(0, p1);
      ((AcDbPolyline *) pEnt)->getBulgeAt(0, Bulge);       
      ArcAngle = atan(Bulge) * 4; // vertice curva di adattamento (segue un arco)

      for (int i = 1; i < NumVerts; i++) // punti successivi
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, p2);

         PrevLen = Len;
         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(p1, p2, ArcAngle, &radius, center);
            Len += gsc_getArcLen(radius, ArcAngle);
         }
         else
            Len += gsc_dist(p1, p2);

         if (offset <= Len) break;

         PrevArcAngle = ArcAngle;
         ((AcDbPolyline *) pEnt)->getBulgeAt(i, Bulge); // tang di 1/4 angolo incluso     
         ArcAngle = atan(Bulge) * 4; // vertice curva di adattamento (segue un arco)

         PrevPt = p1;
         p1     = p2;
      }

      if (offset > Len) // offset è maggiore della lunghezza totale
      {
         if (PrevArcAngle != 0) // arco
         {
            if (PrevArcAngle > 0) // senso antiorario
               *Angle = gsc_angle(center, p2) + (PI / 2);
            else // senso orario
               *Angle = gsc_angle(center, p2) - (PI / 2);
         }
         else
            *Angle = gsc_angle(PrevPt, p2);

         gsc_polar(p2, *Angle, offset - Len, p1);
         pt.set(p1.x, p1.y, 0);
      }
      else
      {
         if (ArcAngle != 0) // arco
         {
            *Angle = gsc_angle(center, p1);
            if (ArcAngle > 0) // senso antiorario
            {
               *Angle += gsc_getArcAngle(radius, offset - PrevLen);
               gsc_polar(center, *Angle, radius, p1);
               pt.set(p1.x, p1.y, 0);
               *Angle += (PI / 2);
            }
            else // senso orario
            {
               *Angle -= gsc_getArcAngle(radius, offset - PrevLen);
               gsc_polar(center, *Angle, radius, p1);
               pt.set(p1.x, p1.y, 0);
               *Angle -= (PI / 2);
            }
         }
         else
         {
            *Angle = gsc_angle(p1, p2);
            gsc_polar(p1, *Angle, offset - PrevLen, p1);
            pt.set(p1.x, p1.y, 0);
         }      
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d p1, p2, PrevPt;
      int         NumVerts = ((AcDbMline *) pEnt)->numVertices();

      p1 = ((AcDbMline *) pEnt)->vertexAt(0);

      for (int i = 1; i < NumVerts; i++) // punti successivi
      {
         p2 = ((AcDbMline *) pEnt)->vertexAt(i);

         PrevLen = Len;
         Len += gsc_dist(p1, p2);

         if (offset <= Len) break;

         PrevPt = p1;
         p1     = p2;
      }

      if (offset > Len) // offset è maggiore della lunghezza totale
      {
         *Angle = gsc_angle(PrevPt, p2);
         gsc_polar(p2, *Angle, offset - Len, p1);
         pt.set(p1.x, p1.y, 0);
      }
      else
      {
         *Angle = gsc_angle(p1, p2);
         gsc_polar(p1, *Angle, offset - PrevLen, p1);
         pt.set(p1.x, p1.y, 0);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      AcGePoint3d p1, p2;

      p1 = ((AcDbLine *) pEnt)->startPoint(); // primo punto
      p2 = ((AcDbLine *) pEnt)->endPoint(); // primo punto
      *Angle = gsc_angle(p1, p2);
      gsc_polar(p1, *Angle, offset, pt);
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_real    radius, init_angle, final_angle;
      AcGePoint3d center;

      center = ((AcDbArc *) pEnt)->center();
      radius = ((AcDbArc *) pEnt)->radius();
      init_angle = ((AcDbArc *) pEnt)->startAngle();
      final_angle = ((AcDbArc *) pEnt)->endAngle();

      Len = gsc_getArcLen(radius, init_angle, final_angle);

      if (offset > Len) // offset è maggiore della lunghezza totale
      {
         gsc_polar(center, final_angle, radius, pt);
         *Angle = final_angle + (PI / 2);
         gsc_polar(pt, *Angle, offset - Len, pt);
      }
      else
      {
         init_angle += gsc_getArcAngle(radius, offset);
         *Angle = init_angle + (PI / 2);
         gsc_polar(center, init_angle, radius, pt);
      }
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (*Angle > (2 * PI))
      *Angle = fmod(*Angle, (2 * PI));

   return result;
}


/*************************************************************************/
/*.doc struct resbuf *gsc_getMiddlePts                   (ads_name dummy)*/
/*+
  La funzione riceve come parametro il nome di un'entita` e ne restituisce
  i punti notevi interpretati come punti intermedi dell'oggetto.
  
  La funzione puo' essere utilizzata con oggetti rispettivamente di tipo:
  POLYLINE,LWPOLYLINE   -> ne vengono restituiti i punti medi dei tratti
  LINE                  -> restituisce il punto medio della linea
  ARC                   -> restituisce il punto medio dell'arco.
  3DFACE/SOLID          -> restituisce i punti medi dei lati degli oggetti.
  
  La funzione in caso di errore restituisce NULL.  
-*/
/*************************************************************************/
resbuf *gsc_getMiddlePts(ads_name ent)
{
   presbuf   p_rb;
   C_RB_LIST descr, punti;
   ads_point centro, iniz, fine, risultato;
   ads_real  raggio = 0, angolo = 0, calco;

   if ((descr << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p_rb = descr.SearchType(0)) == NULL || p_rb->resval.rstring == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   if (gsc_strcmp(p_rb->resval.rstring, _T("POLYLINE")) == 0)
   {
      ads_name nxt;

      ads_name_set(ent, nxt);
      iniz[X] = iniz[Y] = iniz[Z] = fine[X] = fine[Y] = fine[Z] = 0.0;
      
      do
      {
         // prossimo vertice
         if (ads_entnext(nxt, nxt) == RTERROR)
            { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
         if ((descr << acdbEntGet(nxt)) == NULL)
            { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

         if ((iniz[X] != 0.0) && (fine[X] != 0.0) && (iniz[X] != fine[X]))
         {
            if (angolo != 0.0)
            {
               calco = 0.5 * ((1.0 / angolo) - angolo);
               // calcolo il centro dell'arco
               centro[X] = 0.5 * (iniz[X] + fine[X] - (fine[Y] - iniz[Y]) * calco);
               centro[Y] = 0.5 * (iniz[Y] + fine[Y] + (fine[X] - iniz[X]) * calco);
               centro[Z] = (iniz[Z] + fine[Z]) / 2.0;
               if (angolo > 0)
                  gsc_retMidArc(centro, iniz, fine, risultato);
               else
                  gsc_retMidArc(centro, fine, iniz, risultato);
           
               ads_point_set(risultato, centro);
            }
            else
               gsc_getMidLine(iniz, fine, centro);

            if ((punti += acutBuildList(RT3DPOINT, centro, 0)) == NULL)
               return GS_BAD;
         }

         // verifico fine polilinea
         if ((p_rb = descr.SearchType(0)) == NULL || p_rb->resval.rstring == NULL) 
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         if (gsc_strcmp(p_rb->resval.rstring, _T("VERTEX")) != 0) break;

         // punto del vertice
         if ((p_rb = descr.SearchType(10)) == NULL) 
            { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
         ads_2Dpoint_set(fine, iniz);
         ads_2Dpoint_set(p_rb->resval.rpoint, fine);

         // angolo di eventuale arco (opzionale)
         angolo = raggio;
         raggio = 0.0;
         if ((p_rb = descr.SearchType(42)) != NULL)
            raggio = p_rb->resval.rreal;
      }
      while (1);
   }
   else
   if (gsc_strcmp(p_rb->resval.rstring, _T("LWPOLYLINE")) == 0)
   {
      iniz[X] = iniz[Y] = iniz[Z] = fine[X] = fine[Y] = fine[Z] = 0.0;

      while (p_rb)
      {
         if ((iniz[X] != 0.0) && (fine[X] != 0.0) && (iniz[X] != fine[X]))
         {
            if (angolo != 0.0)
            {
               calco = 0.5 * ((1.0 / angolo) - angolo);
               // calcolo il centro dell'arco
               centro[X] = 0.5 * (iniz[X] + fine[X] - (fine[Y] - iniz[Y]) * calco);
               centro[Y] = 0.5 * (iniz[Y] + fine[Y] + (fine[X] - iniz[X]) * calco);
               centro[Z] = 0; // le LWPOLYLINE sono 2D
               if (angolo > 0)
                  gsc_retMidArc(centro, iniz, fine, risultato);
               else
                  gsc_retMidArc(centro, fine, iniz, risultato);
           
               ads_point_set(risultato, centro);
            }
            else
               gsc_getMid2DLine(iniz, fine, centro);

            if ((punti += acutBuildList(RT3DPOINT, centro, 0)) == NULL)
               return GS_BAD;
         }

         // punto del vertice
         while (p_rb && p_rb->restype != 10) p_rb = p_rb->rbnext;
         if (p_rb == NULL) break;
         ads_2Dpoint_set(fine, iniz); // le LWPOLYLINE sono 2D
         ads_2Dpoint_set(p_rb->resval.rpoint, fine);// le LWPOLYLINE sono 2D

         angolo = raggio;
         raggio = 0.0;
         // angolo di eventuale arco
         while (p_rb && p_rb->restype != 42) p_rb = p_rb->rbnext;
         if (p_rb == NULL) break;
         raggio = p_rb->resval.rreal;
      }
   }
   else
   if (gsc_strcmp(p_rb->resval.rstring, _T("LINE")) == 0)
   {
      // primo vertice
      if ((p_rb = descr.SearchType(10)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      ads_point_set(p_rb->resval.rpoint, iniz);
      // secondo vertice
      if ((p_rb = descr.SearchType(11)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      ads_point_set(p_rb->resval.rpoint, fine);

      gsc_getMidLine(iniz, fine, centro);

      if ((punti << acutBuildList(RT3DPOINT, centro, 0)) == NULL)
         return GS_BAD;
   }
   else
   if (gsc_strcmp(p_rb->resval.rstring, _T("ARC")) == 0)
   {
      // centro
      if ((p_rb = descr.SearchType(10)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      ads_point_set(p_rb->resval.rpoint, centro);

      // raggio
      if ((p_rb = descr.SearchType(40)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      raggio = p_rb->resval.rreal;
      
      // angolo iniziale
      if ((p_rb = descr.SearchType(50)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      angolo = p_rb->resval.rreal;

      acutPolar(centro, angolo, raggio, iniz);

      // angolo finale
      if ((p_rb = descr.SearchType(51)) == NULL) 
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      angolo = p_rb->resval.rreal;

      acutPolar(centro, angolo, raggio, fine);
      gsc_retMidArc(centro, iniz, fine, risultato);

      if ((punti << acutBuildList(RT3DPOINT, centro, 0)) == NULL)
         return GS_BAD;
   }
   else
   if (gsc_strcmp(p_rb->resval.rstring, _T("3DFACE")) == 0 ||
       gsc_strcmp(p_rb->resval.rstring, _T("SOLID")) == 0)
   {
      p_rb = descr.get_head();
      while (p_rb->rbnext != NULL)
      {
         if (p_rb->restype == 10)
         {
            ads_point_set(p_rb->resval.rpoint, iniz);
            ads_point_set(iniz, centro);
         }
         else if ((p_rb->restype >= 11) && (p_rb->restype < 14))
         {
            ads_point_set(p_rb->resval.rpoint, fine);
            gsc_getMidLine(centro, fine, risultato);
            
            if ((punti += acutBuildList(RT3DPOINT, risultato, 0)) == NULL) return GS_BAD;
            ads_point_set(fine, centro);
         }
         p_rb = p_rb->rbnext;
      }

      gsc_getMidLine(iniz, fine, risultato);
      
      if ((punti += acutBuildList(RT3DPOINT, risultato, 0)) == NULL) return GS_BAD;
   }
   else
     return NULL;

   punti.ReleaseAllAtDistruction(GS_BAD);

   return punti.get_head(); 
}


/*********************************************************/
/*.doc gsc_getPtList_BetweenPts               <external> */
/*+                                                                       
  Funzione che calcola la distanza che intercorre tra due punti
  lungo un oggetto lineare.
  Parametri:
  ads_name ent;               Oggetto da analizzare
  double DistanceFromStart;   Distanza dal punto iniziale
  double Length;              Lunghezza del tratto
  C_POINT_LIST &PtList;
 
  Restituisce la distanza in caso di successo altrimenti restituisce -1. 
-*/  
/*********************************************************/
int gsc_getPtList_BetweenPts(ads_name ent, double DistanceFromStart, double Length, C_POINT_LIST &PtList)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   int          Result = GS_GOOD;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return -1; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return -1; }
   Result = gsc_getPtList_BetweenPts((AcDbEntity *) pObj, DistanceFromStart, Length, PtList);
   pObj->close();

   return Result;
}
int gsc_getPtList_BetweenPts(AcDbEntity *pEnt, double DistanceFromStart, double Length, C_POINT_LIST &PtList)
{
   Acad::ErrorStatus Res;

   PtList.remove_all();
   GS_ERR_COD = eGSNoError;
   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      bool               Spline;

      AcGePoint3d  pt1, pt2, pt;
      double       Bulge, NextBulge, PartialLength;
      int          CalculateRes;
      C_POINT_LIST PartialPtList;
      C_POINT      *pPt;

      if (((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dQuadSplinePoly ||
          ((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         pt1 = pVertex->position();
         Bulge = pVertex->bulge();
         pVertex->close();
         break;
      }
      pVertIter->step();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         pt2       = pVertex->position();
         NextBulge = pVertex->bulge();
         pVertex->close();

         if (pt1 == pt2) continue; // evito i vertici sovrapposti

         if (Bulge == 0) // è un segmento
            CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, 0.0, DistanceFromStart, Length, 
                                                    PartialPtList, &PartialLength);
         else // è un arco
            CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, Bulge, DistanceFromStart, Length,
                                                    PartialPtList, &PartialLength);

         if (CalculateRes == GS_GOOD)
         {
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            PtList.add_point(((C_POINT *) PartialPtList.goto_pos(2))->point); // 1-index
            break;
         }

         if (GS_ERR_COD == eGSInvStartPt)
            DistanceFromStart -= PartialLength;
         else if (GS_ERR_COD == eGSInvEndPt)
         {
            DistanceFromStart = 0.0;
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            Length -= PartialLength;
         }

         Bulge = NextBulge;
         pt1   = pt2;
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc())) // 3D entity type
   {
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      bool                 Spline;

      AcGePoint3d  pt1, pt2, pt;
      double       PartialLength;
      int          CalculateRes;
      C_POINT_LIST PartialPtList;
      C_POINT      *pPt;

      if (((AcDb3dPolyline *) pEnt)->polyType() == AcDb::k2dQuadSplinePoly ||
          ((AcDb3dPolyline *) pEnt)->polyType() == AcDb::k2dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb3dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k3dFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k3dControlVertex ||
                pVertex->vertexType() == AcDb::k3dFitVertex)
               { pVertex->close(); continue; }

         pt1 = pVertex->position();
         pVertex->close();
         break;
      }
      pVertIter->step();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         pt2       = pVertex->position();
         pVertex->close();

         CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, 0.0, DistanceFromStart, Length, 
                                                 PartialPtList, &PartialLength);

         if (CalculateRes == GS_GOOD)
         {
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point);
            PtList.add_point(((C_POINT *) PartialPtList.goto_pos(2))->point); // 1-index
            break;
         }

         if (GS_ERR_COD == eGSInvStartPt)
            DistanceFromStart -= PartialLength;
         else if (GS_ERR_COD == eGSInvEndPt)
         {
            DistanceFromStart = 0.0;
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point);
            Length -= PartialLength;
         }

         pt1   = pt2;
      }
      delete pVertIter;
   }
   else      
   if (pEnt->isKindOf(AcDbArc::desc()))
   {
      AcGePoint3d pt1, pt2;
      double      Angle;

      if ((Res = ((AcDbArc *) pEnt)->getPointAtDist(DistanceFromStart, pt1)) != Acad::eOk)
         { GS_ERR_COD = eGSInvStartPt; return GS_BAD; }
      PtList.add_point(pt1.x, pt1.y, pt1.z);

      if ((Res = ((AcDbArc *) pEnt)->getPointAtDist(DistanceFromStart + Length, pt2)) != Acad::eOk)
      { 
         ((AcDbArc *) pEnt)->getEndParam(Angle);
         ((AcDbArc *) pEnt)->getPointAtParam(Angle, pt2); // punto finale dell'arco
      }

      Angle = gsc_angle(((AcDbArc *) pEnt)->center(), pt2) - gsc_angle(((AcDbArc *) pEnt)->center(), pt1);
      ((C_POINT *) PtList.get_cursor())->Bulge = tan(Angle / 4.0);

      if (Res != Acad::eOk) // se il punto finale era oltre l'arco
         { GS_ERR_COD = eGSInvEndPt; return GS_BAD; }

      PtList.add_point(pt2.x, pt2.y, pt2.z);
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      return gsc_getPtList_BetweenPts(((AcDbLine *) pEnt)->startPoint(), 
                                      ((AcDbLine *) pEnt)->endPoint(), 0.0, 
                                      DistanceFromStart, Length, PtList);
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d  pt1, pt2, pt;
      double       Bulge, NextBulge, PartialLength;
      int          NumVerts = ((AcDbPolyline *) pEnt)->numVerts(), CalculateRes;
      C_POINT_LIST PartialPtList;
      C_POINT      *pPt;

      ((AcDbPolyline *) pEnt)->getPointAt(0, pt1);
      ((AcDbPolyline *) pEnt)->getBulgeAt(0, Bulge);       

      for (int i = 1; i < NumVerts; i++)
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, pt2);
         ((AcDbPolyline *) pEnt)->getBulgeAt(i, NextBulge);       

         if (Bulge == 0) // è un segmento
            CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, 0.0, DistanceFromStart, Length, 
                                                    PartialPtList, &PartialLength);
         else // è un arco
            CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, Bulge, DistanceFromStart, Length,
                                                    PartialPtList, &PartialLength);

         if (CalculateRes == GS_GOOD)
         {
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            PtList.add_point(((C_POINT *) PartialPtList.goto_pos(2))->point); // 1-index
            break;
         }

         if (GS_ERR_COD == eGSInvStartPt)
            DistanceFromStart -= PartialLength;
         else if (GS_ERR_COD == eGSInvEndPt)
         {
            DistanceFromStart = 0.0;
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            Length -= PartialLength;
         }

         Bulge = NextBulge;
         pt1   = pt2;
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d  pt1, pt2, pt;
      double       PartialLength;
      int          NumVerts = ((AcDbMline *) pEnt)->numVertices(), CalculateRes;
      C_POINT_LIST PartialPtList;
      C_POINT      *pPt;

      pt1 = ((AcDbMline *) pEnt)->vertexAt(0);

      for (int i = 1; i < NumVerts; i++)
      {
         pt2 = ((AcDbMline *) pEnt)->vertexAt(i);

         CalculateRes = gsc_getPtList_BetweenPts(pt1, pt2, 0.0, DistanceFromStart, Length, 
                                                 PartialPtList, &PartialLength);

         if (CalculateRes == GS_GOOD)
         {
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            PtList.add_point(((C_POINT *) PartialPtList.goto_pos(2))->point); // 1-index
            break;
         }

         if (GS_ERR_COD == eGSInvStartPt)
            DistanceFromStart -= PartialLength;
         else if (GS_ERR_COD == eGSInvEndPt)
         {
            DistanceFromStart = 0.0;
            pPt = (C_POINT *) PartialPtList.get_head();
            PtList.add_point(pPt->point, pPt->Bulge);
            Length -= PartialLength;
         }

         pt1 = pt2;
      }
   }

   return GS_GOOD;
}
int gsc_getPtList_BetweenPts(AcGePoint3d &ptStart, AcGePoint3d &ptEnd, double Bulge, 
                             double DistanceFromStart, double Length, C_POINT_LIST &PtList,
                             double *MyLength)
{
   double    Angle, Len;
   ads_point point;

   PtList.remove_all();

   if (Bulge == 0) // segmento retto
   {
      Len = gsc_dist(ptStart, ptEnd);
      if (DistanceFromStart > Len) 
      { 
         if (MyLength) *MyLength = Len; // la lunghezza del segmento
         GS_ERR_COD = eGSInvStartPt; 
         return GS_BAD; 
      }
      Angle = gsc_angle(ptStart, ptEnd);
      ads_point_set_from_AcGePoint3d(ptStart, point);
      acutPolar(point, Angle, DistanceFromStart, point);
      PtList.add_point(point);
      if (DistanceFromStart + Length > Len) 
      {
         if (MyLength) *MyLength = gsc_dist(point, ptEnd); // la lunghezza dal punto trovato alla fine del segmento
         GS_ERR_COD = eGSInvEndPt; 
         return GS_BAD; 
      }
      acutPolar(point, Angle, Length, point);
      PtList.add_point(point);
   }
   else // arco
   {
      ads_point InitPt, FinalPt, Center;
      double    ArcAngle, Radius, startAngle, endAngle, Angle2;
      int       res = GS_GOOD;

      ArcAngle = atan(Bulge) * 4;
      ads_point_set_from_AcGePoint3d(ptStart, InitPt);
      ads_point_set_from_AcGePoint3d(ptEnd, FinalPt);
      gsc_getArcRayCenter(InitPt, FinalPt, ArcAngle, &Radius, Center, &startAngle, &endAngle);
      Angle = DistanceFromStart / Radius;

      if (Bulge < 0) // senso orario
      {
         if ((Angle = startAngle - Angle) < endAngle) 
         {
            if (MyLength) *MyLength = gsc_getArcLen(Radius, endAngle, startAngle); // la lunghezza dell'arco
            GS_ERR_COD = eGSInvStartPt; 
            return GS_BAD;
         }
         acutPolar(Center, Angle, Radius, point);
         PtList.add_point(point);

         Angle2 = Angle - (Length / Radius);
         if (Angle2 > endAngle)
            acutPolar(Center, Angle2, Radius, point);
         else
         {
            if (MyLength) *MyLength = gsc_getArcLen(Radius, endAngle, Angle); // la lunghezza dal punto trovato alla fine dell'arco
            Angle2 = endAngle;
            res = GS_BAD; 
         }
         Angle = Angle - Angle2;
         ((C_POINT *) PtList.get_cursor())->Bulge = -1 * tan(Angle / 4.0);
      }
      else // senso antiorario
      {
         if ((Angle = startAngle + Angle) > endAngle)
         {
            if (MyLength) *MyLength = gsc_getArcLen(Radius, startAngle, endAngle); // la lunghezza dell'arco
            GS_ERR_COD = eGSInvStartPt; 
            return GS_BAD;
         }
         acutPolar(Center, Angle, Radius, point);
         PtList.add_point(point);

         Angle2 = Angle + (Length / Radius);
         if (Angle2 <= endAngle)
            acutPolar(Center, Angle2, Radius, point);
         else
         {
            if (MyLength) *MyLength = gsc_getArcLen(Radius, Angle, endAngle); // la lunghezza dal punto trovato alla fine dell'arco
            Angle2 = endAngle;
            res = GS_BAD; 
         }

         Angle = Angle2 - Angle;
         ((C_POINT *) PtList.get_cursor())->Bulge = tan(Angle / 4.0);
      }

      // se il punto finale era oltre l'arco
      if (res == GS_BAD)
      {
         GS_ERR_COD = eGSInvEndPt; 
         return GS_BAD; 
      }

      PtList.add_point(point);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getClosestPointTo                  <external> */
/*+                                                                       
  Funzione che restituisce il punto dell'entità più vicina a
  ad un punto dato.
  Parametri:
  ads_name ent;               Oggetto da analizzare
  ads_point pt;               Punto di riferimento
  ads_point ClosestPointTo;   Punto dell'entità più vicino a pt
  double *distance;           Opzionale; minima distanza tra ent e pt
                              (default = NULL)
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_getClosestPointTo(ads_name ent, ads_point pt, ads_point ClosestPointTo, double *distance)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   AcGePoint3d  ReferencePt, ResultPt;
   int          Result = GS_GOOD;

   AcGePoint3d_set_from_ads_point(pt, ReferencePt);
   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if ((Result = gsc_getClosestPointTo(pObj, ReferencePt, ResultPt, distance)) == GS_GOOD)
      ads_point_set_from_AcGePoint3d(ResultPt, ClosestPointTo);

   pObj->close();

   return Result;
}
int gsc_getClosestPointTo(AcDbObject *pObj, AcGePoint3d &pt, AcGePoint3d &ClosestPointTo, double *distance)
{
   int Result = GS_GOOD;

   if (pObj->isKindOf(AcDb2dPolyline::desc()))
   {
      if (((AcDb2dPolyline *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDb3dPolyline::desc())) // 3D entity type
   {
      if (((AcDb3dPolyline *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else      
   if (pObj->isKindOf(AcDbArc::desc()))
   {
      if (((AcDbArc *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDbLeader::desc()))
   {
      if (((AcDbLeader *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDbLine::desc()))
   {
      if (((AcDbLine *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDbMline::desc()))
   {
      if (((AcDbMline *) pObj)->getClosestPointTo(pt, ClosestPointTo, false) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (pObj->isKindOf(AcDbSpline::desc()))
   {
      if (((AcDbSpline *) pObj)->getClosestPointTo(pt, ClosestPointTo) != Acad::eOk)
         Result = GS_BAD;
   }
   else
   if (gsc_isPunctualEntity((AcDbEntity *) pObj))
   {
      ads_point _p;
      if (gsc_get_firstPoint((AcDbEntity *) pObj, _p) == GS_BAD) return GS_BAD;
      AcGePoint3d_set_from_ads_point(_p, ClosestPointTo);
   }
   else
   {
      GS_ERR_COD = eGSInvGraphObjct;
      Result = GS_BAD;
   }

   pObj->close();

   if (Result == GS_GOOD)
      if (distance) *distance = ClosestPointTo.distanceTo(pt);

   return Result;
}


/*********************************************************/
/*.doc gsc_getClosestPointTo                  <external> */
/*+                                                                       
  Funzione che restituisce il punto dell'entità più vicina a
  ad un punto dato.
  Parametri:
  ads_name Ent;               Oggetto da analizzare
  ads_point pt;               Oggetto di riferimento
  ads_point ClosestPointTo;   Punto dell'entità più vicino a pt
  double *distance;           Opzionale; minima distanza tra ent e pt
                              (default = NULL)
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_getClosestPointTo(ads_name Ent, ads_name ReferenceEnt,
                          ads_point EntClosestPointTo, ads_point ReferenceEntClosestPointTo, double *distance)
{
   AcDbObjectId objId;
   AcDbEntity   *pObj, *pReferenceObj;
   AcGePoint3d  ReferencePt, ResultPt;
   int          Result = GS_GOOD;

   if (acdbGetObjectId(objId, Ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbGetObjectId(objId, ReferenceEnt) != Acad::eOk)
      { pObj->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pReferenceObj, objId, AcDb::kForRead) != Acad::eOk)
      { pObj->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if ((Result = gsc_getClosestPointTo(pObj, pReferenceObj, ResultPt, ReferencePt, distance)) == GS_GOOD)
   {
      ads_point_set_from_AcGePoint3d(ResultPt, EntClosestPointTo);
      ads_point_set_from_AcGePoint3d(ReferencePt, ReferenceEntClosestPointTo);
   }

   pObj->close();
   pReferenceObj->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_getClosestTo                       <external> */
/*+
  Questa funzione ottiene un gruppo di entità (ResultEntitySet) che sono quelle 
  di un gruppo (ObjEntitySet) più vicine ad un altro gruppo di entità (ReferenceEntitySet).
  Parametri:
  AcDbEntityPtrArray &ObjEntitySet;       Gruppo di entità da cui scegliere quelle più vicine
  AcDbEntityPtrArray &ReferenceEntitySet; Gruppo di entità da cui calcolare la distanza
  AcDbEntityPtrArray &ResultEntitySet;    Gruppo delle entità più vicine
  int Qty;                                Quante entità mantenere tra le più vicine (default = 1)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getClosestTo(AcDbEntityPtrArray &ObjEntitySet, AcDbEntityPtrArray &ReferenceEntitySet,
                     AcDbEntityPtrArray &ResultEntitySet, int Qty) 
{
   AcDbEntity         *_ent, *Ent;
   long               i, j;
   AcGePoint3d        pt, pt1;
   double             distance;
   C_REAL_VOIDPTR_LIST Distance_Ent_List; // lista delle distanze delle entità in ClosestSS
   C_REAL_VOIDPTR      *pDistance_Ent;

   for (i = 0; i < ObjEntitySet.length(); i++)
   {
      _ent = ObjEntitySet[i];
      for (j = 0; j < ReferenceEntitySet.length(); j++)
      {
         Ent = ReferenceEntitySet[j];
         if (gsc_getClosestPointTo(_ent, Ent, pt, pt1, &distance) != GS_GOOD) return GS_BAD;

         // aggiungo una nuova distanza
         if (Distance_Ent_List.insert_ordered_double_voidptr(distance, _ent) == GS_BAD) return GS_BAD;

         // se devo selezionare solo le entità con distanza minore
         if (Qty > 0 && Distance_Ent_List.get_count() > Qty)
            Distance_Ent_List.remove_tail();
      }
   }

   ResultEntitySet.removeAll();
   pDistance_Ent = (C_REAL_VOIDPTR *) Distance_Ent_List.get_head();
   while (pDistance_Ent)
   {
      ResultEntitySet.append(((AcDbEntity *) pDistance_Ent->get_VoidPtr()));
      pDistance_Ent = (C_REAL_VOIDPTR *) Distance_Ent_List.get_next();
   }

   return GS_GOOD;
}
int gsc_getClosestPointTo(AcDbEntity *pObj, AcDbEntity *pReferenceObj,
                          AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo, double *distance)
{
   ads_point    point;
   C_POINT_LIST IntersectionPts;
   C_POINT      *pIntersectionPt;

   // Verifico le intersezioni del segmento orizzontale con pContainerEnt
   if (IntersectionPts.add_intersectWith(pObj, pReferenceObj) != GS_GOOD)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if ((pIntersectionPt = (C_POINT *) IntersectionPts.get_head()))
   {
      AcGePoint3d_set_from_ads_point(pIntersectionPt->point, ClosestPointTo);
      AcGePoint3d_set_from_ads_point(pIntersectionPt->point, ReferenceClosestPointTo);
      *distance = 0;

      return GS_GOOD;
   }

   if (gsc_isPunctualEntity(pObj)) // Se pObj è puntuale
   {  // inverto le entità perchè la gsc_getClosestPointTo può accettare
      // un elemento lineare come primo parametro e un punto come secondo
      if (gsc_get_firstPoint(pObj, point) == GS_BAD) return GS_BAD;
      AcGePoint3d_set_from_ads_point(point, ClosestPointTo);
      return gsc_getClosestPointTo(pReferenceObj, ClosestPointTo, ReferenceClosestPointTo, distance);
   }

   if (gsc_isPunctualEntity(pReferenceObj)) // Se pReferenceObj è puntuale
   {
      if (gsc_get_firstPoint(pReferenceObj, point) == GS_BAD) return GS_BAD;
      AcGePoint3d_set_from_ads_point(point, ReferenceClosestPointTo);
      return gsc_getClosestPointTo(pObj, ReferenceClosestPointTo, ClosestPointTo, distance);
   }

   // Entrambi pObj e pReferenceObj sono lineari

   // li esplodo ricorsivamente per trattare oggetti semplici (linee ed archi)
   AcDbVoidPtrArray ObjEntitySet, ReferenceEntitySet;
   AcDbEntity       *pExplodedObj, *pExplodedReference;
   AcGePoint3d      BestClosestPointTo, BestReferenceClosestPointTo;
   double           BestDistance = (std::numeric_limits<double>::max)(), _distance;
   bool             ObjEntitySetToDelete = true, ReferenceEntitySetToDelete = true;
   int              i, j;

   if (gsc_explodeNoGraph(pObj, ObjEntitySet, true) == GS_BAD) // se non si può esplodere
   {
      if (!pObj->isKindOf(AcDbLine::desc()) && !pObj->isKindOf(AcDbArc::desc()))
      {  // Se non si tratta di linea o arco provo a trasformarlo in polilinea
         C_POINT_LIST PtList;
         
         if (PtList.add_vertex_point(pObj, GS_GOOD, GEOsimAppl::TOLERANCE) == GS_BAD) return GS_BAD;
         if (!(pExplodedObj = PtList.toPolyline())) return GS_BAD;
         if (gsc_explodeNoGraph(pExplodedObj, ObjEntitySet, true) == GS_BAD) // se non si può esplodere
            return GS_BAD;
         delete pExplodedObj;
      }
      else
      {
         ObjEntitySet.append(pObj);
         ObjEntitySetToDelete = false;
      }
   }

   if (gsc_explodeNoGraph(pReferenceObj, ReferenceEntitySet, true) == GS_BAD) // se non si può esplodere
   {
      if (!pReferenceObj->isKindOf(AcDbLine::desc()) && !pReferenceObj->isKindOf(AcDbArc::desc()))
      {  // Se non si tratta di linea o arco provo a trasformarlo in polilinea
         C_POINT_LIST PtList;
         
         if (PtList.add_vertex_point(pReferenceObj, GS_GOOD, GEOsimAppl::TOLERANCE) == GS_BAD) return GS_BAD;
         if (!(pExplodedReference = PtList.toPolyline())) return GS_BAD;
         if (gsc_explodeNoGraph(pExplodedReference, ReferenceEntitySet, true) == GS_BAD) // se non si può esplodere
            return GS_BAD;
         delete pExplodedReference;
      }
      else
      {
         ReferenceEntitySet.append(pReferenceObj);
         ReferenceEntitySetToDelete = false;
      }
   }

   // per ciascun segmento dell'entità pObj
   for (i = 0; i < ObjEntitySet.length(); i++)
   {
      pExplodedObj = (AcDbEntity *) ObjEntitySet[i];

      // per ciascun segmento dell'entità pReference
      for (j = 0; j < ReferenceEntitySet.length(); j++)
      {
         pExplodedReference = (AcDbEntity *) ReferenceEntitySet[j];

         // sono entrambe linee
         if (pExplodedObj->isKindOf(AcDbLine::desc()) && pExplodedReference->isKindOf(AcDbLine::desc()))
         {
            if (gsc_getClosestPointTo((AcDbLine *) pExplodedObj, (AcDbLine *) pExplodedReference,
                                      ClosestPointTo, ReferenceClosestPointTo, &_distance) == GS_BAD)
               return GS_BAD;
         }
         else // una linea e un arco
         if ((pExplodedObj->isKindOf(AcDbLine::desc()) || pExplodedObj->isKindOf(AcDbArc::desc())) &&
             pExplodedReference->isKindOf(AcDbArc::desc()))
         {
            if (gsc_getClosestPointToLineOrArc(pExplodedObj, (AcDbArc *) pExplodedReference,
                                               ClosestPointTo, ReferenceClosestPointTo, &_distance) == GS_BAD)
               return GS_BAD;
         }
         else // un arco e una linea 
         if (pExplodedObj->isKindOf(AcDbArc::desc()) && pExplodedReference->isKindOf(AcDbLine::desc()))
         {  // inverto pExplodedObj con ReferenceClosestPointTo
            if (gsc_getClosestPointToLineOrArc(pExplodedReference, (AcDbArc *) pExplodedObj,
                                               ReferenceClosestPointTo, ClosestPointTo, &_distance) == GS_BAD)
               return GS_BAD;
         }
         else
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         if (BestDistance > _distance)
         {
            BestDistance                = _distance;
            BestClosestPointTo          = ClosestPointTo;
            BestReferenceClosestPointTo = ReferenceClosestPointTo;
         }
      }
   }

   if (ObjEntitySetToDelete)
      for (i = 0; i < ObjEntitySet.length(); i++) delete (AcDbEntity *) ObjEntitySet[i];
   if (ReferenceEntitySetToDelete)
      for (i = 0; i < ReferenceEntitySet.length(); i++) delete (AcDbEntity *) ReferenceEntitySet[i];

   if (distance) *distance = BestDistance;
   ClosestPointTo          = BestClosestPointTo;
   ReferenceClosestPointTo = BestReferenceClosestPointTo;

   return GS_GOOD;
}
int gsc_getClosestPointTo(AcDbLine *pObj, AcDbLine *pReferenceObj,
                          AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo, double *distance)
{
   AcGePoint3d ClosestPointToStart, ClosestPointToEnd;
   double      DistanceFromStart, DistanceFromEnd;

   // calcolo minima distanza dal punto iniziale
   if (gsc_getClosestPointTo(pObj, pReferenceObj->startPoint(), ClosestPointToStart, &DistanceFromStart) == GS_BAD) return GS_BAD;
   // calcolo minima distanza dal punto finale
   if (gsc_getClosestPointTo(pObj, pReferenceObj->endPoint(), ClosestPointToEnd, &DistanceFromEnd) == GS_BAD) return GS_BAD;

   if (DistanceFromStart < DistanceFromEnd) // se il punto iniziale è più vicino del punto finale
      ClosestPointTo = ClosestPointToStart;
   else // se il punto finale è più vicino del punto iniziale
      ClosestPointTo = ClosestPointToEnd;

   // Se il punto più vicino ricade sugli estremi
   if (ClosestPointTo == pObj->startPoint() || ClosestPointTo == pObj->endPoint())
      // calcolo minima distanza dal punto finale
      return gsc_getClosestPointTo(pReferenceObj, ClosestPointTo, ReferenceClosestPointTo, distance);

   if (DistanceFromStart < DistanceFromEnd) // se il punto iniziale è più vicino del punto finale
   {
      ReferenceClosestPointTo = pReferenceObj->startPoint();
      if (distance) *distance = DistanceFromStart;
   }
   else // se il punto finale è più vicino del punto iniziale
   {
      ReferenceClosestPointTo = pReferenceObj->endPoint();
      if (distance) *distance = DistanceFromEnd;
   }

   return GS_GOOD;
}
int gsc_getClosestPointToLineOrArc(AcDbEntity *pObj, AcDbArc *pReferenceObj,
                                   AcGePoint3d &ClosestPointTo, AcGePoint3d &ReferenceClosestPointTo, double *distance)
{
   ads_point   point;
   AcGePoint3d StartPt, EndPt;
   double      DistanceFromStart, DistanceFromEnd;

   if (!pObj->isKindOf(AcDbLine::desc()) && !pObj->isKindOf(AcDbArc::desc()))
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // Leggo punto iniziale
   ads_point_set_from_AcGePoint3d(pReferenceObj->center(), point);
   acutPolar(point, pReferenceObj->startAngle(), pReferenceObj->radius(), point);
   AcGePoint3d_set_from_ads_point(point, StartPt);
   // Leggo punto finale
   ads_point_set_from_AcGePoint3d(pReferenceObj->center(), point);
   acutPolar(point, pReferenceObj->endAngle(), pReferenceObj->radius(), point);
   AcGePoint3d_set_from_ads_point(point, EndPt);

   // calcolo minima distanza dal punto iniziale
   if (gsc_getClosestPointTo(pObj, StartPt, ClosestPointTo, &DistanceFromStart) == GS_BAD) return GS_BAD;
   ReferenceClosestPointTo = StartPt;
   if (distance) *distance = DistanceFromStart;
   // calcolo minima distanza dal punto finale
   if (gsc_getClosestPointTo(pObj, EndPt, ClosestPointTo, &DistanceFromEnd) == GS_BAD) return GS_BAD;

   while (gsc_dist(StartPt, EndPt) > GEOsimAppl::TOLERANCE)
   {
      double dummy = gsc_dist(StartPt, EndPt);
      // calcolo il punto medio        
      gsc_retMidArc(pReferenceObj->center(), StartPt, EndPt, ReferenceClosestPointTo);

      if (DistanceFromStart > DistanceFromEnd) // se il punto iniziale è più distante del punto finale
      {
         StartPt = ReferenceClosestPointTo;
         // calcolo minima distanza dal punto iniziale
         if (gsc_getClosestPointTo(pObj, StartPt, ClosestPointTo, &DistanceFromStart) == GS_BAD) return GS_BAD;
         if (distance) *distance = DistanceFromStart;
      }
      else // se il punto finale è più distante del punto finale
      {
         EndPt = ReferenceClosestPointTo;
         // calcolo minima distanza dal punto finale
         if (gsc_getClosestPointTo(pObj, EndPt, ClosestPointTo, &DistanceFromEnd) == GS_BAD) return GS_BAD;
         if (distance) *distance = DistanceFromEnd;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getDistAtPoint                     <external> */
/*+                                                                       
  Funzione che restituisce la distanza tra il punto dato e il
  punto iniziale dell'entità.
  Parametri:
  ads_name ent;               Oggetto da analizzare
  ads_point pt;               Punto di riferimento
 
  Restituisce la distanza in caso di successo altrimenti restituisce -1. 
-*/  
/*********************************************************/
double gsc_getDistAtPoint(ads_name ent, ads_point pt)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   AcGePoint3d  ReferencePt;
   double       Result = -1;

   AcGePoint3d_set_from_ads_point(pt, ReferencePt);
   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (pObj->isKindOf(AcDb2dPolyline::desc()))
   {
      if (((AcDb2dPolyline *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   if (pObj->isKindOf(AcDb3dPolyline::desc())) // 3D entity type
   {
      if (((AcDb3dPolyline *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else      
   if (pObj->isKindOf(AcDbArc::desc()))
   {
      if (((AcDbArc *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   if (pObj->isKindOf(AcDbLeader::desc()))
   {
      if (((AcDbLeader *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   if (pObj->isKindOf(AcDbLine::desc()))
   {
      if (((AcDbLine *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   if (pObj->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   if (pObj->isKindOf(AcDbSpline::desc()))
   {
      if (((AcDbSpline *) pObj)->getDistAtPoint(ReferencePt, Result) != Acad::eOk)
         Result = -1;
   }
   else
   {
      GS_ERR_COD = eGSInvGraphObjct;
      Result = -1;
   }

   pObj->close();

   return Result;
}


/**************************************************************/
/*.doc gsc_getLength                             <external>   */
/*+
   Ritorna la lunghezza dell'ogetto grafico.
   Parametri:
   AcDbEntity *pEnt;  entità grafica
-*/  
/**************************************************************/
double gsc_getLength(AcDbEntity *pEnt)
{
   double EndParam, Length;

   if (gsc_isLinearEntity(pEnt))
   {    
      ((AcDbCurve *) pEnt)->getEndParam(EndParam);
      ((AcDbCurve *) pEnt)->getDistAtParam(EndParam, Length);
   }
   else
      Length = 0;

   return Length;
}


///////////////////////////////////////////////////////////////////////////
int gs_set_linetype(void)
{
   TCHAR  *name;
   TCHAR  old[MAX_LEN_LINETYPENAME];
   resbuf *arg;

   acedRetNil(); 
   arg=acedGetArgs();

   if (arg==NULL || arg->restype!=RTSTR)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   name = arg->resval.rstring;
   
   if (gsc_set_CurrentLineType(name,old)==GS_BAD) return RTERROR;
      
   acedRetT();
   return RTNORM;
}
int gs_set_textstyle(void)
{
   TCHAR     *name;
   TCHAR     old[MAX_LEN_TEXTSTYLENAME];
   resbuf   *arg;

   acedRetNil(); 
   arg=acedGetArgs();

   if (arg==NULL || arg->restype!=RTSTR)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   name=arg->resval.rstring;
   
   if (gsc_set_CurrentTextStyle(name,old)==GS_BAD) return RTERROR;
      
   acedRetT();
   return RTNORM;
}
int gs_set_elevation(void)
{
   double elev, old;
   resbuf *arg;

   acedRetNil(); 
   arg=acedGetArgs();

   if (arg==NULL || arg->restype!=RTREAL)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   elev=arg->resval.rreal;
   
   if (gsc_set_CurrentElevation(elev,&old)==GS_BAD) return RTERROR;

   acedRetT();
   return RTNORM;
}
int gs_set_htext(void)
{
   double htext, old;
   resbuf *arg;

   acedRetNil(); 
   arg = acedGetArgs();

   if (arg==NULL || arg->restype!=RTREAL)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   htext=arg->resval.rreal;
   
   if (gsc_set_CurrentHText(htext, &old) == GS_BAD) return RTERROR;

   acedRetT();
   return RTNORM;
}
int gs_set_width(void)
{
double   width,old;
resbuf   *arg;

   acedRetNil(); 
   arg=acedGetArgs();

   if (arg==NULL || arg->restype!=RTREAL)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   width=arg->resval.rreal;
   
   if (gsc_set_width(width,&old)==GS_BAD) return RTERROR;

   acedRetT();
   return RTNORM;
}
int gs_set_thickness(void)
{
double   thickness,old;
resbuf   *arg;

   acedRetNil(); 
   arg=acedGetArgs();

   if (arg==NULL || arg->restype!=RTREAL)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   thickness=arg->resval.rreal;
   
   if (gsc_set_CurrentThickness(thickness,&old)==GS_BAD) return RTERROR;

   acedRetT();
   return RTNORM;
}


/*+
-------------------------------------------------------------
  Queste funzioni richiamano la funzione di UNDO di autocad 
  corrispondenti.
  
  gsc_startTransaction() -> Da richiamare all'inizio di ogni comando GEOSIM per la
                 gestione dell'undo.
  gsc_endTransaction() -> Da richiamare alla fine di un comando di GEOSIM inizializzato da
              gsc_startTransaction() per la gestione dell'undo.
  gsc_abortTransaction() -> Annullamento di un comando GEOSIM inizializzato da gsc_startTransaction()+
                  da richiamare in caso di errore o di annullamento dall'utente
                  per la gestione dell'undo.
                                     
-------------------------------------------------------------
-*/
//-----------------------------------------------------------------------//
// Questa funzione non fa' nulla serve solo ad esegure un comando
// annullando il quale si ripristina una situazione precedente (undo EED)
int gsc_finto()    
{
   int last_echo;

   if (gsc_set_echo(0, &last_echo) == GS_BAD) return GS_BAD;
   if (acedCommandS(RTSTR, _T("_.COLOR"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
      { gsc_set_echo(last_echo); GS_ERR_COD = eGSUndoErr; return GS_BAD; }
   if (gsc_set_echo(last_echo) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetUndoRecording                     <extern> */
/*+
   Disabilita o abilita la registrazione per UNDO per il database
   associato al documento corrente.
   Parametr.:
   int State;  TRUE per attivare UNDO, FALSE per disattivarlo
   
   Ritorna lo stato di UNDO precedente (TRUE se era attivo altrimenti FALSE).
   N.B: la funzione è richiamabile da finestre modeless.
-*/
/*********************************************************/
int gsc_SetUndoRecording(int State)
{
   int Result = (curDoc()->database()->undoRecording() == Adesk::kTrue) ? TRUE : FALSE;
   curDoc()->database()->disableUndoRecording((State) ? Adesk::kFalse : Adesk::kTrue);
   return Result;
}
int gsc_ClearUndoStack(void)
{
   resbuf rb;
   int    undoCtl;

   // disattiva UNDO e scarica tutte le informazioni di UNDO precedenti
   acedGetVar(_T("UNDOCTL"), &rb);
   gsc_rb2Int(&rb, &undoCtl);

   // 0  = Il comando ANNULLA è disattivato 
   // 1  = Il comando ANNULLA è attivato
   // 2  = Può essere attivato un solo comando
   // 4  = La funzionalità automatica è attivata
   // 8  = Un gruppo è attualmente attivo
   // 16 = Le operazioni di zoom e panoramica sono raggruppate in una singola azione
   // 32 = Le operazioni delle proprietà layer sono raggruppate in una singola azione 
   bool isOn = (undoCtl & 1) == 1; // UNDO abilitato (Control)
   bool isOneCmd = (undoCtl & 2) == 2; // UNDO abilitato su un solo comando (One)
   bool isAuto = (undoCtl & 4) == 4; // UNDO automatico abilitato

   if (!isOn) return GS_GOOD;

   // se non sono nelle condizioni di avviare un comando in modo sincrono
   // (ad es. in un evento o in una finestra modeless)
   if (gsc_callCmd(_T("_.UNDO"), RTSTR, _T("_Control"), RTSTR, _T("_None"), 0) != RTNORM)
   { // provo in maniera asincrona
      C_STRING Cmd(_T("_.UNDO\n_Control\n_None\n"));

      if (isOneCmd) // UNDO abilitato su un solo comando (One)
         Cmd += _T("_.UNDO\n_One\n");
      else
         Cmd += _T("_.UNDO\n_All\n"); // UNDO abilitato su un tutti i comando (All)

      if (isAuto) // UNDO automatico abilitato
         Cmd += _T("_.UNDO\n_Auto\n_Off\n");

      if (acDocManager->sendStringToExecute(acDocManager->curDocument(), Cmd.get_name(),
                                            true, false, false) != Acad::eOk)
         { GS_ERR_COD = eGSUndoErr; return GS_BAD; }
   }
   else
   {
      if (isOneCmd) // UNDO abilitato su un solo comando (One)
      {
         if (gsc_callCmd(_T("_.UNDO"), RTSTR, _T("_One"), 0) != RTNORM)
            { GS_ERR_COD = eGSUndoErr; return GS_BAD; }
      }
      else
         if (gsc_callCmd(_T("_.UNDO"), RTSTR, _T("_All"), 0) != RTNORM) // UNDO abilitato su un tutti i comando (All)
            { GS_ERR_COD = eGSUndoErr; return GS_BAD; }

      if (isAuto) // UNDO automatico abilitato
         if (gsc_callCmd(_T("_.UNDO"), RTSTR, _T("_Auto"), RTSTR, _T("_Off"), 0) != RTNORM)
            { GS_ERR_COD = eGSUndoErr; return GS_BAD; }

   }

   return GS_GOOD;
}
   

void gsc_disableUndoRecording(bool disable)   
{
   acdbHostApplicationServices()->workingDatabase()->disableUndoRecording(disable);
}

//-----------------------------------------------------------------------//
int gsc_startTransaction()    
{
   return (actrTransactionManager->startTransaction() != NULL) ? GS_GOOD : GS_BAD;
}
//-----------------------------------------------------------------------//
int gsc_endTransaction()
{
   return (actrTransactionManager->endTransaction() == Acad::eOk) ? GS_GOOD : GS_BAD;
}
//-----------------------------------------------------------------------//
int gsc_abortTransaction()
{
   return (actrTransactionManager->abortTransaction() == Acad::eOk) ? GS_GOOD : GS_BAD;
}
//-----------------------------------------------------------------------//



/*+
-------------------------------------------------------------
  Registrano il nome dell'applicazione per l'utilizzo
  Extended Data dell'entita' di Autocad. 

int gsc_regapp()  -> Registra il il nome dell'applicazione GEOSIM
                     GEO_APP_ID="GS".
int gsc_regapp(const TCHAR* appl)

-------------------------------------------------------------
-*/
int gsc_regapp() { return gsc_regapp(GEO_APP_ID); }
int gsc_regapp(const TCHAR* appl)
{
   resbuf *rb;

   if ((rb = acdbTblSearch(_T("APPID"), appl, 0)) == NULL)
   {
      if (acdbRegApp(appl) == RTERROR)
         { GS_ERR_COD = eGSInvalidApp; return GS_BAD; }
   } 
   else acutRelRb(rb);

   return GS_GOOD;
}


/**************************************************************/
/*.doc gsc_ispline     <external>                             */
/*+
   Ritorna GS_GOOD se ent e' di tipo POLYLINE o LWPOLYLINE,
   altrimenti GS_BAD;
   Parametri:
   ads_name ent;  entità grafica
-*/  
/**************************************************************/
int gsc_ispline(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("POLYLINE"), type) == 0 || gsc_strcmp(_T("LWPOLYLINE"), type) == 0) ? GS_GOOD : GS_BAD;
}

bool gsc_isLinearEntity(ads_name ent)
{
   AcDbEntity   *pEnt;
   AcDbObjectId objId;
   bool         result;

   if (!ent || ads_name_nil(ent)) return false;
   if (acdbGetObjectId(objId, ent) != Acad::eOk) return false;
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk) return false;
   result = gsc_isLinearEntity(pEnt);
   if (pEnt->close() != Acad::eOk) return false;

   return result;
}
bool gsc_isLinearEntity(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc()) ||
       pEnt->isKindOf(AcDb3dPolyline::desc()) ||
       pEnt->isKindOf(AcDbArc::desc()) ||
       pEnt->isKindOf(AcDbLeader::desc()) ||
       pEnt->isKindOf(AcDbLine::desc()) ||
       pEnt->isKindOf(AcDbMline::desc()) ||
       pEnt->isKindOf(AcDbPolyline::desc()) ||
       pEnt->isKindOf(AcDbSpline::desc()))
      return true;
   else
      return false;
}


bool gsc_isPunctualEntity(ads_name ent)
{
   AcDbEntity   *pEnt;
   AcDbObjectId objId;
   bool         result;

   if (!ent || ads_name_nil(ent)) return false;
   if (acdbGetObjectId(objId, ent) != Acad::eOk) return false;
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk) return false;
   result = gsc_isPunctualEntity(pEnt);
   if (pEnt->close() != Acad::eOk) return false;

   return result;
}
bool gsc_isPunctualEntity(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDbText::desc()) ||
       pEnt->isKindOf(AcDbMText::desc()) ||
       pEnt->isKindOf(AcDbAttribute::desc()) ||
       pEnt->isKindOf(AcDbAttributeDefinition::desc()) ||
       pEnt->isKindOf(AcDbBlockReference::desc()) ||
       pEnt->isKindOf(AcDbPoint::desc()))
      return true;
   else
      return false;
}


/*-------------------------------------------------------------
   int gsc_isline(ads_name ent)

      Ritorna GS_GOOD se ent e' di tipo LINE,
      altrimenti GS_BAD;
      
-------------------------------------------------------------*/
int gsc_isline(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("LINE"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*-------------------------------------------------------------
   int gsc_isblock(ads_name ent)

      Ritorna GS_GOOD se ent e' di tipo BLOCK,
      altrimenti GS_BAD;
      
-------------------------------------------------------------*/
int gsc_isblock(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("INSERT"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*-------------------------------------------------------------
   int gsc_isattDef(ads_name ent)

      Ritorna GS_GOOD se ent e' di tipo Definizione Attributo,
      altrimenti GS_BAD;
      
-------------------------------------------------------------*/
int gsc_isattDef(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("ATTDEF"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_isPoint                            <external> */
/*+                                                                       
  Verifica che l'oggetto grafico sia un punto.
  Parametri:
  ads_name ent;       Oggetto da verificare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_isPoint(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("POINT"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_isText                             <external> */
/*+                                                                       
  Verifica che l'oggetto grafico sia un testo o un testo multiplo.
  Parametri:
  ads_name ent;       Oggetto da verificare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_isText(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("TEXT"), type) == 0 || gsc_strcmp(_T("MTEXT"), type) == 0) ? GS_GOOD : GS_BAD;
}
bool gsc_isText(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDbText::desc()) ||
       pEnt->isKindOf(AcDbMText::desc()) ||
       pEnt->isKindOf(AcDbAttribute::desc()))
      return true;
   else
      return false;
}


/*********************************************************/
/*.doc gsc_ishatch                            <external> */
/*+                                                                       
  Parametri:
  ads_name ent;      	Oggetto grafico
  
  Restituisce GS_GOOD se entity è un riempimento altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_ishatch(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("HATCH"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_ismpolygon                         <external> */
/*+                                                                       
  Parametri:
  ads_name ent;      	Oggetto grafico
  
  Restituisce GS_GOOD se entity è un mpolygon altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_ismpolygon(ads_name ent)
{
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   return (gsc_strcmp(_T("MPOLYGON"), type) == 0) ? GS_GOOD : GS_BAD;
}


/*-------------------------------------------------------------
   int gsc_get_textstyle(ads_name ent, C_STRING &style, double *styleheight)     
-------------------------------------------------------------*/
int gsc_get_textstyle(ads_name ent, C_STRING &Style, double *StyleHeight)
{
   C_RB_LIST rb_list;
   presbuf   p;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(7)) == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   Style = p->resval.rstring;
   
   if ((rb_list << acdbTblSearch(_T("STYLE"), Style.get_name(), 0)) == NULL)
      { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }

   if ((p = rb_list.SearchType(40)) == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   *StyleHeight = p->resval.rreal;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getInfoText                        <external> */
/*+                                                                       
  La funzione ricava il nome dello stile di testo, l'altezza del testo,

  di un oggetto grafico.
  Parametri:
  ads_name ent;            entità autocad
  C_STRING *pText;         Opzionale; valore del testo (default = NULL)
  C_STRING *pStyleName;    Opzionale; nome dello stile di testo (default = NULL)
  double   *pHText;        Opzionale; altezza testo (default = NULL)
  int      *pHorizAlign;   Opzionale; flag di allineam. orizzontale (default = NULL)
  int      *pVertAlign;    Opzionale; flag di allineam. verticale (default = NULL)
  double   *pObliqueAng;   Opzionale; angolo obliquo in radianti (default = NULL)

  Restituisce GS_GOOD se esiste almeno un riempimento altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_getInfoText(ads_name ent, C_STRING *pText,
                    C_STRING *pStyleName, double *pHText, 
                    int *pHorizAlign, int *pVertAlign,
                    double *pObliqueAng, double *pWidthFactor)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_getInfoText((AcDbEntity *) pObj, pText, pStyleName, pHText,
                       pHorizAlign, pVertAlign, pObliqueAng, pWidthFactor) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_getInfoText(AcDbEntity *pEnt, C_STRING *pText,
                    C_STRING *pStyleName, double *pHText,
                    int *pHorizAlign, int *pVertAlign, 
                    double *pObliqueAng, double *pWidthFactor)
{
   if (pEnt->isKindOf(AcDbText::desc()) ||
       pEnt->isKindOf(AcDbAttribute::desc()) ||
       pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      if (pText)
         pText->set_name(((AcDbText *) pEnt)->textStringConst());
      if (pStyleName) 
         pStyleName->set_name(gsc_getSymbolName(((AcDbText *) pEnt)->textStyle()));
      if (pHText)
         *pHText = ((AcDbText *) pEnt)->height();
      if (pHorizAlign)
         *pHorizAlign = ((AcDbText *) pEnt)->horizontalMode();
      if (pVertAlign)
         *pVertAlign = ((AcDbText *) pEnt)->verticalMode();
      if (pObliqueAng)
         *pObliqueAng = ((AcDbText *) pEnt)->oblique();
      if (pWidthFactor)
         *pWidthFactor = ((AcDbText *) pEnt)->widthFactor();
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      if (pText)
      {
         pText->paste(((AcDbMText *) pEnt)->text()); // non uso contents (che ritorna caratteri di controllo)
         // rimuovo i caratteri per andare a capo e li sostituisco con uno spazio
         pText->strtran(_T("\r\n"), _T(" "));
      }
      if (pStyleName) 
         pStyleName->set_name(gsc_getSymbolName(((AcDbMText *) pEnt)->textStyle()));
      if (pHText)
         *pHText = ((AcDbMText *) pEnt)->textHeight();
      if (pHorizAlign)
         switch (((AcDbMText *) pEnt)->attachment())
         {
            case AcDbMText::kTopLeft:
            case AcDbMText::kMiddleLeft:
            case AcDbMText::kBottomLeft:
            case AcDbMText::kBaseLeft:
               *pHorizAlign = AcDb::kTextLeft;
               break;
            case AcDbMText::kTopCenter:
            case AcDbMText::kMiddleCenter:
            case AcDbMText::kBottomCenter:
            case AcDbMText::kBaseCenter:
               *pHorizAlign = AcDb::kTextCenter;
               break;
            case AcDbMText::kTopRight:
            case AcDbMText::kMiddleRight:
            case AcDbMText::kBottomRight:
            case AcDbMText::kBaseRight:
               *pHorizAlign = AcDb::kTextRight;
               break;
            case AcDbMText::kBaseAlign:
               *pHorizAlign = AcDb::kTextAlign;
               break;
            case AcDbMText::kBaseMid:
               *pHorizAlign = AcDb::kTextMid;
               break;
            case AcDbMText::kBaseFit:
               *pHorizAlign = AcDb::kTextFit;
               break;
            default:
               *pHorizAlign = AcDb::kTextLeft;
               break;
         }
          
      if (pVertAlign)
         switch (((AcDbMText *) pEnt)->attachment())
         {
            case AcDbMText::kTopLeft:
            case AcDbMText::kTopCenter:
            case AcDbMText::kTopRight:
               *pVertAlign = AcDb::kTextTop;
               break;
            case AcDbMText::kMiddleLeft:
            case AcDbMText::kMiddleCenter:
            case AcDbMText::kMiddleRight:
               *pVertAlign = AcDb::kTextVertMid;
               break;
            case AcDbMText::kBottomLeft:
            case AcDbMText::kBottomCenter:
            case AcDbMText::kBottomRight:
               *pVertAlign = AcDb::kTextBottom;
               break;
            case AcDbMText::kBaseAlign:
            case AcDbMText::kBaseMid:
            case AcDbMText::kBaseFit:
               *pVertAlign = AcDb::kTextBase;
               break;
            default:
               *pVertAlign = AcDb::kTextBase;
               break;
         }

      if (pObliqueAng) *pObliqueAng = 0; // non usato da mtext
      if (pWidthFactor) *pWidthFactor = 0; // non usato da mtext
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*-------------------------------------------------------------
   int gsc_get_elevation(ads_name ent, double *elev)     
-------------------------------------------------------------*/
int gsc_get_elevation(ads_name ent, double *elev)
{
   C_RB_LIST   rb_list;
   presbuf     p;
   ads_point   point;

	if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(0)) == NULL || p->resval.rstring == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

	if (gsc_strcmp(p->resval.rstring, _T("LWPOLYLINE")) == 0)
   {
		if (p = rb_list.SearchType(38))
			*elev=p->resval.rreal;
	}
	else
	{
		if ((p = rb_list.SearchType(10)) == NULL)
			*elev=0;
		else
		{
			ads_point_set(p->resval.rpoint, point);
			*elev=point[Z];
		}	
	}

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_elevation                      <external> */
/*+                                                                       
  Funzione che setta l'elevazione di un oggetto 
  grafico esistente.
  Parametri:
  ads_name ent;        Oggetto da analizzare
  double   elevation;  elevazione  
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_elevation(ads_name ent, double elevation)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_elevation((AcDbEntity *) pObj, elevation) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_set_elevation(AcDbEntity *pEnt, double elevation)
{
   Acad::ErrorStatus Res;

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      if (((AcDb2dPolyline *) pEnt)->elevation() != elevation)
      {
         Res = ((AcDb2dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDb2dPolyline *) pEnt)->setElevation(elevation);
      }
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc())) // 3D entity type
   {
      AcDbObjectIterator   *pVertIter;
      AcDbObjectId         vertexObjId;
      AcDb3dPolylineVertex *pVertex;
      AcGePoint3d          Pt;

      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();
      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         {
            Pt = pVertex->position();
            if (Pt.z != elevation)
            {
               Res = pVertex->upgradeOpen();

               if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
                  { pVertex->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
               Pt.z = elevation;
               pVertex->setPosition(Pt);
            }
            pVertex->close();
         }
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      if (((AcDbDimension *) pEnt)->elevation() != elevation)
      {
         Res = ((AcDbDimension *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbDimension *) pEnt)->setElevation(elevation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbHatch::desc()))
   {
      if (((AcDbHatch *) pEnt)->elevation() != elevation)
      {
         Res = ((AcDbHatch *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbHatch *) pEnt)->setElevation(elevation);
      }
   }
   else      
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      AcGePoint3d  Pt;

      Pt = ((AcDbArc *) pEnt)->center();
      if (Pt.z != elevation)
      {
         Res = ((AcDbArc *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbArc *) pEnt)->setCenter(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {  //  Group entity
      AcGePoint3d  Pt;

      Pt = ((AcDbAttribute *) pEnt)->position();
      if (Pt.z != elevation)
      {
         Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbAttribute *) pEnt)->setPosition(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {  //  Group entity
      AcGePoint3d  Pt;

      Pt = ((AcDbAttributeDefinition *) pEnt)->position();
      if (Pt.z != elevation)
      {
         Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbAttributeDefinition *) pEnt)->setPosition(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   { //  Group entity
      AcGePoint3d  Pt;

      Pt = ((AcDbBlockReference *) pEnt)->position();
      if (Pt.z != elevation)
      {
         Res = ((AcDbBlockReference *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbBlockReference *) pEnt)->setPosition(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {
      AcGePoint3d  Pt;

      Pt = ((AcDbCircle *) pEnt)->center();
      if (Pt.z != elevation)
      {
         Res = ((AcDbCircle *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbCircle *) pEnt)->setCenter(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      AcGePoint3d  Pt;

      Pt = ((AcDbDimension *) pEnt)->textPosition();
      if (Pt.z != elevation)
      {
         Res = ((AcDbDimension *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbDimension *) pEnt)->setTextPosition(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      AcGePoint3d  Pt;

      Pt = ((AcDbEllipse *) pEnt)->center();
      if (Pt.z != elevation)
      {
         Res = ((AcDbEllipse *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbEllipse *) pEnt)->setCenter(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      AcGePoint3d Pt;
      int numVerts = ((AcDbLeader *) pEnt)->numVertices();
      bool Upgraded = false;

      for (int i = 0; i < numVerts; i++)
      {
         Pt = ((AcDbLeader *) pEnt)->vertexAt(i);
         if (Pt.z != elevation)
         {
            if (!Upgraded)
            {
               Res = ((AcDbLeader *) pEnt)->upgradeOpen();

               if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
                  { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
               Upgraded = true;
            }

            Pt.z = elevation;
            ((AcDbLeader *) pEnt)->setVertexAt(i, Pt);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      AcGePoint3d Pt1, Pt2;

      Pt1 = ((AcDbLine *) pEnt)->startPoint();
      Pt2 = ((AcDbLine *) pEnt)->endPoint();
      if (Pt1.z != elevation || Pt2.z != elevation)
      {
         Res = ((AcDbLine *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt1.z = elevation;
         Pt2.z = elevation;
         ((AcDbLine *) pEnt)->setStartPoint(Pt1);
         ((AcDbLine *) pEnt)->setEndPoint(Pt2);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      AcGePoint3d Pt;

      Pt = ((AcDbMText *) pEnt)->location();
      if (Pt.z != elevation)
      {
         Res = ((AcDbMText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbMText *) pEnt)->setLocation(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
   {
      AcGePoint3d Pt;

      Pt = ((AcDbPoint *) pEnt)->position();
      if (Pt.z != elevation)
      {
         Res = ((AcDbPoint *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbPoint *) pEnt)->setPosition(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d Pt;
      int         NumVerts = ((AcDbMline *) pEnt)->numVertices();

      for (int i = 1; i < NumVerts; i++) // punti successivi
      {
         Pt = ((AcDbMline *) pEnt)->vertexAt(i);
         if (Pt.z != elevation)
         {
            Res = ((AcDbMline *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            Pt.z = elevation;
            ((AcDbMline *) pEnt)->moveVertexAt(1, Pt);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pEnt)->elevation() != elevation)
      {
         Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbPolyline *) pEnt)->setElevation(elevation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbRay::desc()))
   {
      AcGePoint3d Pt;

      Pt = ((AcDbRay *) pEnt)->basePoint();
      if (Pt.z != elevation)
      {
         Res = ((AcDbRay *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbRay *) pEnt)->setBasePoint(Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      AcGePoint3d Pt;

      if (((AcDbText *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbText *) pEnt)->verticalMode() == AcDb::kTextBase)
      {
         Pt = ((AcDbText *) pEnt)->position();
         if (Pt.z != elevation)
         {
            Res = ((AcDbText *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            Pt.z = elevation;
            ((AcDbText *) pEnt)->setPosition(Pt);
         }
      }
      else
      {
         Pt = ((AcDbText *) pEnt)->alignmentPoint();
         if (Pt.z != elevation)
         {
            Res = ((AcDbText *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            Pt.z = elevation;
            ((AcDbText *) pEnt)->setAlignmentPoint(Pt);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbXline::desc()))
   {
      AcGePoint3d Pt;

      Pt = ((AcDbXline *) pEnt)->basePoint();
      if (Pt.z != elevation)
      {
         Res = ((AcDbXline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         Pt.z = elevation;
         ((AcDbXline *) pEnt)->setBasePoint(Pt);
      }
   }

   return GS_GOOD;
}


/*-------------------------------------------------------------
   int gsc_get_ltScale(ads_name ent, double *scale)

      
-------------------------------------------------------------*/
int gsc_get_ltScale(ads_name ent, double *scale)
{
   C_RB_LIST rb_list;
   presbuf   p;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(48)) == NULL)
      *scale=1;
   else
      *scale=p->resval.rreal;

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_lineTypeScale(ads_name ent, double ltScale)
{
   AcDbObjectId objId;
   AcDbEntity   *pEnt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_lineTypeScale(pEnt, ltScale) == GS_BAD)
      { pEnt->close(); return GS_BAD; }
   pEnt->close();
   
   return GS_GOOD;
}
int gsc_set_lineTypeScale(AcDbEntity *pEnt, double ltScale)
{
   if (pEnt->linetypeScale() != ltScale)
   {
      Acad::ErrorStatus Res;

      Res = pEnt->upgradeOpen();
      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (pEnt->setLinetypeScale(ltScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidScale; return GS_BAD; }
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_thickness                       <external> */
/*+                                                                       
  Funzione che legge lo spessore verticale di un oggetto 
  grafico.
  Parametri:
  ads_name ent;         Oggetto da analizzare
  double   *thickness;  thickness  
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_thickness(ads_name ent, double *thickness)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_thickness((AcDbEntity *) pObj, thickness) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_thickness(AcDbEntity *pEnt, double *thickness)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
      *thickness = ((AcDb2dPolyline *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbArc::desc()))
      *thickness = ((AcDbArc *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
      *thickness = ((AcDbCircle *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
      *thickness = ((AcDbLine *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
      *thickness = ((AcDbPoint *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
      *thickness = ((AcDbPolyline *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbShape::desc()))
      *thickness = ((AcDbShape *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
      *thickness = ((AcDbSolid *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbText::desc()))
      *thickness = ((AcDbText *) pEnt)->thickness();
   else
   if (pEnt->isKindOf(AcDbTrace::desc()))
      *thickness = ((AcDbTrace *) pEnt)->thickness();
   else
      *thickness = 0;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_thickness                       <external> */
/*+                                                                       
  Funzione che setta la lo spessore verticale di un oggetto 
  grafico esistente.
  Parametri:
  ads_name ent;        Oggetto da analizzare
  double   thickness;  thickness  
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_thickness(ads_name ent, double thickness)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_thickness((AcDbEntity *) pObj, thickness) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_set_thickness(AcDbEntity *pEnt, double thickness)
{
   Acad::ErrorStatus Res;

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      if (((AcDb2dPolyline *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDb2dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDb2dPolyline *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc()))
   {
      if (((AcDbArc *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbArc *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbArc *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {
      if (((AcDbCircle *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbCircle *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbCircle *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      if (((AcDbLine *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbLine *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbLine *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
   {
      if (((AcDbPoint *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbPoint *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbPoint *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbPolyline *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbShape::desc()))
   {
      if (((AcDbShape *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbShape *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbShape *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
   {
      if (((AcDbSolid *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbSolid *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbSolid *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (((AcDbText *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbText *) pEnt)->setThickness(thickness);
      }
   }
   else
   if (pEnt->isKindOf(AcDbTrace::desc()))
   {
      if (((AcDbTrace *) pEnt)->thickness() != thickness)
      {
         Res = ((AcDbTrace *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbTrace *) pEnt)->setThickness(thickness);
      }
   }

   return GS_GOOD;
}


/*******************************************************/
/*.doc int gsc_get_blockName                           */
/*+                                                                   
  Funzione che legge il nome del blocco. 
  Parametri:
  ads_name ent;         entità grafica (input)
  C_STRING &BlockName;  Nome del blocco

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_blockName(ads_name ent, C_STRING &BlockName)
{
   C_RB_LIST Descr;
   presbuf   p;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((Descr << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   // Verifico che sia un blocco
   if ((p = Descr.SearchType(0)) == NULL || gsc_strcmp(p->resval.rstring, _T("INSERT")) != 0)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   if ((p = Descr.SearchType(2)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   BlockName = p->resval.rstring;

   return GS_GOOD;
}


/*-------------------------------------------------------------
   int gsc_get_prompt(ads_name ent, TCHAR **prompt)

      
-------------------------------------------------------------*/
int gsc_get_prompt(ads_name ent, TCHAR **prompt)
{
   C_RB_LIST rb_list;
   presbuf   p;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(3)) == NULL) 
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   *prompt = gsc_tostring(p->resval.rstring);

   return GS_GOOD;
}


/*-------------------------------------------------------------
   int gsc_get_center(ads_name ent)

      
-------------------------------------------------------------*/
int gsc_get_center(ads_name ent)
{
   C_RB_LIST   rb_list;
   presbuf     p;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(10)) != NULL)
      ads_point_set(p->resval.rpoint, BASE);

   if ((p = rb_list.SearchType(40)) != NULL)
      _SCALE =p->resval.rreal;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_appendLoopToMPolygon               <internal> */
/*+
  Questa funzione aggiunge un oggetto al poligono.
  Sono ammessi solo AcDbPolyline, AcDb2dPolyline o AcDbCircle.
  Parametri:
  AcDbMPolygon *pMPoly;    poligono a cui aggiungere l'oggetto
  AcDbEntity *pEnt;        oggetto da aggiungere
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_appendLoopToMPolygon(AcDbMPolygon *pMPoly, AcDbEntity *pEnt)
{
	AcDbPolyline   *pPoly   = NULL;
	AcDb2dPolyline *p2dPoly = NULL;
	AcDbCircle     *pCircle = NULL;
   AcDbSpline     *pSpline = NULL;
   Acad::ErrorStatus es;

	if ((pPoly = AcDbPolyline::cast(pEnt)) != NULL)
	{
      if (!pPoly->isClosed())
      {
         ads_point FirstPoint, LastPoint;

         if (gsc_get_firstPoint(pPoly, FirstPoint) != GS_GOOD || gsc_get_lastPoint(pPoly, LastPoint) != GS_GOOD ||
             !gsc_point_equal(FirstPoint, LastPoint))
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (gsc_close_pline(pPoly) != GS_GOOD) return GS_BAD;
      }
	   if ((es = pMPoly->appendLoopFromBoundary(pPoly)) != Acad::eOk) // if it is not a closed pline we get 'eInvalidInput'
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
	}
	else if ((p2dPoly = AcDb2dPolyline::cast(pEnt)) != NULL)
	{
      if (!p2dPoly->isClosed())
      {
         ads_point FirstPoint, LastPoint;

         if (gsc_get_firstPoint(p2dPoly, FirstPoint) != GS_GOOD || gsc_get_lastPoint(p2dPoly, LastPoint) != GS_GOOD ||
             !gsc_point_equal(FirstPoint, LastPoint))
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (gsc_close_pline(p2dPoly) != GS_GOOD) return GS_BAD;
      }
	   if ((es = pMPoly->appendLoopFromBoundary(p2dPoly)) != Acad::eOk) // if it is not a closed pline we get 'eInvalidInput'
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else if ((pCircle = AcDbCircle::cast(pEnt)) != NULL)
	{
		if ((es = pMPoly->appendLoopFromBoundary(pCircle)) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
	}
	else
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_create_MPolygon                    <internal> */
/*+
  Questa funzione crea un poligono.
  Parametri:
  AcDbEntityPtrArray &EntArray;
  oppure
  C_SELSET &SelSet;  gruppo di oggetti grafici che insieme formano il poligono

  Restituisce il puntatore al poligono in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
AcDbMPolygon *gsc_create_MPolygon(AcDbEntityPtrArray &EntArray)
{
   AcDbMPolygon *pMPolygon = new AcDbMPolygon();
   AcDbEntity   *pEnt;
   bool         appended = false;

   for (int i = 0; i < EntArray.length(); i++)
   {
      pEnt = EntArray.at(i);
      // Salto gli oggetti che non possono comporre l'MPolygon
      if (pEnt->isKindOf(AcDbPolyline::desc()) ||
          pEnt->isKindOf(AcDb2dPolyline::desc()) ||
          pEnt->isKindOf(AcDbCircle::desc()))
	      if (gsc_appendLoopToMPolygon(pMPolygon, pEnt) == GS_BAD)	// loop		
	         { delete pMPolygon; return NULL; }
         else
            appended = true;
      else
         if (pEnt->isKindOf(AcDbMPolygon::desc())) // se è un AcDbMPolygon lo copio e mi fermo
         {
            if (pMPolygon->copyFrom(pEnt) != Acad::eOk)
               { delete pMPolygon; return NULL; }
            return pMPolygon;
         }
   }

   if (appended == false || pMPolygon->balanceTree() != Acad::eOk) // Setta l'ordine dei vertici
      { delete pMPolygon; GS_ERR_COD = eGSInvGraphObjct; return NULL; }

   return pMPolygon;
}
AcDbMPolygon *gsc_create_MPolygon(C_SELSET &SelSet)
{
   AcDbEntityPtrArray EntArray;
   AcDbMPolygon       *pMPolygon;

   if (SelSet.get_AcDbEntityPtrArray(EntArray) == GS_BAD) return NULL;
   pMPolygon = gsc_create_MPolygon(EntArray);
   if (gsc_close_AcDbEntities(EntArray) == GS_BAD) return GS_BAD;

   return pMPolygon;
}


/*********************************************************/
/*.doc gsc_check_MPolygon_by_topology         <internal> */
/*+
  Questa funzione lisp verifica controlla che gli oggetti possano 
  essere considerati multi-poligono creando la topologia di map.
  Se la topologia non è valida vengono visualizzati dei simboli 
  sui punti non validi come impostazioni di default di MAP).
  Parametri:
  (<ss>)

  Ritorna gli oggetti non idonei.
-*/  
/*********************************************************/
int gs_check_mpolygon_by_topology(void)
{
   presbuf  arg = acedGetArgs();
   C_SELSET ss, entSS, res;
   ads_name ent;
   C_CLASS  *pCls;
   ade_id   var_id = ADE_NULLID;
   long     Tot, PrevLen, i = 0;
   C_EED    eed;

   acedRetNil();

   // Gruppo di selezione da esaminare 
   if (arg->restype != RTPICKS && arg->restype != RTENAME)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   ss << arg->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);

   Tot = ss.length();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1062)); // "Elaborazione entità"
   StatusBarProgressMeter.Init(Tot);

   resbuf rb;
   rb.rbnext  = NULL;
   rb.restype = RTSHORT;

   if ((var_id = tpm_varalloc()) == ADE_NULLID) return GS_BAD;

   // Flag for centroids generation (integer).
   // 1 = Generate
   // 0 = Do not generate
   rb.resval.rint = 0;
   tpm_varset(var_id, _T("CREATE_CNTR"), &rb);
   // Whether errors should be marked with persistent markers as drawn by tpm_cleanerrormark (integer).
   // 0 = Do not mark (default) 
   // 1 = Mark 
   rb.resval.rint = 1;
   tpm_varset(var_id, _T("CREATE_MARKERS"), &rb);
   // Whether errors should be marked with temporary markers as drawn by tpm_cleanerrordraw
   // 0 = Do not mark
   // 1 = Mark (default)
   rb.resval.rint = 0;
   tpm_varset(var_id, _T("CREATE_VIEW"), &rb);

   while (ss.entname(0, ent) == GS_GOOD)
   {  
      entSS.clear();

      // Se è un oggetto grafico di GEOsim con database associato
      if (eed.load(ent) == GS_GOOD && GS_CURRENT_WRK_SESSION &&
          (pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) &&
          pCls->ptr_info())
         pCls->get_SelSet(ent, entSS);
      else
         entSS.add(ent);

      PrevLen = ss.length();
      ss.subtract(entSS);
      i = i + (PrevLen - ss.length());
      
      entSS.intersectType(GRAPHICAL);
      if (entSS.length() > 0)
      {
         entSS.get_selection(ent);
		   // creo la topologia di tipo poligono (il nome non deve essere più lungo di 17 car)
         // 3 = poligono
		   if (tpm_mntbuild(var_id, _T("gs_check_mPolygon"), GS_EMPTYSTR, 3, 
                          NULL, ent, NULL) == RTNORM)
         {
            if (tpm_mnterase(_T("gs_check_mPolygon")) != RTNORM)
               break;
         }
         else
            res.add_selset(entSS);
      }

      StatusBarProgressMeter.Set(i);
   }

   tpm_varfree(var_id);

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   acutPrintf(gsc_msg(227), Tot,  res.length()); // "\nOggetti grafici elaborati %ld, scartati %ld."
   
   if (res.length() == 0) return RTNORM;
   res.get_selection(ent);
   acedRetName(ent, RTPICKS);
   res.ReleaseAllAtDistruction(GS_BAD);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_centroidpoint                  <internal> */
/*+
  Questa funzione ricava il centroide di un oggetto.
  Parametri:
  AcDbEntity *pEnt;  polilinea chiusa
  ads_point point;   centroide
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_centroidpoint(AcDbEntity *pEnt, ads_point point)
{
   ads_name ent;
   int      Result = GS_BAD;

   if (pEnt->objectId().isNull())
   {
      AcDbBlockTable       *pBlockTable;
      AcDbBlockTableRecord *pBlockTableRecord;
      AcDbEntity           *pTempEnt;

      // Clono e creo l'entità temporaneamente (poi la cancello)
      if ((pTempEnt = (AcDbEntity *) pEnt->clone()) == NULL) return GS_BAD;
      // Open the block table for read.
      if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                           AcDb::kForRead) != Acad::eOk)
         { delete pTempEnt; return GS_BAD; }
      if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
         { delete pTempEnt; pBlockTable->close(); return GS_BAD; }
      pBlockTable->close();
      if (pBlockTableRecord->appendAcDbEntity(pTempEnt) != Acad::eOk)
         { delete pTempEnt; pBlockTableRecord->close(); return GS_BAD; }
      pTempEnt->close();
      pBlockTableRecord->close();

      if (acdbGetAdsName(ent, pTempEnt->objectId()) == Acad::eOk)
         Result = gsc_get_centroidpoint(ent, point);

      gsc_EraseEnt(ent);
   }
   else
   {
      if (acdbGetAdsName(ent, pEnt->objectId()) != Acad::eOk) return GS_BAD;
      Result = gsc_get_centroidpoint(ent, point);
   }

   return Result;
}


/*********************************************************/
/*.doc gsc_get_centroidpoint                  <internal> */
/*+
  Questa funzione ricava il centroide di un oggetto.
  Parametri:
  ads_name ent;      polilinea chiusa
  ads_point point;   centroide
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_centroidpoint(ads_name ent, ads_point point)
{
   C_SELSET ss;

   ss.add(ent);

   return gsc_get_centroidpoint(ss, point);
}


/*********************************************************/
/*.doc gsc_get_centroidpoint                  <internal> */
/*+
  Questa funzione ricava il centroide di un oggetto.
  Parametri:
  C_SELSET &ss;      gruppo di selezione di una superficie che può
                     essere composta da più polilinee chiuse (isole)
  ads_point point;   centroide
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_centroidpoint(C_SELSET &ss, ads_point point)
{
   AcDbMPolygon      *pMPoly;
   AcDbObjectIdArray MPolygonIds, Centroids;
   long              i = 0;
   AcDbObjectId      objId;
   AcDbObject        *pObj;
   AcDbBlockTable       *pBlockTable;
   AcDbBlockTableRecord *pInternalBlockTableRecord;

   // Creo un MPOLYGON
   if ((pMPoly = gsc_create_MPolygon(ss)) == NULL) return GS_BAD;

   // Open the block table for read.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();
   pInternalBlockTableRecord->appendAcDbEntity(objId, pMPoly);
   pMPoly->close();
   pInternalBlockTableRecord->close();

   // creo un vettore di ObjectId
   MPolygonIds.append(objId);

	if (AcMapUtilities::CreateCentroids(Centroids, MPolygonIds, 
		                                 _T("0"), _T("ACAD_POINT")) != AcMap::kOk)
      { GS_ERR_COD = eGSInvalidGraphCalc; return GS_BAD; }
   
   if (acdbOpenObject(pObj, Centroids.first(), AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_firstPoint((AcDbEntity *) pObj, point) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();
   
   // Cancello il poligono
   gsc_EraseEnt(objId);
   // Cancello il Centroide
   return gsc_EraseEnt(Centroids);
}


/*********************************************************/
/*.doc gsc_get_firstPoint                     <internal> */
/*+
  Ritorna nel paramentro point le coordinate del punto iniziale 
  dell'entita' specificata da ent se questa e' una polilinea.
  Ritorna nel paramentro point le coordinate del punto di 
  inserimento dell'entita' specificata da ent altrimenti.
  Parametri:
  AcDbEntity *pEnt;  entità da esaminare
  ads_point point;   punto
  
  oppure

  ads_name ent;      entità da esaminare
  ads_point point;   punto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_firstPoint(AcDbEntity *pEnt, ads_point point)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        Position;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      pVertIter = ((AcDb2dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex) // punto di controllo
         {
            Position = pVertex->position();
            if (ToClose) pVertex->close();
            delete pVertIter;
            ads_point_set_from_AcGePoint3d(Position, point);
         
            return GS_GOOD;
         }
         
         if (ToClose) pVertex->close();
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose;
      
      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();

      if (pVertIter->done()) { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      // Leggo vertice
      if ((pVertexEnt = pVertIter->entity()) != NULL)
      {
         ToClose = FALSE;
         pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
      }
      else
      {
         ToClose = TRUE;
         vertexObjId = pVertIter->objectId();
         if (((AcDb3dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
     
      Position = pVertex->position();
      if (ToClose) pVertex->close();
      
      delete pVertIter;
      ads_point_set_from_AcGePoint3d(Position, point);
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point pt;

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), pt);
      acutPolar(pt, ((AcDbArc *) pEnt)->startAngle(), ((AcDbArc *) pEnt)->radius(), point);
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   { //  Group entity
      if (((AcDbAttribute *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbAttribute *) pEnt)->verticalMode() == AcDb::kTextBase)
         ads_point_set_from_AcGePoint3d(((AcDbAttribute *) pEnt)->position(), point);
      else
         ads_point_set_from_AcGePoint3d(((AcDbAttribute *) pEnt)->alignmentPoint(), point);
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   { //  Group entity
      if (((AcDbAttributeDefinition *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbAttributeDefinition *) pEnt)->verticalMode() == AcDb::kTextBase)
         ads_point_set_from_AcGePoint3d(((AcDbAttributeDefinition *) pEnt)->position(), point);
      else
         ads_point_set_from_AcGePoint3d(((AcDbAttributeDefinition *) pEnt)->alignmentPoint(), point);
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
      //  Group entity
      ads_point_set_from_AcGePoint3d(((AcDbBlockReference *) pEnt)->position(), point);
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbCircle *) pEnt)->center(), point);
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbDimension *) pEnt)->textPosition(), point);
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbEllipse *) pEnt)->center(), point);
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbLeader *) pEnt)->firstVertex(), point);
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbLine *) pEnt)->startPoint(), point);
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbMText *) pEnt)->location(), point);
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbPoint *) pEnt)->position(), point);
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d pt;

      ((AcDbPolyline *) pEnt)->getPointAt(0, pt);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt;

      pt = ((AcDbMline *) pEnt)->vertexAt(0);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbRay::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbRay *) pEnt)->basePoint(), point);
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
   {
      AcGePoint3d pt;

      ((AcDbSolid *) pEnt)->getPointAt(0, pt);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbFace::desc()))
   {
      AcGePoint3d pt;

      ((AcDbFace *) pEnt)->getVertexAt(0, pt);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePoint3d pt;

      if (((AcDbSpline *) pEnt)->hasFitData() == Adesk::kTrue)
         ((AcDbSpline *) pEnt)->getFitPointAt(0, pt);
      else
         ((AcDbSpline *) pEnt)->getControlPointAt(0, pt);

      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (((AcDbText *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbText *) pEnt)->verticalMode() == AcDb::kTextBase)
         ads_point_set_from_AcGePoint3d(((AcDbText *) pEnt)->position(), point);
      else
         ads_point_set_from_AcGePoint3d(((AcDbText *) pEnt)->alignmentPoint(), point);
   }
   else
   if (pEnt->isKindOf(AcDbXline::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbXline *) pEnt)->basePoint(), point);
   else // ricavo le estensioni
   {
      AcDbExtents Extent;

      if (pEnt->getGeomExtents(Extent) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      ads_point_set_from_AcGePoint3d(Extent.minPoint(), point);
   }

   return GS_GOOD;
}
int gsc_get_firstPoint(ads_name ent, ads_point point)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_firstPoint((AcDbEntity *) pObj, point) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_firstPoint                     <external> */
/*+
  Funzione di ausilio alla omonima gsc_set_firstPoint(ads_name ent, 
  ads_point point, ads_real *rot, int check_rotz).
  Questa funzione cambia il punto di inserimento di un oggetto nodale 
  o il primo punto di oggetti lineari.
  Parametri:
  AcDbEntity *pEnt;     entità
  ads_point point;      nuovo punto di inserimento
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_set_firstPoint(AcDbEntity *pEnt, ads_point point)
{
   AcGePoint3d Position(point[X], point[Y], point[Z]);

   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      pVertIter = ((AcDb2dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex) // punto di controllo
         {
            Acad::ErrorStatus Res;

            if (Position == pVertex->position()) Res = Acad::eOk;
            else
            {
               Res = pVertex->upgradeOpen();

               if (Res == Acad::eOk) pVertex->setPosition(Position);
               else GS_ERR_COD = eGSInvGraphObjct; 
            }

            if (ToClose) pVertex->close();
            delete pVertIter; 

            return (Res == Acad::eOk) ? GS_GOOD : GS_BAD;
         }
         
         if (ToClose) pVertex->close();
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose;
      
      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();

      if (pVertIter->done()) { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      // Leggo vertice
      if ((pVertexEnt = pVertIter->entity()) != NULL)
      {
         ToClose = FALSE;
         pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
      }
      else
      {
         ToClose = TRUE;
         vertexObjId = pVertIter->objectId();
         if (((AcDb3dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }

      Acad::ErrorStatus Res;

      if (Position == pVertex->position()) Res = Acad::eOk;
      else
      {
         Res = pVertex->upgradeOpen();

         if (Res == Acad::eOk) pVertex->setPosition(Position);
         else GS_ERR_COD = eGSInvGraphObjct; 
      }

      if (ToClose) pVertex->close();     
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point pt;
      double    NewStartAngle, NewRadius;

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), pt);
      NewStartAngle = acutAngle(pt, point);
      NewRadius     = gsc_dist(pt, point);

      if (NewStartAngle != ((AcDbArc *) pEnt)->startAngle() ||
          NewRadius != ((AcDbArc *) pEnt)->radius())
      {
         Acad::ErrorStatus Res = ((AcDbArc *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         ((AcDbArc *) pEnt)->setStartAngle(NewStartAngle);
         ((AcDbArc *) pEnt)->setRadius(NewRadius);
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   { //  Group entity
      Acad::ErrorStatus Res;

      if (((AcDbAttribute *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbAttribute *) pEnt)->verticalMode() == AcDb::kTextBase)
      {
         if (Position != ((AcDbAttribute *) pEnt)->position())
         {
            Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbAttribute *) pEnt)->setPosition(Position);
         }
      }
      else
      {
         if (Position != ((AcDbAttribute *) pEnt)->alignmentPoint())
         {
            Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbAttribute *) pEnt)->setAlignmentPoint(Position);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   { //  Group entity
      Acad::ErrorStatus Res;

      if (((AcDbAttributeDefinition *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbAttributeDefinition *) pEnt)->verticalMode() == AcDb::kTextBase)
      {
         if (Position != ((AcDbAttributeDefinition *) pEnt)->position())
         {
            Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbAttributeDefinition *) pEnt)->setPosition(Position);
         }
      }
      else
      {
         if (Position != ((AcDbAttributeDefinition *) pEnt)->alignmentPoint())
         {
            Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbAttributeDefinition *) pEnt)->setAlignmentPoint(Position);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   { //  Group entity
      if (Position != ((AcDbBlockReference *) pEnt)->position())
      {
         Acad::ErrorStatus Res = ((AcDbBlockReference *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         
         ads_point PrevPoint;
         ads_point_set_from_AcGePoint3d(((AcDbBlockReference *) pEnt)->position(), PrevPoint);

         ((AcDbBlockReference *) pEnt)->setPosition(Position);

         // Riposiziono gli eventuali attributi del blocco
         ads_point          AttrPIns;
         ads_real           OffSetX, OffSetY, OffSetZ;
         AcDbObjectId       objId;
         AcDbObjectIterator *pAttrIter;
   	   AcDbAttribute      *pAttrib;

         OffSetX = point[X] - PrevPoint[X];
         OffSetY = point[Y] - PrevPoint[Y];
         OffSetZ = point[Z] - PrevPoint[Z];

         // Ciclo sugli attributi
         pAttrIter = ((AcDbBlockReference *) pEnt)->attributeIterator();
	      for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
         {
	         objId = pAttrIter->objectId();

            if (((AcDbBlockReference *) pEnt)->openAttribute(pAttrib, objId, AcDb::kForWrite) == Acad::eOk)
            {
               // leggo punto di inserimento
               gsc_get_firstPoint(pAttrib, AttrPIns);

               // Calcolo nuovo punto di inserimento
               AttrPIns[X] += OffSetX;
               AttrPIns[Y] += OffSetY;
               AttrPIns[Z] += OffSetZ;

               if (gsc_set_firstPoint(pAttrib, AttrPIns) == GS_BAD)
                  { pAttrib->close(); return GS_BAD; }

               pAttrib->close();
            }
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {
      if (Position != ((AcDbCircle *) pEnt)->center())
      {
         Acad::ErrorStatus Res = ((AcDbCircle *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbCircle *) pEnt)->setCenter(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      if (Position != ((AcDbDimension *) pEnt)->textPosition())
      {
         Acad::ErrorStatus Res = ((AcDbDimension *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbDimension *) pEnt)->setTextPosition(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      if (Position != ((AcDbEllipse *) pEnt)->center())
      {
         Acad::ErrorStatus Res = ((AcDbEllipse *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbEllipse *) pEnt)->setCenter(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      if (Position != ((AcDbLeader *) pEnt)->firstVertex())
      {
         Acad::ErrorStatus Res = ((AcDbLeader *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbLeader *) pEnt)->setVertexAt(0, Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      if (Position != ((AcDbLine *) pEnt)->startPoint())
      {
         Acad::ErrorStatus Res = ((AcDbLine *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbLine *) pEnt)->setStartPoint(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      if (Position != ((AcDbMText *) pEnt)->location())
      {
         Acad::ErrorStatus Res = ((AcDbMText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbMText *) pEnt)->setLocation(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
   {
      if (Position != ((AcDbPoint *) pEnt)->position())
      {
         Acad::ErrorStatus Res = ((AcDbPoint *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbPoint *) pEnt)->setPosition(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d pt;

      ((AcDbPolyline *) pEnt)->getPointAt(0, pt);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else
         {
            AcGePoint2d pt2D(Position.x, Position.y);
            ((AcDbPolyline *) pEnt)->setPointAt(0, pt2D);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt;

      pt = ((AcDbMline *) pEnt)->vertexAt(0);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbMline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         ((AcDbMline *) pEnt)->moveVertexAt(0, pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbRay::desc()))
   {
      if (Position != ((AcDbRay *) pEnt)->basePoint())
      {
         Acad::ErrorStatus Res = ((AcDbRay *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbRay *) pEnt)->setBasePoint(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
   {
      AcGePoint3d pt;

      ((AcDbSolid *) pEnt)->getPointAt(0, pt);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbSolid *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbSolid *) pEnt)->setPointAt(0, Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbFace::desc()))
   {
      AcGePoint3d pt;

      ((AcDbFace *) pEnt)->getVertexAt(0, pt);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbFace *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbFace *) pEnt)->setVertexAt(0, Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePoint3d pt;

      if (((AcDbSpline *) pEnt)->hasFitData() == Adesk::kTrue)
      {
         ((AcDbSpline *) pEnt)->getFitPointAt(0, pt);
         if (Position != pt)
         {
            Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbSpline *) pEnt)->setFitPointAt(0, Position);
         }
      }
      else
      {
         ((AcDbSpline *) pEnt)->getControlPointAt(0, pt);
         if (Position != pt)
         {
            Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbSpline *) pEnt)->setControlPointAt(0, Position);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (((AcDbText *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbText *) pEnt)->verticalMode() == AcDb::kTextBase)
      {
         if (Position != ((AcDbText *) pEnt)->position())
         {
            Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbText *) pEnt)->setPosition(Position);
         }
      }
      else
      {
         if (Position != ((AcDbText *) pEnt)->alignmentPoint())
         {
            Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbText *) pEnt)->setAlignmentPoint(Position);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbXline::desc()))
   {
      if (Position != ((AcDbXline *) pEnt)->basePoint())
      {
         Acad::ErrorStatus Res = ((AcDbXline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbXline *) pEnt)->setBasePoint(Position);
      }
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_firstPoint                     <external> */
/*+
  Questa funzione cambia il punto di inserimento e opzionalmente
  la rotazione di un oggetto nodale o il primo punto di oggetti lineari.
  Parametri:
  ads_name ent;         entità
  ads_point point;      nuovo punto di inserimento
  ads_real *rot;        Nuova rotazione (in radianti; default = NULL)
  int       check_rotz; se GS_GOOD il testo non sarà mai capovolto (default = GS_GOOD)
                        applicato a testi e agli attributi di un blocco
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_set_firstPoint(ads_name ent, ads_point point, ads_real *rot, int check_rotz)
{
   AcDbObjectId objId;
   AcDbEntity *pEnt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // Setto il nuovo primo punto
   if (gsc_set_firstPoint(pEnt, point) == GS_BAD) { pEnt->close(); return GS_BAD; }
   // se specificata setto la rotazione (dipende dal tipo di oggetto)
   if (rot) gsc_set_rotation(pEnt, *rot, check_rotz);

   pEnt->close();
   if (pEnt->drawable()) pEnt->draw();

   return GS_GOOD;
}


/*+
-------------------------------------------------------------
   int gsc_get_numvertex(ads_name ent)

     Ritorna il numero di vertici di una polilinea (ent).
     Ritorna 0 in caso di errore o entita' sbagliata.
      
-------------------------------------------------------------
-*/
int gsc_get_numvertex(ads_name ent)
{
   int       cont = 0;
   presbuf   p_rb;
   C_RB_LIST descr;

   if ((descr << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return 0; }

   if ((p_rb = descr.SearchType(0)) == NULL || p_rb->resval.rstring == NULL) 
      { GS_ERR_COD = eGSInvRBType; return 0; }

   if (gsc_strcmp(p_rb->resval.rstring, _T("POLYLINE")) == 0)
   {
      ads_name ndx;
   
      ads_name_set(ent, ndx);
      while (1)
      {
         if (ads_entnext(ndx, ndx) == RTERROR)
            { GS_ERR_COD = eGSInvEntityOp; return 0; }

         if ((descr << acdbEntGet(ndx)) == NULL)
            { GS_ERR_COD = eGSInvEntityOp; return 0; }

         if ((p_rb = descr.SearchType(0)) == NULL || p_rb->resval.rstring == NULL) 
            { GS_ERR_COD = eGSInvRBType; return 0; }
      
         if (gsc_strcmp(p_rb->resval.rstring, _T("SEQEND")) == 0) break;

         cont++;
      }
   }
   else if (gsc_strcmp(p_rb->resval.rstring, _T("LWPOLYLINE")) == 0)
   {
      p_rb = descr.SearchType(10);
      while (p_rb)
      {
         cont++;
         // prossimo vertice
         p_rb = p_rb->rbnext;
         while (p_rb && p_rb->restype != 10) p_rb = p_rb->rbnext;
      }
   }
   
   return cont;
}


/*********************************************************/
/*.doc gsc_get_lastPoint                      <internal> */
/*+
  Ritorna nel paramentro point le coordinate del punto finale 
  dell'entita' specificata da ent.
  Se non si tratta di polilinea ritorna il punto di inserimento. 
  Parametri:
  AcDbEntity *pEnt;  entità da esaminare
  ads_point point;   punto
  
  oppure

  ads_name ent;      entità da esaminare
  ads_point point;   punto
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_lastPoint(AcDbEntity *pEnt, ads_point point)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        Position;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      pVertIter = ((AcDb2dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         Position = pVertex->position();
         if (ToClose) pVertex->close();
      }
      delete pVertIter;

      ads_point_set_from_AcGePoint3d(Position, point);
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose;
      
      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb3dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }
     
         Position = pVertex->position();
         if (ToClose) pVertex->close();
      }

      delete pVertIter;
      ads_point_set_from_AcGePoint3d(Position, point);
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point pt;

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), pt);
      acutPolar(pt, ((AcDbArc *) pEnt)->endAngle(), ((AcDbArc *) pEnt)->radius(), point);
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbLeader *) pEnt)->lastVertex(), point);
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
      ads_point_set_from_AcGePoint3d(((AcDbLine *) pEnt)->endPoint(), point);
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d pt;

      ((AcDbPolyline *) pEnt)->getPointAt(((AcDbPolyline *) pEnt)->numVerts() - 1, pt);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt;

      pt = ((AcDbMline *) pEnt)->vertexAt(((AcDbMline *) pEnt)->numVertices() - 1);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePoint3d pt;

      ((AcDbSpline *) pEnt)->getEndPoint(pt);
      ads_point_set_from_AcGePoint3d(pt, point);
   }
   else
      return gsc_get_firstPoint(pEnt, point);

   return GS_GOOD;
}
int gsc_get_lastPoint(ads_name ent, ads_point point)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_lastPoint((AcDbEntity *) pObj, point) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_lastpoint                      <internal> */
/*+
  Setta le coordinate del punto finale dell'entita' specificata da ent 
  se questa e' una polilinea.
  Se non si tratta di polilinea setta il punto di inserimento. 
  Parametri:
  ads_name  ent;    oggetto grafico polilinea
  ads_point point;  punto finale
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_lastPoint(ads_name ent, ads_point point)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_lastPoint((AcDbEntity *) pObj, point) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_set_lastPoint(AcDbEntity *pEnt, ads_point point)
{
   AcGePoint3d Position(point[X], point[Y], point[Z]);

   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        Position;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      pVertIter = ((AcDb2dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex) // punto di controllo
         {
            Acad::ErrorStatus Res;

            if (Position == pVertex->position()) Res = Acad::eOk;
            else
            {
               Res = pVertex->upgradeOpen();

               if (Res == Acad::eOk) pVertex->setPosition(Position);
               else GS_ERR_COD = eGSInvGraphObjct; 
            }

            if (ToClose) pVertex->close();
            delete pVertIter; 

            return (Res == Acad::eOk) ? GS_GOOD : GS_BAD;
         }

         if (ToClose) pVertex->close();
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose = FALSE;
      
      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         if (ToClose) pVertex->close();

         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb3dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }
      }

      Acad::ErrorStatus Res;

      if (Position == pVertex->position()) Res = Acad::eOk;
      else
      {
         Res = pVertex->upgradeOpen();

         if (Res == Acad::eOk) pVertex->setPosition(Position);
         else GS_ERR_COD = eGSInvGraphObjct; 
      }

      if (ToClose) pVertex->close();     

      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point pt;
      double    NewEndAngle, NewRadius;

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), pt);
      NewEndAngle = acutAngle(pt, point);
      NewRadius   = gsc_dist(pt, point);

      if (NewEndAngle != ((AcDbArc *) pEnt)->endAngle() ||
          NewRadius != ((AcDbArc *) pEnt)->radius())
      {
         Acad::ErrorStatus Res = ((AcDbArc *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         ((AcDbArc *) pEnt)->setEndAngle(NewEndAngle);
         ((AcDbArc *) pEnt)->setRadius(NewRadius);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      if (Position != ((AcDbLeader *) pEnt)->lastVertex())
      {
         Acad::ErrorStatus Res = ((AcDbLeader *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbLeader *) pEnt)->setVertexAt(((AcDbLeader *) pEnt)->numVertices() - 1, Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      if (Position != ((AcDbLine *) pEnt)->endPoint())
      {
         Acad::ErrorStatus Res = ((AcDbLine *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else ((AcDbLine *) pEnt)->setEndPoint(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint3d pt;

      ((AcDbPolyline *) pEnt)->getPointAt(((AcDbPolyline *) pEnt)->numVerts() - 1, pt);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         else
         {
            AcGePoint2d pt2D(Position.x, Position.y);
            ((AcDbPolyline *) pEnt)->setPointAt(((AcDbPolyline *) pEnt)->numVerts() - 1, pt2D);
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt;

      pt = ((AcDbMline *) pEnt)->vertexAt(((AcDbMline *) pEnt)->numVertices() - 1);
      if (Position != pt)
      {
         Acad::ErrorStatus Res = ((AcDbMline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbMline *) pEnt)->moveVertexAt(((AcDbMline *) pEnt)->numVertices() - 1, pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePoint3d pt;

      if (((AcDbSpline *) pEnt)->hasFitData() == Adesk::kTrue)
      {
         ((AcDbSpline *) pEnt)->getFitPointAt(((AcDbSpline *) pEnt)->numFitPoints() - 1, pt);
         if (Position != pt)
         {
            Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbSpline *) pEnt)->setFitPointAt(((AcDbSpline *) pEnt)->numFitPoints() - 1, Position);
         }
      }
      else
      {
         ((AcDbSpline *) pEnt)->getControlPointAt(((AcDbSpline *) pEnt)->numControlPoints() - 1, pt);
         if (Position != pt)
         {
            Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

            if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            else ((AcDbSpline *) pEnt)->setControlPointAt(((AcDbSpline *) pEnt)->numControlPoints() - 1, Position);
         }
      }
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_addvertex                          <internal> */
/*+
  Questa funzione aggiunge un vertice alla fine della polilinea.
  Parametri:
  ads_name  ent;    oggetto grafico polilinea
  ads_point point;  punto da aggiungere
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_addvertex(AcDbEntity *pEnt, ads_point point)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      AcDb2dPolyline *pLine = (AcDb2dPolyline *) pEnt;
      AcGePoint3d dummyPt(point[X], point[Y], point[Z]);            
      AcDb2dVertex* pt = new AcDb2dVertex(dummyPt);

      if (pLine->appendVertex(pt) != Acad::eOk)
         { delete pt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }


      pt->close();
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      AcDb3dPolyline *pLine = (AcDb3dPolyline *) pEnt;
      AcGePoint3d dummyPt(point[X], point[Y], point[Z]);            
      AcDb3dPolylineVertex* pt = new AcDb3dPolylineVertex(dummyPt);

      if (pLine->appendVertex(pt) != Acad::eOk)
         { delete pt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      pt->close();
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcDbPolyline *pLine = (AcDbPolyline *) pEnt;
      AcGePoint2d  pt(point[X], point[Y]);

      if (pLine->addVertexAt(pLine->numVerts(), pt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;   
}
int gsc_addvertex(ads_name ent, ads_point point)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_addvertex((AcDbEntity *) pObj, point) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_delLastVertex                      <internal> */
/*+
  Questa funzione cancella l'ultimo vertice della polilinea.
  Parametri:
  ads_name  ent;    oggetto grafico polilinea
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_delLastVertex(AcDbEntity *pEnt)
{
   Acad::ErrorStatus Res;

   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      AcDb2dPolyline *pLine = (AcDb2dPolyline *) pEnt;
      AcDbObjectIterator *pVertIter = pLine->vertexIterator();
      AcDb2dVertex       *pVertex;
      AcDbEntity         *pVertexEnt;
      AcDbObjectId       vertexObjId;
      int                ToClose, count = 0;

      // conto i vertici
      for (count; !pVertIter->done(); pVertIter->step()) count++;

      if (count <= 2) return GS_BAD;

      pVertIter->start(true); // ultimo vertice

      // Leggo vertice
      if ((pVertexEnt = pVertIter->entity()) != NULL)
      {
         ToClose = FALSE;
         pVertex = (AcDb2dVertex *) pVertexEnt;
      }
      else
      {
         ToClose = TRUE;
         vertexObjId = pVertIter->objectId();
         if ((Res = pLine->openVertex(pVertex, vertexObjId, AcDb::kForWrite)) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }

      if (!pVertex->isErased())
         if ((Res = pVertex->erase()) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      if (ToClose) pVertex->close();
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      AcDb3dPolyline       *pLine = (AcDb3dPolyline *) pEnt;
      AcDbObjectIterator   *pVertIter = pLine->vertexIterator();
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcDbEntity           *pVertexEnt;
      int                  ToClose, count = 0;

      // conto i vertici
      for (count; !pVertIter->done(); pVertIter->step()) count++;

      if (count <= 2) return GS_BAD;

      pVertIter->start(true); // ultimo vertice

      // Leggo vertice
      if ((pVertexEnt = pVertIter->entity()) != NULL)
      {
         ToClose = FALSE;
         pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
      }
      else
      {
         ToClose = TRUE;
         vertexObjId = pVertIter->objectId();
         if ((Res = pLine->openVertex(pVertex, vertexObjId, AcDb::kForWrite)) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }

      if (!pVertex->isErased())
         if ((Res = pVertex->erase()) != Acad::eOk)
            { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      if (ToClose) pVertex->close();
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcDbPolyline *pLine = (AcDbPolyline *) pEnt;

      Res = pLine->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      if (pLine->removeVertexAt(pLine->numVerts() - 1) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;   
}
int gsc_delLastVertex(ads_name ent)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_delLastVertex((AcDbEntity *) pObj) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_join_point                          <internal> */
/*+
  Questa funzione crea una polilinea passante tra due punti.
  Parametri:
  ads_point first;   primo punto
  ads_point second;  secondo punto
  ads_name ent_out;  polilinea risultante
  C_CLASS   *pCls;   puntatore a classe per definizione fas (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_join_point(ads_point first, ads_point second, ads_name ent_out,
                   C_CLASS *pCls)
{
   C_FAS    *fas, OldGraphEnv;
   C_ID     *id;
   int      ret = GS_GOOD;
   ads_name ent;
   C_EED    tmp_EED;

   if (ent_out) ads_name_clear(ent_out);

   if (pCls) // Effettua settaggi FAS di default
   {
      id = pCls->ptr_id();
      if ((fas = pCls->ptr_fas()) == NULL)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
      if (gsc_insert_pline(first, second, fas->layer, &fas->color, fas->line, fas->line_scale, fas->width) == GS_BAD)
         return GS_BAD;
   }
   else
      if (gsc_insert_pline(first, second) == GS_BAD)
         return GS_BAD;

   if (acdbEntLast(ent) != RTNORM) 
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (ent_out != NULL)
      ads_name_set(ent, ent_out);

   return GS_GOOD;   
}


/*+
-------------------------------------------------------------
int gsc_open_pline(ads_name ent)

     Apre una polilinea polilinea (ent) eventualmente chiusa.
     Ritorna GS_BAD in caso di errore o entita' sbagliata.
     GS_GOOD altrimenti.
      
-------------------------------------------------------------
-*/
int gsc_open_pline(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      if (((AcDb2dPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDb2dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDb2dPolyline *) pEnt)->makeOpen() != Acad::eOk) 
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      if (((AcDb3dPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDb3dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDb3dPolyline *) pEnt)->makeOpen() != Acad::eOk) 
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbPolyline *) pEnt)->setClosed(false);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      if (((AcDbSpline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         int              degree; 
         Adesk::Boolean   rational, closed, periodic;
         AcGePoint3dArray controlPoints;
         AcGeDoubleArray  knots;
         AcGeDoubleArray  weights;
         double           controlPtTol, knotTol;
         
         if (((AcDbSpline *) pEnt)->getNurbsData(degree, rational, closed, periodic,                 
                                                 controlPoints, knots, weights,
                                                 controlPtTol, knotTol) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         closed = Adesk::kFalse;
         if (((AcDbSpline *) pEnt)->setNurbsData(degree, rational, closed, periodic,
                                                 controlPoints, knots, weights,
                                                 controlPtTol, knotTol) != Acad::eOk)        
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         
         return GS_BAD;
      }
   }
   else
      return GS_BAD;

   return GS_GOOD;
}
int gsc_open_pline(ads_name ent)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_open_pline((AcDbEntity *) pObj) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_close_pline                        <external> */
/*+                                                                       
  Chiude una polilinea di qualunque tipo (AcDb2dPolyline, AcDb3dPolyline,
  AcDbPolyline, AcDbSpline)
  Parametri:
  AcDbEntity *pEnt;   Oggetto da verificare
  oppure
  ads_name ent;       Oggetto da verificare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_close_pline(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      if (!((AcDb2dPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDb2dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDb2dPolyline *) pEnt)->makeClosed() != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      if (!((AcDb3dPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDb3dPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDb3dPolyline *) pEnt)->makeClosed() != Acad::eOk) 
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (!((AcDbPolyline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbPolyline *) pEnt)->setClosed(true);
      }
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      if (!((AcDbSpline *) pEnt)->isClosed())
      {
         Acad::ErrorStatus Res = ((AcDbSpline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         int              degree; 
         Adesk::Boolean   rational, closed, periodic;
         AcGePoint3dArray controlPoints;
         AcGeDoubleArray  knots;
         AcGeDoubleArray  weights;
         double           controlPtTol, knotTol;
         
         ((AcDbSpline *) pEnt)->getNurbsData(degree, rational, closed, periodic,                 
                                             controlPoints, knots, weights,
                                             controlPtTol, knotTol);
         closed = Adesk::kTrue;
         ((AcDbSpline *) pEnt)->setNurbsData(degree, rational, closed, periodic,
                                             controlPoints, knots, weights,
                                             controlPtTol, knotTol);         

         return GS_BAD;
      }
   }
   else
      return GS_BAD;

   return GS_GOOD;
}
int gsc_close_pline(ads_name ent)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_close_pline((AcDbEntity *) pObj) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*+
-------------------------------------------------------------
   int gsc_breakpline(ads_name ent,ads_point point)

     Spezza una polilinea ent nel punto point e duplica la EED.
     Ritorna GS_BAD in caso di errore o entita' sbagliata.
     GS_GOOD altrimenti.
     NB: Ritorna GS_GOOD anche se si cerca di spezzare una polilinea
     nel punto di inizio o fine (in questo caso pero' non esegue nulla)
      
-------------------------------------------------------------
-*/
int gsc_breakpline(ads_name ent,ads_point point,C_EED *new_EED)
{
   ads_point pnt;
   ads_name second,last;
   C_EED tmp_EED;

   if (!ent || !point) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (acdbEntLast(last) != RTNORM) 
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (gsc_isLinearEntity(ent) == GS_BAD) return GS_BAD;

   // Non posso spezzare una polilinea nei suoi estremi
   if (gsc_get_firstPoint(ent, pnt) == GS_BAD) return GS_BAD;
   if (gsc_point_equal(point, pnt)) return GS_GOOD;
   if (gsc_get_lastPoint(ent, pnt) == GS_BAD) return GS_BAD;
   if (gsc_point_equal(point, pnt)) return GS_GOOD;

   if (gsc_callCmd(_T("_.BREAK"), RTENAME, ent,
                   RTPOINT, point, RTPOINT, point, 0) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   // Setta "ent" come primo pezzo della polilinea
   if (ads_entnext(last, ent) != RTNORM) 
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   while (1)
   {
      if (ads_entnext(last, last) != RTNORM) 
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (gsc_is_acad_main_entity(last) == TRUE) break;
   }
   ads_name_set(last, ent);

   // Setta "second" come secondo pezzo della polilinea
   while (1)
   {
      if (ads_entnext(last, last) != RTNORM) 
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (gsc_is_acad_main_entity(last) == TRUE) break;
   }
   ads_name_set(last, second);

   if (new_EED)
   {
   }
     
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_graph_type <external> */
/*+                                                                       
  Restituisce il tipo di un oggetto grafico.
  Parametri:
  ads_name ent;	Oggetto
  TCHAR *type;		Stringa già allocata a MAX_LEN_GRAPH_TYPE
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_graph_type(ads_name ent, TCHAR *type)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   AcRxClass    *WhatIs;

   if (!ent || ads_name_nil(ent)) return GS_BAD;
   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   if ((WhatIs = pObj->isA()) == NULL) { pObj->close(); return GS_BAD; }
   wcscpy(type, WhatIs->dxfName());
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isClosedPline <external> */
/*+                                                                       
  Verifica se la polilinea è chiusa o aperta.
  Parametri:
  ads_name ent;		Oggetto
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_isClosedPline(ads_name ent)
{
   C_RB_LIST DescrEnt;
   presbuf   p_rb;

   if (ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((DescrEnt << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p_rb = DescrEnt.SearchType(0)) == NULL || p_rb->resval.rstring == NULL)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if (gsc_strcmp(p_rb->resval.rstring, _T("POLYLINE")) == 0 ||
       gsc_strcmp(p_rb->resval.rstring, _T("LWPOLYLINE")) == 0)
   {
      if ((p_rb = DescrEnt.SearchType(70)) == NULL)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; } 
      if (p_rb->resval.rint & 1) return GS_GOOD;

      // Verifico che il primo e l'ultimo vertice coincidano
      ads_point FirstPt, LastPt;

      if (gsc_get_firstPoint(ent, FirstPt) == GS_BAD) return GS_BAD;
      if (gsc_get_lastPoint(ent, LastPt) == GS_BAD) return GS_BAD;

      if (gsc_point_equal(FirstPt, LastPt)) return GS_GOOD;
   }
   else if (gsc_strcmp(p_rb->resval.rstring, _T("MLINE")) == 0)
   {
      if ((p_rb = DescrEnt.SearchType(71)) == NULL)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; } 
      if (p_rb->resval.rint & 2) return GS_GOOD;
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_isClosedEntity                     <external> */
/*+                                                                       
  Verifica se l'entità è chiusa o aperta.
  Parametri:
  AcDbEntity *pEnt;		Oggetto
  
  Restituisce true in caso di successo altrimenti restituisce false. 
-*/  
/*********************************************************/
bool gsc_isClosedEntity(ads_name ent)  // roby shape
{
   AcDbEntity   *pEnt;
   AcDbObjectId objId;
   bool         result;

   if (!ent || ads_name_nil(ent)) return false;
   if (acdbGetObjectId(objId, ent) != Acad::eOk) return false;
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk) return false;
   result = gsc_isClosedEntity(pEnt);
   if (pEnt->close() != Acad::eOk) return false;

   return result;
}
bool gsc_isClosedEntity(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      if (((AcDb2dPolyline *) pEnt)->isClosed()) return true;

      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      if (((AcDb3dPolyline *) pEnt)->isClosed()) return true;

      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc()))
   {
      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
      return true;
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      if (((AcDbMline *) pEnt)->closedMline()) return true;

      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }
   else
   if (pEnt->isKindOf(AcDbMPolygon::desc()))
      return true;
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pEnt)->isClosed()) return true;

      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      if (((AcDbSpline *) pEnt)->isClosed()) return true;

      ads_point StartPt, EndPt;

      if (gsc_get_firstPoint(pEnt, StartPt) == GS_GOOD && 
          gsc_get_lastPoint(pEnt, EndPt) == GS_GOOD && 
          ads_point_equal(StartPt, EndPt))
         return true;
   }

   return false;
}


/*********************************************************/
/*.doc gsc_enthand                              <extern> */
/*+
   Ricava l'handle dell'oggetto grafico.
   Parametri:
   ads_name ent;     oggetto grafico
   TCHAR *hand;      stringa allocata ad almeno MAX_LEN_HANDLE
   size_t hand_len;  dimensione dell'allocazione di hand (default = MAX_LEN_HANDLE)

   Ritorna GS_BAD in caso ERRORE altrimenti GS_GOOD.
-*/
/*********************************************************/
int gsc_enthand(ads_name ent, TCHAR *hand, size_t hand_len)
{
   AcDbObjectId objId;
   AcDbHandle   objHand;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   return gsc_enthand(objId, hand, hand_len);
}
int gsc_enthand(AcDbObjectId &objId, TCHAR *hand, size_t hand_len)
{
   AcDbObject *pObj;
   AcDbHandle objHand;

   if (acdbOpenObject(pObj, objId, AcDb::kForRead, true) != Acad::eOk) return GS_BAD;
   gsc_enthand(pObj, hand, hand_len);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int gsc_enthand(AcDbObject *pObj, TCHAR *hand, size_t handle_len)
{
   AcDbHandle objHand;

   pObj->getAcDbHandle(objHand);
   objHand.getIntoAsciiBuffer(hand, handle_len);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_IsInternalPt                         <extern> */
/*+
   Verifica se l'oggetto grafico ContainerEnt contiene il punto.
   Parametri:
   ads_name ContainerEnt;     oggetto grafico contenitore
   ads_point point;           punto da verificare

   Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
-*/
/*********************************************************/
int gsc_IsInternalPt(AcDbEntity *pContainerEnt, ads_point point)
{
   ads_point        Corner1, Corner2;
   AcDbPolyline     HorizSegment(2), VertSegment(2);
   AcGePoint2d      Vertice;
   int              dx = 0, sx = 0;
   C_POINT_LIST     IntersectionPts;
   C_POINT          *pIntersectionPt;

   // Se devo verificare se è dentro un cerchio faccio così che è più veloce
   if (pContainerEnt->isKindOf(AcDbCircle::desc()))
   {
      ads_point center;

      if (gsc_get_firstPoint(pContainerEnt, center) == GS_BAD) return GS_BAD;
      return (gsc_dist(center, point) > ((AcDbCircle *) pContainerEnt)->radius()) ? GS_BAD : GS_GOOD;
   }

   // Ricavo la finestra di occupazione di ContainerEnt
   if (gsc_get_ent_window(pContainerEnt, Corner1, Corner2) == GS_BAD) return GS_BAD;

   // Creo un segmento orizzontale
   Vertice.set(Corner1[X], point[Y]);
   HorizSegment.addVertexAt(0, Vertice);
   Vertice.set(Corner2[X], point[Y]);
   HorizSegment.addVertexAt(1, Vertice);
   HorizSegment.setElevation(point[Z]);

   // Verifico le intersezioni del segmento orizzontale con pContainerEnt
   if (IntersectionPts.add_intersectWith(pContainerEnt, &HorizSegment) != GS_GOOD)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   pIntersectionPt = (C_POINT *) IntersectionPts.get_head();
   
   // Se un punto di intersezione coincide con il punto, allora era interno,
   // oppure se il numero di intersezioni a destra del punto è dispari
   // era interno a ContainerEnt
   while (pIntersectionPt)
   {
      if (pIntersectionPt->point[X] == point[X]) return GS_GOOD;     
      //if (fabs(IntersectionPt.point[X] - point[X]) <= GEOsimAppl::TOLERANCE) return GS_GOOD;
      else
      if (pIntersectionPt->point[X] > point[X]) dx++; // intersezione a destra
      else sx++; // intersezione a sinistra

      pIntersectionPt = (C_POINT *) pIntersectionPt->get_next();
   }

   if ((sx % 2) == 0 || (dx % 2) == 0) return GS_BAD;

   // Creo un segmento verticale
   Vertice.set(point[X], Corner1[Y]);
   VertSegment.addVertexAt(0, Vertice);
   Vertice.set(point[X], Corner2[Y]);
   VertSegment.addVertexAt(1, Vertice);
   VertSegment.setElevation(point[Z]);

   // Verifico le intersezioni del segmento verticale con pContainerEnt
   IntersectionPts.remove_all();
   dx = sx = 0;
   if (IntersectionPts.add_intersectWith(pContainerEnt, &VertSegment) != GS_GOOD)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   pIntersectionPt = (C_POINT *) IntersectionPts.get_head();

   // Se un punto di intersezione coincide con il punto, allora era interno,
   // oppure se il numero di intersezioni a sopra (dx) del punto è dispari
   // era interno a ContainerEnt
   while (pIntersectionPt)
   {
      if (pIntersectionPt->point[Y] == point[Y]) return GS_GOOD;     
      //if (fabs(pIntersectionPt->point[Y] - point[Y]) <= GEOsimAppl::TOLERANCE) return GS_GOOD;
      else
      if (pIntersectionPt->point[Y] > point[Y]) dx++; // intersezione sopra
      else sx++; // intersezione sotto

      pIntersectionPt = (C_POINT *) pIntersectionPt->get_next();
   }

   return ((dx % 2) == 1 && (sx % 2) == 1) ? GS_GOOD : GS_BAD;
}
int gsc_IsInternalPt(ads_name ContainerEnt, ads_point point)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   int          Result;

   // Verifico le intersezioni del segmento con ContainerEnt
   if (acdbGetObjectId(objId, ContainerEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   Result = gsc_IsInternalPt((AcDbEntity *) pObj, point);

   pObj->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_IsInternalNearIntersPt              <internal> */
/*+
   Verifica se il punto di intersezione trovato dalla funzione
   "gsc_IsInternalEnt" si riferisce ad un segmento interno o esterno
   all'oggetto grafico ContainerEnt.
   Parametri:
   AcDbEntity *pContainerEnt; oggetto grafico contenitore
   ads_point p1;              primo punto del segmento
   ads_point p2;              primo punto del segmento
   ads_point IntersPt;        punto di intersezione del segmento con ContainerEnt

   oppure

   ads_name ContainerEnt;     oggetto grafico contenitore
   ads_point p1;              primo punto del segmento
   ads_point p2;              primo punto del segmento
   ads_point IntersPt;        punto di intersezione del segmento con ContainerEnt

   Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
-*/
/*********************************************************/
int gsc_IsInternalNearIntersPt(AcDbEntity *pContainerEnt, ads_point p1, ads_point p2,
                               ads_point IntersPt)
{
   ads_point ptNear;
   double    Angle = acutAngle(p1, p2);

   // se il punto coincide con il punto iniziale
   if (gsc_point_equal(p1, IntersPt) == GS_GOOD)
   {
      // se poco dopo non si era all'interno
      acutPolar(p1, Angle, GEOsimAppl::TOLERANCE, ptNear);
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
   }
   else
   // se il punto coincide con il punto finale
   if (gsc_point_equal(p2, IntersPt) == GS_GOOD)
   {
      // se poco prima non si era all'interno
      acutPolar(p2, Angle + PI, GEOsimAppl::TOLERANCE, ptNear);
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
   }
   else 
   // se il punto è intermedio
   { 
      // se poco dopo non si era all'interno
      acutPolar(IntersPt, Angle, GEOsimAppl::TOLERANCE, ptNear);
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
      // se poco prima non si era all'interno
      acutPolar(IntersPt, Angle + PI, GEOsimAppl::TOLERANCE, ptNear);
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_IsInternalNearIntersPt(ads_name ContainerEnt, ads_point p1, ads_point p2,
                               ads_point IntersPt)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   int          Result;

   if (acdbGetObjectId(objId, ContainerEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   Result = gsc_IsInternalNearIntersPt((AcDbEntity *) pObj, p1, p2, IntersPt);

   pObj->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_IsInternalNearIntersPt              <internal> */
/*+
   Verifica se il punto di intersezione trovato dalla funzione
   "gsc_IsInternalEnt" si riferisce ad un arco interno o esterno
   all'oggetto grafico ContainerEnt.
   Parametri:
   AcDbEntity *pContainerEnt; oggetto grafico contenitore
   ads_point p1;              punto iniziale dell'arco
   ads_point p2;              punto finale dell'arco
   ads_point center;          centro dell'arco
   double    radius;          raggio dell'arco
   double    startAngle;      angolo iniziale
   double    endAngle;        angolo finale
   ads_point IntersPt;        punto di intersezione del segmento con ContainerEnt

   oppure

   ads_name ContainerEnt;     oggetto grafico contenitore
   ads_point p1;              punto iniziale dell'arco
   ads_point p2;              punto finale dell'arco
   ads_point center;          centro dell'arco
   double    radius;          raggio dell'arco
   double    startAngle;      angolo iniziale
   double    endAngle;        angolo finale
   ads_point IntersPt;        punto di intersezione del segmento con ContainerEnt

   Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
-*/
/*********************************************************/
int gsc_IsInternalNearIntersPt(AcDbEntity *pContainerEnt, ads_point p1, ads_point p2,
                               ads_point center, double radius, double startAngle,
                               double endAngle, ads_point IntersPt)
{
   ads_point ptNear;
   double    Angle = acutAngle(center, IntersPt);
   double    OffSet = (180 * GEOsimAppl::TOLERANCE) / (PI * radius);

   // se il punto coincide con il punto iniziale
   if (gsc_point_equal(p1, IntersPt) == GS_GOOD)
   {
      // se poco dopo non si era all'interno
      if (endAngle > startAngle) // senso antiorario
         acutPolar(center, Angle + OffSet, radius, ptNear);
      else // senso orario
         acutPolar(center, Angle - OffSet, radius, ptNear);
      
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD )return GS_BAD;
   }
   else
   // se il punto coincide con il punto finale
   if (gsc_point_equal(p2, IntersPt) == GS_GOOD)
   {
      // se poco prima non si era all'interno
      if (endAngle > startAngle) // senso antiorario
         acutPolar(center, Angle - OffSet, radius, ptNear);
      else // senso orario
         acutPolar(center, Angle + OffSet, radius, ptNear);

      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
   }
   else 
   // se il punto è intermedio
   { 
      // se poco dopo non si era all'interno
      if (endAngle > startAngle) // senso antiorario
         acutPolar(center, Angle + OffSet, radius, ptNear);
      else // senso orario
         acutPolar(center, Angle - OffSet, radius, ptNear);
      
      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;

      // se poco prima non si era all'interno
      if (endAngle > startAngle) // senso antiorario
         acutPolar(center, Angle - OffSet, radius, ptNear);
      else // senso orario
         acutPolar(center, Angle + OffSet, radius, ptNear);

      if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_IsInternalNearIntersPt(ads_name ContainerEnt, ads_point p1, ads_point p2,
                               ads_point center, double radius, double startAngle,
                               double endAngle, ads_point IntersPt)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   int          Result;

   if (acdbGetObjectId(objId, ContainerEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   Result = gsc_IsInternalNearIntersPt((AcDbEntity *) pObj, p1, p2, center,
                                       radius, startAngle, endAngle, IntersPt);

   pObj->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_IsInternalNearIntersPt              <internal> */
/*+
   Verifica se il punto di intersezione trovato dalla funzione
   "gsc_IsInternalEnt" si riferisce ad una ellisse interna o esterna
   all'oggetto grafico ContainerEnt.
   Parametri:
   AcDbEntity   *pContainerEnt;  Oggetto grafico contenitore
   AcGeVector3d majorAxis;       Asse maggiore
   double       radiusRatio;     rappoto tra asse minore e asse maggiore
   ads_point    center;          centro dell'ellisse
   double       startAngle;      angolo iniziale
   double       endAngle;        angolo finale
   ads_point    IntersPt;        punto di intersezione del segmento con ContainerEnt

   Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
-*/
/*********************************************************/
int gsc_IsInternalNearIntersPt(AcDbEntity *pContainerEnt, AcGeVector3d majorAxis,
                               ads_point center, double radiusRatio, double startAngle,
                               double endAngle, ads_point IntersPt)
{
   ads_point ptNear, ConvIntersPt, ConvPt, ptConvNear, Origin;
   double    Angle = acutAngle(center, IntersPt);

   Origin[X] = Origin[Y] = 0.0; Origin[Z] = center[Z];

   // Effettuo i calcoli considerando un'ellisse con centro 0,0
   // e avente asse maggiore coincidente all'asse X

   ConvPt[X] = center[X] + majorAxis.x;
   ConvPt[Y] = center[Y] + majorAxis.y;
   ConvPt[Z] = center[Z];
   double majorAxisAng = acutAngle(center, ConvPt); // Inclinazione asse maggiore
   double majorAxisLen = gsc_dist(center, ConvPt); // 
   double minorAxisLen = majorAxisLen * radiusRatio;
   double AngIntersPt = acutAngle(center, IntersPt);
   double IntersPtDistFromCenter = gsc_dist(center, IntersPt);
   double OffsetAng              = AngIntersPt - majorAxisAng;

   // lo ruoto
   acutPolar(center, OffsetAng, IntersPtDistFromCenter, ConvIntersPt);

   // traslo il punto di intersezione
   ConvIntersPt[X] = ConvIntersPt[X] - center[X];
   ConvIntersPt[Y] = ConvIntersPt[Y] - center[Y];
   ConvIntersPt[Z] = ConvIntersPt[Z] - center[Z];

   ptNear[Z] = ConvIntersPt[Z];
   // se il punto poco dopo è compreso tra gli estremi dell'asse maggiore
   ptNear[X] = ConvIntersPt[X] + GEOsimAppl::TOLERANCE;
   if (ptNear[X] <= majorAxisLen && ptNear[X] >= -1 * majorAxisLen)
   {
      ptNear[Y] = sqrt(4 * pow(minorAxisLen, 2) - 4 * (pow(minorAxisLen, 2) * pow(ptNear[X], 2) / pow(majorAxisLen, 2))) / 2;
      if (ConvIntersPt[Y] < 0) ptNear[Y] = ptNear[Y] * -1;
      // lo ruoto
      OffsetAng              = acutAngle(Origin, ptNear);
      IntersPtDistFromCenter = gsc_dist(Origin, ptNear);
      acutPolar(Origin, OffsetAng + majorAxisAng, IntersPtDistFromCenter, ptConvNear);
      // traslo il punto
      ptConvNear[X] += center[X];
      ptConvNear[Y] += center[Y];
      ptConvNear[Z] += center[Z];

      if (gsc_IsInternalPt(pContainerEnt, ptConvNear) == GS_BAD) return GS_BAD;
   }
   // se il punto poco prima è compreso tra gli estremi dell'asse maggiore
   ptNear[X] = ConvIntersPt[X] - GEOsimAppl::TOLERANCE;
   if (ptNear[X] <= majorAxisLen && ptNear[X] >= -1 * majorAxisLen)
   {
      ptNear[Y] = sqrt(4 * pow(minorAxisLen, 2) - 4 * (pow(minorAxisLen, 2) * pow(ptNear[X], 2) / pow(majorAxisLen, 2))) / 2;
      if (ConvIntersPt[Y] < 0) ptNear[Y] = ptNear[Y] * -1;
      // lo ruoto
      OffsetAng              = acutAngle(Origin, ptNear);
      IntersPtDistFromCenter = gsc_dist(Origin, ptNear);
      acutPolar(Origin, OffsetAng + majorAxisAng, IntersPtDistFromCenter, ptConvNear);
      // traslo il punto
      ptConvNear[X] += center[X];
      ptConvNear[Y] += center[Y];
      ptConvNear[Z] += center[Z];

      if (gsc_IsInternalPt(pContainerEnt, ptConvNear) == GS_BAD) return GS_BAD;
   }

   // se il punto poco dopo è compreso tra gli estremi dell'asse minore
   ptNear[Y] = ConvIntersPt[Y] + GEOsimAppl::TOLERANCE;
   if (ptNear[Y] <= minorAxisLen && ptNear[Y] >= -1 * minorAxisLen)
   {
      ptNear[X] = sqrt(4 * pow(majorAxisLen, 2) - 4 * (pow(majorAxisLen, 2) * pow(ptNear[Y], 2) / pow(minorAxisLen, 2))) / 2;
      if (ConvIntersPt[X] < 0) ptNear[X] = ptNear[X] * -1;
      // lo ruoto
      OffsetAng              = acutAngle(Origin, ptNear);
      IntersPtDistFromCenter = gsc_dist(Origin, ptNear);
      acutPolar(Origin, OffsetAng + majorAxisAng, IntersPtDistFromCenter, ptConvNear);
      // traslo il punto
      ptConvNear[X] += center[X];
      ptConvNear[Y] += center[Y];
      ptConvNear[Z] += center[Z];

      if (gsc_IsInternalPt(pContainerEnt, ptConvNear) == GS_BAD) return GS_BAD;
   }
   // se il punto poco prima è compreso tra gli estremi dell'asse maggiore
   ptNear[Y] = ConvIntersPt[Y] - GEOsimAppl::TOLERANCE;
   if (ptNear[Y] <= minorAxisLen && ptNear[Y] >= -1 * minorAxisLen)
   {
      ptNear[X] = sqrt(4 * pow(majorAxisLen, 2) - 4 * (pow(majorAxisLen, 2) * pow(ptNear[Y], 2) / pow(minorAxisLen, 2))) / 2;
      if (ConvIntersPt[X] < 0) ptNear[X] = ptNear[X] * -1;
      // lo ruoto
      OffsetAng              = acutAngle(Origin, ptNear);
      IntersPtDistFromCenter = gsc_dist(Origin, ptNear);
      acutPolar(Origin, OffsetAng + majorAxisAng, IntersPtDistFromCenter, ptConvNear);
      // traslo il punto
      ptConvNear[X] += center[X];
      ptConvNear[Y] += center[Y];
      ptConvNear[Z] += center[Z];

      if (gsc_IsInternalPt(pContainerEnt, ptConvNear) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_IsInternalEnt                        <extern> */
/*+
   Verifica se l'oggetto grafico ContainerEnt contiene l'oggetto grafico ent.
   Parametri:
   AcDbEntity *pContainerEnt
   AcDbEntity *pEnt;
   int        Mode;           Se Mode = INSIDE l'oggetto ent deve essere 
                              completamente interno (i punti tangenti sono 
                              considerati interni). Se Mode = CROSSING 
                              l'oggetto può anche essere intersecare
                              (default = INSIDE).

   oppure
   
   ads_name ContainerEnt;     oggetto grafico contenitore
   ads_name ent;              oggetto grafico da verificare
   int      Mode;             Se Mode = INSIDE l'oggetto ent deve essere 
                              completamente interno (i punti tangenti sono 
                              considerati interni). Se Mode = CROSSING 
                              l'oggetto può anche intersecare
                              (default = INSIDE).

   Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
   N.B.: Attualmente non funziona per oggetti di tipo AcDbDimension, AcDbEllipse,
   AcDbLeader, AcDbPolyFaceMesh, AcDbPolygonMesh, AcDbRay, AcDbSpline, AcDbXline
-*/
/*********************************************************/
int gsc_IsInternalEnt(AcDbEntity *pContainerEnt, AcDbEntity *pEnt, int Mode)
{
   C_POINT_LIST IntersPts;
   C_POINT      *pIntersPt;

   // se è un elemento puntuale faccio così che è più veloce
   if (gsc_isPunctualEntity(pEnt))
   {
      // Se devo verificare se un punto è dentro un cerchio
      if (pContainerEnt->isKindOf(AcDbCircle::desc()))
      {
         ads_point center, point;

         if (gsc_get_firstPoint(pContainerEnt, center) == GS_BAD) return GS_BAD;
         if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
         return (gsc_dist(center, point) > ((AcDbCircle *) pContainerEnt)->radius()) ? GS_BAD : GS_GOOD;
      }
      else
      {
         ads_point point;

         // I 2 oggetti grafici non si intersecano, quindi se un punto qualsiasi di
         // ent è interno a ContainerEnt significa che tutto l'oggetto è interno
         if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;

         return gsc_IsInternalPt(pContainerEnt, point);
      }
   }

   if (IntersPts.add_intersectWith(pContainerEnt, pEnt) == GS_BAD) return GS_BAD;

   if (IntersPts.get_count() == 0)
   {
      ads_point point;

      // I 2 oggetti grafici non si intersecano, quindi se un punto qualsiasi di
      // ent è interno a ContainerEnt significa che tutto l'oggetto è interno
      if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;

      return gsc_IsInternalPt(pContainerEnt, point);
   }

   // Se gli oggetti si intersecano ritorno GS_GOOD se la modalità era CROSSING
   if (Mode == CROSSING) return GS_GOOD;

   // Devo verificare per ogni punto di intersezione se poco prima e poco dopo
   // l'oggetto intersecante era interno

   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        FirstPt, pt1, pt2;
      int                isClosed, ToClose;
      double             radius, startAngle, endAngle, ArcAngle; // angolo inscritto
      ads_point          dummyPt1, dummyPt2, center;
      AcDbEntity         *pVertexEnt;

      // Se è chiusa bisogna considerare il segmento di chiusura con il primo vertice
      isClosed = (((AcDb2dPolyline *) pEnt)->isClosed() == Adesk::kTrue) ? GS_GOOD : GS_BAD;

      pVertIter = ((AcDb2dPolyline *)pEnt)->vertexIterator();

      // primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex) // punto di controllo
            { if (ToClose) pVertex->close(); continue; }

         pt1      = pVertex->position();           
         // vertice curva di adattamento (segue un arco)
         ArcAngle = (pVertex->bulge() != 0) ? atan(pVertex->bulge()) * 4 : 0.0; // tang di 1/4 angolo incluso
         if (ToClose) pVertex->close();

         if (isClosed == GS_GOOD) FirstPt = pt1;
         break;
      }

      // vertici successivi
      for (pVertIter->step(); !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
               { delete pVertIter; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         }

         if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex) // punto di controllo
            { if (ToClose) pVertex->close(); continue; }

         pt2 = pVertex->position();
         IntersPts.remove_all();
         ads_point_set_from_AcGePoint3d(pt1, dummyPt1);
         ads_point_set_from_AcGePoint3d(pt2, dummyPt2);

         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(dummyPt1, dummyPt2, ArcAngle, &radius, center);
            if (ArcAngle > 0) // senso antiorario
            {
               startAngle = acutAngle(center, dummyPt1);
               endAngle   = acutAngle(center, dummyPt2);
            }
            else
            {
               startAngle = acutAngle(center, dummyPt2);
               endAngle   = acutAngle(center, dummyPt1);
            }

            IntersPts.add_intersectWithArc(pContainerEnt, center, radius, startAngle, endAngle);

            // per ogni punto di intersezione
            pIntersPt = (C_POINT *) IntersPts.get_head();
            while (pIntersPt)
            {
               if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, center, radius,
                                              startAngle, endAngle, pIntersPt->point) == GS_BAD)
                  { if (ToClose) pVertex->close(); delete pVertIter; return GS_BAD; }

               pIntersPt = (C_POINT *) IntersPts.get_next();
            }
         }
         else // è un segmento
         {
            IntersPts.add_intersectWithLine(pContainerEnt, dummyPt1, dummyPt2);

            // per ogni punto di intersezione
            pIntersPt = (C_POINT *) IntersPts.get_head();
            while (pIntersPt)
            {
               // Verifico se poco prima e poco dopo il punto di intersezione
               // il segmento intersecante era interno
               if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
                  { if (ToClose) pVertex->close(); delete pVertIter; return GS_BAD; }

               pIntersPt = (C_POINT *) IntersPts.get_next();
            }
         }

         // vertice curva di adattamento (se <> 0 segue un arco)
         ArcAngle = (pVertex->bulge() != 0) ? atan(pVertex->bulge()) * 4 : 0.0; // tang di 1/4 angolo incluso
         pt1 = pt2;
         if (ToClose) pVertex->close();
      }

      delete pVertIter;

      if (isClosed == GS_GOOD)
      {
         pt2 = FirstPt;
         ads_point_set_from_AcGePoint3d(pt1, dummyPt1);
         ads_point_set_from_AcGePoint3d(pt2, dummyPt2);

         IntersPts.remove_all();
         IntersPts.add_intersectWithLine(pContainerEnt, dummyPt1, dummyPt2);

         // per ogni punto di intersezione
         pIntersPt = (C_POINT *) IntersPts.get_head();
         while (pIntersPt)
         {
            // Verifico se poco prima e poco dopo il punto di intersezione
            // il segmento intersecante era interno
            if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
               return GS_BAD;

            pIntersPt = (C_POINT *) IntersPts.get_next();
         }
      }
   }
   else if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      double      radius, bulge, startAngle, endAngle, ArcAngle; // angolo inscritto
      int         isClosed, i;
      AcGePoint3d FirstPt, pt1, pt2;
      ads_point   dummyPt1, dummyPt2, center;

      // Se è chiusa bisogna considerare il segmento di chiusura con il primo vertice
      isClosed = (((AcDbPolyline *) pEnt)->isClosed() == Adesk::kTrue) ? GS_GOOD : GS_BAD;

      ((AcDbPolyline *) pEnt)->getPointAt(0, pt1);
      if (isClosed == GS_GOOD) FirstPt = pt1;
      
      // vertice curva di adattamento (segue un arco)
      ((AcDbPolyline *) pEnt)->getBulgeAt(0, bulge);
      ArcAngle = (bulge != 0) ? atan(bulge) * 4 : 0.0; // tang di 1/4 angolo incluso

      i = 1;
      while (((AcDbPolyline *) pEnt)->getPointAt(i, pt2) == Acad::eOk) // vertici successivi
      {
         IntersPts.remove_all();
         ads_point_set_from_AcGePoint3d(pt1, dummyPt1);
         ads_point_set_from_AcGePoint3d(pt2, dummyPt2);

         IntersPts.remove_all();
         if (ArcAngle != 0) // arco
         {
            gsc_getArcRayCenter(dummyPt1, dummyPt2, ArcAngle, &radius, center);
            if (ArcAngle > 0) // senso antiorario
            {
               startAngle = acutAngle(center, dummyPt1);
               endAngle   = acutAngle(center, dummyPt2);
            }
            else
            {
               startAngle = acutAngle(center, dummyPt2);
               endAngle   = acutAngle(center, dummyPt1);
            }
            
            IntersPts.add_intersectWithArc(pContainerEnt, center, radius, startAngle, endAngle);

            // per ogni punto di intersezione
            pIntersPt = (C_POINT *) IntersPts.get_head();
            while (pIntersPt)
            {
               if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, center, radius,
                                              startAngle, endAngle, pIntersPt->point) == GS_BAD)
                  return GS_BAD;
          
               pIntersPt = (C_POINT *) IntersPts.get_next();
            }
         }
         else // è un segmento
         {
            IntersPts.add_intersectWithLine(pContainerEnt, dummyPt1, dummyPt2);

            // per ogni punto di intersezione
            pIntersPt = (C_POINT *) IntersPts.get_head();
            while (pIntersPt)
            {
               // Verifico se poco prima e poco dopo il punto di intersezione
               // il segmento intersecante era interno
               if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
                  return GS_BAD;

               pIntersPt = (C_POINT *) IntersPts.get_next();
            }
         }

         ((AcDbPolyline *) pEnt)->getBulgeAt(i, bulge);
         // vertice curva di adattamento (segue un arco)
         ArcAngle = (bulge != 0) ? atan(bulge) * 4 : 0.0; // tang di 1/4 angolo incluso
         pt1 = pt2;
         i++;
      }

      if (isClosed == GS_GOOD)
      {
         pt2 = FirstPt;
         ads_point_set_from_AcGePoint3d(pt1, dummyPt1);
         ads_point_set_from_AcGePoint3d(pt2, dummyPt2);

         IntersPts.remove_all();
         IntersPts.add_intersectWithLine(pContainerEnt, dummyPt1, dummyPt2);

         // per ogni punto di intersezione
         pIntersPt = (C_POINT *) IntersPts.get_head();
         while (pIntersPt)
         {
            // Verifico se poco prima e poco dopo il punto di intersezione
            // il segmento intersecante era interno
            if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
               return GS_BAD;

            pIntersPt = (C_POINT *) IntersPts.get_next();
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d pt1, pt2;
      ads_point   dummyPt1, dummyPt2;
      int         NumVerts = ((AcDbMline *) pEnt)->numVertices();

      pt1 = ((AcDbMline *) pEnt)->vertexAt(0);
      
      for (int i = 1; i < NumVerts; i++)
      {
         pt2 = ((AcDbMline *) pEnt)->vertexAt(i);

         IntersPts.remove_all();
         ads_point_set_from_AcGePoint3d(pt1, dummyPt1);
         ads_point_set_from_AcGePoint3d(pt2, dummyPt2);

         IntersPts.remove_all();
         IntersPts.add_intersectWithLine(pContainerEnt, dummyPt1, dummyPt2);

         // per ogni punto di intersezione
         pIntersPt = (C_POINT *) IntersPts.get_head();
         while (pIntersPt)
         {
            // Verifico se poco prima e poco dopo il punto di intersezione
            // il segmento intersecante era interno
            if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
               return GS_BAD;

            pIntersPt = (C_POINT *) IntersPts.get_next();
         }

         pt1 = pt2;
      }
   }
   else if (pEnt->isKindOf(AcDbLine::desc()))
   {
      ads_point dummyPt1, dummyPt2;

      // primo punto
      ads_point_set_from_AcGePoint3d(((AcDbLine *) pEnt)->startPoint(), dummyPt1);
      // secondo punto
      ads_point_set_from_AcGePoint3d(((AcDbLine *) pEnt)->endPoint(), dummyPt2);

      // per ogni punto di intersezione
      pIntersPt = (C_POINT *) IntersPts.get_head();
      while (pIntersPt)
      {
         // Verifico se poco prima e poco dopo il punto di intersezione
         // il segmento intersecante era interno
         if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, pIntersPt->point) == GS_BAD)
            return GS_BAD;

         pIntersPt = (C_POINT *) IntersPts.get_next();
      }
   }
   else if (pEnt->isKindOf(AcDbArc::desc()) &&
            ((AcDbArc *) pEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0)) // arco piano
   {
      double     radius, startAngle, endAngle;
      ads_point  dummyPt1, dummyPt2, center;

      // centro
      center[X]  = ((AcDbArc *) pEnt)->center().x;
      center[Y]  = ((AcDbArc *) pEnt)->center().y;
      center[Z]  = ((AcDbArc *) pEnt)->center().z;
      radius     = ((AcDbArc *) pEnt)->radius();     // raggio
      startAngle = ((AcDbArc *) pEnt)->startAngle(); // angolo iniziale
      endAngle   = ((AcDbArc *) pEnt)->endAngle();   // angolo finale

      acutPolar(center, startAngle, radius, dummyPt1);
      acutPolar(center, endAngle, radius, dummyPt2);
      
      // per ogni punto di intersezione
      pIntersPt = (C_POINT *) IntersPts.get_head();
      while (pIntersPt)
      {
         if (gsc_IsInternalNearIntersPt(pContainerEnt, dummyPt1, dummyPt2, center, radius,
                                        startAngle, endAngle, pIntersPt->point) == GS_BAD)
            return GS_BAD;

         pIntersPt = (C_POINT *) IntersPts.get_next();
      }
   }
   else if (pEnt->isKindOf(AcDbCircle::desc()) &&
            ((AcDbCircle *) pEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0)) // cerchio piano
   {
      double    radius, OffSet, Angle;
      ads_point center, ptNear;

      // centro
      ads_point_set_from_AcGePoint3d(((AcDbCircle *) pEnt)->center(), center);
      radius    = ((AcDbCircle *) pEnt)->radius();         // raggio
     
      OffSet = (180 * GEOsimAppl::TOLERANCE) / (PI * radius);
      // per ogni punto di intersezione
      pIntersPt = (C_POINT *) IntersPts.get_head();
      while (pIntersPt)
      {
         Angle = acutAngle(center, pIntersPt->point);

         // se poco dopo non si era all'interno
         acutPolar(center, Angle + OffSet, radius, ptNear);        
         if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;

         // se poco prima non si era all'interno
         acutPolar(center, Angle - OffSet, radius, ptNear);
         if (gsc_IsInternalPt(pContainerEnt, ptNear) == GS_BAD) return GS_BAD;

         pIntersPt = (C_POINT *) IntersPts.get_next();
      }
   }
   else if (pEnt->isKindOf(AcDbBlockReference::desc()) ||
            pEnt->isKindOf(AcDbMText::desc()) ||
            pEnt->isKindOf(AcDbPoint::desc()) ||
            pEnt->isKindOf(AcDbText::desc()))
   {
      ads_point point;

      // Se il punto era all'interno
      gsc_get_firstPoint(pEnt, point);
      if (gsc_IsInternalPt(pContainerEnt, point) == GS_BAD) return GS_BAD;
   }
   //else if (pEnt->isKindOf(AcDbDimension::desc()))
   //{
   //}
   else if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      ads_point center;

      // centro
      center[X]  = ((AcDbEllipse *) pEnt)->center().x;
      center[Y]  = ((AcDbEllipse *) pEnt)->center().y;
      center[Z]  = ((AcDbEllipse *) pEnt)->center().z;

      // per ogni punto di intersezione
      pIntersPt = (C_POINT *) IntersPts.get_head();
      while (pIntersPt)
      {
         if (gsc_IsInternalNearIntersPt(pContainerEnt,
                                        ((AcDbEllipse *) pEnt)->majorAxis(),
                                        center,
                                        ((AcDbEllipse *) pEnt)->radiusRatio(),
                                        ((AcDbEllipse *) pEnt)->startAngle(),
                                        ((AcDbEllipse *) pEnt)->endAngle(),
                                        pIntersPt->point) == GS_BAD)
            return GS_BAD;

         pIntersPt = (C_POINT *) IntersPts.get_next();
      }
   }
   //else if (pEnt->isKindOf(AcDbLeader::desc()))
   //{
   //}
   //else if (pEnt->isKindOf(AcDbPolyFaceMesh::desc()))
   //{ //  Group entity
   //}
   //else if (pEnt->isKindOf(AcDbPolygonMesh::desc()))
   //{ //  Group entity
   //}
   //else if (pEnt->isKindOf(AcDbRay::desc()))
   //{
   //}
   //else if (pEnt->isKindOf(AcDbSpline::desc()))
   //{
   //}
   //if (pEnt->isKindOf(AcDbXline::desc()))
   //{
   //}
   else // ricavo le estensioni
   {
      AcDbExtents  Extent;
      AcGePoint3d  p1, p2;
      AcDbPolyline DummyEnt(4);
      AcGePoint2d  Vertice;

      if (pEnt->getGeomExtents(Extent) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      p1 = Extent.minPoint();
      p2 = Extent.maxPoint();

      Vertice.set(p1.x, p1.y);
      DummyEnt.addVertexAt(0, Vertice);
      Vertice.set(p2.x, p1.y);
      DummyEnt.addVertexAt(1, Vertice);
      Vertice.set(p2.x, p2.y);
      DummyEnt.addVertexAt(2, Vertice);
      Vertice.set(p1.x, p2.y);
      DummyEnt.addVertexAt(3, Vertice);
      
      DummyEnt.setClosed(Adesk::kTrue); // polilinea chiusa

      if (gsc_IsInternalEnt(pContainerEnt, &DummyEnt, Mode) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_IsInternalEnt(ads_name ContainerEnt, ads_name ent, int Mode = INSIDE)
{
   AcDbObject   *pObj1, *pObj2;
   AcDbObjectId objId;
   int          Result;

   if (acdbGetObjectId(objId, ContainerEnt) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj1, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { pObj1->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj2, objId, AcDb::kForRead) != Acad::eOk)
      { pObj1->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   Result = gsc_IsInternalEnt((AcDbEntity *) pObj1, (AcDbEntity *) pObj2, Mode);

   pObj1->close();
   pObj2->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_IsInternalEnt                      <external> */
/*+
  Questa funzione verifica se gli oggetti grafici (una serie di superfici
  considerando le isole) contengono l'oggetto grafico ent.
  Parametri:
  C_SELSET &SelSet;        gruppo di selezione di aree
  ads_name ent;            oggetto grafico da verificare
  int      Mode;           Se Mode = INSIDE l'oggetto ent deve essere 
                           completamente interno (i punti tangenti sono 
                           considerati interni). Se Mode = CROSSING 
                           l'oggetto può anche essere intersecare
                           (default = INSIDE).
  int      DeepOrder;     Parametro di uso interno (default = 1)
  
  Ritorna GS_GOOD in caso affermativo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_IsInternalEnt(C_SELSET &SelSet, AcDbEntity *pEnt, int Mode, int DeepOrder)
{
   ads_name   entity, ContainerEnt;
   long       i = 0, j;
   int        Contained, _OnBorder = FALSE, IsInternal = GS_CAN;

   // fra le superfici ricavo quelle non contenute da altre
   while (SelSet.entname(i, entity) == GS_GOOD)
   {
      j         = 0;
      Contained = FALSE;
      while (SelSet.entname(j, ContainerEnt) == GS_GOOD)
         if (j != i && gsc_IsInternalEnt(ContainerEnt, entity) == GS_GOOD)
         {
            Contained = TRUE;
            break; 
         }
         else j++;

      if (!Contained) // non contenuta da altre superfici
      {
         AcDbObjectId objId;
         AcDbEntity   *pContainerEnt;
         C_SELSET     ContainedSS;
         C_POINT_LIST IntersList;

         if (acdbGetObjectId(objId, entity) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (acdbOpenObject(pContainerEnt, objId, AcDb::kForRead) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

         // Verifico se l'oggetto è interno a questa area
         if (gsc_IsInternalEnt(pContainerEnt, pEnt, Mode) == GS_GOOD)
         {
            if ((DeepOrder % 2) == 0)
            {
               IsInternal = GS_BAD;

               // devo verificare che non abbia intersezioni col contorno della superfice
               if (IntersList.add_intersectWith(pContainerEnt, pEnt) == GS_GOOD)
                  if (IntersList.get_count() > 0)
                  {
                     pContainerEnt->close();
                     IsInternal = GS_GOOD;
                     break;
                  }
            }
            else
               IsInternal = GS_GOOD;

            // Ricavo le superfici contenute da questa
            ContainedSS.clear();
            j = 0;
            while (SelSet.entname(j, entity) == GS_GOOD)
            {
               if (j != i && gsc_IsInternalEnt(pContainerEnt, pEnt) == GS_GOOD)
               {
                  ContainedSS.add(entity);
               }
               j++;
            }
            pContainerEnt->close();

            // Verifico se l'oggetto è interno a queste superfici
            if (ContainedSS.length() > 0)
            {
               int dummy = gsc_IsInternalEnt(ContainedSS, pEnt, Mode, DeepOrder + 1);              
               if (dummy != GS_CAN) IsInternal = dummy;
            }
         }
         else
         {
            pContainerEnt->close();
            if (DeepOrder == 1 && IsInternal == GS_CAN) IsInternal = GS_BAD;
         }
      }
      i++;
   }
   
   return IsInternal;
}
int gsc_IsInternalEnt(C_SELSET &SelSet, ads_name ent, int Mode, int DeepOrder)
{
   AcDbObjectId objId;
   AcDbEntity   *pEnt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   int IsInternal = gsc_IsInternalEnt(SelSet, pEnt, Mode, DeepOrder);

   pEnt->close();

   return IsInternal;
}


/*********************************************************/
/*.doc gsc_DeepClone2ModSpace                   <extern> */
/*+
   Duplica l'oggetto grafico in model space.
   Parametri:
   C_SELSET &ss;     gruppo di oggetti grafici

   oppure

   ads_name ent;     oggetto grafico

   Ritorna GS_GOOD se l'oggetto è stato duplicato altrimenti GS_BAD
-*/
/*********************************************************/
int gsc_DeepClone2ModSpace(C_SELSET &ss)
{
   long     i = 0;
   ads_name ent;

   while (ss.entname(i++, ent) == GS_GOOD)
      if (gsc_DeepClone2ModSpace(ent) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int gsc_DeepClone2ModSpace(ads_name ent)
{
   AcDbObjectId      objId;
   AcDbObjectIdArray objList;

   // Add obtained objectId to list of objects to be cloned
   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   objList.append(objId);

   // Get the objectId of the desired owner for the cloned objects.
   // We'll use Model Space for this example.
   AcDbBlockTable *pBlockTable;
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   AcDbObjectId  modelSpaceId;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, modelSpaceId) != Acad::eOk)
      { pBlockTable->close(); return GS_BAD; }
   pBlockTable->close();

   // Create a new ID map
   AcDbIdMapping idMap;

   // Call deepCloneObjects()
   if (acdbHostApplicationServices()->workingDatabase()->deepCloneObjects(objList, modelSpaceId, idMap) != Acad::eOk)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getEntOnXY                         <internal> */
/*+
   Ottengo una nuova entità con tutte le Z = 0.0.

   Parametri:
   ads_name ent;     oggetto grafico

   Ritorna GS_GOOD se l'oggetto è stato duplicato altrimenti GS_BAD
   N.B. Alloca memoria
-*/
/*********************************************************/
int gsc_getEntOnXY(ads_name ent, AcDbEntity **pOutEnt)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_getEntOnXY((AcDbEntity *) pObj, pOutEnt) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_getEntOnXY(AcDbEntity *pInEnt, AcDbEntity **pOutEnt)
{
   if (pInEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {  //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex, *pNewVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        Position;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      pVertIter = ((AcDb2dPolyline *) pInEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pInEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            {
               delete *pOutEnt; 
               delete pVertIter; 
               GS_ERR_COD = eGSInvGraphObjct; 
               return GS_BAD;
            }
         }

         if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex) // punto di controllo
         {
            Position = pVertex->position();
            Position.z = 0.0;
            if ((pNewVertex = new AcDb2dVertex(Position, pVertex->bulge(), 0.0, 0.0, pVertex->tangent())) == NULL)
            {
               if (ToClose) pVertex->close(); 
               delete *pOutEnt;
               delete pVertIter;
               GS_ERR_COD = eGSOutOfMem; 
               return GS_BAD;
            }
            if (((AcDb2dPolyline *)(*pOutEnt))->appendVertex(pNewVertex) != Acad::eOk)
            { 
               if (ToClose) pVertex->close();
               delete pNewVertex;
               delete *pOutEnt;
               delete pVertIter;
               GS_ERR_COD = eGSInvGraphObjct;
               return GS_BAD;
            }
            pNewVertex->close();
         }
         if (ToClose) pVertex->close();
      }
      delete pVertIter;

      if (((AcDb2dPolyline *) pInEnt)->isClosed() == Adesk::kTrue)
         ((AcDb2dPolyline *) (*pOutEnt))->makeClosed();
   }
   else
   if (pInEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex, *pNewVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      
      pVertIter = ((AcDb3dPolyline *) pInEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb3dPolyline *) pInEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            {
               delete pVertIter;
               delete *pOutEnt; 
               GS_ERR_COD = eGSInvGraphObjct; 
               return GS_BAD; 
            }
         }

         Position   = ((AcDb3dPolylineVertex *) pVertex)->position();
         Position.z = 0.0;

         if (ToClose) pVertex->close();

         if ((pNewVertex = new AcDb3dPolylineVertex(Position)) == NULL)
         {
            delete *pOutEnt;
            delete pVertIter;
            GS_ERR_COD = eGSOutOfMem;
            return GS_BAD;
         }
         if (((AcDb3dPolyline *)(*pOutEnt))->appendVertex(pNewVertex) != Acad::eOk)
         { 
            delete pNewVertex;
            delete *pOutEnt;
            delete pVertIter;
            GS_ERR_COD = eGSInvGraphObjct;
            return GS_BAD;
         }
         pNewVertex->close();
      }
      delete pVertIter;

      if (((AcDb3dPolyline *) pInEnt)->isClosed() == Adesk::kTrue)
         ((AcDb3dPolyline *) (*pOutEnt))->makeClosed();
   }
   //else
   //if (pInEnt->isKindOf(AcDb3dSolid::desc()))
   //{
   //}
   else
   if (pInEnt->isKindOf(AcDbArc::desc()))
   {
      AcGePlane Plane;
      if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else
    // da rivedere perchè crea un rettangolo non un blocco - roby
   if (pInEnt->isKindOf(AcDbBlockReference::desc()))
   { //  Group entity
      AcDbExtents      Extent;
      AcGePoint3d      p1, p2;
      AcGePoint3dArray Vertices;

      if (((AcDbBlockReference *) pInEnt)->geomExtentsBestFit(Extent) == Acad::eOk)
      {
         p1 = Extent.minPoint();
         p2 = Extent.maxPoint();
      }
      else
      if (((AcDbBlockReference *) pInEnt)->getGeomExtents(Extent) == Acad::eOk)
      {
         p1 = Extent.minPoint();
         p2 = Extent.maxPoint();
      }
      else
      {
         ads_point pt;

         if (gsc_get_firstPoint((AcDbBlockReference *) pInEnt, pt) == GS_BAD)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         p1.set(pt[X], pt[Y], pt[Z]);
         p2.set(pt[X], pt[Y], pt[Z]);
      }

      Vertices.setLogicalLength(4);
      Vertices[0].set(p1.x, p1.y, 0.0);
      Vertices[1].set(p2.x, p1.y, 0.0);
      Vertices[2].set(p2.x, p2.y, 0.0);
      Vertices[3].set(p1.x, p2.y, 0.0);

      if ((*pOutEnt = new AcDb2dPolyline(AcDb::k2dSimplePoly, Vertices, 0.0, Adesk::kTrue)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   }
   else
   if (pInEnt->isKindOf(AcDbCircle::desc()))
   {
      if (((AcDbCircle *) pInEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0)) // in piano
      {
         AcGePoint3d center;
         if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
            { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         center = ((AcDbCircle *) (*pOutEnt))->center();
         if (center.z != 0.0)
         {
            center.z = 0.0;
            ((AcDbCircle *) (*pOutEnt))->setCenter(center);
         }
      }
      else
      {
         AcGePlane Plane;
         if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else // quotatura piana
   if (pInEnt->isKindOf(AcDbDimension::desc()) &&
       ((AcDbText *) pInEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0))
   {
      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      ((AcDbDimension *) *pOutEnt)->setElevation(0.0);
   }
   else
   if (pInEnt->isKindOf(AcDbEllipse::desc()))
   {
      AcGePlane Plane;
      if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else
   if (pInEnt->isKindOf(AcDbLeader::desc()))
   {
      AcGePoint3d Position;
      int         numVerts = ((AcDbLeader *) pInEnt)->numVertices();

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      for (int i = 0; i < numVerts; i++)
      {
         Position = ((AcDbLeader *) pInEnt)->vertexAt(i);
         Position.z = 0.0;
         if (((AcDbLeader *) (*pOutEnt))->setVertexAt(i, Position) != Adesk::kTrue)
            { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      }
   }
   else
   if (pInEnt->isKindOf(AcDbLine::desc()))
   {
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { delete *pOutEnt; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      Position = ((AcDbLine *) pInEnt)->startPoint();
      Position.z = 0.0;
      ((AcDbLine *) (*pOutEnt))->setStartPoint(Position);

      Position = ((AcDbLine *) pInEnt)->endPoint();
      Position.z = 0.0;
      ((AcDbLine *) (*pOutEnt))->setEndPoint(Position);
   }
   else
   if (pInEnt->isKindOf(AcDbMText::desc()))
   {
      AcGePoint3dArray Vertices;
      AcGePoint3d      Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      ((AcDbMText *) pInEnt)->getBoundingPoints(Vertices);
      Vertices[0].z = 0.0;
      Vertices[1].z = 0.0;
      Vertices[2].z = 0.0;
      Vertices[3].z = 0.0;
      // scambio gli ultimi 2 punti
      Position.set(Vertices[2].x, Vertices[2].y, Vertices[2].z);
      Vertices[2].set(Vertices[3].x, Vertices[3].y, Vertices[3].z);
      Vertices[3].set(Position.x, Position.y, Position.z);

      if ((*pOutEnt = new AcDb2dPolyline(AcDb::k2dSimplePoly, Vertices, 0.0, Adesk::kTrue)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   }
   else
   if (pInEnt->isKindOf(AcDbPoint::desc()))
   {
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

      Position = ((AcDbPoint *) pInEnt)->position();
      Position.z = 0.0;
      ((AcDbPoint *) (*pOutEnt))->setPosition(Position);
   }
   //else
   //if (pInEnt->isKindOf(AcDbPolyFaceMesh::desc()))
   //{ //  Group entity
   //}
   //else
   //if (pInEnt->isKindOf(AcDbPolygonMesh::desc()))
   //{ //  Group entity
   //}
   else
   if (pInEnt->isKindOf(AcDbPolyline::desc()))
   {
      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      ((AcDbPolyline *) *pOutEnt)->setElevation(0.0);
   }
   else
   if (pInEnt->isKindOf(AcDbMline::desc()))
   {
      int         NumVerts = ((AcDbMline *) pInEnt)->numVertices();
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }    

      for (int i = 0; i < NumVerts; i++)
      {
         Position = ((AcDbMline *) pInEnt)->vertexAt(i);
         Position.z = 0;
         ((AcDbMline *) pInEnt)->moveVertexAt(i, Position);
      }
   }
   else
   if (pInEnt->isKindOf(AcDbRay::desc()))
   {
      AcGePlane Plane;
      if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else
   if (pInEnt->isKindOf(AcDbSolid::desc())) // polygon 2D
   {
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      ((AcDbSolid *) *pOutEnt)->getPointAt(0, Position);
      Position.z = 0.0;
      ((AcDbSolid *) *pOutEnt)->setPointAt(0, Position);

      ((AcDbSolid *) *pOutEnt)->getPointAt(1, Position);
      Position.z = 0.0;
      ((AcDbSolid *) *pOutEnt)->setPointAt(1, Position);

      ((AcDbSolid *) *pOutEnt)->getPointAt(2, Position);
      Position.z = 0.0;
      ((AcDbSolid *) *pOutEnt)->setPointAt(2, Position);

      ((AcDbSolid *) *pOutEnt)->getPointAt(3, Position);
      Position.z = 0.0;
      ((AcDbSolid *) *pOutEnt)->setPointAt(3, Position);
   }
   else
   if (pInEnt->isKindOf(AcDbFace::desc())) // polygon 2D
   {
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      ((AcDbFace *) *pOutEnt)->getVertexAt(0, Position);
      Position.z = 0.0;
      ((AcDbFace *) *pOutEnt)->setVertexAt(0, Position);

      ((AcDbFace *) *pOutEnt)->getVertexAt(1, Position);
      Position.z = 0.0;
      ((AcDbFace *) *pOutEnt)->setVertexAt(1, Position);

      ((AcDbFace *) *pOutEnt)->getVertexAt(2, Position);
      Position.z = 0.0;
      ((AcDbFace *) *pOutEnt)->setVertexAt(2, Position);

      ((AcDbFace *) *pOutEnt)->getVertexAt(3, Position);
      Position.z = 0.0;
      ((AcDbFace *) *pOutEnt)->setVertexAt(3, Position);
   }
   else
   if (pInEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePlane Plane;
      if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else // Testo piano
   if (pInEnt->isKindOf(AcDbText::desc()) && 
       ((AcDbText *) pInEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0))
   {
      AcGePoint3d Position;

      if ((*pOutEnt = (AcDbEntity *) pInEnt->isA()->create()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if ((*pOutEnt)->copyFrom(pInEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      Position = ((AcDbText *) pInEnt)->position();
      Position.z = 0.0;
      ((AcDbText *) (*pOutEnt))->setPosition(Position);
   }
   else
   if (pInEnt->isKindOf(AcDbXline::desc()))
   {
      AcGePlane Plane;
      if (((AcDbCurve *) pInEnt)->getOrthoProjectedCurve(Plane, (AcDbCurve*&) *pOutEnt) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }
   else // ricavo le estensioni
   {
      AcDbExtents      Extent;
      AcGePoint3d      p1, p2;
      AcGePoint3dArray Vertices;

      if (pInEnt->getGeomExtents(Extent) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      p1 = Extent.minPoint();
      p2 = Extent.maxPoint();

      Vertices.setLogicalLength(4);
      Vertices[0].set(p1.x, p1.y, 0.0);
      Vertices[1].set(p2.x, p1.y, 0.0);
      Vertices[2].set(p2.x, p2.y, 0.0);
      Vertices[3].set(p1.x, p2.y, 0.0);

      if ((*pOutEnt = new AcDb2dPolyline(AcDb::k2dSimplePoly, Vertices, 0.0, Adesk::kTrue)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_MoveEntOnXY                        <internal> */
/*+
   Sposta le coordinate dell'entità di un OffSet per l'asse X e Y.
   Parametri:
   ads_name ent;     oggetto grafico
   double OffSetX;   OffSet asse X (default = 0)
   double OffSetY;   OffSet asse Y (default = 0)
   int Dec;          Opzionale, se diverso da >=0 le coordinate vengono 
                     approssimate al n di decimali specificato (default = -1)

   Ritorna GS_GOOD se l'oggetto è stato duplicato altrimenti GS_BAD
-*/
/*********************************************************/
int gsc_MoveEntOnXY(AcDbEntity *pEnt, double OffSetX, double OffSetY, int Dec)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   { //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      AcGePoint3d        Position;
      AcDbEntity         *pVertexEnt;
      int                ToClose;

      pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb2dVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb2dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            {
               delete pVertIter; 
               GS_ERR_COD = eGSInvGraphObjct; 
               return GS_BAD;
            }
         }

         Position = pVertex->position();
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         pVertex->setPosition(Position);

         if (ToClose) pVertex->close();
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   { //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      AcGePoint3d          Position;
      AcDbEntity           *pVertexEnt;
      int                  ToClose;
      
      pVertIter = ((AcDb3dPolyline *) pEnt)->vertexIterator();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         if ((pVertexEnt = pVertIter->entity()) != NULL)
         {
            ToClose = FALSE;
            pVertex = (AcDb3dPolylineVertex *) pVertexEnt;
         }
         else
         {
            ToClose = TRUE;
            vertexObjId = pVertIter->objectId();
            if (((AcDb3dPolyline *) pEnt)->openVertex(pVertex, vertexObjId, AcDb::kForRead) != Acad::eOk)
            {
               delete pVertIter;
               GS_ERR_COD = eGSInvGraphObjct; 
               return GS_BAD; 
            }
         }

         Position = ((AcDb3dPolylineVertex *) pVertex)->position();
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         pVertex->setPosition(Position);

         if (ToClose) pVertex->close();
      }
      delete pVertIter;
   }
   //else
   //if (pEnt->isKindOf(AcDb3dSolid::desc()))
   //{
   //}
   else
   if (pEnt->isKindOf(AcDbArc::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbArc *) pEnt)->center();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbArc *) pEnt)->setCenter(Position);
   }
   else
    // da rivedere perchè crea un rettangolo non un blocco - roby
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   { //  Group entity
      AcGePoint3d Position;

      Position = ((AcDbBlockReference *) pEnt)->position();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbBlockReference *) pEnt)->setPosition(Position);
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbCircle *) pEnt)->center();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbCircle *) pEnt)->setCenter(Position);
   }
   else // quotatura piana
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbDimension *) pEnt)->dimBlockPosition();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbDimension *) pEnt)->setDimBlockPosition(Position);
   }
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbEllipse *) pEnt)->center();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbEllipse *) pEnt)->setCenter(Position);
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      AcGePoint3d Position;
      int         numVerts = ((AcDbLeader *) pEnt)->numVertices();

      for (int i = 0; i < numVerts; i++)
      {
         Position = ((AcDbLeader *) pEnt)->vertexAt(i);
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         if (((AcDbLeader *) pEnt)->setVertexAt(i, Position) != Adesk::kTrue)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbLine *) pEnt)->startPoint();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbLine *) pEnt)->setStartPoint(Position);

      Position = ((AcDbLine *) pEnt)->endPoint();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbLine *) pEnt)->setEndPoint(Position);
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbMText *) pEnt)->location();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbMText *) pEnt)->setLocation(Position);
   }
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbPoint *) pEnt)->position();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbPoint *) pEnt)->setPosition(Position);
   }
   //else
   //if (pInEnt->isKindOf(AcDbPolyFaceMesh::desc()))
   //{ //  Group entity
   //}
   //else
   //if (pInEnt->isKindOf(AcDbPolygonMesh::desc()))
   //{ //  Group entity
   //}
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      AcGePoint2d Position;
      int         NumVerts = ((AcDbPolyline *) pEnt)->numVerts();

      for (int i = 0; i < NumVerts; i++)
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, Position);
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         ((AcDbPolyline *) pEnt)->setPointAt(i, Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      AcGePoint3d Position;
      int         numVerts = ((AcDbLeader *) pEnt)->numVertices();

      for (int i = 0; i < numVerts; i++)
      {
         Position = ((AcDbMline *) pEnt)->vertexAt(i);
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         if (((AcDbMline *) pEnt)->moveVertexAt(i, Position) != Adesk::kTrue)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     
      }
   }
   else
   if (pEnt->isKindOf(AcDbRay::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbRay *) pEnt)->basePoint();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbRay *) pEnt)->setBasePoint(Position);
   }
   else
   if (pEnt->isKindOf(AcDbSolid::desc())) // polygon 2D
   {
      AcGePoint3d Position;

      ((AcDbSolid *) pEnt)->getPointAt(0, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbSolid *) pEnt)->setPointAt(0, Position);

      ((AcDbSolid *) pEnt)->getPointAt(1, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbSolid *) pEnt)->setPointAt(1, Position);

      ((AcDbSolid *) pEnt)->getPointAt(2, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbSolid *) pEnt)->setPointAt(2, Position);

      ((AcDbSolid *) pEnt)->getPointAt(3, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbSolid *) pEnt)->setPointAt(3, Position);
   }
   else
   if (pEnt->isKindOf(AcDbFace::desc())) // polygon 2D
   {
      AcGePoint3d Position;

      ((AcDbFace *) pEnt)->getVertexAt(0, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbFace *) pEnt)->setVertexAt(0, Position);

      ((AcDbFace *) pEnt)->getVertexAt(1, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbFace *) pEnt)->setVertexAt(1, Position);

      ((AcDbFace *) pEnt)->getVertexAt(2, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbFace *) pEnt)->setVertexAt(2, Position);

      ((AcDbFace *) pEnt)->getVertexAt(3, Position);
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbFace *) pEnt)->setVertexAt(3, Position);
   }
   else
   if (pEnt->isKindOf(AcDbSpline::desc()))
   {
      AcGePoint3d Position;
      int         NumVerts = ((AcDbSpline *) pEnt)->numFitPoints();

      for (int i = 0; i < NumVerts; i++)
	   {
		   ((AcDbSpline *) pEnt)->getFitPointAt(i, Position);
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         ((AcDbSpline *) pEnt)->setFitPointAt(i, Position);
      }

      NumVerts = ((AcDbSpline *) pEnt)->numControlPoints();
      for (int i = 0; i < NumVerts; i++)
	   {
		   ((AcDbSpline *) pEnt)->getControlPointAt(i, Position);
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         ((AcDbSpline *) pEnt)->setControlPointAt(i, Position);
      }
   }
   else // Testo piano
   if (pEnt->isKindOf(AcDbText::desc()) && 
       ((AcDbText *) pEnt)->normal() == AcGeVector3d(0.0, 0.0, 1.0))
   {
      AcGePoint3d Position;

      if (((AcDbText *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
          ((AcDbText *) pEnt)->verticalMode() == AcDb::kTextBase)
      {
         Position = ((AcDbText *) pEnt)->position();
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         ((AcDbText *) pEnt)->setPosition(Position);
      }
      else
      {
         Position = ((AcDbText *) pEnt)->alignmentPoint();
         Position.x = gsc_truncate(Position.x + OffSetX, Dec);
         Position.y = gsc_truncate(Position.y + OffSetY, Dec);
         ((AcDbText *) pEnt)->setAlignmentPoint(Position);
      }
   }
   else
   if (pEnt->isKindOf(AcDbXline::desc()))
   {
      AcGePoint3d Position;

      Position = ((AcDbRay *) pEnt)->basePoint();
      Position.x = gsc_truncate(Position.x + OffSetX, Dec);
      Position.y = gsc_truncate(Position.y + OffSetY, Dec);
      ((AcDbRay *) pEnt)->setBasePoint(Position);
   }
   else // ricavo le estensioni
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }     

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ReleaseSubEnts                     <external> */
/*+
   Per le entità complesse (es. “AcDb3dPolyline”) prima di distuggere l'oggetto c++
   bisogna esplicitamente distruggere le sotto-entità quando l'entità complessa 
   non è database resident.
   Parametri:
   AcDbEntity *pEnt;

   Ritorna GS_GOOD se l'oggetto è stato duplicato altrimenti GS_BAD
-*/
/*********************************************************/
int gsc_ReleaseSubEnts(AcDbEntity *pEnt)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      AcDbObjectIterator *pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      if (!pVertIter) return GS_GOOD;
      while (pVertIter->done() == false)
      {
          AcDbObjectId vertexObjId = pVertIter->objectId();

          if (vertexObjId == AcDbObjectId::kNull) // non è database resident
          {
             AcDbEntity *pVertexEnt = pVertIter->entity();
             if (pVertexEnt) delete pVertexEnt;
          }
          pVertIter->step();                
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter = ((AcDb3dPolyline *) pEnt)->vertexIterator();

      if (!pVertIter) return GS_GOOD;
      while (pVertIter->done() == false)
      {
          AcDbObjectId vertexObjId = pVertIter->objectId();

          if (vertexObjId == AcDbObjectId::kNull) // non è database resident
          {
             AcDbEntity *pVertexEnt = pVertIter->entity();
             if (pVertexEnt) delete pVertexEnt;
          }
          pVertIter->step();                
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {
      AcDbObjectIterator *pAttrIter = pAttrIter = ((AcDbBlockReference *) pEnt)->attributeIterator();

      if (!pAttrIter) return GS_GOOD;
      while (pAttrIter->done() == false)
      {
          AcDbObjectId attrObjId = pAttrIter->objectId();

          if (attrObjId == AcDbObjectId::kNull) // non è database resident
          {
             AcDbEntity *pAttrEnt = pAttrIter->entity();
             if (pAttrEnt) delete pAttrEnt;
          }
          pAttrIter->step();                
      }
      delete pAttrIter;
   }

   return GS_GOOD;
}


/*********************************************************/
/*  INIZIO FUNZIONI PER I RIEMPIMENTI                    */
/*********************************************************/


/*****************************************************************************/
/*.doc gsc_GetAssociativeHatch                                               */
/*+                                                                       
  Legge la lista dei riempimenti associati ad un oggetto grafico.
  Parametri:
  ads_name ent;      oggetto grafico
  int      cls;      Codice classe per eventuale filtro (opzionale, default = 0)
  int      sub;      Codice sotto-classe per eventuale filtro (opzionale, default = 0)

  Ritorna una lista di GS_GOOD in caso di inserimento corretto altrimenti GS_BAD.
-*/
/*****************************************************************************/
presbuf gsc_GetAssociativeHatch(ads_name ent, int cls, int sub)
{
   C_RB_LIST rb_list, rb_react_list, result;
   presbuf   p;
   ads_name  hatch;
   C_EED     eed;

   if (!ent || ads_name_nil(ent)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
   if ((rb_list << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if ((p = rb_list.SearchType(0)) == NULL || p->resval.rstring == NULL) 
      { GS_ERR_COD = eGSInvRBType; return NULL; }
   // se non è una polilinea o una light weight polyline
   if (gsc_strcmp(_T("POLYLINE"), p->resval.rstring) != 0 &&
       gsc_strcmp(_T("LWPOLYLINE"), p->resval.rstring) != 0)
      { GS_ERR_COD = eGSInvGraphObjct; return NULL; }

   // ricavo la lista delle entità che sono collegate tramite reattore
   p = rb_list.SearchType(102); // cerca dall'inizio
   while (p)
   {
      // inizio sequenza
      if (p->resval.rstring && gsc_strcmp(_T("{ACAD_REACTORS"), p->resval.rstring) == 0)
      {
         while ((p = rb_list.get_next()) != NULL)
         {
            if (p->restype == 102 && 
                p->resval.rstring && gsc_strcmp(_T("}"), p->resval.rstring) == 0)
               break; // fine sequenza

            if (p->restype == 330)
            {
               ads_name_set(p->resval.rlname, hatch);
               if ((rb_react_list << ads_entgetx(p->resval.rlname, GEOsimAppl::APP_ID_LIST.get_head())) == NULL)
                  { GS_ERR_COD = eGSInvEntityOp; return NULL; }
               if ((p = rb_react_list.SearchType(0)) == NULL || p->resval.rstring == NULL) 
                  { GS_ERR_COD = eGSInvRBType; return NULL; }
               // se è un riempimento
               if (gsc_strcmp(_T("HATCH"), p->resval.rstring) == 0)
               {
                  if (cls != 0) // applico filtro per classe
                  {
                     if ((p = rb_react_list.SearchType(-3)) != NULL && // cerco intestazione EED
                         (p = gsc_EEDsearch(GEO_APP_ID, p)) != NULL && // cerco l'etichetta di GEOsim
                         eed.load(p) == GS_GOOD && eed.cls == cls &&   // carico i dati e li confronto
                         eed.sub == sub)                               // con i codici della classe
                        // aggiungo alla lista
                        if ((result += acutBuildList(RTENAME, hatch, 0)) == NULL)
                           return NULL;
                  }
                  else // aggiungo alla lista
                     if ((result += acutBuildList(RTENAME, hatch, 0)) == NULL)
                        return NULL;
               }
            }
         }
      }

      // Cerco dalla posizione del cursore successiva a quella attuale
      p = rb_list.SearchNextType(102);
   }

   result.ReleaseAllAtDistruction(GS_BAD);

   return result.get_head();
}


/*****************************************************************************/
/*.doc gsc_setHatchSS                                                        */
/*+                                                                       
  Setta un unico riempimento NON associativo per le polilinee chiuse 
  appartenenti al gruppo di selezione.
  Parametri:
  ads_name selset;      gruppo di selezione
  TCHAR    *hatch;      nome riempimento
  double   scale;       scala     (default = 1)
  double   rotation;    rotazione in gradi (default = 0)
  C_COLOR *color;       colore (default = NULL, colore corrente)
  const TCHAR *layer;   nome layer (default = NULL, layer corrente)
  C_SELSET *pResultSS;  gruppo di selezione riempimenti creati
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di successo, GS_CAN se l'area è troppo piccola
  per contenere un tratteggio, altrimenti GS_BAD.
-*/
/*****************************************************************************/
int gsc_setHatchSS(C_SELSET &selset, TCHAR *hatch, double scale, double rotation,
                   C_COLOR *color, const TCHAR *layer,
                   C_SELSET *pResultSS, AcDbBlockTableRecord *pBlockTableRecord)
{
   ads_name _selset;
   selset.get_selection(_selset);
   return gsc_setHatchSS(_selset, hatch, scale, rotation, color, layer,
                         pResultSS, pBlockTableRecord);
}
AcDbHatch *gsc_create_hatch(AcDbEntityPtrArray Ents, TCHAR *hatch, double scale,
                            double rotation, C_COLOR *color, const TCHAR *layer)
{
   AcDbHatch         *pHatch = new AcDbHatch();
   C_STRING          SolidFill;
   AcDbEntity        *pEnt;
   long              i;
   Acad::ErrorStatus err;
   AcGePoint2dArray  vertices;
   AcGeDoubleArray   bulges;
   C_POINT_LIST      VertexList;

   if (!hatch || gsc_strlen(hatch) == 0) return NULL;

   // Set hatch plane
   AcGeVector3d normal(0.0, 0.0, 1.0);
   pHatch->setNormal(normal);
   pHatch->setElevation(0.0);

   // Set hatch color
   if (color)
      gsc_set_color(pHatch, *color);

   // Set hatch layer
   if (layer && gsc_strlen(layer) > 0)
      if (gsc_setLayer(pHatch, layer) != GS_GOOD)
         { delete pHatch; return NULL; }

   // Set non associative hatch
   pHatch->setAssociative(Adesk::kFalse);

   // Set hatch pattern to SolidFill type
   if (gsc_strcmp(hatch, _T("_SOLID"), FALSE) == 0 || // Versione internazionale
      gsc_strcmp(hatch, gsc_msg(328), FALSE) == 0) // "SOLIDO"
      SolidFill = _T("SOLID");
   else
      SolidFill = hatch;

   // Set hatch style to kNormal
   pHatch->setHatchStyle(AcDbHatch::kNormal);

   // Set pattern scale
   if (scale > 0) pHatch->setPatternScale(scale);

   // Set pattern scale (converto da gradi a radianti)
   pHatch->setPatternAngle(gsc_grd2rad(rotation));

   if (pHatch->setPattern(AcDbHatch::kPreDefined, SolidFill.get_name()) != Acad::eOk)
      { delete pHatch; return NULL; }

   // Ciclo sugli oggetti
   for (i = 0; i < Ents.length(); i++)
   {
      pEnt = Ents.at(i);

      if (pEnt->isKindOf(AcDbMPolygon::desc())) continue; // scarto i poligoni

      VertexList.remove_all();

      // Ricavo la lista dei vertici e dei bulge
      if (VertexList.add_vertex_point(pEnt, GS_GOOD) == GS_BAD)
         { delete pHatch; return NULL; }
      
      if (VertexList.toAcGeArray(vertices, bulges) == GS_BAD)
         { delete pHatch; return NULL; }

      // Append objs to hatch
      if (pHatch->appendLoop(AcDbHatch::kDefault, vertices, bulges) != Acad::eOk)
         { delete pHatch; return NULL; }
   }

   // Elaborate solid fill
   if ((err = pHatch->evaluateHatch(true)) != Acad::eOk)
      { delete pHatch; return NULL; }

   return pHatch;
}
int gsc_setHatchSS(ads_name selset, TCHAR *hatch, double scale, double rotation,
                   C_COLOR *color, const TCHAR *layer,
                   C_SELSET *pResultSS, AcDbBlockTableRecord *pBlockTableRecord)
{   
   if (!hatch || gsc_strlen(hatch) == 0) return GS_GOOD;

   ads_name             LastBefore, LastAfter;
   AcDbHatch            *pHatch;
   C_STRING             SolidFill;
   long                 i = 0;
   double               PatternAngle = gsc_grd2rad(rotation);
   AcDbBlockTableRecord *pInternalBlockTableRecord;
   C_SELSET             dummy;
   AcDbEntity           *pEnt;
   AcDbEntityPtrArray   Ents;
   AcDbEntityPtrArray   MPolygons;

   acdbEntLast(LastBefore);
   
   dummy << selset;
   dummy.ReleaseAllAtDistruction(GS_BAD);
   dummy.get_AcDbEntityPtrArray(Ents); // aapre gli oggetti in lettura

   // separo gli mpolygon
   for (i = 0; i < Ents.length(); i++)
   {
      pEnt = Ents.at(i);

      if (pEnt->isKindOf(AcDbMPolygon::desc()))
      {
         MPolygons.append(pEnt);
         Ents.removeAt(i); i--; // non incremento il contatore
      }
   }

   if (gsc_close_AcDbEntities(Ents) == GS_BAD) return GS_BAD;

   if (Ents.length() > 0)
   {
      if ((pHatch = gsc_create_hatch(Ents, hatch, scale, rotation, color, layer)) == NULL)
         return GS_CAN;

      if (!pBlockTableRecord)
      {
         // Post hatch entity to database
         AcDbBlockTable       *pBlockTable;

         if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
            { delete pHatch; GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
            { delete pHatch; pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

         pBlockTable->close();
      }
      else
         pInternalBlockTableRecord = pBlockTableRecord;

      pInternalBlockTableRecord->appendAcDbEntity(pHatch);
      pHatch->close();

      if (!pBlockTableRecord) pInternalBlockTableRecord->close();

      acdbEntLast(LastAfter);
      if (ads_name_equal(LastBefore, LastAfter)) // area troppo piccola 
         return GS_CAN;                        // per contenere il riempimento
   }

   for (i = 0; i < MPolygons.length(); i++)
      if (gsc_set_hatch(MPolygons.at(i), hatch, &rotation, &scale, color, layer) != GS_GOOD)
         return GS_CAN;

   if (pResultSS)
   {
      pResultSS->clear();
      // creo gruppo di selezione delle entità nuove generate da "BHATCH"
      while (gsc_mainentnext(LastBefore, LastBefore) == GS_GOOD)
         if (pResultSS->add(LastBefore) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_setHatchEnt                                                       */
/*+                                                                       
  Setta un riempimento NON associativo all'oggetto grafico (una polilinea chiusa).
  Parametri:
  ads_name ent;         oggetto grafico
  TCHAR    *hatch;      nome riempimento
  double   scale;       scala     (default = 1)
  double   rotation;    rotazione in gradi (default = 0)
  C_COLOR *color;       colore (default = NULL, colore corrente)
  const TCHAR *layer;   nome layer (default = NULL, layer corrente)
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di successo, GS_CAN se l'area è troppo piccola
  per contenere un tratteggio, altrimenti GS_BAD.
-*/
/*****************************************************************************/
int gsc_setHatchEnt(ads_name ent, TCHAR *hatch, double scale, double rotation,
                    C_COLOR *color, const TCHAR *layer, AcDbBlockTableRecord *pBlockTableRecord)
{
   if (gsc_ismpolygon(ent) == GS_GOOD)
      return gsc_set_hatch(ent, hatch, &rotation, &scale, color, layer);
   else
   {
      C_SELSET selset;

      selset.add(ent);
      return gsc_setHatchSS(selset, hatch, scale, rotation, color, layer, 
                            NULL, pBlockTableRecord);
   }
}


/*****************************************************************************/
/*.doc gsc_UpdToDefHatch                                          <internal> */
/*+                                                                       
  Cancella tutti i riempimenti del gruppo di selezione e riempie
  tutte le polilinee chiuse del gruppo con il riempimento impostato.
  La funzione è da usare con un gruppo di selezione degli oggetti appartenenti
  alla stessa entità GEOsim considerata aggiornabile.
  Nel caso di spaghetti inserisce nuovi riempimenti.
  Parametri:
  C_SELSET &SelSet;     Gruppo di Selezione
  TCHAR    *hatch;      nome riempimento
  double   scale;       scala     (default = 1)
  double   rotation;    rotazione in gradi (default = 0)
  C_COLOR  *color;      colore    (default = NULL -> "colore corrente")
  const TCHAR *layer;   piano     (default = NULL -> "layer corrente")

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
  N.B. viene modificato il SelSet !!!
-*/
/*****************************************************************************/
int gsc_UpdToDefHatch(C_SELSET &SelSet, TCHAR *hatch, double scale,
                      double rotation, C_COLOR *color, const TCHAR *layer)
{
   C_SELSET  HatchToIns, MPolygonSS;
   ads_name  Ent, Last;
   long      i, key, Qty;
   C_RB_LIST DescrEnt;
   int       Result = GS_BAD, prj;
   C_EED     eed;
   C_CLASS   *pCls;
   C_LINK    Link;
   TCHAR     Type[MAX_LEN_GRAPH_TYPE];

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   prj = GS_CURRENT_WRK_SESSION->get_PrjId();

   do
   {
      // ciclo sugli oggetti grafici per trovare il primo valido
      // alcuni oggetti potrebbero essere stati cancellati (vedi gsc_class_align)
      i = 0;
      while (SelSet.entname(i++, Ent) == GS_GOOD)
         if ((DescrEnt << acdbEntGet(Ent)) && gsc_graph_type(Ent, Type) == GS_GOOD)
            if (gsc_strcmp(_T("CIRCLE"), Type) == 0 || gsc_strcmp(_T("ELLIPSE"), Type) == 0 ||
                gsc_isClosedPline(Ent) == GS_GOOD || gsc_strcmp(_T("MPOLYGON"), Type) == 0)
               break;
         
      if (i > SelSet.length()) { Result = GS_GOOD; break; }

      if (eed.load(Ent) == GS_BAD)
         { GS_ERR_COD = eGSGEOsimObjNotFound; break; }
      // Cerco caratteristiche classe
      if ((pCls = gsc_find_class(prj, eed.cls, eed.sub)) == NULL) break;

      // Caso spaghetti
      if (pCls->get_category() == CAT_SPAGHETTI)
      {
         C_SELSET ResultSS;

         if (gsc_strcmp(_T("MPOLYGON"), Type) != 0) // se non era poligono
         {
            // aggiungo al gruppo di selezione dei riempimenti da inserire
            if (HatchToIns.add(Ent) == GS_BAD) { Result = GS_BAD; break; }
            // Inserisco nuovi riempimenti
            // se ritorna GS_CAN la superficie era troppo piccola per
            // contenere il riempimento
            if ((Result = gsc_setHatchSS(HatchToIns, hatch, scale, rotation, color, layer,
                                         &ResultSS)) == GS_BAD)
               break;

            i = 0;
            while (ResultSS.entname(i++, Last) == GS_GOOD)
            {
               if ((Result = pCls->ins_data(Last, DescrEnt)) == GS_BAD) break;
               // Se non si tratta di spaghetti e la grafica è in tabella DB
               if (pCls->get_category() != CAT_SPAGHETTI && 
                   pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
               {
                  C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pCls->ptr_GphInfo();
                  TCHAR        Handle[MAX_LEN_HANDLE];
                  C_BSTR_REAL  *pHandleId;

                  // ricavo l'handle del contorno
                  gsc_enthand(Ent, Handle);
                  // Verifico se il contorno è già stato estratto
                  if ((pHandleId = (C_BSTR_REAL *) pGphInfo->HandleId_LinkTree.search(Handle)))
                  {
                     // ricavo l'handle del riempimento
                     gsc_enthand(Last, Handle);
                     // aggiorno la lista in memoria delle coppie <handle>-<id> e 
                     // <id>-<puntatore a <handle-id>> per marcare che il riempimento è
                     // il secondo oggetto grafico generato dalla stessa riga della tabella
                     // che memorizza il contorno (il poligono in SQL)
                     pGphInfo->AddToLinkTree(Handle, pHandleId->get_key()+ 0.02, GRAPHICAL);
                  }
               }
            }
         }
         else // era un mpolygon
         {
            C_RB_LIST dummy;

            if ((Result = gsc_set_hatch(Ent, hatch, &rotation, &scale)) == GS_BAD)
               break;
            // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
            if ((Result = pCls->upd_data(Ent, dummy)) == GS_BAD) break;
         }
         break;
      }

      // leggo la chiave
      if (pCls->getKeyValue(Ent, &key) == GS_BAD) break;

      // ciclo sugli oggetti grafici
      Result = GS_GOOD;
      i = 0;
      while (SelSet.entname(i, Ent) == GS_GOOD)
      {
         // alcuni oggetti potrebbero essere stati cancellati (vedi gsc_class_align)
         if ((DescrEnt << acdbEntGet(Ent)) == NULL) 
         {
            i++; // vado al successivo
            continue;
         }

         gsc_graph_type(Ent, Type);

         if (gsc_strcmp(_T("CIRCLE"), Type) == 0 || gsc_strcmp(_T("ELLIPSE"), Type) == 0 ||
             gsc_isClosedPline(Ent) == GS_GOOD)
         {
            // aggiungo al gruppo di selezione dei riempimenti da inserire
            if (HatchToIns.add(Ent) == GS_BAD) { Result = GS_BAD; break; }
            i++; // vado al successivo
         }
         else
         if (gsc_strcmp(_T("MPOLYGON"), Type) == 0)
         {
            // aggiungo al gruppo di selezione degli mpolygon da trattare
            if (MPolygonSS.add(Ent) == GS_BAD) { Result = GS_BAD; break; }
            i++; // vado al successivo
         }
         else
         {
            if (gsc_strcmp(Type, _T("HATCH")) == 0)
            {  
               // aggiungo nel gruppo di selezione GEOsimAppl::SAVE_SS
               if (gsc_addEnt2savess(Ent) == GS_BAD) { Result = GS_BAD; break; }
               // Cancello l'entità dal gruppo di selezione
               if (SelSet.subtract_ent(Ent) == GS_BAD) { Result = GS_BAD; break; }
               // Cancello il riempimento
               if (gsc_EraseEnt(Ent) != GS_GOOD) { Result = GS_BAD; break; }
            }
            else
               i++; // vado al successivo
         }
      }
      if (Result == GS_BAD) break;
      Result = GS_BAD;

      if (hatch && wcslen(hatch) > 0)
      {
         C_SELSET ResultSS;

         i = 0;
         // Inserisco nuovi riempimenti
         // se ritorna GS_CAN la superficie era troppo piccola per
         // contenere il riempimento
         if ((Result = gsc_setHatchSS(HatchToIns, hatch, scale, rotation, color, layer,
                                      &ResultSS)) == GS_BAD)
            break;

         if (Result == GS_GOOD)
            while (ResultSS.entname(i++, Last) == GS_GOOD)
            {
               if (Link.Set(Last, pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                            key, INSERT) == GS_BAD)
                  { Result = GS_BAD; break; }
               // Aggiungo il nuovo riempimento nel SelSet
               if (SelSet.add(Last) == GS_BAD) { Result = GS_BAD; break; }
            }
         else Result = GS_GOOD;

         if (Result == GS_BAD) break;

         // Aggiorno i riempimenti dei polygoni
         i = 0;
         while (MPolygonSS.entname(i++, Last) == GS_GOOD)
            if ((Result = gsc_set_hatch(Last, hatch, &rotation, &scale)) == GS_BAD)
               break;

         if (Result == GS_BAD) break;
         Result = GS_BAD;
      }

      // ricavo il numero di oggetti del gruppo di selezione
      if ((Qty = SelSet.length()) <= 0) break;
      eed.num_el = Qty;
      // inserisco etichette "entità di GEOsim" agli oggetti grafici
      // che vengono aggiunti in GEOsimAppl::SAVE_SS per salvataggio
      if (eed.save_selset(SelSet) == GS_BAD) break;

      Result = GS_GOOD;
   }
   while (0);
      
   return Result;
}


/*********************************************************/
/*  FINE FUNZIONI PER I RIEMPIMENTI                      */
/*********************************************************/


/*********************************************************/
/*.doc gsc_insert_pline <external> */
/*+ 
  Richiama il comando di inserimento di una polilinea di Autocad.
  Se point e' diverso da NULL point sara' il punto di inserimento
  dell'entita' altrimenti verra richiesto dal comando.
  Parametri:
  C_POINT_LIST &pointList;  Lista dei punti della pline
  const TCHAR *Layer;       (default = NULL)
  C_COLOR *Color;           (default = NULL -> "colore corrente")
  const TCHAR *LineType;    (default = NULL)
  double Scale;             (default -1)
  double Width;             (default -1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_insert_pline(ads_point p1, ads_point p2, const TCHAR *Layer, C_COLOR *Color,
                     const TCHAR *LineType, double Scale, double Width)
{
   C_POINT_LIST PointList;

   if (PointList.add_point(p1) == NULL) return GS_BAD;
   if (PointList.add_point(p2) == NULL) return GS_BAD;

   return gsc_insert_pline(PointList, Layer, Color, LineType, Scale, Width);
}
int gsc_insert_pline(C_POINT_LIST &PointList, const TCHAR *Layer, C_COLOR *Color,
                     const TCHAR *LineType, double Scale, double Width)
{
   AcDbBlockTable       *pBlockTable;
   AcDbBlockTableRecord *pInternalBlockTableRecord;
   AcDbCurve            *pEnt;

   if (PointList.get_count() < 2) return GS_BAD;

   // Open the block table for read.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   if ((pEnt = PointList.toPolyline(Layer, Color, LineType, Scale, Width)))
   {
      pInternalBlockTableRecord->appendAcDbEntity(pEnt);
      pEnt->close();
   }
   pInternalBlockTableRecord->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_insert_pline <external> */
/*+ 
  Richiama il comando di inserimento di una polilinea di Autocad.
  Se point e' diverso da NULL point sara' il punto di inserimento
  dell'entita' altrimenti verra richiesto dal comando.
  Parametri:
  ads_point startPoint;	         Punto di inizio (opzionale)
  C_CLASS *ClassToCheckOverlap;  usato solo se point = NULL e se l'inserimento
                                 deve controllare la sovrapposizione 
                                 (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_insert_pline(ads_point startPoint, C_CLASS *ClassToCheckOverlap)
{
   int       rc;
   ads_name  ent;
   ads_point firstPoint, lastPoint, pt;

   if (!startPoint)
   {
      do
      {  // "\nSpecificare punto iniziale: "
         while ((rc = acedGetPoint(NULL, gsc_msg(1), pt)) == RTNONE)
            acutPrintf(gsc_msg(107)); // "\nPunto non valido."
         if (rc == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         if (rc == RTCAN) return GS_CAN;
         if (ClassToCheckOverlap) // se e si devono controllare le sovrapposizioni
         {
            // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
            if (gsc_OverlapValidation(pt, ClassToCheckOverlap) == GS_GOOD) break;
            acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
         }
         break;
      }
      while (1);
      ads_point_set(pt, firstPoint);
   }
   else
      ads_point_set(startPoint, firstPoint);

   ads_point_set(firstPoint, lastPoint);
   int  count = 1;
   C_STRING Msg;
   do
   {
      if (count > 1)
      {
         acedInitGet(0, gsc_msg(1094)); // _T("ANnulla")
         Msg = gsc_msg(1093); // "\nSpecificare punto successivo o [ANnulla]: "
      }
      else
         Msg = gsc_msg(1054); // "\nSpecificare punto successivo: "

      if ((rc = acedGetPoint(lastPoint, Msg.get_name(), pt)) == RTNONE || rc == RTCAN)
      {
         if (count == 1) return GS_CAN;
         return GS_GOOD;
      }

      if (rc == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      else if (rc == RTKWORD)
      {
         if (count > 2)
         {
            if (gsc_delLastVertex(ent) != GS_GOOD) return GS_BAD;
            if (gsc_get_lastPoint(ent, lastPoint) != GS_GOOD) return GS_BAD;
         }
         else
         {
            if (gsc_get_firstPoint(ent, lastPoint) != GS_GOOD) return GS_BAD;
            if (acdbEntDel(ent) != RTNORM) return GS_BAD;
         }
         count--;
      }
      else
      {
         if (ads_point_equal(pt, lastPoint))
         {
            acutPrintf(gsc_msg(1055)); // "\nVertice duplicato."
            continue;
         }
      
         ads_point_set(pt, lastPoint);

         if (count == 1)
         {
            if (gsc_insert_pline(firstPoint, lastPoint) != GS_GOOD) return GS_BAD;
            acdbEntLast(ent);
         }
         else
            if (gsc_addvertex(ent, lastPoint) != GS_GOOD) return GS_BAD;
         count++;
      }
   }
   while (1);

   return GS_GOOD;
}
//int gsc_insert_pline(ads_point point, C_CLASS *ClassToCheckOverlap)
//{
//   int       rc, err = GS_GOOD;
//   ads_name  last, ent;
//   ads_point tmp_point;
//   resbuf    NewOsMode, OldOsMode;
//
//   if (acdbEntLast(last)!=RTNORM) ads_name_clear(last);
//
//   while (1)
//   {
//      if (!point)
//      {
//         do
//         {  // "\nDal punto: "
//            while ((rc = acedGetPoint(NULL, gsc_msg(1), tmp_point)) == RTNONE)
//               acutPrintf(gsc_msg(107)); // "\nPunto non valido."
//            if (rc == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
//            if (rc == RTCAN) return GS_CAN;
//            if (ClassToCheckOverlap) // se e si devono controllare le sovrapposizioni
//            {
//               // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
//               if (gsc_OverlapValidation(tmp_point, ClassToCheckOverlap) == GS_GOOD) break;
//               acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
//            }
//            else break;
//         }
//         while (1);
//      }
//      else
//      {
//         ads_point_set(point,tmp_point);
//         
//         // leggo valore variabile OSMODE
//         if (acedGetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
//            { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
//         // Setto il nuovo valore di OSMODE (disabilitato)
//         NewOsMode.restype = RTSHORT;
//         NewOsMode.resval.rint = 0;
//         if (acedSetVar(_T("OSMODE"), &NewOsMode) != RTNORM)
//            { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
//      }
//
//      if (gsc_callCmd(_T("_.PLINE"), 0) != RTNORM) 
//      { 
//         if (point) acedSetVar(_T("OSMODE"), &OldOsMode);
//         GS_ERR_COD = eGSAdsCommandErr;
//         err = GS_BAD;
//         break;
//      }
//
//      if (acedCommand(RTPOINT,tmp_point,0) != RTNORM) 
//      {
//         if (point) acedSetVar(_T("OSMODE"), &OldOsMode);
//         GS_ERR_COD = eGSAdsCommandErr;
//         err = GS_BAD;
//         break;
//      }
//
//      if (point)
//         // ripristino valore variabile OSMODE
//         if (acedSetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
//            { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
//
//      err = gsc_end_command(_T("PLINE"));
//      break;
//   }
//
//   // AUTOCAD in caso di BREAK(Cancel) crea comunque la polilinea
//   if (acdbEntLast(ent) != RTNORM) 
//      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
//   if (ads_name_equal(last, ent)) return GS_CAN;
//   if (gsc_ispline(ent) == GS_GOOD) return GS_GOOD;
//
//   return err;
//}


/*********************************************************/
/*.doc gsc_insert_text                       <internal> */
/*+
  Inserisce una entita' di tipo testo.
  Parametri:
  TCHAR *Txt;         testo
  ads_point InsPt;    punto di inserimento
  double Height;      Altezza testo
  double Rot;         Rotazione in gradi (default = 0)
  const TCHAR *Style; Stile testo (default = "STANDARD")
  double Thickness;   Spessore (default = 0)
  double xScale;      Fattore di scala X (default = 1)
  double OblAng;      Angolo di testo obliquo in gradi (default = 0)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_text(const TCHAR *Txt, ads_point InsPt, double Height, double Rot,
                    const TCHAR *Style, double Thickness, double xScale, double OblAng)
{
   AcDbBlockTable  *pBlockTable;
   AcGePoint3d     basePoint(InsPt[X], InsPt[Y], InsPt[Z]);
   AcDbSymbolTable *pSymbolTable   = NULL; 
   AcDbObjectId    StyleId = AcDbObjectId::kNull;

   // Ricavo ID della tabella degli stili testo del database corrente
   if (acdbHostApplicationServices()->workingDatabase()->getTextStyleTable(pSymbolTable,
                                                                           AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   pSymbolTable->getAt(Style, StyleId);
   pSymbolTable->close();

   // Open the block table for read.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   AcDbBlockTableRecord *pBlockTableRecord;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   AcDbText *pText;
   
   
   if ((pText = gsc_create_text(Txt, InsPt, Height, Rot, StyleId, 
                                Thickness, xScale, OblAng)) == NULL)
      return GS_BAD;

   pBlockTableRecord->appendAcDbEntity(pText);
   pText->close();
   pBlockTableRecord->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_create_text                        <external> */
/*+
  Crea un oggetto testo senza inserirlo nel DB grafico di AutoCAD.
  Parametri:
  TCHAR *Txt;            Testo
  ads_point InsPt;       Punto di inserimento
  double Height;         Altezza testo (default = 1)
  double Rot;            Rotazione in gradi (default = 0)
  AcDbObjectId &StyleId; Identificatore dello stile testo 
                         (default = AcDbObjectId::kNull)
  double Thickness;      Spessore (default = 0)
  double Width;          Fattore di scala X (default = 1)
  double OblAng;         Angolo di testo obliquo in gradi (default = 0)
  TextHorzMode HorzMode; Allineamento orizzontale (default = kTextLeft)
  TextVertMode VertMode; Allineamento verticale (default = kTextBase)

  Restituisce il puntatore all'oggetto grafico in caso di successo
  altrimenti restituisce NULL. 
-*/  
/*********************************************************/
AcDbText *gsc_create_text(const TCHAR *Txt, ads_point InsPt, double Height, 
                          double Rot, const AcDbObjectId &StyleId, 
                          double Thickness, double Width, double OblAng,
                          TextHorzMode HorzMode, TextVertMode VertMode)
{
   AcGePoint3d BasePoint(InsPt[X], InsPt[Y], InsPt[Z]);

   AcDbText *pText = new AcDbText(BasePoint, Txt, StyleId, Height, gsc_grd2rad(Rot));
   if (!pText) return NULL;
   if (pText->setThickness(Thickness) != Acad::eOk)
      { delete pText; GS_ERR_COD = eGSInvalidThickness; return NULL; }
   if (Width > 0)
      if (pText->setWidthFactor(Width) != Acad::eOk)
         { delete pText; GS_ERR_COD = eGSInvalidWidth; return NULL; }
   if (pText->setOblique(gsc_grd2rad(OblAng)) != Acad::eOk)
      { delete pText; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pText->setHorizontalMode(HorzMode) != Acad::eOk)
      { delete pText; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pText->setVerticalMode(VertMode) != Acad::eOk)
      { delete pText; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (HorzMode != AcDb::kTextLeft || VertMode != AcDb::kTextBase)
      if (pText->setAlignmentPoint(BasePoint) != Acad::eOk)
      { delete pText; GS_ERR_COD = eGSInvGraphObjct; return NULL; }
   pText->close();

   return pText;
}


/*********************************************************/
/*.doc gsc_insert_point                       <internal> */
/*+
  Inserisce un oggetto grafico punto.
  Parametri:
  ads_point Pt;       Coordinate del punto
  const TCHAR *Layer; Nome piano esistente (Default = NULL "piano corrente")
  C-COLOR *Color;    Codice del colore (default = NULL -> "colore corrente")
  double Thickness;   Spessore (default = 0)
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)
  AcDbPoint *pResult;  Oggetto punto creato (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_point(ads_point Pt, const TCHAR *Layer, C_COLOR *Color, double Thickness,
                     AcDbBlockTableRecord *pBlockTableRecord, AcDbPoint **pResult)
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

   AcDbPoint *pPoint = gsc_create_point(Pt, Layer, Color, Thickness);

   pInternalBlockTableRecord->appendAcDbEntity(pPoint);
   pPoint->close();
   if (!pBlockTableRecord) pInternalBlockTableRecord->close();

   if (pResult) *pResult = pPoint;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_create_point                       <external> */
/*+
  Crea un oggetto punto senza inserirlo nel DB grafico di AutoCAD.
  Parametri:
  ads_point Pt;       Coordinate del punto
  const TCHAR *Layer; Nome piano esistente (Default = NULL "piano corrente")
  C_COLOR *Color;     Codice del colore (default = NULL -> "colore corrente")
  double Thickness;   Spessore (default = 0)

  Restituisce il puntatore all'oggetto grafico in caso di successo
  altrimenti restituisce NULL. 
-*/  
/*********************************************************/
AcDbPoint *gsc_create_point(ads_point Pt, const TCHAR *Layer, 
                            C_COLOR *Color, double Thickness)
{
   AcGePoint3d Position(Pt[X], Pt[Y], Pt[Z]);
   AcDbPoint   *pPoint;

   if ((pPoint = new AcDbPoint(Position)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if (Layer)
      if (gsc_setLayer(pPoint, Layer) != GS_GOOD)
         { delete pPoint; return NULL; }

   if (Color)
      if (gsc_set_color(pPoint, *Color) == GS_BAD)
         { delete pPoint; return NULL; }

   if (pPoint->setThickness(Thickness) != Acad::eOk)
      { delete pPoint; GS_ERR_COD = eGSInvalidThickness; return NULL; }

   pPoint->close();

   return pPoint;
}


/*********************************************************/
/*.doc gsc_insert_3dFace                      <internal> */
/*+
  Inserisce un oggetto grafico 3DFace.
  Parametri:
  ads_point Pt1;      Coordinate del punto 1
  ads_point Pt2;      Coordinate del punto 2
  ads_point Pt3;      Coordinate del punto 3
  ads_point Pt4;      Opzionale; Coordinate del punto 4 (default = NULL)
  const TCHAR *Layer; Nome piano esistente (Default = NULL "piano corrente")
  C_COLOR *Color;     Codice del colore (default = NULL -> "colore corrente")
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_3dFace(ads_point Pt1, ads_point Pt2, ads_point Pt3, ads_point Pt4,
                      const TCHAR *Layer, C_COLOR *Color,
                      AcDbBlockTableRecord *pBlockTableRecord)
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

   AcDbFace *pFace;
   AcGePoint3d p1(Pt1[X], Pt1[Y], Pt1[Z]);
   AcGePoint3d p2(Pt2[X], Pt2[Y], Pt2[Z]);
   AcGePoint3d p3(Pt3[X], Pt3[Y], Pt3[Z]);
   if (Pt4)
   {
      AcGePoint3d p4(Pt4[X], Pt4[Y], Pt4[Z]);
      pFace = new AcDbFace(p1, p2, p3, p4);
   }
   else
      pFace = new AcDbFace(p1, p2, p3);

   if (Layer)
      if (gsc_setLayer(pFace, Layer) != GS_GOOD)
         { delete pFace; return GS_BAD; }
   
   if (Color)
      if (gsc_set_color(pFace, *Color) != GS_GOOD)
         { delete pFace; return GS_BAD; }

   pInternalBlockTableRecord->appendAcDbEntity(pFace);
   pFace->close();
   if (!pBlockTableRecord) pInternalBlockTableRecord->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_insert_rectangle                   <internal> */
/*+
  Inserisce un oggetto grafico rettangolo.
  Parametri:
  ads_point Pt1;      Coordinate di un angolo
  ads_point Pt2;      Coordinate dell'angolo opposto
  const TCHAR *Layer; Nome piano esistente (Default = NULL "piano corrente")
  C_COLOR *Color;     Codice del colore (default = NULL -> "colore corrente")
  int Mode;           Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                      (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_rectangle(ads_point Pt1, ads_point Pt2, const TCHAR *Layer, C_COLOR *Color,
                         int Mode, AcDbBlockTableRecord *pBlockTableRecord)
{
   if (Mode == PREVIEW)
   {
      ads_point PtNdx;
      int       AutoCADColorIndex;
         
      if (!Color) AutoCADColorIndex = COLOR_BYLAYER;
      else Color->getAutoCADColorIndex(&AutoCADColorIndex);

      PtNdx[X] = Pt2[X]; PtNdx[Y] = Pt1[Y];
      if (acedGrDraw(Pt1, PtNdx, AutoCADColorIndex, 0) != RTNORM || acedGrDraw(PtNdx, Pt2, AutoCADColorIndex, 0) != RTNORM)
         return GS_BAD;
      PtNdx[X] = Pt1[X]; PtNdx[Y] = Pt2[Y];
      if (acedGrDraw(Pt2, PtNdx, AutoCADColorIndex, 0) != RTNORM || acedGrDraw(PtNdx, Pt1, AutoCADColorIndex, 0) != RTNORM)
         return GS_BAD;
      return GS_GOOD;
   }

   C_POINT_LIST PointList;

   if (PointList.add_point(Pt1) == NULL) return GS_BAD;
   if (PointList.add_point(Pt2[X], Pt1[Y], Pt1[Z]) == NULL) return GS_BAD;
   if (PointList.add_point(Pt2[X], Pt2[Y], Pt1[Z]) == NULL) return GS_BAD;
   if (PointList.add_point(Pt1[X], Pt2[Y], Pt1[Z]) == NULL) return GS_BAD;
   if (PointList.add_point(Pt1) == NULL) return GS_BAD;

   return gsc_insert_pline(PointList, Layer, Color, NULL);
}


/*********************************************************/
/*.doc gsc_insert_lwpline                     <internal> */
/*+
  Inserisce un oggetto grafico line weight polyline.
  La quota del primo punto determina l'elevazione della linea.
  Parametri:
  ads_point Pt1;         Coordinate del primo punto
  ads_point Pt2;         Coordinate del secondo punto
  const TCHAR *Layer;    Nome piano esistente (Default = NULL "piano corrente")
  C_COLOR *Color;        Codice del colore (default = NULL -> "colore corrente")
  const TCHAR *LineType; Nome del tipo linea (Default = NULL)
  double Scale;          Scala del tipolinea (default = 1.0)
  double Width;          Spessore linea (default = 0.0)
  int Mode;              Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                         (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_lwpline(ads_point Pt1, ads_point Pt2, const TCHAR *Layer, C_COLOR *Color,
                       const TCHAR *LineType, double Scale, double Width,
                       int Mode, AcDbBlockTableRecord *pBlockTableRecord)
{
   if (Mode == PREVIEW)
   {
      int AutoCADColorIndex = COLOR_BYLAYER;

      if (Color) Color->getAutoCADColorIndex(&AutoCADColorIndex);

      return (acedGrDraw(Pt1, Pt2, AutoCADColorIndex, 0) != RTNORM)  ? GS_BAD : GS_GOOD;
   }
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

   AcDbPolyline *pLine = new AcDbPolyline(2); // 2 vertici
   AcGePoint2d  pt;
   pt.set(Pt1[X], Pt1[Y]);
   pLine->addVertexAt(0, pt);
   pt.set(Pt2[X], Pt2[Y]);
   pLine->addVertexAt(1, pt);

   if (Layer)
      if (gsc_setLayer(pLine, Layer) != GS_GOOD)
         { delete pLine; return GS_BAD; }

   if (Color)
      if (gsc_set_color(pLine, *Color) != GS_GOOD)
         { delete pLine; return GS_BAD; }

   pLine->setElevation(Pt1[Z]);
   if (LineType)
   {
      if (gsc_set_lineType(pLine, LineType)!= GS_GOOD)
         { delete pLine; return GS_BAD; }
      pLine->setLinetypeScale(Scale);
   }
   pLine->setConstantWidth(Width);

   pInternalBlockTableRecord->appendAcDbEntity(pLine);
   pLine->close();
   if (!pBlockTableRecord) pInternalBlockTableRecord->close();

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_insert_hatch                                                      */
/*+                                                                       
  Inserisce un riempimento della forma di un rettangolo.
  Parametri:
  ads_point Pt1;        Coordinate di un angolo
  ads_point Pt2;        Coordinate dell'angolo opposto
  const TCHAR *hatch;   Nome riempimento
  const TCHAR *Layer;   Nome piano esistente (Default = NULL "piano corrente")
  double   scale;       scala     (default = 1)
  double   rotation;    rotazione in gradi (default = 0)
  C_COLOR  *color;      colore (default = null -> "colore corrente")
  AcDbBlockTableRecord *pBlockTableRecord;   Per migliorare le prestazioni
                                             (default = NULL)


  Ritorna GS_GOOD in caso di successo, GS_CAN se l'area è troppo piccola
  per contenere un tratteggio, altrimenti GS_BAD.
-*/
/*****************************************************************************/
int gsc_insert_hatch(ads_point Pt1, ads_point Pt2, const TCHAR *hatch, 
                    const TCHAR *Layer, double scale, double rotation,
                    C_COLOR *color, AcDbBlockTableRecord *pBlockTableRecord)
{
   AcGePoint2dArray     vertexPts;
   AcGeDoubleArray      vertexBulges;
   AcDbHatch            *pHatch = new AcDbHatch();
   C_STRING             SolidFill;
   long                 i = 0;
   double               PatternAngle = gsc_grd2rad(rotation);
   AcDbBlockTableRecord *pInternalBlockTableRecord;
   Acad::ErrorStatus    err;

   // Set hatch plane
   AcGeVector3d normal(0.0, 0.0, 1.0);
   pHatch->setNormal(normal);
   pHatch->setElevation(0.0);

   // Set hatch layer
   if (Layer)
      if (gsc_setLayer(pHatch, Layer) != GS_GOOD)
         { delete pHatch; return GS_BAD; }

   // Set hatch color
   if (color)
      if (gsc_set_color(pHatch, *color) != GS_GOOD)
         { delete pHatch; return GS_BAD; }

   // Set non associative hatch
   pHatch->setAssociative(Adesk::kFalse);

   // Set hatch pattern to SolidFill type
   if (gsc_strcmp(hatch, _T("_SOLID"), FALSE) == 0 || // Versione internazionale
      gsc_strcmp(hatch, gsc_msg(328), FALSE) == 0) // "SOLIDO"
      SolidFill = _T("SOLID");
   else
      SolidFill = hatch;

   // Set hatch style to kNormal
   pHatch->setHatchStyle(AcDbHatch::kNormal);

   // Set pattern scale
   pHatch->setPatternScale(scale);

   // Set pattern scale
   pHatch->setPatternAngle(PatternAngle);

   pHatch->setPattern(AcDbHatch::kPreDefined, SolidFill.get_name());

   // Construct hatch external boundary
   vertexPts.setPhysicalLength(0).setLogicalLength(5);
   vertexPts[0].set(Pt1[X], Pt1[Y]);
   vertexPts[1].set(Pt2[X], Pt1[Y]);
   vertexPts[2].set(Pt2[X], Pt2[Y]);
   vertexPts[3].set(Pt1[X], Pt2[Y]);
   vertexPts[4].set(Pt1[X], Pt1[Y]);
   vertexBulges.setPhysicalLength(0).setLogicalLength(5);
   for (int j = 0; j < 5; j++) vertexBulges[j] = 0.0;

   // Append an external loop (rectangle) to hatch boundary   
   pHatch->appendLoop(AcDbHatch::kExternal, vertexPts, vertexBulges);

   // Elaborate solid fill
   if ((err = pHatch->evaluateHatch(true)) != Acad::eOk)
      { delete pHatch; GS_ERR_COD = eGSInvalidHatch; return GS_CAN; }

   if (!pBlockTableRecord)
   {
      // Post hatch entity to database
      AcDbBlockTable       *pBlockTable;

      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         { delete pHatch; GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
         { delete pHatch; pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      pBlockTable->close();
   }
   else
      pInternalBlockTableRecord = pBlockTableRecord;

   pInternalBlockTableRecord->appendAcDbEntity(pHatch);
   pHatch->close();

   if (!pBlockTableRecord) pInternalBlockTableRecord->close();

   return GS_GOOD;
}


/*+
-------------------------------------------------------------
int gsc_insert_text(TCHAR *style,ads_point point,double htext,double rot,TCHAR *intext)

       Inserisce una entita' di tipo nel punto di inserimento point,
       se questo e' NULL viene richiesto da input.
       style,htext,rot sono i valori di default rispettivamenti per 
       stile, altezza e rotazione testo. Il comando e' molto simile al
       corrispondente _TEXT di Autocad con in piu' le funzionalita' di 
       DRAGGEN. Il testo viene richiesto ad input.
       Se intext!=NULL il testo dell'entita' grafica e' gia definito.
  Parametri:
  TCHAR *style;
  ads_point point;
  double htext;
  double rot;
  TCHAR *intext;
  TCHAR *DefaultText;
  C_CLASS *ClassToCheckOverlap;  usato solo point = NULL e se l'inserimento
                                 deve controllare la sovrapposizione 
                                 (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto, GS_CAN in caso di
       comando annullato dall'utente o GS_BAD in caso di errore.
-------------------------------------------------------------
-*/
int gsc_insert_text(TCHAR *style, ads_point point, double htext, double rot,
                    TCHAR *intext, TCHAR *DefaultText, C_CLASS *ClassToCheckOverlap)
{
   ads_name  ent,sel;
   ads_point pt,ins;
   int       rcc, rc, flag, res, OsModeToDisable = (point) ? TRUE : FALSE;
   TCHAR     str[133], just[133], text[133], prompt[50];
   TCHAR     new_style[MAX_LEN_TEXTSTYLENAME], old_style[MAX_LEN_TEXTSTYLENAME];
   double    ys,rt;
   resbuf    NewOsMode, OldOsMode;
   C_STRING  Prompt;

   if (style == NULL || htext <= 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (gsc_set_CurrentTextStyle(style, old_style) == GS_BAD) return GS_BAD;

   if (point==NULL)
   {       
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      do
      {  // "\nPunto di inserimento: "
         while ((rc = acedGetPoint(NULL, gsc_msg(20), ins)) == RTNONE)
            acutPrintf(gsc_msg(107)); // "\nPunto non valido."
         if (rc == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         if (rc == RTCAN) return GS_CAN;
         point = ins;
         if (ClassToCheckOverlap) // se e si devono controllare le sovrapposizioni
         {
            // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
            if (gsc_OverlapValidation(ins, ClassToCheckOverlap) == GS_GOOD) break;
            acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
         }
         else break;
      }
      while (1);
   }

   // Richide stile o giustificazione
   wcscpy(just,gsc_msg(2)); // "Sinistra"

   do
   {
      str[0]= _T('\0');

      Prompt = gsc_msg(93);   // "\nDigitare un'opzione "
      if (intext!=NULL)
      {
         Prompt += gsc_msg(235); // "[Giustificato/Stile/<Continua>]: "
         acedInitGet(0, gsc_msg(236));   // "Giustificato Stile Continua"
      }
      else
      {
         Prompt += gsc_msg(4); // "[Giustificato/Stile/<Testo>]: "
         acedInitGet(0,gsc_msg(3)); // "Giustificato Stile Testo"
      }

      if ((rcc = acedGetKword(Prompt.get_name(), str)) != RTNORM && rcc != RTNONE)
      {
         gsc_set_CurrentTextStyle(old_style,NULL);
         if (rcc==RTCAN) return GS_CAN;
         GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; 
      }
      if (gsc_strcmp(str, gsc_msg(250)) == 0) wcscpy(str, gsc_msg(14)); // "Continua" "Testo"

      if (gsc_strcmp(str, gsc_msg(5))==0)    // "Giustificato"
      {
         // "Proporzionale adaTta Ce Mezzo Destra AS AC AD MS MC MD BS BC BD Sinistra"
         acedInitGet(0, gsc_msg(6));
         Prompt = gsc_msg(93);   // "\nDigitare un'opzione "
         // "[Proporzionale/adaTta/Centro/Mezzo/Destra/AS/AC/AD/MS/MC/MD/BS/BC/BD/<Sinistra>]: "
         Prompt += gsc_msg(7); 
         
         if ((rc = acedGetKword(Prompt.get_name(), just)) != RTNORM && rc!=RTNONE)
         {
            gsc_set_CurrentTextStyle(old_style, NULL);
            if (rc == RTCAN) return GS_CAN;
            GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; 
         }
         if (rc == RTNONE || just[0] == _T('\0')) wcscpy(just,gsc_msg(2));   // "Sinistra"
         // Traduce opzione di giustificazione
         if (gsc_strcmp(just, gsc_msg(8))==0) wcscpy(just, _T("_A"));         // "Proporzionale"
         else if (gsc_strcmp(just, gsc_msg(9))==0) wcscpy(just, _T("_F"));    // "adaTta"
         else if (gsc_strcmp(just, gsc_msg(238))==0) wcscpy(just, _T("_C"));  // "Centro"
         else if (gsc_strcmp(just, gsc_msg(239))==0) wcscpy(just, _T("_M"));  // "Mezzo"
         else if (gsc_strcmp(just, gsc_msg(240))==0) wcscpy(just, _T("_R"));  // "Destra"
         else if (gsc_strcmp(just, gsc_msg(241))==0) wcscpy(just, _T("_TL")); // "AS"
         else if (gsc_strcmp(just, gsc_msg(242))==0) wcscpy(just, _T("_TC")); // "AC"
         else if (gsc_strcmp(just, gsc_msg(243))==0) wcscpy(just, _T("_TR")); // "AD"
         else if (gsc_strcmp(just, gsc_msg(244))==0) wcscpy(just, _T("_ML")); // "MS"
         else if (gsc_strcmp(just, gsc_msg(245))==0) wcscpy(just, _T("_MC")); // "MC"
         else if (gsc_strcmp(just, gsc_msg(246))==0) wcscpy(just, _T("_MR")); // "MD"
         else if (gsc_strcmp(just, gsc_msg(247))==0) wcscpy(just, _T("_BL")); // "BS"
         else if (gsc_strcmp(just, gsc_msg(248))==0) wcscpy(just, _T("_BC")); // "BC"
         else if (gsc_strcmp(just, gsc_msg(249))==0) wcscpy(just, _T("_BR")); // "BD"
      
         // Input secondo punto solo per giustificazione proporzionale o adatta
         if (gsc_strcmp(just, _T("_A")) == 0 || gsc_strcmp(just, _T("_F")) == 0)
            acedGetPoint(point,gsc_msg(10),pt);
      }
      else if (gsc_strcmp(str, gsc_msg(11))==0)   // Stile
      {
         swprintf(prompt, 50, gsc_msg(12), style); // "\nNome dello stile (o ?) <%s>: "
         do
         {
            if ((rc = ads_getstring(1, prompt, new_style)) != RTNORM && rc != RTNONE)
            {
               gsc_set_CurrentTextStyle(old_style, NULL);
               if (rc == RTCAN) return GS_CAN;
               GS_ERR_COD = eGSAdsCommandErr;
               return GS_BAD; 
            }
            gsc_alltrim(new_style);
            if (rc == RTNONE || new_style[0] == _T('\0')) 
               { gsc_set_CurrentTextStyle(style, NULL); break; }

            if (new_style[0] == _T('?')) // si richiede la lista degli stili testo di GEOsim
            {
               C_RB_LIST txt_style_list;
               presbuf   p_txt_style;

               if ((txt_style_list << gsc_getgeosimstyle()) == NULL) return GS_BAD;

               p_txt_style = txt_style_list.get_head();
               while (p_txt_style)
               {
                  acutPrintf(_T("\n%s"), p_txt_style->resval.rstring);
                  p_txt_style = txt_style_list.get_next();
               }
               break;
            }

            if ((rc = gsc_set_CurrentTextStyle(new_style, NULL)) == GS_BAD)
               acutPrintf(gsc_msg(13)); // "\nIl nome dello stile testo è sconosciuto o non valido."
         }
         while (rc != GS_GOOD);
      }
   }
   while (rcc != RTNONE && gsc_strcmp(str, gsc_msg(14)) != 0 && str[0] != _T('\0')); // "Testo"

   // Input testo da scrivere
   if (intext)
      gsc_strcpy(text, intext, 133);
   else
   {
      if ((rc = ads_getstring(1, gsc_msg(15), text)) != RTNORM) // "\nTesto: "
      {
         gsc_set_CurrentTextStyle(old_style, NULL);
         if (rc == RTCAN || rc == RTNONE) return GS_CAN;
         GS_ERR_COD = eGSAdsCommandErr;
         return GS_BAD; 
      }
      if (wcslen(text) == 0)
         if (DefaultText && wcslen(DefaultText) > 0) gsc_strcpy(text, DefaultText, 133);
         else { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   }

   // Creazione della classe testo con rotazione e htext di default
   if (OsModeToDisable)
   {
      // leggo valore variabile OSMODE
      if (acedGetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      // Setto il nuovo valore di OSMODE (disabilitato)
      NewOsMode.restype = RTSHORT;
      NewOsMode.resval.rint = 0;
      if (acedSetVar(_T("OSMODE"), &NewOsMode) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   }

   res = GS_BAD;
   do
   {
      double FixedHText;
      
      // verifico se lo stile di testo ha una altezza fissa
      if (gsc_get_charact_textstyle(style, &FixedHText) == GS_BAD) break;

      if (gsc_strcmp(just, _T("_A")) == 0)
      {
         if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_J"), RTSTR, just, RTPOINT, point,
                         RTPOINT, pt, RTSTR, text, 0) != RTNORM)
            break;
      }
      else if (gsc_strcmp(just, _T("_F")) == 0)
      {
         if (FixedHText == 0) // non è stata impostata altezza fissa
         {
            if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_J"), RTSTR, just, RTPOINT, point,
                            RTPOINT, pt, RTREAL, htext, RTSTR, text, 0) != RTNORM)
               break;
         }
         else
            if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_J"), RTSTR, just, RTPOINT, point,
                            RTPOINT, pt, RTSTR, text, 0) != RTNORM)
               break;
      }
      else if (gsc_strcmp(just, gsc_msg(2)) == 0) // "Sinistra"
      {
         if (FixedHText == 0) // non è stata impostata altezza fissa
         {
            if (gsc_callCmd(_T("_.TEXT"), RTPOINT, point, RTREAL, htext, RTREAL, rot,
                            RTSTR, text, 0) != RTNORM)
               break;
         }
         else
            if (gsc_callCmd(_T("_.TEXT"), RTPOINT, point, RTREAL, rot,
                            RTSTR, text, 0) != RTNORM)
               break;
      }
      else 
      {
         if (FixedHText == 0) // non è stata impostata altezza fissa
         {
            if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_J"), RTSTR, just, RTPOINT, point,
                            RTREAL, htext, RTREAL, rot, RTSTR, text, 0) != RTNORM)
               break;
         }
         else
            if (gsc_callCmd(_T("_.TEXT"), RTSTR, _T("_J"), RTSTR, just, RTPOINT, point,
                            RTREAL, rot, RTSTR, text, 0) != RTNORM)
               break;
      }
      res = GS_GOOD;
   }
   while (0);
   
   gsc_set_CurrentTextStyle(old_style, NULL);

   if (OsModeToDisable)
      // ripristino valore variabile OSMODE
      if (acedSetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }

   if (res == GS_BAD) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   // Seleziona il blocco appena inserito
   if (acdbEntLast(ent)!=RTNORM) 
      { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }

   if (acedSSAdd(ent,NULL,sel)!=RTNORM) 
      { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }

   // Imposta punto base=punto di inserimento
   BASE[0] = point[0];
   BASE[1] = point[1];
   BASE[2] = point[2];
   _SCALE  = htext;
   ROTA    = acutAngle(BASE, pt);

   if (gsc_strcmp(just, _T("_A"))!=0)
   {
      if (GEOsimAppl::GLOBALVARS.get_InsHText() == MANUAL) // impostazione manuale dell'altezza testo
      {
         swprintf(prompt, 50, gsc_msg(16), htext); // "\nAltezza testo <%f>: "
         do
         {
            flag=0;ys=htext; 
            acedInitGet(RSG_OTHER, GS_EMPTYSTR);
      
            if (gsc_strcmp(just, _T("_F")) == 0) rc = ads_draggen(sel, prompt, 0, gsc_HYscale, pt);
            else rc = ads_draggen(sel, prompt, 0, gsc_Hscale, pt);

            switch (rc)
            {
               case RTNORM :
                  if (YSCALE == 0)
                  {
                     acutPrintf(gsc_msg(17)); // "\n Il valore deve essere diverso da zero."
                     flag=1; break;
                  }
                  if (gsc_set_hText(ent, YSCALE)==GS_BAD)
                     { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
                  break;
               case RTSTR  :
                  ads_getinput(str);
                  if ((ys = _wtof(str)) <= 0.0) 
                  {
                     acutPrintf(gsc_msg(18)); // "\n Il valore deve essere maggiore di zero."
                     flag = 1; break;
                  }
               case RTNONE :
                  if (gsc_set_hText(ent, ys)==GS_BAD)
                     { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
                  break;
               case RTCAN :
                  ads_ssfree(sel);
                  gsc_EraseEnt(ent);
                  return GS_CAN;     
               case RTERROR :
               default :
                  ads_ssfree(sel);
                  gsc_EraseEnt(ent);
                  return GS_BAD;
            }      
         }
         while(flag==1);
      }
   }

   if (GEOsimAppl::GLOBALVARS.get_InsRotaz() == MANUAL) // impostazione manuale della rotazione
   {
      if (gsc_strcmp(just, _T("_F")) != 0 && gsc_strcmp(just, _T("_A")) != 0)
      {
         // Imposta rotazione nel piano XY (intorno asse Z)
         rt = (rot/180)*PI;
         swprintf(prompt, 50, gsc_msg(19), rot); // "\nAngolo di rotazione <%f>: "

         acedInitGet(RSG_OTHER, GS_EMPTYSTR);
         ads_point_set(BASE,GS_POINT_CURSOR);
         acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
         rc = ads_draggen(sel,prompt,0,gsc_Zrotate,pt);
         acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

         switch(rc)
         {
            case RTNORM :
               if (gsc_set_rotation(ent, rt + ROTA, GS_BAD) == GS_BAD)
                  { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
               break; 
            case RTSTR :
               ads_getinput(str);
               rt = _wtof(str);
               rt = (rt/180)*PI;
            case RTNONE :
               if (gsc_set_rotation(ent, rt, GS_BAD) == GS_BAD)
                  { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
              break;
            case RTCAN :
               gsc_EraseEnt(ent);
               ads_ssfree(sel);
               return GS_CAN;     
            case RTERROR :
            default :
               gsc_EraseEnt(ent);
               ads_ssfree(sel);
               return GS_BAD;
         }
      }
   }
   ads_ssfree(sel);     
   
   return GS_GOOD;   
}


/*+
-------------------------------------------------------------
int gsc_set_text(ads_name ent,TCHAR *text)
    Funzione usata per modificare il testo
    di una entita' esistente di tipo testo.
  Parametri:
  ads_name ent;
  TCHAR *text;
-------------------------------------------------------------
-*/
int gsc_set_text(AcDbEntity *pEnt, const TCHAR *text)
{
   if (!text || wcslen(text) == 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (gsc_strcmp(text, ((AcDbText *)pEnt)->textStringConst()) != 0)
      {
         Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbText *)pEnt)->setTextString(text) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      C_STRING PrevText;
      
      PrevText.paste(((AcDbMText *) pEnt)->text()); // non uso contents (che ritorna caratteri di controllo)
      // rimuovo i caratteri per andare a capo e li sostituisco con uno spazio
      PrevText.strtran(_T("\r\n"), _T(" "));

      if (PrevText.comp(text) != 0)
      {
         Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbMText *)pEnt)->setContents(text) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      if (gsc_strcmp(text, ((AcDbAttribute *)pEnt)->textStringConst()) != 0)
      {
         Acad::ErrorStatus Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttribute *)pEnt)->setTextString(text) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      if (gsc_strcmp(text, ((AcDbAttributeDefinition *)pEnt)->textStringConst()) != 0)
      {
         Acad::ErrorStatus Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttributeDefinition *)pEnt)->setTextString(text) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }

   return GS_GOOD;
}
int gsc_set_text(ads_name ent, const TCHAR *text)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   int Res = gsc_set_text((AcDbEntity *) pObj, text);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return Res;
}


/*********************************************************/
/*.doc gsc_get_rotation                        <external> */
/*+                                                                       
  Funzione che restituisce la rotazione in radianti di un oggetto
  grafico esistente.
  Parametri:
  ads_name ent;        Oggetto da analizzare
  double   *rotation;  Rotazione in radianti (out)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_rotation(ads_name ent, double *rotation)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (!rotation) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_rotation((AcDbEntity *) pObj, rotation) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_rotation(AcDbEntity *pEnt, double *rotation)
{
   if (pEnt->isKindOf(AcDbAttribute::desc()))
      *rotation = ((AcDbAttribute *) pEnt)->rotation();
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
      *rotation = ((AcDbAttributeDefinition *) pEnt)->rotation();
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
      *rotation = ((AcDbBlockReference *) pEnt)->rotation();
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
      *rotation = ((AcDbDimension *) pEnt)->textRotation();
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
      *rotation = ((AcDbMText *) pEnt)->rotation();
   else
   if (pEnt->isKindOf(AcDbText::desc()))
      *rotation = ((AcDbText *) pEnt)->rotation();
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_rotation                       <external> */
/*+                                                                       
  Funzione che setta la rotazione in radianti di un oggetto 
  grafico esistente.
  Parametri:
  ads_name ent;       Oggetto da analizzare
  double   rotation;  Rotazione in radianti
  int      check_rot; Se GS_GOOD il testo non sarà mai capovolto (default = GS_GOOD)
                      applicato a testi e agli attributi di un blocco
  bool AttribRepositioning; Flag, usato solo per blocchi con attributi.
                            Se = true gli attributi saranno riposizionati
                            insieme al blocco (default = true).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_rotation(ads_name ent, double rotation, int check_rot, 
                     bool AttribRepositioning)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_rotation((AcDbEntity *) pObj, rotation, check_rot,
                        AttribRepositioning) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_set_rotation(AcDbEntity *pEnt, double rotation, int check_rot,
                     bool AttribRepositioning)
{
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      if (check_rot == GS_GOOD) // verifico che il testo non sia capovolto
         if (rotation > gsc_grd2rad(90) &&  rotation <= gsc_grd2rad(270))
            rotation -= gsc_grd2rad(180);

      if (((AcDbAttribute *) pEnt)->rotation() != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbAttribute *) pEnt)->setRotation(rotation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      if (check_rot == GS_GOOD) // verifico che il testo non sia capovolto
         if (rotation > gsc_grd2rad(90) &&  rotation <= gsc_grd2rad(270))
            rotation -= gsc_grd2rad(180);

      if (((AcDbAttributeDefinition *) pEnt)->rotation() != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbAttributeDefinition *) pEnt)->setRotation(rotation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {
      double PrevRot = ((AcDbBlockReference *) pEnt)->rotation();

      if (PrevRot != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbBlockReference *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }         

         ((AcDbBlockReference *) pEnt)->setRotation(rotation);

         if (AttribRepositioning)
         {
            // Riposiziono gli eventuali attributi del blocco
            ads_point          AttrPIns, BlockPIns;
            ads_real           Dist, AttrRot, Angle;
            AcDbObjectId       objId;
            AcDbObjectIterator *pAttrIter;
   	      AcDbAttribute      *pAttrib;

            // Leggo il punto di inserimento del blocco
            if (gsc_get_firstPoint(pEnt, BlockPIns) == GS_BAD) return GS_BAD;

            // Ciclo sugli attributi
            pAttrIter = ((AcDbBlockReference *) pEnt)->attributeIterator();
   	      for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
            {
	            objId = pAttrIter->objectId();

               if (((AcDbBlockReference *) pEnt)->openAttribute(pAttrib, objId, AcDb::kForWrite) == Acad::eOk)
               {
                  // leggo punto di inserimento
                  gsc_get_firstPoint(pAttrib, AttrPIns);

                  Dist = gsc_dist(BlockPIns, AttrPIns);
                  Angle = acutAngle(BlockPIns, AttrPIns) + rotation - PrevRot;

                  AttrRot = pAttrib->rotation() - PrevRot + rotation;
                  acutPolar(BlockPIns, Angle, Dist, AttrPIns);           

                  if (gsc_set_firstPoint(pAttrib, AttrPIns) == GS_BAD)
                     { pAttrib->close(); return GS_BAD; }
                  if (gsc_set_rotation(pAttrib, AttrRot, check_rot) == GS_BAD)
                     { pAttrib->close(); return GS_BAD; }

                  pAttrib->close();
               }
            }
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      if (check_rot == GS_GOOD) // verifico che il testo non sia capovolto
         if (rotation > gsc_grd2rad(90) &&  rotation <= gsc_grd2rad(270))
            rotation -= gsc_grd2rad(180);

      if (((AcDbDimension *) pEnt)->textRotation() != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbDimension *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbDimension *) pEnt)->setTextRotation(rotation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      if (check_rot == GS_GOOD) // verifico che il testo non sia capovolto
         if (rotation > gsc_grd2rad(90) &&  rotation <= gsc_grd2rad(270))
            rotation -= gsc_grd2rad(180);

      if (((AcDbMText *) pEnt)->rotation() != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbMText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbMText *) pEnt)->setRotation(rotation);
      }
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (check_rot == GS_GOOD) // verifico che il testo non sia capovolto
         if (rotation > gsc_grd2rad(90) &&  rotation <= gsc_grd2rad(270))
            rotation -= gsc_grd2rad(180);

      if (((AcDbText *) pEnt)->rotation() != rotation)
      {
         Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ((AcDbText *) pEnt)->setRotation(rotation);
      }
   }
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_scale                          <external> */
/*+                                                                       
  Funzione che restituisce la scala di un oggetto grafico esistente.
  Parametri:
  ads_name ent;      Oggetto da analizzare
  double   *XScale;  Fattore di scala X
  double   *YScale;  Opzionale, Fattore di scala Y (default = NULL)
  double   *ZScale;  Opzionale, Fattore di scala Z (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_scale(ads_name ent, double *XScale, double *YScale, double *ZScale)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_scale((AcDbEntity *) pObj, XScale, YScale, ZScale) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_scale(AcDbEntity *pEnt, double *XScale, double *YScale, double *ZScale)
{
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      *XScale = ((AcDbAttribute *) pEnt)->widthFactor();
      if (YScale) *YScale = ((AcDbAttribute *) pEnt)->height();
      if (ZScale) *ZScale = 1.0;
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      *XScale = ((AcDbAttributeDefinition *)pEnt)->widthFactor();
      if (YScale) *YScale = ((AcDbAttributeDefinition *)pEnt)->height();
      if (ZScale) *ZScale = 1.0;
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {
      *XScale = ((AcDbBlockReference *)pEnt)->scaleFactors().sx;
      if (YScale) *YScale = ((AcDbBlockReference *)pEnt)->scaleFactors().sy;
      if (ZScale) *ZScale = ((AcDbBlockReference *)pEnt)->scaleFactors().sz;
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      *XScale = ((AcDbText *)pEnt)->widthFactor();
      if (YScale) *YScale = ((AcDbText *)pEnt)->height();
      if (ZScale) *ZScale = 1.0;
   }
   // da aggiungere i tipi di oggetti man mano che servono
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_scale                          <external> */
/*+                                                                       
  Funzione che setta la scala di un oggetto grafico esistente.
  Parametri:
  ads_name ent;      Oggetto da analizzare
  double   XScale;   Fattore di scala X
  double   YScale;   Fattore di scala Y
  double   ZScale;   Fattore di scala Z
  bool AbsScale;     Flag, usato solo per blocchi.
                     Se = true i fattori di scala saranno considerati
                     assoluti altrimenti relativi alla scala precedente 
                     (default = true).
  bool AttribRepositioning; Flag, usato solo per blocchi con attributi.
                            Se = true gli attributi saranno riposizionati
                            insieme al blocco (default = true).
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_scale(AcDbEntity *pEnt, double XScale, double YScale, double ZScale,
                  bool AbsScale, bool AttribRepositioning)
{
   Acad::ErrorStatus Res;

   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      double Prev;

      if (XScale == 1 && YScale == 1) return GS_GOOD;

      Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      // Nuova larghezza
      Prev = ((AcDbAttribute *)pEnt)->widthFactor();
      if (((AcDbAttribute *)pEnt)->setWidthFactor(Prev * XScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
      // Nuova altezza
      Prev = ((AcDbAttribute *)pEnt)->height();
      if (((AcDbAttribute *)pEnt)->setHeight(Prev * YScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      double Prev;

      if (XScale == 1 && YScale == 1) return GS_GOOD;

      Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      // Nuova larghezza
      Prev = ((AcDbAttributeDefinition *)pEnt)->widthFactor();
      if (((AcDbAttributeDefinition *)pEnt)->setWidthFactor(Prev * XScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
      // Nuova altezza
      Prev = ((AcDbAttributeDefinition *)pEnt)->height();
      if (((AcDbAttributeDefinition *)pEnt)->setHeight(Prev * YScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {
      AcGeScale3d PrevScale, Scale(XScale, YScale, ZScale);

      PrevScale = ((AcDbBlockReference *)pEnt)->scaleFactors();
      if (!AbsScale) Scale *= PrevScale;
      
      if (Scale == PrevScale) return GS_GOOD;

      Res = ((AcDbBlockReference *) pEnt)->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (((AcDbBlockReference *)pEnt)->setScaleFactors(Scale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidScale; return GS_BAD; }

      if (AttribRepositioning)
      {
         // Riposiziono gli eventuali attributi del blocco
         ads_point          AttrPIns, BlockPIns;
         ads_real           XFactor, YFactor, ZFactor;
         AcDbObjectId       objId;
         AcDbObjectIterator *pAttrIter;
         AcDbAttribute      *pAttrib;

         // Leggo il punto di inserimento del blocco
         if (gsc_get_firstPoint(pEnt, BlockPIns) == GS_BAD) return GS_BAD;

         // Ciclo sugli attributi
         pAttrIter = ((AcDbBlockReference *) pEnt)->attributeIterator();
         for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
         {
            objId = pAttrIter->objectId();

            if (((AcDbBlockReference *) pEnt)->openAttribute(pAttrib, objId, AcDb::kForWrite) == Acad::eOk)
            {
               // Calcolo i fattori di spostamento
               XFactor = (pAttrib->position().x - BlockPIns[X]) / PrevScale.sx; 
               YFactor = (pAttrib->position().y - BlockPIns[Y]) / PrevScale.sy; 
               ZFactor = (pAttrib->position().z - BlockPIns[Z]) / PrevScale.sz; 
               // setto il punto di inserimento dell'attributo
               AttrPIns[X] = XFactor * Scale.sx;
               AttrPIns[Y] = YFactor * Scale.sy;
               AttrPIns[Z] = ZFactor * Scale.sz;
               if (gsc_set_firstPoint(pAttrib, AttrPIns) == GS_BAD)
                  { pAttrib->close(); return GS_BAD; }
               // setto nuove dimensioni dell'attributo      
               if (gsc_set_scale(pAttrib, Scale.sx / PrevScale.sx, 
                                 Scale.sy / PrevScale.sy, 1) == GS_BAD)
                  { pAttrib->close(); return GS_BAD; }

               pAttrib->close();
            }
         }
      }
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      double Prev;

      if (XScale == 1 && YScale == 1) return GS_GOOD;

      Res = ((AcDbText *) pEnt)->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      // Nuova larghezza
      Prev = ((AcDbText *)pEnt)->widthFactor();
      if (((AcDbText *)pEnt)->setWidthFactor(Prev * XScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
      // Nuova altezza
      Prev = ((AcDbText *)pEnt)->height();
      if (((AcDbText *)pEnt)->setHeight(Prev * YScale) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidH_text; return GS_BAD; }
   }
   // da aggiungere i tipi di oggetti man mano che servono

   return GS_GOOD;
}
int gsc_set_scale(ads_name ent, double XScale, double YScale, double ZScale, 
                  bool AbsScale, bool AttribRepositioning)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_scale((AcDbEntity *) pObj, XScale, YScale, ZScale,
                     AbsScale, AttribRepositioning) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_width                          <external> */
/*+                                                                       
  Funzione che setta la larghezza di un oggetto grafico esistente.
  Parametri:
  ads_name ent;      Oggetto da analizzare
  double   width;    Larghezza
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_set_width(ads_name ent, double width)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_width((AcDbEntity *) pObj, width) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_set_width(AcDbEntity *pEnt, double width)
{
   Acad::ErrorStatus Res;

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
         {
            if (pVertex->startWidth() != width || pVertex->endWidth() != width)
            {
               if (pVertex->upgradeOpen() != Acad::eOk)
                  { pVertex->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

               pVertex->setStartWidth(width);
               pVertex->setEndWidth(width);
            }
         }
         pVertex->close();
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      double ConstWidth;
      
      Res = ((AcDbPolyline *) pEnt)->getConstantWidth(ConstWidth);
      if (Res == Acad::eInvalidInput ||   // the polyline doesn't have constant width
          (Res == Acad::eOk && ConstWidth != width))
      {
         Res = ((AcDbPolyline *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbPolyline *) pEnt)->setConstantWidth(width) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   
   return GS_GOOD;
}
/*********************************************************/
/*.doc gsc_get_width                          <external> */
/*+                                                                       
  Funzione che legge la larghezza di un oggetto grafico esistente.
  Parametri:
  ads_name ent;      Oggetto da analizzare
  double   *width;    Larghezza
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_width(ads_name ent, double *width)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_width((AcDbEntity *) pObj, width) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_width(AcDbEntity *pEnt, double *width)
{
   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {
      AcDbObjectIterator *pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();
      AcDbObjectId       vertexObjId;
      AcDb2dVertex       *pVertex;

      // Leggo solo il width iniziale del primo vertice
      vertexObjId = pVertIter->objectId();
      if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         *width = pVertex->startWidth();
      pVertex->close();
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      if (((AcDbPolyline *) pEnt)->getConstantWidth(*width) == Acad::eInvalidInput)
      {
         double dummy;

         // the polyline doesn't have constant width
         // Leggo solo il width iniziale del primo vertice
         if (((AcDbPolyline *) pEnt)->getWidthsAt(0, *width, dummy) != Acad::eOk)
            *width = 0;
      }
   }
   else
      *width = 0;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_reverseCurve                       <external> */
/*+                                                                       
  Funzione che imposta i vertici al contrario (l'ultimo diventa il primo e viceversa).
  Parametri:
  ads_name ent;      Oggetto grafico
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_reverseCurve(ads_name ent)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (((AcDbCurve *) pObj)->reverseCurve() != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*+
-------------------------------------------------------------
int gsc_set_hText(ads_name ent, double htext)

Setta l'altezza testo.
-------------------------------------------------------------
-*/
int gsc_set_hText(AcDbEntity *pEnt, double htext)
{
   if (htext <= 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (htext != ((AcDbText *)pEnt)->height())
      {
         Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbText *)pEnt)->setHeight(htext) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      if (htext != ((AcDbMText *) pEnt)->textHeight())
      {
         Acad::ErrorStatus Res = ((AcDbMText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbMText *)pEnt)->setTextHeight(htext) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      if (htext != ((AcDbAttribute *)pEnt)->height())
      {
         Acad::ErrorStatus Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttribute *)pEnt)->setHeight(htext) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      if (htext != ((AcDbAttributeDefinition *)pEnt)->height())
      {
         Acad::ErrorStatus Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttributeDefinition *)pEnt)->setHeight(htext) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }

   return GS_GOOD;
}
int gsc_set_hText(ads_name ent, double htext)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   int Res = gsc_set_hText((AcDbEntity *) pObj, htext);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return Res;
}


/*+
-------------------------------------------------------------
int gsc_Hscale(ads_point pp, ads_matrix mat)
    Funzione usata dal DRAGGEN di gsc_insert_text per la variazione
    di HTEXT (scala X = Y) dei testi escluso i giustificati con "adaTta".
-------------------------------------------------------------
-*/
int gsc_Hscale(ads_point pp, ads_matrix mat)
{
   ads_real  D;
   ads_point tp;

   // generate uniformly scaled matrix
   YSCALE = gsc_dist(pp, BASE);
   D      = YSCALE / _SCALE;
   gsc_mat_scale(D, D, 1, mat);

   // calculate new translation point
   gsc_mat_x_vec(mat, BASE, tp);
   gsc_subvec(BASE, tp, tp);
   mat[X][T] = tp[X];
   mat[Y][T] = tp[Y];
   mat[Z][T] = tp[Z];

   return RTNORM;
}

/*+
-------------------------------------------------------------
int gsc_HYscale(ads_point pp, ads_matrix mat)
    Funzione usata dal DRAGGEN di gsc_insert_text per la variazione
    di HTEXT (scala solo Y) dei testi giustificati con "adaTta".
-------------------------------------------------------------
-*/
int gsc_HYscale(ads_point pp, ads_matrix mat)
{
   ads_real   D;
   ads_point  tp;
   ads_matrix tmp;

    /* generate the rotation matrix */
    gsc_mat_rot(ROTA,Z, tmp);

    /* scaleY the matrix */
    YSCALE = gsc_dist(pp, BASE);
    D      = YSCALE / _SCALE;
    gsc_mat_scale(1,D, 1, mat);
    gsc_mat_x_mat(tmp,mat,mat);

        /* rotate the matrix */
    gsc_mat_rot(-ROTA,Z, tmp);
    gsc_mat_x_mat(mat,tmp,mat);

        /* calculate new translation point */
    gsc_mat_x_vec(mat, BASE, tp);
    gsc_subvec(BASE, tp, tp);
    mat[X][T] = tp[X];
    mat[Y][T] = tp[Y];
    mat[Z][T] = tp[Z];

    return RTNORM;
}


/*+
-------------------------------------------------------------
int gsc_ddscale_block(ads_name ent,ads_real sca)

  Chiede all'utente di variare la scala di un blocco (scalaX=scalaY).
  Ritorna GS_GOOD in caso di operazione corretta, GS_CAN
  in caso di annullamento, GS_BAD in caso di errore.
  
  - ent e' l'entita' da scalare.
  - sca e' la scala corrente del blocco.
  
-------------------------------------------------------------
-*/
int gsc_ddscale_block(ads_name ent, ads_real sca, ads_real *new_sca)
{
   ads_name  sel;
   ads_point pt;
   int       rc,flag;
   TCHAR     str[133],prompt[50];
   double    sc;

   // Seleziona il blocco
   if (acedSSAdd(ent,NULL,sel)!=RTNORM) 
      { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }

   // Imposta punto base = punto di inserimento
   if (gsc_get_firstPoint(ent, BASE)==GS_BAD)
      { ads_ssfree(sel); return GS_BAD; }

   _SCALE = 1;
   sc     = sca;
   // Imposta fattore di scala fattore di scala nel piano XY //
   swprintf(prompt, 50, gsc_msg(21), sca);
   do
   {
      flag = 0;
      acedInitGet(RSG_OTHER, GS_EMPTYSTR);
      
      rc = ads_draggen(sel,prompt,0,gsc_Hscale,pt);
      switch(rc)
      {
         case RTNORM :
           if (YSCALE==0)
           {
              acutPrintf (gsc_msg(17));
              flag=1; break;
           }
           if (new_sca!=NULL) *new_sca=sca*YSCALE;
           if (gsc_set_scale(ent, sca*YSCALE, sca*YSCALE, 1)==GS_BAD)
              { ads_ssfree(sel); return GS_BAD; }
           break;

         case RTSTR  :
           ads_getinput(str);
           if ((sc = _wtof(str))==0.0) 
           {
              acutPrintf (gsc_msg(17));
              flag=1; break;
           }

         case RTNONE : 
           if (new_sca!=NULL) *new_sca=sc;
           if (gsc_set_scale(ent, sc, sc, 1)==GS_BAD)
              { ads_ssfree(sel); return GS_BAD; }
           break;

         case RTCAN :
           ads_ssfree(sel);
           return GS_CAN;     
           
         case RTERROR :
         default :
           ads_ssfree(sel);
           return GS_BAD;
      }      
   }
   while(flag==1);
   ads_ssfree(sel);

   return GS_GOOD;
}


/*+
-------------------------------------------------------------
int gsc_ddrotate_block(ads_name ent,ads_real rot)

  Chiede all'utente di ruotare un blocco intorno all'asse Z.
  Ritorna GS_GOOD in caso di operazione corretta, GS_CAN
  in caso di annullamento, GS_BAD in caso di errore.
  
  - ent e' l'entita' da scalare.
  - rot e' la rotazione corrente del blocco.
  
-------------------------------------------------------------
-*/
int gsc_ddrotate_block(ads_name ent,ads_real rot,ads_real *new_rot)
{
   ads_name  sel;
   ads_point pt;
   int       rc;
   TCHAR     str[133], prompt[50];
   double    rt;

   // Seleziona il blocco
   if (acedSSAdd(ent,NULL,sel)!=RTNORM) 
      { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }

   // Imposta punto base=punto di inserimento
   if (gsc_get_firstPoint(ent, BASE)==GS_BAD)
      { ads_ssfree(sel); return GS_BAD; }

   // Imposta rotazione nel piano XY (intorno asse Z)
   rt=(rot/180)*PI;
   swprintf(prompt, 50, gsc_msg(19), rot); // "\nAngolo di rotazione <%f>: "

   acedInitGet(RSG_OTHER, GS_EMPTYSTR);
   ads_point_set(BASE,GS_POINT_CURSOR);
   acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
   rc = ads_draggen(sel,prompt,0,gsc_Zrotate,pt);
   acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

   switch(rc)
      {
      case RTNORM :
           if (new_rot!=NULL) *new_rot=rot+(ROTA*180)/PI;
           if (gsc_set_rotation(ent, ROTA + rt, GS_BAD) == GS_BAD)
              { ads_ssfree(sel); return GS_BAD; }
           break; 

      case RTSTR :
           ads_getinput(str);
           rt = _wtof(str);
           rt = (rt/180)*PI;
      case RTNONE :
           if (new_rot!=NULL) *new_rot=(rt*180)/PI;
           if (gsc_set_rotation(ent, rt, GS_BAD) == GS_BAD)
              { ads_ssfree(sel); return GS_BAD; }
           break;

      case RTCAN :
           ads_ssfree(sel);
           return GS_CAN;     

      case RTERROR :
      default :
           ads_ssfree(sel);
           return GS_BAD;
      }

   ads_ssfree(sel);     
   return GS_GOOD;   
}


/*********************************************************/
/*.doc gsc_insert_block                       <internal> */
/*+
  Inserisce una entita' di tipo blocco replicando il comportamento del comando 
  autocad "_.INSERT" (chiede il valore degli eventuali attributi).
  Parametri:
  TCHAR *BlkName;         nome blocco
  ads_point InsPt;        punto di inserimento
  double xScale;          fattore di scala X (default = 1)
  double yScale;          fattore di scala Y (default = 1)
  double Rot;             angolo di rotazione in gradi (default = 0)

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_block(const TCHAR *BlkName, ads_point InsPt, double xScale,
                     double yScale, double Rot)
{
   AcDbObjectId   BlockId;
   AcDbBlockTable *pBlockTable;
   AcGePoint3d    basePoint;

   // Open the block table for read.
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->getAt(BlkName, BlockId) != Acad::eOk)
   {
      pBlockTable->close();
      GS_ERR_COD = eGSInvalidBlock;
      return GS_BAD;
   }

   AcDbBlockTableRecord *pBlockTableRecord;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   pBlockTable->close();

   AcDbBlockReference *pBlkRef = gsc_create_block(BlockId, InsPt, xScale, yScale, Rot);

   AcDbObjectId newEntId;
   pBlockTableRecord->appendAcDbEntity(newEntId, pBlkRef);

   // Open the block definition for read.
   AcDbBlockTableRecord *pBlockDef;
   acdbOpenObject(pBlockDef, BlockId, AcDb::kForRead);

   // Set up a block table record iterator to iterate
   // over the attribute definitions.
   AcDbBlockTableRecordIterator *pIterator;
   pBlockDef->newIterator(pIterator);

   AcDbEntity              *pEnt;
   AcDbAttributeDefinition *pAttdef;
   TCHAR                   InputStr[132];
   bool                    FirstTime = TRUE;
   C_STRING                Msg;

   for (pIterator->start(); !pIterator->done(); pIterator->step())
   {
      // Get the next entity.
      pIterator->getEntity(pEnt, AcDb::kForRead);

      // Make sure the entity is an attribute definition
      // and not a constant.
      pAttdef = AcDbAttributeDefinition::cast(pEnt);

      if (pAttdef != NULL && !pAttdef->isConstant())
      {         
         // We have a non-constant attribute definition,
         // so build an attribute entity.
         AcDbAttribute *pAtt = new AcDbAttribute();
         pAtt->setPropertiesFrom(pAttdef);
         pAtt->setInvisible(pAttdef->isInvisible());

         // Translate the attribute by block reference.
         // To be really correct, the entire block
         // reference transform should be applied here.
         basePoint = pAttdef->position();
         basePoint += pBlkRef->position().asVector();
         pAtt->setPosition(basePoint);

         pAtt->setHeight(pAttdef->height());
         pAtt->setRotation(pAttdef->rotation());

         TCHAR *pTag, *pPrompt, *pDefaultText;
         
         if ((pTag = pAttdef->tag()))
         {
            pAtt->setTag(pTag);
            // Use the ObjectARX global memory function to release
		      // memory for the string returned by the SDK
            acutDelString(pTag);
         }

         pAtt->setFieldLength(pAttdef->fieldLength());

         if (FirstTime)
         {
            FirstTime = FALSE;
            acutPrintf(gsc_msg(105)); // "\nDigitare i valori di attributo:"
         }
         Msg = _T('\n');
         if ((pPrompt = pAttdef->prompt()))
            { Msg += pPrompt; acutDelString(pPrompt); }
         Msg += _T(" <");
         if ((pDefaultText = pAttdef->textString()))
            Msg += pDefaultText;
         Msg += _T(">: ");

         wcscpy(InputStr, GS_EMPTYSTR);
         if (acedGetString(0, Msg.get_name(), InputStr) == RTNORM)
            pAtt->setTextString((wcslen(InputStr) == 0) ? pDefaultText : InputStr);

         if (pDefaultText) acutDelString(pDefaultText);

         AcDbObjectId attId;

         pBlkRef->appendAttribute(attId, pAtt);
         pAtt->close();
      }
      pEnt->close(); // use pEnt... pAttdef might be NULL
   }
   delete pIterator;
   pBlockDef->close();
   pBlkRef->close();
   pBlockTableRecord->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_insert_block                       <internal> */
/*+
  Inserisce una entita' di tipo blocco (name) nel punto di inserimento 
  point, se questo e' NULL viene richiesto da input.
  sca e rot sono i valori di default rispettivamenti per 
  scala e rotazione del blocco. Il comando e' molto simile al
  corrispondente _INSERT di Autocad.
  Parametri:
  TCHAR *name;            nome blocco
  ads_point point;        punto di inserimento
  double sca;             fattore di scala
  double rot;             angolo di rotazione in gradi
  C_CLASS *ClassToCheckOverlap;  usato solo se point = NULL e se l'inserimento
                                 deve controllare la sovrapposizione 
                                 (default = NULL)

  Ritorna GS_GOOD in caso di inserimento corretto, GS_CAN in caso di
  comando annullato dall'utente o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_insert_block(TCHAR *name, ads_point point, double sca, double rot,
                     C_CLASS *ClassToCheckOverlap)
{
   ads_name  ent, sel;
   ads_point pt, tmp_point;
   int       rc, flag;
   TCHAR     str[133], prompt[50];
   double    xs, ys, rt;

   if (name==NULL || sca==0)
      { GS_ERR_COD=eGSInvalidArg; return GS_BAD; }
   
   if (gsc_validblock(name) == GS_BAD) return GS_BAD;

   if (point==NULL)
   {
      acedInitGet(RSG_NONULL, GS_EMPTYSTR);
      do
      {  // "\nPunto di inserimento: "
         while ((rc = acedGetPoint(NULL, gsc_msg(20), tmp_point)) == RTNONE)
            acutPrintf(gsc_msg(107)); // "\nPunto non valido."
         if (rc == RTERROR) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
         if (rc == RTCAN) return GS_CAN;
         if (ClassToCheckOverlap) // se e si devono controllare le sovrapposizioni
         {
            // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
            if (gsc_OverlapValidation(tmp_point, ClassToCheckOverlap) == GS_GOOD) break;
            acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
         }
         else break;
      }
      while (1);
   }
   else
      ads_point_set(point, tmp_point);

   if (gsc_insert_block(name, tmp_point, sca, sca, rot) != GS_GOOD)
     return GS_BAD;
      
   // Seleziona il blocco appena inserito
   if (acdbEntLast(ent)!=RTNORM) 
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (acedSSAdd(ent, NULL, sel) != RTNORM) 
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   // Imposta punto base = punto di inserimento
   if (gsc_get_firstPoint(ent, BASE) == GS_BAD) { ads_ssfree(sel); return GS_BAD; }

   ROTA = (rot / 180) * PI;

   if (GEOsimAppl::GLOBALVARS.get_InsXScale() == MANUAL) // impostazione manuale del fattore di scala X
   {
      // Imposta fattore di scala fattore di scala nel piano XY
      swprintf(prompt, 50, gsc_msg(21), sca); // "\nFattore di scala X <%f>: "
      do
      {
         flag=0; xs=sca;
         acedInitGet(RSG_OTHER, GS_EMPTYSTR);
      
         rc = ads_draggen(sel,prompt,0,gsc_XYscale,pt);
         switch(rc)
         {
            case RTNORM :
               if (XSCALE==0 || YSCALE==0)
               {
                  acutPrintf (gsc_msg(17)); // "\n Il valore deve essere diverso da zero."
                  flag=1; break;
               }
               if (gsc_set_scale(ent, sca*XSCALE, sca*YSCALE, 1)==GS_BAD)
                  { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
               break;
            case RTSTR  :
               ads_getinput(str);
               if ((xs = _wtof(str))==0.0) 
               {
                  acutPrintf(gsc_msg(17)); // "\n Il valore deve essere diverso da zero."
                  flag=1; break;
               }
            case RTNONE :
               ys=xs;
               if (GEOsimAppl::GLOBALVARS.get_InsYScale() == MANUAL) // impostazione manuale del fattore di scala X
               {
                  do
                  {
                     ads_getstring(0,gsc_msg(22),str); // "\nFattore di scala per Y (standard=X): "            
                     if (gsc_strcmp(str, GS_EMPTYSTR) == 0) ys=xs;
                     else 
                     {
                        if ((ys = _wtof(str)) == 0.0) 
                           acutPrintf (gsc_msg(17)); // "\n Il valore deve essere diverso da zero."
                     }
                  }
                  while (ys == 0.0);
                  if (gsc_set_scale(ent, xs, ys, 1) == GS_BAD)
                     { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
               }
               break;
            case RTCAN :
               ads_ssfree(sel);
               gsc_EraseEnt(ent);
               return GS_CAN;     
            case RTERROR :
            default :
               ads_ssfree(sel);
               gsc_EraseEnt(ent);
               return GS_BAD;
         }      
      }
      while(flag==1);
   }

   if (GEOsimAppl::GLOBALVARS.get_InsRotaz() == MANUAL) // impostazione manuale della rotazione
   {
      // Imposta rotazione nel piano XY (intorno asse Z)
      rt=(rot/180)*PI;
      swprintf(prompt, 50, gsc_msg(19), rot); // "\nAngolo di rotazione <%f>: "

      acedInitGet(RSG_OTHER,GS_EMPTYSTR);
      ads_point_set(BASE,GS_POINT_CURSOR);
      acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
      rc = ads_draggen(sel,prompt,0,gsc_Zrotate,pt);
      acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

      switch(rc)
      {
         case RTNORM :
            if (gsc_set_rotation(ent, rt + ROTA, GS_BAD)==GS_BAD)
               { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
            break; 
         case RTSTR :
            ads_getinput(str);
            rt=_wtof(str);
            rt=(rt/180)*PI;
         case RTNONE :
            if (gsc_set_rotation(ent, rt, GS_BAD)==GS_BAD)
               { ads_ssfree(sel); gsc_EraseEnt(ent); return GS_BAD; }
            break;
         case RTCAN :
            gsc_EraseEnt(ent);
            ads_ssfree(sel);
            return GS_CAN;
         case RTERROR :
         default :
            gsc_EraseEnt(ent);
            ads_ssfree(sel);
            return GS_BAD;
      }
   }

   ads_ssfree(sel);

   return GS_GOOD;   
}


/*********************************************************/
/*.doc gsc_create_block                       <external> */
/*+
  Crea un oggetto blocco senza inserirlo nel DB grafico di AutoCAD.
  Parametri:
  AcDbObjectId &BlockId;  Identificatore del blocco
  ads_point InsPt;        Punto di inserimento
  double xScale;          Fattore di scala X (default = 1)
  double yScale;          Fattore di scala Y (default = 1)
  double Rot;             Angolo di rotazione in gradi (default = 0)

  Restituisce il puntatore all'oggetto grafico in caso di successo
  altrimenti restituisce NULL. 
-*/  
/*********************************************************/
AcDbBlockReference *gsc_create_block(AcDbObjectId &BlockId, ads_point InsPt, double xScale,
                                     double yScale, double Rot)
{
   AcGePoint3d basePoint(InsPt[X], InsPt[Y], InsPt[Z]);
   AcGeScale3d scale(xScale, yScale, 1.0);

   AcDbBlockReference *pBlkRef = new AcDbBlockReference(basePoint, BlockId);
   pBlkRef->setDatabaseDefaults();
   if (pBlkRef->setRotation(gsc_grd2rad(Rot)) != Acad::eOk)
      { delete pBlkRef; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pBlkRef->setScaleFactors(scale) != Acad::eOk)
      { delete pBlkRef; GS_ERR_COD = eGSInvalidScale; return NULL; }

   return pBlkRef;
}


/*********************************************************/
/*.doc gsc_create_attribute                   <external> */
/*+
  Crea un oggetto attributo senza inserirlo nel DB grafico di AutoCAD.
  Parametri:
  TCHAR *Txt;            Testo
  const TCHAR *AttrName; Nome dell'attributo
  ads_point InsPt;       Punto di inserimento
  double Height;         Altezza testo (default = 1)
  double Rot;            Rotazione in gradi (default = 0)
  AcDbObjectId &StyleId; Identificatore dello stile testo 
                         (default = AcDbObjectId::kNull)
  double Thickness;      Spessore (default = 0)
  double Width;          Fattore di scala X (default = 1)
  double OblAng;         Angolo di testo obliquo in gradi (default = 0)
  TextHorzMode HorzMode; Allineamento orizzontale (default = kTextLeft)
  TextVertMode VertMode; Allineamento verticale (default = kTextBase)

  Restituisce il puntatore all'oggetto grafico in caso di successo
  altrimenti restituisce NULL. 
-*/  
/*********************************************************/
AcDbAttribute *gsc_create_attribute(const TCHAR *Txt, const TCHAR *AttrName,
                                    ads_point InsPt, double Height, 
                                    double Rot, const AcDbObjectId &StyleId, 
                                    double Thickness, double Width, double OblAng,
                                    TextHorzMode HorzMode, TextVertMode VertMode)
{
   AcDbAttribute *pAtt;
   AcGePoint3d   BasePoint(InsPt[X], InsPt[Y], InsPt[Z]);

   if ((pAtt = new AcDbAttribute(BasePoint, Txt, AttrName, StyleId)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   if (pAtt->setRotation(gsc_grd2rad(Rot)) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pAtt->setHeight(Height) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidH_text; return NULL; }
   if (pAtt->setThickness(Thickness) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidThickness; return NULL; }
   if (pAtt->setWidthFactor(Width) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidWidth; return NULL; }
   if (pAtt->setOblique(gsc_grd2rad(OblAng)) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pAtt->setHorizontalMode(HorzMode) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (pAtt->setVerticalMode(VertMode) != Acad::eOk)
      { delete pAtt; GS_ERR_COD = eGSInvalidRotation; return NULL; }
   if (HorzMode != AcDb::kTextLeft || VertMode != AcDb::kTextBase)
      if (pAtt->setAlignmentPoint(BasePoint) != Acad::eOk)
         { delete pAtt; GS_ERR_COD = eGSInvGraphObjct; return NULL; }

   pAtt->close();

   return pAtt;
}


/*+
-------------------------------------------------------------
int gsc_XYscale(ads_point pp, ads_matrix mat)
    Funzione usata dal DRAGGEN di gsc_insert_block
    per la variazione della scala X e Y.
-------------------------------------------------------------
-*/
int gsc_XYscale(ads_point pp, ads_matrix mat)
{
   ads_point tp;
   ads_matrix tmp;

        /* generate uniformly scaled matrix */
    XSCALE = pp[0] - BASE[0];
    YSCALE = pp[1] - BASE[1];

        /* generate the scale-rotation matrix */
    gsc_mat_rot(ROTA,Z, tmp);
    gsc_mat_scale(XSCALE,YSCALE, 1, mat);
    gsc_mat_x_mat(tmp,mat,mat);
    gsc_mat_rot(-ROTA,Z, tmp);
    gsc_mat_x_mat(mat,tmp,mat);

        /* calculate new translation point */
    gsc_mat_x_vec(mat, BASE, tp);
    gsc_subvec(BASE, tp, tp);
    mat[X][T] = tp[X];
    mat[Y][T] = tp[Y];
    mat[Z][T] = tp[Z];

    return RTNORM;
}


/*+
-------------------------------------------------------------
int gsc_Zrotate(ads_point pp, ads_matrix mat)
    Funzione usata dal DRAGGEN di gsc_insert_block e gsc_insert_text
    per la variazione della rotazione intorno all'asse Z.
-------------------------------------------------------------
-*/
int gsc_Zrotate(ads_point pp, ads_matrix mat)
{
   ads_real  angle;
   ads_point tp;

	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
	ads_point_set(pp,GS_POINT_CURSOR);
	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

   // calculate angle of line between LastPoint and tp
   angle = acutAngle(BASE, pp);

   // generate rotation matrix about axis
   gsc_mat_rot(angle,Z, mat);

   // calculate new translation point
   gsc_mat_x_vec(mat, BASE, tp);
   gsc_subvec(BASE, tp, tp);
   mat[X][T] = tp[X];
   mat[Y][T] = tp[Y];
   mat[Z][T] = tp[Z];
    
   ROTA = angle;
   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//  FUNZIONI GENERICE OPERAZIONI SU VETTORI E MATRICI //
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/******************************************************************************/
/*.doc gsc_mat_ident(internal) */
/*+
    Set up an identity matrix.
-*/
/******************************************************************************/
void gsc_mat_ident(ads_matrix matrix)
{
    memcpy(matrix, ads_identmat, sizeof(ads_matrix));
}

/******************************************************************************/
/*.doc gsc_subvec(internal) */
/*+
    Subtract two ads_points.
-*/
/******************************************************************************/
void gsc_subvec(ads_point ap, ads_point bp, ads_point dp)
{
    dp[X] = ap[X] - bp[X];
    dp[Y] = ap[Y] - bp[Y];
    dp[Z] = ap[Z] - bp[Z];
}


/******************************************************************************/
/*.doc gsc_fabsv(internal) */
/*+
    Get normal of a vector.
-*/
/******************************************************************************/
ads_real gsc_fabsv(ads_point ap)
{
    return sqrt(ap[X] * ap[X] + ap[Y] * ap[Y] + ap[Z] * ap[Z]);
}


/******************************************************************************/
/*.doc gsc_dist                                                               */
/*+
    Calculate distance between two points.
-*/
/******************************************************************************/
ads_real gsc_dist(ads_point p1, ads_point p2)
{
   return acutDistance(p1, p2);
}
ads_real gsc_dist(AcGePoint3d &p1, AcGePoint3d &p2)
   { return p1.distanceTo(p2); }
ads_real gsc_dist(AcGePoint2d &p1, AcGePoint2d &p2)
   { return p1.distanceTo(p2); }
ads_real gsc_dist(C_POINT *p1, AcGePoint3d &p2)
{
   ads_point _p2;
   
   ads_point_set_from_AcGePoint3d(p2, _p2);

   return gsc_dist(p1->point, _p2);
}
ads_real gsc_dist(ads_point p1, AcGePoint3d &p2)
{
   ads_point _p2;
   
   ads_point_set_from_AcGePoint3d(p2, _p2);

   return gsc_dist(p1, _p2);
}


/******************************************************************************/
/*.doc gsc_dist2d                                                             */
/*+
    Calcola la distanza di due punti in piano senza considerare la Z.
-*/
/******************************************************************************/
ads_real gsc_dist2d(ads_point p1, ads_point p2)
{
   ads_point p1_2d, p2_2d;

   ads_2Dpoint_set(p1, p1_2d);
   ads_2Dpoint_set(p2, p2_2d);
   
   return gsc_dist(p1_2d, p2_2d);
}


/******************************************************************************/
/*.doc gsc_angle                                                              */
/*+
    Calcola il coefficiente angolare (in radianti) del segmento p1 p2.
-*/
/******************************************************************************/
ads_real gsc_angle(AcGePoint3d &p1, AcGePoint3d &p2)
{
   ads_point _p1, _p2;

   ads_point_set_from_AcGePoint3d(p1, _p1);
   ads_point_set_from_AcGePoint3d(p2, _p2);

   return acutAngle(_p1, _p2);
}
ads_real gsc_angle(AcGePoint2d &p1, AcGePoint2d &p2)
{
   ads_point _p1, _p2;

   ads_point_set_from_AcGePoint2d(p1, _p1);
   ads_point_set_from_AcGePoint2d(p2, _p2);

   return acutAngle(_p1, _p2);
}


/******************************************************************************/
/*.doc gsc_polar                                                              */
/*+
    Calcola le coordinate di un punto in coordinate polari da un punto noto.
-*/
/******************************************************************************/
void gsc_polar(AcGePoint3d &p1, double angle, double dist, AcGePoint3d &p2)
{
   ads_point _p1, _p2;

   ads_point_set_from_AcGePoint3d(p1, _p1);
   ads_point_set_from_AcGePoint3d(p2, _p2);

   acutPolar(_p1, angle, dist, _p2);
   p2.set(_p2[X], _p2[Y], _p2[Z]);
}
void gsc_polar(AcGePoint2d &p1, double angle, double dist, AcGePoint2d &p2)
{
   ads_point _p1, _p2;

   ads_point_set_from_AcGePoint2d(p1, _p1);
   ads_point_set_from_AcGePoint2d(p2, _p2);

   acutPolar(_p1, angle, dist, _p2);
   p2.set(_p2[X], _p2[Y]);
}


/******************************************************************************/
/*.doc gsc_mat_ixlate(internal) */
/*+
    Set up a translation matrix.
-*/
/******************************************************************************/
int gsc_mat_ixlate(ads_point vec, ads_matrix result)
{
    gsc_mat_ident(result);
    result[X][T] = vec[X] - BASE[X];
    result[Y][T] = vec[Y] - BASE[Y];
    result[Z][T] = vec[Z] - BASE[Z];

    return RTNORM;
}

/******************************************************************************/
/*.doc gsc_mat_iscale(internal) */
/*+
    Set up a scale matrix.
-*/
/******************************************************************************/
int gsc_mat_iscale(ads_point vec, ads_matrix result)
{
   ads_real x, y, z;
   ads_real r;
   ads_point tp;

	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
	ads_point_set(vec,GS_POINT_CURSOR);
	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

   x = vec[X] - BASE[X];
   y = vec[Y] - BASE[Y];
   z = vec[Z] - BASE[Z];
   r = sqrt((x*x)+(y*y)+(z*z));
   r = r / _SCALE;

   gsc_mat_scale(r, r, r, result);

   // calculate new translation point
   gsc_mat_x_vec(result, BASE, tp);
   gsc_subvec(BASE, tp, tp);
   result[X][T] = tp[X];
   result[Y][T] = tp[Y];
   result[Z][T] = tp[Z];

   return RTNORM;
}


/******************************************************************************/
/*.doc gsc_mat_irotate(internal) */
/*+
    Set up a rotate matrix.
-*/
/******************************************************************************/
int gsc_mat_irotate(ads_point vec, ads_matrix result)
{
   ads_real        angle;
   ads_point       tp;

	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);
	ads_point_set(vec,GS_POINT_CURSOR);
	acedGrDraw(BASE,GS_POINT_CURSOR,-1,0);

// calculate angle of line between FirstPoint and vec 
   angle = acutAngle(BASE, vec);
   angle = angle - ROTA;

// generate rotation matrix about axis Z
   gsc_mat_rot(angle, Z, result);

// calculate new translation point 
   gsc_mat_x_vec(result, BASE, tp);
   gsc_subvec(BASE, tp, tp);
   result[X][T] = tp[X];
   result[Y][T] = tp[Y];
   result[Z][T] = tp[Z];

   return RTNORM;
}

/******************************************************************************/
/*.doc gsc_mat_rot(internal) */
/*+
    Generate a matrix that rotates about a given axis.
-*/
/******************************************************************************/
void gsc_mat_rot(ads_real angle, int axis, ads_matrix m)
{
    int axp1, axp2;
 
    axp1 = (axis + 1) % 3;
    axp2 = (axis + 2) % 3;
    gsc_mat_ident(m);
    m[axp1][axp1] = m[axp2][axp2] = cos(angle);
    m[axp1][axp2] = -(m[axp2][axp1] = sin(angle));
}

/******************************************************************************/
/*.doc gsc_mat_scale(internal) */
/*+
    Generate a matrix that scales the 3 axes by given amounts.
-*/
/******************************************************************************/
void gsc_mat_scale(ads_real xscale, ads_real yscale, ads_real zscale, ads_matrix m)
{
    gsc_mat_ident(m);
    m[X][X] = xscale;
    m[Y][Y] = yscale;
    m[Z][Z] = zscale;
}
 
/******************************************************************************/
/*.doc gsc_mat_x_pt(internal) */
/*+
    Multiply matrix by a given point.  Note that it does translation.
-*/
/******************************************************************************/
void gsc_mat_x_pt(ads_matrix mat, ads_point pin, ads_point pout)
{
    int i;
    ads_point temp;

    for (i = X; i <= Z; i++)
        temp[i] = mat[i][X] * pin[X] +
                  mat[i][Y] * pin[Y] +
                  mat[i][Z] * pin[Z] +
                  mat[i][T];
    memcpy(pout, temp, sizeof(ads_point));
}

/******************************************************************************/
/*.doc gsc_mat_x_vec(internal) */
/*+
    Multiply matrix by a given vector.  Note that it does NO translation.
-*/
/******************************************************************************/
void gsc_mat_x_vec(ads_matrix mat, ads_point pin, ads_point pout)
{
    int i;
    ads_point temp;
 
    for (i = X; i <= Z; i++)
        temp[i] = mat[i][X] * pin[X] +
                  mat[i][Y] * pin[Y] +
                  mat[i][Z] * pin[Z];
    memcpy(pout, temp, sizeof(ads_point));
}
 
/******************************************************************************/
/*.doc gsc_mat_x_mat(internal) */
/*+
    Multiply two matrices.  Any or all arguments may point to the same array.
-*/
/******************************************************************************/
void gsc_mat_x_mat(ads_matrix mata, ads_matrix matb, ads_matrix matout)
{
    ads_matrix t;
    int i, j, k;
    ads_real sum;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            sum = 0.0;
            for (k=0; k<4; k++)
                sum += mata[i][k] * matb[k][j];
            t[i][j] = sum;
        }   
    memcpy(matout, t, sizeof(ads_matrix));
}
 

/*+
-------------------------------------------------------------
   FUNZIONI PER IL SETTAGGIO DELLA VARIABILI DI SISTEMA
   AUTOCAD RELATIVE ALLA GRAFICA :
   
   int gsc_set_CurrentLineType (TCHAR *name,TCHAR *old);
   int gsc_set_CurrentTextStyle (TCHAR *name,TCHAR *old);

   int gsc_set_CurrentElevation (double elev,double *old);
   int gsc_set_hText (double htext,double *old);
   int gsc_set_width (double width,double *old);
   int gsc_set_CurrentThickness (double thick,double *old);

   int gsc_set_layer (TCHAR *name,TCHAR *old);
   int gsc_set_echo (int echo,int *old);
   int gsc_set_expert (int exp,int *old);

   int gsc_set_env_cmd();
   int gsc_reset_env_cmd();
  
-------------------------------------------------------------
-*/
int gsc_set_env_cmd()
{
   if (gsc_set_echo(0,&GS_LAST_ECHO)==GS_BAD) return GS_BAD;
   if (gsc_set_expert(5,&GS_LAST_EXPERT)==GS_BAD) return GS_BAD;
   return GS_GOOD;
}

int gsc_reset_env_cmd()
{
   if (gsc_set_echo(GS_LAST_ECHO,NULL)==GS_BAD) return GS_BAD;
   if (gsc_set_expert(GS_LAST_EXPERT,NULL)==GS_BAD) return GS_BAD;
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_acedGetStrVar(const TCHAR *var, C_STRING &Value)
{
   resbuf rb;

   if (acedGetVar(var, &rb) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   if (rb.restype == RTSTR && rb.resval.rstring)
      { Value = rb.resval.rstring; free(rb.resval.rstring); }
   else
      Value.clear();
   
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_CurrentLTScale(double ltscale, double *old)
{
   resbuf *rb;

   // Memorizza vecchio valore in old
   if (old)
   {
      if ((rb = acutBuildList(RTNIL,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("LTSCALE"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTREAL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old = rb->resval.rreal;
      acutRelRb(rb); 
   }                              

   if (gsc_validscale(ltscale) == GS_BAD) return GS_BAD;

   if ((rb = acutBuildList(RTREAL,ltscale, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("LTSCALE"), rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int gsc_set_CurrentElevation (double elev,double *old)
{
   resbuf *rb;

   if (gsc_validelevation(elev)==GS_BAD) return GS_BAD;

   // Memorizza vecchio valore in old
   if (old!=NULL)
   {
      if ((rb=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("ELEVATION"), rb)!=RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype!=RTREAL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old=rb->resval.rreal;
      acutRelRb(rb); 
   }                              

   if ((rb=acutBuildList(RTREAL,elev,0))==NULL)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("ELEVATION"), rb)!=RTNORM)
      { GS_ERR_COD=eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_CurrentHText(double htext, double *old)
{
   resbuf *rb;

   if (gsc_validhtext(htext)==GS_BAD) return GS_BAD;

   // Memorizza vecchio valore in old
   if (old!=NULL)
   {
      if ((rb=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("TEXTSIZE"), rb)!=RTNORM)
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype!=RTREAL)   
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old=rb->resval.rreal;
      acutRelRb(rb); 
   }                              

   if ((rb=acutBuildList(RTREAL,htext,0))==NULL)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("TEXTSIZE"), rb)!=RTNORM)
      { GS_ERR_COD=eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_width(double width, double *old)
{
   resbuf *rb;

   if (gsc_validwidth(width)==GS_BAD) return GS_BAD;

   // Memorizza vecchio valore in old
   if (old!=NULL)
   {
      if ((rb=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("PLINEWID"), rb)!=RTNORM)
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype!=RTREAL)   
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old=rb->resval.rreal;
      acutRelRb(rb); 
      }                              

   if ((rb=acutBuildList(RTREAL,width,0))==NULL)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("PLINEWID"), rb)!=RTNORM)
      { GS_ERR_COD=eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_CurrentThickness(double thick, double *old)
{
   resbuf *rb;

   if (gsc_validthickness(thick)==GS_BAD) return GS_BAD;

   // Memorizza vecchio valore in old //
   if (old!=NULL)
   {
      if ((rb=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("THICKNESS"), rb)!=RTNORM)
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype!=RTREAL)   
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old=rb->resval.rreal;
      acutRelRb(rb); 
      }                              

   if ((rb=acutBuildList(RTREAL,thick,0))==NULL)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("THICKNESS"), rb)!=RTNORM)
      { GS_ERR_COD=eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_expert(int exp, int *old)
{
   presbuf rb;

   if (exp < 0 || exp > 5) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Memorizza vecchio valore in old
   if (old)
   {
      if ((rb = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("EXPERT"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTSHORT)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old = rb->resval.rint;
      acutRelRb(rb); 
   }                              

   if ((rb = acutBuildList(RTSHORT, exp, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("EXPERT"), rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int gsc_set_echo(int echo, int *old)
{
   resbuf *rb;

   if (echo != 0 && echo != 1) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Memorizza vecchio valore in old
   if (old)
   {
      if ((rb=acutBuildList(RTNIL,0))==NULL)
         { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("CMDECHO"), rb)!=RTNORM)
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype!=RTSHORT)   
         { GS_ERR_COD=eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      *old=rb->resval.rint;
      acutRelRb(rb); 
   }                              

   if ((rb=acutBuildList(RTSHORT,echo,0))==NULL)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("CMDECHO"), rb) != RTNORM)
      { GS_ERR_COD=eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int gsc_set_cmddia(int cmddia, int *old)
{
   struct resbuf RbCmdDia;            // CMDDIA setting

   if (cmddia != 0 && cmddia != 1) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   RbCmdDia.restype = RTSHORT;

   // Memorizza vecchio valore in old
   if (old)
   {
      if (acedGetVar(_T("CMDDIA"), &RbCmdDia) != RTNORM) // Read current setting
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      *old = RbCmdDia.resval.rint;
   }                              

   RbCmdDia.resval.rint = cmddia;
   
   if (acedSetVar(_T("CMDDIA"), &RbCmdDia) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
int gsc_set_osmode(int osmode, int *old)
{
   struct resbuf RbOsMode;            // OSMODE setting

   RbOsMode.restype = RTSHORT;

   // Memorizza vecchio valore in old
   if (old)
   {
      if (acedGetVar(_T("OSMODE"), &RbOsMode) != RTNORM) // Read current setting
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      *old = RbOsMode.resval.rint;
   }                              

   RbOsMode.resval.rint = osmode;
   
   if (acedSetVar(_T("OSMODE"), &RbOsMode) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


int gsc_set_CLIPROMPTUPDATE(int new_value, int *old_value)
{
   /*
   From AutoCAD 2013, CLIPROMPTUPDATE system variable controls the whether the command line displays 
   the progress as a command or script is run.
   When CLIPROMPTUPDATE is set to 1, AutoCAD updates the command line during 
   the running of command or lisp routine.
   With Value set as 0, AutoCAD only updates the command line at the end of command or end of lisp routine
   */
   resbuf rb;

   if (new_value != 0 && new_value != 1) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Memorizza vecchio valore in old
   if (old_value)
   {
      acedGetVar(_T("CLIPROMPTUPDATE"), &rb);
      gsc_rb2Int(&rb, old_value);
   } 

   rb.restype = RTSHORT;
   rb.resval.rint = new_value;  
   if (acedSetVar(_T("CLIPROMPTUPDATE"), &rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}

/****************************************************************************/
/*.doc gsc_setenv_graph <internal> */
/*+
  Questa funzione setta le caratteristiche grafiche dell'ambiente corrente
  di ACAD (es. colore, layer, larghezza, spessore, tipolinea, stile testo, 
  stile quotatura, elevazione ...).
  Parametri:
  int cat;           categoria classe dell'oggetto di GEOsim da inserire
  int type;          tipo di classe dell'oggetto GEOsim da inserire
  C_FAS &NewFAS;     Nuova impostazione di FAS
  C_FAS *OldFAS;     Impostazione FAS precedente

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Non setta il colore e il layer del riempimento.
-*/
/******************************************************************************/
int gsc_setenv_graph(int cat, int type, C_FAS &NewFAS, C_FAS *OldFAS)
{
   C_COLOR  *color = NULL;
   double   *width = NULL, *thickness = NULL, *elevation = NULL, *h_text = NULL;
   double   *line_scale = NULL, *hatch_scale = NULL, *hatch_rotation = NULL;
   TCHAR    *layer = NULL, *line = NULL, *style = NULL, *hatch = NULL;
   C_STRING *dim_style = NULL;

   if (OldFAS)
   {
      color          = &OldFAS->color;
      width          = &OldFAS->width;
      thickness      = &OldFAS->thickness;
      elevation      = &OldFAS->elevation;
      h_text         = &OldFAS->h_text;
      layer          = OldFAS->layer;
      line           = OldFAS->line;
      line_scale     = &OldFAS->line_scale;
      style          = OldFAS->style;
      hatch          = OldFAS->hatch;
      hatch_scale    = &OldFAS->hatch_scale;
      hatch_rotation = &OldFAS->hatch_rotation;
      dim_style      = &OldFAS->dimension_style;
   }                         
                             
   if (NewFAS.layer)        
      if (gsc_set_CurrentLayer(NewFAS.layer, layer) == GS_BAD) return GS_BAD;
                             
   if (gsc_set_CurrentElevation(NewFAS.elevation, elevation) == GS_BAD) return GS_BAD;
                             
   if (cat == CAT_SPAGHETTI)  // insieme misto
   {                         
      if (gsc_set_CurrentColor(NewFAS.color, color) == GS_BAD) return GS_BAD;
      if (gsc_set_width(NewFAS.width, width) == GS_BAD) return GS_BAD;
      if (gsc_set_CurrentThickness(NewFAS.thickness, thickness) == GS_BAD) return GS_BAD;

      if (gsc_set_CurrentLineType(NewFAS.line, line) == GS_BAD) return GS_BAD;
      if (gsc_set_CurrentLTScale(NewFAS.line_scale, line_scale) == GS_BAD) return GS_BAD;

      if (gsc_set_CurrentHText(NewFAS.h_text, h_text) == GS_BAD) return GS_BAD;
      if (gsc_set_CurrentTextStyle(NewFAS.style, style) == GS_BAD) return GS_BAD;

      if (gsc_set_CurrentHatch(NewFAS.hatch, hatch) == GS_BAD) return GS_BAD;
      if (gsc_set_CurrentHatchRotation(NewFAS.hatch_rotation, hatch_rotation) == GS_BAD) return GS_BAD;
      if (gsc_set_CurrentHatchScale(NewFAS.hatch_scale, hatch_scale) == GS_BAD) return GS_BAD;

      if (gsc_set_CurrentDimStyle(NewFAS.dimension_style, dim_style) == GS_BAD) return GS_BAD;
   }
   else
      switch (type)
      {
         case TYPE_POLYLINE:
         case TYPE_SURFACE:
            if (gsc_set_CurrentColor(NewFAS.color, color) == GS_BAD) return GS_BAD;
            if (gsc_set_width(NewFAS.width, width) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentThickness(NewFAS.thickness, thickness) == GS_BAD) return GS_BAD;

            if (gsc_set_CurrentLineType(NewFAS.line, line) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentLTScale(NewFAS.line_scale, line_scale) == GS_BAD) return GS_BAD;

            if (gsc_set_CurrentHatch(NewFAS.hatch, hatch) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentHatchRotation(NewFAS.hatch_rotation, hatch_rotation) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentHatchScale(NewFAS.hatch_scale, hatch_scale) == GS_BAD) return GS_BAD;

            break;
         case TYPE_TEXT:
            if (gsc_set_CurrentColor(NewFAS.color, color) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentThickness(NewFAS.thickness, thickness) == GS_BAD) return GS_BAD;

            if (gsc_set_CurrentHText(NewFAS.h_text, h_text) == GS_BAD) return GS_BAD;
            if (gsc_set_CurrentTextStyle(NewFAS.style, style) == GS_BAD) return GS_BAD;
            break;
         case TYPE_NODE:
            if (gsc_set_CurrentColor(NewFAS.color, color) == GS_BAD) return GS_BAD;
            break;
         default:
            GS_ERR_COD = eGSInvClassType;
            return GS_BAD;
      }

   return GS_GOOD;
} 


// ACTION TILE : click su lista classi //
static void CALLB dcl_ent_listcls(ads_callback_packet *dcl)
{
   int        pos;
   ads_hdlg   hdlg;
   C_CLS_PUNT *punt;
   C_CLS_PUNT_LIST *pSelClsList;

   pSelClsList = (C_CLS_PUNT_LIST *) dcl->client_data;
   hdlg= dcl->dialog;
   pos = _wtoi(dcl->value);

   if ((punt = (C_CLS_PUNT *) pSelClsList->getptr_at(pos + 1)) == NULL)
      return;
   SEL_CLS = (C_CLASS *) punt->get_class();

   if (dcl->reason == CBR_DOUBLE_CLICK) ads_done_dialog(hdlg, DLGOK);
}
// ACTION TILE : click su tasto OK //
static void CALLB dcl_ent_accept_ok(ads_callback_packet *dcl)
{
   if (SEL_CLS != NULL) ads_done_dialog(dcl->dialog, DLGOK);
}
// ACTION TILE : click su tasto CANCEL //
static void CALLB dcl_ent_quitter(ads_callback_packet *dcl)
{
   SEL_CLS = NULL;
   ads_done_dialog(dcl->dialog, DLGCANCEL);         
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_ent_help(ads_callback_packet *dcl)
{
   //gsc_help();
} 


/*********************************************************/
/*.doc gsc_get_class_list                     <external> */
/*+                                                                       
  Dato un gruppo di selezione, ritorna la lista delle classi di
  apparatenenza di ciascuna entità di GEOsim.
  Parametri:
  ads_name        SelSet;     Gruppo di selezione
  C_CLS_PUNT_LIST &ClassList; Lista delle classi (out)
  int             *Sub;       Codice sottoclasse (se nel gruppo SelSet è presente 
                              solo una classe simulazione con una sola sottoclasse)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_class_list(C_SELSET SelSet, C_CLS_PUNT_LIST &ClassList, int *Sub)
{
   ads_name   ent;
   long       ndx = 0;
   C_CLS_PUNT *pCodeClass;
   int        PrevExtCls = 0, PrevExtSub = 0, ManySub = FALSE;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   ClassList.remove_all();
   // Ricavo la lista delle classi nel gruppo di selezione
   while (SelSet.entname(ndx++, ent) == GS_GOOD)
   {
      if ((pCodeClass = new C_CLS_PUNT) == NULL)
         { ClassList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      // se non è di GEOsim
      if (pCodeClass->from_ent(ent) == GS_BAD) { delete pCodeClass; continue; }
      // se la classe non esiste ancora in lista
      if (((C_CLASS *) pCodeClass->cls)->is_subclass() == GS_GOOD)
      {  
         if (PrevExtCls == 0 && PrevExtSub == 0) // prima volta
         {
            PrevExtCls = ((C_CLASS *) pCodeClass->cls)->ptr_id()->code;
            PrevExtSub = ((C_CLASS *) pCodeClass->cls)->ptr_id()->sub_code;
         }
         else
            if (PrevExtCls != ((C_CLASS *) pCodeClass->cls)->ptr_id()->code ||
                PrevExtSub != ((C_CLASS *) pCodeClass->cls)->ptr_id()->sub_code)
               ManySub = TRUE; // Selezionate più sottoclassi

         // considero la classe madre
         pCodeClass->cls = GS_CURRENT_WRK_SESSION->find_class(((C_CLASS *) pCodeClass->cls)->ptr_id()->code);
      }
      if (ClassList.search_Cls(pCodeClass->get_class())) { delete pCodeClass; continue; }

      ClassList.add_tail(pCodeClass);
   }

   if (Sub)
      *Sub = (ClassList.get_count() == 1 && PrevExtSub != 0 && !ManySub) ? PrevExtSub : 0;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_ddselect_selset                    <external> */
/*+                                                                       
  Funzione per la selezione di un'entità di GEOsim.
  Parametri:
  ads_name     ss;         Gruppo di selezione (opzionale se = NULL la funzione lo chiede)
  C_LONG_BTREE &KeyList;   Lista codici chiave delle entità (out)
  C_CLASS      **pSelCls;  Puntatore alla classe selezionata

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ddselect_selset(C_SELSET &ss, C_LONG_BTREE &KeyList, C_CLASS **pSelCls)
{
   ads_name _ss;
   ss.get_selection(_ss);
   return gsc_ddselect_selset(_ss, KeyList, pSelCls);
}
int gsc_ddselect_selset(ads_name ss, C_LONG_BTREE &KeyList, C_CLASS **pSelCls)
{
   C_SELSET        SelSet;
   ads_name        ent;
   C_EED           eed;
   long            ndx = 0, Key;
   C_CLS_PUNT_LIST CodeClassList;
   C_ID            *pID;
   int             res, Sub;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (ss)
   {
      SelSet << ss;
      SelSet.ReleaseAllAtDistruction(GS_BAD);
   }
   else
      if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) != RTNORM) return GS_CAN;
   
   // Ricavo la lista delle classi nel gruppo di selezione
   if (gsc_get_class_list(SelSet, CodeClassList, &Sub) == GS_BAD) return GS_BAD;

   if (CodeClassList.get_count() == 1 && Sub > 0)
   {
      *pSelCls = (C_CLASS *) ((C_CLS_PUNT *) CodeClassList.get_head())->get_class();
      *pSelCls = (C_CLASS *) (*pSelCls)->ptr_sub_list()->search_key(Sub);
   }
   else
      if ((res = gsc_ddselect_class(NONE, pSelCls, FALSE, &CodeClassList)) != GS_GOOD)
         return res;

   if (!pSelCls || (*pSelCls)->is_extracted() == GS_BAD) return GS_BAD;

   KeyList.remove_all();
   pID = (*pSelCls)->ptr_id();
   ndx = 0;
   while (SelSet.entname(ndx++, ent) == GS_GOOD)
   {
      eed.load(ent);
      if (eed.cls == pID->code && eed.sub == pID->sub_code)
      {
         if ((*pSelCls)->getKeyValue(ent, &Key) == GS_BAD) continue;
         if (KeyList.add(&Key) == GS_BAD) // Non vengono inseriti elementi doppi
            { KeyList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_select_entity <external> */
/*+                                                                       
  Funzione per la selezione di un'entità di GEOsim.
  Parametri:
  ads_name in;     oggetto grafico (opzionale se = NULL la funzione lo chiede)
  long *gs_id;     codice entità (out)
  int *num_el;     numero aggregazioni (out)
  ads_name out;    oggetto grafico selezionato
  C_CLASS **ppcls; puntatore alla classe di appartenenza

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_select_entity(ads_name in, long *gs_id, int *num_el,
                      ads_name out, C_CLASS **ppcls)
{
   C_STRING          path;
   TCHAR             pos[20], prompt[150], errmsg[30] = GS_EMPTYSTR;   
   ads_hdlg          dcl_id;
   int               i, ret, status, nnn, mark, EntSelection, DclFile;
   ads_name          ent;
   ads_point         point;
   C_EED             ent_EED;
   C_CLASS           *pclasse, *tmpcls;
   C_CLS_PUNT        *punt;
   C_ENT_FAMILY_LIST member_list;
   C_ENT_FAMILY      *member;
   long              key;
   Common_Dcl_DataIns_Struct Common_Dcl_DataIns;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (in != NULL)
   {
      ent_EED.load(in);
      if ((pclasse = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) == NULL ||
           pclasse->is_extracted() == GS_BAD) return GS_BAD;
	   ads_name_set(in, ent);
	}
   else
   { 
      // creo una lista delle classi griglia estratte
      if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(Common_Dcl_DataIns.SelClsList, CAT_GRID) == GS_BAD)
         return GS_BAD;

      // se non ci sono griglie estratte
      if (Common_Dcl_DataIns.SelClsList.get_count() == 0)
         EntSelection = TRUE; // selezione per oggetto grafico
      else
      {
         TCHAR str[128];

         wcscpy(str, gsc_msg(215));     // "Entità"
         acedInitGet(0, gsc_msg(284));  // "Entità Spaziale"
         // "\nSelezione per [Entità/Spaziale] <Entità>: "
         if ((ret = acedGetKword(gsc_msg(285), str)) == RTERROR)
            { GS_ERR_COD=eGSAdsCommandErr; return GS_CAN; }
         if (ret == RTCAN) return GS_CAN;

         EntSelection = (gsc_strcmp(str,gsc_msg(286)) == 0) ? FALSE : TRUE; // "Spaziale"
      }

      if (EntSelection) // selezione per oggetto grafico
      {
         acutPrintf(gsc_msg(210)); // "\nSeleziona entità: "
         do 
         {
            acedInitGet(RSG_NONULL, GS_EMPTYSTR);

            while ((ret=acedEntSel(GS_EMPTYSTR, ent, point)) == RTERROR);
            if (ret==RTREJ) { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }
            if (ret==RTCAN) return GS_CAN;

            if (ent_EED.load(ent) == GS_GOOD)
               if ((pclasse = GS_CURRENT_WRK_SESSION->find_class(ent_EED.cls, ent_EED.sub)) != NULL)
                  if (pclasse->is_extracted() == GS_GOOD) break;

            acutPrintf(gsc_msg(28)); // "\nSelezione entità non valida."
   	   }
         while (1);
      }
      else // selezione spaziale
      {
         acutPrintf(gsc_msg(31)); // "\nSeleziona un punto: "
         acedInitGet(RSG_NONULL, GS_EMPTYSTR);

         do
         {
            if ((ret = acedGetPoint(NULL, GS_EMPTYSTR, point)) == RTERROR)
               { GS_ERR_COD=eGSAdsCommandErr; return GS_BAD; }
            if (ret == RTCAN) return GS_CAN;

            if (SelectGrid(point, gs_id, Common_Dcl_DataIns.SelClsList) == GS_GOOD)
            {
               if (num_el) *num_el = 1;
               if (ppcls)  *ppcls  = SEL_CLS;
               return GS_GOOD;
            }
            acutPrintf(gsc_msg(107)); // "\nPunto non valido."
         }
         while (1);
      }
	}
   if (num_el!=NULL) *num_el = ent_EED.num_el;
   if (out!=NULL) ads_name_set(ent,out);
   if (ppcls!=NULL) *ppcls = pclasse;

   // Se è una entità di tipo SPAGHETTI allora non devo leggere valore chiave
   if (pclasse->get_category() == CAT_SPAGHETTI) return GS_GOOD;

   // RICHIEDE LA LISTA DELLE CLASSI SELEZIONATE NEL PROGETTO
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(Common_Dcl_DataIns.SelClsList) == GS_BAD)
      return GS_BAD;
   punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_head();
   while (punt)
   {
      tmpcls = (C_CLASS *) punt->get_class();
      if (tmpcls->is_member_class(ent_EED.cls) == GS_GOOD)
	   {
         punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_next();
	      continue;
		}
	   Common_Dcl_DataIns.SelClsList.remove(punt);
	   punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_cursor();
   }

   // NON HA GRUPPI O GRUPPI COLLEGATE 
   if (Common_Dcl_DataIns.SelClsList.get_count()==1) 
   {
      if (gs_id)
         if (gsc_getKeyValue(ent, gs_id) == GS_BAD) return GS_BAD;
      return GS_GOOD;
   }

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &DclFile) == RTERROR) return GS_BAD;
    
   while (1)
   {
      ads_new_dialog(_T("Entsel"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
      if (dcl_id == NULL)
      {
         ads_unload_dialog(DclFile);
         GS_ERR_COD=eGSAbortDCL; 
         return GS_BAD; 
      }
  
      // RIEMPIE LA LIST-BOX DELLE CLASSI SELEZIONABILI //
      i = 0;
      ads_start_list(dcl_id, _T("listcls"), LIST_NEW, 0);
      punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_head();
      while (punt)
      {
         gsc_add_list(punt->get_name());
	      if (punt->get_key()==ent_EED.cls) 
	      {
   	      swprintf(pos, 20, _T("%d"), i);
            SEL_CLS = (C_CLASS *) punt->get_class();
         }
	      i++;
         punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_next();
      }
      ads_end_list();
       
      ads_action_tile(dcl_id, _T("listcls"), (CLIENTFUNC) dcl_ent_listcls);
      ads_client_data_tile(dcl_id, _T("listcls"), &(Common_Dcl_DataIns.SelClsList));

      ads_action_tile(dcl_id, _T("accept"),  (CLIENTFUNC) dcl_ent_accept_ok);
      ads_action_tile(dcl_id, _T("cancel"),  (CLIENTFUNC) dcl_ent_quitter);
      ads_action_tile(dcl_id, _T("help"),    (CLIENTFUNC) dcl_ent_help);

      ads_mode_tile(dcl_id, _T("connect"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("listcls"), MODE_SETFOCUS);

      ads_set_tile(dcl_id, _T("listcls"), pos);
      ads_set_tile(dcl_id, _T("error"), errmsg);

      // LANCIA LA DIALOG-BOX
      ads_start_dialog(dcl_id, &status);
      if (SEL_CLS==NULL)    
         { ads_unload_dialog(DclFile); return GS_CAN; }

      // E' richiesta solo la selezione di una classe non di una entita'
      if (out==NULL)
      {
         if (gs_id!=NULL) *gs_id   = 0;
         if (num_el!=NULL) *num_el = 0;
         if (ppcls!=NULL) *ppcls   = SEL_CLS;
         ads_unload_dialog(DclFile);
         
         return GS_GOOD; 
      }

      if (gsc_getKeyValue(ent_EED.ent, &key) == GS_BAD) 
         { ads_unload_dialog(DclFile); return GS_BAD; }

      // Ritorno selezione entita' semplice
      if (SEL_CLS->get_key() == ent_EED.cls) 
      { 
         if (gs_id) *gs_id = key;
         ads_unload_dialog(DclFile);
         return GS_GOOD;
      }

      if (SEL_CLS->fill_group_class(ent_EED.cls, key, member_list) == GS_BAD) 
         { ads_unload_dialog(DclFile); return GS_BAD; }

      if ((member=(C_ENT_FAMILY*)member_list.get_head())!=NULL) break; // C'e' almeno un elemento
      swprintf(errmsg, 30, gsc_msg(251), SEL_CLS->get_name()); // "Nessuna entita' \"%s\"."
      Common_Dcl_DataIns.SelClsList.remove_key(SEL_CLS->get_key());
}
   ads_unload_dialog(DclFile);

   int AllOnVideo;

   i    = 1;
   mark = 0;
   
   // indipendentemente dal numero di gruppi legati all'oggetto chiedo la conferma
   // perchè in questo modo rimangono evidenziati gli oggetti grafici
   {
      while(1)
      {
         if (member->evid(0, &AllOnVideo)==GS_BAD) return GS_BAD; // evidenzio
         if (!AllOnVideo) acutPrintf(gsc_msg(262)); // "\nEntità non completamente a video."
   	   swprintf(prompt, 150, gsc_msg(252), i , member_list.get_count()); // "\nEntità numero %d : Conferma/[1..%d]/Esci/<Successivo>: "
         acedInitGet(0, gsc_msg(253)); // "Conferma Successivo Esci"
   	   if ((ret = ads_getint(prompt, &nnn))==RTERROR) return GS_BAD;
   	   if (ret == RTCAN) 
   	   {
            if (member->evid(-1) == GS_BAD) return GS_BAD;
   	      return GS_CAN;
   		}
   	   else if (ret == RTNORM) 
   	   {
   	      if ((C_ENT_FAMILY *) member_list.getptr_at(nnn))
   		   {
   		      i = nnn;
   		      member = (C_ENT_FAMILY *) member_list.get_cursor();
   		      continue;
   		   }

   		   acutPrintf(gsc_msg(254)); // "\nNumero entità non valido."
   		   continue;
   	   }
   	   else if (ret == RTKWORD)
   	   {
   		   ads_getinput(pos);
   		   if (gsc_strcmp(pos, gsc_msg(255)) == 0)	 // "Esci"
   		   {
               if (member->evid(-1) == GS_BAD) return GS_BAD;
   	         return GS_CAN;
   		   }
   		   else if (gsc_strcmp(pos, gsc_msg(256)) == 0) break;	 // "Conferma"
   	   }
   	   i++;
         if ((member = (C_ENT_FAMILY *) member->get_next()) == NULL)
         { 
            member = (C_ENT_FAMILY *) member_list.get_head();
   	      i = 1;
   		}
   	}
   }

   if (member->evid(-1) == GS_BAD) return GS_BAD;

   if (gs_id!=NULL)  *gs_id=member->get_id();
   if (num_el!=NULL) *num_el=0;
   if (out!=NULL)    ads_name_clear(out);
   if (ppcls!=NULL)  *ppcls=SEL_CLS;

   return GS_GOOD;   
}


/*********************************************************/
/*.doc gsc_ddMultiSelectClass                 <external> */
/*+                                                                       
  Funzione per la selezione multipla di classi via DCL.
  Classe con codice 0 significa "Oggetti Autocad".
  Parametri:
  C_INT_INT_LIST &ClsSubList;    Lista delle classi da scegliere (codice e sottocodice)
  C_INT_INT_LIST &SelClsList;     Lista delle classi scelte

  Restituisce GS_GOOD in caso di successo altrimenti restituisce 
  GS_CAN in caso di annullamento da parte dell'utente o GS_BAD in caso di errore.
-*/  
/*********************************************************/
// ACTION TILE : click su tasto OK //
static void CALLB MultiSelectClassDlg_ok(ads_callback_packet *dcl)
{
   TCHAR      val[TILE_STR_LIMIT];
   C_INT_LIST *pSelectedList = (C_INT_LIST *) dcl->client_data;

   ads_get_tile(dcl->dialog, _T("listcls"), val, TILE_STR_LIMIT);
   pSelectedList->from_str(val, _T(' '));
   ads_done_dialog(dcl->dialog, DLGOK);
}
int gsc_ddMultiSelectClass(C_INT_INT_LIST &ClsSubList, C_INT_INT_LIST &SelClsList)
{
   C_INT_INT          *pClsSub;
   C_INT_INT_STR_LIST DescrClassList;
   C_INT_INT_STR      *pDescrClass;
   C_CLASS            *pCls;
   C_STRING           Descr, Path;
   ads_hdlg           dcl_id;
   int                DclFile, status;
   C_INT_LIST         SelectedList;
   C_INT              *pSelected;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // carico la lista delle descrizioni delle classi e la ordino
   pClsSub = (C_INT_INT *) ClsSubList.get_head();
   while (pClsSub)
   {
      if (pClsSub->get_key() != 0)
      {
         if (!(pCls = GS_CURRENT_WRK_SESSION->find_class(pClsSub->get_key(), 
                                                         pClsSub->get_type())))
            return GS_BAD;
         pCls->get_CompleteName(Descr);
      }
      else
         Descr = gsc_msg(462); // "<Oggetti grafici AutoCAD Map>"

      if ((pDescrClass = new C_INT_INT_STR) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pDescrClass->set_name(Descr.get_name());
      pDescrClass->set_key(pClsSub->get_key());
      pDescrClass->set_type(pClsSub->get_type());
      DescrClassList.add_tail(pDescrClass);

      pClsSub = (C_INT_INT *) ClsSubList.get_next();
   }
   DescrClassList.sort_name();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   Path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   Path = GEOsimAppl::WORKDIR;
   Path += _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(Path, &DclFile) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("MultiSelectClassDlg"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
   if (dcl_id == NULL)
   {
      ads_unload_dialog(DclFile);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD; 
   }

   // RIEMPIE LA LIST-BOX DELLE CLASSI SELEZIONABILI
   ads_start_list(dcl_id, _T("listcls"), LIST_NEW, 0);
   pDescrClass = (C_INT_INT_STR *) DescrClassList.get_head();
   while (pDescrClass)
   {
      gsc_add_list(pDescrClass->get_name());
      pDescrClass = (C_INT_INT_STR *) DescrClassList.get_next();
   }
   ads_end_list();

   ads_client_data_tile(dcl_id, _T("accept"), &SelectedList);
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) MultiSelectClassDlg_ok);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(DclFile);

   if (status == DLGOK)
   {
      pSelected = (C_INT *) SelectedList.get_head();
      while (pSelected)
      {
         if ((pDescrClass = (C_INT_INT_STR *) DescrClassList.getptr_at(pSelected->get_key() + 1)))
         {
            if ((pClsSub = new C_INT_INT) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pClsSub->set_key(pDescrClass->get_key());
            pClsSub->set_type(pDescrClass->get_type());
            SelClsList.add_tail(pClsSub);
         }
         pSelected = (C_INT *) SelectedList.get_next();
      }
      return GS_GOOD;
   }
   else
      return GS_CAN;
}


/*********************************************************/
/*.doc SelectGrid <external> */
/*+                                                                       
  Funzione per la selezione di una entità griglia di GEOsim.
  La lista delle classi griglie estratte è gia stata inizializzata
  in SelClsList mentre la classe selezionata è in SEL_CLS.
  Parametri:
  ads_point pt;                  punto selezionato
  long *gs_id;                   codice entità (out)
  C_CLS_PUNT_LIST &SelClsList;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int SelectGrid(ads_point pt, long *gs_id, C_CLS_PUNT_LIST &SelClsList)
{
   C_STRING   path;
   TCHAR      pos[20], errmsg[30] = GS_EMPTYSTR;   
   ads_hdlg   dcl_id;
   int        DclFile, status;
   C_CLS_PUNT *punt;
   long       key;

   if (SelClsList.get_count() == 0) return GS_BAD;

   if (SelClsList.get_count() == 1)
   {
      SEL_CLS = (C_CLASS *) (((C_CLS_PUNT *) (SelClsList.get_head()))->get_class());
      if (SEL_CLS->ptr_grid() && SEL_CLS->ptr_grid()->pt2key(pt, &key) == GS_GOOD)
      {
         if (gs_id) *gs_id = key;
         return GS_GOOD;
      }
      else return GS_BAD;
   }

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &DclFile) == RTERROR) return GS_BAD;
    
   while (1)
   {
      ads_new_dialog(_T("Entsel"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
      if (dcl_id == NULL)
      {
         ads_unload_dialog(DclFile);
         GS_ERR_COD=eGSAbortDCL; 
         return GS_BAD; 
      }
  
      // RIEMPIE LA LIST-BOX DELLE CLASSI SELEZIONABILI //
      ads_start_list(dcl_id, _T("listcls"), LIST_NEW, 0);
      punt = (C_CLS_PUNT *) SelClsList.get_head();
      while (punt)
      {
         gsc_add_list(punt->get_name());
         punt = (C_CLS_PUNT *) SelClsList.get_next();
      }
      ads_end_list();
       
      ads_action_tile(dcl_id, _T("listcls"), (CLIENTFUNC) dcl_ent_listcls);
      ads_client_data_tile(dcl_id, _T("listcls"), &SelClsList);

      ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_ent_accept_ok);
      ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC) dcl_ent_quitter);
      ads_action_tile(dcl_id, _T("help"),   (CLIENTFUNC) dcl_ent_help);

      ads_mode_tile(dcl_id, _T("connect"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("listcls"), MODE_SETFOCUS);

      ads_set_tile(dcl_id, _T("listcls"), pos);
      ads_set_tile(dcl_id, _T("error"), errmsg);

      // LANCIA LA DIALOG-BOX
      ads_start_dialog(dcl_id, &status);
      if (SEL_CLS==NULL) { ads_unload_dialog(DclFile); return GS_CAN;	}

      if (SEL_CLS->ptr_grid() && SEL_CLS->ptr_grid()->pt2key(pt, &key) == GS_GOOD)
      {
         if (gs_id) *gs_id = key;
         ads_unload_dialog(DclFile); 
         return GS_GOOD;
      }

      swprintf(errmsg, 30, gsc_msg(251), SEL_CLS->get_name()); // "Nessuna entita' \"%s\"."
      SelClsList.remove_key(SEL_CLS->get_key());
   }
   ads_unload_dialog(DclFile);

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_ENT_FAMILY::evid <internal> */
/*+                                                                       
  Evidenzio o elimino evidenziazione di oggetti.
  Parametri:
  int mode;        se = -1 cancella eventuali evidenziazioni
                   se = 0 evidenzia con grip
  int *AllOnVideo; se tutte le entità erano a video = TRUE
                   altrimenti FALSE

-*/  
/*********************************************************/
int C_ENT_FAMILY::evid(int mode, int *AllOnVideo)
{
   C_CLS_PUNT *punt;
   int        ret;
   ads_name   grip_sel;

   if (acedSSAdd(NULL, NULL, grip_sel) != RTNORM)
      { GS_ERR_COD=eGSOutOfMem; return GS_BAD; }

	if (mode == -1) // cancello le evidenziazioni
   {
      if (acedSSSetFirst(grip_sel, grip_sel) != RTNORM)
         { ads_ssfree(grip_sel); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      ads_ssfree(grip_sel);
      return GS_GOOD;
   }

   if (AllOnVideo) *AllOnVideo = TRUE;

   punt = (C_CLS_PUNT *) family.get_head();

   while (punt)
   {
      if (ads_name_nil(punt->ent))
      {
         if (AllOnVideo) *AllOnVideo = FALSE;
      }
      else ret = acedSSAdd(punt->ent, grip_sel, grip_sel);

      punt=(C_CLS_PUNT*)punt->get_next();
   }

   if (acedSSSetFirst(grip_sel, grip_sel) != RTNORM)
      { ads_ssfree(grip_sel); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ads_ssfree(grip_sel) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_ENT_FAMILY::to_rb            <internal> */
/*+                                                                       
  Ritorna una lista di resbuf così composta:
  (<id group>((<cls1><id1>)(<cls2><id2>) ...))
-*/  
/*********************************************************/
presbuf C_ENT_FAMILY::to_rb(void)
{
   C_CLS_PUNT *punt;
   C_RB_LIST  out;

   if ((out << acutBuildList(RTLB, RTSHORT, get_id(), 0)) == NULL) return NULL;

   punt = (C_CLS_PUNT *) family.get_head();
   if (punt)
   {
      if ((out += acutBuildList(RTLB, 0)) == NULL) return NULL;

      while (punt)
      {
         if ((out += acutBuildList(RTLB,
                                   RTSHORT, punt->get_key(),
                                   RTREAL,  (double) punt->get_gs_id(),
                                   RTLE, 0)) == NULL)
            return NULL;

	      punt = (C_CLS_PUNT *) punt->get_next();
      }
      
      if ((out += acutBuildList(RTLE, 0)) == NULL) return NULL;
   }
   if ((out += acutBuildList(RTLE, 0)) == NULL) return NULL;

   out.ReleaseAllAtDistruction(GS_BAD);

   return out.get_head();
}


/*********************************************************/
/*.doc (new 2) C_ENT_FAMILY::to_rb            <internal> */
/*+                                                                       
  Ritorna una lista di resbuf così composta:
  ((<id group>((<cls1><id1>)(<cls2><id2>) ...))
   (<id group>((<cls1><id1>)(<cls2><id2>) ...)) ...)
-*/  
/*********************************************************/
presbuf C_ENT_FAMILY_LIST::to_rb(void)
{
   C_ENT_FAMILY *punt;
   C_RB_LIST    out;

   punt = (C_ENT_FAMILY *) get_head();
   while (punt)
   {
      if ((out += punt->to_rb()) == NULL) return NULL;
	   punt = (C_ENT_FAMILY *) punt->get_next();
   }

   out.ReleaseAllAtDistruction(GS_BAD);

   return out.get_head();
}

///////////////////////////////////////////////////////////////////////////
// INIZIO      INTERROGAZIONE DINAMICA 
///////////////////////////////////////////////////////////////////////////

C_DYNAMIC_QRY_ATTR::C_DYNAMIC_QRY_ATTR(AcApDocument* pDoc, C_CLS_PUNT_LIST *pClsList)
{
   m_pDoc = pDoc;
	Acad::ErrorStatus es = m_pDoc->inputPointManager()->addPointMonitor(this);

   pClsList->copy(GridClassList);
}

C_DYNAMIC_QRY_ATTR::~C_DYNAMIC_QRY_ATTR()
{
	if (m_pDoc)
   {
      m_pDoc->inputPointManager()->removePointMonitor(this);
      m_pDoc = NULL;
   }
}

bool C_DYNAMIC_QRY_ATTR::excludeFromOsnapCalculation(const AcArray<AcDbObjectId>& nestedEntity,
	                                                  int gsSelectionMark)
{
   return false;
}

Acad::ErrorStatus C_DYNAMIC_QRY_ATTR::monitorInputPoint(const AcEdInputPoint& input, 
                                                        AcEdInputPointMonitorResult& output)
{
	Acad::ErrorStatus es;
	AcDbEntity*       pEnt;
	AcDbObjectId      highlightId = AcDbObjectId::kNull;

   if (!GS_CURRENT_WRK_SESSION) return Acad::eNotApplicable;
   m_TooltipString.clear();

	if (input.pointComputed())
	{
      C_CLASS   *pCls;
      C_RB_LIST ColValues;
      C_STRING  ClassName, Buff;

		// Analyze entities.
		if (input.pickedEntities().length() > 0)
		{
         ads_name ent;

         if ((es = acdbGetAdsName(ent, input.pickedEntities()[0])) != Acad::eOk) return es;

         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) != NULL)
         {
            // entità di GEOsim
            if (pCls->query_data(ent, ColValues) == GS_BAD) ColValues.remove_all();

            highlightId = input.pickedEntities()[0];
         }
		}
      else
      {
         C_CLS_PUNT *pGridCls;
         ads_point Pt;

         Pt[X] = input.rawPoint().x;
         Pt[Y] = input.rawPoint().y;
         Pt[Z] = input.rawPoint().z;

         // se esistono griglie
         if ((pGridCls = (C_CLS_PUNT *) GridClassList.get_head()))
            while (pGridCls)
            {
               pCls = (C_CLASS *) pGridCls->get_class();
               // entità di GEOsim
               if (pCls->query_data(Pt, ColValues) == GS_GOOD) break;

               pGridCls = (C_CLS_PUNT *) pGridCls->get_next();
            }

            if (!pGridCls) ColValues.remove_all();
      }

      if (ColValues.GetCount() > 0)
      {
         pCls->get_CompleteName(ClassName);

         if (input.history() & Acad::eNotDigitizer) // fatto un click
         {
            // Formatto i valori in una stringa per out su video e in clipboard
            if (pCls->ptr_attrib_list()->ParseToString(ColValues, Buff, FALSE,
                                                       pCls->ptr_id()->code,
                                                       pCls->ptr_id()->sub_code) == GS_GOOD)
            {
               m_TooltipString = ClassName;
               m_TooltipString += _T("\n"); // (char) 13; m_TooltipString += (char) 10;
               m_TooltipString += Buff;
            }

            // Stampo a video il testo
            acutPrintf(_T("\n\n\n"));
            acutPrintf(m_TooltipString.get_name());

            // Inserisco il testo nella clipboard
            if (OpenClipboard(NULL) != 0) // Apertura OK
            {
            	HANDLE m_hMemory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, 
                                              (m_TooltipString.len() + 1) * sizeof(TCHAR));

               if (EmptyClipboard())
               {
				      TCHAR *pstr = (TCHAR *) GlobalLock(m_hMemory);

					   lstrcpy(pstr, m_TooltipString.get_name());
   					GlobalUnlock(m_hMemory);

                  SetClipboardData(CF_TEXT, m_hMemory);
               }
               CloseClipboard();
            }              
         }

         m_TooltipString.clear();
         resbuf rb;
         if (acedGetVar(_T("ROLLOVERTIPS"), &rb) == RTNORM && rb.resval.rint == 1 &&
             input.pickedEntities().length() > 0)
         {
            // Formatto i valori in una stringa per ToolTip
            if (pCls->ptr_attrib_list()->ParseToString(ColValues, Buff, TRUE,
                                                         pCls->ptr_id()->code,
                                                         pCls->ptr_id()->sub_code) == GS_GOOD)
            {
               // vedi http://adndevblog.typepad.com/autocad/2013/03/page/3/
               m_TooltipString = _T("]]>");
               m_TooltipString += _T("</TextBlock>");
               // inizio tooltip
               m_TooltipString += _T("<PropertyToolTip Title=\"");
               ClassName.toAXML();
               m_TooltipString += ClassName;
               m_TooltipString += _T("\" ");
               // inizio proprietà
               m_TooltipString += _T("xmlns=\"clr-namespace:Autodesk.AutoCAD.Internal.Windows;assembly=AcWindows\">");
               m_TooltipString += _T("<PropertyToolTip.Properties>");
               // fine proprietà
               m_TooltipString += Buff;
               m_TooltipString += _T("</PropertyToolTip.Properties>");
               // fine tooltip
               m_TooltipString += _T("</PropertyToolTip>");
               m_TooltipString += _T("<TextBlock><![CDATA[");
            }
         }
         else
            // Formatto i valori in una stringa
            if (pCls->ptr_attrib_list()->ParseToString(ColValues, Buff, FALSE,
                                                       pCls->ptr_id()->code,
                                                       pCls->ptr_id()->sub_code) == GS_GOOD)
            {
               m_TooltipString = ClassName;
               m_TooltipString += _T("\n");
               m_TooltipString += Buff;
            }
      }
	}

	// Do highlighting, only the top level entity is highlighted.
	static AcDbObjectId oldHighlightId = AcDbObjectId::kNull;
	if (highlightId != oldHighlightId)
	{
		if (AcDbObjectId::kNull != oldHighlightId)
		{
			es = acdbOpenAcDbEntity(pEnt, oldHighlightId, AcDb::kForRead);
			if (es == Acad::eOk)
			{
				es = pEnt->unhighlight();
				pEnt->close();
				oldHighlightId = AcDbObjectId::kNull;
			}
		}
		es = acdbOpenAcDbEntity(pEnt, highlightId, AcDb::kForRead);
		if (es == Acad::eOk)
		{
			es = pEnt->highlight();
			pEnt->close();
			oldHighlightId = highlightId;
		}
	}

   if (m_TooltipString.len() > 0)
      output.setAdditionalTooltipString(m_TooltipString.get_name());

	return Acad::eOk;
}

// non va a 64 bit
//Acad::ErrorStatus C_DYNAMIC_QRY_ATTR::monitorInputPoint(bool& bAppendToTooltipStr,
//                                                        TCHAR*& pAdditionalTooltipString,
//                                                        AcGiViewportDraw* pDrawContext,
//                                                        AcApDocument* pDocument,
//                                                        bool pointComputed,
//                                                        int history,
//                                                        const AcGePoint3d& lastPoint,
//                                                        const AcGePoint3d& rawPoint,
//                                                        const AcGePoint3d& grippedPoint,
//                                                        const AcGePoint3d& cartesianSnappedPoint,
//                                                        const AcGePoint3d& osnappedPoint,
//                                                        AcDb::OsnapMask osnapMask,
//                                                        const AcArray<AcDbCustomOsnapMode*>& customOsnapModes,
//                                                        AcDb::OsnapMask osnapOverrides,
//                                                        const AcArray<AcDbCustomOsnapMode*>& customOverrides,
//                                                        const AcArray<AcDbObjectId>& apertureEntities,
//                                                        const AcArray< AcDbObjectIdArray,
//                                                        AcArrayObjectCopyReallocator< AcDbObjectIdArray > >& nestedApertureEntities,
//                                                        const AcArray<int>& gsSelectionMark,
//                                                        const AcArray<AcDbObjectId>& keyPointEntities,
//                                                        const AcArray< AcDbObjectIdArray,
//                                                        AcArrayObjectCopyReallocator< AcDbObjectIdArray > >& nestedKeyPointEntities,
//                                                        const AcArray<int>& keyPointGsSelectionMark,
//                                                        const AcArray<AcGeCurve3d*>& alignmentPaths,
//                                                        const AcGePoint3d& computedPoint,
//                                                        const TCHAR* pTooltipString)
//{
//	return Acad::eOk;
//}


/*********************************************************/
/*.doc gsDynamicQueryAttr                 <external> */
/*+                           
   Comando per abilitare (toggle mode) l'interrogazione delle entità GEOsim
   in modo dinamico.
-*/  
/*********************************************************/
void gsDynamicQueryAttr(void)
{
   C_CLS_PUNT_LIST ExtrGridClsList;

   GEOsimAppl::CMDLIST.StartCmd();
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   if (GEOsimAppl::DYNAMIC_QRY_ATTR) 
   {
      delete GEOsimAppl::DYNAMIC_QRY_ATTR;
      GEOsimAppl::DYNAMIC_QRY_ATTR = NULL;
      acutPrintf(gsc_msg(433)); // "\nInterrogazione dinamica disabilitata."
   }
   else
   {
      // verifico se tra le classi estratte vi sono delle classi tipo griglia
      if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(ExtrGridClsList, CAT_GRID) == GS_BAD)
         return GEOsimAppl::CMDLIST.ErrorCmd();

      if ((GEOsimAppl::DYNAMIC_QRY_ATTR = new C_DYNAMIC_QRY_ATTR(curDoc(),
                                                                 &ExtrGridClsList)) != NULL)
         acutPrintf(gsc_msg(435)); // "\nInterrogazione dinamica abilitata."
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*****************************************************************************/
/*.doc gsc_getSplitMode                                                      */
/*+
  Questa funzione ha lo scopo di controllare se un punto dato può essere
  usato come punto per spezzare una polilinea con la modalità espressa
  dalla regola di connessione.
  Parametri:
  ads_point  pt;         Punto di divisione
  ads_name   ent;        Entità da spezzare
  C_INT_INT *punt;       puntatore alla regola di connessione

  Ritorna SOFT_SPLIT o HARD_SPLIT in caso di necessità di dividere l'entità
  altrimenti ritorna NO_CONCT_CONTROL.
-*/  
/*****************************************************************************/
int gsc_getSplitMode(ads_point pt, ads_name ent, C_INT_INT *punt)
{
   C_POINT_LIST point_list;
   C_POINT      *pPoint;
   int          SplitMode = NO_CONCT_CONTROL;

   if (punt->get_type() & SOFT_SPLIT) SplitMode = SOFT_SPLIT;
   else if (punt->get_type() & HARD_SPLIT) SplitMode = HARD_SPLIT;
   else  return NO_CONCT_CONTROL;

   // Leggo punto iniziale e finale
   if (!gsc_isLinearEntity(ent)) return NO_CONCT_CONTROL;
   if (point_list.add_sten_point(ent) != GS_GOOD) return NO_CONCT_CONTROL;
   // il punto di split non deve coincidere con punto iniziale
   pPoint = (C_POINT *) point_list.get_head();
   if (ads_point_equal(pt, pPoint->point)) return NO_CONCT_CONTROL;
   // il punto di split non deve coincidere con punto finale
   pPoint = (C_POINT *) pPoint->get_next();
   if (ads_point_equal(pt, pPoint->point)) return NO_CONCT_CONTROL;
   
   return SplitMode;
}


/*****************************************************************************/
/*.doc gsc_get_connect_point                                                 */
/*+
  Questa funzione ha lo scopo di restituire un punto di connessione rispettando
  le regole impostate nella classe GEOsim (p_class). Il punto ricavato sarà 
  restituito attraverso un putatore (ins) ad un ads_point (allocato in questa 
  funzione). Se <ent> = nil (0,0) la funzione richiede la selezione di una 
  entita' a cui connettersi ed evetualmete la scelta di uno tra i punti di 
  connessione, altrimenti (ent!=nil) richiede solo la scelta tra i punti 
  di connessione.     
  Se start!=NULL e' usato come punto di riferimento per il "draggen" 
  nella scelta del punto di connessione.
  Parametri:
  C_CLASS    *p_class;   Puntatore alla classe GEOsim
  ads_point  start;      Punto già selezionato
  ads_name   con_ent;    Entità già selezionata
  ads_pointp *ins;       Punto di connessione (output) 
  int        *SplitMode; Opzionale, Modalità di split (hard o soft) se ci 
                         si collega ad una entità di tipo polilinea (output)
                         (default = NULL);

  Ritorna GS_GOOD in caso di selezione corretta, GS_BAD in caso di errore 
  oppure GS_CAN nel caso la funzione sia cancellata dall'utente in fase di input.
  Warning: Alloca memoria !!!
-*/  
/*****************************************************************************/
int gsc_get_connect_point(C_CLASS *p_class, ads_point start, ads_name con_ent,
                          ads_pointp *ins, int *SplitMode)
{
   C_EED          ent_EED;
   C_POINT_LIST   point_list;
   C_POINT        *ppp;
   ads_point      point;
   ads_name       ent;
   C_INT_INT      *punt;
   C_CONNECT_LIST *con_list = p_class->ptr_connect_list();
   C_ID           *p_id = p_class->ptr_id();
   int            ret, cont, flag = 1;
   resbuf         NewOsMode, OldOsMode;

   // devono esserci regole di connessione
   if (!con_list || con_list->is_to_be_connected() == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // devo selezionare entità a cui connettersi
   if (con_ent == NULL || ads_name_nil(con_ent))
   {
      do
      {
         do
         {
            if (flag == 0) acutPrintf(gsc_msg(28)); // "\nSelezione entita' non valida."
            flag = 0;
            acutPrintf(gsc_msg(29)); // "\nSeleziona entita' a cui connettersi :"
            acedInitGet(RSG_NONULL, GS_EMPTYSTR);
            while ((ret = acedEntSel(GS_EMPTYSTR, ent, point))==RTERROR);
            if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
            if (ret == RTCAN) return GS_CAN;
   
            if (gsc_is_DABlock(ent) == GS_BAD) // Non devo selezionare la scheda degli attributi
   		   {
               ent_EED.load(ent);
               if (p_class->is_subclass() == GS_BAD)
               {
                  // deve avere qualche controllo di connessione (non solo sovrapposizione)
                  if ((punt = (C_INT_INT *) con_list->search_key(ent_EED.cls)) != NULL &&
                      punt->get_type() != NO_OVERLAP)
                     break;
               }
               else
                  if (ent_EED.cls == p_class->ptr_id()->code)
                     // deve avere qualche controllo di connessione (non solo sovrapposizione)
                     if ((punt = (C_INT_INT *) con_list->search_key(ent_EED.sub)) != NULL &&
                         punt->get_type() != NO_OVERLAP)
                        break;
   		   }
         }
         while (1);

         if (punt->get_type() & CONCT_ANY_POINT)
         {
            ads_point dummy;
            struct resbuf OldApertureValue, PickboxValue;

            // setto la dimesione della finestra di osnap come quella di pickbox
            if (acedGetVar(_T("PICKBOX"), &PickboxValue) != RTNORM ||
                acedGetVar(_T("APERTURE"), &OldApertureValue) != RTNORM)
               { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }

            acedSetVar(_T("APERTURE"), &PickboxValue);
            // prendo l'entità più vicina
            if (ads_osnap(point, _T("_NEAR"), point) != RTNORM)
            {
               acedSetVar(_T("APERTURE"), &OldApertureValue); 
               GS_ERR_COD = eGSAdsCommandErr;
               return GS_BAD;
            }

            acedSetVar(_T("APERTURE"), &OldApertureValue);

            acutPrintf(gsc_msg(30)); // "\naltro punto/<Conferma punto di connessione>: "
            acedInitGet(0,gsc_msg(256)); // "Conferma"

            if ((ret = acedGetPoint(point, GS_EMPTYSTR, dummy)) == RTERROR)
               { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

            if (ret == RTCAN) return GS_CAN;
            else
               if (ret == RTNONE || ret == RTKWORD)
               {
                  // controllo che in quel punto non ci sia un oggetto GEOsim non sovrapponibile
                  if (gsc_OverlapValidation(point, p_class) == GS_BAD)
                  {
                     acutPrintf(gsc_msg(598)); // "\nTrovata entità GEOsim non sovrapponibile."
                     flag = 1;
                     GS_ERR_COD = eGSOverlapValidation;
                     continue;
                  }

                  if ((*ins = (ads_pointp) malloc(sizeof(ads_point))) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  ads_point_set(point, *ins);
                  if (con_ent) ads_name_set(ent, con_ent);

                  // Se devo verificare la modalità di split
                  if (SplitMode)
                     *SplitMode = gsc_getSplitMode(point, ent, punt);

                  return GS_GOOD;
               }
            flag = 1;
         }
         else
         {
            // CREAZIONE LISTA PUNTI NOTEVOLI PER POSSIBILE CONNESSIONE
            if (point_list.add_connect_point_list(ent, punt->get_type(),
                                                  p_id->code, p_id->sub_code) == GS_BAD)
               return GS_BAD;

            if ((cont = point_list.get_count()) == 0)
            {
               acutPrintf(gsc_msg(600)); // "\nEntità priva di punti di connessione validi."
               flag = 1;
               GS_ERR_COD = eGSOverlapValidation;
               continue;
            }
            
            if (con_ent) ads_name_set(ent, con_ent);

            break;
         }
      }
      while (1);
   }
   else
   {  
      ads_name_set(con_ent, ent);
      ent_EED.load(ent);
      if (p_class->is_subclass() == GS_BAD)
      {
         if ((punt = (C_INT_INT*) con_list->search_key(ent_EED.cls)) == NULL)
            { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
      }
      else 
         if (ent_EED.cls != p_class->ptr_id()->code ||
             (punt = (C_INT_INT *) con_list->search_key(ent_EED.sub)) == NULL)
            { GS_ERR_COD = eGSInvalidEED; return GS_BAD;  }

      ads_name_set(con_ent,ent);

      // si presume che sia un'entità nodale ?
      if (gsc_get_firstPoint(ent, point) == GS_BAD) return GS_BAD;

      // CREAZIONE LISTA PUNTI NOTEVOLI PER POSSIBILE CONNESSIONE
      if (point_list.add_connect_point_list(ent, punt->get_type(),
                                            p_id->code, p_id->sub_code) == GS_BAD)
         return GS_BAD;

      *ins = NULL;
      if ((cont = point_list.get_count()) == 0)
      {
         acutPrintf(gsc_msg(600)); // "\nEntità priva di punti di connessione validi."
         return GS_GOOD;
      }
   }

   // leggo valore variabile OSMODE
   if (acedGetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   // Setto il nuovo valore di OSMODE (disabilitato)
   NewOsMode.restype = RTSHORT;
   NewOsMode.resval.rint = 0;
   if (acedSetVar(_T("OSMODE"), &NewOsMode) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }

   // ritorna il punto più vicino
   do 
   {
      if ((ppp = point_list.get_nearest(point))==NULL) return GS_BAD;
      ads_point_set(ppp->point, point);
      if (cont == 1) break;

      if (flag == 0) acutPrintf(gsc_msg(30)); // "\naltro punto/<Conferma punto di connessione>: "
      flag = 1;
      acedInitGet(0, gsc_msg(256)); // "Conferma"

      if (start == NULL)
      {
         if ((ret = acedGetPoint(point, GS_EMPTYSTR, point)) == RTERROR)
            { acedSetVar(_T("OSMODE"), &OldOsMode); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      }
      else   
         if ((ret = acedGetPoint(start, GS_EMPTYSTR, point)) == RTERROR)
            { acedSetVar(_T("OSMODE"), &OldOsMode); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

      if (ret == RTCAN) return GS_CAN;
      else 
      if (ret == RTNONE || ret == RTKWORD) break;
   }
   while (1);
   
   // ripristino valore variabile OSMODE
   if (acedSetVar(_T("OSMODE"), &OldOsMode) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }

   if ((*ins = (ads_pointp) malloc(sizeof(ads_point))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   ads_point_set(point, *ins);

   // Se devo verificare la modalità di split
   if (SplitMode)
      *SplitMode = gsc_getSplitMode(point, ent, punt);

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER SELEZIONE CLASSE VIA DCL  -  INIZIO
///////////////////////////////////////////////////////////////////////////////


/*+
-------------------------------------------------------------
int gsc_select_class(void) -> Lancia DCL.

  Funzioni per la gestione della DCL per la selezione delle
  classi da inserire.
  Comunicano tra loro e con le funzioni esterne attraverso
  le variabili globali :

       SEL_EXT puntatore alla classe simulazione selezionata,
               (se !=NULL attiva menu selezione sottoclassi).
       SEL_CLS puntatore alla classe selezionata per l'inserimento.
   GEOsimAppl::LAST_CLS codice ultima classe inserita 
               (se 0 nessuna inserita prima).
   GEOsimAppl::LAST_SUB sottocodice ultima sottoclasse inserita.
-------------------------------------------------------------
-*/
// ACTION TILE : click su lista sottoclassi //
static void CALLB dcl_sel_listsub(ads_callback_packet *dcl)
{
   int             pos;
   C_CLS_PUNT      *punt;
   ads_hdlg        hdlg = dcl->dialog;
   C_CLS_PUNT_LIST *pSelSubList = (C_CLS_PUNT_LIST *) dcl->client_data; // LISTA PUNT. SOTTOCLASSI SELEZ.

   pos = _wtoi(dcl->value);

   if (!(punt = (C_CLS_PUNT *) pSelSubList->getptr_at(pos + 1))) return;
   SEL_CLS = (C_CLASS *) punt->get_class();

   if (dcl->reason == CBR_DOUBLE_CLICK)
      ads_done_dialog(hdlg, DLGOK);
}
// ACTION TILE : click su lista classi
static void CALLB dcl_sel_listcls(ads_callback_packet *dcl)
{
   int            pos;
   ads_hdlg       hdlg;
   C_CLASS        *cls;
   C_CLS_PUNT     *punt;
   C_CONNECT_LIST *con;
   Common_Dcl_DataIns_Struct *pCommon_Dcl_DataIns;

   pCommon_Dcl_DataIns = (Common_Dcl_DataIns_Struct *) dcl->client_data;

   hdlg = dcl->dialog;
   pos  = _wtoi(dcl->value);
   punt = (C_CLS_PUNT *) pCommon_Dcl_DataIns->SelClsList.getptr_at(pos + 1);
   cls  = (C_CLASS *) punt->get_class();

   // controlli diversi a seconda di "mode"
   switch (pCommon_Dcl_DataIns->mode)
   {
      case INSERT: // selezione di una classe per inserimento oggetti
         if (cls->get_category() == CAT_EXTERN)
         {
            if (((C_EXTERN *) cls)->get_DirectInsertableSubs(pCommon_Dcl_DataIns->SelSubList) == GS_GOOD)
            {
               ads_mode_tile(hdlg, _T("connect"), MODE_DISABLE);

               ads_start_list(hdlg, _T("listsub"), LIST_NEW, 0);
               // riempie la lista delle sottoclassi inseribili direttamente
               punt = (C_CLS_PUNT *) pCommon_Dcl_DataIns->SelSubList.get_head();
               while (punt)
               {
                  gsc_add_list(punt->get_name());
                  punt = (C_CLS_PUNT *) punt->get_next();
               }
               ads_end_list();

               SEL_EXT = cls;
               SEL_CLS = NULL;
            }
         }
         else
         {
            ads_start_list(hdlg, _T("listsub"), LIST_NEW, 0);
            ads_end_list();
            pCommon_Dcl_DataIns->SelSubList.remove_all();
            if ((con=cls->ptr_connect_list())==NULL || con->get_count()==0)
               ads_mode_tile(hdlg, _T("connect"), MODE_DISABLE);
            else
            {   
               ads_mode_tile(hdlg, _T("connect"), MODE_ENABLE);
               ads_set_tile(hdlg, _T("connect"), _T("abilit"));
            }

            SEL_EXT = NULL;
            SEL_CLS = cls;

            if (dcl->reason == CBR_DOUBLE_CLICK) ads_done_dialog(hdlg, DLGOK);
         }
         break;
      case NONE:   // selezione di una classe tra quelle estratte
      {
         C_SUB      *pSub;
         C_CLS_PUNT *new_nod;

         if (cls->get_category() == CAT_EXTERN)
         {
            pCommon_Dcl_DataIns->SelSubList.remove_all();

            ads_start_list(hdlg, _T("listsub"), LIST_NEW, 0);
            // ciclo per ogni sottoclasse
            pSub = (C_SUB *) cls->ptr_sub_list()->get_head();
            while (pSub)
            {
               if ((new_nod = new C_CLS_PUNT(pSub)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; pCommon_Dcl_DataIns->SelSubList.remove_all(); break; }
               pCommon_Dcl_DataIns->SelSubList.add_tail(new_nod);
               gsc_add_list(pSub->get_name());
               pSub = (C_SUB *) cls->ptr_sub_list()->get_next();
            }
            ads_end_list();

            SEL_EXT = cls;
            SEL_CLS = NULL;
         }
         else
         {
            SEL_EXT = NULL;
            SEL_CLS = cls;

            if (dcl->reason == CBR_DOUBLE_CLICK) ads_done_dialog(hdlg, DLGOK);
            else
            {
               ads_start_list(hdlg, _T("listsub"), LIST_NEW, 0);
               ads_end_list();
            }
         }
         break;
      }
      default:
         GS_ERR_COD = eGSInvalidArg; 
   }
}
// ACTION TILE : click su controllo connessioni //
static void CALLB dcl_sel_connect(ads_callback_packet *dcl)
{
   TCHAR    val[20] = GS_EMPTYSTR;
   ads_hdlg hdlg = dcl->dialog;
   int      *pConnectCheck = (int *) (dcl->client_data);

   if (SEL_CLS==NULL || !pConnectCheck) return;
   
   ads_get_tile(hdlg, _T("connect"), val, 20);
   if (gsc_strcmp(val, _T("abilit")) == 0) *pConnectCheck = TRUE;
   else *pConnectCheck = FALSE;
}
// ACTION TILE : click su selezione classe da grafica
static void CALLB dcl_sel_graph(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGSTATUS);
}
// ACTION TILE : click su tasto OK //
static void CALLB dcl_sel_accept_ok(ads_callback_packet *dcl)
{
   if (SEL_CLS!=NULL)
   {
      ads_hdlg hdlg=dcl->dialog;
      ads_done_dialog(hdlg, DLGOK);
   }
}
// ACTION TILE : click su tasto CANCEL //
static void CALLB dcl_sel_quitter(ads_callback_packet *dcl)
{
   ads_hdlg hdlg=dcl->dialog;

   SEL_CLS = NULL;
   ads_done_dialog(hdlg, DLGCANCEL);         
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_InsertClass_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Inserimentoentit); } 
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_SelectClass_help(ads_callback_packet *dcl)
{
   //gsc_help();
} 


/*********************************************************/
/*.doc gsc_ddselect_class                       <external> */
/*+                                                                       
  La funzione propone una selezione di una classe tramite diverse modalità. 
  E' comunque sempre previsto l'uso delle finestre via DCL.
  Parametri:
  int mode;          Se = NONE selezione di una classe tra quelle estratte
                     se = INSERT selezione di una classe per inserimento oggetti
                     default = NONE.
  C_CLASS **pSelCls; Puntatore alla classe selezionata (default = NULL)
  int *ConnectCheck; Check di controllo connessione (usato se mode = INSERT)
                     Se l'utente vuole inserire con controllo di connessione = TRUE
                     altrimenti FALSE (default = FALSE)
  C_CLS_PUNT_LIST  *pClsListToSel; Se <> NULL indica una lista di classi da cui
                                   scegliere, viene quindi disabilitata la
                                   selezione di una classe via mouse-pick (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD o GS_CAN.
  N.B. La funzione necessita delle seguenti variabili globali:
       GEOsimAppl::LAST_CLS (solo per inserimento), SEL_CLS che rappresenta la classe scelta.
-*/  
/*********************************************************/
int gs_ddselect_class(void)
{
   int       mode, ConnectCheck;
   presbuf   arg = acedGetArgs();
   C_CLASS   *pSelCls;
   C_RB_LIST ret;

   acedRetNil(); 

   if (!arg || arg->restype != RTSHORT) // modo
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   mode = arg->resval.rint;

   if (!(arg = arg->rbnext) || arg->restype != RTSHORT) // ConnectCheck
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ConnectCheck = arg->resval.rint;

   if (gsc_ddselect_class(mode, &pSelCls, &ConnectCheck) == GS_BAD) return RTERROR;
   if ((ret << acutBuildList(RTSHORT, pSelCls->ptr_id()->code,
                             RTSHORT, pSelCls->ptr_id()->sub_code, 0)) == NULL)
      return RTERROR;

   ret.LspRetList();

   return RTNORM;

}
int gsc_ddselect_class(int mode, C_CLASS **pSelCls, int *ConnectCheck, 
                       C_CLS_PUNT_LIST *pClsListToSel)
{
   C_STRING            path;
   TCHAR               val[30];   
   ads_hdlg            dcl_id;
   ads_callback_packet packet;
   int                 status, pos, DclFile;
   C_CLS_PUNT          *punt;
   Common_Dcl_DataIns_Struct Common_Dcl_DataIns;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (pClsListToSel && pClsListToSel->get_count() > 0) // Si utilizza una lista già preparata
   {
      if (pClsListToSel->copy(Common_Dcl_DataIns.SelClsList) == GS_BAD) return GS_BAD;
   }
   else // Ottiene la lista delle classi estratte nella sessione corrente
      if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(Common_Dcl_DataIns.SelClsList) == GS_BAD)
         return GS_BAD;

   if (Common_Dcl_DataIns.SelClsList.get_count() == 0)
      { GS_ERR_COD = eGSNotClassExtr; return GS_BAD; }
   else
   if (Common_Dcl_DataIns.SelClsList.get_count() == 1)
   {
      SEL_CLS = (C_CLASS *) ((C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_head())->get_class();
      if (SEL_CLS->get_category() != CAT_EXTERN ||
          (SEL_CLS->get_category() == CAT_EXTERN && SEL_CLS->ptr_sub_list()->get_count() == 1))
      {
         if (pSelCls) *pSelCls = SEL_CLS;
         // Non esco se si tratta di inserimento per poter scegliere se 
         // abilitare o meno le connessioni
         if (mode != INSERT) return GS_GOOD;
      }
   }

   Common_Dcl_DataIns.mode = mode;
   // Ordino la lista alfabeticamente
   Common_Dcl_DataIns.SelClsList.sort_name();
      
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &DclFile) == RTERROR) return GS_BAD;
    
   do
   {
      switch (mode)
      {
         case INSERT: // selezione di una classe per inserimento oggetti
            ads_new_dialog(_T("InsertClass"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
            ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_InsertClass_help);
            break;
         case NONE:   // selezione di una classe tra quelle estratte
            ads_new_dialog(_T("SelectClass"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
            ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SelectClass_help);
            break;
         default:
            GS_ERR_COD = eGSInvalidArg; 
            return GS_BAD; 
      }

      if (dcl_id == NULL)
      {
         ads_unload_dialog(DclFile);
         GS_ERR_COD = eGSAbortDCL; 
         return GS_BAD; 
      }
     
      // RIEMPIE LA LIST-BOX DELLE CLASSI SELEZIONABILI
      ads_start_list(dcl_id, _T("listcls"), LIST_NEW, 0);
      punt = (C_CLS_PUNT *) Common_Dcl_DataIns.SelClsList.get_head();
      while (punt)
      {
         gsc_add_list(punt->get_name());
         punt = (C_CLS_PUNT *) punt->get_next();
      }
      ads_end_list();
       
      // selezione di una classe per inserimento oggetti
      if (mode == INSERT)
      {
         ads_action_tile(dcl_id, _T("connect"), (CLIENTFUNC) dcl_sel_connect);
         if (ConnectCheck) ads_client_data_tile(dcl_id, _T("connect"), ConnectCheck);
         ads_mode_tile(dcl_id, _T("connect"), MODE_DISABLE);
      }

      // Se viene passata una lista di classi: "Selezione via Pick" non abilitata
      if (pClsListToSel && pClsListToSel->get_count() > 0)
         ads_mode_tile(dcl_id, _T("Graphical"), MODE_DISABLE);
      else
         ads_action_tile(dcl_id, _T("Graphical"), (CLIENTFUNC) dcl_sel_graph);

      // controlli in comune
      ads_action_tile(dcl_id, _T("listcls"), (CLIENTFUNC) dcl_sel_listcls);
      ads_client_data_tile(dcl_id, _T("listcls"), &(Common_Dcl_DataIns));

      ads_action_tile(dcl_id, _T("listsub"), (CLIENTFUNC) dcl_sel_listsub);
      ads_client_data_tile(dcl_id, _T("listsub"), &(Common_Dcl_DataIns.SelSubList));

      ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC)dcl_sel_accept_ok);
      ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC)dcl_sel_quitter);

      // CONTROLLA VALIDITA' ULTIMA CLASSE INSERITA ED INIZIALIZZA LA SELEZIONE //
      SEL_CLS = NULL;
      ads_mode_tile(dcl_id, _T("listcls"), MODE_SETFOCUS);

      if (GEOsimAppl::LAST_CLS != 0 && (pos = Common_Dcl_DataIns.SelClsList.getpos_key(GEOsimAppl::LAST_CLS)) != 0)
      {
         swprintf(val, 30, _T("%d"), pos - 1);
         packet.dialog      = dcl_id;
         packet.value       = val;
         packet.reason      = CBR_SELECT;
         packet.client_data = &Common_Dcl_DataIns;
         dcl_sel_listcls(&packet);
         ads_set_tile(dcl_id, _T("listcls"), val);

         if (ConnectCheck && *ConnectCheck == TRUE)
            ads_set_tile(dcl_id, _T("connect"), _T("abilit"));
         else
            ads_set_tile(dcl_id, _T("connect"), _T("disabilit"));

         if (GEOsimAppl::LAST_SUB != 0 && (pos = Common_Dcl_DataIns.SelSubList.getpos_key(GEOsimAppl::LAST_SUB)) != 0)
         {
            swprintf(val, 30, _T("%d"), pos - 1);
            packet.client_data = &Common_Dcl_DataIns.SelSubList;
            dcl_sel_listsub(&packet);
            ads_set_tile(dcl_id, _T("listsub"), val);
            ads_mode_tile(dcl_id, _T("listsub"), MODE_SETFOCUS);
         }
      }
   
      // LANCIA LA DIALOG-BOX
      ads_start_dialog(dcl_id, &status);

      if (status == DLGSTATUS) // selezione classe da grafica
      {
         int       ret;
         C_CLASS   *pCls;

         if ((ret = gsc_select_entity(NULL, NULL, NULL, NULL, &pCls)) == GS_GOOD) 
         {
            SEL_CLS = pCls;
            status = 1;       // OK classe selezionata
         }
      }
   }
   while (status != DLGCANCEL && status != DLGOK); // finche si esce con "OK" o "ANNULLA"

   ads_unload_dialog(DclFile);
   if (SEL_CLS == NULL) return GS_CAN;
   
   if (pSelCls) *pSelCls = SEL_CLS;
   
   return GS_GOOD; 
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER SELEZIONE CLASSE VIA DCL  -  FINE
///////////////////////////////////////////////////////////////////////////////


/*+
-------------------------------------------------------------
int gsc_ddselect_subnode(C_CONNECT_LIST *con_list) -> Lancia DCL.

  Funzioni per la gestione della DCL per la selezione delle
  sotto classi da inserire.
  Comunicano tra loro e con le funzioni esterne attraverso
  le variabili globali :
       SEL_EXT puntatore alla classe simulazione madre.
       SEL_CLS puntatore alla classe selezionata per l'inserimento.
-------------------------------------------------------------
-*/
// ACTION TILE : click su lista //
static void CALLB dcl_sel_listnode(ads_callback_packet *dcl)
{
   int        pos;
   ads_hdlg   hdlg=dcl->dialog;
   C_CLS_PUNT *punt;
   C_CLS_PUNT_LIST *pSelSubList = (C_CLS_PUNT_LIST *) dcl->client_data; // LISTA PUNT. SOTTOCLASSI SELEZ.

   pos     = _wtoi(dcl->value);
   punt    = (C_CLS_PUNT *) pSelSubList->getptr_at(pos+1);
   SEL_CLS = (C_CLASS *) punt->get_class();

   if (dcl->reason == CBR_DOUBLE_CLICK) ads_done_dialog(hdlg, DLGOK);
}
// ACTION TILE : click su tasto OK //
static void CALLB dcl_node_accept_ok(ads_callback_packet *dcl)
{
   if (SEL_CLS != NULL)
      ads_done_dialog(dcl->dialog, DLGOK);
}
// ACTION TILE : click su tasto CANCEL //
static void CALLB dcl_node_quitter(ads_callback_packet *dcl)
{
   SEL_CLS = NULL;
   ads_done_dialog(dcl->dialog, DLGCANCEL);         
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_node_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Inserimentoentit); } 

   
/////////////////////////////////////////////////////////
// FUNZIONE PER SELEZIONE NODO DI PARTENZA E DI ARRIVO //
/////////////////////////////////////////////////////////
int gsc_ddselect_subnode(C_CONNECT_LIST *con_list, TCHAR *title)
{
   C_STRING            path;
   TCHAR               val[30];   
   ads_hdlg            dcl_id;
   ads_callback_packet packet;
   int                 status, pos, DclFile;
   C_CLS_PUNT          *punt,*new_nod;
   C_LIST              *sub_list;
   C_INT_INT           *con;
   C_CLASS             *cls;
   C_CLS_PUNT_LIST     SelSubList;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (!con_list || !title) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // RIEMPIE LA LISTA DEI NODI
   SelSubList.remove_all();
   con = (C_INT_INT *) con_list->get_head();
   while (con)
   {
      cls = NULL;
      if (SEL_EXT)  sub_list = SEL_EXT->ptr_sub_list();
      if (sub_list) cls = (C_CLASS*)sub_list->search_key(con->get_key());
      if (cls)
      {
         if ((new_nod = new C_CLS_PUNT(cls)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; SelSubList.remove_all(); return GS_BAD; }
         SelSubList.add_tail(new_nod);
      } 
      con = (C_INT_INT *) con->get_next();
   }

   status = con_list->get_count();
   if (status == 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (status == 1)
   {
      punt=(C_CLS_PUNT*)SelSubList.get_head();
      SEL_CLS=(C_CLASS*)punt->get_class();
      return GS_GOOD;
   }
   
   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_GRAPH.DCL");
   if (gsc_load_dialog(path, &DclFile) == RTERROR) return GS_BAD;
    
   ads_new_dialog(title, DclFile, (CLIENTFUNC)NULLCB, &dcl_id);
   if (dcl_id == NULL)
   {
      ads_unload_dialog(DclFile);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD; 
   }
     
   // RIEMPIE LA LIST-BOX DELLE CLASSI SELEZIONABILI
   ads_start_list(dcl_id, _T("listnode"), LIST_NEW, 0);
   punt=(C_CLS_PUNT*)SelSubList.get_head();
   while(punt!=NULL)
   {
      gsc_add_list(punt->get_name());
      punt=(C_CLS_PUNT*)punt->get_next(); 
   }
   ads_end_list();
       
   ads_action_tile(dcl_id, _T("listnode"), (CLIENTFUNC) dcl_sel_listnode);
   ads_client_data_tile(dcl_id, _T("listnode"), &SelSubList);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_node_accept_ok);
   ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC) dcl_node_quitter);
   ads_action_tile(dcl_id, _T("help"),   (CLIENTFUNC) dcl_node_help);

   // CONTROLLA VALIDITA' ULTIMO NODO INSERITO ED INIZIALIZZA LA SELEZIONE //
   SEL_CLS=NULL;
   ads_mode_tile(dcl_id, _T("listnode"), MODE_SETFOCUS);
   if (GS_LAST_NODE_CLS==SEL_EXT->get_key())
      if (GS_LAST_NODE_SUB != 0 && (pos = SelSubList.getpos_key(GS_LAST_NODE_SUB))!=0)
      {
         swprintf(val, 30, _T("%d"), pos - 1);
         packet.dialog      = dcl_id;
         packet.value       = val;
         packet.reason      = CBR_SELECT;
         packet.client_data = &SelSubList;

         dcl_sel_listnode(&packet);
         ads_set_tile(dcl_id, _T("listnode"), val);
      }

// LANCIA LA DIALOG-BOX //
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(DclFile);

   if (SEL_CLS==NULL) return GS_CAN;
   return GS_GOOD; 
}


/*************************************************************/
/*.doc gs_get_group_by_member
/*+                                                                       
  Funzione LISP che restituisce la lista (eventuale) di entità collegate
  tra loro da un entità della classe gruppo.
  Parametri:
  lista di RESBUF
  (<prj><cls semplice><cod ent semplice><cls group>)

  Restituisce una lista in caso di successo altrimenti nil. 
-*/  
/*************************************************************/
int gs_get_group_by_member(void)
{
   presbuf           arg = acedGetArgs();
   C_RB_LIST         ret;
   int               prj, memb_cls, grp_cls;
   long              memb_key;
   C_CLASS           *p_group;
   C_ENT_FAMILY_LIST member_list;

   acedRetNil(); 

   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) // codice progetto
      { GS_ERR_COD = eGSInvalidPrjCode; return RTERROR; }

   if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &memb_cls) == GS_BAD) // codice classe membro
      { GS_ERR_COD = eGSInvClassCode; return RTERROR; }

   if (!(arg = arg->rbnext) || gsc_rb2Lng(arg, &memb_key) == GS_BAD) // codice chiave membro
      { GS_ERR_COD = eGSInvalidKey; return RTERROR; }

   if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &grp_cls) == GS_BAD) // codice classe gruppo
      { GS_ERR_COD = eGSInvClassCode; return RTERROR; }

   if ((p_group = gsc_find_class(prj, grp_cls)) == NULL) return RTERROR;
   if (p_group->fill_group_class(memb_cls, memb_key, member_list) == GS_BAD) return RTERROR;

   if ((ret << member_list.to_rb()) != NULL)
      ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc C_CLASS::get_graphical_data <external> */
/*+
  Questa funzione ricava dei dati grafici di una entità GEOsim.
  Parametri:
  C_RB_LIST &key_values;   lista valori chiave ((attrib valore) ...)
  TCHAR     *what;         espressione (es: ".color", ".area", ".length" ...)
  int       len;           usato solo per what = ".x1,.y1.z1"
  int       dec;           usato solo per what = ".x1,.y1.z1"
  TCHAR     *type;         tipo del risultato (short, long, real, string, point)
                           se = NULL decide il tipo in base a what
  oppure
  C_SELSET &SelSet;        gruupo di selzione oggetti
  TCHAR     *what;         espressione (es: ".color", ".area", ".length" ...)
  int       len;           usato solo per what = ".x1,.y1.z1"
  int       dec;           usato solo per what = ".x1,.y1.z1"
  TCHAR     *type;         tipo del risultato (short, long, real, string, point)
                           se = NULL decide il tipo in base a what
  bool SelSetToClean;      Flag; se = true la funzione provvede a pulire SetSet
                           escludendo i blocchi DA e i riempimenti, se false si
                           intende che SetSet sia già pulito (default = true)

  Restituisce puntatore a resbuf in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf C_CLASS::get_graphical_data(C_RB_LIST &key_values, TCHAR *what, int len, int dec,
                                    TCHAR *type)
{
   C_SELSET SelSet;

   if (get_SelSet(key_values, SelSet, GRAPHICAL) == GS_BAD) return NULL;

   return get_graphical_data(SelSet, what, len, dec, type);
}
presbuf C_CLASS::get_graphical_data(C_SELSET &SelSet, TCHAR *what, int len, int dec,
                                    const TCHAR *type, bool SelSetToClean)
{
   presbuf   result = NULL;
   C_STRING  upper_what;
   ads_name  entity;
   long      ItemNum = 0;
   C_RB_LIST rb_list;
   C_SELSET  *pInternalSS;

   if (!type) type = gsc_get_graphical_data_type(what);
   if (!what || !type) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return NULL; }

   upper_what = what;
   gsc_toupper(upper_what.get_name());

   do
   {
      if (SelSetToClean)
      {
         if ((pInternalSS = new C_SELSET()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
         // ricavo un SelSet di soli oggetti grafici (senza blocchi DA)
         if (SelSet.copy(*pInternalSS) == GS_BAD) break;
         if (pInternalSS->intersectType(GRAPHICAL) == GS_BAD) break;
         // scarto i riempimenti
         if (pInternalSS->subtract(_T("HATCH")) == GS_BAD) break;
      }
      else
         pInternalSS = &SelSet;

      if (gsc_strcmp(upper_what.get_name(), GS_LISP_AREA) == 0)
      {
         if ((result = acutBuildList(RTREAL, gsc_get_graphical_area(*pInternalSS), 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
      }
      else
      if (gsc_strcmp(upper_what.get_name(), GS_LISP_LENGTH) == 0)
      {
         ads_real parziale = 0.0;
         long     i = 0;

         while (pInternalSS->entname(i++, entity) == GS_GOOD)
         {
            if ((result = gsc_get_graphical_data(entity, upper_what.get_name(), type)) == NULL)
               break;

            parziale += result->resval.rreal;
            acutRelRb(result);
         }
         if (result == NULL) break;
         if ((result = acutBuildList(RTREAL, parziale, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
      }
      else
      if (gsc_strcmp(upper_what.get_name(), GS_LISP_WINDOW) == 0)
      {
         ads_point corner1, corner2;
         ads_point ptDest1, ptDest2;
         C_STRING  StrWindow, dummy;
  
         if (pInternalSS->getWindow(corner1, corner2) == GS_BAD) break;

         // Se il sistema di coordinate della sessione corrente non coincide con quello della classe
         // allora il punto è memorizzato nel sistema di coordinate della classe
         if (ptr_GphInfo() && ptr_GphInfo()->get_isCoordToConvert() == GS_GOOD)
         {
            TCHAR *pDestSRID;

            // se la geometria è in tabella DB
            if (ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource) 
               pDestSRID = ((C_DBGPH_INFO*)ptr_GphInfo())->get_ClsSRID_converted_to_AutocadSRID()->get_name();
            else  // la geometria è in DWG
               pDestSRID = ptr_GphInfo()->coordinate_system.get_name();

            if (gsc_getCoordConv(corner1, GS_CURRENT_WRK_SESSION->get_coordinate(),
                                 ptDest1, pDestSRID) != GS_GOOD ||
                  gsc_getCoordConv(corner2, GS_CURRENT_WRK_SESSION->get_coordinate(),
                                 ptDest2, pDestSRID) != GS_GOOD)
            {
               ads_point_set(corner1, ptDest1);
               ads_point_set(corner2, ptDest2);
            }
         }
         else
         {
            ads_point_set(corner1, ptDest1);
            ads_point_set(corner2, ptDest2);
         }

         // Nel caso di punti viene convertito in stringa (non esiste il campo di tipo "POINT")
         // usando il sistema di coordinate della classe

         if (StrWindow.paste(gsc_tostring(ptDest1, 15)) == NULL) break;
         if (dummy.paste(gsc_tostring(ptDest2, 15)) == NULL) break;
         StrWindow += _T(";");
         StrWindow += dummy;
      
         if ((result = acutBuildList(RTSTR, StrWindow.get_name(), 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; break; }
      }
      else
      if (gsc_strcmp(upper_what.get_name(), GS_LISP_ROTATION_DEGREE) == 0)
      {
         double rot;

         // cerco primo oggetto grafico
         if (pInternalSS->entname(0, entity) != GS_GOOD) break;
         
         if ((result = gsc_get_graphical_data(entity, GS_LISP_ROTATION, type)) == NULL)
            break;
         // converto la rotazione da radianti in gradi.
         if (gsc_rb2Dbl(result, &rot) == GS_GOOD)
            gsc_RbSubst(result, gsc_rad2grd(rot));
      }
      else
      {
         TCHAR *pDestSRID = NULL;

         // cerco primo oggetto grafico
         if (pInternalSS->entname(0, entity) != GS_GOOD) break;
         
         // Se il sistema di coordinate della sessione corrente non coincide con quello della classe
         // allora il punto è memorizzato nel sistema di coordinate della classe
         if (ptr_GphInfo() && ptr_GphInfo()->get_isCoordToConvert() == GS_GOOD)
            // se la geometria è in tabella DB
            if (ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource) 
               pDestSRID = ((C_DBGPH_INFO*)ptr_GphInfo())->get_ClsSRID_converted_to_AutocadSRID()->get_name();
            else  // la geometria è in DWG
               pDestSRID = ptr_GphInfo()->coordinate_system.get_name();

         if ((result = gsc_get_graphical_data(entity, upper_what.get_name(), type, pDestSRID)) == NULL)
            break;

         // Nel caso di punto viene convertito in stringa (non esiste il campo di tipo "POINT")
         if (gsc_strcmp(type, _T("POINT"), FALSE) == 0)
         {
            if ((result->resval.rstring = gsc_tostring(result->resval.rpoint, len)) == NULL)
               { acutRelRb(result); result = NULL; break; }

            result->restype = RTSTR;
         }
      }
   }
   while (0);

   if (SelSetToClean && pInternalSS) delete pInternalSS;

   return result;
}


/*********************************************************/
/*.doc gsc_get_graphical_data <external> */
/*+
  Questa funzione ricava dei dati grafici di una entità.
  Parametri:
  ads_name ent;         oggetto grafico
  TCHAR   *what;        espressione (es: ".color", ".area", ".length" ...)
  const TCHAR *type;    tipo del risultato (short, long, real, string, point)
                        se = NULL decide il tipo in base a what
  TCHAR *pDestSRID;     Codice del sistema di coordinate Autocad di destinazione
                        (default = NULL)

  Restituisce puntatore a resbuf in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
presbuf gsc_get_graphical_data(ads_name ent, TCHAR *what, const TCHAR *type, TCHAR *pDestSRID)
{
   presbuf result = NULL;

   if (!type) type = gsc_get_graphical_data_type(what);
   if (!ent || !what || !type) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   // punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_INS_POINT, FALSE) == 0 || 
       gsc_strcmp(what, GS_LISP_SPOINT, FALSE) == 0)
      result = ade_expreval(ent, _T(".x1,.y1,.z1"), _T("point"));
   else
   // coord X punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_X_INS_POINT, FALSE) == 0 ||
       gsc_strcmp(what, GS_LISP_X_SPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x1,.y1,.z1"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[X], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[X], 0);
      }
   }
   else
   // coord Y punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_Y_INS_POINT, FALSE) == 0 ||
       gsc_strcmp(what, GS_LISP_Y_SPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x1,.y1,.z1"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[Y], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[Y], 0);
      }
   }
   else
   // coord Z punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_Z_INS_POINT, FALSE) == 0 ||
       gsc_strcmp(what, GS_LISP_Z_SPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x1,.y1,.z1"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[Z], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[Z], 0);
      }
   }
   else
   // punto finale
   if (gsc_strcmp(what, GS_LISP_EPOINT, FALSE) == 0)
      result = ade_expreval(ent, _T(".x2,.y2,.z2"), _T("point"));
   else
   // coord X punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_X_EPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x2,.y2,.z2"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[X], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[X], 0);
      }
   }
   else
   // coord Y punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_Y_EPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x2,.y2,.z2"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[Y], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[Y], 0);
      }
   }
   else
   // coord Z punto di inserimento o punto iniziale
   if (gsc_strcmp(what, GS_LISP_Z_EPOINT, FALSE) == 0)
   {
      C_RB_LIST partial;

      if ((partial << ade_expreval(ent, _T(".x2,.y2,.z2"), _T("point"))) != NULL)
      {
         ads_point ptDest;

         if (gsc_getCoordConv(partial.get_head()->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            result = acutBuildList(RTREAL, ptDest[Z], 0);
         else
            result = acutBuildList(RTREAL, partial.get_head()->resval.rpoint[Z], 0);
      }
   }
   else
   if (gsc_strcmp(what, GS_LISP_WINDOW, FALSE) == 0)       // finestra di occupazione
   {
      ads_point corner1, corner2;
      C_SELSET  ss;
   
      ss.add(ent);
      if (ss.getWindow(corner1, corner2) == GS_GOOD)
      {
         C_STRING  StrWindow, dummy;
         ads_point ptDest1, ptDest2;

         if (gsc_getCoordConv(corner1, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest1, pDestSRID) == GS_GOOD &&
               gsc_getCoordConv(corner2, GS_CURRENT_WRK_SESSION->get_coordinate(),
                                ptDest2, pDestSRID) == GS_GOOD)
         {
            ads_point_set(ptDest1, corner1);
            ads_point_set(ptDest2, corner2);
         }

         StrWindow.paste(gsc_tostring(corner1, 15));
         dummy.paste(gsc_tostring(corner2, 15));
         StrWindow += _T(";");
         StrWindow += dummy;

         result = acutBuildList(RTSTR, StrWindow.get_name(), 0);
      }
   }
   else
   if (gsc_strcmp(what, GS_LISP_INITIAL_NODE, FALSE) == 0)  // punto iniziale di un lato (grafi)
   {
      C_EED eed;

      if (eed.load(ent) == GS_GOOD)
         if ((result = acutBuildList(RTLONG, eed.initial_node, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
   }
   else
   if (gsc_strcmp(what, GS_LISP_FINAL_NODE, FALSE) == 0)  // punto finale di un lato (grafi)
   {
      C_EED eed;

      if (eed.load(ent) == GS_GOOD)
         if ((result = acutBuildList(RTLONG, eed.final_node, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
   }
   else
   {
      C_STRING DummyType(type);
      result = ade_expreval(ent, what, DummyType.get_name());
   }

   // se il risultato non esiste
   if (result == NULL)
   {
      if (gsc_strcmp(type, _T("real"), GS_BAD) == 0) // Case insensitive
      {
         if ((result = acutBuildList(RTREAL, 0.0, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
      if (gsc_strcmp(type, _T("short"), GS_BAD) == 0) // Case insensitive
      {
         if ((result = acutBuildList(RTSHORT, 0, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
      if (gsc_strcmp(type, _T("long"), GS_BAD) == 0) // Case insensitive
      {
         if ((result = acutBuildList(RTLONG, 0, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
      if (gsc_strcmp(type, _T("string"), GS_BAD) == 0) // Case insensitive
      {
         if ((result = acutBuildList(RTSTR, GS_EMPTYSTR, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
      if (gsc_strcmp(type, _T("point"), GS_BAD) == 0) // Case insensitive
      {
         ads_point pt;
         ads_point_clear(pt);

         if ((result = acutBuildList(RT3DPOINT, pt, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
   }
   else
      if (gsc_strcmp(type, _T("point"), GS_BAD) == 0) // Case insensitive
      {
         ads_point ptDest;

         if (gsc_getCoordConv(result->resval.rpoint, GS_CURRENT_WRK_SESSION->get_coordinate(),
                              ptDest, pDestSRID) == GS_GOOD)
            ads_point_set(pDestSRID, result->resval.rpoint);
      }


   return result;
}


/*********************************************************/
/*.doc gsc_get_graphical_data_type <external> */
/*+
  Questa funzione ricava il tipo di risultato di una data funzione
  di calcolo di dati dalla grafica.
  Parametri:
  TCHAR *what; espressione (es: ".color", ".area", ".length" ...)
  
  Restituisce il tipo del risultato (short, long, real, string, point)
  in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
const TCHAR *gsc_get_graphical_data_type(TCHAR *what)
{
   C_STRING str;

   if (!what) return NULL;

   str = what;
   str.alltrim();
   str.toupper();

   for (long i = 0; i < VectorFromGraphGEOLispFunctionsLen(); i++)
      if (str.comp(VectorFromGraphGEOLispFunctions[i].Name, 0) == 0)
         return VectorFromGraphGEOLispFunctions[i].Type;

   return NULL;
}


/*********************************************************/
/*.doc gsc_get_graphical_area                 <external> */
/*+
  Questa funzione ricava l'area di una serie di superfici considerando le isole.
  Parametri:
  C_SELSET &SelSet; gruppo di selezione di aree
  
  Restituisce il calcolo della sessione in caso di successo altrimenti -1.
-*/  
/*********************************************************/
double gsc_get_graphical_area(C_SELSET &SelSet)
{
   presbuf   result;
   ads_name  entity, ContainerEnt;
   long      i = 0, j;
   int       Contained;
   double    area;

   area = 0.0;

   // fra le superfici ricavo quelle non contenuta da altre
   while (SelSet.entname(i, entity) == GS_GOOD)
   {
      j         = 0;
      Contained = FALSE;
      while (SelSet.entname(j, ContainerEnt) == GS_GOOD)
         if (j != i && gsc_IsInternalEnt(ContainerEnt, entity) == GS_GOOD)
         {
            Contained = TRUE;
            break; 
         }
         else j++;

      if (!Contained) // non contenuta da altre superfici
      {
         C_SELSET ContainedSS;

         ads_name_set(entity, ContainerEnt);

         if ((result = gsc_get_graphical_data(ContainerEnt, _T(".area"), _T("real"))) != NULL)
         {
            area += result->resval.rreal;
            acutRelRb(result);
         }

         // Ricavo le superfici contenute da questa
         ContainedSS.clear();
         j = 0;
         while (SelSet.entname(j, entity) == GS_GOOD)
         {
            if (j != i && gsc_IsInternalEnt(ContainerEnt, entity) == GS_GOOD)
               ContainedSS.add(entity);
            j++;
         }
         
         area -= gsc_get_graphical_area(ContainedSS);
      }

      i++;
   }
   
   return area;
}


/////////////////////////////////////////////////////////////////////////
///////////////// INIZIO FUNZIONI PER C_ATTRIB_LIST /////////////////////
/////////////////////////////////////////////////////////////////////////


/************************************************************/
/*.doc gsc_upd_initfinal_node
/*+
  Questa funzione aggiorna il codice del nodo iniziale e finale
  Parametri:
  C_ATTRIB_LIST *p_attrib_list;  lista attributi
  C_RB_LIST &ColValues;          lista valori
  long initial_node;             codice nodo iniziale
  long final_node;               codice nodo finale
                             
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_upd_initfinal_node(C_ATTRIB_LIST *p_attrib_list, C_RB_LIST &ColValues,
                           long initial_node, long final_node)
{
   C_ATTRIB *p_attrib;
   presbuf  p;
   int      result = GS_GOOD;

   p_attrib = (C_ATTRIB *) p_attrib_list->get_head();
   while (p_attrib)
   {
      if (p_attrib->is_calculated() == GS_GOOD)
      {
         if (gsc_strcmp(p_attrib->calc_func, GS_LISP_INITIAL_NODE, FALSE) == 0)
         { // modifico initial_node in lista resbuf
            if ((p = ColValues.CdrAssoc(p_attrib->get_name())) == NULL)
               { result = GS_BAD; break; }
            if (gsc_RbSubst(p, initial_node) == GS_BAD) { result = GS_BAD; break; }
         }
         else
            if (gsc_strcmp(p_attrib->calc_func, GS_LISP_FINAL_NODE, FALSE) == 0)
            { // modifico final_node in lista resbuf
               if ((p = ColValues.CdrAssoc(p_attrib->get_name())) == NULL)
                  { result = GS_BAD; break; }
               if (gsc_RbSubst(p, final_node) == GS_BAD) { result = GS_BAD; break; }
            }
      }

      p_attrib = (C_ATTRIB *) p_attrib->get_next();
   }

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA C_ATTRIB_LIST                  */
/*  INIZIO FUNZIONI DELLA C_CONNECT_LIST                 */
/*********************************************************/
 

/*********************************************************/
/*.doc C_CONNECT_LIST::is_to_be_connected <internal> */
/*+
  Questa funzione verifica se ci sono dei collegamenti da rispettare.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CONNECT_LIST::is_to_be_connected(void)
{
   int       ret = GS_BAD;
   C_INT_INT *punt;

   if (get_count() > 0)
   {
      // verifico che ci sia almeno un controllo di connessione
      punt = (C_INT_INT *) get_head();
      while (punt)
      {
         if (punt->get_type() != NO_CONCT_CONTROL && punt->get_type() != NO_OVERLAP)
            { ret = GS_GOOD; break; }
         punt = (C_INT_INT *) get_next();
      }
   }
   return ret;
}
 

/*********************************************************/
/*.doc C_CONNECT_LIST::is_to_be_unique        <internal> */
/*+
  Questa funzione verifica se ci sono dei controlli sulla sovrapposizione
  di oggetti.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CONNECT_LIST::is_to_be_unique(void)
{
   int       ret = GS_BAD;
   C_INT_INT *punt;

   if (get_count() > 0)
   {
      // verifico che ci sia almeno un controllo di sovrapposizione
      punt = (C_INT_INT *) get_head();
      while (punt)
      {
         if (punt->get_type() & NO_OVERLAP) { ret = GS_GOOD; break; }
         punt = (C_INT_INT *) get_next();
      }
   }
   return ret;
}


/*********************************************************/
/*  FINE FUNZIONI DELLA C_CONNECT_LIST                   */
/*********************************************************/


/************************************************************/
/*.doc gsc_checkValue                           < external> */
/*+
  Questa funzione controlla un valore letto verificando le sue 
  caratteristiche rispetto ad un attibuto, di una lista Resbuf
  di attributi e di un Drv.
  Parametri:
  C_CLASS   *pCls;      puntatore a classe GEOsim
  presbuf   pRbAttrib   puntatore a resbuf dell'attributo corrente
  C_ATTRIB  *pAttrib    Attributo di riferimento
  TCHAR     *Value;     Valore dell'attributo in formato stringa

  Restituisce in caso di errore il messaggio da stampare, altrimenti NULL.
  N.B. Alloca memoria.
-*/  
/************************************************************/
TCHAR *gsc_checkValue(C_CLASS *pCls, presbuf pRbAttrib, C_ATTRIB *pAttrib, TCHAR *Value)
{
   C_DBCONNECTION *pOldConn;
   presbuf        pRB;

   if (!pCls->ptr_attrib_list() || !pCls->ptr_info())
      { GS_ERR_COD = eGSInvClassType; return NULL; }

   if ((pOldConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL) return NULL;
   
   if (pCls->ptr_attrib_list()->init_ADOType(pOldConn) == GS_BAD) return NULL;
  
   // converto valore in resbuf
   if ((pRB = pOldConn->ptr_DataTypeList()->DataValueStr2Rb(pAttrib->type, Value)) == NULL)
      return gsc_tostring(gsc_msg(223));  // *Errore* Tipo attributo non valido.

   if (pAttrib->CheckValue(pRB) == GS_BAD)
   {
      switch (GS_ERR_COD)
      {
         case eGSInvAttribLen:
            return gsc_tostring(gsc_msg(222));  // *Errore* Lunghezza attributo non valida.
         case eGSInvAttribType:
            return gsc_tostring(gsc_msg(223));  // *Errore* Tipo attributo non valido.
         case eGSObbligFound:
            return gsc_tostring(gsc_msg(78));   // *Errore* Obbligatorità attributo non soddisfatta.
         default:
            return gsc_tostring(gsc_msg(224));  // *Errore* Valore attributo non valido.
      }
   }

   if (pRbAttrib->rbnext == NULL || 
       gsc_sostitutebuf(pRB, pRbAttrib->rbnext) == GS_BAD)
   {
      acutRelRb(pRB);
      return gsc_tostring(gsc_msg(225));        // *Errore* Assegnazione fallita.
   }

   acutRelRb(pRB);
   
   return NULL;
}


/************************************************************/
/*.doc gsdummyalign                                         */
/*+                            
  Comando fasullo per attivare il riallineamento della banca dati di GEOsim
  in seguito a modifiche apportate con acad.
-*/  
/************************************************************/
void gsdummyalign()
{
   GEOsimAppl::CMDLIST.StartCmd();
   return GEOsimAppl::CMDLIST.EndCmd();
}


// ACTION TILE : click su tasto OK
static void CALLB classalign_ok(ads_callback_packet *dcl)
{  
   TCHAR val[3];
   int   Choose = 1;  // è un flag codificato come segue:
                     // 1 bit = si è scelto OK
                     // 2 bit = 1 se si vuole la FAS di default

   // letto flag di cambio FAS
   ads_get_tile(dcl->dialog, _T("defaultFAS"), val, 2);
   if (gsc_strcmp(val, _T("1")) == 0) Choose += 2;
   
   ads_done_dialog(dcl->dialog, Choose);
   return;
}

// ACTION TILE : click su tasto Cancel
static void CALLB classalign_cancel(ads_callback_packet *dcl)
{  
   ads_done_dialog(dcl->dialog, DLGCANCEL);
   return;
}

// ACTION TILE : click su tasto Help
static void CALLB classalign_help(ads_callback_packet *dcl)
{
   gsc_help(IDH_COMANDIDIGEOSIM);
}


/************************************************************/
/*.doc gsdummyalign                                         */
/*+                            
  Comando per riallineare le entità di GEOsim modificate con acad.
-*/  
/************************************************************/
void gsddclassalign(void)
{
   ads_hdlg dcl_id;
   int      status, DclFile;
   C_STRING pathfile;
   C_SELSET selset;

   GEOsimAppl::CMDLIST.StartCmd();

   do
   {
      if (gsc_ssget(NULL, NULL, NULL, NULL, selset) != RTNORM) return GEOsimAppl::CMDLIST.CancelCmd();
      if (selset.length() > 0) break;
   }
   while (1);

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   pathfile = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_AREA.DCL");
   if (gsc_load_dialog(pathfile, &DclFile) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd();

   if (ads_new_dialog(_T("class_align"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
       dcl_id == NULL)
      { GS_ERR_COD = eGSAbortDCL; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   if (ads_action_tile(dcl_id, _T("accept"), classalign_ok) != RTNORM)     // tasto Chiudi
      { GS_ERR_COD = eGSAbortDCL; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   if (ads_action_tile(dcl_id, _T("cancel"), classalign_cancel) != RTNORM) // tasto Cancel
      { GS_ERR_COD = eGSAbortDCL; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   if (ads_action_tile(dcl_id, _T("help"), classalign_help) != RTNORM)     // tasto Help
      { GS_ERR_COD = eGSAbortDCL; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   // status = 2 se OK e cambio FAS (vedi classalign_ok)
   // status = 3 se OK e non cambio FAS (vedi classalign_ok)
   if (ads_start_dialog(dcl_id, &status) != RTNORM)
      { GS_ERR_COD = eGSAbortDCL; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   ads_unload_dialog(DclFile);

   // "status" è un flag codificato come segue:
   // 1 bit = si è scelto OK (altrimenti è CANCEL)
   // 2 bit = 1 se si vuole la FAS di default (altrimenti rimane inalterata)
   
   if (status & 1)
   {
      GEOsimAppl::REFUSED_SS.clear();
      if (gsc_class_align(selset,
                          (status & 2) ? GS_GOOD : GS_BAD, // change_fas
                          GS_BAD,                          // AttribValuesFromVideo
                          &(GEOsimAppl::REFUSED_SS),       // SS
                          GS_GOOD) == -1)                  // CounterToVideo
         return GEOsimAppl::CMDLIST.ErrorCmd();
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


// funzione di ausilio a gsc_get_window
void set_window(ads_point corner1, ads_point corner2, ads_point point)
{  // controllo su angolo inferiore sinistro
   if (point[X] < corner1[X]) corner1[X] = point[X];
   else if (point[X] > corner2[X]) corner2[X] = point[X];
   // controllo su angolo superiore destro
   if (point[Y] < corner1[Y]) corner1[Y] = point[Y];
   else if (point[Y] > corner2[Y]) corner2[Y] = point[Y];
}


/************************************************************/
/*.doc gsc_get_window                                       */
/*+                            
  Questa funzione restituisce la finestra occupata dagli oggetti contenuti in
  un gruppo di selezione.
  Parametri:
  ads_name selset;      gruppo di selezione
  ads_point corner1;    angolo in basso a sinistra
  ads_point corner2;    angolo in alto a destra

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_window(ads_name selset, ads_point corner1, ads_point corner2)
{
   ads_name  ent;
   long      i = 0;
   int       first_time = TRUE;
   ads_point p1, p2;

   if (ads_name_nil(selset)) return GS_BAD;

   while (acedSSName(selset, i++, ent) == RTNORM)
   {
      if (gsc_get_ent_window(ent, p1, p2) == GS_BAD) return GS_BAD;

      if (first_time)
      {
         first_time = FALSE;

         ads_point_set(p1, corner1);
         ads_point_set(p2, corner2);
      }
      else
      {
         set_window(corner1, corner2, p1);
         set_window(corner1, corner2, p2);
      }
   }

   if (first_time) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; } // non c'era una bella mazza

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_get_ent_window                                       */
/*+                            
  Questa funzione restituisce la finestra occupata dall'oggetto grafico.
  Parametri:
  AcDbEntity *pEnt;     oggetto grafico
  ads_point corner1;    angolo in basso a sinistra
  ads_point corner2;    angolo in alto a destra
  
  oppure

  ads_name  ent;        oggetto grafico
  ads_point corner1;    angolo in basso a sinistra
  ads_point corner2;    angolo in alto a destra

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_ent_window(AcDbEntity *pEnt, ads_point corner1, ads_point corner2)
{
   AcDbExtents  Extent;
   AcGePoint3d  p1, p2;

   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {
      if (((AcDbBlockReference *) pEnt)->geomExtentsBestFit(Extent) != Acad::eOk)
         if (((AcDbBlockReference *) pEnt)->getGeomExtents(Extent) != Acad::eOk)
         {
            if (gsc_get_firstPoint((AcDbBlockReference *) pEnt, corner1) == GS_BAD)
               { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
            ads_point_set(corner1, corner2);

            return GS_GOOD;
         }
   }
   else
      if (pEnt->getGeomExtents(Extent) != Acad::eOk)
      {
         if (gsc_get_firstPoint((AcDbEntity *) pEnt, corner1) == GS_BAD)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         ads_point_set(corner1, corner2);        

         return GS_GOOD;
      }

   p1 = Extent.minPoint();
   p2 = Extent.maxPoint();

   corner1[X] = p1.x;
   corner1[Y] = p1.y;
   corner1[Z] = p1.z;

   corner2[X] = p2.x;
   corner2[Y] = p2.y;
   corner2[Z] = p2.z;

   return GS_GOOD;
}
int gsc_get_ent_window(ads_name ent, ads_point corner1, ads_point corner2)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   TCHAR        type[MAX_LEN_GRAPH_TYPE];

   // Ricavo il tipo di oggetto grafico
   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;
   // Se è attributo di un blocco
   if (gsc_strcmp(type, _T("ATTRIB")) == 0)
   {
      ads_point p1, p2, p3, p4;

      // devo usare questa funzione
      if (gsc_get_txt_rectangle(ent, p1, p2, p3, p4) == GS_BAD) return GS_BAD;

      ads_point_set(p1, corner1);     
      if (p2[X] < corner1[X]) corner1[X] = p2[X];
      if (p3[X] < corner1[X]) corner1[X] = p3[X];
      if (p4[X] < corner1[X]) corner1[X] = p4[X];
      if (p2[Y] < corner1[Y]) corner1[Y] = p2[Y];
      if (p3[Y] < corner1[Y]) corner1[Y] = p3[Y];
      if (p4[Y] < corner1[Y]) corner1[Y] = p4[Y];

      ads_point_set(p1, corner2);     
      if (p2[X] > corner2[X]) corner2[X] = p2[X];
      if (p3[X] > corner2[X]) corner2[X] = p3[X];
      if (p4[X] > corner2[X]) corner2[X] = p4[X];
      if (p2[Y] > corner2[Y]) corner2[Y] = p2[Y];
      if (p3[Y] > corner2[Y]) corner2[Y] = p3[Y];
      if (p4[Y] > corner2[Y]) corner2[Y] = p4[Y];

      return GS_GOOD;
   }

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (gsc_get_ent_window((AcDbEntity *) pObj, corner1, corner2) == GS_BAD)
      { pObj->close(); return GS_BAD; }

   if (pObj->close() != Acad::eOk) { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_get_window                                       */
/*+                            
  Questa funzione restituisce la finestra occupata dagli oggetti contenuti in
  un AcDbBlockTableRecordIterator.
  Parametri:
  AcDbBlockTableRecordIterator *pIterator;
  ads_point corner1;    angolo in basso a sinistra
  ads_point corner2;    angolo in alto a destra

  Restituisce GS_GOOD se è stato trovato almeno un oggetto altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_window(AcDbBlockTableRecordIterator *pIterator, ads_point corner1, ads_point corner2)
{
   AcDbEntity *pEntity;
   ads_point  p1, p2;
   int        Found = GS_BAD;

   // Cerco primo oggetto valido
   for (; !pIterator->done(); pIterator->step())
   {
      if (pIterator->getEntity(pEntity, AcDb::kForRead) != AcMap::kOk) return GS_BAD;
      if (gsc_get_ent_window(pEntity, p1, p2) == GS_GOOD)
      {
         pEntity->close();
         ads_point_set(p1, corner1);
         ads_point_set(p2, corner2);
         pIterator->step();
         Found = GS_GOOD;
         break;
      }
      pEntity->close();
   }

   // proseguo con gl altri oggetti
   for (; !pIterator->done(); pIterator->step())
   {
      if (pIterator->getEntity(pEntity, AcDb::kForRead) != AcMap::kOk) return GS_BAD;
      if (gsc_get_ent_window(pEntity, p1, p2) == GS_GOOD)
      {
         set_window(corner1, corner2, p1);
         set_window(corner1, corner2, p2);
      }
      pEntity->close();
   }

   return Found;
}


/*******************************************************/
/*.doc gs_get_window                                   */
/*+                                           
   Funzione LISP che che restituisce la regione rettangolare
   contenente gli oggetti di un gruppo di selezione.
-*/  
/*******************************************************/
int gs_get_window()
{
   presbuf   arg = acedGetArgs();
   ads_point corner1, corner2;
   C_RB_LIST ret;
   
   acedRetNil();

   // Il primo ed unico parametro è il gruppo di selezione
   if (!arg || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_get_window(arg->resval.rlname, corner1, corner2) == GS_BAD)
      return RTERROR;
   if ((ret << ads_buildlist(RTPOINT, corner1, RTPOINT, corner2, 0)) == NULL)
      return RTERROR;
   ret.LspRetList();

   return RTNORM;
}


/************************************************************/
/*.doc gsc_get_txt_rectangle                                */
/*+                            
  Questa funzione restituisce un rettangolo (anche ruotato) occupato
  dal testo (non va con MTEXT ma solo con TEXT).
  ads_name  ent;        oggetto grafico di tipo testo O attributo
  ads_point p1;    angolo del rettangolo basso-sx
  ads_point p2;    angolo del rettangolo alto-sx
  ads_point p3;    angolo del rettangolo alto-dx
  ads_point p4;    angolo del rettangolo basso-dx

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_txt_rectangle(ads_name ent, ads_point p1, ads_point p2,
                          ads_point p3, ads_point p4)
{
   C_RB_LIST TextEnt;
   presbuf   p;
   ads_point origin, LowLeft, UpRight;
   double    rot = 0;

   if ((TextEnt << acdbEntGet(ent)) == NULL) return GS_BAD;
   // punto di inserimento   
   if ((p = TextEnt.SearchType(10)) == NULL) return GS_BAD;
   ads_point_set(p->resval.rpoint, origin); // ECS coordinates 
   // rotazione
   if ((p = TextEnt.SearchType(50))) rot = p->resval.rreal;

   if (acedTextBox(TextEnt.get_head(), LowLeft, UpRight) != RTNORM) return GS_BAD;

   acutPolar(origin, rot, LowLeft[X], p1);
   acutPolar(p1, rot + PI / 2, UpRight[Y], p4);
   acutPolar(p1, rot + PI / 2, LowLeft[Y], p1);
   acutPolar(p1, rot, UpRight[X] - LowLeft[X], p2);
   acutPolar(p4, rot, UpRight[X] - LowLeft[X], p3);

   return GS_GOOD;
}


/**************************************************************/
/*.doc gsc_get_block_rectangle                                */
/*+                            
  Questa funzione restituisce un rettangolo (anche ruotato) occupato
  dal blocco.
  ads_name  ent;   oggetto grafico di tipo blocco
  ads_point p1;    angolo del rettangolo basso-sx
  ads_point p2;    angolo del rettangolo alto-sx
  ads_point p3;    angolo del rettangolo alto-dx
  ads_point p4;    angolo del rettangolo basso-dx

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_block_rectangle(ads_name ent, ads_point p1, ads_point p2,
                            ads_point p3, ads_point p4)
{     
   AcDbObjectId       objId;
   AcDbEntity         *pEnt;
   AcDbBlockReference *pTempEnt;
   double             rot;
   ads_point          InsPt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (!pEnt->isKindOf(AcDbBlockReference::desc()))
      { GS_ERR_COD = eGSInvGraphObjct; pEnt->close(); return GS_BAD; }

   // Clono e creo l'entità temporaneamente (poi la cancello)
   if ((pTempEnt = (AcDbBlockReference *) pEnt->clone()) == NULL) // da vedere deepClone
      { pEnt->close(); return GS_BAD; }
   pEnt->close();

   InsPt[X] = pTempEnt->position().x;
   InsPt[Y] = pTempEnt->position().y;
   InsPt[Z] = pTempEnt->position().z;

   rot = pTempEnt->rotation();
   pTempEnt->setRotation(0.0); // lo pongo orizzontale

   if (gsc_get_ent_window(pTempEnt, p1, p3) == GS_BAD)
      { delete pTempEnt; return GS_BAD; }
   delete pTempEnt;
   p2[X] = p1[X]; p2[Y] = p3[Y]; p2[Z] = p1[Z];
   p4[X] = p3[X]; p4[Y] = p1[Y]; p4[Z] = p1[Z];
  
   acutPolar(InsPt, acutAngle(InsPt, p1) + rot, gsc_dist(InsPt, p1), p1);
   acutPolar(InsPt, acutAngle(InsPt, p2) + rot, gsc_dist(InsPt, p2), p2);
   acutPolar(InsPt, acutAngle(InsPt, p3) + rot, gsc_dist(InsPt, p3), p3);
   acutPolar(InsPt, acutAngle(InsPt, p4) + rot, gsc_dist(InsPt, p4), p4);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getPtNoOverlappedEnt
/*+                                                                       
  Calcola il punto dove non ci siano sovrapposizioni tra ent ed altri oggetti grafici.
  Parametri:
  ads_name	ent;              nome entità 
  const TCHAR *NoOverlapMode; controlla che l'entità non
                              si sovrapponga ad altri oggetti grafici e in caso positivo
                              sposta il punto a destra ("R") a sinistra ("L") 
                              in alto ("U") o in basso ("D") finchè ci si allontana 
                              entro una distanza limite. 
                              Es. "U30,D20" significa "trova un punto in cui
                              non ci siano sovrapposizioni spostando in alto fino 
                              ad una distanza max di 30, in caso negativo prova 
                              in basso fino ad una distanza max di 20.
  ads_point RefPt;            punto base di riferimento (angolo in basso-sinistra 
                              del rettangolo di occupazione)
  ads_point NewPt;            nuovo punto di spostamento (angolo in basso-sinistra 
                              del rettangolo di occupazione nella nuova posizione)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_getPtNoOverlappedEnt(ads_name ent, const TCHAR *NoOverlapMode, 
                             ads_point RefPt, ads_point NewPt)
{
   ads_point  pt1, pt2, pt3, pt4;
   ads_point  ipt1, ipt2, ipt3, ipt4;
   C_RB_LIST  CoordList;
   C_SELSET   ss;
   long       Best_ssLength;
   C_STR_LIST ModeList;
   C_STR      *pMode;
   double     Limit, Width, Height, Dist;
	TCHAR      *ch, type[MAX_LEN_GRAPH_TYPE];
   ads_name   OwnerEnt;

   if (!NoOverlapMode) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   ads_name_clear(OwnerEnt);
   // Ricavo il tipo di oggetto grafico
   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;
   // Se è attributo di un blocco
   if (gsc_strcmp(type, _T("ATTRIB")) == 0)
   {
      gsc_getOwnerAdsName(ent, OwnerEnt);
      // Ricavo il rettangolo di occupazione di ent
      if (gsc_get_txt_rectangle(ent, pt1, pt2, pt3, pt4) == GS_BAD) return GS_BAD;
   }
   else
   if (gsc_strcmp(type, _T("TEXT")) == 0)
   {
      // Ricavo il rettangolo di occupazione di ent
      if (gsc_get_txt_rectangle(ent, pt1, pt2, pt3, pt4) == GS_BAD) return GS_BAD;
   }
   else
   if (gsc_strcmp(type, _T("INSERT")) == 0)
   {
      // Ricavo il rettangolo di occupazione di ent
      if (gsc_get_block_rectangle(ent, pt1, pt2, pt3, pt4) == GS_BAD) return GS_BAD;
   }
   else
   {
      // Ricavo la finestra di occupazione di ent
      if (gsc_get_ent_window(ent, pt1, pt3) == GS_BAD) return GS_BAD;

      pt2[X] = pt1[X]; pt2[Y] = pt3[Y];
      pt4[X] = pt3[X]; pt4[Y] = pt1[Y];
   }

   if ((Width  = pt3[X] - pt1[X]) == 0) return GS_BAD;
   if ((Height = pt3[Y] - pt1[Y]) == 0) return GS_BAD;
   ads_point_set(pt1, RefPt);

   // Filtro gli oggetti dentro il rettangolo dell'entità
   CoordList << acutBuildList(RTPOINT, pt1, RTPOINT, pt2, RTPOINT, pt3, RTPOINT, pt4, 0);

   //                     CoordList, type    , ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
   if (gsc_selObjsPolygon(CoordList, CROSSING, ss, GS_GOOD, GS_GOOD, GS_BAD, 0, 0, ALL) == GS_BAD)
      return GS_BAD;

   if (ads_name_nil(OwnerEnt)) ss.subtract_ent(ent); // sottraggo ent da ss
   else ss.subtract_ent(OwnerEnt); // sottraggo OwnerEnt da ss

   ads_point_set(pt1, NewPt);
   if ((Best_ssLength = ss.length()) == 0)
      return GS_GOOD; // trovato punto senza sovrapposizioni

   // Ciclo tra tutte le direzioni
   ModeList.from_str(NoOverlapMode);
   pMode = (C_STR *) ModeList.get_head();
   while (pMode)
   {
      // Limite di default = 5 volte la diagonale del rettangolo di occupazione
      Limit = gsc_dist(pt1, pt3) * 5;
      ch = pMode->get_name();
      while (*ch != _T('\0'))
      {  // se non è un carattere alfabetico
         if (iswalpha(*ch) == 0)
         {
            Limit = fabs(_wtof(ch));
            *ch = _T('\0');
            break;
         }
         ch++;
      }

      // Ciclo fino al limite di distanza
      ads_point_set(pt1, ipt1);
      ads_point_set(pt2, ipt2);
      ads_point_set(pt3, ipt3);
      ads_point_set(pt4, ipt4);
      if (pMode->comp(_T("U"), GS_BAD) == 0) // "Upper" verso l'alto
      {
         ipt1[Y] += Height; ipt2[Y] += Height; ipt3[Y] += Height; ipt4[Y] += Height;
         Dist = ipt1[Y] - pt1[Y];
      }
      else if (pMode->comp(_T("D"), GS_BAD) == 0) // "Down" verso il basso
      {
         ipt1[Y] -= Height; ipt2[Y] -= Height; ipt3[Y] -= Height; ipt4[Y] -= Height;
         Dist = pt1[Y] - ipt1[Y];
      }
      else if (pMode->comp(_T("R"), GS_BAD) == 0) // "Right" verso destra
      {
         ipt1[X] += Height; ipt2[X] += Height; ipt3[X] += Height; ipt4[X] += Height;
         Dist = ipt1[X] - pt1[X];
      }
      else if (pMode->comp(_T("L"), GS_BAD) == 0) // "Left" verso sinistra
      {
         ipt1[X] -= Height; ipt2[X] -= Height; ipt3[X] -= Height; ipt4[X] -= Height;
         Dist = pt1[X] - ipt1[X];
      }
      else return GS_BAD;

      while (Dist <= Limit)
      {
         // Filtro gli oggetti dentro il rettangolo dell'entità
         CoordList << acutBuildList(RTPOINT, ipt1, RTPOINT, ipt2, RTPOINT, ipt3, RTPOINT, ipt4, 0);

         //                     CoordList, type    , ss,ExactMode,CheckOn2D,OnlyInsPt,Cls,Sub,ObjType
         if (gsc_selObjsPolygon(CoordList, CROSSING, ss, GS_GOOD, GS_GOOD, GS_BAD, 0, 0, ALL) == GS_BAD)
            return GS_BAD;

         if (ads_name_nil(OwnerEnt)) ss.subtract_ent(ent); // sottraggo ent da ss
         else ss.subtract_ent(OwnerEnt); // sottraggo OwnerEnt da ss

         if (ss.length() < Best_ssLength)
         {
            ads_point_set(ipt1, NewPt);
            if ((Best_ssLength = ss.length()) == 0) break; // trovato punto senza sovrapposizioni
         }

         if (pMode->comp(_T("U"), GS_BAD) == 0) // "Upper" verso l'alto
         {
            ipt1[Y] += Height; ipt2[Y] += Height; ipt3[Y] += Height; ipt4[Y] += Height;
            Dist = ipt1[Y] - pt1[Y];
         }
         else if (pMode->comp(_T("D"), GS_BAD) == 0) // "Down" verso il basso
         {
            ipt1[Y] -= Height; ipt2[Y] -= Height; ipt3[Y] -= Height; ipt4[Y] -= Height;
            Dist = pt1[Y] - ipt1[Y];
         }
         else if (pMode->comp(_T("R"), GS_BAD) == 0) // "Right" verso destra
         {
            ipt1[X] += Height; ipt2[X] += Height; ipt3[X] += Height; ipt4[X] += Height;
            Dist = ipt1[X] - pt1[X];
         }
         else if (pMode->comp(_T("L"), GS_BAD) == 0) // "Left" verso sinistra
         {
            ipt1[X] -= Height; ipt2[X] -= Height; ipt3[X] -= Height; ipt4[X] -= Height;
            Dist = pt1[X] - ipt1[X];
         }
         else return GS_BAD;
      }

      if (Best_ssLength == 0) break; // trovato punto senza sovrapposizioni
      pMode = (C_STR *) pMode->get_next();
   }

   return GS_GOOD;
}


/************************************************************/
/*.doc gsc_get_AdeQryPolyFence_window                       */
/*+                            
  Questa funzione restituisce la finestra occupata dalla query
  ADE per Polyline, Polygon o per Fence.
  Parametri:
  C_RB_LIST &AdeQryCond;   Condizione della query ADE
  ads_point corner1;       Angolo in basso a sinistra
  ads_point corner2;       Angolo in alto a destra

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int gsc_get_AdeQryPolyFence_window(C_RB_LIST &AdeQryCond, ads_point corner1, ads_point corner2)
{
   presbuf          p;
   int              i = 11, isBulge = FALSE, PrevPt;
   Adesk::Boolean   IsClosed;
   AcGeDoubleArray  BulgeList;
   AcGePoint3dArray VertexList;         
   AcGePoint3d      Vertex, p1, p2;
   AcDbExtents      Extent;

   // controllo il tipo di contorno
   if ((p = AdeQryCond.getptr_at(6)) == NULL || p->restype != RTSTR) return GS_BAD;

   if (gsc_strcmp(p->resval.rstring, _T("polyline"), FALSE) == 0)
   {
      // controllo la modalità di ricerca
      if ((p = AdeQryCond.getptr_at(7)) == NULL || p->restype != RTSTR) return GS_BAD;

      if (gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0 || // polygon
          gsc_strcmp(p->resval.rstring, FENCE_SPATIAL_COND, FALSE) == 0   || // fence      
          gsc_strcmp(p->resval.rstring, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0) // fencebutstartend      
      {
         i = (gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0) ? 9 : 8; // polygon

         if ((p = AdeQryCond.getptr_at(i)) == NULL || p->restype != RTSHORT) return GS_BAD;
         IsClosed = (p->resval.rint == 0) ? Adesk::kFalse : Adesk::kTrue;

         // direzione del vettore; perchè sia parallelo deve essere (0.0 0.0 1.0)
         i++;
         if ((p = AdeQryCond.getptr_at(i)) == NULL || 
             (p->restype != RTPOINT && p->restype != RT3DPOINT) ||
             p->resval.rpoint[X] != 0.0 || p->resval.rpoint[Y] != 0.0 || p->resval.rpoint[Z] != 1.0)
            return GS_BAD;
      }
      else
         return GS_BAD;
   }
   else if (gsc_strcmp(p->resval.rstring, FENCE_SPATIAL_COND, FALSE) == 0 ||           // fence
            gsc_strcmp(p->resval.rstring, FENCEBUTSTARTEND_SPATIAL_COND, FALSE) == 0)  // fencebutstartend
   {
      i        = 6;
      IsClosed = Adesk::kFalse;
   }
   else if (gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0) // "polygon"
   {
      i        = 7;
      IsClosed = Adesk::kTrue;
   }
   else return GS_BAD;

   while ((p = AdeQryCond.getptr_at(++i)) != NULL)
   {
      if (p->restype == RTREAL) // bulge factor
      {
         isBulge = TRUE;
         BulgeList.append(p->resval.rreal);
         PrevPt = FALSE;
      }
      else if (p->restype == RTPOINT || p->restype == RT3DPOINT) // Vertex
      {
         Vertex.x = p->resval.rpoint[X];
         Vertex.y = p->resval.rpoint[Y];
         Vertex.z = (p->restype == RTPOINT) ? 0.0 : p->resval.rpoint[Z];
         VertexList.append(Vertex);
         
         if (isBulge && PrevPt)
            BulgeList.append(0.0);

         PrevPt = TRUE;
      }
      else break;
   }

   AcDbPolyline dummyPline(VertexList.length());
   AcGePoint2d  Vertice;

   for (int iVertex = 0; i < VertexList.length(); i++)
   {
      Vertice.set(VertexList[i].x, VertexList[i].y);
      dummyPline.addVertexAt(i, Vertice, (isBulge) ? BulgeList[i] : 0.0);
   }

   dummyPline.setClosed(IsClosed);
   if (dummyPline.getGeomExtents(Extent) != Acad::eOk) return GS_BAD;

   p1 = Extent.minPoint();
   p2 = Extent.maxPoint();
   corner1[X] = p1.x;
   corner1[Y] = p1.y;
   corner1[Z] = p1.z;
   corner2[X] = p2.x;
   corner2[Y] = p2.y;
   corner2[Z] = p2.z;

   return GS_GOOD;
}


/***************************************************************************/
/*.doc gsc_setVarForCommandSpeed() */
/*+                                                                       
  Funzione che memorizza in un resbuf una serie di variabili di memoria
  di AutoCAD.
  Le setta al valore voluto e restituisce attraverso il resbuf di
  output i valori vecchi nel seguente formato:
  (("BLIPMODE", 1)("HIGHLIGHT",1)("CMDECHO",1))
  
  Restituisce un resbuf in caso si successo altrimenti NULL. 
-*/  
/***************************************************************************/
presbuf gsc_setVarForCommandSpeed()
{
   struct resbuf  ValOld;
   presbuf        comodo;
   C_RB_LIST      ListVarOLd;

   // Memorizzo valori vecchi di Blipmode, highlight.
   if ((ListVarOLd << acutBuildList(RTLB, 0)) == NULL) return NULL;
   if (acedGetVar(_T("BLIPMODE"), &ValOld) != RTNORM)
      { GS_ERR_COD=eGSVarNotDef; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLB, RTSTR, _T("BLIPMODE"), 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((comodo = acutBuildList(RTSHORT, ValOld.resval.rint, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += comodo) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLE, 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   if (acedGetVar(_T("HIGHLIGHT"), &ValOld) != RTNORM)
      { GS_ERR_COD=eGSVarNotDef; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLB, RTSTR, _T("HIGHLIGHT"), 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((comodo = acutBuildList(RTSHORT, ValOld.resval.rint, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += comodo) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLE, 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLE, 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   if (acedGetVar(_T("CMDECHO"), &ValOld) != RTNORM)
      { GS_ERR_COD=eGSVarNotDef; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLB, RTSTR, _T("CMDECHO"), 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((comodo = acutBuildList(RTSHORT, ValOld.resval.rint, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += comodo) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLE, 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((ListVarOLd += acutBuildList(RTLE, 0)) == NULL) 
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   // Setto i valori di Blipmode, highlight.
   if ((comodo = acutBuildList(RTSHORT, 0, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if (acedSetVar(_T("BLIPMODE"), comodo) != RTNORM)
      { GS_ERR_COD = eGSNotAllocVar; return NULL; }

   if ((comodo = acutBuildList(RTSHORT, 0, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if (acedSetVar(_T("HIGHLIGHT"), comodo) != RTNORM)
      { GS_ERR_COD = eGSNotAllocVar; return NULL; }

   if ((comodo = acutBuildList(RTSHORT, 0, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if (acedSetVar(_T("CMDECHO"), comodo) != RTNORM)
      { GS_ERR_COD = eGSNotAllocVar; return NULL; }

   ListVarOLd.ReleaseAllAtDistruction(GS_BAD);

   return ListVarOLd.get_head();
}


/***************************************************************/
/*.doc gsc_setVarForCommandNormal(presbuf OldValues)           */
/*+                                                                       
  Funzione che setta le variabili di sistema di AutoCAD leggendole
  dal resbuf passato in input nel seguente formato:
  ( ( HIGHLIGHT , 1 ) ( BLIPMODE , 1 ) ... )
   
  Parametri:
  presbuf   OldValues      Resbuf di memorizzazione della Var.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/***************************************************************/
int gsc_setVarForCommandNormal(presbuf OldValues)
{
   presbuf  p, cmd;

   p = OldValues;
   while (p)
   {
      if ((p->restype == RTLE) || (p->restype == RTLB))
         { p = p->rbnext; continue; }
      if (p->restype == RTSTR)
      {
         cmd = p;
         if (p->rbnext != NULL)
         {   
            p = p->rbnext;
            if (acedSetVar(cmd->resval.rstring, p) != RTNORM)
               { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
         }
      }
      if (p->rbnext != NULL) p = p->rbnext;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_getCurrentScreenCoordinate                             <external> */
/*+                                           
   Funzione che restituisce le coordinate dello schemo attuale.
   Parametri:
   ads_point pt1;    angolo in basso a sinistra
   ads_point pt2;    angolo in altro a destra
   
   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_getCurrentScreenCoordinate(ads_point pt1, ads_point pt2)
{
   resbuf buffer;
   double len_X, len_Y, X_center = 0.0, Y_center = 0.0;

   // Dimensioni dello schermo = Variabile SCREENSIZE ( Dim X, Dim Y ) in pixel
   // lunghezza in unità della ordinata dello schermo = Variabile VIEWSIZE
   // lunghezza in unità dell' ascissa dello schermo:
   // proporzione VIEWSIZE : Dim Y = Incognita : Dim X
   //   P2                     P3
   //      -----------------
   //     |                 |
   //     |                 |
   //     |        X        |
   //     |                 |
   //     |                 |
   //      -----------------
   //   P1                     P4 

   // leggo variabile VIEWSIZE
   acedGetVar(_T("VIEWSIZE"), &buffer);          // lunghezza ordinata
   len_Y = buffer.resval.rreal;
   // leggo variabile VIEWCTR
   acedGetVar(_T("VIEWCTR"), &buffer);           // punto centrale dello schermo
   X_center = buffer.resval.rpoint[0];           // dimensione ascissa
   Y_center = buffer.resval.rpoint[1];           // dimensione ordinata
   // leggo variabile SCREENSIZE
   acedGetVar(_T("SCREENSIZE"), &buffer);
   len_X = len_Y * buffer.resval.rpoint[0] / buffer.resval.rpoint[1]; // lunghezza ascissa
   // Calcolo coordinate schermo
   pt1[X] = X_center - (len_X / 2);
   pt2[X] = X_center + (len_X / 2);
   pt1[Y] = Y_center - (len_Y / 2);
   pt2[Y] = Y_center + (len_Y / 2);

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_getKeyValue                                            */
/*+                                                            
  Funzione che legge il valore chiave di un identificatore di link.
  Parametri:
  ads_name ent;         oggetto grafico
  long     *KeyValue;   valore chiave

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int gsc_getKeyValue(ads_name ent, long *gs_id)
{
   C_CLASS *pclass;
   C_EED   eed;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (eed.load(ent) == GS_BAD)
      { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
   if ((pclass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), eed.cls, eed.sub)) == NULL)
      return GS_BAD;

   return pclass->getKeyValue(ent, gs_id);
}
int C_CLASS::getKeyValue(ads_name ent, long *gs_id)
{
   C_LINK Link(ent);
   int    _cls, _sub;

   if (Link.GetKey(gs_id, &_cls, &_sub) == GS_BAD) return GS_BAD;
   return (_cls == id.code && _sub == id.sub_code) ? GS_GOOD : GS_BAD;
}
int C_CLASS::getKeyValue(C_RB_LIST &ColValues, long *key)
{
   C_INFO  *p_info = ptr_info();
   presbuf p;

   if (!p_info) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // leggo il valore chiave
   if (!(p = ColValues.CdrAssoc(p_info->key_attrib.get_name())) || gsc_rb2Lng(p, key) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_CLASS::get_SelSet                                        */
/*+                                                            
  Funzione che restituisce il gruppo di oggetti dell'entità della classe 
  di GEOsim.
  Parametri:
  ads_name ent;      Oggetto grafico appartenente alla classe
  C_SELSET &SelSet;  gruppo di selezione
  int      what;     GRAPHICAL per gli oggetti grafici
                     DA_BLOCK  per i blocchi degli attributi visibili
                     ALL       per entrambi (per default)

  oppure

  long     key;      Valore chiave entità GEOsim
  C_SELSET &SelSet;  gruppo di selezione
  int      what;     GRAPHICAL per gli oggetti grafici
                     DA_BLOCK  per i blocchi degli attributi visibili
                     ALL       per entrambi (per default)

  oppure

  C_RB_LIST &ColValues; Valori degli attributi della scheda dell'entità
  C_SELSET &SelSet;     gruppo di selezione
  int      what;        GRAPHICAL per gli oggetti grafici
                        DA_BLOCK  per i blocchi degli attributi visibili
                        ALL       per entrambi (per default)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_CLASS::get_SelSet(ads_name ent, C_SELSET &SelSet, int what)
{
   C_LINK_SET LinkSS;

   if (LinkSS.GetSS(ent, SelSet) == GS_BAD) return GS_BAD;
   return SelSet.intersectType(what);
}
int C_CLASS::get_SelSet(long key, C_SELSET &SelSet, int what)
{
   C_LINK_SET LinkSS;

   // se gruppo
   if (ptr_group_list())
   {
      C_PREPARED_CMD   pTempCmd, pOldCmd;
      C_2INT_LONG_LIST MemberList;
      C_2INT_LONG      *pMember;
      C_CLASS          *pMemberClass;
      int              result = GS_GOOD;
      C_SELSET         entSS;

      SelSet.clear();
      if (prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) return GS_BAD;
      if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) return GS_BAD;

      // leggo i codici dei membri che costituiscono il gruppo
      if (get_member(pTempCmd, pOldCmd, key, &MemberList) == GS_BAD)
         return GS_BAD;

      // Per ogni membro del gruppo
      pMember = (C_2INT_LONG *) MemberList.get_head();
      while (pMember)
      {
   	   if ((pMemberClass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(),
                                            pMember->get_key(), pMember->get_type())) == NULL)
   		   { result = GS_BAD; break; }
         // ricavo il gruppo di oggetti grafici dell'entità membro
         if (pMemberClass->get_SelSet(pMember->get_id(), entSS) == GS_BAD)
            { result = GS_BAD; break; }
         SelSet.add_selset(entSS);

         pMember = (C_2INT_LONG *) MemberList.get_next();
      } 
   }
   else
      if (LinkSS.GetSS(id.code, id.sub_code, key, SelSet) == GS_BAD) return GS_BAD;

   return SelSet.intersectType(what);
}
int C_CLASS::get_SelSet(C_RB_LIST &ColValues, C_SELSET &SelSet, int what)
{
   long key;

   // cerco valore chiave
   if (getKeyValue(ColValues, &key) == GS_BAD) return GS_BAD;
   return get_SelSet(key, SelSet, what);
}


/******************************************************************/
/*.doc C_CLASS::get_SelSet                                        */
/*+                                                            
  Funzione che restituisce tutti gli oggetti grafici della classe.
  Parametri:
  C_SELSET &SelSet;  gruppo di selezione
  int      what;     GRAPHICAL per gli oggetti grafici
                     DA_BLOCK  per i blocchi degli attributi visibili
                     ALL       per entrambi (per default)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_CLASS::get_SelSet(C_SELSET &SelSet, int what)
{
   // Se classe spaghetti utilizzo SSGET filtrato sulle entità estese
   if (id.category == CAT_SPAGHETTI)
   {
      C_RB_LIST Xdata;

      if ((Xdata << acutBuildList(-3, 1001, GEO_APP_ID, 1070, ptr_id()->code, 0)) == NULL) 
         return GS_BAD;
      if (gsc_ssget(_T("X"), NULL, NULL, Xdata.get_head(), SelSet) != RTNORM) return GS_BAD;
      
      return GS_GOOD;
   }
   else
   {
      C_LINK_SET LinkSS;

      return LinkSS.GetGSClassSS(id.code, id.sub_code, SelSet, NULL, what);
   }
}


/******************************************************************/
/*.doc C_CLASS::get_SelSet                                        */
/*+                                                            
  Funzione che restituisce il link set di tutti gli oggetti grafici 
  della classe collegati a record che soddisfano la condizione SQL.
  Parametri:
  const TCHAR *Cond;    Condizione SQL 
  C_LINK_SET &LinkSet;  Link Set risultante
  C_SELSET *pInputSS;   Opzionale, gruppo di selezione sul quale eseguire 
                        la ricerca SQL (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_CLASS::get_LinkSet(const TCHAR *Cond, C_LINK_SET &LinkSet, C_SELSET *pInputSS)
{
   if (id.category == CAT_SPAGHETTI) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // La ricerca è su tutti gli oggetti grafici estratti nella sessione corrente
   if (!pInputSS)
   {
      double t1, t2;
      long   n;

      // Devo decidere quale tecnica usare tra
      // 1) db -> gph 
      if ((t1 = LinkSet.getApproxTimeForInitSQLCond((C_NODE *) this, Cond)) == -1)
         return GS_BAD;

      // 2) gph -> db
      // Si conta quanti sono gli oggetti grafici della classe
      if ((n = LinkSet.GetCountClassObjs(id.code, id.sub_code)) == -1 ||
          (t2 = LinkSet.getApproxTimeForInitSelSQLCond(n)) == -1)
         return GS_BAD;

      // Se il tempo con la tecnica 1 è minore
      if (t1 < t2)
      {
         if (LinkSet.initSQLCond((C_NODE *) this, Cond) == GS_BAD) return GS_BAD;
      }
      else
      {
         C_SELSET SSClass;

         if (LinkSet.GetGSClassSS(id.code, id.sub_code, SSClass, NULL, ALL) == GS_BAD) return GS_BAD;
         if (LinkSet.initSelSQLCond((C_NODE *) this, SSClass, Cond) == GS_BAD) return GS_BAD;
      }
   }
   else // La ricerca rigurada solo gli oggetti grafici del gruppo di selezione "pInputSS"
   {
      // Poichè il tempo di intersect tra il link set generato dalla 
      // "LinkSet.initSQLCond" e quello in input è molto grande 
      // (per 70000 oggetti grafici = 450 sec) si è deciso di non utilizzare la tecnica
      // db->gph ma solo quella gph -> db.
      if (LinkSet.initSelSQLCond((C_NODE *) this, *pInputSS, Cond) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_CLASS::get_Key_SelSet                                    */
/*+                                                            
  Funzione che restituisce il valore chiave e il gruppo di oggetti 
  dell'oggetto grafico della classe.
  Parametri:
  ads_name Ent;      Oggetto grafico
  long     *Key;     Valore chiave
  C_SELSET &SelSet;  Gruppo di selezione
  int      What;     GRAPHICAL per gli oggetti grafici
                     DA_BLOCK  per i blocchi degli attributi visibili
                     ALL       per entrambi (default)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_CLASS::get_Key_SelSet(ads_name Ent, long *Key, C_SELSET &SelSet, int What)
{
   C_LINK     Link(Ent);
   C_LINK_SET LinkSS;

   if (Link.GetKey(Key) == GS_BAD) return GS_BAD;
   if (LinkSS.GetSS(id.code, id.sub_code, *Key, SelSet) == GS_BAD) return GS_BAD;
   if (SelSet.intersectType(What) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/******************************************************/
/*.doc gsc_conversion_from_line2polyline              */
/*+                                                            
  Funzione che trasforma una LINE o una CIRCLE in 
  POLYLINE, attenzione che il pedit non trasforma 
  la LINE ma ne crea una nuova e cancella la vecchia.

   Parametri:
   ads_name    line           line da convertire
   ads_name    new_entity     line convertita

  Restituisce GS_GOOD in caso di successo altrimenti 
  restituisce GS_BAD. 
-*/  
/******************************************************/
int gsc_AcadObjToPline(ads_name eent, ads_name enew)
{
   ads_name elast;
   int      result;
  
   // Memorizzo l'ultima entità nel disegno
   if (acdbEntLast(elast) != RTNORM) return GS_BAD; 
   if (gsc_AcadObjToPline(eent) != GS_GOOD) return GS_BAD;
   // Memorizzo ora la nuova entità
   if (acdbEntLast(enew) != RTNORM) return GS_BAD; 

   result = GS_BAD;
   if (!ads_name_equal(elast, enew))
      result = GS_GOOD;
   else
      ads_name_set(eent, enew);

   return result;
}


/*********************************************************/
/*.doc (new 2) gsc_ssadd <extern>                        */
/*+
-*/
/*********************************************************/
int gsc_ssadd(ads_name ss1, ads_name ss2, ads_name ss3)
{
   long     i,len;
   ads_name ent;

   if (!ss1 || !ss2 || !ss3) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (ads_sslength(ss1, &len) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   for (i = 0; i < len; i++)
   {
      if (acedSSName(ss1, i, ent) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      if (acedSSAdd(ent, ss2, ss3) != RTNORM)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_select_pick <extern> */
/*+
   Parametri:
   ads_name sss;     
-*/
/*********************************************************/
int gsc_select_pick(ads_name sss)
{
   int      ret;
   ads_name temp;

   // Ripulisce gruppo di selezione AUTOCAD
   gsc_clear_pick_first();
   ads_name_clear(temp);
   if ((ret = acedSSGet(NULL, NULL, NULL, NULL, temp)) != RTNORM) return ret; 
       
   if (ads_name_nil(temp)) return GS_CAN;
  
   if (gsc_ssadd(temp, sss, sss) == GS_BAD) { ads_ssfree(temp); return GS_BAD; }
   ads_ssfree(temp);

   // Ripulisce gruppo di selezione AUTOCAD
   gsc_clear_pick_first();

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_clear_pick_first <extern> */
/*+
-*/
/*********************************************************/
int gsc_clear_pick_first()
{
   ads_name tmp;

   ads_name_clear(tmp);
   if (acedSSSetFirst(tmp, tmp) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_pick_first                       <extern> */
/*+
-*/
/*********************************************************/
int gsc_get_pick_first(C_SELSET &SelSet)
{
   presbuf GripSet = NULL, PickSet = NULL;
   
   if (acedSSGetFirst(&GripSet, &PickSet) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   if (GripSet)
   {
      if (GripSet->restype == RTPICKS) acedSSFree(GripSet->resval.rlname);
      acutRelRb(GripSet);
   }

   if (PickSet)
   {
      if (PickSet->restype == RTPICKS) SelSet << PickSet->resval.rlname;
      acutRelRb(PickSet);
   }
   else SelSet.clear();

   return GS_GOOD;
}


/*******************************************************/
/*.doc         gsc_AcadObjToPline                      */
/*+                                           
   Funzione base che che converte un oggetto grafico in LWPOLILINE.
   Gestisce solo: LINE, ARC e POLYLINE (se tutte le Z sono uguali).
   Parametri:
   ad_name ent;   Oggetto grafico

   Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
   
   N.B.: Questa funzione non effettua riallineamento GEosim
-*/  
/*******************************************************/
int gsc_AcadObjToPline(ads_name ent)
{
   TCHAR    type[MAX_LEN_GRAPH_TYPE];
   C_EED    eed;
   bool     SetEED = false, SetOD = false;
   ads_name NewEnt;
   C_STRING ODTableName;
   C_CLASS  *pCls;
   long     IdInternal;

   // Ricavo le caratteristiche grafiche dell' oggetto
   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;

   // Leggo l'entità estesa di GEOsim se esistente
   if (eed.load(ent) == GS_GOOD)
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      if (!(pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)))
         return GS_BAD;

      SetEED = true;
      
      // Leggo i dato OD se si tratta di oggetto GEOsim collegato a DB
      if (pCls->get_category() != CAT_SPAGHETTI)
      {
         gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(),
                            eed.cls, eed.sub, ODTableName);
         if (gsc_getIDfromODTable(ent, ODTableName, &IdInternal) == GS_BAD)
            return GS_BAD;
         SetOD = true;
      }
   }

   // Se si tratta di linea oppure arco
   if (gsc_strcmp(type, _T("LINE")) == 0 || gsc_strcmp(type, _T("ARC")) == 0)
   {
      // Il comando edit porta le entità estese ma non gli OD nella nuova entità
      if (gsc_callCmd(_T("_.PEDIT"), RTENAME, ent, RTSTR, _T("_Y"), RTSTR, GS_EMPTYSTR, 0) != RTNORM)
         return GS_BAD;
      SetEED = false;
   }
   else // Se si tratta di polilinea
   if (gsc_strcmp(type, _T("POLYLINE")) == 0)
   {
   }

   if (SetEED) // Aggiungo l'entità estesa
      eed.save(NewEnt);      

   if (SetOD) // Aggiungo gli OD
      gsc_setID2ODTable(NewEnt, ODTableName, IdInternal);

   return GS_GOOD;
}


/*******************************************************/
/*.doc gs_AcadObjToPline                               */
/*+                                           
   Funzione LISP che che converte una LINE oppure un
   ARC di AutoCAD in una POLYLINE.

   Riceve come parametro un gruppo di selezione.

   Restituisce GS_GOOD se tutto OK altrimenti 
   restituisce GS_BAD.
-*/  
/*******************************************************/
int gs_AcadObjToPline()
{
   presbuf  arg;
   ads_name ent;
   long     index = 0, qty = 0, LenSelSet;
   
   // Leggo i parametri da lisp
   acedRetNil();
   arg = acedGetArgs();

   // Il primo ed unico parametro è il gruppo di selezione da 
   // controllare per convertire gli oggetti.
   if (arg == NULL || arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   gsc_enable_reactors();

   if (acedSSLength(arg->resval.rlname, &LenSelSet) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1062)); // "Elaborazione entità"
   StatusBarProgressMeter.Init(LenSelSet);

   while (acedSSName(arg->resval.rlname, index++, ent) == RTNORM)
   {
      StatusBarProgressMeter.Set(index);

      if (gsc_AcadObjToPline(ent) == GS_GOOD) qty++;
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   acedRetReal(qty);
   gsc_disable_reactors();

   // controllo se ci sono oggetti da riallineare
   gsc_align_data_base();

   return RTNORM;
}


/*******************************************************/
/*.doc gsc_CopyXdata                                   */
/*+                                           
  Funzione che copia la lista dei dati estesi da un oggetto ad un altro.
  Parametri:
  ads_name Src;         oggetto di autocad sorgente
  ads_name Dst;         oggetto di autocad destinazione

  Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/*******************************************************/
int gsc_CopyXdata(ads_name Src, ads_name Dst)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;
   int          res = GS_GOOD;
   
   if (acdbGetObjectId(objId, Src) == Acad::eOk)
      if (acdbOpenObject(pObj, objId, AcDb::kForRead) == Acad::eOk)
      {
         C_RB_LIST lst;

         lst << pObj->xData(NULL);
         pObj->close();
         
         if (lst.GetCount() > 0)
            if (acdbGetObjectId(objId, Dst) == Acad::eOk)
               if (acdbOpenObject(pObj, objId, AcDb::kForWrite) == Acad::eOk)
               {
                  if (pObj->setXData(lst.get_head()) != Acad::eOk) res = GS_BAD;
                  pObj->close();
               }
      }

   return res;
}


/*******************************************************/
/*.doc gsc_AcadObjToBlock                              */
/*+                                           
  Funzione che converte un oggetto POINT, TEXT, MTEXT 
  in uno INSERT (l'oggetto grafico originale viene cancellato).
  Parametri:
  ads_name ent;          oggetto di autocad
  const TCHAR *BlkName;  nome di blocco

  Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/*******************************************************/
int gsc_AcadObjToBlock(ads_name ent, const TCHAR *BlkName)
{
   TCHAR      type[MAX_LEN_GRAPH_TYPE];
   ads_point InsPt;
   ads_name  eLast;

   // Ricavo il tipo di oggetto
   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;
   if (gsc_strcmp(type, _T("POINT")) == 0)
   {
      if (gsc_get_firstPoint(ent, InsPt) == GS_BAD) return GS_BAD;
      if (gsc_insert_block(BlkName, InsPt) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(type, _T("TEXT")) == 0 || gsc_strcmp(type, _T("MTEXT")) == 0)
   {
      double rot;

      if (gsc_get_firstPoint(ent, InsPt) == GS_BAD) return GS_BAD;
      if (gsc_get_rotation(ent, &rot) == GS_BAD) return GS_BAD;
      if (gsc_insert_block(BlkName, InsPt, gsc_rad2grd(rot)) == GS_BAD) return GS_BAD;
   }
   else
      return GS_BAD;

   if (acdbEntLast(eLast) != RTNORM) return GS_BAD; 

   if (gsc_CopyXdata(ent, eLast) == GS_BAD)
      { gsc_EraseEnt(eLast); return GS_BAD; }
   
   return gsc_EraseEnt(ent);
}


/*******************************************************/
/*.doc gs_AcadObjToBlock                               */
/*+                                           
  Funzione LISP che converte un oggetto POINT, TEXT, MTEXT 
  in uno INSERT (l'oggetto grafico originale viene cancellato).
  Riceve come parametro un gruppo di selezione e il nome di un blocco.

   Restituisce il numero di oggetti convertiti.
-*/  
/*******************************************************/
int gs_AcadObjToBlock(void)
{
   presbuf  arg, pRbSelSet;
   ads_name ent;
   long     index = 0, qty = 0, LenSelSet;
   
   // Leggo i parametri da lisp
   acedRetNil();
   arg = acedGetArgs();

   // Il primo parametro è il gruppo di selezione da 
   // controllare per convertire gli oggetti.
   if (arg == NULL || arg->restype != RTPICKS)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pRbSelSet = arg;
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
       { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (acedSSLength(pRbSelSet->resval.rlname, &LenSelSet) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1062)); // "Elaborazione entità"
   StatusBarProgressMeter.Init(LenSelSet);

   while (acedSSName(pRbSelSet->resval.rlname, index++, ent) == RTNORM)
   {
      StatusBarProgressMeter.Set(index);

      if (gsc_AcadObjToBlock(ent, arg->resval.rstring) == GS_GOOD) qty++;
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."      
   acedRetReal(qty);

   return RTNORM;
}


/*******************************************************/
/*.doc gsc_AcadObjToText                               */
/*+                                           
  Funzione che converte un oggetto POINT, INSERT, MTEXT, LWPOLYLINE 
  in uno TEXT (l'oggetto grafico originale viene cancellato).
  Parametri:
  ads_name ent;         oggetto di autocad

  Restituisce GS_GOOD se tutto OK altrimenti restituisce GS_BAD.
-*/  
/*******************************************************/
int gsc_AcadObjToText(ads_name ent)
{
   TCHAR     type[MAX_LEN_GRAPH_TYPE];
   ads_point InsPt;
   ads_name  eLast;
   double    rot;

   // Ricavo il tipo di oggetto
   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;
   if (gsc_strcmp(type, _T("POINT")) == 0)
   {
      if (gsc_get_firstPoint(ent, InsPt) == GS_BAD) return GS_BAD;
      if (gsc_insert_text(_T("?"), InsPt, 1) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(type, _T("INSERT")) == 0)
   {
      if (gsc_get_firstPoint(ent, InsPt) == GS_BAD) return GS_BAD;
      if (gsc_get_rotation(ent, &rot) == GS_BAD) return GS_BAD;
      if (gsc_insert_text(_T("?"), InsPt, 1, gsc_rad2grd(rot)) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(type, _T("MTEXT")) == 0)
   {
      C_STRING Txt;
      double   htext;

      if (gsc_get_firstPoint(ent, InsPt) == GS_BAD) return GS_BAD;
      // leggo valore testo e altezza testo
      if (gsc_getInfoText(ent, &Txt, NULL, &htext) == GS_BAD) return GS_BAD;
      if (gsc_get_rotation(ent, &rot) == GS_BAD) return GS_BAD;
      // per ora il testo non eredita tutto il resto...
      if (gsc_insert_text(Txt.get_name(), InsPt, htext, gsc_rad2grd(rot)) == GS_BAD) return GS_BAD;
   }
   else if (gsc_strcmp(type, _T("LWPOLYLINE")) == 0 || gsc_strcmp(type, _T("POLYLINE")) == 0 ||
            gsc_strcmp(type, _T("LINE")) == 0)
   {
      if (gsc_getPntRtzOnObj(ent, InsPt, &rot, _T("MML"), 0, 0, GS_GOOD) == GS_BAD)
         return GS_BAD;
      if (gsc_insert_text(_T("?"), InsPt, 1, gsc_rad2grd(rot)) == GS_BAD) return GS_BAD;
   }
   else
      return GS_BAD;

   if (acdbEntLast(eLast) != RTNORM) return GS_BAD; 

   if (gsc_CopyXdata(ent, eLast) == GS_BAD)
      { gsc_EraseEnt(eLast); return GS_BAD; }
   
   return gsc_EraseEnt(ent);
}


/*******************************************************/
/*.doc gs_AcadObjToText                                */
/*+                                           
  Funzione LISP che converte un oggetto POINT, INSERT, MTEXT 
  in uno TEXT (l'oggetto grafico originale viene cancellato).
  Riceve come parametro un gruppo di selezione .

  Restituisce il numero di oggetti convertiti.
-*/  
/*******************************************************/
int gs_AcadObjToText(void)
{
   presbuf  arg;
   ads_name ent;
   long     index = 0, qty = 0, LenSelSet;
   
   // Leggo i parametri da lisp
   acedRetNil();
   arg = acedGetArgs();

   // Il primo ed unico parametro è il gruppo di selezione da 
   // controllare per convertire gli oggetti.
   if (arg == NULL || arg->restype != RTPICKS)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (acedSSLength(arg->resval.rlname, &LenSelSet) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1062)); // "Elaborazione entità"
   StatusBarProgressMeter.Init(LenSelSet);

   while (acedSSName(arg->resval.rlname, index++, ent) == RTNORM)
   {
      StatusBarProgressMeter.Set(index);

      if (gsc_AcadObjToText(ent) == GS_GOOD) qty++;
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."      
   acedRetReal(qty);

   return RTNORM;
}


/*******************************************************/
/*.doc gsc_getOwnerAdsName                             */
/*+                                           
  Funzione restituisce il l'entità madre di quella passata come
  parametro.
  Parametri:
  ads_name Ent;         entità figlia (input)
  ads_name OwnerEnt;    entità madre (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*******************************************************/
int gsc_getOwnerAdsName(ads_name Ent, ads_name &OwnerEnt)
{ 
   AcDbObject   *pObj;
   AcDbObjectId objId;
   
   if (acdbGetObjectId(objId, Ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }

   // cerco entità madre
   if (acdbGetAdsName(OwnerEnt, pObj->ownerId()) != Acad::eOk)
      { pObj->close(); return GS_BAD; }

   if (pObj->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}


/***************************************************************************/
/*.doc gsc_modifyEntToFas(ads_name Group, C_FAS *pFas, int Flag) */
/*+                                                                       
  Funzione che cambia le proprietà grafiche di ogni entità di un gruppo 
  di selezione settandole come quelle indicate dall'oggetto C_FAS passato 
  come parametro (attraverso un puntatore).
  Flag specifica quali proprietà modificare. 
  Parametri:
  ads_name Group        Gruppo di selezione da elaborare.
  C_FAS    *pFas        Puntatore alla FAS dell' entità.
  long     Flag;        Flag per tipo di modifica.
  bool     AbsScale;    Flag se = TRUE la scala è impostata in valore assoluto
                        se = FALSE la scala e relativa alla scala precedente
                        (default = TRUE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************************/
int gsc_modifyEntToFas(C_SELSET &SelSet, C_FAS *pFas, long Flag, bool AbsScale)
{
   ads_name ss;
   SelSet.get_selection(ss);
   return gsc_modifyEntToFas(ss, pFas, Flag, AbsScale);
}
int gsc_modifyEntToFas(ads_name Group, C_FAS *pFas, long Flag, bool AbsScale)
{
   ads_name  ent;
   long      cont;

   if (!Group || ads_name_nil(Group) || ads_sslength(Group, &cont) != RTNORM ||
       cont == 0) return GS_GOOD;

   // Cambio del piano (per tutte le entità ad eccezione dei riempimenti)
   if (Flag & GSLayerSetting)     
   {
      // se non esiste il layer lo creo.
      if (gsc_crea_layer(pFas->layer) == GS_BAD) return GS_BAD;

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         if (gsc_ishatch(ent) != GS_GOOD) // se non è un riempimento
            gsc_setLayer(ent, pFas->layer);
   }

   // Cambio del blocco (solo per blocchi)
   if (Flag & GSBlockNameSetting)    
   {
      AcDbObjectId BlockId;
       
      if (gsc_getBlockId(pFas->block, BlockId) == GS_BAD) return GS_BAD;

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_block(ent, BlockId);
   }
   
   // Cambio lo stile del testo (solo per testi)
   if (Flag & GSTextStyleSetting)    
   {
      AcDbObjectId TextStyleId;
       
      if (gsc_getTextStyleId(pFas->style, TextStyleId) == GS_BAD) return GS_BAD;
      
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_textStyle(ent, TextStyleId);
   }

   // Cambio l' altezza del testo (solo per testi)
   if (Flag & GSTextHeightSetting) 
   {
      if (gsc_validhtext(pFas->h_text) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_hText(ent, pFas->h_text);
   }

   // Cambio la scala dell'entità (solo per blocchi)
   if (Flag & GSBlockScaleSetting) 
   {
      if (gsc_validscale(pFas->block_scale) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_scale(ent, pFas->block_scale, pFas->block_scale, pFas->block_scale,
                       AbsScale);
   }

   // Cambio il tipo di linea (solo per lightpolyline, polyline, circle, ellisse)
   if (Flag & GSLineTypeSetting) 
   {
      TCHAR name[MAX_LEN_LINETYPENAME];

      // controlla la validità, carica il tipolinea
      if (gsc_load_linetype(pFas->line) == GS_BAD) return GS_BAD;

      gsc_strcpy(name, pFas->line, MAX_LEN_LINETYPENAME);
      gsc_alltrim(name);                              
      // case insensitive
      if (gsc_strcmp(name, gsc_msg(23), FALSE) == 0) wcscpy(name, LINETYPE_BYLAYER); // "DALAYER"
      if (gsc_strcmp(name, gsc_msg(24), FALSE) == 0) wcscpy(name, LINETYPE_BYBLOCK); // "DABLOCCO"

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_lineType(ent, name);
   }

   // Cambio la larghezza (solo per polilinee)
   if (Flag & GSWidthSetting) 
   {
      if (gsc_validwidth(pFas->width) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_width(ent, pFas->width);
   }

   // Cambio il colore (per tutte le entità ad eccezione dei riempimenti)
   if (Flag & GSColorSetting)
   {
      if (pFas->color.getColorMethod() == C_COLOR::None)
         { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         if (gsc_ishatch(ent) != GS_GOOD) // se non è un riempimento
            gsc_set_color(ent, pFas->color);
   }

   // Cambio il nome oppure la scala oppure la rotazione dei riempimenti
   if (Flag & GSHatchNameSetting ||
       Flag & GSHatchScaleSetting || 
       Flag & GSHatchRotationSetting)
   {
      TCHAR  *pHatchName = NULL;
      double *pHatchScale = NULL, *pHatchRot = NULL;

      if (Flag & GSHatchNameSetting)
         if (gsc_validhatch(pFas->hatch) == GS_BAD) return GS_BAD;
            pHatchName = pFas->hatch;

      if (Flag & GSHatchScaleSetting)
         if (gsc_validscale(pFas->hatch_scale) == GS_BAD) return GS_BAD;
            pHatchScale = &(pFas->hatch_scale);

      if (Flag & GSHatchRotationSetting)
         if (gsc_validrotation(pFas->hatch_rotation) == GS_BAD) return GS_BAD;
            pHatchRot = &(pFas->hatch_rotation);

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         if (gsc_ishatch(ent) == GS_GOOD) // se è un riempimento
            gsc_set_hatch(ent, pHatchName, pHatchRot, pHatchScale);
   }

   // Cambio il piano dei riempimenti
   if (Flag & GSHatchLayerSetting)
   {
      // se non esiste il layer lo creo.
      if (gsc_crea_layer(pFas->hatch_layer) == GS_BAD) return GS_BAD;

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         if (gsc_ishatch(ent) == GS_GOOD) // se è un riempimento
            gsc_setLayer(ent, pFas->hatch_layer);
   }

   // Cambio il colore dei riempimenti
   if (Flag & GSHatchColorSetting)
   {
      if (pFas->hatch_color.getColorMethod() == C_COLOR::None)
         { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         if (gsc_ishatch(ent) == GS_GOOD) // se è un riempimento
            gsc_set_color(ent, pFas->hatch_color);
   }

   // Cambio il thickness (solo per polilinee e testo)
   if (Flag & GSThicknessSetting)
   {
      if (gsc_validthickness(pFas->thickness) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_thickness(ent, pFas->thickness);
   }

   // Cambio l'elevazione 
   if (Flag & GSElevationSetting)
   {
      if (gsc_validelevation(pFas->elevation) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_elevation(ent, pFas->elevation);
   }

   // Cambio la rotazione (in gradi)
   if (Flag & GSRotationSetting)
   {
      double RotRadian; // rotazione in radianti

      if (gsc_validrotation(pFas->rotation) == GS_BAD) return GS_BAD;
      RotRadian = gsc_grd2rad(pFas->rotation);
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         // gli attributi dei blocchi non saranno ruotati
         gsc_set_rotation(ent, RotRadian, GS_BAD, false);
   }

   // Cambio la scala del tipo di linea (solo per polilinee e cerchi)
   if (Flag & GSLineTypeScaleSetting) 
   {
      if (gsc_validscale(pFas->line_scale) == GS_BAD) return GS_BAD;
      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_lineTypeScale(ent, pFas->line_scale);
   }

   // Cambio lo stile di quotatura (solo per quotature)
   if (Flag & GSDimensionStyleSetting)    
   {
      AcDbObjectId DimStyleId;
       
      if (gsc_getDimStyleId(pFas->dimension_style.get_name(), DimStyleId) == GS_BAD)
         return GS_BAD;

      cont = 0;
      while (acedSSName(Group, cont++, ent) == RTNORM)
         gsc_set_dimStyle(ent, DimStyleId);
   }

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_SetBitForChangeFas <external>
/*+                                                            
  Funzione che restituisce il flag che determina la combinazione di bit
  relativa alle proprietà della Fas che possono essere cambiate.

   Type                     Pol.  Text  Node  Surf. Grid  Compl Group Spagh.

   1    piano              |  *  |  *  |  *  |  *  |     |     |     |  *  | 
   -------------------------------------------------------------------------
   2    blocco             |     |     |  *  |     |     |     |     |     |
   -------------------------------------------------------------------------
   4    stile testo        |     |  *  |     |     |     |     |     |  *  |
   -------------------------------------------------------------------------
   8    scala              |     |  *  |  *  |     |     |     |     |     |
   -------------------------------------------------------------------------
   16   tipo di linea      |  *  |     |     |  *  |     |     |     |  *  |
   -------------------------------------------------------------------------
   32   larghezza polilinea|  *  |     |     |  *  |     |     |     |  *  |
   -------------------------------------------------------------------------
   64   colore             |  *  |  *  |     |  *  |     |     |     |  *  |
   -------------------------------------------------------------------------
   128  altezza del testo  |     |  *  |     |     |     |     |     |  *  |
   -------------------------------------------------------------------------
   256  riempimento        |     |     |     |  *  |     |     |     |     |
   -------------------------------------------------------------------------
   512  thickness          |  *  |  *  |     |  *  |     |     |     |  *  |
   -------------------------------------------------------------------------
   1024 elevazione         |  *  |  *  |  *  |  *  |     |     |     |  *  |
   -------------------------------------------------------------------------
   2048 rotazione          |     |  *  |  *  |     |     |     |     |     |
    
  Parametri:
  int  Category;   Categoria classe.
  int  Type;       Tipo classe.
  long *BitForFAS; Numero di codifica per cambiamento FAS.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int gsc_SetBitForChangeFas(int Category, int Type, long *BitForFAS)
{
   switch (Category)
   {
      case CAT_SIMPLEX:
      case CAT_SUBCLASS:
         switch (Type)
         {
            case TYPE_POLYLINE:
               *BitForFAS = GSLayerSetting + GSLineTypeSetting + GSWidthSetting + 
                            GSColorSetting + GSThicknessSetting + GSElevationSetting;   
               break;
            case TYPE_TEXT:
               *BitForFAS = GSLayerSetting + GSTextStyleSetting + GSBlockScaleSetting + 
                            GSColorSetting + GSTextHeightSetting + GSThicknessSetting + 
                            GSElevationSetting + GSRotationSetting;
               break;
            case TYPE_NODE:
               *BitForFAS = GSLayerSetting + GSBlockNameSetting + GSBlockScaleSetting + 
                            GSElevationSetting + GSRotationSetting;
               break;
            case TYPE_SURFACE:
               *BitForFAS = GSLayerSetting + GSLineTypeSetting  + GSWidthSetting + 
                            GSColorSetting + GSHatchNameSetting + GSThicknessSetting +
                            GSElevationSetting;
               break;
            default:
               GS_ERR_COD = eGSInvClassType;
               return GS_BAD;
         }
         break;
      case CAT_GROUP:
         if (Type == TYPE_GROUP) *BitForFAS = 0;
         else { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
         break;
      case CAT_GRID:
         if (Type == TYPE_GRID) *BitForFAS = 0;
         else { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
         break;
      case CAT_SPAGHETTI:
         if (Type == TYPE_SPAGHETTI) 
            *BitForFAS = GSLayerSetting + GSTextStyleSetting + GSLineTypeSetting + 
                         GSWidthSetting + GSColorSetting + GSTextHeightSetting +
                         GSThicknessSetting + GSElevationSetting;
         else 
            { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
         break;
      default:
         { GS_ERR_COD = eGSInvCategoryType; return GS_BAD; }
   }

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_getBufferOnArc                              <external> */
/*+                                                            
  Funzione che restituisce un oggetto di tipo polilinea che rappresenta 
  un buffer attorno ad un arco.
  Parametri:
  AcGePoint3d   Pt1;
  AcGePoint3d   Pt2;
  double        Bulge;        1/4 tang angolo inscritto (negativo se l'arco 
                              va in senso orario)
  double        OffSet;       Offset del buffer
  AcDbPolyline  **pExtBuffer; Oggetto di tipo polilinea che descrive
                              il buffer esterno all'arco (viene allocato).
  AcDbPolyline **pIntBuffer;  Oggetto di tipo polilinea che descrive
                              il buffer interno all'arco (viene allocato),
                              se = NULL vuol dire che non esiste.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int gsc_getBufferOnArc(AcGePoint3d Pt1, AcGePoint3d Pt2, double Bulge, double OffSet,
                       AcDbPolyline **pExtBuffer, AcDbPolyline **pIntBuffer)
{
   double      Radius, Angle, _Bulge, ArcAngle, DistInitFinalPts, AngleInitPt;
   double      DistFormMiddlePt, HalfCord;
   ads_point   InitPt, FinalPt, Center, _pt1, _pt2, _pt3, _pt4, IntersPt, MiddlePt;
   AcGePoint2d pt;

   *pExtBuffer = new AcDbPolyline();
   (*pExtBuffer)->setClosed(Adesk::kTrue);

   *pIntBuffer = NULL;

   ArcAngle = atan(Bulge) * 4;
   if (Bulge < 0) ArcAngle *= -1;

   ads_point_set_from_AcGePoint3d(Pt1, InitPt);
   ads_point_set_from_AcGePoint3d(Pt2, FinalPt);

   gsc_getArcRayCenter(InitPt, FinalPt, ArcAngle, &Radius, Center);

   AngleInitPt = acutAngle(Center, InitPt);
   acutPolar(InitPt, AngleInitPt, OffSet, _pt1);
   acutPolar(InitPt, AngleInitPt, -1 * OffSet, _pt4);

   Angle = acutAngle(FinalPt, Center);
   acutPolar(FinalPt, Angle, -1 * OffSet, _pt2);
   acutPolar(FinalPt, Angle, OffSet, _pt3);

   // arco esterno
   pt.set(_pt1[X], _pt1[Y]);
   (*pExtBuffer)->addVertexAt(0, pt);
   (*pExtBuffer)->setBulgeAt(0, Bulge);
   pt.set(_pt2[X], _pt2[Y]);
   (*pExtBuffer)->addVertexAt(1, pt);

   if (ArcAngle < PI) // se arco con angolo interno < 180 gradi
   {
      if (OffSet >= Radius) // se OffSet > raggio arco
      {
         // esiste una intersezione tra gli archi di offset 
         // alle estremità dell'arco
   
         // punto medio della corda tra gli estremi dell'arco
         gsc_getMid2DLine(InitPt, FinalPt, MiddlePt);
         // lunghezza di mezza corda
         HalfCord = gsc_dist(InitPt, MiddlePt);
         // Calcolo distanza dal punto medio della corda con pitagora
         DistFormMiddlePt = pow((OffSet * OffSet) - (HalfCord * HalfCord), (double) 1/2); // radice quadrata
         Angle = acutAngle(MiddlePt, Center);
         // Calcolo punto di intersezione
         acutPolar(MiddlePt, Angle, DistFormMiddlePt, IntersPt);

         Angle = acutAngle(InitPt, IntersPt);
         _Bulge = tan((2 * PI - fabs(AngleInitPt - Angle)) / 4);
         if (Bulge < 0) _Bulge *= -1; // arco in senso orario

         (*pExtBuffer)->setBulgeAt(1, _Bulge);
         pt.set(IntersPt[X], IntersPt[Y]);
         (*pExtBuffer)->addVertexAt(2, pt);
         (*pExtBuffer)->setBulgeAt(2, _Bulge);
      }
      else
      {
         // non esiste una intersezione tra gli archi di offset
         // alle estremità dell'arco
         (*pExtBuffer)->setBulgeAt(1, (Bulge < 0) ? -1 : 1);
         pt.set(_pt3[X], _pt3[Y]);
         (*pExtBuffer)->addVertexAt(2, pt);
         (*pExtBuffer)->setBulgeAt(2, -1 * Bulge);
         pt.set(_pt4[X], _pt4[Y]);
         (*pExtBuffer)->addVertexAt(3, pt);
         (*pExtBuffer)->setBulgeAt(3, (Bulge < 0) ? -1 : 1);
      }
   }
   else // se arco con angolo interno >= 180 gradi
      // se la distanza tra gli estremi dell'arco <= OffSet
      if ((DistInitFinalPts = gsc_dist(InitPt, FinalPt)) <= 2 * OffSet)
      {
         // esiste una intersezione tra gli archi di offset
         // alle estremità dell'arco per l'offset esterno

         // punto medio della corda tra gli estremi dell'arco
         gsc_getMid2DLine(InitPt, FinalPt, MiddlePt);
         // lunghezza di mezza corda
         HalfCord = gsc_dist(InitPt, MiddlePt);
         // Calcolo distanza dal punto medio della corda con pitagora
         DistFormMiddlePt = pow((OffSet * OffSet) - (HalfCord * HalfCord), (double) 1/2); // radice quadrata
         if (2 * OffSet == DistFormMiddlePt)
         {
            _Bulge = 1;
            ads_point_set(MiddlePt, IntersPt);
         }
         else
         {
            Angle = acutAngle(Center, MiddlePt);
            // Calcolo punto di intersezione
            acutPolar(MiddlePt, Angle, DistFormMiddlePt, IntersPt);

            Angle = acutAngle(InitPt, IntersPt);

            // Calcolo l'angolo interno
            if (AngleInitPt < Angle)
               Angle = (AngleInitPt + 2 * PI)- Angle;
            else
               Angle = AngleInitPt - Angle;

            _Bulge = tan(Angle / 4);
            if (Bulge < 0) _Bulge *= -1; // arco in senso orario
         }
         if (Bulge < 0) _Bulge *= -1; // arco in senso orario

         (*pExtBuffer)->setBulgeAt(1, _Bulge);
         pt.set(IntersPt[X], IntersPt[Y]);
         (*pExtBuffer)->addVertexAt(2, pt);
         (*pExtBuffer)->setBulgeAt(2, _Bulge);

         if (OffSet < Radius) // se OffSet < raggio arco
         {  // esiste un'intersezione interna
            *pIntBuffer = new AcDbPolyline();
            (*pIntBuffer)->setClosed(Adesk::kTrue);

            pt.set(_pt4[X], _pt4[Y]);
            (*pIntBuffer)->addVertexAt(0, pt);
            (*pIntBuffer)->setBulgeAt(0, Bulge);
            pt.set(_pt3[X], _pt3[Y]);
            (*pIntBuffer)->addVertexAt(1, pt);

            Angle = acutAngle(MiddlePt, Center);
            // Calcolo punto di intersezione
            acutPolar(MiddlePt, Angle, DistFormMiddlePt, IntersPt);

            AngleInitPt = acutAngle(InitPt, Center);
            Angle       = acutAngle(InitPt, IntersPt);

            // Calcolo l'angolo interno
            if (Angle < AngleInitPt)
               Angle = (Angle + 2 * PI)- AngleInitPt;
            else
               Angle = Angle - AngleInitPt;

            _Bulge = tan(Angle / 4);
            
            // Se prima era in senso antiorario ora va in senso antiorario
            if (Bulge > 0) _Bulge *= -1;

            (*pIntBuffer)->setBulgeAt(1, _Bulge);
            pt.set(IntersPt[X], IntersPt[Y]);
            (*pIntBuffer)->addVertexAt(2, pt);
            (*pIntBuffer)->setBulgeAt(2, _Bulge);
         }
      }
      else
      {
         // non esiste una intersezione tra gli archi di offset
         // alle estremità dell'arco
         (*pExtBuffer)->setBulgeAt(1, (Bulge < 0) ? -1 : 1);
         pt.set(_pt3[X], _pt3[Y]);
         (*pExtBuffer)->addVertexAt(2, pt);
         (*pExtBuffer)->setBulgeAt(2, -1 * Bulge);
         pt.set(_pt4[X], _pt4[Y]);
         (*pExtBuffer)->addVertexAt(3, pt);
         (*pExtBuffer)->setBulgeAt(3, (Bulge < 0) ? -1 : 1);
      }
  
   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_getBufferOnLine                             <external> */
/*+                                                            
  Funzione che restituisce un oggetto di tipo polilinea che rappresenta 
  un buffer attorno ad una linea.
  Parametri:
  AcGePoint3d  Pt1;
  AcGePoint3d  Pt2;
  double       OffSet;    Offset del buffer
  AcDbPolyline **pBuffer; Oggetto di tipo polilinea che descrive
                          il buffer (viene allocato).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. : Alloca memoria
-*/  
/******************************************************************/
int gsc_getBufferOnLine(AcGePoint3d Pt1, AcGePoint3d Pt2,
                       double OffSet, AcDbPolyline **pBuffer)
{
   ads_point InitPt, FinalPt, _pt1, _pt2, _pt3, _pt4;
   double    Angle;

   ads_point_set_from_AcGePoint3d(Pt1, InitPt);
   ads_point_set_from_AcGePoint3d(Pt2, FinalPt);

   Angle = acutAngle(InitPt, FinalPt);
   acutPolar(InitPt,  Angle + (PI / 2), OffSet, _pt1);
   acutPolar(FinalPt, Angle + (PI / 2), OffSet, _pt2);
   acutPolar(FinalPt, Angle - (PI / 2), OffSet, _pt3);
   acutPolar(InitPt,  Angle - (PI / 2), OffSet, _pt4);

   // gli archi sono in senso orario bulge vale -1

   *pBuffer = new AcDbPolyline(4);
   AcGePoint2d  pt;

   pt.set(_pt1[X], _pt1[Y]);
   (*pBuffer)->addVertexAt(0, pt);
   pt.set(_pt2[X], _pt2[Y]);
   (*pBuffer)->addVertexAt(1, pt);
   (*pBuffer)->setBulgeAt(1, -1);
   pt.set(_pt3[X], _pt3[Y]);
   (*pBuffer)->addVertexAt(2, pt);
   pt.set(_pt4[X], _pt4[Y]);
   (*pBuffer)->addVertexAt(3, pt);
   (*pBuffer)->setBulgeAt(3, -1);

   (*pBuffer)->setClosed(Adesk::kTrue);

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_getBufferOnVertexEnt                        <external> */
/*+                                                            
  Funzione che restituisce un oggetto di tipo polilinea che rappresenta 
  un buffer attorno ad tratto di un oggetto lineare (no spline).
  Parametri:
  AcDbEntity   *pEnt;         Entità su cui calcolare il buffer
  double       OffSet;        Offset del buffer
  int          nVertex;       Numero del vertice iniziale (0-based)
  AcDbPolyline **pExtBuffer;  Oggetto di tipo polilinea che descrive
                              il buffer per il tratto esterno che va dal
                              vertice n-esimo al successivo (viene allocato).
  AcDbPolyline **pIntBuffer;  Oggetto di tipo polilinea che descrive un eventuale
                              buffer per il tratto interno che va dal
                              vertice n-esimo al successivo nel caso di archi
                              (viene allocato), potrebbe essere = NULL.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. : Alloca memoria
-*/  
/******************************************************************/
int gsc_getBufferOnVertexEnt(AcDbEntity *pEnt, double OffSet, int nVertex,
                             AcDbPolyline **pExtBuffer, AcDbPolyline **pIntBuffer)
{
   AcGePoint3d Position, NextPosition;
   double      Bulge, NextBulge;
   int         iVertex = 0, res = GS_BAD;

   if (OffSet <= 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (pEnt->isKindOf(AcDb2dPolyline::desc()))
   {  //  Group entity
      AcDbObjectIterator *pVertIter;
      AcDb2dVertex       *pVertex;
      AcDbObjectId       vertexObjId;
      bool               Spline;

      if (((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dQuadSplinePoly ||
          ((AcDb2dPolyline *) pEnt)->polyType() == AcDb::k2dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb2dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         Position = pVertex->position();
         Bulge = pVertex->bulge();
         pVertex->close();
         break;
      }
      pVertIter->step();

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k2dSplineCtlVertex ||
                pVertex->vertexType() == AcDb::k2dSplineFitVertex)
               { pVertex->close(); continue; }

         NextPosition = pVertex->position();
         NextBulge    = pVertex->bulge();
         pVertex->close();

         if (iVertex == nVertex)
         {
            res = GS_GOOD;
            break;
         }

         Bulge    = NextBulge;
         Position = NextPosition;
         iVertex++;
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc()))
   {  //  Group entity
      AcDbObjectIterator   *pVertIter;
      AcDb3dPolylineVertex *pVertex;
      AcDbObjectId         vertexObjId;
      bool                 Spline;

      if (((AcDb3dPolyline *) pEnt)->polyType() == AcDb::k3dQuadSplinePoly ||
          ((AcDb3dPolyline *) pEnt)->polyType() == AcDb::k3dCubicSplinePoly)
         Spline = true;
      else
         Spline = false;

      pVertIter = ((AcDb3dPolyline *) pEnt)->vertexIterator();

      // Leggo primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k3dFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k3dControlVertex ||
                pVertex->vertexType() == AcDb::k3dFitVertex)
               { pVertex->close(); continue; }

         Position = pVertex->position();
         pVertex->close();
         break;
      }
      pVertIter->step();
      Bulge = 0;

      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo prossimo vertice
         vertexObjId = pVertIter->objectId();
         acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead);

         if (Spline)
         {
            if (pVertex->vertexType() != AcDb::k3dFitVertex)
               { pVertex->close(); continue; }
         }
         else
            if (pVertex->vertexType() == AcDb::k3dControlVertex ||
                pVertex->vertexType() == AcDb::k3dFitVertex)
               { pVertex->close(); continue; }

         NextPosition = pVertex->position();
         pVertex->close();

         if (iVertex == nVertex)
         {
            res = GS_GOOD;
            break;
         }

         Position = NextPosition;
         iVertex++;
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      ads_point center, pt;
      double    Angle;

      if (nVertex > 0) { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

      ads_point_set_from_AcGePoint3d(((AcDbArc *) pEnt)->center(), center);
      acutPolar(center, ((AcDbArc *) pEnt)->startAngle(), ((AcDbArc *) pEnt)->radius(), pt);
      Position.set(pt[X], pt[Y], pt[Z]);
      acutPolar(center, ((AcDbArc *) pEnt)->endAngle(), ((AcDbArc *) pEnt)->radius(), pt);
      NextPosition.set(pt[X], pt[Y], pt[Z]);

      // Calcolo l'angolo interno
      if (((AcDbArc *) pEnt)->endAngle() < ((AcDbArc *) pEnt)->startAngle())
         Angle = (((AcDbArc *) pEnt)->endAngle() + 2 * PI)- ((AcDbArc *) pEnt)->startAngle();
      else
         Angle = ((AcDbArc *) pEnt)->endAngle() - ((AcDbArc *) pEnt)->startAngle();

      Bulge = tan(Angle / 4);

      res = GS_GOOD;
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      if (nVertex > 0) { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

      Position     = ((AcDbLine *) pEnt)->startPoint();
      NextPosition = ((AcDbLine *) pEnt)->endPoint();
      Bulge        = 0.0;

      res = GS_GOOD;
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      int NumVerts = ((AcDbPolyline *) pEnt)->numVerts();

      ((AcDbPolyline *) pEnt)->getPointAt(0, Position);
      ((AcDbPolyline *) pEnt)->getBulgeAt(0, Bulge);       

      for (int i = 1; i < NumVerts; i++)
      {
         ((AcDbPolyline *) pEnt)->getPointAt(i, NextPosition);
         ((AcDbPolyline *) pEnt)->getBulgeAt(i, NextBulge);       

         if (i - 1 == nVertex)
         {
            res = GS_GOOD;
            break;
         }

         Bulge    = NextBulge;
         Position = NextPosition;
      }
   }
   else
   if (pEnt->isKindOf(AcDbMline::desc()))
   {
      int NumVerts = ((AcDbMline *) pEnt)->numVertices();

      Position = ((AcDbMline *) pEnt)->vertexAt(0);

      for (int i = 1; i < NumVerts; i++)
      {
         NextPosition = ((AcDbMline *) pEnt)->vertexAt(i);

         if (i - 1 == nVertex)
         {
            res = GS_GOOD;
            break;
         }

         Position = NextPosition;
      }
   }
   else
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   if (res == GS_GOOD)
      if (Bulge == 0) // segmento rettilineo
      {
         res = gsc_getBufferOnLine(Position, NextPosition, OffSet, pExtBuffer);
         *pIntBuffer = NULL;
      }
      else // arco
      {
         if (Bulge < 0)
         {
            AcGePoint3d dummy(Position);
            
            Position     = NextPosition;
            NextPosition = dummy;
            Bulge        *= -1;
         }

         res = gsc_getBufferOnArc(Position, NextPosition, Bulge, OffSet,
                                  pExtBuffer, pIntBuffer);
      }

   return res;
}


Acad::ErrorStatus getPointAtDist(const AcGeCurve2d *pGeCurve, double dist, AcGePoint2d& point)
{
   Acad::ErrorStatus es = Acad::eOk;
   AcGePoint2d       StartPt;
   double            pa, datumParam;

   if (!pGeCurve) return Acad::eInvalidInput;

   if (pGeCurve->hasStartPoint(StartPt) == false) return Acad::eInvalidInput;
   datumParam = pGeCurve->paramOf(StartPt);
   pa         = pGeCurve->paramAtLength(datumParam, dist, Adesk::kTrue);
   point      = pGeCurve->evalPoint(pa);

   return es;
}


bool gsc_isPaperSpaceCurrent(void)
{
   resbuf rb;

   // se ci si trova nello spazio carta
   if (acedGetVar(_T("CVPORT"), &rb) != RTNORM || rb.restype != RTSHORT)
      { GS_ERR_COD = eGSVarNotDef; return NULL; }
   return (rb.resval.rint == 1);
}


////////////////////////////////////////////////////////////////////
// test
////////////////////////////////////////////////////////////////////


#if defined(GSDEBUG) // se versione per debugging
#include "gs_filtr.h" 
#include "gs_grid.h" 
#include "gs_topo.h" 
#include "gs_netw.h" 
#include "gs_sql.h" 
#include "gs_evid.h" 
#include "gs_ade.h" 

#endif

void test1(long n)
{
   C_LONG_BTREE Tree;
   long         num;

   num = 80;
   Tree.add(&num);
   num = 30;
   Tree.add(&num);
   num = 130;
   Tree.add(&num);
   num = 20;
   Tree.add(&num);
   num = 50;
   Tree.add(&num);
   num = 110;
   Tree.add(&num);
   num = 160;
   Tree.add(&num);
   num = 10;
   Tree.add(&num);
   num = 40;
   Tree.add(&num);
   num = 60;
   Tree.add(&num);
   num = 90;
   Tree.add(&num);
   num = 120;
   Tree.add(&num);
   num = 140;
   Tree.add(&num);
   num = 190;
   Tree.add(&num);
   num = 70;
   Tree.add(&num);
   num = 100;
   Tree.add(&num);
   num = 119;
   Tree.add(&num);
   num = 150;
   Tree.add(&num);
   num = 170;
   Tree.add(&num);
   num = 200;
   Tree.add(&num);
   num = 180;
   Tree.add(&num);
   num = 145;
   Tree.add(&num);

   Tree.print();
   ads_printf(_T("\n----------------------------------"));

   while (Tree.get_count())
   {
      ads_printf(_T("\n----------------------------------"));
      //acedEntSel(GS_EMPTYSTR, ent, point);
      Tree.go_bottom();
      if (((C_BLONG *) Tree.get_cursor())->get_key() == 50)
         ads_printf(_T("\n-----------  O  R  A  -----------"));
      Tree.remove_at();
      Tree.print();
   }
}

int test()
{
#if defined(GSDEBUG) // se versione per debugging
   C_TOPOLOGY Topo;
   C_2STR_INT      *pCostSQL;
   C_2STR_INT_LIST CostSQLList;
   long            StartNode;
   C_INT_LONG_LIST VisitedNodes;
   C_INT_LONG_LIST VisitedLinks;
   C_INT_LONG      *pItem;
   C_SELSET        SS, entSS;

   C_CLASS   *pCls = gsc_find_class(1, 90), *pSub;

   Topo.set_type(TYPE_POLYLINE); // tipologia di tipo rete
   Topo.set_cls(pCls);

   // costo da nodo iniziale -> finale
   pCostSQL = new C_2STR_INT();
   pCostSQL->set_name("LUNGHEZZA"); // COSTO
   pCostSQL->set_type(2); // sub del lato
   CostSQLList.add_tail(pCostSQL);
   // costo da nodo finale -> iniziale
   pCostSQL = new C_2STR_INT();
   pCostSQL->set_name("LUNGHEZZA"); // COSTO
   pCostSQL->set_type(2); // sub del lato
   CostSQLList.add_tail(pCostSQL);

   if (Topo.LoadInMemory(&CostSQLList) == GS_BAD)	return GS_BAD;

   StartNode = 399;
   StartNode = gsc_searchTopoNetNode(Topo.NodesVett, Topo.nNodesVett, StartNode);

   Topo.NetCost = 50000;
   if (Topo.GetNetPropagation(StartNode) == GS_BAD)
   //Topo.NetFinalNode = 4005;
   //Topo.NetCost = (std::numeric_limits<double>::max)();
   //if (Topo.GetShortestNetPath(StartNode, 0.0) == GS_BAD)
      return GS_BAD;

   // links
   pSub = (C_CLASS *) pCls->ptr_sub_list()->search_key(2);
   pItem = (C_INT_LONG *) Topo.ptr_NetLinks()->get_head();
   while (pItem)
   {
      if (pSub->get_SelSet(pItem->get_id(), entSS) == GS_BAD) return GS_BAD;
      SS.add_selset(entSS);
         
      pItem = (C_INT_LONG *) pItem->get_next();
   }

   gsc_ddevid(pSub, SS);
#endif

   return 1;
}

#include "gs_whip.h"      // per "gsc_LispParser"
#include "gs_conv.h"

//class CThreadEx : public CWinThread
//{
//	public:
//   DECLARE_DYNCREATE(CThreadEx)
//	virtual BOOL InitInstance();
//	virtual int ExitInstance();
//         
//   //DECLARE_MESSAGE_MAP()
//};
//IMPLEMENT_DYNCREATE(CThreadEx, CWinThread)
//BOOL CThreadEx::InitInstance()
//{
//   C_STATUSBAR_PROGRESSMETER Progress1(_T("Prova di elaborazione 1"));
//   Progress1.Init(4, false);
//	m_pMainWnd = GEOsimAppl::WAIT_DLG_INSTANCE.pWaitDlg;
//   for (int qty = 1; qty <= 4; qty++)
//   {
//      gsc_wait(1);
//
//      //C_STATUSLINE_MESSAGE StatusLineMsg(_T("Prova di elaborazione 2"));
//      //StatusLineMsg.Init(gsc_msg(70), 1000);
//      C_STATUSBAR_PROGRESSMETER Progress2(_T("Prova di elaborazione 2"));
//      long tot = 10000000, i;
//      Progress2.Init(tot);
//      for (i = 1; i <= tot; i++)
//      {
//      //   //StatusLineMsg.Set(i);
//         Progress2.Set(i);
//         //gsc_wait(1);
//      }
//      ////StatusLineMsg.End();
//      Progress2.End();
//
//      Progress1.Set(qty);
//   }
//   Progress1.End();
//
//	return TRUE;
//}
//BOOL CThreadEx::ExitInstance()
//{
//   return CWinThread::ExitInstance();
//}
//
//UINT __cdecl MyControllingFunction( LPVOID pParam )
//{
//   long              id = 0, qty;
//   C_STATUSBAR_PROGRESSMETER Progress1(_T("Prova di elaborazione 1"));
//
//   id = 4;
//   Progress1.Init(id);
//
//   for (qty = 1; qty <= id; qty++)
//   {
//      //gsc_wait(1);
//
//      //C_STATUSLINE_MESSAGE StatusLineMsg(_T("Prova di elaborazione 2"));
//      //StatusLineMsg.Init(gsc_msg(70), 1000);
//      //C_STATUSBAR_PROGRESSMETER Progress2(_T("Prova di elaborazione 2"));
//      //long tot = 10000000, i;
//      //Progress2.Init(tot);
//      //for (i = 1; i <= tot; i++)
//      //{
//      //   //StatusLineMsg.Set(i);
//      //   Progress2.Set(i);
//      //   //gsc_wait(1);
//      //}
//      ////StatusLineMsg.End();
//      //Progress2.End();
//      Progress1.Set(qty);
//   }
//   Progress1.End();
//
//   return 1;
//}

int gs_prova()
{
#if defined(GSDEBUG) // se versione per debugging
   C_RB_LIST	  ColValues, TypeValues, Descr, arg;
   int           ret;
   C_CLASS       *pClass, *pCls;
   //struct _timeb t1, t2;
   double        tempo=0;
   ads_point     point, point2;
   ads_name      ent;
   C_SELSET      selset, SelSet;
   C_EED         eed;
   C_STRING      csrname, statement;
   
   long          qty = 0;
 
   C_LINK_SET    LinkSet;
   C_ATTRIB_LIST *p_attrib_list;
   
   C_STRING       InitStr, TableRef;

   C_DBCONNECTION *pConn;

   COleDateTime  _CreationDate;
   CString sss;   

   C_RB_LIST      qrycond;
   C_LINK_SET L_SS;

   C_CLASS          *pSensorCls = NULL, *pDetectedCls = NULL;
   C_RB_LIST         rbDescr, AdjList, RecValve, RecTrace, rb1, rb2;
   C_TOPOLOGY        Topa;
   presbuf           p = NULL;
   long              id = 0;
   int               res = 0, CValve = 0, tot = 0;
   int               i = 0;   
   double            Tot_Len = 0.0, Tot_Port = 0.0, param = 0.0, len1 = 0.0, len2 = 0.0;
   double            Port1 = 0.0, Port2 = 0.0;
   _CommandPtr    pCmd;
   _RecordsetPtr  pRs;
   FieldPtr       pFld;

   C_PREPARED_CMD pTestCmd, pTestCmd2;
   _RecordsetPtr  pTestRs, pTestRs2;
   C_STR_LIST     TransConnStrList;
   _CommandPtr    pCmd2;
   _ParameterPtr  pParam, pParam2;
   _variant_t     Val;
   _variant_t     vNull;

   gsc_getGUID(InitStr);
   return GS_GOOD;

   //AcDbDatabase *pDb = acdbHostApplicationServices()->workingDatabase();
   //AcApLayoutManager *pLayoutMngr = dynamic_cast<AcApLayoutManager *> (acdbHostApplicationServices()->layoutManager());
   //AcDbDictionary *pDict = NULL;

   //if (Acad::eOk == pDb->getLayoutDictionary(pDict, AcDb::kForRead))
   //{          
   //   AcDbObject *pObj;
   //   ACHAR *pLayoutName;
   //   const TCHAR *pLayerName;
   //   double scale;
   //   AcDbDictionaryIterator *pIter = pDict->newIterator();
   //   AcDbObjectIdArray ViewportArray;
   //   AcDbViewport *pEnt = NULL;

   //   //Iterate through all the items in the dictionary
   //   for(;!pIter->done();pIter->next())
   //   {
   //      pIter->getObject(pObj,AcDb::kForRead);

   //      AcDbLayout *pLayout = AcDbLayout::cast(pObj);

   //      //The caller should not free the returned string.
   //      ViewportArray = pLayout->getViewportArray();

   //      for (int i = 0; i < ViewportArray.length(); i++)
		 //  {
		 //     if (acdbOpenObject((AcDbObject*&)pEnt, ViewportArray[i], AcDb::kForRead) == Acad::eOk)
		 //     {
   //            pLayerName = pEnt->layer();
   //            if (wcscmp(pLayerName, _T("LAYOUT_VIEWPORT_A3")) == 0)
   //            {
   //               pLayout->getLayoutName(pLayoutName);
   //               scale = pEnt->customScale();
   //               acutPrintf(ACRX_T("\nLayout %s; Scala viewport %f, Scala "), pLayoutName, scale);
   //            }
   //   		}
   // 			pEnt->close();
		 //  }

   //      pLayout->close();
   //   }
   //   //clean up
   //   delete pIter;
   //   pDict->close();
   //}

   return GS_GOOD;

   //CWinThread *pWinThread = AfxBeginThread(RUNTIME_CLASS(CThreadEx)) ;
   //gsc_wait(30);
   //return GS_GOOD;
   //
   //CWinThread *Thread;
   //Thread = AfxBeginThread(MyControllingFunction, NULL);
   ////Thread = AfxBeginThread(MyControllingFunction, NULL, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
   ////Thread->m_bAutoDelete=FALSE;
   ////Thread->ResumeThread();
   //gsc_wait(30);
   //return GS_GOOD;

   vNull.vt    = VT_ERROR;
   vNull.scode = DISP_E_PARAMNOTFOUND;

   _RecordsetPtr  pSourceRs;
   _CommandPtr    pInsCmd;

   //double flo = 0.0000089847259658580416;
   //TCHAR *s = gsc_tostring(flo);        // DOUBLE

   //TCHAR _buffer[1000];  // make sure this is big enough!!!
   //swprintf_s(_buffer, sizeof(_buffer), _T("%.99f"), flo);
   //flo = 872346798734657;
   //swprintf_s(_buffer, sizeof(_buffer), _T("%.99f"), flo);
   //gsc_rtrim(_buffer, _T('0'));
   //gsc_rtrim(_buffer, _T('.'));

   //return GS_GOOD;
   InitStr = _T("V:\\gs2015\\CUSTOM\\PRJ6_CLS10.sql");
   gsc_ExeCmdFromFile(InitStr);
   return GS_GOOD;

   C_STATUSBAR_PROGRESSMETER Progress1(_T("Prova di elaborazione 1"));

   id = 4;
   Progress1.Init(id);

   for (qty = 1; qty <= id; qty++)
   {
      gsc_wait(1);

      C_STATUSLINE_MESSAGE StatusLineMsg(_T("Prova di elaborazione 2"));
      StatusLineMsg.Init(gsc_msg(70), 1000);
      //C_STATUSBAR_PROGRESSMETER Progress2(_T("Prova di elaborazione 2"));
      long tot = 100000, i;
      //Progress2.Init(tot);
      for (i = 1; i <= tot; i++)
      {
         StatusLineMsg.Set(i);
      //   if (i >= 9363000)
      //      Port2 = 1;
      //   Progress2.Set(i);
      }
      StatusLineMsg.End();
      //Progress2.End();
      Progress1.Set(qty);
   }
   Progress1.End();

   return GS_GOOD;


   C_2STR_LIST UDLProperties(_T("Data Source"), _T("$GEOSIM$\\SAMPLE\\Access 2010 Sample GS_PRJ.accdb"));
   if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProperties)) == NULL)
      return GS_BAD;
   if ((ColValues << pConn->ReadStruct(_T("GS_WRKSESSION"), ONETEST)) == NULL) return GS_BAD;
   gsc_printlist(ColValues.get_head());

   if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(_T("PostgreSQL test.udl"))) == NULL)
      return GS_BAD; 
   C_STRING ProviderDescr;
   long ProviderSize;
   int ProviderPrec;
   if (pConn->Type2ProviderType(adWChar, TRUE, TRUE, 5019, 1, 0, -1, ProviderDescr, &ProviderSize, &ProviderPrec) == GS_BAD)
      return GS_BAD;
   acutPrintf(_T("\n descr=%s, size=%ld"), ProviderDescr, ProviderSize);
   return GS_GOOD;

//   pCls = gsc_find_class(13, 54);
//   pConn = pCls->ptr_info()->getDBConnection(OLD);
//return gsc_getTablePrimaryKey(pConn->get_Connection(), _T("testi"), _T("testgs2011"), _T("dati"), &p);

   pCls = gsc_find_class(1, 53);
   pConn = pCls->ptr_info()->getDBConnection(OLD);

return gsc_getTableIndexes(pConn->get_Connection(), NULL, NULL, _T("croce"), &p);

   return GS_GOOD;


   //pCls->BeginTransaction(OLD, TRUE, TransConnStrList);

   pCls = gsc_find_class(23, 2, 1);

   pConn = pCls->ptr_info()->getDBConnection(OLD);
   pConn->get_Connection()->BeginTrans();

   //pTestRs.CreateInstance(__uuidof(Recordset));
   //pTestRs->Open(_T("SELECT * FROM dati.gas_s0n0021 WHERE gs_id=1"), pConn->get_Connection().GetInterfacePtr(), adOpenDynamic, adLockOptimistic, adCmdText);
   //pTestRs->Close();
   //pTestRs2.CreateInstance(__uuidof(Recordset));
   //pTestRs2->Open(_T("SELECT * FROM dati.gas_s0n0021 WHERE gs_id=1"), pConn->get_Connection().GetInterfacePtr(), adOpenDynamic, adLockOptimistic, adCmdText);
   //pTestRs2->Close();

   return GS_GOOD;

   C_DBL_MATRIX Matrix;
   //pCls = gsc_find_class(2, 26);
   pCls = gsc_find_class(1, 113);
   InitStr = _T("Z");
   point[X] = 1.0; point[Y] = 1.0;
   //point2[X] = 3.0; point2[Y] = 3.0;
   point2[X] = 2000000.0; point2[Y] = 5000000.0;
   statement = _T("VALLE");
   csrname   = _T("N_MONTE");

   if (acedGetPoint(NULL, _T("\nPrimo punto"), point) != RTNORM) return GS_BAD;
   //if (acedGetPoint(point, _T("\nSecondo punto"), point2) != RTNORM) return GS_BAD;

   ((C_CGRID *)pCls)->ptr_grid()->pt2key(point, &id);
   //if (((C_CGRID *)pCls)->SetHydrologyFlow(InitStr, point, point2, statement) == GS_BAD)
   //if (((C_CGRID *)pCls)->SetHydrologyCountUpstreamCells(statement, point, point2, csrname) == GS_BAD)
   if (((C_CGRID *)pCls)->GetCatchmentAreaCells(id, statement, L_SS) == GS_BAD)
      return GS_BAD;
   if (gsc_GridEvid((C_CGRID *) pCls, L_SS,
                    false, // ByCells,
                    InitStr, NULL, 1) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;


   if (gsc_ssget(NULL, NULL, NULL, NULL, selset) != RTNORM) return 0; 

   //L_SS.initSelSQLCond(pCls, selset, _T("CODICE<>0"));

//   _ftime(&t1);

//   if (L_SS.GetGSClassSS(pCls->ptr_id()->code, pCls->ptr_id()->sub_code, selset) == GS_BAD) return GS_BAD;
//   _ftime(&t2);
//   acutPrintf("Time %6.2f seconds.\n", (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000) );

   //Tot_Len = L_SS.getApproxTimeForInitSQLCond(pCls, _T("CODICE<>0"));
   pCls->get_LinkSet(_T("CODICE<>0"), L_SS);
   //pCls->get_SelSet("GS_ID<>0", L_SS);
   //SelSet.intersect(selset);

   L_SS.ptr_SelSet()->redraw(3);
   L_SS.ptr_KeyList()->print();

   return GS_GOOD;


/*
if (gsc_DBOpenRs(test2, _T("select * from test11"), pRs, adOpenKeyset,
                    adLockPessimistic, adCmdText) == GS_BAD)
                    if (gsc_DBOpenRs(test2, _T("test11"), pRs, adOpenKeyset,
                    adLockPessimistic, adCmdTable) == GS_BAD)
                    if (gsc_DBOpenRs(test2, _T("test11"), pRs, adOpenDynamic,
                    adLockPessimistic, adCmdTable) == GS_BAD)
                    if (gsc_DBOpenRs(test2, _T("select * from test11"), pRs, adOpenDynamic,
                    adLockPessimistic, adCmdText) == GS_BAD)
                    if (gsc_DBOpenRs(test2, _T("test11"), pRs, adOpenDynamic,
                    adLockBatchOptimistic, adCmdTable) == GS_BAD)
      return GS_BAD;
   gsc_DBCloseRs(pRs);
*/

return GS_GOOD;
      /*
   test2 = pConn->get_Connection();
   for (int i = 0; i < test2->Properties->Count; i++)
   acutPrintf(_T("\n%s=%s") , (TCHAR *)(test2->Properties->GetItem((long)i)->GetName()),
                            (TCHAR *)_bstr_t(test2->Properties->GetItem((long)i)->GetValue()));
*/



   return GS_GOOD;




   acutPrintf(gsc_msg(210)); // "\nSeleziona entità: "
   acedInitGet(RSG_NONULL,GS_EMPTYSTR);
   while ((ret = acedEntSel(GS_EMPTYSTR, ent, point)) == RTERROR);
   if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;

   eed.load(ent);
   // Cerco caratteristiche classe
   pClass = GS_CURRENT_WRK_SESSION->find_class(eed.cls);
   p_attrib_list = pClass->ptr_attrib_list();


return GS_GOOD;


//   _ftime(&t1);
//   _ftime(&t2);
//   tempo = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
//   acutPrintf("\n<gsc_getNodesOnPt> impiega %6.2f secondi.\n", tempo);

   acutPrintf(gsc_msg(210)); // "\nSeleziona entità: "
   acedInitGet(RSG_NONULL, GS_EMPTYSTR);
   while ((ret = acedEntSel(GS_EMPTYSTR, ent, point)) == RTERROR);
   if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (ret == RTCAN) return GS_CAN;

   eed.load(ent);
   // Cerco caratteristiche classe
   pClass = GS_CURRENT_WRK_SESSION->find_class(eed.cls);

 
#endif

   return GS_GOOD;
}