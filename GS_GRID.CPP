/**********************************************************
Name: GS_GRID.CPP

Module description: File per gestione delle griglia
            
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
#include <actrans.h>

#include <aced.h>
#include <acdb.h>
#include <dbelipse.h>
#include <dblead.h>
#include <dbray.h>
#include <dbxline.h>
#include <dbhatch.h>

#include "GSresource.h"

#include "gs_opcod.h"     // codici delle operazioni
#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori

#include "gs_resbf.h"     // gestione resbuf
#include "gs_list.h"      // gestione liste C++ 
#include "gs_thm.h"       // gestione tematismi e sistemi di coordinate
#include "gs_class.h" 
#include "gs_prjct.h" 
#include "gs_graph.h" 
#include "gs_query.h" 
#include "gs_attbl.h"     // gestione blocchi attributi visibili
#include "gs_utily.h" 
#include "gs_init.h" 
#include "gs_grid.h" 


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double GrdTempo1 = 0.0, GrdTempo2 = 0.0, GrdTempo3 = 0.0, GrdTempo4 = 0.0, GrdTempo5 = 0.0;
   double GrdTempo6 = 0.0, GrdTempo7 = 0.0, GrdTempo8 = 0.0, GrdTempo9 = 0.0, GrdTempo10 = 0.0;
   double GrdTempo11 = 0.0, GrdTempo12 = 0.0, GrdTempo13 = 0.0, GrdTempo14 = 0.0, GrdTempo15 = 0.0;
#endif


///////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS


int gsc_setFAS4Grid(C_FAS *pFAS, int Mode, C_COLOR *Color = NULL, TCHAR **Layer = NULL,
                    TCHAR **Hatch = NULL, C_COLOR *HatchColor = NULL,
                    double *HatchScale = NULL, double *HatchRotation = NULL);
int gsc_CloneTo3dPolyline(AcDb2dPolyline *pEnt);
int gsc_CloneTo3dPolyline(AcDbPolyline *pEnt);
int gsc_CloneTo3dPolyline(AcDbLine *pEnt);
int gsc_InitGridToMemory(C_GRID *pGrid, double **Vector, long *VectorLen);
void gsc_MatrixKey2VettPos(C_GRID *pInfoGrid, long LeftBottomKey, C_DBL_MATRIX &Matrix);
void gsc_MatrixVettPos2Key(C_DBL_MATRIX &Matrix, C_GRID *pInfoGrid, long LeftBottomKey);


/*********************************************************/
/*.doc gsc_setFAS4Grid                        <internal> */
/*+
   Funzione interna di ausilio per le funzioni di visualizzazione
   della griglia.
   C_FAS *pFAS;         Caratteristiche grafiche della visualizzazione
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici
   C_COLOR *Color;       Se = NULL non viene usato
   TCHAR **Layer;
   TCHAR **Hatch;
   C_COLOR *HatchColor;       Se = NULL non viene usato
   double *HatchScale;
   double *HatchRotation;
   
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_setFAS4Grid(C_FAS *pFAS, int Mode, C_COLOR *Color, TCHAR **Layer,
                    TCHAR **Hatch, C_COLOR *HatchColor, double *HatchScale, 
                    double *HatchRotation)
{
   if (pFAS)
   {
      if (Color)
         if (Mode == PREVIEW)
         {
            if (pFAS->color.getColorMethod() == C_COLOR::None)
               Color->setAutoCADColorIndex(7);
            else
               *Color = pFAS->color;
         }
         else 
            *Color = pFAS->color;

      if (Layer)
      {
         *Layer = pFAS->hatch_layer;
         // Se non esiste il layer lo crea
         if (gsc_crea_layer(*Layer) == GS_BAD) return GS_BAD;
      }
      
      if (Hatch) *Hatch = pFAS->hatch;

      if (HatchColor)
         if (Mode == PREVIEW)
         {
            if (pFAS->color.getColorMethod() == C_COLOR::None)
               HatchColor->setForeground();
            else
               *HatchColor = pFAS->hatch_color;
         }
         else 
            *HatchColor = pFAS->hatch_color;

      if (HatchScale)    *HatchScale    = pFAS->hatch_scale;
      if (HatchRotation) *HatchRotation = pFAS->hatch_rotation;
   }
   else
   {
      if (Color)
         if (Mode == PREVIEW) Color->setAutoCADColorIndex(7);
         else Color->setByLayer();   

      if (Layer)    *Layer    = NULL;
      if (Hatch)    *Hatch    = NULL;

      if (HatchColor)
         if (Mode == PREVIEW) HatchColor->setForeground();
         else HatchColor->setByLayer();   

      if (HatchScale)    *HatchScale    = 1.0;
      if (HatchRotation) *HatchRotation = 0.0;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_CloneTo3dPolyline                    <extern> */
/*+
   Duplica l'oggetto grafico in AcDb3dPolyline.
   Parametri:
   AcDb2dPolyline *pEnt;

   oppure
   AcDbPolyline *pEnt;

   Ritorna un GS_GOOD se l'oggetto è stato duplicato  altrimenti NULL.
-*/
/*********************************************************/
int gsc_CloneTo3dPolyline(AcDb2dPolyline *pEnt)
{
   AcGePoint3dArray   Vertices;
   AcDbObjectIterator *pVertIter = pEnt->vertexIterator();
   AcDbObjectId       vertexObjId;
   AcDb2dVertex       *pVertex;
   AcDb::Poly3dType   Type;
   C_STRING           layerName, lineTypeName;

   // Leggo vertici
   for (; !pVertIter->done(); pVertIter->step())
   {
      vertexObjId = pVertIter->objectId();
      if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
      {  // se NON è un punto di controllo della cornice di spline
         if (pVertex->vertexType() != AcDb::k2dSplineCtlVertex)
            Vertices.append(pVertex->position());
         pVertex->close();
      }
   }
   delete pVertIter;

   // Open the block table for read.
   AcDbBlockTable *pBlockTable;
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   AcDbBlockTableRecord *pBlockTableRecord;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   switch (pEnt->polyType())
   {
      case AcDb::k2dSimplePoly:
      case AcDb::k2dFitCurvePoly:
         Type = AcDb::k3dSimplePoly;
         break;
      case AcDb::k2dQuadSplinePoly:
         Type = AcDb::k3dQuadSplinePoly;
         break;
      case AcDb::k2dCubicSplinePoly:
         Type = AcDb::k3dCubicSplinePoly;
         break;
   }

   AcDb3dPolyline *p3dPolyline = new AcDb3dPolyline(Type, Vertices, pEnt->isClosed());

   p3dPolyline->setColor(pEnt->color());

   gsc_getLayer(pEnt, layerName);
   if (gsc_setLayer(p3dPolyline, layerName.get_name()) != GS_GOOD)
      { delete p3dPolyline; return GS_BAD; }

   gsc_get_lineType(pEnt, lineTypeName);
   p3dPolyline->setLinetype(lineTypeName.get_name());
   p3dPolyline->setLinetypeScale(pEnt->linetypeScale());
   p3dPolyline->setLineWeight(pEnt->lineWeight());

   pBlockTableRecord->appendAcDbEntity(p3dPolyline);
   pBlockTableRecord->close();
   p3dPolyline->close();

   return GS_GOOD;
}
int gsc_CloneTo3dPolyline(AcDbPolyline *pEnt)
{
   AcGePoint3dArray Vertices;
   AcGePoint3d      location;
   int              i = 0;
   C_STRING         layerName, lineTypeName;

   // Leggo vertici
   while (pEnt->getPointAt(i++, location) == Acad::eOk)
      Vertices.append(location);

   // Open the block table for read.
   AcDbBlockTable *pBlockTable;
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   AcDbBlockTableRecord *pBlockTableRecord;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   AcDb3dPolyline *p3dPolyline = new AcDb3dPolyline(AcDb::k3dSimplePoly, Vertices, pEnt->isClosed());

   p3dPolyline->setColor(pEnt->color());

   gsc_getLayer(pEnt, layerName);
   if (gsc_setLayer(p3dPolyline, layerName.get_name()) != GS_GOOD)
      { delete p3dPolyline; return GS_BAD; }

   gsc_get_lineType(pEnt, lineTypeName);
   p3dPolyline->setLinetype(lineTypeName.get_name());
   p3dPolyline->setLinetypeScale(pEnt->linetypeScale());
   p3dPolyline->setLineWeight(pEnt->lineWeight());

   pBlockTableRecord->appendAcDbEntity(p3dPolyline);
   pBlockTableRecord->close();
   p3dPolyline->close();

   return GS_GOOD;
}
int gsc_CloneTo3dPolyline(AcDbLine *pEnt)
{
   AcGePoint3dArray Vertices;
   AcGePoint3d      location;
   int              i = 0;
   C_STRING         layerName, lineTypeName;

   // Leggo vertici
   Vertices.append(pEnt->startPoint());
   Vertices.append(pEnt->endPoint());

   // Open the block table for read.
   AcDbBlockTable *pBlockTable;
   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                        AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   AcDbBlockTableRecord *pBlockTableRecord;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   AcDb3dPolyline *p3dPolyline = new AcDb3dPolyline(AcDb::k3dSimplePoly, Vertices, pEnt->isClosed());

   p3dPolyline->setColor(pEnt->color());

   gsc_getLayer(pEnt, layerName);
   if (gsc_setLayer(p3dPolyline, layerName.get_name()) != GS_GOOD)
      { delete p3dPolyline; return GS_BAD; }

   gsc_get_lineType(pEnt, lineTypeName);
   p3dPolyline->setLinetype(lineTypeName.get_name());
   p3dPolyline->setLinetypeScale(pEnt->linetypeScale());
   p3dPolyline->setLineWeight(pEnt->lineWeight());

   pBlockTableRecord->appendAcDbEntity(p3dPolyline);
   pBlockTableRecord->close();
   p3dPolyline->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GridPrepare_data_on_row_map_fmt   <internal>  */
/*+                                                                       
  Prepara un comando per la restituzione dei record in un intervallo.
  E' composta in 2 parti:
  
  gsc_GridPrepare_data_on_row_map_fmt_1
  Parametri:
  C_CGRID     *pCls;      Classe griglia
  const TCHAR *what;      Eventuale espressione da ritornare (default = NULL)
  const TCHAR *SQLWhere;  Eventuale condizione di filtro (default = NULL)
  C_STRING    &StmPrefix; out; Da usare in "gsc_GridPrepare_data_on_row_map_fmt_2"
  CAsiSession **pSession; out; Da usare in "gsc_GridPrepare_data_on_row_map_fmt_2"

  gsc_GridPrepare_data_on_row_map_fmt_2
  Parametri:
  C_CGRID     *pCls;      Classe griglia
  C_STRING    &StmPrefix;
  long        MinValue;   Valore minimo intervallo
  long        MaxValue;   Valore massimo intervallo
  CAsiSession *pSession;  out
  CAsiExecStm **pStm;     out
  CAsiCsr     **pCsr;     out

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_GridPrepare_data_on_row_map_fmt_1(C_CGRID *pCls, TCHAR *what, TCHAR *SQLWhere,
                                          C_STRING &StmPrefix, CAsiSession **pSession)
{
   C_STRING       UDL, TableRef, KeyAttribCorrected;
   C_DBCONNECTION *pConn;

   // ricavo connessione OLE-DB per tabella TEMP
   if ((pConn = pCls->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (pCls->getTempTableRef(TableRef, GS_BAD) == GS_BAD) return GS_BAD; // senza creare la tabella

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pCls->ptr_info()->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // preparo istruzione per lettura da TEMP
   StmPrefix = _T("SELECT ");
   StmPrefix += KeyAttribCorrected;
   if (what != NULL && wcslen(what) > 0)
   {
      StmPrefix += _T(',');
      StmPrefix += what;
   }
   StmPrefix += _T(" FROM ");
   if (gsc_Table2MAPFormat(pConn, TableRef, UDL) == GS_BAD) return GS_BAD;
   StmPrefix += UDL;

   StmPrefix += _T(" WHERE ");
   if (SQLWhere != NULL && wcslen(SQLWhere) > 0)
   {
      StmPrefix += _T("(");
      StmPrefix += SQLWhere;
      StmPrefix += _T(") AND ");
   }

	// se ha db e grafica
   if (pCls->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;
	if (gsc_SetACADUDLFile(UDL.get_name(), pCls->ptr_info()->getDBConnection(TEMP),
                          TableRef.get_name()) == GS_BAD)
		return GS_BAD;

   if ((*pSession = gsc_ASICreateSession(UDL.get_name())) == NULL)
      return GS_BAD;

   return GS_GOOD;
}
int gsc_GridPrepare_data_on_row_map_fmt_2(C_CGRID *pCls, C_STRING &StmPrefix,
                                          long MinValue, long MaxValue,
                                          CAsiSession *pSession, CAsiExecStm **pStm, 
                                          CAsiCsr **pCsr)
{
   C_STRING UDL, statement, KeyAttribCorrected;

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pCls->ptr_info()->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   // preparo istruzione per lettura da TEMP
   statement = StmPrefix;
   statement += _T("");
   statement += KeyAttribCorrected;
   statement += _T(">=");
   statement += MinValue;
   statement += _T(" AND ");
   statement += KeyAttribCorrected;
   statement += _T("<=");
   statement += MaxValue;

	// se ha db e grafica
   if (pCls->getLPNameTemp(UDL) == GS_BAD) return GS_BAD;

   if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                         pStm, pCsr) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONE C_GRID
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_GRID::pt2key                         <external> */
/*+                                                                       
  Dato un punto restituisce il codice della cella che lo contiene.
  N.B.: I punti noti della griglia sono considerati centroidi delle celle.
  Parametri:
  ads_point pt;		Punto selezionato
  long      *key;    Codice chiave della cella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GRID::pt2key(ads_point pt, long *key)
{                 
   ads_real CoordX, CoordY;
   long     OffsetX, OffsetY;

   CoordX = pt[X] - x;
   CoordY = pt[Y] - y;

   if (CoordX < 0 || CoordX >= (dx * nx))
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   if (CoordY < 0 || CoordY >= (dy * ny))
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   OffsetX = long(CoordX / dx) + 1;
   OffsetY = long(CoordY / dy);

   *key = OffsetY * nx + OffsetX;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GRID::key2pt                         <external> */
/*+                                                                       
  Dato il codice della cella restituisce il punto corrispondente (solo X e Y).
  Parametri:
  long      key;        Codice chiave della cella
  ads_point pt;         Punto selezionato
  bool      Centroid;   Se = TRUE, il punto sarà il centroide della cella altrimenti
                        sarà il punto in basso a sinistra (default = TRUE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GRID::key2pt(long key, ads_point pt, bool Centroid)
{                 
   long _internalKey = key - 1; // lo modifico per i calcoli

   pt[X] = x;
   pt[Y] = y;
   if (_internalKey)
   {
      pt[X] += ((_internalKey % nx) * dx);
      pt[Y] += ((long (_internalKey / nx)) * dy);
   }
   
   if (Centroid)
   {
      pt[X] += (dx / 2);
      pt[Y] += (dy / 2);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GRID::key2Rect                       <external> */
/*+                                                                       
  Dato il codice della cella restituisce i punti del rettangolo
  corrispondente.
  Parametri:
  long      key;        Codice chiave della cella
  ads_point pt1;        Punto in basso-sinistra
  ads_point pt2;        Punto alto-destra
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GRID::key2Rect(long key, ads_point pt1, ads_point pt2)
{
   if (key2pt(key, pt1, false) == GS_BAD) return GS_BAD;
   pt2[X] = pt1[X] + dx;
   pt2[Y] = pt1[Y] + dy;
   
   return GS_GOOD;
}
AcDbPolyline *C_GRID::key2Rect(long key)
{
   AcDbPolyline *pRect;
   AcGePoint2d  Vertice;
   ads_point    p1, p2;

   if (key2Rect(key, p1, p2) == GS_BAD) return NULL;

   if ((pRect = new AcDbPolyline(4)) == NULL) // 4 vertici
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   Vertice.set(p1[X], p1[Y]);
   pRect->addVertexAt(0, Vertice);
   Vertice.set(p1[X], p2[Y]);
   pRect->addVertexAt(1, Vertice);
   Vertice.set(p2[X], p2[Y]);
   pRect->addVertexAt(2, Vertice);
   Vertice.set(p2[X], p1[Y]);
   pRect->addVertexAt(3, Vertice);
   pRect->setClosed(Adesk::kTrue); // polilinea chiusa

   return pRect;
}


/*********************************************************/
/*.doc C_GRID::RowColumn2pt                   <external> */
/*+                                                                       
  Dato il numero della riga e della colonna restituisce il punto corrispondente (solo X e Y).
  Parametri:
  long      Column;     Colonna (inizia da 0)
  long      Row;        Riga (inizia da 0)
  ads_point pt;         Punto selezionato
  bool      Centroid;   Se = TRUE, il punto sarà il centroide della cella altrimenti
                        sarà il punto in basso a sinistra (default = TRUE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GRID::RowColumn2pt(long Column, long Row, ads_point pt, bool Centroid)
{                 
   pt[X] = x + dx * Column;
   pt[Y] = y + dy * Row;
   
   if (Centroid)
   {
      pt[X] += (dx / 2);
      pt[Y] += (dy / 2);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GRID::getRow                         <external> */
/*+                                                                       
  Dato il codice della cella restituisce la riga di appartenenza (0-based).
  Parametri:
  long Key;        Codice chiave della cella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
long C_GRID::getRow(long Key)
{                 
   long _internalKey = Key - 1; // lo modifico per i calcoli

   return (long) floor((double) (_internalKey / nx));
}


/*********************************************************/
/*.doc C_GRID::getRow                         <external> */
/*+                                                                       
  Dato un punto della cella restituisce la riga di appartenenza (0-based).
  Parametri:
  ads_point pt;   Punto interno ad una cella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
long C_GRID::getRow(ads_point pt)
{                 
   return (long) floor((double) ((pt[Y] - y) / dy));
}


/*********************************************************/
/*.doc C_GRID::getColumn                      <external> */
/*+                                                                       
  Dato il codice della cella restituisce la colonna di appartenenza (0-based).
  Parametri:
  long Key;        Codice chiave della cella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
long C_GRID::getColumn(long Key)
{                 
   long _internalKey = Key - 1; // lo modifico per i calcoli
   
   return (_internalKey % nx);
}


/*********************************************************/
/*.doc C_GRID::getColumn                      <external> */
/*+                                                                       
  Dato un punto della cella restituisce la colonna di appartenenza (0-based).
  Parametri:
  ads_point pt;   Punto interno ad una cella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
long C_GRID::getColumn(ads_point pt)
{                 
   return (long) floor((double) ((pt[X] - x) / dx));
}


/*********************************************************/
/*.doc C_GRID::getKey                         <external> */
/*+                                                                       
  Dato la colonna e la riga ricava il codice della cella.
  Parametri:
  long Column;       Colonna (0-based)
  long Row;          Riga    (0-based)

  Restituisce il codice della cella in caso di successo altrimenti restituisce -1. 
-*/  
/*********************************************************/
long C_GRID::getKey(long Column, long Row)
{                   
   return (nx * Row) + Column + 1;
}


/*********************************************************/
/*.doc C_GRID::getXLimit                      <external> */
/*+                                                                       
  Restituisce il limite massimo delle X della griglia.
-*/  
/*********************************************************/
double C_GRID::getXLimit() { return x + (dx * nx); }


/*********************************************************/
/*.doc C_GRID::getYLimit                      <external> */
/*+                                                                       
  Restituisce il limite massimo delle X della griglia.
-*/  
/*********************************************************/
double C_GRID::getYLimit() { return y + (dy * ny); }


/*****************************************************************************/
/*.doc C_GRID::getExtension                                       <external> */
/*+                                                                       
  Restituisce le estensioni della griglia.
  Parametri:
  C_RECT &Rect;   Rettangolo di estensione della griglia
  bool SubtractOffSetForTopRight; opzionale; siccome la cella considera il punto in 
                                  basso a sinistra compreso e il punto in alto a destra 
                                  escluso, in certi caso è utile considerare l'angolo
                                  in alto a destra meno un piccolo offset (x e y),
                                  default = false.
-*/  
/*****************************************************************************/
void C_GRID::getExtension(C_RECT &Rect, bool SubtractOffSetForTopRight)
{
   Rect.BottomLeft.point[X] = x;
   Rect.BottomLeft.point[Y] = y;
   Rect.TopRight.point[X]   = getXLimit();
   Rect.TopRight.point[Y]   = getYLimit();
   if (SubtractOffSetForTopRight)
   {
      Rect.TopRight.point[X] -= (dx / 10);
      Rect.TopRight.point[Y] -= (dy / 10);
   }
}


/*****************************************************************************/
/*.doc C_GRID::getKeyListInEntity                                 <external> */
/*+                                                                       
  Restituisce la lista dei codici delle celle che intersecano l'entità grafica.
  Parametri:
  AcDbEntity *pEnt;      puntatore a entità grafica
  int Mode;              Flag: INSIDE oppure CROSSING
  C_LONG_BTREE &KeyList; Lista dei codici delle celle (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*****************************************************************************/
int C_GRID::getKeyListInEntity(AcDbEntity *pEnt, int Mode, C_LONG_BTREE &KeyList)
{
   ads_point    pt1, pt2;
   C_RECT       Rect;
   AcDbPolyline *pEntRect;
   C_BLONG      *pKey;

   // calcolo il rettangolo di occupazione dell'entità
   if (gsc_get_ent_window(pEnt, pt1, pt2) == GS_BAD) return GS_BAD;
   Rect.Set(pt1, pt2);

   if (getKeyListInWindow(Rect, Mode, KeyList) == GS_BAD) return GS_BAD;

   // per ogni cella
   pKey = (C_BLONG *) KeyList.go_top();
   while (pKey)
   {
      // ricavo il rettangolo che descrive la cella
      if ((pEntRect = key2Rect(pKey->get_key())) == NULL)
         return GS_BAD;

      // verifico se l'occupazione del rettangolo della cella si interseca o è interna all'entità
      if (gsc_IsInternalEnt(pEnt, pEntRect, Mode) == GS_GOOD)
         pKey = (C_BLONG *) KeyList.go_next();
      else
      {
         KeyList.remove_at();
         pKey = (C_BLONG *) KeyList.get_cursor();
      }
      
      delete pEntRect;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_GRID::getKeyListInWindow                                 <external> */
/*+                                                                       
  Restituisce la lista dei codici delle celle che intersecano la finestra.
  Parametri:
  C_RB_LIST &CoordList;  Lista delle coordinate
  int type;              Flag: INSIDE oppure CROSSING
  C_LONG_BTREE &KeyList; Lista dei cofici delle celle (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*****************************************************************************/
int C_GRID::getKeyListInWindow(C_RECT &Rect, int Mode, C_LONG_BTREE &KeyList)
{
   C_RECT    Extension;
   ads_point pt;
   long      KeyMin, KeyMax;

   KeyList.remove_all();

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   getExtension(Extension, true);
   // Ricavo l'area di intersezione tra i 2 rettangoli
   if (Extension.Intersect(Rect) == GS_BAD) return GS_GOOD; // Non c'è intersezione

   if (Mode == INSIDE) // solo le celle completamente interne al rettangolo
   {       
      if (pt2key(Extension.BottomLeft.point, &KeyMin) == GS_BAD) return GS_BAD;
      if (key2pt(KeyMin, pt, false) == GS_BAD) return GS_BAD;
      if (pt[X] < Extension.BottomLeft.point[X]) Extension.BottomLeft.point[X] += dx;
      if (pt[Y] < Extension.BottomLeft.point[Y]) Extension.BottomLeft.point[Y] += dy;

      if (pt2key(Extension.TopRight.point, &KeyMin) == GS_BAD) return GS_BAD;
      if (key2pt(KeyMin, Extension.TopRight.point, false) == GS_BAD) return GS_BAD;
      Extension.TopRight.point[X] -= (dx / 10);
      Extension.TopRight.point[Y] -= (dy / 10);
   }
   else
   {
      if (pt2key(Extension.BottomLeft.point, &KeyMin) == GS_BAD) return GS_BAD;
      if (key2pt(KeyMin, Extension.BottomLeft.point, false) == GS_BAD) return GS_BAD;
   }

   pt[Y] = Extension.BottomLeft.point[Y]; // riga inferiore
   while (pt[Y] <= Extension.TopRight.point[Y])
   {
      // cella a sinistra
      pt[X] = Extension.BottomLeft.point[X];
      if (pt2key(pt, &KeyMin) == GS_BAD) return GS_BAD;

      // cella a destra
      pt[X] = Extension.TopRight.point[X];
      if (pt2key(pt, &KeyMax) == GS_BAD) return GS_BAD;

      for (long i = KeyMin; i <= KeyMax; i++)
         KeyList.add(&i);

      pt[Y] += dy; // riga superiore
   }

   return GS_GOOD;
}
int C_GRID::getKeyListInWindow(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList)
{
   C_RECT Rect;

   gsc_rb2Pt(CoordList.getptr_at(1), Rect.BottomLeft.point);
   gsc_rb2Pt(CoordList.getptr_at(2), Rect.TopRight.point);
   return getKeyListInWindow(Rect, Mode, KeyList);
}
int C_GRID::getKeyListInCircle(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList)
{
   ads_point center;
   double    radius;
   presbuf   p;

   if ((p = CoordList.get_head()) == NULL ||
       (p->restype != RTPOINT && p->restype != RT3DPOINT))
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   ads_point_set(p->resval.rpoint, center);
   if ((p = CoordList.get_next()) == NULL || p->restype != RTREAL)
      { GS_ERR_COD = eGSBadLocationQry; return GS_BAD; }
   radius = p->resval.rreal;

   AcGePoint3d dummyCenter(center[X], center[Y], center[Z]);
   AcDbCircle ContainerEnt(dummyCenter, AcGeVector3d(0.0, 0.0, 1.0), radius);

   return getKeyListInEntity(&ContainerEnt, Mode, KeyList);
}
int C_GRID::getKeyListInPolygon(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList)
{
   AcGePoint3dArray Vertices;
   presbuf          p;
   int              i = 0;

   Vertices.setLogicalLength(CoordList.GetCount());
   p = CoordList.get_head();
   while (p)
   {
      Vertices[i++].set(p->resval.rpoint[X], p->resval.rpoint[Y], 0.0) ; // X, Y, Z
      p = CoordList.get_next();
   }  

   AcDb3dPolyline ContainerEnt(AcDb::k3dSimplePoly, Vertices, Adesk::kTrue); // closed

   return getKeyListInEntity(&ContainerEnt, Mode, KeyList);
}
/*****************************************************************************/
/*.doc C_GRID::getKeyListFence                                               */
/*+
  Questa funzione effettua la selezione dei codici delle celle intersecanti ad
  una fence esistente (celle intersecanti una polyline).
  Parametri: 
  C_RB_LIST &CoordList;    Lista delle coordinate 
                           (<flag aperta o chiusa> <piano> <bulge1> <punto1> <bulge2> <punto2> ...)
                           <flag aperta o chiusa> = 0 se polilinea aperta altrimenti 1
                           <piano> vettore normale che identifica l'asse Z (0 0 1)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_GRID::getKeyListFence(C_RB_LIST &CoordList, C_LONG_BTREE &KeyList)
{
   presbuf      p;
   int          Closed, i = 0;
   double       Bulge;
   AcGePoint2d  Vertex;
   AcDbPolyline ContainerEnt;

   if (!(p = CoordList.get_head())) return GS_BAD;
   if (gsc_rb2Int(p, &Closed) == GS_BAD) return GS_BAD; // leggo flag aperta/chiusa

   if (!(p = p->rbnext)) return GS_BAD;
   if (!(p = p->rbnext)) return GS_BAD; // salto il vettore normale dell'asse Z
   
   while (p)
   {
      if (gsc_rb2Dbl(p, &Bulge) == GS_BAD) Bulge = 0.0; // potrebbe non esserci
      else if (!(p = p->rbnext)) return GS_BAD;

      AcGePoint2d_set_from_ads_point(p->resval.rpoint, Vertex);
      ContainerEnt.addVertexAt(i++, Vertex, Bulge);

      p = p->rbnext;
   }  

   if (Closed == 1) ContainerEnt.setClosed(Adesk::kTrue);

   return getKeyListInEntity(&ContainerEnt, CROSSING, KeyList);
}
int C_GRID::getKeyListBufferFence(C_RB_LIST &CoordList, int Mode, C_LONG_BTREE &KeyList)
{
   presbuf      p;
   int          Closed;
   double       Bulge1, Bulge2, OffSet;
   AcGePoint3d  Vertex, NextVertex, StartVertex;
   C_LONG_BTREE PartialKeyList;

   if (!(p = CoordList.get_head())) return GS_BAD;
   if (gsc_rb2Dbl(p, &OffSet) == GS_BAD) return GS_BAD; // leggo offset
   if (!(p = p->rbnext)) return GS_BAD;
   if (gsc_rb2Int(p, &Closed) == GS_BAD) return GS_BAD; // leggo flag aperta/chiusa

   if (!(p = p->rbnext)) return GS_BAD;
   if (!(p = p->rbnext)) return GS_BAD; // salto il vettore normale dell'asse Z

   // Per ogni tratto della polilinea verifico gli oggetti
   if (gsc_rb2Dbl(p, &Bulge1) == GS_BAD) Bulge1 = 0.0; // potrebbe non esserci
   else if (!(p = p->rbnext)) return GS_BAD;

   if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
   Vertex.set(p->resval.rpoint[X], p->resval.rpoint[Y], p->resval.rpoint[Z]);
   if (Closed == 1) StartVertex.set(p->resval.rpoint[X], p->resval.rpoint[Y], p->resval.rpoint[Z]);

   while ((p = p->rbnext))
   {
      if (gsc_rb2Dbl(p, &Bulge2) == GS_BAD) Bulge2 = 0.0; // potrebbe non esserci
      else if (!(p = p->rbnext)) return GS_BAD;

      if (p->restype != RTPOINT && p->restype != RT3DPOINT) return GS_BAD;
      NextVertex.set(p->resval.rpoint[X], p->resval.rpoint[Y], p->resval.rpoint[Z]);

      if (getKeyListBufferFence(Vertex, NextVertex, Bulge1, OffSet, Mode, PartialKeyList) == GS_BAD)
         return GS_BAD;

      if (KeyList.add_list(PartialKeyList) == GS_BAD) return GS_BAD;

      Vertex.set(NextVertex.x, NextVertex.y, NextVertex.z);
      Bulge1 = Bulge2;
   }

   if (Closed == 1) // Se polilinea chiusa
   {
      if (getKeyListBufferFence(Vertex, StartVertex, Bulge1, OffSet, Mode, PartialKeyList) == GS_BAD)
         return GS_BAD;

      if (KeyList.add_list(PartialKeyList) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_GRID::getKeyListBufferFence                           <internal>    */
/*+
  Questa funzione è di ausilio alla C_GRID::getKeyListBufferFence e ottiene
  gli oggetti che sono entro un buffer di un segmento rettilineo o un arco.
  Parametri:
  AcGePoint3d &Vertex;     Primo punto del segmento
  AcGePoint3d &NextVertex; Secondo punto del segmento
  double Bulge;            Tangente di 1/4 angolo interno
  double OffSet;           Distanza
  int Mode;                Flag: se INSIDE -> "inside", se CROSSING -> "crossing"
  C_LONG_BTREE &KeyList;   Risultato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_GRID::getKeyListBufferFence(AcGePoint3d &Vertex, AcGePoint3d &NextVertex, 
                                  double Bulge, double OffSet, int Mode,
                                  C_LONG_BTREE &KeyList)
{
   AcDbPolyline *pExtBuffer, *pIntBuffer;
   int          res;

   if (Bulge == 0) // segmento rettilineo
   {
      if (gsc_getBufferOnLine(Vertex, NextVertex, OffSet, &pExtBuffer) == GS_BAD) return GS_BAD;
      pIntBuffer = NULL;
   }
   else // arco
      if (Bulge < 0) // senso orario
      {
         if (gsc_getBufferOnArc(NextVertex, Vertex, -1 * Bulge, OffSet, &pExtBuffer, &pIntBuffer) == GS_BAD)
            return GS_BAD;
      }
      else // senso antiorario
         if (gsc_getBufferOnArc(Vertex, NextVertex, Bulge, OffSet, &pExtBuffer, &pIntBuffer) == GS_BAD)
            return GS_BAD;

   do
   {
      if ((res = getKeyListInEntity(pExtBuffer, Mode, KeyList)) == GS_BAD) break;

      if (pIntBuffer) // sottraggo le celle che sono in pIntBuffer
      {
         C_LONG_BTREE SubtractKeyList;

         if ((res = getKeyListInEntity(pIntBuffer, Mode, SubtractKeyList)) == GS_BAD) break;
         KeyList.subtract(SubtractKeyList);
      }
   }
   while (0);

   pExtBuffer->close();
   delete pExtBuffer;
   if (pIntBuffer)
   {
      pIntBuffer->close();
      delete pIntBuffer;
   }

   return res;
}

int C_GRID::getKeyListInsideSS(C_SELSET &EntSS, int Mode, C_LONG_BTREE &KeyList)
{
   ads_point    pt1, pt2;
   C_RECT       Rect;
   AcDbPolyline *pEntRect;
   C_BLONG      *pKey;

   // calcolo il rettangolo di occupazione dell'entità
   if (EntSS.getWindow(pt1, pt2) == GS_BAD) return GS_BAD;
   Rect.Set(pt1, pt2);

   if (getKeyListInWindow(Rect, Mode, KeyList) == GS_BAD) return GS_BAD;

   // per ogni cella
   pKey = (C_BLONG *) KeyList.go_top();
   while (pKey)
   {
      // ricavo il rettangolo che descrive la cella
      if ((pEntRect = key2Rect(pKey->get_key())) == NULL)
         return GS_BAD;

      // verifico se l'occupazione del rettangolo della cella si interseca o è interna all'entità
      if (gsc_IsInternalEnt(EntSS, pEntRect, Mode) == GS_GOOD)
         pKey = (C_BLONG *) KeyList.go_next();
      else
      {
         KeyList.remove_at();
         pKey = (C_BLONG *) KeyList.get_cursor();
      }

      delete pEntRect;
   }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONE C_GRID
// INIZIO FUNZIONE C_CGRID
///////////////////////////////////////////////////////////////////////////////

/*********************************************************/
/*.doc C_CGRID::LoadToMatrix                  <internal> */
/*+
  Funzione che carica in un matrice i valori numerici di un attributo
  per tutte le celle della griglia interne ad un rettangolo.
  C_STRING &AttribZ;    Nome dell'attributo da utilizzare per leggere la
                        coordinata Z.
  ads_point pt1;        punto in basso a sinistra
  ads_point pt2;        punto in alto a destra
  C_DBL_MATRIX &Matrix; Matrice di numeri double
  long *LeftBottomKey;  Codice chiave della cella in basso-sinistra
                        (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::LoadToMatrix(C_STRING &AttribZ, ads_point pt1, ads_point pt2,
                          C_DBL_MATRIX &Matrix, long *LeftBottomKey)
{
   C_GRID         *pInfoGrid = ptr_grid();
   C_RECT         Rect, Location(pt1, pt2);
   ads_point      pt;
   C_PREPARED_CMD pCmd;
   long           iColumn, InitColumn, LastColumn, nRows, nCols, i = 0;
   long           iRow, LastRow, Key, count = 0, x = 0, y = 0;
   int            Result = GS_GOOD, IsRsCloseable;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pZValue = NULL;
   double         Val;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   // Ricavo l'area di intersezione tra i 2 rettangoli
   if (Rect.Intersect(Location) == GS_BAD) return GS_GOOD; // Non c'è intersezione

   // Ricavo il codice della cella in basso-sinistra
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD) return GS_BAD;
   // Lo memorizzo se richiesto
   if (LeftBottomKey) *LeftBottomKey = Key;
   // Ricavo riga e colonna iniziale
   InitColumn = pInfoGrid->getColumn(Key);
   iRow       = pInfoGrid->getRow(Key);
   
   // Ricavo il codice della cella in alto-sinistra
   pt[X] = Rect.TopRight.point[X];
   pt[Y] = Rect.TopRight.point[Y];
   if (pInfoGrid->pt2key(pt, &Key) == GS_BAD) return GS_BAD;
   LastRow    = pInfoGrid->getRow(Key);
   LastColumn = pInfoGrid->getColumn(Key);

   // Calcolo il numero di colonne che intercorrono tra le due celle
   if ((nCols = LastColumn - InitColumn + 1) <= 0) return GS_BAD;
   // Calcolo il numero di righe che intercorrono tra le due celle
   if ((nRows = LastRow - iRow + 1) <= 0) return GS_BAD;

   // alloco la matrice
   if (Matrix.alloc(nCols, nRows) == GS_BAD) return GS_BAD;
   
   // preparo comando di lettura dati
   if (prepare_data(pCmd, TEMP) == GS_BAD) return GS_BAD;

   C_STRING Title;
   Title.set_name_formatted(gsc_msg(407), AttribZ.get_name()); // "Caricamento valore <%s> della griglia in memoria"
   acutPrintf(_T("\n%s\n"), Title.get_name());

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(nRows * nCols);

   while (iRow <= LastRow)
   {
      x       = 0;
      iColumn = InitColumn;
      if ((Key = pInfoGrid->getKey(iColumn, iRow)) == -1)
         { Result = GS_BAD; break; }

      if (gsc_get_data(pCmd, Key, pRs, &IsRsCloseable) == GS_BAD)
         { Result = GS_BAD; break; }

      while (iColumn++ <= LastColumn)
      {
         StatusBarProgressMeter.Set(++i);

         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            { Result = GS_BAD; break; }

         if (!pZValue)
            if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name())))
               { Result = GS_BAD; break; }

         if (gsc_rb2Dbl(pZValue, &Val) == GS_GOOD)
            Matrix.set(x, y, Val);
         else
            Matrix.set(x, y, (std::numeric_limits<double>::min)()); // assegno il minimo per un double

         x++;

         // se è possibile usare la tecnica del seek
         if (IsRsCloseable == GS_BAD)
            gsc_Skip(pRs);
         else
         {
            gsc_DBCloseRs(pRs);
            gsc_get_data(pCmd, ++Key, pRs, &IsRsCloseable);
         }
      }
      iRow++;
      y++;
   }

   if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return Result;
}


/*********************************************************/
/*.doc C_CGRID::UpdFromMatrix                 <internal> */
/*+
  Funzione che aggiorna un campo numerico di un database leggendo
  i valori da una matrice.
  ads_point pt1;        punto in basso a sinistra
  ads_point pt2;        punto in alto a destra
  C_DBL_MATRIX &Matrix; Matrice di numeri double
  C_STRING &AttribZ;    Nome dell'attributo da aggiornare.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::UpdFromMatrix(ads_point pt1, ads_point pt2,
                           C_DBL_MATRIX &Matrix, C_STRING &Attrib)
{
   C_GRID         *pInfoGrid = ptr_grid();
   C_RECT         Rect, Location(pt1, pt2);
   ads_point      pt;
   C_PREPARED_CMD pCmd;
   long           iColumn, InitColumn, LastColumn, nRows, nCols, i = 0;
   long           iRow, LastRow, Key, count = 0, x = 0, y = 0;
   int            Result = GS_GOOD, IsRsCloseable;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pValue = NULL;
   double         Val;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   // Ricavo l'area di intersezione tra i 2 rettangoli
   if (Rect.Intersect(Location) == GS_BAD) return GS_GOOD; // Non c'è intersezione

   // Ricavo il codice della cella in basso-sinistra
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD) return GS_BAD;
   // Ricavo riga e colonna iniziale
   InitColumn = pInfoGrid->getColumn(Key);
   iRow       = pInfoGrid->getRow(Key);
   
   // Ricavo il codice della cella in alto-sinistra
   pt[X] = Rect.TopRight.point[X];
   pt[Y] = Rect.TopRight.point[Y];
   if (pInfoGrid->pt2key(pt, &Key) == GS_BAD) return GS_BAD;
   LastRow    = pInfoGrid->getRow(Key);
   LastColumn = pInfoGrid->getColumn(Key);

   // Calcolo il numero di colonne che intercorrono tra le due celle
   if ((nCols = LastColumn - InitColumn + 1) <= 0) return GS_BAD;
   // Calcolo il numero di righe che intercorrono tra le due celle
   if ((nRows = LastRow - iRow + 1) <= 0) return GS_BAD;
  
   // preparo comando di aggiornamento dati
   if (prepare_data(pCmd, TEMP) == GS_BAD) return GS_BAD;

   C_STRING Title(gsc_msg(460)) ;// "Aggiornamento griglia"
   acutPrintf(_T("\n%s\n"), Title.get_name());

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(nRows * nCols);

   while (iRow <= LastRow)
   {
      x       = 0;
      iColumn = InitColumn;
      if ((Key = pInfoGrid->getKey(iColumn, iRow)) == -1)
         { Result = GS_BAD; break; }

      if (gsc_get_data(pCmd, Key, pRs, &IsRsCloseable) == GS_BAD)
         { Result = GS_BAD; break; }

      while (iColumn++ <= LastColumn)
      {
         StatusBarProgressMeter.Set(++i);

         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { Result = GS_BAD; break; }

         if (!pValue)
            if (!(pValue = ColValues.CdrAssoc(Attrib.get_name())))
               { Result = GS_BAD; break; }

         Val = Matrix.get(x, y);
         if (Val == (std::numeric_limits<double>::min)()) gsc_RbSubstNIL(pValue);
         else gsc_RbSubst(pValue, Matrix.get(x, y));

         if (upd_data(Key, ColValues, &pCmd) == GS_BAD)
            { Result = GS_BAD; break; }

         x++;
         Key++;

         // se è possibile usare la tecnica del seek
         if (IsRsCloseable == GS_BAD)
            gsc_Skip(pRs);
         else
         {
            gsc_DBCloseRs(pRs);
            gsc_get_data(pCmd, Key, pRs, &IsRsCloseable);
         }
      }
      iRow++;
      y++;
   }

   if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return Result;
}


/*********************************************************/
/*.doc gs_DisplayGridBorder                   <external> */
/*+
   Funzione LISP per visualizzare il bordo della griglia.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   (<prj><cls><sub>[Color[Layer[Hatch[Display mode]]]])

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_DisplayGridBorder(void)
{
   presbuf arg = acedGetArgs();
   C_FAS   FAS;
   int     prj, cls, sub, DisplayMode = PREVIEW;
   C_CGRID *pCls;
   bool    ChangeFAS = FALSE;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   if (arg)
   {  // codice colore
      if (FAS.hatch_color.setResbuf(arg) == GS_BAD) FAS.hatch_color.setByLayer();
      else ChangeFAS = TRUE;

      if ((arg = arg->rbnext))
      {  // nome del piano
         if (arg->restype == RTSTR)
         {
            gsc_strcpy(FAS.hatch_layer, arg->resval.rstring, MAX_LEN_LAYERNAME);
            ChangeFAS = TRUE;
         }

         if ((arg = arg->rbnext))                           
         {  // nome del riempimento
            if (arg->restype == RTSTR)
            {
               gsc_strcpy(FAS.hatch, arg->resval.rstring, MAX_LEN_HATCHNAME);
               ChangeFAS = TRUE;
            }

            if ((arg = arg->rbnext))                           
               // Modalità di visualizzazione
               gsc_rb2Int(arg, &DisplayMode);
         }
      }
   }

   if (pCls->DisplayBorder((ChangeFAS) ? &FAS : NULL, DisplayMode) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}

/*********************************************************/
/*.doc C_CGRID::DisplayBorder                  external> */
/*+
   Funzione LISP per visualizzare il bordo della griglia.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_FAS *pFAS;         Opzionale; Caratteristiche grafiche della visualizzazione
                        (default = NULL)
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayBorder(C_FAS *pFAS, int Mode)
{
   C_RECT    Rect;
   TCHAR     *Layer, *Hatch;
   C_COLOR   Color, HatchColor;
   double    HatchScale, HatchRotation;

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer, &Hatch, 
                       &HatchColor, &HatchScale, &HatchRotation) == GS_BAD)
      return GS_BAD;

   // Estensioni della griglia
   grid.getExtension(Rect, true);

   if (Mode != PREVIEW && gsc_strlen(Hatch) > 0)
      // Visualizza solo il riempimento
      return gsc_insert_hatch(Rect.BottomLeft.point, Rect.TopRight.point, Hatch, Layer, 
                              HatchScale, HatchRotation, &HatchColor);
   else
      // Visualizza rettangolo
      return gsc_insert_rectangle(Rect.BottomLeft.point, Rect.TopRight.point, Layer, &Color, Mode);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_DisplayGridPts                       external> */
/*+
   Funzione LISP per visualizzare la griglia tramite dei punti centro delle celle.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   (<prj><cls><sub>[<pt1><pt2>[<AttribZ>[SQLCond[Color[Layer[Centroid[Display mode]]]]]])

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_DisplayGridPts(void)
{
   presbuf   arg = acedGetArgs();
   ads_point pt1, pt2;
   C_STRING  AttribZ, SQLCond;
   C_FAS     FAS;
   int       prj, cls, sub, DisplayMode = PREVIEW;
   C_CGRID   *pCls;
   bool      pt1Exist = FALSE, Location = FALSE, ChangeFAS = FALSE, Centroid = FALSE;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   if (arg)
   {
      // punto inferiore della finestra
      if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
      if ((arg = arg->rbnext))
      {
         if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
         if ((arg = arg->rbnext))
         {  // nome dell'attributo contenente il valore Z dei punti
            if (arg->restype == RTSTR) AttribZ = arg->resval.rstring;
            if ((arg = arg->rbnext))
            {  // condizione SQL
               if (arg->restype == RTSTR) SQLCond = arg->resval.rstring;
               if ((arg = arg->rbnext))
               {  // codice colore
                  if (FAS.color.setResbuf(arg) == GS_BAD) FAS.color.setByLayer();
                  else ChangeFAS = TRUE;

                  if ((arg = arg->rbnext))
                  {  // nome del piano
                     if (arg->restype == RTSTR)
                        { gsc_strcpy(FAS.layer, arg->resval.rstring, 255); ChangeFAS = TRUE; }

                     if ((arg = arg->rbnext))                           
                     {
                        // Se il punto deve essere nel centroide della cella
                        if (arg->restype == RTT) Centroid = TRUE;

                        if ((arg = arg->rbnext))                           
                           // Modalità di visualizzazione
                           gsc_rb2Int(arg, &DisplayMode);
                     }
                  }
               }
            }
         }
      }
   }

   if (pCls->DisplayPts((Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                        (AttribZ.get_name()) ? &AttribZ : NULL,
                        &SQLCond, FALSE, // ACCESS Format
                        (ChangeFAS) ? &FAS : NULL,
                        Centroid,
                        DisplayMode) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_GridDisplayPtsSQL                             */
/*+
   Funzione per visualizzare la griglia tramite dei punti delle celle
   che soddisfano una condizione SQL in formato ACCESS.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_CGRID              *pCls;
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pAttribZ;  Opzionale; Nome dell'attributo da utilizzare per leggere la
                        coordinata Z. Se = NULL i punti saranno con Z = 0 (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   const TCHAR *Layer;  Nome del piano
   C_COLOR &Color;       Codice colore
   bool Centroid;       Flag; Se = TRUE indica che i punti saranno generati nel centroide
                        delle celle, altrimenti i punti saranno relativi all'angolo in
                        basso a sinistra di ciascuna cella (default = FALSE)
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_GridDisplayPtsSQL(C_CGRID *pCls, ads_point pt1, ads_point pt2,
                          C_STRING *pAttribZ, C_STRING *pSQLCond,
                          const TCHAR *Layer, C_COLOR &Color, bool Centroid, int Mode)
{
   C_GRID         *pInfoGrid = pCls->ptr_grid();
   C_RECT         Rect;
   ads_point      pt;
   long           Key, i = 0;
   C_PREPARED_CMD Cmd;
   C_RB_LIST      ColValues;
   long           KeyMin, KeyMax;
   ads_point      _pt2;
   _RecordsetPtr  pRs;
   presbuf        pZValue, pKeyValue;
   bool           FirstTime = TRUE;
   int            AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."
   
   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto della prima cella da visualizzare
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       pInfoGrid->key2pt(Key, Rect.BottomLeft.point, Centroid) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // preparo comando per lettura record nella riga
      if (pCls->prepare_data_on_row(Cmd,
                                    (pAttribZ) ? pAttribZ->get_name() : NULL,            // What
                                    (pSQLCond) ? pSQLCond->get_name() : NULL) == GS_BAD) // Where
         AfxThrowUserException();
      _pt2[X] = Rect.TopRight.point[X]; // estremo destro

      // Ciclo sulla Y
      while (pt[Y] <= Rect.Top())
      {
         // imposto id inferiore e superiore
         if (pInfoGrid->pt2key(pt, &KeyMin) == GS_BAD) AfxThrowUserException();
         _pt2[Y] = pt[Y];
         if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) AfxThrowUserException();

         if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs) == GS_BAD)
            AfxThrowUserException();

         // Ciclo sulla X
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) AfxThrowUserException();

            if (FirstTime)
            {
               FirstTime = FALSE;
               if (pAttribZ) pZValue = ColValues.CdrAssoc(pAttribZ->get_name());
               pKeyValue = ColValues.CdrAssoc(pCls->ptr_info()->key_attrib.get_name());
            }

            if (pSQLCond)
            {
               if (gsc_rb2Lng(pKeyValue, &Key) == GS_BAD) Key = 0;
               if (pInfoGrid->key2pt(Key, pt, Centroid) == GS_BAD) AfxThrowUserException();
            }

            if (pAttribZ)
               if (gsc_rb2Dbl(pZValue, &(pt[Z])) == GS_BAD)
               {
                  StatusLineMsg.Set(++i); // "%ld celle elaborate."

                  pt[X] += pInfoGrid->dx; // avanza con la X
                  gsc_Skip(pRs);

                  continue;
               }

            // Visualizza punto
            if (Mode == PREVIEW)
            {
               if (acedGrDraw(pt, pt, AutoCADColorIndex, 0) != RTNORM) AfxThrowUserException();
            }
            else
               if (gsc_insert_point(pt, Layer, &Color, 0.0) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pt[X] += pInfoGrid->dx; // avanza con la X
            gsc_Skip(pRs);
         }
         gsc_DBCloseRs(pRs);
            
         pt[X] = Rect.BottomLeft.point[X];
         pt[Y] += pInfoGrid->dy; // avanza con la Y
      }
   } // fine try

   catch (...) // any type of exception
   {
      return GS_BAD;
   }

   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}
int gsc_GridDisplayPtsMAPSQL(C_CGRID *pCls, ads_point pt1, ads_point pt2,
                             C_STRING *pAttribZ, C_STRING *pSQLCond,
                             const TCHAR *Layer, C_COLOR &Color, bool Centroid, int Mode)
{
   C_GRID         *pInfoGrid = pCls->ptr_grid();
   C_RECT         Rect;
   ads_point      pt;
   long           Key, i = 0;
   CAsiSession    *pSession;
   CAsiExecStm    *pStm;
   CAsiCsr        *pCsr;
   CAsiData       *pData;
   CAsiRow        *pRow;
   long           KeyMin, KeyMax;
   ads_point      _pt2;
   resbuf         RbZValue, RbKeyValue;
   C_STRING       SQLwhat, StmPrefix;
   int            AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   if (pAttribZ)
   {
      SQLwhat = _T('\"');
      SQLwhat += pAttribZ->get_name();
      SQLwhat += _T('\"');
   }

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto della prima cella da visualizzare
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       pInfoGrid->key2pt(Key, Rect.BottomLeft.point, Centroid) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // preparo comando per lettura record nella riga
      if (gsc_GridPrepare_data_on_row_map_fmt_1(pCls,
                                                SQLwhat.get_name(),   // What
                                                (pSQLCond) ? pSQLCond->get_name() : NULL, // Where
                                                StmPrefix, &pSession) == GS_BAD)
         AfxThrowUserException();
      _pt2[X] = Rect.TopRight.point[X]; // estremo destro

      // Ciclo sulla Y
      while (pt[Y] <= Rect.Top())
      {
         // imposto id inferiore e superiore
         if (pInfoGrid->pt2key(pt, &KeyMin) == GS_BAD) AfxThrowUserException();
         _pt2[Y] = pt[Y];
         if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) AfxThrowUserException();

         if (gsc_GridPrepare_data_on_row_map_fmt_2(pCls, StmPrefix, KeyMin, KeyMax,
                                                   pSession, &pStm, 
                                                   &pCsr) == GS_BAD)
            AfxThrowUserException();

         if (pCsr->Open() != kAsiGood)
         {
            gsc_printASIErr(pCsr);
            AfxThrowUserException();
         }

         // Ciclo sulla X
         while (pCsr->Fetch() == kAsiGood)
         {
            if ((pRow = pCsr->getCurrentRow()) == NULL)
               { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }

            if (pSQLCond)
            {
               if ((pData = (*pRow)[0].getData()) == NULL) // Key_attrib
                  { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }
               if (gsc_ASIData2Rb(pData, &RbKeyValue) == GS_BAD)
                  { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }

               if (gsc_rb2Lng(&RbKeyValue, &Key) == GS_BAD) Key = 0;
               if (pInfoGrid->key2pt(Key, pt, Centroid) == GS_BAD) AfxThrowUserException();
            }

            if (pAttribZ)
            {
               if ((pData = (*pRow)[1].getData()) == NULL) // AttribZ
                  { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }
               if (gsc_ASIData2Rb(pData, &RbZValue) == GS_BAD)
                  { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }

               if (gsc_rb2Dbl(&RbZValue, &(pt[Z])) == GS_BAD)
               {
                  StatusLineMsg.Set(++i); // "%ld celle elaborate."

                  pt[X] += pInfoGrid->dx; // avanza con la X

                  continue;
               }
            }

            // Visualizza punto
            if (Mode == PREVIEW)
            {
               if (acedGrDraw(pt, pt, AutoCADColorIndex, 0) != RTNORM) AfxThrowUserException();
            }
            else
               if (gsc_insert_point(pt, Layer, &Color, 0.0) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pt[X] += pInfoGrid->dx; // avanza con la X
         }
         pCsr->Close();
         delete pCsr;
         gsc_ASITermStm(&pStm);
            
         pt[X] = Rect.BottomLeft.point[X];
         pt[Y] += pInfoGrid->dy; // avanza con la Y
      }
   } // fine try

   catch (...) // any type of exception
   {
      if (pCsr) { pCsr->Close(); delete pCsr; }
      gsc_ASITermStm(&pStm);
      gsc_ASITermSession(&pSession);

      return GS_BAD;
   }

   gsc_ASITermSession(&pSession);

   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::DisplayPts                     external> */
/*+
   Funzione per visualizzare la griglia tramite dei punti delle celle.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pAttribZ;  Opzionale; Nome dell'attributo da utilizzare per leggere la
                        coordinata Z. Se = NULL i punti saranno con Z = 0 (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   bool SQLMAPFmt;      Opzionale usato se pSQLCond<>NULL; indica se la sintassi 
                        della condizione SQL è nel formato di Autocad MAP oppure
                        se ACCESS (default = TRUE)
   C_FAS *pFAS;         Opzionale; Caratteristiche grafiche della visualizzazione
                        (default = NULL)
   bool Centroid;       Flag; Se = TRUE indica che i punti saranno generati nel centroide
                        delle celle, altrimenti i punti saranno relativi all'angolo in
                        basso a sinistra di ciascuna cella (default = FALSE)
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayPts(ads_point pt1, ads_point pt2,
                        C_STRING *pAttribZ, C_STRING *pSQLCond, bool SQLMAPFmt,
                        C_FAS *pFAS, bool Centroid, int Mode)
{
   C_RECT    Rect;
   ads_point pt;
   long      Key, i = 0;
   TCHAR     *Layer;
   C_COLOR   Color;
   int       AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer) == GS_BAD) return GS_BAD;
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto della prima cella da visualizzare
   if (grid.pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       grid.key2pt(Key, Rect.BottomLeft.point, Centroid) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      if (!pAttribZ && (!pSQLCond || pSQLCond->len() == 0)) // non devo leggere il db
      {
         // Ciclo sulla Y
         while (pt[Y] <= Rect.Top())
         {
            // Ciclo sulla X
            while (pt[X] <= Rect.Right())
            {
               // Visualizza punto
               if (Mode == PREVIEW)
               {
                  if (acedGrDraw(pt, pt, AutoCADColorIndex, 0) != RTNORM) AfxThrowUserException();
               }
               else
                  if (gsc_insert_point(pt, Layer, &Color, 0.0) == GS_BAD)
                     AfxThrowUserException();

               StatusLineMsg.Set(++i); // "%ld celle elaborate."

               pt[X] += grid.dx; // avanza con la X
            }
               
            pt[X] = Rect.BottomLeft.point[X];
            pt[Y] += grid.dy; // avanza con la Y
         }

         acutPrintf(GS_LFSTR);
         acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
         acutPrintf(GS_LFSTR);
      }   
      else // devo leggere il db
         if (SQLMAPFmt)
         {
            if (gsc_GridDisplayPtsMAPSQL(this, pt1, pt2, pAttribZ, pSQLCond,
                                         Layer, Color, Centroid, Mode) == GS_BAD)
               AfxThrowUserException();
         }
         else
            if (gsc_GridDisplayPtsSQL(this, pt1, pt2, pAttribZ, pSQLCond,
                                      Layer, Color, Centroid, Mode) == GS_BAD)
               AfxThrowUserException();
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   gsc_endTransaction();
   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::DisplayPts                     external> */
/*+
   Funzione per visualizzare la griglia tramite dei punti delle celle.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_LONG_BTREE *pKeyList; Lista dei codici delle celle da evidenziare
   C_STRING *pAttribZ;     Opzionale; Nome dell'attributo da utilizzare per leggere la
                           coordinata Z. Se = NULL i punti saranno con Z = 0 (default = NULL)
   C_FAS *pFAS;            Opzionale; Caratteristiche grafiche della visualizzazione
                           (default = NULL)
   bool Centroid;          Flag; Se = TRUE indica che i punti saranno generati nel centroide
                           delle celle, altrimenti i punti saranno relativi all'angolo in
                           basso a sinistra di ciascuna cella (default = FALSE)
   int Mode;               Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayPts(C_LONG_BTREE *pKeyList, C_STRING *pAttribZ,
                        C_FAS *pFAS, bool Centroid, int Mode)
{
   ads_point pt;
   long      i = 0;
   TCHAR     *Layer;
   C_COLOR   Color;
   C_BLONG   *pItem = (C_BLONG *) pKeyList->go_top();
   int       AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer) == GS_BAD) return GS_BAD;
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      if (!pAttribZ) // non devo leggere il db
         while (pItem)
         {
            // Mi ricavo le coordinate del punto
            if (grid.key2pt(pItem->get_key(), pt, Centroid) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Visualizza punto
            if (Mode == PREVIEW)
            {
               if (acedGrDraw(pt, pt, AutoCADColorIndex, 0) != RTNORM) AfxThrowUserException();
            }
            else
               if (gsc_insert_point(pt, Layer, &Color, 0.0) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pItem = (C_BLONG *) pKeyList->go_next();
         }
      else // devo leggere il db
      {
         C_PREPARED_CMD_LIST CmdList;
         C_RB_LIST           ColValues;
         presbuf             pRbZValue = NULL;

         // Preparo i comandi di lettura dei dati della classe dal temp/old
         if (prepare_data(CmdList) == GS_BAD) return GS_BAD;

         while (pItem)
         {
            // Mi ricavo le coordinate del punto
            if (grid.key2pt(pItem->get_key(), pt, Centroid) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Lettura della quota
            if (query_data(pItem->get_key(), ColValues, &CmdList) == GS_BAD) AfxThrowUserException();
            if (!pRbZValue)
               if (!(pRbZValue = ColValues.CdrAssoc(pAttribZ->get_name())))
                  AfxThrowUserException();

            if (gsc_rb2Dbl(pRbZValue, &(pt[Z])) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Visualizza punto
            if (Mode == PREVIEW)
            {
               if (acedGrDraw(pt, pt, AutoCADColorIndex, 0) != RTNORM) AfxThrowUserException();
            }
            else
               if (gsc_insert_point(pt, Layer, &Color, 0.0) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pItem = (C_BLONG *) pKeyList->go_next();
         }
      }

      StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   gsc_endTransaction();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_DisplayGridCells                       external> */
/*+
   Funzione LISP per visualizzare la griglia tramite le superfici delle celle.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   (<prj><cls><sub>[<pt1><pt2>[<AttribZ>[SQLCond[HatchColor[Layer[Hatch[Display mode]]]]]])

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_DisplayGridCells(void)
{
   presbuf   arg = acedGetArgs();
   ads_point pt1, pt2;
   C_STRING  AttribZ, SQLCond;
   C_FAS     FAS;
   int       prj, cls, sub, DisplayMode = PREVIEW;
   C_CGRID   *pCls;
   bool      pt1Exist = FALSE, Location = FALSE, ChangeFAS = FALSE;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   if (arg)
   {
      // punto inferiore della finestra
      if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
      if ((arg = arg->rbnext))
      {
         if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
         if ((arg = arg->rbnext))
         {  // nome dell'attributo contenente il valore Z dei punti
            if (arg->restype == RTSTR) AttribZ = arg->resval.rstring;
            if ((arg = arg->rbnext))
            {  // condizione SQL
               if (arg->restype == RTSTR) SQLCond = arg->resval.rstring;
               if ((arg = arg->rbnext))
               {  // codice colore
                  if (FAS.hatch_color.setResbuf(arg) == GS_BAD) FAS.hatch_color.setByLayer();
                  else ChangeFAS = TRUE;

                  if ((arg = arg->rbnext))
                  {  // nome del piano
                     if (arg->restype == RTSTR)
                     { 
                        gsc_strcpy(FAS.hatch_layer, arg->resval.rstring, MAX_LEN_LAYERNAME);
                        ChangeFAS = TRUE;
                     }

                     if ((arg = arg->rbnext))
                     {  // nome del riempimento
                        if (arg->restype == RTSTR)
                        {
                           gsc_strcpy(FAS.hatch, arg->resval.rstring, MAX_LEN_HATCHNAME);
                           ChangeFAS = TRUE;
                        }

                        if ((arg = arg->rbnext))                           
                           // Modalità di visualizzazione
                           gsc_rb2Int(arg, &DisplayMode);
                     }
                  }
               }
            }
         }
      }
   }

   if (DisplayMode == PREVIEW) FAS.color = FAS.hatch_color;
   else if (gsc_strlen(FAS.hatch) == 0) FAS.color = FAS.hatch_color;

   if (!(AttribZ.get_name()))
   { // griglia 2D
      if (pCls->DisplayCells((Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                             &SQLCond, FALSE, // ACCESS Format
                             (ChangeFAS) ? &FAS : NULL, DisplayMode) == GS_BAD)
      return RTERROR;
   }
   else // griglia 3D
      if (pCls->DisplayCells(AttribZ, (Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                             &SQLCond, FALSE, // ACCESS Format
                             (ChangeFAS) ? &FAS : NULL, DisplayMode) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}

            
/*********************************************************/
/*.doc gsc_GridDisplayCellsSQL                           */
/*+
   Funzione per visualizzare la griglia tramite superfici piane
   quadrate che soddisfano una condizione SQL in formato ACCESS.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_CGRID              *pCls;
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   const TCHAR *Layer;  Nome del piano
   C_COLOR &Color;      Codice colore
   const TCHAR *Hatch;  Nome del riempimento
   int HatchColor;      Codice colre riempimento
   double Scale;        Scala del riempimento
   double rotation;     Rotazione del riempimento
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_GridDisplayCellsSQL(C_CGRID *pCls, ads_point pt1, ads_point pt2,
                            C_STRING *pSQLCond,
                            const TCHAR *Layer, C_COLOR &Color, 
                            const TCHAR *Hatch, C_COLOR HatchColor,
                            double HatchScale, double HatchRotation,
                            int Mode)
{
   C_GRID         *pInfoGrid = pCls->ptr_grid();
   C_RECT         Rect;
   ads_point      pt, ptRect2;
   long           Key, i = 0;
   C_PREPARED_CMD Cmd;
   C_RB_LIST      ColValues;
   long           KeyMin, KeyMax;
   ads_point      _pt2;
   _RecordsetPtr  pRs;
   presbuf        pKeyValue;
   bool           FirstTime = TRUE;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       pInfoGrid->key2pt(Key, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);
   ptRect2[Z] = 0.0;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // preparo comando per lettura record nella riga
      if (pCls->prepare_data_on_row(Cmd, NULL, pSQLCond->get_name()) == GS_BAD) // Where
         AfxThrowUserException();
      _pt2[X] = Rect.TopRight.point[X]; // estremo destro

      // Ciclo sulla Y
      while (pt[Y] <= Rect.Top())
      {
         // imposto id inferiore e superiore
         if (pInfoGrid->pt2key(pt, &KeyMin) == GS_BAD) AfxThrowUserException();
         _pt2[Y] = pt[Y];
         if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) AfxThrowUserException();
         ptRect2[Y] = pt[Y] + pInfoGrid->dy; // Y riga superiore

         if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs) == GS_BAD)
            AfxThrowUserException();

         // Ciclo sulla X
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) AfxThrowUserException();

            if (FirstTime)
            {
               FirstTime = FALSE;
               pKeyValue = ColValues.CdrAssoc(pCls->ptr_info()->key_attrib.get_name());
            }

            if (gsc_rb2Lng(pKeyValue, &Key) == GS_BAD) Key = 0;
            if (pInfoGrid->key2pt(Key, pt, FALSE) == GS_BAD) AfxThrowUserException();
            ptRect2[X] = pt[X] + pInfoGrid->dx; // X colonna superiore

            if (Mode != PREVIEW && gsc_strlen(Hatch) > 0)
            {
               // Visualizza solo il riempimento
               if (gsc_insert_hatch(pt, ptRect2, Hatch, Layer, 
                                    HatchScale, HatchRotation, &HatchColor) != GS_GOOD)
                  AfxThrowUserException();
            }
            else
               // Visualizza rettangolo
               if (gsc_insert_rectangle(pt, ptRect2, Layer, &Color, Mode) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pt[X] += pInfoGrid->dx; // avanza con la X
            gsc_Skip(pRs);
         }
         gsc_DBCloseRs(pRs);
            
         pt[X] = Rect.BottomLeft.point[X];
         pt[Y] += pInfoGrid->dy; // avanza con la Y
      }
   } // fine try

   catch (...) // any type of exception
   {
      return GS_BAD;
   }

   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}
int gsc_GridDisplayCellsMAPSQL(C_CGRID *pCls, ads_point pt1, ads_point pt2,
                               C_STRING *pSQLCond,
                               const TCHAR *Layer, C_COLOR &Color, 
                               const TCHAR *Hatch, C_COLOR &HatchColor,
                               double HatchScale, double HatchRotation,
                               int Mode)
{
   C_GRID      *pInfoGrid = pCls->ptr_grid();
   C_RECT      Rect;
   ads_point   pt, ptRect2;
   long        Key, i = 0;
   CAsiSession *pSession;
   CAsiExecStm *pStm;
   CAsiCsr     *pCsr;
   CAsiData    *pData;
   CAsiRow     *pRow;
   long        KeyMin, KeyMax;
   ads_point   _pt2;
   resbuf      RbKeyValue;
   C_STRING    SQLwhat, StmPrefix;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       pInfoGrid->key2pt(Key, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);
   ptRect2[Z] = 0.0;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // preparo comando per lettura record nella riga
      if (gsc_GridPrepare_data_on_row_map_fmt_1(pCls,
                                                SQLwhat.get_name(),   // What
                                                (pSQLCond) ? pSQLCond->get_name() : NULL, // Where
                                                StmPrefix, &pSession) == GS_BAD)
         AfxThrowUserException();
      _pt2[X] = Rect.TopRight.point[X]; // estremo destro

      // Ciclo sulla Y
      while (pt[Y] <= Rect.Top())
      {
         // imposto id inferiore e superiore
         if (pInfoGrid->pt2key(pt, &KeyMin) == GS_BAD) AfxThrowUserException();
         _pt2[Y] = pt[Y];
         if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) AfxThrowUserException();
         ptRect2[Y] = pt[Y] + pInfoGrid->dy; // Y riga superiore

         if (gsc_GridPrepare_data_on_row_map_fmt_2(pCls, StmPrefix, KeyMin, KeyMax,
                                                   pSession, &pStm, 
                                                   &pCsr) == GS_BAD)
            AfxThrowUserException();

         if (pCsr->Open() != kAsiGood)
         {
            gsc_printASIErr(pCsr);
            AfxThrowUserException();
         }

         // Ciclo sulla X
         while (pCsr->Fetch() == kAsiGood)
         {
            if ((pRow = pCsr->getCurrentRow()) == NULL)
               { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }

            if ((pData = (*pRow)[0].getData()) == NULL) // Key_attrib
               { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }
            if (gsc_ASIData2Rb(pData, &RbKeyValue) == GS_BAD)
               { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }

            if (gsc_rb2Lng(&RbKeyValue, &Key) == GS_BAD) Key = 0;
            if (pInfoGrid->key2pt(Key, pt, FALSE) == GS_BAD) AfxThrowUserException();
            ptRect2[X] = pt[X] + pInfoGrid->dx; // X colonna superiore

            if (Mode != PREVIEW && gsc_strlen(Hatch) > 0)
            {
               // Visualizza solo il riempimento
               if (gsc_insert_hatch(pt, ptRect2, Hatch, Layer, 
                                    HatchScale, HatchRotation, &HatchColor) != GS_GOOD)
                  AfxThrowUserException();
            }
            else
               // Visualizza rettangolo
               if (gsc_insert_rectangle(pt, ptRect2, Layer, &Color, Mode) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pt[X] += pInfoGrid->dx; // avanza con la X
         }
         pCsr->Close();
         delete pCsr;
         gsc_ASITermStm(&pStm);
            
         pt[X] = Rect.BottomLeft.point[X];
         pt[Y] += pInfoGrid->dy; // avanza con la Y
      }
   } // fine try

   catch (...) // any type of exception
   {
      if (pCsr) { pCsr->Close(); delete pCsr; }
      gsc_ASITermStm(&pStm);
      gsc_ASITermSession(&pSession);

      return GS_BAD;
   }

   gsc_ASITermSession(&pSession);

   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::DisplayCells                   external> */
/*+
   Funzione per visualizzare la griglia tramite superfici piane
   quadrate.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   bool SQLMAPFmt;      Opzionale usato se pSQLCond<>NULL; indica se la sintassi 
                        della condizione SQL è nel formato di Autocad MAP oppure
                        se ACCESS (default = TRUE)
   C_FAS *pFAS;         Opzionale; Caratteristiche grafiche della visualizzazione
                        (default = NULL)
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayCells(ads_point pt1, ads_point pt2, C_STRING *pSQLCond, bool SQLMAPFmt,
                          C_FAS *pFAS, int Mode)
{
   C_RECT    Rect;
   ads_point pt, ptRect2;
   long      Key, i = 0;
   TCHAR     *Layer, *Hatch;
   C_COLOR   Color, HatchColor;
   double    HatchScale, HatchRotation;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer, &Hatch, 
                       &HatchColor, &HatchScale, &HatchRotation) == GS_BAD)
      return GS_BAD;
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare
   if (grid.pt2key(Rect.BottomLeft.point, &Key) == GS_BAD || 
       grid.key2pt(Key, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);
   ptRect2[Z] = 0.0;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      if (!pSQLCond || pSQLCond->len() == 0) // non devo leggere il db
      {
         // Ciclo sulla Y
         while (pt[Y] <= Rect.Top())
         {
            ptRect2[Y] = pt[Y] + grid.dy; // Y riga superiore
            // Ciclo sulla X
            while (pt[X] <= Rect.Right())
            {
               ptRect2[X] = pt[X] + grid.dx; // X colonna superiore

               if (Mode != PREVIEW && gsc_strlen(Hatch) > 0)
               {
                  // Visualizza solo il riempimento
                  if (gsc_insert_hatch(pt, ptRect2, Hatch, Layer, 
                                       HatchScale, HatchRotation, &HatchColor) != GS_GOOD)
                     AfxThrowUserException();
               }
               else
                  // Visualizza rettangolo
                  if (gsc_insert_rectangle(pt, ptRect2, Layer, &Color, Mode) == GS_BAD)
                     AfxThrowUserException();

               StatusLineMsg.Set(++i); // "%ld celle elaborate."

               pt[X] += grid.dx; // avanza con la X
            }
               
            pt[X] = Rect.BottomLeft.point[X];
            pt[Y] += grid.dy; // avanza con la Y
         }

         acutPrintf(GS_LFSTR);
         acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
         acutPrintf(GS_LFSTR);
      }
      else // devo leggere il db
         if (SQLMAPFmt)
         {
            if (gsc_GridDisplayCellsMAPSQL(this, pt1, pt2, pSQLCond,
                                           Layer, Color, Hatch, HatchColor,
                                           HatchScale, HatchRotation, Mode) == GS_BAD)
               AfxThrowUserException();
         }
         else
            if (gsc_GridDisplayCellsSQL(this, pt1, pt2, pSQLCond,
                                        Layer, Color, Hatch, HatchColor,
                                        HatchScale, HatchRotation, Mode) == GS_BAD)
               AfxThrowUserException();
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      actrTransactionManager->abortTransaction();

      return GS_BAD;
   }

   gsc_endTransaction();
   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::DisplayCells                   external> */
/*+
   Funzione per visualizzare la griglia tramite superfici piane
   quadrate.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_LONG_BTREE *pKeyList; Lista dei codici delle celle da evidenziare
   C_STRING *pAttribZ;     Opzionale; Nome dell'attributo da utilizzare per leggere la
                           coordinata Z. Se = NULL i punti saranno con Z = 0 (default = NULL)
   C_FAS *pFAS;            Opzionale; Caratteristiche grafiche della visualizzazione
                           (default = NULL)
   int Mode;               Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayCells(C_LONG_BTREE *pKeyList, C_STRING *pAttribZ,
                          C_FAS *pFAS, int Mode)
{
   ads_point pt1, pt2;
   long      i = 0;
   TCHAR     *Layer, *Hatch;
   C_COLOR   Color, HatchColor;
   double    HatchScale, HatchRotation;
   C_BLONG   *pItem = (C_BLONG *) pKeyList->go_top();
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer, &Hatch, 
                       &HatchColor, &HatchScale, &HatchRotation) == GS_BAD)
      return GS_BAD;
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      if (!pAttribZ) // non devo leggere il db
         while (pItem)
         {
            // Mi ricavo le coordinate del punto
            if (grid.key2pt(pItem->get_key(), pt1, FALSE) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            pt2[Y] = pt1[Y] + grid.dy; // Y riga superiore
            pt2[X] = pt1[X] + grid.dx; // X colonna superiore

            if (Mode != PREVIEW && gsc_strlen(Hatch) > 0)
            {
               // Visualizza solo il riempimento
               if (gsc_insert_hatch(pt1, pt2, Hatch, Layer, 
                                    HatchScale, HatchRotation, &HatchColor) != GS_GOOD)
                  AfxThrowUserException();
            }
            else
               // Visualizza rettangolo
               if (gsc_insert_rectangle(pt1, pt2, Layer, &Color, Mode) == GS_BAD)
                  AfxThrowUserException();

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pItem = (C_BLONG *) pKeyList->go_next();
         }
      else // devo leggere il db
      {
         C_PREPARED_CMD_LIST CmdList;
         C_RB_LIST           ColValues;
         presbuf             pRbZValue = NULL;
         ads_point           pt3, pt4;
         long                Col, Row;
         int                 AutoCADColorIndex;

         if (Mode == PREVIEW)
            if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

         // Preparo i comandi di lettura dei dati della classe dal temp/old
         if (prepare_data(CmdList) == GS_BAD) return GS_BAD;

         while (pItem)
         {
            Col = grid.getColumn(pItem->get_key());
            Row = grid.getRow(pItem->get_key());

            if ((Col > grid.nx - 1) || (Row > grid.ny - 1))
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Mi ricavo le coordinate del punto1
            if (grid.key2pt(pItem->get_key(), pt1, FALSE) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }
            if (query_data(pItem->get_key(), ColValues, &CmdList) == GS_BAD) AfxThrowUserException();
            if (!pRbZValue)
               if (!(pRbZValue = ColValues.CdrAssoc(pAttribZ->get_name())))
                  AfxThrowUserException();
            if (gsc_rb2Dbl(pRbZValue, &(pt1[Z])) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Mi ricavo le coordinate del punto2
            pt2[Y] = pt1[Y];
            pt2[X] = pt1[X] + grid.dx; // X colonna superiore
            if (query_data(pItem->get_key() + 1, ColValues, &CmdList) == GS_BAD ||
                gsc_rb2Dbl(pRbZValue, &(pt2[Z])) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Mi ricavo le coordinate del punto3
            pt3[Y] = pt1[Y] + grid.dy; // Y riga superiore
            pt3[X] = pt1[X] + grid.dx; // X colonna superiore
            if (query_data(pItem->get_key() + 1 + grid.nx, ColValues, &CmdList) == GS_BAD ||
                gsc_rb2Dbl(pRbZValue, &(pt3[Z])) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // Mi ricavo le coordinate del punto4
            pt4[Y] = pt1[Y] + grid.dy; // Y riga superiore
            pt4[X] = pt1[X];
            if (query_data(pItem->get_key() + grid.nx, ColValues, &CmdList) == GS_BAD ||
                gsc_rb2Dbl(pRbZValue, &(pt4[Z])) == GS_BAD)
            {
               pItem = (C_BLONG *) pKeyList->go_next();
               continue;
            }

            // disegna i triangoli
            if (Mode == PREVIEW)
            {
               if (acedGrDraw(pt1, pt2, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
               if (acedGrDraw(pt1, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
               if (acedGrDraw(pt1, pt4, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
               if (acedGrDraw(pt4, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
            }
            else
            {
               if (gsc_insert_3dFace(pt1, pt2, pt3, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;
               if (gsc_insert_3dFace(pt1, pt3, pt4, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;
            }

            StatusLineMsg.Set(++i); // "%ld celle elaborate."

            pItem = (C_BLONG *) pKeyList->go_next();
         }
      }

      StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   gsc_endTransaction();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::get2DCells                   external> */
/*+
   Funzione per creare un vettore di polilinee (rettangoil) che descrivono
   graficamente l'occupazione delle celle aventi codici nella lista pKeyList.
   N.B.: Gli oggetti grafici generati non sono inseriti in grafica e 
   vanno rilasciati dalla funzione chiamante.
   Parametri:
   C_LONG_BTREE *pKeyList;          Lista dei codici delle celle da evidenziare
   AcDbEntityPtrArray &EntitySet;   Lista dei rettangoli

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::get2DCells(C_LONG_BTREE *pKeyList, AcDbEntityPtrArray &EntitySet)
{
   ads_point Pt1, Pt2;
   long      i = 0;
   C_BLONG   *pItem = (C_BLONG *) pKeyList->go_top();
   C_POINT_LIST PointList;
   AcDbCurve *pEnt;

   EntitySet.removeAll();
   while (pItem)
   {
      // Mi ricavo le coordinate del punto
      if (grid.key2pt(pItem->get_key(), Pt1, FALSE) == GS_BAD)
      {
         pItem = (C_BLONG *) pKeyList->go_next();
         continue;
      }

      Pt2[Y] = Pt1[Y] + grid.dy; // Y riga superiore
      Pt2[X] = Pt1[X] + grid.dx; // X colonna superiore

      PointList.remove_all();
      if (PointList.add_point(Pt1) == NULL) return GS_BAD;
      if (PointList.add_point(Pt2[X], Pt1[Y], Pt1[Z]) == NULL) return GS_BAD;
      if (PointList.add_point(Pt2[X], Pt2[Y], Pt1[Z]) == NULL) return GS_BAD;
      if (PointList.add_point(Pt1[X], Pt2[Y], Pt1[Z]) == NULL) return GS_BAD;
      if (PointList.add_point(Pt1) == NULL) return GS_BAD;
      if ((pEnt = PointList.toPolyline()) == NULL) return GS_BAD;
      pEnt->close();
      EntitySet.append(pEnt);

      pItem = (C_BLONG *) pKeyList->go_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Display3DGridCells                    <internal> */
/*+
   Funzione di appoggio alla C_CGRID::DisplayCells che visualizza
   una riga della griglia tramite superfici 3D triangolari. 
   La funzione riceve 2 recordset che contengono
   tutti i record di 2 righe adiacenti della griglia. pRs1 contiene le
   celle della riga inferiore e pRs2 quelle della riga superiore.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   _RecordsetPtr &pRs1;    Recordset con tutte le celle della prima riga
   _RecordsetPtr &pRs2;    Recordset con tutte le celle della seconda riga
   C_RB_LIST &ColValues;   Lista di resbuf per lettura valori cella (già inizializzata)
   presbuf   pZValue;      puntatore a resbuf contenente la quota (già inizializzato)
   double    Xmin;         coordinata X corrispondente al primo record di pRs1
   double    Ymin;         coordinata Y corrispondente al primo record di pRs1
   C_GRID    *pGrid;       puntatore alle caratteristiche della griglia
   long      *i;           contatore
   const TCHAR *Layer;     Layer
   C_COLOR &Color;         Colore
   int Mode;               Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_Display3DGridCells(_RecordsetPtr &pRs1, _RecordsetPtr &pRs2,
                           C_RB_LIST &ColValues, presbuf pZValue,
                           double Xmin, double Ymin, C_GRID *pGrid,
                           long *i, const TCHAR *Layer, C_COLOR &Color, int Mode = PREVIEW)
{
   ads_point pt1, pt2, pt3, pt4;
   int       ValidPt1, ValidPt2, ValidPt3, ValidPt4;
   int       AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   
   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

   if (gsc_isEOF(pRs1) == GS_GOOD || gsc_isEOF(pRs2) == GS_GOOD) return GS_GOOD;

   pt1[X] = pt4[X] = Xmin;
   pt2[X] = pt3[X] = Xmin + pGrid->dx;
   pt1[Y] = pt2[Y] = Ymin;
   pt3[Y] = pt4[Y] = Ymin + pGrid->dy;

   // leggo prima cella
   if (gsc_DBReadRow(pRs1, ColValues) == GS_BAD) return GS_BAD;
   ValidPt1 = gsc_rb2Dbl(pZValue, &(pt1[Z]));
   gsc_Skip(pRs1);

   if (gsc_DBReadRow(pRs2, ColValues) == GS_BAD) return GS_BAD;
   ValidPt4 = gsc_rb2Dbl(pZValue, &(pt4[Z]));
   gsc_Skip(pRs2);

   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Ciclo sulla X
   while (gsc_isEOF(pRs1) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs1, ColValues) == GS_BAD) return GS_BAD;
      ValidPt2 = gsc_rb2Dbl(pZValue, &(pt2[Z]));
      gsc_Skip(pRs1);

      if (gsc_DBReadRow(pRs2, ColValues) == GS_BAD) return GS_BAD;
      ValidPt3 = gsc_rb2Dbl(pZValue, &(pt3[Z]));
      gsc_Skip(pRs2);

      // disegna i triangoli
      if (Mode == PREVIEW)
      {
         if (ValidPt1 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (acedGrDraw(pt1, pt2, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt3 == GS_GOOD)
               if (acedGrDraw(pt1, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (acedGrDraw(pt1, pt4, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
         }

         if (ValidPt4 == GS_GOOD && ValidPt3 == GS_GOOD)
            if (acedGrDraw(pt4, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
      }
      else
         if (ValidPt1 == GS_GOOD && ValidPt3 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt2, pt3, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt3, pt4, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;
         }

      StatusLineMsg.Set(++(*i)); // "%ld celle elaborate."

      // avanza con la X
      pt1[X] += pGrid->dx; pt1[Z] = pt2[Z]; ValidPt1 = ValidPt2;
      pt4[X] = pt1[X]; pt4[Z] = pt3[Z]; ValidPt4 = ValidPt3;
      pt2[X] = pt3[X] = pt1[X] + pGrid->dx;
   }
   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Display3DGridCells                    <internal> */
/*+
   Funzione di appoggio alla C_CGRID::DisplayCells che visualizza
   una riga della griglia tramite superfici 3D triangolari. 
   La funzione riceve 1 recordset che contiene solo i record di una
   riga che soddisfano un certo criterio.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_CGRID *pClass;        classe griglia
   _RecordsetPtr &pRs;     Recordset con le celle della prima riga che soddisfano
                           una condizione SQL
   C_RB_LIST &ColValues;   Lista di resbuf per lettura valori cella (già inizializzata)
   presbuf   pIdValue;     puntatore a resbuf contenente il valore chiave (già inizializzato)
   presbuf   pZValue;      puntatore a resbuf contenente la quota (già inizializzato)
   C_PREPARED_CMD_LIST &CmdList; Comando preparato per l'interrogazione delle
                                 celle adiacenti quella interessata                                    
   C_GRID    *pGrid;       puntatore alle caratteristiche della griglia
   long      *i;           contatore
   const TCHAR *Layer;     Layer
   C_COLOR &Color;         Colore
   int Mode;               Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_Display3DGridCells(C_CGRID *pClass, _RecordsetPtr &pRs, C_RB_LIST &ColValues,
                           presbuf pIdValue, presbuf pZValue, 
                           C_PREPARED_CMD_LIST &CmdList, C_RB_LIST &AdjColValues,
                           presbuf pAdjZValue,
                           long *i, const TCHAR *Layer, C_COLOR &Color, int Mode = PREVIEW)
{
   long      Key;
   ads_point pt1, pt2, pt3, pt4;
   int       ValidPt1, ValidPt2, ValidPt3, ValidPt4;
   bool      NoLeftCell = FALSE;
   C_GRID    *pGrid = pClass->ptr_grid();
   double    LimitX = pGrid->x + pGrid->dx * (pGrid->nx - 1);
   double    LimitY = pGrid->y + pGrid->dy * (pGrid->ny - 1);
   int       AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;
      if (gsc_rb2Lng(pIdValue, &Key) == GS_BAD) return GS_BAD;
      // Mi ricavo le coordinate del punto (in basso a sinistra)
      // della cella da visualizzare
      if (pGrid->key2pt(Key, pt1, FALSE) == GS_BAD) return GS_BAD;
      ValidPt1 = gsc_rb2Dbl(pZValue, &(pt1[Z]));

      pt4[X] = pt1[X];
      pt2[Y] = pt1[Y];
      pt2[X] = pt3[X] = pt1[X] + pGrid->dx;
      pt3[Y] = pt4[Y] = pt1[Y] + pGrid->dy;
      // leggo le quote dalle celle adiacenti
      // se NON ci sono celle a sinistra
      if (pt3[X] > LimitX)
      {
         gsc_Skip(pRs);
         continue;
      }
      else // leggo da DB pt2
      {
         if (pClass->query_data(Key + 1, AdjColValues, &CmdList) == GS_BAD) return GS_BAD;
         ValidPt2 = gsc_rb2Dbl(pAdjZValue, &(pt2[Z]));
      }

      // se NON ci sono celle sopra
      if (pt3[Y] > LimitY)
         break;
      else
      {
         // leggo da DB pt4
         if (pClass->query_data(Key + pGrid->nx, AdjColValues, &CmdList) == GS_BAD)
            return GS_BAD;
         ValidPt4 = gsc_rb2Dbl(pAdjZValue, &(pt4[Z]));

         if (NoLeftCell) pt3[Z] = pt2[Z];
         else // leggo da DB pt3
         {
            if (pClass->query_data(Key + pGrid->nx + 1, AdjColValues, &CmdList) == GS_BAD)
               return GS_BAD;
            ValidPt3 = gsc_rb2Dbl(pAdjZValue, &(pt3[Z]));
         }
      }

      // disegna i triangoli
      if (Mode == PREVIEW)
      {
         if (ValidPt1 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (acedGrDraw(pt1, pt2, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt3 == GS_GOOD)
               if (acedGrDraw(pt1, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (acedGrDraw(pt1, pt4, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
         }

         if (ValidPt4 == GS_GOOD && ValidPt3 == GS_GOOD)
            if (acedGrDraw(pt4, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
      }
      else
         if (ValidPt1 == GS_GOOD && ValidPt3 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt2, pt3, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt3, pt4, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;
         }

      StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."

      gsc_Skip(pRs);
   }
   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."
    
   return GS_GOOD;
}
int gsc_Display3DGridCellsMAPSQL(C_CGRID *pClass, CAsiCsr *pCsr, 
                                 C_PREPARED_CMD_LIST &CmdList, C_RB_LIST &AdjColValues,
                                 presbuf pAdjZValue,
                                 long *i, const TCHAR *Layer, C_COLOR &Color, int Mode = PREVIEW)
{
   long      Key;
   ads_point pt1, pt2, pt3, pt4;
   int       ValidPt1, ValidPt2, ValidPt3, ValidPt4;
   bool      NoLeftCell = FALSE;
   C_GRID    *pGrid = pClass->ptr_grid();
   double    LimitX = pGrid->x + pGrid->dx * (pGrid->nx - 1);
   double    LimitY = pGrid->y + pGrid->dy * (pGrid->ny - 1);
   CAsiData  *pData;
   CAsiRow   *pRow;
   resbuf    RbKeyValue, RbZValue;
   int       AutoCADColorIndex;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   if (Mode == PREVIEW)
      if (Color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) AutoCADColorIndex = COLOR_BYLAYER;

   while (pCsr->Fetch() == kAsiGood)
   {
      if ((pRow = pCsr->getCurrentRow()) == NULL) return GS_BAD;
      if ((pData = (*pRow)[0].getData()) == NULL) // Key_attrib
         { GS_ERR_COD = eGSReadRow; return GS_BAD; }
      if (gsc_ASIData2Rb(pData, &RbKeyValue) == GS_BAD)
         { GS_ERR_COD = eGSReadRow; return GS_BAD; }
      if (gsc_rb2Lng(&RbKeyValue, &Key) == GS_BAD) return GS_BAD;
      // Mi ricavo le coordinate del punto (in basso a sinistra)
      // della cella da visualizzare
      if (pGrid->key2pt(Key, pt1, FALSE) == GS_BAD) return GS_BAD;

      if ((pData = (*pRow)[1].getData()) == NULL) // AttribZ
         { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }
      if (gsc_ASIData2Rb(pData, &RbZValue) == GS_BAD)
         { GS_ERR_COD = eGSReadRow; AfxThrowUserException(); }
      ValidPt1 = gsc_rb2Dbl(&RbZValue, &(pt1[Z]));

      pt4[X] = pt1[X];
      pt2[Y] = pt1[Y];
      pt2[X] = pt3[X] = pt1[X] + pGrid->dx;
      pt3[Y] = pt4[Y] = pt1[Y] + pGrid->dy;
      // leggo le quote dalle celle adiacenti
      // se NON ci sono celle a sinistra
      if (pt3[X] > LimitX)
         continue;
      else // leggo da DB pt2
      {
         if (pClass->query_data(Key + 1, AdjColValues, &CmdList) == GS_BAD) return GS_BAD;
         ValidPt2 = gsc_rb2Dbl(pAdjZValue, &(pt2[Z]));
      }

      // se NON ci sono celle sopra
      if (pt3[Y] > LimitY)
         break;
      else
      {
         // leggo da DB pt4
         if (pClass->query_data(Key + pGrid->nx, AdjColValues, &CmdList) == GS_BAD)
            return GS_BAD;
         ValidPt4 = gsc_rb2Dbl(pAdjZValue, &(pt4[Z]));

         if (NoLeftCell) pt3[Z] = pt2[Z];
         else // leggo da DB pt3
         {
            if (pClass->query_data(Key + pGrid->nx + 1, AdjColValues, &CmdList) == GS_BAD)
               return GS_BAD;
            ValidPt3 = gsc_rb2Dbl(pAdjZValue, &(pt3[Z]));
         }
      }

      // disegna i triangoli
      if (Mode == PREVIEW)
      {
         if (ValidPt1 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (acedGrDraw(pt1, pt2, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt3 == GS_GOOD)
               if (acedGrDraw(pt1, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (acedGrDraw(pt1, pt4, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
         }

         if (ValidPt4 == GS_GOOD && ValidPt3 == GS_GOOD)
            if (acedGrDraw(pt4, pt3, AutoCADColorIndex, 0) != RTNORM) return GS_BAD;
      }
      else
         if (ValidPt1 == GS_GOOD && ValidPt3 == GS_GOOD)
         {
            if (ValidPt2 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt2, pt3, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;

            if (ValidPt4 == GS_GOOD)
               if (gsc_insert_3dFace(pt1, pt3, pt4, NULL, Layer, &Color) == GS_BAD)
                  return GS_BAD;
         }

      StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."
   }
   StatusLineMsg.End(gsc_msg(408), i); // "%ld celle elaborate."
    
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GridDisplay3DCellsSQL                         */
/*+
   Funzione per visualizzare la griglia tramite superfici superfici 3D
   triangolari che soddisfano una condizione SQL in formato ACCESS.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_CGRID              *pCls;
   C_STRING &AttribZ;   Nome dell'attributo da utilizzare per leggere la
                        coordinata Z.
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   const TCHAR *Layer;  Nome del piano
   C_COLOR &Color;      Codice colore
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_GridDisplay3DCellsSQL(C_CGRID *pCls, C_STRING &AttribZ,
                              ads_point pt1, ads_point pt2,
                              C_STRING *pSQLCond,
                              const TCHAR *Layer, C_COLOR &Color, int Mode)
{
   C_GRID              *pInfoGrid = pCls->ptr_grid();
   C_RECT              Rect;
   ads_point           pt, _pt2;
   C_PREPARED_CMD      Cmd;
   C_RB_LIST           ColValues;
   long                i = 0, KeyMin, KeyMax;
   _RecordsetPtr       pRs1;
   presbuf             pZValue;
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           AdjColValues;
   presbuf             pAdjZValue, pIdValue;

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       pInfoGrid->key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) return GS_BAD;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(CmdList) == GS_BAD) AfxThrowUserException();
      // Faccio una lettura solo per inizializzare <AdjColValues> e <pAdjZValue>
      if (pCls->query_data(1, AdjColValues, &CmdList) == GS_BAD) AfxThrowUserException();
      if (!(pAdjZValue = AdjColValues.CdrAssoc(AttribZ.get_name())))
            AfxThrowUserException();

      // preparo comando per lettura record nella riga
      if (pCls->prepare_data_on_row(Cmd, AttribZ.get_name(), pSQLCond->get_name()) == GS_BAD)
         AfxThrowUserException();
      if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
         AfxThrowUserException();

      if (gsc_InitDBReadRow(pRs1, ColValues) == GS_BAD) AfxThrowUserException();
      if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name(), FALSE)) || 
            !(pIdValue = ColValues.CdrAssoc(pCls->ptr_info()->key_attrib.get_name())))
            AfxThrowUserException();

      // Ciclo sulla Y
      do
      {
         if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
            AfxThrowUserException();

         if (gsc_Display3DGridCells(pCls, pRs1, ColValues, pIdValue, pZValue,
                                    CmdList, AdjColValues, pAdjZValue,
                                    &i, Layer, Color, Mode) == GS_BAD)
            AfxThrowUserException();
         gsc_DBCloseRs(pRs1);

         // imposto id inferiore e superiore
         KeyMin += pInfoGrid->nx;
         KeyMax += pInfoGrid->nx;
         pt[Y]  += pInfoGrid->dy;
      }
      while (pt[Y] <= Rect.Top());
   } // fine try

   catch (...) // any type of exception
   {
      return GS_BAD;
   }

   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}
int gsc_GridDisplay3DCellsMAPSQL(C_CGRID *pCls, C_STRING &AttribZ,
                                 ads_point pt1, ads_point pt2,
                                 C_STRING *pSQLCond,
                                 const TCHAR *Layer, C_COLOR &Color, int Mode)
{
   C_GRID              *pInfoGrid = pCls->ptr_grid();
   C_RECT              Rect;
   ads_point           pt, _pt2;
   long                i = 0, KeyMin, KeyMax;
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           AdjColValues;
   presbuf             pAdjZValue;
   CAsiSession         *pSession;
   CAsiExecStm         *pStm;
   CAsiCsr             *pCsr;
   C_STRING            SQLwhat, StmPrefix;

   SQLwhat = _T('\"');
   SQLwhat += AttribZ;
   SQLwhat += _T('\"');

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   pInfoGrid->getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (pInfoGrid->pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       pInfoGrid->key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (pInfoGrid->pt2key(_pt2, &KeyMax) == GS_BAD) return GS_BAD;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   try
   {
      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(CmdList) == GS_BAD) AfxThrowUserException();
      // Faccio una lettura solo per inizializzare <AdjColValues> e <pAdjZValue>
      if (pCls->query_data(1, AdjColValues, &CmdList) == GS_BAD) AfxThrowUserException();
      if (!(pAdjZValue = AdjColValues.CdrAssoc(AttribZ.get_name())))
            AfxThrowUserException();

      // preparo comando per lettura record nella riga
      if (gsc_GridPrepare_data_on_row_map_fmt_1(pCls,
                                                SQLwhat.get_name(),   // What
                                                (pSQLCond) ? pSQLCond->get_name() : NULL, // Where
                                                StmPrefix, &pSession) == GS_BAD)
         AfxThrowUserException();

      // Ciclo sulla Y
      do
      {
         if (gsc_GridPrepare_data_on_row_map_fmt_2(pCls, StmPrefix, KeyMin, KeyMax,
                                                   pSession, &pStm, 
                                                   &pCsr) == GS_BAD)
            AfxThrowUserException();
         if (pCsr->Open() != kAsiGood) AfxThrowUserException();

         if (gsc_Display3DGridCellsMAPSQL(pCls, pCsr,
                                          CmdList, AdjColValues, pAdjZValue,
                                          &i, Layer, Color, Mode) == GS_BAD)
            AfxThrowUserException();

         pCsr->Close();
         delete pCsr;
         gsc_ASITermStm(&pStm);

         // imposto id inferiore e superiore
         KeyMin += pInfoGrid->nx;
         KeyMax += pInfoGrid->nx;
         pt[Y]  += pInfoGrid->dy;
      }
      while (pt[Y] <= Rect.Top());
   } // fine try

   catch (...) // any type of exception
   {
      if (pCsr) { pCsr->Close(); delete pCsr; }
      gsc_ASITermStm(&pStm);
      gsc_ASITermSession(&pSession);

      return GS_BAD;
   }

   gsc_ASITermSession(&pSession);

   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::DisplayCells                   external> */
/*+
   Funzione per visualizzare la griglia tramite superfici 3D
   triangolari.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_STRING &AttribZ;   Nome dell'attributo da utilizzare per leggere la
                        coordinata Z.
   ads_point pt1;       Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                        in cui visualizzare la griglia, Rappresenta il punto 
                        a sinistra in basso (default = NULL)
   ads_point pt2;       Rappresenta il punto a destra in alto (default = NULL)
   C_STRING *pSQLCond;  Opzionale; Condizione SQL per filtrare la visualizzazione.
                        Se = NULL nessun filtro (default = NULL)
   bool SQLMAPFmt;      Opzionale usato se pSQLCond<>NULL; indica se la sintassi 
                        della condizione SQL è nel formato di Autocad MAP oppure
                        se ACCESS (default = TRUE)
   C_FAS *pFAS;         Opzionale; Caratteristiche grafiche della visualizzazione
                        (default = NULL)
   int Mode;            Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                        (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayCells(C_STRING &AttribZ, ads_point pt1, ads_point pt2,
                          C_STRING *pSQLCond, bool SQLMAPFmt, C_FAS *pFAS, int Mode)
{
   C_RECT         Rect;
   ads_point      pt, _pt2;
   TCHAR          *Layer;
   C_COLOR        Color;
   C_PREPARED_CMD Cmd;
   C_RB_LIST      ColValues;
   long           i = 0, KeyMin, KeyMax;
   _RecordsetPtr  pRs1;
   presbuf        pZValue;
   bool           OddRow = FALSE;

   if (gsc_setFAS4Grid(pFAS, Mode, &Color, &Layer) == GS_BAD) return GS_BAD;

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (grid.pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       grid.key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (grid.pt2key(_pt2, &KeyMax) == GS_BAD) return GS_BAD;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      if (!pSQLCond || pSQLCond->len() == 0) // non ci sono condizioni SQL
      {
         _RecordsetPtr pRs2;

         // preparo comando per lettura record nella riga
         if (prepare_data_on_row(Cmd, AttribZ.get_name()) == GS_BAD)
            AfxThrowUserException();
         if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
            AfxThrowUserException();

         if (gsc_InitDBReadRow(pRs1, ColValues) == GS_BAD) AfxThrowUserException();
         if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name())))
             AfxThrowUserException();

         // Ciclo sulla Y
         do
         {
            // imposto id inferiore e superiore
            KeyMin += grid.nx;
            KeyMax += grid.nx;

            if (OddRow) // leggere pRs1
            {
               gsc_DBCloseRs(pRs1);
               gsc_MoveFirst(pRs2);
               if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
                  AfxThrowUserException();
               if (gsc_Display3DGridCells(pRs2, pRs1, ColValues, pZValue,
                                          pt[X], pt[Y], ptr_grid(),
                                          &i, Layer, Color, Mode) == GS_BAD)
                  AfxThrowUserException();
            }
            else // leggere pRs2
            {
               gsc_DBCloseRs(pRs2);
               gsc_MoveFirst(pRs1);
               if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs2) == GS_BAD)
                  AfxThrowUserException();
               if (gsc_Display3DGridCells(pRs1, pRs2, ColValues, pZValue,
                                          pt[X], pt[Y], ptr_grid(),
                                          &i, Layer, Color, Mode) == GS_BAD)
                  AfxThrowUserException();
            }

            OddRow = !OddRow;
            pt[Y] += grid.dy;
         }
         while (pt[Y] <= Rect.Top());

         acutPrintf(GS_LFSTR);
         acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
         acutPrintf(GS_LFSTR);
      }
      else  // ci sono condizioni SQL
         if (SQLMAPFmt)
         {
            if (gsc_GridDisplay3DCellsMAPSQL(this, AttribZ, pt1, pt2, pSQLCond,
                                             Layer, Color, Mode) == GS_BAD)
               AfxThrowUserException();
         }
         else
            if (gsc_GridDisplay3DCellsSQL(this, AttribZ, pt1, pt2, pSQLCond,
                                          Layer, Color, Mode) == GS_BAD)
               AfxThrowUserException();
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   acutPrintf(GS_LFSTR);
   gsc_endTransaction();

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER CURVE DI LIVELLO
///////////////////////////////////////////////////////////////////////////////
/*********************************************************/
/*.doc gsc_getXY3DLine                        <external> */
/*+
   La funzione calcola la X e la Y di un segmento con punto 
   iniziale e finale noti e con quota nota.
   Parametri:
   ads_point p1;
   ads_point p2;
   double    Quote;
   ads_point pt;  punto di cui si deve calcolare la X e la Y

   Ritorna GS_GOOD in caso di successo, GS_BAD se non esiste soluzione,
   GS_CAN se ha soluzioni infinite.
-*/  
/*************************************************************/
int gsc_getXY3DLine(ads_point p1, ads_point p2, double Quote, ads_point pt)
{
   if (p1[Z] == p2[Z])
      return (p1[Z] == Quote) ? GS_CAN : GS_BAD;

   double factor =  (Quote - p1[Z]) / (p1[Z] - p2[Z]);

   // x = (x1-x2) (z-z1) / (z1-z2) + x1
   pt[X] = (p1[X] - p2[X]) * factor + p1[X];

   // y = (y1-y2) (z-z1) / (z1-z2) + y1
   pt[Y] = (p1[Y] - p2[Y]) * factor + p1[Y];

   pt[Z] = Quote;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DrawGridContours                   <internal> */
/*+
   La funzione disegna le curve di livello di una cella dividendola in 
   2 triangoli.
   Parametri:
   ads_point   pt1;         pt4---pt3
   ads_point   pt2;          |  /  |
   ads_point   pt3;          | /   |
   ads_point   pt4;         pt1---pt2
   double      Elevation;
   const TCHAR *Layer;      Layer
   C_COLOR     &Color;       Colore
   TCHAR       *LineType;   Tipo linea
   double      Scale;       Scala tipolinea
   double      Width;       Spessore
   int         Mode;        Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
   C_SELSET    *pSSOut;     Se Mode = EXTRACTION le polilinee saranno inserite nel gruppo
                            di selezione puntato da pSSOut se esistente (default = NULL)

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*************************************************************/
int gsc_DrawGridContours(ads_point pt1, ads_point pt2, ads_point pt3, ads_point pt4,
                         double Elevation,
                         const TCHAR *Layer, C_COLOR &Color, 
                         TCHAR *LineType, double Scale, double Width,
                         int Mode = PREVIEW, C_SELSET *pSSOut = NULL)
{
   ads_point Inters_1_2, Inters_2_3, Inters_3_1, Inters_3_4, Inters_4_1;
   int       Exist_1_2 = GS_BAD, Exist_2_3 = GS_BAD, Exist_3_1 = GS_BAD;
   int       Exist_3_4 = GS_BAD, Exist_4_1 = GS_BAD;
   ads_name  ent;

   if (!pt1 || !pt3) return GS_GOOD;

   // Considero la diagonale della cella
   if (pt3[Z] <= Elevation && pt1[Z] >= Elevation ||
       pt3[Z] >= Elevation && pt1[Z] <= Elevation)
   {
      Exist_3_1 = gsc_getXY3DLine(pt3, pt1, Elevation, Inters_3_1);

      /*
      if (Exist_3_1 == GS_CAN) // infiniti punti di intersezione
         // Disegno la linea che va da pt3 a pt1
         if (gsc_insert_lwpline(pt3, pt1, Layer, Color, LineType, Width, Mode) == GS_BAD)
            return GS_BAD;
      */
   }

   // Considero il triangolo       pt3
   //                            / |
   //                           /  |
   //                        pt1---pt2
   if (!pt2)
   {
      Exist_1_2 = GS_BAD;
      Exist_2_3 = GS_BAD;
   }
   else
   {
      if (pt1[Z] <= Elevation && pt2[Z] >= Elevation ||
          pt1[Z] >= Elevation && pt2[Z] <= Elevation)
         Exist_1_2 = gsc_getXY3DLine(pt1, pt2, Elevation, Inters_1_2);

      if (pt2[Z] <= Elevation && pt3[Z] >= Elevation ||
          pt2[Z] >= Elevation && pt3[Z] <= Elevation)
         Exist_2_3 = gsc_getXY3DLine(pt2, pt3, Elevation, Inters_2_3);
   }

   if (Exist_1_2 == GS_GOOD)
   {  // se esiste intersezione nei lati 1-2 e 2-3 e i 2 punti non sono coincidenti
      if (Exist_2_3 == GS_GOOD && !ads_2Dpoint_equal(Inters_1_2, Inters_2_3))
      {  // Disegno la linea che va da Inters_1_2 a Inters_2_3
         if (gsc_insert_lwpline(Inters_1_2, Inters_2_3, Layer, &Color, 
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }
      else // se esiste intersezione nei lati 1-2 e 3-1 e i 2 punti non sono coincidenti
      if (Exist_3_1 == GS_GOOD && !ads_2Dpoint_equal(Inters_1_2, Inters_3_1))
      {
         // Disegno la linea che va da Inters_1_2 a Inters_3_1
         if (gsc_insert_lwpline(Inters_1_2, Inters_3_1, Layer, &Color,
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }
   }
   else // se esiste intersezione nei lati 2-3 e 3-1 e i 2 punti non sono coincidenti
      if (Exist_2_3 == GS_GOOD && Exist_3_1 == GS_GOOD && !ads_2Dpoint_equal(Inters_2_3, Inters_3_1))
      {
         // Disegno la linea che va da Inters_2_3 a Inters_3_1             
         if (gsc_insert_lwpline(Inters_2_3, Inters_3_1, Layer, &Color,
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }

   // Considero il triangolo pt4---pt3
   //                         |  /
   //                         | /
   //                        pt1
   if (!pt4)
   {
      Exist_3_4 = GS_BAD;
      Exist_4_1 = GS_BAD;
   }
   else
   {
      if (pt3[Z] <= Elevation && pt4[Z] >= Elevation ||
          pt3[Z] >= Elevation && pt4[Z] <= Elevation)
         Exist_3_4 = gsc_getXY3DLine(pt3, pt4, Elevation, Inters_3_4);

      if (pt4[Z] <= Elevation && pt1[Z] >= Elevation ||
          pt4[Z] >= Elevation && pt1[Z] <= Elevation)
         Exist_4_1 = gsc_getXY3DLine(pt4, pt1, Elevation, Inters_4_1);
   }

   if (Exist_3_4 == GS_GOOD)
   {  // se esiste intersezione nei lati 3-4 e 4-1 e i 2 punti non sono coincidenti
      if (Exist_4_1 == GS_GOOD && !ads_2Dpoint_equal(Inters_3_4, Inters_4_1))
      {  // Disegno la linea che va da Inters_3_4 a Inters_4_1
         if (gsc_insert_lwpline(Inters_3_4, Inters_4_1, Layer, &Color,
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }
      else // se esiste intersezione nei lati 3-4 e 3-1 e i 2 punti non sono coincidenti
      if (Exist_3_1 == GS_GOOD && !ads_2Dpoint_equal(Inters_3_4, Inters_3_1))
      {
         // Disegno la linea che va da Inters_3_4 a Inters_3_1
         if (gsc_insert_lwpline(Inters_3_4, Inters_3_1, Layer, &Color,
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }
   }
   else // se esiste intersezione nei lati 4-1 e 3-1 e i 2 punti non sono coincidenti
      if (Exist_4_1 == GS_GOOD && Exist_3_1 == GS_GOOD && !ads_2Dpoint_equal(Inters_4_1, Inters_3_1))
      {
         // Disegno la linea che va da Inters_4_1 a Inters_3_1             
         if (gsc_insert_lwpline(Inters_4_1, Inters_3_1, Layer, &Color,
                                LineType, Scale, Width, Mode) == GS_BAD)
            return GS_BAD;
         if (pSSOut) { acdbEntLast(ent); pSSOut->add(ent); }
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_DisplayGridContours                <internal> */
/*+
   Funzione di appoggio alla C_CGRID::DisplayContours che visualizza
   una riga della griglia tramite curve di livello. 
   La funzione riceve 2 recordset che contengono
   tutti i record di 2 righe adiacenti della griglia. pRs1 contiene le
   celle della riga inferiore e pRs2 quelle della riga superiore.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   _RecordsetPtr &pRs1;    Recordset con tutte le celle della prima riga
   _RecordsetPtr &pRs2;    Recordset con tutte le celle della seconda riga
   C_RB_LIST &ColValues;   Lista di resbuf per lettura valori cella (già inizializzata)
   presbuf   pZValue;      puntatore a resbuf contenente la quota (già inizializzato)
   C_FAS_LIST &FAS_list;   Lista delle FAS per calcolare e disegnare le curve di livello
   double    Xmin;         coordinata X corrispondente al primo record di pRs1
   double    Ymin;         coordinata Y corrispondente al primo record di pRs1
   C_GRID    *pGrid;       puntatore alle caratteristiche della griglia
   long      *i;           contatore
   bool      NoDrawTop;    Flag, se = FALSE indica che si tratta della riga superiore
                           della parte di griglia da valutare
   int       Mode;         Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                           (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
   C_SELSET_LIST *pSSOutList; Se Mode = EXTRACTION le polilinee saranno inserite nel gruppo
                              di selezione uno per ogni FAS esistente (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_DisplayGridContours(_RecordsetPtr &pRs1, _RecordsetPtr &pRs2,
                            C_RB_LIST &ColValues, presbuf pZValue, C_FAS_LIST &FAS_list,
                            double Xmin, double Ymin, C_GRID *pGrid, long *i, bool NoDrawTop,                            
                            int Mode = PREVIEW, C_SELSET_LIST *pSSOutList = NULL)
{
   ads_point pt1, pt2, pt3, pt4;
   int       ValidPt1, ValidPt2, ValidPt3, ValidPt4;
   C_COLOR   Color;
   bool      NoDrawRight;
   C_SELSET  *pSSOut;
   C_FAS     *pFAS;
   C_STATUSLINE_MESSAGE StatusLineMsg(NULL);

   if (gsc_isEOF(pRs1) == GS_GOOD || gsc_isEOF(pRs2) == GS_GOOD) return GS_GOOD;

   pt1[X] = pt4[X] = Xmin;
   pt2[X] = pt3[X] = Xmin + pGrid->dx;
   pt1[Y] = pt2[Y] = Ymin;
   pt3[Y] = pt4[Y] = Ymin + pGrid->dy;

   // leggo prima cella
   if (gsc_DBReadRow(pRs1, ColValues) == GS_BAD) return GS_BAD;
   ValidPt1 = gsc_rb2Dbl(pZValue, &(pt1[Z]));
   gsc_Skip(pRs1);

   if (gsc_DBReadRow(pRs2, ColValues) == GS_BAD) return GS_BAD;
   ValidPt4 = gsc_rb2Dbl(pZValue, &(pt4[Z]));
   gsc_Skip(pRs2);

   StatusLineMsg.Init(gsc_msg(408), LARGE_STEP); // ogni 1000 "%ld celle elaborate."

   // Ciclo sulla X
   while (gsc_isEOF(pRs1) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs1, ColValues) == GS_BAD) return GS_BAD;
      ValidPt2 = gsc_rb2Dbl(pZValue, &(pt2[Z]));
      gsc_Skip(pRs1);
      NoDrawRight = (gsc_isEOF(pRs1) == GS_GOOD) ? FALSE : TRUE;

      if (gsc_DBReadRow(pRs2, ColValues) == GS_BAD) return GS_BAD;
      ValidPt3 = gsc_rb2Dbl(pZValue, &(pt3[Z]));
      gsc_Skip(pRs2);

      // per ciascuna quota
      pFAS   = (C_FAS *) FAS_list.get_head();
      if (pSSOutList) pSSOut = (C_SELSET *) pSSOutList->get_head();
      while (pFAS)
      {
         if (Mode == PREVIEW)
         {
            if (pFAS->color.getColorMethod() == C_COLOR::None)
               Color.setAutoCADColorIndex(7);
            else
               Color = pFAS->color;
         }
         else 
            Color = pFAS->color;

         // disegna le curve di livello per i 2 triangoli della cella
         if (gsc_DrawGridContours((ValidPt1 == GS_GOOD) ? pt1 : NULL,
                                  (ValidPt2 == GS_GOOD) ? pt2 : NULL,
                                  (ValidPt3 == GS_GOOD) ? pt3 : NULL,
                                  (ValidPt4 == GS_GOOD) ? pt4 : NULL,
                                  pFAS->elevation, pFAS->layer, Color,
                                  pFAS->line, pFAS->line_scale, pFAS->width,
                                  Mode, pSSOut) == GS_BAD)
            return GS_BAD;

         pFAS = (C_FAS *) pFAS->get_next();
         if (pSSOut) pSSOut = (C_SELSET *) pSSOut->get_next();
      }

      if (!NoDrawTop) // conto anche il punto superiore
         StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."

      StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."

      // avanza con la X 
      pt1[X] += pGrid->dx; pt1[Z] = pt2[Z]; ValidPt1 = ValidPt2;
      pt4[X] = pt1[X]; pt4[Z] = pt3[Z]; ValidPt4 = ValidPt3;
      pt2[X] = pt3[X] = pt1[X] + pGrid->dx;
   }

   // conto anche il punto a destra
   StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."

   if (!NoDrawTop) // conto anche il punto superiore
      StatusLineMsg.Set(gsc_msg(408), ++(*i)); // "%ld celle elaborate."

   return GS_GOOD;
}
int gsc_DisplayGridContours(double *Vector, C_FAS_LIST &FAS_list, C_GRID *pGrid, 
                            int Mode = PREVIEW, bool Join = TRUE, bool Fit = TRUE)
{
   long          i = 0, Key = 1, Key2, Key3, Key4;
   long          KeyMaxPenultimaRiga = pGrid->nx * (pGrid->ny - 1);
   ads_point     pt1, pt2, pt3, pt4;
   int           ValidPt2, ValidPt4;
   C_COLOR       Color;
   C_SELSET_LIST SSOutList;
   C_SELSET      *pSSOut;
   C_FAS         *pFAS;

   // per ciascuna quota creo un gruppo di selezione
   if (!(pFAS = (C_FAS *) FAS_list.get_head())) return GS_GOOD;
   while (pFAS)
   {
      // Se non esiste il layer lo creo
      if (gsc_crea_layer(pFAS->layer) == GS_BAD) return GS_BAD;

      if ((pSSOut = new C_SELSET) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      SSOutList.add_tail(pSSOut);

      pFAS = (C_FAS *) pFAS->get_next();
   }

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1083)); // "Visualizzazione delle curve di livello"
   StatusBarProgressMeter.Init(KeyMaxPenultimaRiga);

   while (Key <= KeyMaxPenultimaRiga)
   {
      StatusBarProgressMeter.Set(Key);

      // se il valore è il minimo per un double significa valore nullo
      if ((Key % pGrid->nx) == 0 ||
          Vector[Key] == (std::numeric_limits<double>::min)())
         { Key++; continue; }
      pt1[Z] = Vector[Key];

      Key3 = Key + pGrid->nx + 1;
      if (Vector[Key3] == (std::numeric_limits<double>::min)())
         { Key++; continue; }
      pGrid->key2pt(Key3, pt3, FALSE);

      pt3[Z] = Vector[Key3];    
      pt1[X] = pt3[X] - pGrid->dx;
      pt1[Y] = pt3[Y] - pGrid->dy;

      Key2 = Key + 1;
      if (Vector[Key2] == (std::numeric_limits<double>::min)())
         ValidPt2 = GS_BAD;
      else
      {
         ValidPt2 = GS_GOOD;
         pt2[X] = pt3[X];
         pt2[Y] = pt1[Y];
         pt2[Z] = Vector[Key2];
      }

      Key4 = Key + pGrid->nx;
      if (Vector[Key4] == (std::numeric_limits<double>::min)())
         ValidPt4 = GS_BAD;
      else
      {
         ValidPt4 = GS_GOOD;
         pt4[X] = pt1[X];
         pt4[Y] = pt3[Y];
         pt4[Z] = Vector[Key4];
      }

      // per ciascuna quota
      pFAS   = (C_FAS *) FAS_list.get_head();
      pSSOut = (C_SELSET *) SSOutList.get_head();
      while (pFAS)
      {
         if (Mode == PREVIEW)
         {
            if (pFAS->color.getColorMethod() == C_COLOR::None)
               Color.setAutoCADColorIndex(7);
            else
               Color = pFAS->color;
         }
         else 
            Color = pFAS->color;

         // disegna le curve di livello per i 2 triangoli della cella
         if (gsc_DrawGridContours(pt1, (ValidPt2 == GS_GOOD) ? pt2 : NULL,
                                  pt3, (ValidPt4 == GS_GOOD) ? pt4 : NULL,
                                  pFAS->elevation, pFAS->layer, Color,
                                  pFAS->line, pFAS->line_scale, pFAS->width,
                                  Mode, pSSOut) == GS_BAD)
            return GS_BAD;

         pFAS = (C_FAS *) pFAS->get_next();
         if (pSSOut) pSSOut = (C_SELSET *) pSSOut->get_next();
      }
      Key++;
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   if (Mode == EXTRACTION)
   {
      if (Join)
      {
         acutPrintf(gsc_msg(405)); // "\nUnione curve di livello..."
         SSOutList.PolylineJoin(); // Ne faccio il Join
         if (Fit)
         {
            acutPrintf(gsc_msg(406)); // "\nCurvatura curve di livello..."
            SSOutList.PolylineFit();  // Ne faccio il Fit
         }
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_DisplayGridContours                 <external> */
/*+
   Funzione LISP per visualizzare le curve di livello della griglia
   ad una determinata quota.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   (<prj><cls><sub><AttribZ><fas list>[<pt1><pt2>[<Display mode>[<Join>[<Fit>]]]])

   <fas list> = (<fas1><fas2>...)
   <fas> = (<color>nil<line><style><layer>nil<width><elevation><scale>)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_DisplayGridContours(void)
{
   presbuf    arg = acedGetArgs();
   ads_point  pt1, pt2;
   C_STRING   AttribZ;
   C_FAS_LIST FAS_list;
   int        prj, cls, sub, DisplayMode = PREVIEW;
   C_CGRID    *pCls;
   bool       pt1Exist = FALSE, Location = FALSE, Join = TRUE, Fit = TRUE;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // nome dell'attributo contenente il valore Z dei punti
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribZ = arg->resval.rstring;

   // FAS
   if (!(arg = arg->rbnext) || FAS_list.from_rb(arg) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // avanzo all'elemento successivo
   if ((arg = gsc_scorri(arg)) == NULL) 
	   { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((arg = arg->rbnext))
   {
      // punto inferiore della finestra
      if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
      if ((arg = arg->rbnext))
      {
         if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
         if ((arg = arg->rbnext))
         {
            // Modalità di visualizzazione
            gsc_rb2Int(arg, &DisplayMode);

            if ((arg = arg->rbnext))
            {  // unione delle curve di livello
               Join = (arg->restype == RTNIL) ? FALSE : TRUE;

               if ((arg = arg->rbnext))
                  // fit delle curve di livello
                  Fit = (arg->restype == RTNIL) ? FALSE : TRUE;
            }
         }
      }
   }

   if (pCls->DisplayContours(AttribZ, FAS_list,
                             (Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                             DisplayMode, Join, Fit) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc C_CGRID::DisplayContours               <external> */
/*+
   Funzione per visualizzare le curve di livello della griglia tramite 
   polilinee.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_STRING &AttribZ;    Nome dell'attributo da utilizzare per leggere la
                         coordinata Z.
   C_FAS_LIST &FAS_list; Lista delle caratteristiche grafiche della visualizzazione:
                         il campo elevation di ciascuna fas determinerà 
                         la quota per le curve di livello.
   ads_point pt1;        Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                         in cui visualizzare le curve di livello, Rappresenta il punto 
                         a sinistra in basso (default = NULL)
   ads_point pt2;        Rappresenta il punto a destra in alto (default = NULL)
   int Mode;             Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                         (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
   bool Join;            Usato se <Mode> = EXTRACTION; Effettua l'unione delle 
                         curve di livello se = TRUE (default = TRUE)
   bool Fit;             Usato se <Mode> = EXTRACTION e Join = TRUE; Effettua la curvatura
                         delle curve di livello se = TRUE (default = TRUE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayContours(C_STRING &AttribZ, C_FAS_LIST &FAS_list,
                             ads_point pt1, ads_point pt2,
                             int Mode, bool Join, bool Fit)
{
   C_RECT         Rect;
   ads_point      pt, _pt2;
   C_PREPARED_CMD Cmd;
   C_RB_LIST      ColValues;
   long           i = 0, KeyMin, KeyMax;
   _RecordsetPtr  pRs1, pRs2;
   presbuf        pZValue;
   bool           OddRow = FALSE, NoDrawTop;
   C_SELSET_LIST  SSOutList;
   C_SELSET       *pSSOut;
   C_FAS          *pFAS;

   // per ciascuna quota creo un gruppo di selezione
   if (!(pFAS = (C_FAS *) FAS_list.get_head())) return GS_GOOD;
   while (pFAS)
   {
      // Se non esiste il layer lo creo
      if (gsc_crea_layer(pFAS->layer) == GS_BAD) return GS_BAD;

      if ((pSSOut = new C_SELSET) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      SSOutList.add_tail(pSSOut);

      pFAS = (C_FAS *) pFAS->get_next();
   }

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (grid.pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       grid.key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (grid.pt2key(_pt2, &KeyMax) == GS_BAD) return GS_BAD;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      // preparo comando per lettura record nella riga
      if (prepare_data_on_row(Cmd, AttribZ.get_name()) == GS_BAD)
         AfxThrowUserException();
      if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
         AfxThrowUserException();

      if (gsc_InitDBReadRow(pRs1, ColValues) == GS_BAD) AfxThrowUserException();
      if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name())))
            AfxThrowUserException();

      // Ciclo sulla Y
      do
      {
         // imposto id inferiore e superiore
         KeyMin += grid.nx;
         KeyMax += grid.nx;

         // Verifico se si tratta del'ultima riga da valutare
         NoDrawTop = ((pt[Y] + 2 * grid.dy) <= Rect.Top()) ? TRUE : FALSE;

         if (OddRow) // leggere pRs1
         {
            gsc_DBCloseRs(pRs1);
            gsc_MoveFirst(pRs2);
            if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs1) == GS_BAD)
               AfxThrowUserException();

            if (gsc_DisplayGridContours(pRs2, pRs1, ColValues, pZValue, FAS_list,
                                        pt[X], pt[Y], ptr_grid(), &i, NoDrawTop, 
                                        Mode, &SSOutList) == GS_BAD)
               AfxThrowUserException();
         }
         else // leggere pRs2
         {
            gsc_DBCloseRs(pRs2);
            gsc_MoveFirst(pRs1);
            if (gsc_get_data_on_row(Cmd, KeyMin, KeyMax, pRs2) == GS_BAD)
               AfxThrowUserException();

            if (gsc_DisplayGridContours(pRs1, pRs2, ColValues, pZValue, FAS_list,
                                        pt[X], pt[Y], ptr_grid(), &i, NoDrawTop,
                                        Mode, &SSOutList) == GS_BAD)
               AfxThrowUserException();
         }

         OddRow = !OddRow;
         pt[Y] += grid.dy;
      }
      while (pt[Y] <= Rect.Top());
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
   acutPrintf(GS_LFSTR);
   gsc_endTransaction();

   if (Mode == EXTRACTION)
   {
      if (Join)
      {
         acutPrintf(gsc_msg(405)); // "\nUnione curve di livello..."
         SSOutList.PolylineJoin(); // Ne faccio il Join
         if (Fit)
         {
            acutPrintf(gsc_msg(406)); // "\nCurvatura curve di livello..."
            SSOutList.PolylineFit();  // Ne faccio il Fit
         }
      }
   }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER INTERPOLAZIONE SPAZIALE
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_getRowBarrierFactor             <internal> */
/*+
   Calcolo il fattore di barriera di celle appartenenti alla stessa
   riga comprese tra due colonne (0 = barriera totale, 1 = senza barriera).
   Funzione di ausilio alla funzione <gsc_getBarrierFactor>.
   C_GRID *pGrid;       Definizione di griglia
   long Row;            Riga in oggetto
   long StartColumn;    Numero di colonna iniziale (compreso)
   long EndColumn;      Numero di colonna finale (compreso)
   short *Vector;       Matrice griglia (già inizializzata) con i valori
                        di barriera (0-100)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
double gsc_getRowBarrierFactor(C_GRID *pGrid, long Row,
                               long StartColumn, long EndColumn, short *Vector)
{
   double Result = 1;
   long   Key, EndKey;

   Key    = pGrid->getKey(StartColumn, Row);
   EndKey = Key + (EndColumn - StartColumn);

   // Calcolo il fattore di barriera di celle appartenenti alla stessa
   // colonna comprese tra due righe
   while (Key <= EndKey)
   {
      // se il valore è il minimo per uno short significa valore nullo
      if (Vector[Key] != (std::numeric_limits<short>::min)())
         Result = Result * ((100.0 - Vector[Key]) / 100.0);

      Key++;
   }

   return Result;
}


/*********************************************************/
/*.doc gsc_getColumnBarrierFactor             <internal> */
/*+
   Calcolo il fattore di barriera di celle appartenenti alla stessa
   colonna comprese tra due righe (0 = barriera totale, 1 = senza barriera).
   Funzione di ausilio alla funzione <gsc_getBarrierFactor>.
   C_GRID *pGrid;       Definizione di griglia
   long Column;         Colonna in oggetto
   long StartRow;       Numero di riga iniziale (compreso)
   long EndRow;         Numero di riga finale (compreso)
   short *Vector;       Matrice griglia (già inizializzata) con i valori
                        di barriera (0-100)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
double gsc_getColumnBarrierFactor(C_GRID *pGrid, long Column,
                                  long StartRow, long EndRow, short *Vector)
{
   double Result = 1;
   long   Row1, Row2, Key;

   if (StartRow < EndRow)
      { Row1 = StartRow; Row2 = EndRow; }
   else
      { Row2 = StartRow; Row1 = EndRow; }

   // Calcolo il fattore di barriera di celle appartenenti alla stessa
   // colonna comprese tra due righe
   while (Row1 <= Row2)
   {
      Key = pGrid->getKey(Column, Row1);

      // se il valore è il minimo per uno short significa valore nullo
      if (Vector[Key] != (std::numeric_limits<short>::min)())
         Result = Result * ((100.0 - Vector[Key]) / 100.0);

      Row1++;
   }

   return Result;
}


/*********************************************************/
/*.doc gsc_getBarrierFactor                   <internal> */
/*+
   Funzione che ricava il fattore di barriera cioè di smorzamento dell'effetto
   di un punto noto nel calcolo IDW. La procedura individua tutte le celle 
   intersecate dalla retta che unisce il centroide della cella da calcolare con
   il centroide della cella con  valore noto. Per tutte queste celle verifica
   quali sono barriere e quanto queste lasciano filtrare con un valore che va
   da 0 (non esiste barriera) a 100 (barriera con non lascia passare niente).
   Ottenendo un fattore (che andrà da 0 = barriera totale a 1=  nessuna barriera)
   che dividerà la distanza delle due celle nel calcolo IDW. 
   Se ad esempio ci fossero 2 barriere che
   filtrano il 50 (con valore 50) il fattore sarebbe 0.5 * 0.5 = 0.25, 
   quindi solo il 25 % sarebbe filtrato e la distanza sarà aumentata di 4 volte.
   C_GRID *pGrid;                Definizione di griglia
   ads_point ptSource;           Punto centrale della cella con valore sconosciuto
   ads_point ptDest;             Punto centrale della cella con valore conosciuto
   short *Vector;                Matrice griglia (già inizializzata) con i valori
                                 di barriera (0-100)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
double gsc_getBarrierFactor(C_GRID *pGrid, ads_point ptSource, ads_point ptDest,
                            short *Vector)
{
   double    Result = 1, a, b;
   ads_point ptSx, ptDx, pt;
   long      Key, Row, Column, EndColumn, EndRow, DxRow, DxColumn;

   // se la retta è orizzontale
   if (ptSource[Y] == ptDest[Y])
   {
      pt[X] = min(ptSource[X], ptDest[X]);
      pt[Y] = ptSource[Y];
      pGrid->pt2key(pt, &Key);
      Row    = pGrid->getRow(Key);
      Column = pGrid->getColumn(Key);

      pt[X] = max(ptSource[X], ptDest[X]);
      pGrid->pt2key(pt, &Key);
      EndColumn = pGrid->getColumn(Key);

      return gsc_getRowBarrierFactor(pGrid, Row, Column, EndColumn, Vector);
   }
   else if (ptSource[X] == ptDest[X]) // se la retta è verticale
   {
      pt[X] = ptSource[X];
      pt[Y] = min(ptSource[Y], ptDest[X]);
      pGrid->pt2key(pt, &Key);
      Column = pGrid->getColumn(Key);
      Row    = pGrid->getRow(Key);

      pt[Y] = max(ptSource[Y], ptDest[Y]);
      pGrid->pt2key(pt, &Key);
      EndRow = pGrid->getRow(Key);

      return gsc_getColumnBarrierFactor(pGrid, Column, Row, EndRow, Vector);
   }


   // Assegno a ptSx il punto a sinistra e a ptDx il punto a destra
   if (ptSource[X] < ptDest[X])
      { ads_point_set(ptSource, ptSx); ads_point_set(ptDest, ptDx); }
   else
   {
      ads_point_set(ptSource, ptDx); 
      ads_point_set(ptDest, ptSx);
   }

   // Riga e colonna del punto a Sx
   pGrid->pt2key(ptSx, &Key);
   Row    = pGrid->getRow(Key);
   Column = pGrid->getColumn(Key);

   // Riga e colonna del punto a Dx
   pGrid->pt2key(ptDx, &Key);
   DxRow    = pGrid->getRow(Key);
   DxColumn = pGrid->getColumn(Key);

   // Calcolo l'equazione della retta passante per 2 punti y = ax + b (x = (y - b) / a)
   // Calcolo a
   a = (ptSx[Y] - ptDx[Y]) / (ptSx[X] - ptDx[X]);
   // Calcolo b
   b = ptSx[Y] - (a * ptSx[X]);

   if (Row < DxRow) // retta in salita
   {
      // ciclo spostandomi sull'asse delle Y fino a DxRow
      while (Row < DxRow)
      {
         // Calcolo la Y della riga successiva (in pt[Y])
         pGrid->RowColumn2pt(0, Row + 1, pt, FALSE);

         // Calcolo il punto di intersezione della retta passante per i 2 punti noti
         // con una retta orizzontale che rappresenta la riga della griglia
         pt[X] = (pt[Y] - b) / a;

         pGrid->pt2key(pt, &Key);
         EndColumn = pGrid->getColumn(Key);

         Result *= gsc_getRowBarrierFactor(pGrid, Row, Column, EndColumn, Vector);

         Column = EndColumn;
         Row++;
      }
      Result *= gsc_getRowBarrierFactor(pGrid, Row, Column, DxColumn, Vector);
   }
   else // retta in discesa
   {
      // ciclo spostandomi sull'asse delle Y fino a DxRow
      while (Row > DxRow)
      {
         // Calcolo la Y della riga successiva (in pt[Y])
         pGrid->RowColumn2pt(0, Row, pt, FALSE);

         // Calcolo il punto di intersezione della retta passante per i 2 punti noti
         // con una retta orizzontale che rappresenta la riga della griglia
         pt[X] = (pt[Y] - b) / a;

         pGrid->pt2key(pt, &Key);
         EndColumn = pGrid->getColumn(Key);

         Result *= gsc_getRowBarrierFactor(pGrid, Row, Column, EndColumn, Vector);

         Column = EndColumn;
         Row--;
      }
      Result *= gsc_getRowBarrierFactor(pGrid, Row, Column, DxColumn, Vector);
   }

   return Result;
}


/*********************************************************/
/*.doc gsc_getBarrierFactorList               <internal> */
/*+
   Funzione che ricava la lista dei fattori di barriera per il tratto che unisce
   il punto centroide di una cella a ciacun punto della lista PtList.
   C_GRID *pGrid;                Definizione di griglia
   ads_point Centroid;           Centroide della cella senza valore
   C_POINT_LIST &PtList;         Lista dei punti validi dell'intorno
   short *BarrierVector;         Matrice griglia contenente i valori
                                 di barriera
   C_REAL_LIST &pBarrierFactors; Lista dei fattori di barriera (1 = nessuna barriera,
                                 0 = barriera totale) per ciascun punto 
                                 in PtList (nello stesso ordine); out

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_getBarrierFactorList(C_GRID *pGrid, ads_point Centroid,
                             C_POINT_LIST &PtList,
                             short *BarrierVector, C_REAL_LIST &BarrierFactors)
{
   C_POINT *pPt = (C_POINT *) PtList.get_head();
   C_REAL  *BarrierFactor;

   BarrierFactors.remove_all();

   while (pPt)
   {
      if (!(BarrierFactor = new C_REAL(gsc_getBarrierFactor(pGrid, Centroid, pPt->point, BarrierVector))))
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      BarrierFactors.add_tail(BarrierFactor);

      pPt = (C_POINT *) PtList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getTotalBarrierFactorCount             <internal> */
/*+
   Funzione che conta quanti sono i fattori di barriera totale
   e che quindi non possono essere usati nel calcolo IDW.
   C_REAL_LIST &pBarrierFactors; Lista dei fattori di barriera

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_getTotalBarrierFactorCount(C_REAL_LIST &BarrierFactors)
{
   C_REAL *pBarrierFactor = (C_REAL *) BarrierFactors.get_head();
   int    Count = 0;

   while (pBarrierFactor)
   {
      if (pBarrierFactor->get_key_double() == 0) Count++; // barriera totale
      pBarrierFactor = (C_REAL *) pBarrierFactor->get_next();
   }

   return Count;
}


/*********************************************************/
/*.doc gsc_addValidGridPtsBetweenKeys         <internal> */
/*+
   Funzione che ricava la lista dei punti centroidi delle celle comprese
   tra un codice chiave minimo e uno massimo (compresi) con valore valido
   (diverso dal limite inferiore di un double).
   C_GRID *pGrid;                Definizione di griglia
   long KeyMin;                  Codice inferiore
   long KeyMax;                  Codice superiore
   double *Vector;               Matrice griglia (già inizializzata)
   C_POINT_LIST &PtList;         Lista dei punti dell'intorno (out)
   C_LONG_BTREE *pReadKeyList;   Opzionale; Lista dei codici delle celle i cui punti
                                 sono già presenti in PtList. Serve per evitare 
                                 che ci siano punti doppi e viene utilizzato
                                 quando non si conosce il raggio di ricerca
                                 ma solo il numero minimo di celle note da trovare
                                 (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_addValidGridPtsBetweenKeys(C_GRID *pGrid, long KeyMin, long KeyMax,
                                   double *Vector, C_POINT_LIST &PtList, 
                                   C_LONG_BTREE *pReadKeyList = NULL)
{
   ads_point Centroid;
   C_POINT   *pPt;
   long      Key = KeyMin;

   while (Key <= KeyMax)
   {
      // se il valore è il minimo per un double significa valore nullo
      if (Vector[Key] == (std::numeric_limits<double>::min)())
         { Key++; continue; }

      // se esiste la lista delle celle già lette
      if (pReadKeyList)
         // Verifico che non sia già presente
         if (pReadKeyList->search(&Key)) { Key++; continue; }

      if (pGrid->key2pt(Key, Centroid, TRUE) == GS_GOOD)
      {
         Centroid[Z] = Vector[Key];
         if ((pPt = new C_POINT(Centroid)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         PtList.add_tail(pPt);

         // se esiste la lista delle celle già lette aggiungo la cella
         if (pReadKeyList) pReadKeyList->add(&Key);
      }

      Key++;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_KeyMinMaxCircle                    <internal> */
/*+
   Funzione che ricava un intervallo di codici delle celle che
   sono all'interno di un cerchio su una riga ad una data distanza Y
   dal centro del cerchio.
   C_CGRID *pGrid;               Puntatore alla classe griglia
   ads_point Centroid;           Punto centrale della cella sconosciuta che 
                                 corrisponde al centro del cerchio
   double OffSetY;               Distanza Y dal centro del cerchio
   double RayXRay;               Raggio al quadrato del cerchio (di comodo)
   double MaxGridLimitX;         X massima della griglia (di comodo)
   long *KeyMin;                 codice inferiore (out)
   long *KeyMax;                 codice superiore (out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_KeyMinMaxCircle(C_GRID *pGrid, ads_point Centroid, double OffSetY,
                        double RayXRay, double MaxGridLimitX, long *KeyMin, long *KeyMax)
{
   ads_point pt, CellCentroid;
   double    OffSetX, OffSetYXOffSetY = OffSetY * OffSetY;

   if (OffSetYXOffSetY > RayXRay)
      { *KeyMin = 1; *KeyMax = 0; return GS_BAD; }

   // Applico il teorema di pitagora
   OffSetX = pow(RayXRay - (OffSetY * OffSetY), (double) 1/2); // radice quadrata

   pt[Y] = Centroid[Y] + OffSetY;

   // Calcolo KeyMin
   pt[X] = max(pGrid->x, Centroid[X] - OffSetX);
   pGrid->pt2key(pt, KeyMin);
   pGrid->key2pt(*KeyMin, CellCentroid);
   if (CellCentroid[X] < pt[X]) (*KeyMin)++;

   // Calcolo KeyMax
   pt[X] = Centroid[X] + OffSetX;
   if (pt[X] >= MaxGridLimitX) pt[X] = MaxGridLimitX - (pGrid->dx / 10);
   pGrid->pt2key(pt, KeyMax);
   pGrid->key2pt(*KeyMax, CellCentroid);
   if (CellCentroid[X] > pt[X]) (*KeyMax)--;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_KeyMinMaxDonut                     <internal> */
/*+
   Funzione che ricava un intervallo di codici delle celle che
   sono all'interno di un anello su una riga ad una data distanza Y
   dal centro del cerchio. Il bordo interno del cerchio escluso.
   C_CGRID *pGrid;               Puntatore alla classe griglia
   ads_point Centroid;           Punto centrale della cella sconosciuta che 
                                 corrisponde al centro del cerchio
   double OffSetY;               Distanza Y dal centro del cerchio
   double MajorRayXRay;          Raggio al quadrato del cerchio maggiore (di comodo)
   double MinorRayXRay;          Raggio al quadrato del cerchio minore (di comodo)
   double MaxGridLimitX;         X massima della griglia (di comodo)
   long *LeftKeyMin;             Codice inferiore lato sx(out)
   long *LeftKeyMax;             Codice superiore lato sx(out)
   long *RightKeyMin;            Codice inferiore lato dx(out)
   long *RightKeyMax;            Codice superiore lato dx(out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_KeyMinMaxDonut(C_GRID *pGrid, ads_point Centroid, double OffSetY,
                       double MajorRayXRay, double MinorRayXRay, 
                       double MaxGridLimitX, long *LeftKeyMin, long *LeftKeyMax,
                       long *RightKeyMin, long *RightKeyMax)
{
   if ((OffSetY * OffSetY) < MinorRayXRay)
   {
      // Calcolo gli estremi esterni dell'anello
      gsc_KeyMinMaxCircle(pGrid, Centroid, OffSetY,
                          MajorRayXRay, MaxGridLimitX, LeftKeyMin, RightKeyMax);
      // Calcolo gli estremi interni dell'anello
      gsc_KeyMinMaxCircle(pGrid, Centroid, OffSetY,
                          MinorRayXRay, MaxGridLimitX, LeftKeyMax, RightKeyMin);

      double Ray = MinorRayXRay / MinorRayXRay;
      // Se l'anello interno va fuori dalla griglia a sx di centroid
      if (pGrid->x > Centroid[X] - Ray)
      {  // annullo questo intervallo
         *LeftKeyMin = 1;
         *LeftKeyMax = 0;
      }
      else if (pGrid->getXLimit() < Centroid[X] + Ray)
      // Se l'anello interno va fuori dalla griglia a dx di centroid
      {  // annullo questo intervallo
         *RightKeyMin = 1;
         *RightKeyMax = 0;
      }
   }
   else
   {
      // Calcolo gli estremi esterni dell'anello
      gsc_KeyMinMaxCircle(pGrid, Centroid, OffSetY,
                          MajorRayXRay, MaxGridLimitX, LeftKeyMin, LeftKeyMax);
      // annullo questo intervallo
      *RightKeyMin = 1;
      *RightKeyMax = 0;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getGridPtsCrossingCircle           <internal> */
/*+
   Funzione che ricava la lista dei punti delle celle con valore valido
   che siano all'interno o intersecanti il cerchio di raggio noto <Ray> 
   con centro nel punto <pt>.
   C_GRID *pGrid;                Definizione della griglia
   ads_point Center;             Punto centrale della cella sconosciuta che 
                                 corrisponde al centro del cerchio
   double  Ray;                  Raggio utilizzato per cercare un intorno di punti
                                 noti a quello sconosciuto.
   double *Vector;               Matrice già inizializzata
   C_POINT_LIST &PtsCrossingCircleList; Lista dei punti dell'intorno (out)
   double PrevRay;               Opzionale; Raggio minore di <Ray> usato per 
                                 cercare solo all'interno di un anello e non 
                                 di un cerchio (default = 0)
   C_LONG_BTREE *pReadKeyList;   Opzionale; Lista dei codici delle celle i cui punti
                                 sono già presenti in PtList. Serve per evitare 
                                 che ci siano punti doppi e viene utilizzato
                                 quando non si conosce il raggio di ricerca
                                 ma solo il numero minimo di celle note da trovare
                                 (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_getGridPtsCrossingCircle(C_GRID *pGrid, ads_point Center,
                                 double Ray, double *Vector, 
                                 C_POINT_LIST &PtsCrossingCircleList,
                                 double PrevRay = 0.0,
                                 C_LONG_BTREE *pReadKeyList = NULL)
{   
   long      KeyMin, KeyMax; // se si tratta di anello (PrevRay > 0) allora è l'intervallo a sx dell'anello
   long      RightKeyMin, RightKeyMax; // si tratta dell'intervallo a dx dell'anello
   double    OffSetY = pGrid->dy;
   double    MaxLimitX = pGrid->getXLimit();
   double    MaxLimitY = pGrid->getYLimit();
   double    RayXRay = Ray * Ray;
   double    PrevRayXRay = PrevRay * PrevRay;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2, t3;
   _ftime(&t1);
#endif

   if (Ray == 0) return GS_BAD;

   // verifico le celle sulla stessa riga del centro del cerchio
   if (PrevRay == 0) // ricerca nel cerchio
   {
      PtsCrossingCircleList.remove_all();

      // calcolo l'intervallo di codici di celle interne al cerchio alla riga 
      // con offset Y = 0 cioè sulla stessa riga del centro del cerchio
      gsc_KeyMinMaxCircle(pGrid, Center, 0.0, 
                          RayXRay, MaxLimitX, 
                          &KeyMin, &KeyMax);
      // Verifico le celle valide nell'intervallo
      gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax,
                                     Vector, PtsCrossingCircleList, pReadKeyList);
   }
   else // ricerca nell'anello e non nel cerchio
   {

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif

      // calcolo l'intervallo di codici di celle interne
      // all'anello alla riga con offset Y = 0 cioè 
      // sulla stessa riga del centro dell'anello
      gsc_KeyMinMaxDonut(pGrid, Center, 0.0,
                         RayXRay, PrevRayXRay, MaxLimitX,
                         &KeyMin, &KeyMax, &RightKeyMin, &RightKeyMax);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   GrdTempo1 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
   _ftime(&t2);
#endif

      // Verifico le celle valide nell'intervallo a sx del centro dell'anello
      gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax,
                                     Vector, PtsCrossingCircleList, pReadKeyList);

      // Verifico le celle valide nell'intervallo a dx del centro dell'anello
      gsc_addValidGridPtsBetweenKeys(pGrid, RightKeyMin, RightKeyMax,
                                     Vector, PtsCrossingCircleList, pReadKeyList);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   GrdTempo2 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
   _ftime(&t2);
#endif
   }

   // Ripeto il ciclo finchè non ho coperto l'area del cerchio
   // badando di non uscire dall'area della griglia
   while (OffSetY <= Ray && 
          ((Center[Y] + OffSetY) < MaxLimitY || (Center[Y] - OffSetY) >= pGrid->y))
   {
      // Sopra il punto (verificando di non superare il bordo superiore della griglia)
      if ((Center[Y] + OffSetY) < MaxLimitY)
      {
         if (PrevRay == 0) // ricerca nel cerchio
         {
            // calcolo l'intervallo di codici di celle interne al cerchio alla riga 
            // indicata da OffSetY rispetto il centro del cerchio
            gsc_KeyMinMaxCircle(pGrid, Center, OffSetY, RayXRay, MaxLimitX, 
                                &KeyMin, &KeyMax);
            // Verifico le celle valide nell'intervallo
            gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax, 
                                           Vector, PtsCrossingCircleList, pReadKeyList);
         }
         else // ricerca nell'anello e non nel cerchio
         {

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif

            // calcolo l'intervallo di codici di celle interne
            // all'anello alla riga indicata da OffSetY rispetto il centro del cerchio
            gsc_KeyMinMaxDonut(pGrid, Center, OffSetY,
                               RayXRay, PrevRayXRay, MaxLimitX,
                               &KeyMin, &KeyMax, &RightKeyMin, &RightKeyMax);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   GrdTempo1 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
   _ftime(&t2);
#endif

            // Verifico le celle valide nell'intervallo a sx del centro dell'anello
            gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax,
                                           Vector, PtsCrossingCircleList, pReadKeyList);

            // Verifico le celle valide nell'intervallo a dx del centro dell'anello
            gsc_addValidGridPtsBetweenKeys(pGrid, RightKeyMin, RightKeyMax,
                                          Vector, PtsCrossingCircleList, pReadKeyList);
         }
      }

      // Sotto il punto (verificando di non superare il bordo inferiore della griglia)
      if ((Center[Y] - OffSetY) >= pGrid->y)
      {         
         if (PrevRay == 0) // ricerca nel cerchio
         {
            // calcolo l'intervallo di codici di celle interne al cerchio alla riga 
            // indicata da OffSetY rispetto il centro del cerchio
            gsc_KeyMinMaxCircle(pGrid, Center, -1 * OffSetY, RayXRay, MaxLimitX, 
                                &KeyMin, &KeyMax);
            // Verifico le celle valide nell'intervallo
            gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax, 
                                           Vector, PtsCrossingCircleList, pReadKeyList);
         }
         else // ricerca nell'anello e non nel cerchio
         {
#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif
            // calcolo l'intervallo di codici di celle interne
            // all'anello alla riga indicata da OffSetY rispetto il centro del cerchio
            gsc_KeyMinMaxDonut(pGrid, Center, -1 * OffSetY,
                               RayXRay, PrevRayXRay, MaxLimitX,
                               &KeyMin, &KeyMax, &RightKeyMin, &RightKeyMax);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   GrdTempo1 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
   _ftime(&t2);
#endif

            // Verifico le celle valide nell'intervallo a sx del centro dell'anello
            gsc_addValidGridPtsBetweenKeys(pGrid, KeyMin, KeyMax,
                                          Vector, PtsCrossingCircleList, pReadKeyList);

            // Verifico le celle valide nell'intervallo a dx del centro dell'anello
            gsc_addValidGridPtsBetweenKeys(pGrid, RightKeyMin, RightKeyMax,
                                          Vector, PtsCrossingCircleList, pReadKeyList);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   GrdTempo2 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
   _ftime(&t2);
#endif

         }
      }

      OffSetY += pGrid->dy;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getIDWValue                        <internal> */
/*+
   Funzione che calcola una media pesata delle quote dei punti nell'intorno.
   Più un punto è lontano minore sarà la sua influenza nel calcolo della media.
   C_POINT_LIST &PtsCrossingCircleList; Lista dei punti dell'intorno
   ads_point pt;                        Le coordinate X e Y sono quelle 
                                        del punto la cui Z deve essere 
                                        calcolata.
   bool Quadratic;                      Flag, se true le distanze vengono 
                                        elevate al quadrato per dare meno
                                        importanza ai punti lontani
                                        (default = false).
   C_REAL_LIST *pBarrierFactors;        Opzionale. Lista di fattori di barriera
                                        per ciascun punto di PtsCrossingCircleList
                                        (nello stesso ordine). Ogni fattore va da
                                        da 0 (barriera totale) 
                                        a 1 (non esiste barriera) che 
                                        moltiplicherà la distanza dei due punti 
                                        nel calcolo IDW (default = NULL).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_getIDWValue(C_POINT_LIST &PtsCrossingCircleList, ads_point pt, 
                    bool Quadratic = false, C_REAL_LIST *pBarrierFactors = NULL)
{
   C_POINT   *pPt = (C_POINT *) PtsCrossingCircleList.get_head();
   double    Numeratore = 0, Denominatore = 0, Distance2D;
   C_REAL    *pBarrier = NULL;

   if (pBarrierFactors) pBarrier = (C_REAL *) pBarrierFactors->get_head();
   pt[Z] = 0.0;

   while (pPt)
   {
      Distance2D = gsc_dist2d(pt, pPt->point);
      if (Quadratic) Distance2D = Distance2D * Distance2D;

      if (pBarrier)
      {
         if (pBarrier->get_key_double() == 0) // barriera totale
         {  // salto questo punto
            pPt = (C_POINT *) pPt->get_next();
            pBarrier = (C_REAL *) pBarrier->get_next();
            continue;
         }
         Distance2D = Distance2D / pBarrier->get_key_double();
         pBarrier = (C_REAL *) pBarrier->get_next();
      }

      Numeratore   += (pPt->point[Z] / Distance2D);
      Denominatore += (1 / Distance2D);

      pPt = (C_POINT *) pPt->get_next();
   }

   if (Denominatore == 0) return GS_BAD;
   pt[Z] = Numeratore / Denominatore;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SpatialInterpolationIDW            <internal> */
/*+
   Funzione di appoggio alla C_CGRID::SpatialInterpolationIDW
   che calcola i punti con quota sconosciuta tramite interpolazione
   spaziale con algoritmo IDW.
   Parametri:
   C_CGRID *pClass;        classe griglia
   double *InVector;       Vettore in input rappresentante la griglia con i valori
                           noti
   double *OutVector;      Vettore in outut rappresentante la griglia con i risultati
                           dell'interpolazione
   long KeyMin;            Codice iniziale della cella da verificare
   long KeyMax;            Codice finale della cella da verificare
   C_PREPARED_CMD_LIST &CmdList; Comando preparato per l'aggiornamento delle
                           celle nel DB (vedi parametro <pZValue>)
   C_RB_LIST &ColValues;   Lista di resbuf per l'aggiornamento delle celle nel DB  
                           (vedi parametro <pZValue>)
   presbuf pZValue;        Puntatore a resbuf nella lista ColValues contenente 
                           il valore della quota. Se questo parametro è NULLO
                           verrà aggiornato solo il vettore in memoria.
   long      *i;           contatore
   double  Ray;            Raggio utilizzato per cercare un intorno di punti
                           noti a quello sconosciuto (se = 0 non viene usato e
                           verrà calcolato un raggio tale da includere almeno
                           <MinKnownPts> celle note, in questo caso <MinKnownPts>
                           non deve essere = 0).
   int MinKnownPts;        Numero minimo di punti noti affinchè sia accettabile 
                           l'interpolazione spaziale (se = 0 non viene usato, in 
                           questo caso <Ray> non deve essere = 0).
   bool Quadratic;         Flag, se true le distanze vengono elevate al quadrato 
                           per dare meno importanza ai punti lontani
                           (default = false).
   short *BarrierVector;   Opzionale; Matrice griglia (già inizializzata) 
                           con i valori di barriera (da 0 = non esiste barriera
                           a 100 = barriera con non lascia passare niente)
                           (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_SpatialInterpolationIDW(C_CGRID *pClass, double *InVector, double *OutVector,
                                long KeyMin, long KeyMax, 
                                C_PREPARED_CMD_LIST &CmdList, C_RB_LIST &ColValues, presbuf pZValue, 
                                long *i, double Ray = 0.0, int MinKnownPts = 3, bool Quadratic = false,
                                short *BarrierVector = NULL)
{
   long           Key = KeyMin;
   ads_point      pt;
   C_GRID         *pGrid = pClass->ptr_grid();
   C_POINT_LIST   PtsCrossingCircleList;
   C_REAL_LIST    BarrierFactors;
   double         MaxLimitX = pGrid->getXLimit();
   double         MaxLimitY = pGrid->getYLimit();
   int            ValidPts;
   ads_point      PrevPt;
   double         PrevMaxRayWithoutKnownPts = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1084)); // "Interpolazione IDW"
   StatusLineMsg.Init(gsc_msg(408), MEDIUM_STEP); // ogni 100 "%ld celle elaborate."

   pt[Z] = PrevPt[Z] = 0.0;

   // per tutte le celle che sono comprese tra KeyMin e KeyMax
   while (Key <= KeyMax)
   {
      // se il valore è il minimo per un double significa valore nullo
      if (InVector[Key] != (std::numeric_limits<double>::min)())
         { Key++; continue; }

      // Mi ricavo le coordinate del punto centrale della cella
      if (pGrid->key2pt(Key, pt) == GS_BAD) return GS_BAD;

      if (Ray <= 0) // se il raggio non è valido
      {  // ricerco in un raggio sempre più grande finchè
         // trovo un numero di oggetto sufficiente (>= MinKnownPts)
         double       OffSet = min(pGrid->dx, pGrid->dy); // aumento il raggio di offset
         double       PrevRay = 0.0;
         double       distFromLowLeftCorner, distFromLowRightCorner;
         double       distFromUpLeftCorner, distFromUpRightCorner;
         ads_point    dummyPt;
         bool         OutOfBound;
         C_LONG_BTREE ReadKeyList;

         dummyPt[Z] = 0.0;

         // Calcolo le distanze del punto centrale della cella dagli angoli 
         // della matrice
         dummyPt[X] = pGrid->x; dummyPt[Y] = pGrid->y;
         distFromLowLeftCorner = fabs(gsc_dist2d(pt, dummyPt));
         dummyPt[X] = MaxLimitX;
         distFromLowRightCorner = fabs(gsc_dist2d(pt, dummyPt));
         dummyPt[X] = pGrid->x; dummyPt[Y] = MaxLimitY;
         distFromUpLeftCorner = fabs(gsc_dist2d(pt, dummyPt));
         dummyPt[X] = MaxLimitX;
         distFromUpRightCorner = fabs(gsc_dist2d(pt, dummyPt));

         // Ricavo la lista dei punti nell'intorno
         Ray = OffSet;

         if (PrevMaxRayWithoutKnownPts > 0)
         {
            // Cerco di riutilizzare una parte delle celle cercate per una cella
            // precedente
            PrevMaxRayWithoutKnownPts = PrevMaxRayWithoutKnownPts - fabs(gsc_dist2d(pt, PrevPt));
            if (PrevMaxRayWithoutKnownPts > Ray)
            {
               PrevRay = PrevMaxRayWithoutKnownPts;
               Ray     = PrevRay + OffSet;
               PtsCrossingCircleList.remove_all();
            }
         }
         ads_point_set(pt, PrevPt);

         do
         {
            // Cerco le celle con valore valido
            if (gsc_getGridPtsCrossingCircle(pGrid, pt, Ray,
                                             InVector, PtsCrossingCircleList,
                                             PrevRay, &ReadKeyList) == GS_BAD)
               return GS_BAD;

            // Conto i punti validi
            ValidPts = PtsCrossingCircleList.get_count();
            
            // Se non ho trovato niente memorizzo il punto e il raggio
            if (ValidPts == 0)
               PrevMaxRayWithoutKnownPts = Ray;
               
            PrevRay = Ray;
            Ray += OffSet;

            // Se il raggio interno dell'anello supera i confini della griglia
            if (PrevRay > distFromLowLeftCorner && PrevRay > distFromLowRightCorner &&
                PrevRay > distFromUpLeftCorner && PrevRay > distFromUpRightCorner)
                OutOfBound = true;
            else 
                OutOfBound = false;

            if (BarrierVector) // Se esiste la griglia con i valori di barriera
            {
               int TotalBarrierFactorCount;

               // Calcolo per ogni punto un fattore di barriera
               if (gsc_getBarrierFactorList(pGrid, pt, PtsCrossingCircleList, 
                                            BarrierVector, BarrierFactors) == GS_BAD)
                  return GS_BAD;

               // Devo ricontare i punti validi perchè potrebbero esserci delle
               // barriere totali che ne escludono alcuni
               TotalBarrierFactorCount = gsc_getTotalBarrierFactorCount(BarrierFactors);
               ValidPts -= TotalBarrierFactorCount;
            }
         }
         while (ValidPts < MinKnownPts && !OutOfBound);
         Ray = 0.0; // Ripristino il valore di Ray

         if (ValidPts < MinKnownPts) { Key++; continue; }
      }
      else
      {
         // Ricavo la lista dei punti nell'intorno di un raggio noto
         if (gsc_getGridPtsCrossingCircle(pGrid, pt, Ray,
                                          InVector, PtsCrossingCircleList) == GS_BAD)
            return GS_BAD;
      
         if (MinKnownPts > 0)
         {
            ValidPts = PtsCrossingCircleList.get_count();
            // Se non c'è un numero di punti sufficiente
            if (ValidPts < MinKnownPts) { Key++; continue; }
         }
         if (BarrierVector) // Se esiste la griglia con i valori di barriera
         {
            // Calcolo per ogni punto un fattore di barriera
            if (gsc_getBarrierFactorList(pGrid, pt, PtsCrossingCircleList, 
                                         BarrierVector, BarrierFactors) == GS_BAD)
               return GS_BAD;

            if (MinKnownPts > 0)
            { 
               int TotalBarrierFactorCount;

               // Devo ricontare i punti validi perchè potrebbero esserci delle
               // barriere totali che ne escludono alcuni
               TotalBarrierFactorCount = gsc_getTotalBarrierFactorCount(BarrierFactors);
               
               // Se non c'è un numero di punti sufficiente
               ValidPts -= TotalBarrierFactorCount;
               if (ValidPts < MinKnownPts) { Key++; continue; }
            }
         }
      }

      // calcolo il valore IDW (se BarrierFactors è vuoto o = NULL non viene usato)
      if (gsc_getIDWValue(PtsCrossingCircleList, pt, Quadratic, &BarrierFactors) != GS_GOOD)
         return GS_BAD;

      OutVector[Key] = pt[Z]; // Aggiorno vettore temporaneo
      if (pZValue)
      {
         // aggiorno la cella
         if (pClass->query_data(Key, ColValues, &CmdList) == GS_GOOD)
         {
            gsc_RbSubst(pZValue, pt[Z]);

            if (pClass->upd_data(Key, ColValues, 
                                 ((C_PREPARED_CMD *) CmdList.get_head())) == GS_GOOD) // Comando per TEMP
               StatusLineMsg.Set(++(*i)); // "%ld celle elaborate."
         }
      }
      else
         StatusLineMsg.Set(++(*i)); // "%ld celle elaborate."

      Key++;
   }
   StatusLineMsg.End(gsc_msg(408), *i); // "%ld celle elaborate."

#if defined(GSDEBUG) // se versione per debugging
   acutPrintf(_T("\n gsc_KeyMinMaxDonut impiega %6.2f secondi.\n"), GrdTempo1);
   acutPrintf(_T("\n gsc_addValidGridPtsBetweenKeys impiega %6.2f secondi.\n"), GrdTempo2);
   acutPrintf(_T("\n gsc_KeyMinMaxCircle impiega %6.2f secondi.\n"), GrdTempo3);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_GridSpatialInterpolationIDW         <external> */
/*+
   Funzione LISP per calcola le quote dei punti della griglia non ancora
   valorizzati partendo da alcuni punti noti tramite l'algoritmo
   IDW (Inverse Distance Weighting) che opera una media pesata delle
   quote dei punti nell'intorno. Più un punto è lontano minore sarà 
   la sua influenza nel calcolo della media.
   Parametri:
   (<prj><cls><sub><AttribZ><Ray>[<MinKnownPts>[<pt1><pt2>[Recursive[BarrierAttrib]]]])

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_GridSpatialInterpolationIDW(void)
{
   presbuf   arg = acedGetArgs();
   ads_point pt1, pt2;
   C_STRING  AttribZ, BarrierAttrib;
   int       prj, cls, sub, MinKnownPts = 3;
   C_CGRID   *pCls;
   bool      pt1Exist = FALSE, Location = FALSE, Recursive = TRUE, Quadratic = false;
   double    Ray = 0.0;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // nome dell'attributo contenente il valore Z dei punti
   if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribZ = arg->resval.rstring;

   if ((arg = arg->rbnext))
   {
      // raggio di ricerca
      if (gsc_rb2Dbl(arg, &Ray) == GS_BAD || Ray < 0)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

      if ((arg = arg->rbnext))
      {
         // Modalità di visualizzazione
         if (gsc_rb2Int(arg, &MinKnownPts) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

         if ((arg = arg->rbnext))
         {
            // punto inferiore della finestra
            if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
            if ((arg = arg->rbnext))
            {
               if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
               // Recursive
               if ((arg = arg->rbnext))
               {
                  if (arg->restype == RTNIL) Recursive = FALSE;
                  if ((arg = arg->rbnext))
                  {
                     // nome dell'attributo contenente il valore dei punti fattori di barriera
                     if (arg->restype == RTSTR) BarrierAttrib = arg->resval.rstring;
                  }
               }
            }
         }
      }
   }

   if (pCls->SpatialInterpolationIDW(AttribZ, Ray, MinKnownPts, Quadratic,
                                     (Location) ? pt1 : NULL,
                                     (Location) ? pt2 : NULL,
                                     Recursive,
                                     (BarrierAttrib.get_name()) ? &BarrierAttrib : NULL) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc C_CGRID::SpatialInterpolationIDW       <external> */
/*+
   Funzione che serve per calcolare i punti non noti di una griglia
   (con valore NULLO) partendo da punti noti tramite l'algoritmo
   IDW (Inverse Distance Weighting) che opera una media pesata delle
   quote dei punti nell'intorno. Più un punto è lontano minore sarà 
   la sua influenza nel calcolo della media.
   C_STRING &AttribZ;    Nome dell'attributo da utilizzare per leggere la
                         coordinata Z.
   double  Ray;          Raggio utilizzato per cercare un intorno di punti
                         noti a quello sconosciuto (se = 0 non viene usato e
                         verrà calcolato un raggio tale da includere almeno
                         <MinKnownPts> celle note, in questo caso <MinKnownPts>
                         non deve essere = 0).
   int MinKnownPts;      Numero minimo di punti noti affinchè sia accettabile 
                         l'interpolazione spaziale (se = 0 non viene usato, in 
                         questo caso <Ray> non deve essere = 0).
   bool Quadratic;       Flag, se true le distanze vengono elevate al quadrato 
                         per dare meno importanza ai punti lontani (default = false).
   ads_point pt1;        Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                         in cui operare l'interpolazione spaziale, Rappresenta il punto 
                         a sinistra in basso (default = NULL)
   ads_point pt2;        Rappresenta il punto a destra in alto (default = NULL)
   bool      Recursive;  Se = TRUE il processo continua ad iterare finchè riesce
                         a interpolare qualche punto altrimenti prova una sola
                         volta (default = TRUE)
   C_STRING *pBarrierAttrib; Opzionale. Nome dell'attributo da utilizzare per leggere
                             il fattore di barriera (0 = nessuna barriera, 
                             100 = barriera totale) (default = NULL).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::SpatialInterpolationIDW(C_STRING &AttribZ, 
                                     double Ray, int MinKnownPts, bool Quadratic,
                                     ads_point pt1, ads_point pt2, bool Recursive,
                                     C_STRING *pBarrierAttrib)
{
   C_RECT              Rect;
   ads_point           pt, _pt2;
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           ColValues;
   long                i = 0, Key, KeyMin, KeyMax, iPrev, KeyMinOrig, KeyMaxOrig;
   presbuf             pZValue;
   double              *Vector = NULL, *TempVector = NULL; // vettori per la Z
   long                VectorLen;
   short               *BarrierVector = NULL; // vettore per le barriere

   // carico in memoria un vettore con le Z
   if (LoadToMemory(AttribZ, &Vector, &VectorLen) == GS_BAD) return GS_BAD;

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         { free(Vector); return GS_GOOD; }
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (grid.pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       grid.key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      { free(Vector); return GS_BAD; }
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (grid.pt2key(_pt2, &KeyMax) == GS_BAD) { free(Vector); return GS_BAD; }

   KeyMinOrig = KeyMin;
   KeyMaxOrig = KeyMax;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);

   try
   {
      // Preparo i comandi di lettura dei dati della classe dal temp
      if (prepare_data(CmdList) == GS_BAD) AfxThrowUserException();
      // Lettura fittizia solo per inizializzare ColValues
      if (gsc_get_data(CmdList, 0, ColValues) == GS_BAD) AfxThrowUserException();
      if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name())))
         AfxThrowUserException();
      
      // Alloco un vettore temporaneo e lo inizializzo
      if (gsc_InitGridToMemory(ptr_grid(), &TempVector, &VectorLen) == GS_BAD)
         return GS_BAD;

      if (pBarrierAttrib) // se si devono leggere i fattori di barriera
         // carico in memoria un vettore con i fattori di barriera
         if (LoadToMemory(*pBarrierAttrib, &BarrierVector, &VectorLen) == GS_BAD)
            return GS_BAD;

      do
      {
         iPrev = i;
         // Ciclo sulla Y
         do
         {
            if (gsc_SpatialInterpolationIDW(this, Vector, TempVector, KeyMin, KeyMax,
                                            CmdList, ColValues, pZValue,
                                            &i, Ray, MinKnownPts, Quadratic,
                                            BarrierVector) == GS_BAD)
               AfxThrowUserException();

            // imposto id inferiore e superiore
            KeyMin += grid.nx;
            KeyMax += grid.nx;
            pt[Y] += grid.dy;
         }
         while (pt[Y] <= Rect.Top());

         // Ciclo di aggiornamento
         KeyMin = KeyMinOrig;
         KeyMax = KeyMaxOrig;
         ads_2Dpoint_set(Rect.BottomLeft.point, pt);
         do
         {
            // Aggiorno vettore originale
            for (Key = KeyMin; Key <= KeyMax; Key++)
               if (TempVector[Key] != (std::numeric_limits<double>::min)())
                  Vector[Key] = TempVector[Key];
            // imposto id inferiore e superiore
            KeyMin += grid.nx;
            KeyMax += grid.nx;
            pt[Y] += grid.dy;
         }
         while (pt[Y] <= Rect.Top());

         KeyMin = KeyMinOrig;
         KeyMax = KeyMaxOrig;
         ads_2Dpoint_set(Rect.BottomLeft.point, pt);

         if (!Recursive) break; // solo una volta
      }
      while (iPrev != i); // finchè sono state fatte interpolazioni
   } // fine try

   catch (...) // any type of exception
   {
      free(Vector);
      if (TempVector) free(TempVector);
      acutPrintf(GS_LFSTR);
      acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
      acutPrintf(GS_LFSTR);
   
      return GS_BAD;
   }

   free(Vector); 
   if (TempVector) free(TempVector);
   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
   acutPrintf(GS_LFSTR);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::LoadToMemory                  <internal> */
/*+
  Funzione che carica in un vettore i valori numerici (double) di un attributo
  per tutte le celle della griglia in modo che vi sia corrispondenza
  tra codice chiave nella tabella DB e il numero di cella del vettore.
  C_STRING &AttribZ;    Nome dell'attributo da utilizzare per leggere il 
                        valore numerico.
  double **Vector;      Vettore che viene allocato dalla funzione
  long *VectorLen;      Lunghezza del vettore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. Alloca memoria
-*/  
/*************************************************************/
int C_CGRID::LoadToMemory(C_STRING &AttribZ, double **Vector, long *VectorLen)
{
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;
   long           i = 0;
   _RecordsetPtr  pRs;
   presbuf        pZValue;
   C_STRING       FldName, statement, TableRef;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   if ((pConn = info.getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;

   FldName = AttribZ;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // leggo record in ordine di GS_ID
   statement = _T("SELECT ");
   statement += FldName;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");

   FldName = info.key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement += FldName;

   // preparo comando SQL
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name())))
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   *VectorLen = (grid.nx * grid.ny) + 1; // conto anche il record 0
   if ((*Vector = (double *) malloc(sizeof(double) * (*VectorLen))) == NULL)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   C_STRING Title;
   Title.set_name_formatted(gsc_msg(407), AttribZ.get_name()); // "Caricamento valore <%s> della griglia in memoria"
   acutPrintf(_T("\n%s\n"), Title.get_name());

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(*VectorLen);

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { free(*Vector); return GS_BAD; }
      if (gsc_rb2Dbl(pZValue, &((*Vector)[i])) == GS_BAD)
         (*Vector)[i] = (std::numeric_limits<double>::min)(); // assegno il minimo per un double

      StatusBarProgressMeter.Set(++i);

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::LoadToMemory                  <internal> */
/*+
  Funzione che carica in un vettore i valori numerici (short) di un attributo
  per tutte le celle della griglia in modo che vi sia corrispondenza
  tra codice chiave nella tabella DB e il numero di cella del vettore.
  C_STRING &Attrib;    Nome dell'attributo da utilizzare per leggere il 
                       valore numerico.
  short **Vector;      Vettore che viene allocato dalla funzione
  long *VectorLen;     Lunghezza del vettore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. Alloca memoria
-*/  
/*************************************************************/
int C_CGRID::LoadToMemory(C_STRING &Attrib, short **Vector, long *VectorLen)
{
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;
   long           i = 0;
   int            Value;
   _RecordsetPtr  pRs;
   presbuf        pValue;
   C_STRING       FldName, statement, TableRef;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   if ((pConn = info.getDBConnection(TEMP)) == NULL) return GS_BAD;
   if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;

   FldName = Attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // leggo record in ordine di GS_ID
   statement = _T("SELECT ");
   statement += FldName;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");

   FldName = info.key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement += FldName;

   // preparo comando SQL
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (!(pValue = ColValues.CdrAssoc(Attrib.get_name())))
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   *VectorLen = (grid.nx * grid.ny) + 1; // conto anche il record 0
   if ((*Vector = (short *) malloc(sizeof(short) * (*VectorLen))) == NULL)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   C_STRING Title;
   Title.set_name_formatted(gsc_msg(407), Attrib.get_name()); // "Caricamento valore <%s> della griglia in memoria"
   acutPrintf(_T("\n%s\n"), Title.get_name());

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(*VectorLen);

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { free(*Vector); return GS_BAD; }
      if (gsc_rb2Int(pValue, &Value) == GS_BAD)
         (*Vector)[i] = (std::numeric_limits<short>::min)(); // assegno il minimo per un double
      else
         (*Vector)[i] = (short) Value;

      StatusBarProgressMeter.Set(++i);

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


int gsc_InitGridToMemory(C_GRID *pGrid, double **Vector, long *VectorLen)
{
   *VectorLen = (pGrid->nx * pGrid->ny) + 1; // conto anche il record 0
   if ((*Vector = (double *) malloc(sizeof(double) * (*VectorLen))) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   // inizializzo vettore con il minimo per un double
   for (long i = 0; i < *VectorLen; i++)
      (*Vector)[i] = (std::numeric_limits<double>::min)();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_InitIterpolateDisplayContoursGrid   <external> */
/*+
   Funzione LISP che inizializza, interpola e visualizza le curve 
   di livello di una griglia in memoria.
   Parametri:
   (<grid info> <init info> <interpolate info> [<contour info>])

   dove:
   <grid info>        = (<OrigX><OrigY><DeltaX><DeltaY><nX><nY>)
   <init info>        = (<selset>[<Source Attrib>])
   <interpolate info> = (<Ray>[<MinKnownPts>])
   <contour info>     = (<fas list>[<Display mode>[<Join>[<Fit>]]])
   <fas list>         = (<fas1><fas2>...) | nil | 
                        <numero intervalli (intero)> | <passo intervalli (reale)>
   <fas>              = (<color>nil<line><style><layer>nil<width><elevation><scale>)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_InitIterpolateDisplayContoursGrid(void)
{
   presbuf    arg = acedGetArgs(), p;
   C_STRING   AttribZ;
   int        MinKnownPts = 0, DisplayMode = EXTRACTION;
   C_CGRID    GridCls;
   C_SELSET   ss;
   bool       Join = TRUE, Fit = FALSE;
   bool       ContoursOnStep = FALSE, ContoursOnIntervals = FALSE, Quadratic = false;
   C_FAS_LIST FAS_list;
   double     *Vector, *TempVector = NULL, Ray = 0.0, Step;
   long       VectorLen, i = 0, iPrev, Key, nIntervals;
   C_PREPARED_CMD_LIST CmdList;    // usato solo per compatibilità
   C_RB_LIST           ColValues;  // usato solo per compatibilità
   
   arg = acedGetArgs();

   acedRetNil();
   
   // <grid info>
   if (!(p = gsc_nth(0, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (GridCls.ptr_grid()->from_rb(p) == GS_BAD) return RTERROR;

   // <init info>
   if (!(p = gsc_nthcopy(1, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(p = p->rbnext) || p->restype != RTPICKS)
      { acutRelRb(p); GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss << p->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);
   if ((p = p->rbnext) && p->restype == RTSTR) AttribZ = p->resval.rstring;
   acutRelRb(p);

   // <interpolate info>
   if (!(p = gsc_nthcopy(2, arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (p->restype == RTPOINT)
   {
      Ray         = p->resval.rpoint[X];
      MinKnownPts = (long) p->resval.rpoint[Y];
   }
   else
   {
      if (!(p = p->rbnext) || gsc_rb2Dbl(p, &Ray) == GS_BAD)
         { acutRelRb(p);GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      if ((p = p->rbnext) && p->restype != RTLE)
         if (gsc_rb2Int(p, &MinKnownPts) == GS_BAD)
            { acutRelRb(p); GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   }
   acutRelRb(p);

   // <contour info> opzionale
   if ((p = gsc_nth(3, arg)))
   { 
      switch (p->restype)
      {
         case RTNIL:
            // suddivisione in 5 intervalli
            ContoursOnStep      = FALSE;
            ContoursOnIntervals = TRUE;
            nIntervals          = 5;
            break;

         case RTSHORT: case RTLONG:
            // suddivisione in un numero di intervalli noto
            ContoursOnStep      = FALSE;
            ContoursOnIntervals = TRUE;
            if (gsc_rb2Lng(p, &nIntervals) == GS_BAD) nIntervals = 0;
            if (nIntervals == 0) 
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            break;

         case RTREAL:
            // suddivisione per un passo noto
            ContoursOnStep      = TRUE;
            gsc_rb2Dbl(p, &Step);
            if (Step == 0) 
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            ContoursOnIntervals = FALSE;
            break;
   
         case RTLB:
            // FAS
            ContoursOnStep      = FALSE;
            ContoursOnIntervals = FALSE;

            if (FAS_list.from_rb(p) == GS_BAD)
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            // avanzo all'elemento successivo
            if ((p = gsc_scorri(p)) == NULL) 
	            { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
            break;
      }

      if (!(p = p->rbnext) && p->restype != RTLE)
         // Modalità di visualizzazione
         if (gsc_rb2Int(p, &DisplayMode) == GS_GOOD)
            if ((p = p->rbnext) && p->restype != RTLE)
            {  // unione delle curve di livello
               Join = (p->restype == RTNIL) ? FALSE : TRUE;

               if ((p = p->rbnext) && p->restype != RTLE)
                  // fit delle curve di livello
                  Fit = (p->restype == RTNIL) ? FALSE : TRUE;
            }
   }

   if (gsc_InitGridToMemory(GridCls.ptr_grid(), &Vector, &VectorLen) == GS_BAD)
      return RTERROR;

   if (GridCls.upd_data_fromGraph(ss, Vector,
                                  (AttribZ.len() > 0) ? &AttribZ : NULL) == GS_BAD)
      { free(Vector); return RTERROR; }

   // Alloco un vettore temporaneo e lo inizializzo
   if (gsc_InitGridToMemory(GridCls.ptr_grid(), &TempVector, &VectorLen) == GS_BAD)
      { free(Vector); return RTERROR; }

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);
   do
   {
      iPrev = i;
      if (gsc_SpatialInterpolationIDW(&GridCls, Vector, TempVector,
                                      1, GridCls.ptr_grid()->nx * GridCls.ptr_grid()->ny,
                                      CmdList, ColValues, NULL, 
                                      &i, Ray, MinKnownPts, Quadratic) == GS_BAD)
         { free(Vector); free(TempVector); return RTERROR; }

      // Aggiorno vettore originale
      for (Key = 1; Key <= (GridCls.ptr_grid()->nx * GridCls.ptr_grid()->ny); Key++)
         if (TempVector[Key] != (std::numeric_limits<double>::min)())
            Vector[Key] = TempVector[Key];
   }
   while (iPrev != i); // finchè sono state fatte interpolazioni
   free(TempVector);
   acutPrintf(GS_LFSTR);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   if (ContoursOnIntervals || ContoursOnStep)
   {
      double   MinValue, MaxValue, OffSet;
      long     i;
      C_FAS    *pFas;
      C_STRING LineType, Layer;

      // Mi ricavo il minimo, il massimo e faccio 5 intervalli
      MinValue = (std::numeric_limits<double>::min)();
      for (i = 0; i < VectorLen; i++)
         if (Vector[i] != (std::numeric_limits<double>::min)())
            if (MinValue == (std::numeric_limits<double>::min)())
               MinValue = MaxValue = Vector[i];
            else
            {
               if (Vector[i] < MinValue) MinValue = Vector[i];
               if (Vector[i] > MaxValue) MaxValue = Vector[i];
            }

      if (MinValue == MaxValue) { free(Vector); return RTERROR; }

      if (ContoursOnIntervals)
      {
         OffSet = (MaxValue - MinValue) / (nIntervals + 1);
         for (i = 0; i < nIntervals; i++)
         {
            if ((pFas = new C_FAS()) == NULL)
               { free(Vector); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            MinValue     = pFas->elevation = MinValue + OffSet;

            LineType.paste(gsc_tostring(MinValue,20,4));
            if (gsc_DefineTxtLineType(LineType, LineType) == GS_GOOD)
               gsc_strcpy(pFas->line, LineType.get_name(), MAX_LEN_LINETYPENAME);

            // Uso la variabile LineType anche per il layer
            if (gsc_get_tmp_layer(_T("ContourSet"), Layer) == GS_BAD)
               { free(Vector); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            Layer += _T("_");

            Layer += LineType;
            gsc_strcpy(pFas->layer, Layer.get_name(), MAX_LEN_LAYERNAME);

            FAS_list.add_tail(pFas);
         }
      }
      else // Intervalli per passo noto
      {
         double Rem = fmod(MinValue, Step);
            
         if (Rem > 0) OffSet = MinValue - Rem;
         else OffSet = MinValue;
         OffSet += Step;

         i = 0;
         while (OffSet < MaxValue)
         {
            if ((pFas = new C_FAS()) == NULL)
               { free(Vector); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pFas->elevation = OffSet;

            LineType.paste(gsc_tostring(OffSet,20,4));
            if (gsc_DefineTxtLineType(LineType, LineType) == GS_GOOD)
               gsc_strcpy(pFas->line, LineType.get_name(), MAX_LEN_LINETYPENAME);

            // Uso la variabile LineType anche per il layer
            Layer = _T("Contour_");
            Layer += LineType;
            gsc_strcpy(pFas->layer, Layer.get_name(), MAX_LEN_LAYERNAME);

            FAS_list.add_tail(pFas);
            OffSet += Step;
            i++;
         }
      }

      // Imposto i colori delle isolinee dal verde (3) al rosso (1)
      C_COLOR_LIST ColorsList;
      C_COLOR      *pColor;
      C_COLOR      From, To;

      From.setAutoCADColorIndex(3); // verde
      To.setAutoCADColorIndex(1);   // rosso
      if (ColorsList.getColorsFromTo(From, To, FAS_list.get_count()) == GS_BAD)
         { free(Vector); return GS_BAD; }

      pFas   = (C_FAS *) FAS_list.get_head();
      pColor = (C_COLOR *) ColorsList.get_head();

      while (pFas && pColor)
      {
         pFas->color = *pColor;

         pFas   = (C_FAS *) FAS_list.get_next();
         pColor = (C_COLOR *) ColorsList.get_next();
      }
   }
   
   // Memorizzo l'ultima entità nel disegno
   ads_name elast;
   acdbEntLast(elast); 

   if (gsc_DisplayGridContours(Vector, FAS_list, GridCls.ptr_grid(), 
                               DisplayMode, Join, Fit) == GS_BAD)
      { free(Vector); return RTERROR; }

   free(Vector);

   if (ContoursOnIntervals || ContoursOnStep)
   {
      double LineLength = min(GridCls.ptr_grid()->dx, GridCls.ptr_grid()->dy);

      while (gsc_mainentnext(elast, elast) == GS_GOOD)
         // Setto la scala del tipo linea
         gsc_set_lineTypeScale(elast, gsc_getLTScaleOnTxtLineType(LineLength));
   }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getFAS4ContoursGrid                <external> */
/*+
  Crea una C_FAS con una impostando colore, elevazione, tipolinea e piano.
  Parametri:
  double Elevation;
  C_COLOR &Color;
  const TCHAR *LayerPrefix;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria
-*/  
/*************************************************************/
C_FAS *gsc_getFAS4ContoursGrid(double Elevation, C_COLOR &Color, const TCHAR *LayerPrefix)
{
   C_FAS    *pFAS;
   C_STRING LineType, Layer;

   if ((pFAS = new C_FAS()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   pFAS->color     = Color;
   pFAS->elevation = Elevation;

   // viene usato lo stile di quotatura per memorizzare la descrizione della curva di livello
   pFAS->dimension_style = Elevation;

   LineType.paste(gsc_tostring(Elevation, 20, 4));
   if (gsc_DefineTxtLineType(LineType, LineType) == GS_GOOD)
      gsc_strcpy(pFAS->line, LineType.get_name(), MAX_LEN_LINETYPENAME);

   // Uso la variabile LineType anche per il layer
   if (gsc_get_tmp_layer(LayerPrefix, Layer) == GS_BAD)
      { delete pFAS; return NULL; }
   Layer += _T("_");

   Layer += LineType;
   gsc_strcpy(pFAS->layer, Layer.get_name(), MAX_LEN_LAYERNAME);

   return pFAS;
}
int gsc_getFASList4ContoursGrid(int nIntervals, double MinValue, double MaxValue,
                                C_COLOR &StartColor, C_COLOR &EndColor,
                                const TCHAR *LayerPrefix,
                                C_FAS_LIST &FAS_list)
{
   double       Value, OffSet = 0;
   C_FAS        *pFas;
   C_COLOR_LIST ColorsList;  
   C_COLOR      *pColor;

   FAS_list.remove_all();
   if (nIntervals > 1) OffSet = (MaxValue - MinValue) / (nIntervals - 1);
   Value = MinValue;

   if (ColorsList.getColorsFromTo(StartColor, EndColor, nIntervals) == GS_BAD)
      return GS_BAD;

   pColor = (C_COLOR *) ColorsList.get_head();
   while (pColor)
   {
      if (!pColor->get_next()) // se è l'ultimo
      {
         if ((pFas = gsc_getFAS4ContoursGrid(MaxValue, *pColor, LayerPrefix)) == NULL)
            return GS_BAD;
      }
      else
         if ((pFas = gsc_getFAS4ContoursGrid(Value, *pColor, LayerPrefix)) == NULL)
            return GS_BAD;

      FAS_list.add_tail(pFas);
      Value += OffSet;

      pColor = (C_COLOR *) ColorsList.get_next();
   }

   return GS_GOOD;
}
int gsc_getFASList4ContoursGrid(double EveryStep, double MinValue, double MaxValue,
                                C_COLOR &StartColor, C_COLOR &EndColor,
                                const TCHAR *LayerPrefix,
                                C_FAS_LIST &FAS_list)
{
   double _MinValue = MinValue, _MaxValue = MaxValue, Rem;
   int    nIntervals;

   if ((Rem = fmod(MinValue, EveryStep)) > 0)
      // il valore minimo diventa quello dello step successivo (se ogni 100 e min=160 -> min=200)
      _MinValue = (MinValue - Rem) + EveryStep;

   if ((Rem = fmod(MaxValue, EveryStep)) > 0)
      // il valore massimo diventa quello dello step precedente (se ogni 100 e max=160 -> max=100)
      _MaxValue = MaxValue - Rem;

   if (_MinValue > MaxValue || _MaxValue < MinValue)
      { FAS_list.remove_all(); return GS_GOOD; }
   
   nIntervals = (int) (((_MaxValue - _MinValue) / EveryStep) + 1);

   return gsc_getFASList4ContoursGrid(nIntervals, _MinValue, _MaxValue, 
                                      StartColor, EndColor, LayerPrefix, FAS_list);
}


///////////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER INTERPOLAZIONE SPAZIALE
// INIZIO FUNZIONI PER SPOSTAMENTO OGGETTO SULLA GRIGLIA
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_getCoeff3DPlane                    <external> */
/*+
   La funzione calcola i 3 coefficienti dell'equazione del
   piano passante per tre punti noti.
   Parametri:
   ads_point p1;
   ads_point p2;
   ads_point p3;
-*/  
/*************************************************************/
void gsc_getCoeff3DPlane(ads_point p1, ads_point p2, ads_point p3,
                         double *a, double *b, double *c)
{
   // Siano P1=(x1,y1), P2=(x2,y2), P3=(x3,y3) i tre vertici del triangolo in cui
   // cade il punto P, e siano z1, z2, z3 le quote corrispondenti. 
   // L'equazione del piano passante per i tre vertici mappati in 3D e' la seguente: 

   // (x-x1)a + (y-y1)b + (z-z1)c = 0

   // dove 
   // a = (y1-y2)(z1-z3)-(z1-z2)(y1-y3)
   // b = (z1-z2)(x1-x3)-(x1-x2)(z1-z3)
   // c = (x1-x2)(y1-y3)-(y1-y2)(x1-x3)

   *a = (p1[Y] - p2[Y]) * (p1[Z] - p3[Z]) - (p1[Z] - p2[Z]) * (p1[Y] - p3[Y]);
   *b = (p1[Z] - p2[Z]) * (p1[X] - p3[X]) - (p1[X] - p2[X]) * (p1[Z] - p3[Z]);
   *c = (p1[X] - p2[X]) * (p1[Y] - p3[Y]) - (p1[Y] - p2[Y]) * (p1[X] - p3[X]);
}


/*********************************************************/
/*.doc gsc_getQuote3DPlane                    <external> */
/*+
   La funzione calcola la quota di un punto noto sul piano passante
   per tre punti noti.
   Parametri:
   ads_point p1;
   ads_point p2;
   ads_point p3;
   ads_point pt;  punto di cui si deve calcolare la quota
-*/  
/*************************************************************/
int gsc_getQuote3DPlane(ads_point p1, ads_point p2, ads_point p3,
                        ads_point pt)
{
   double a, b, c;

   // Siano P1=(x1,y1), P2=(x2,y2), P3=(x3,y3) i tre vertici del triangolo in cui
   // cade il punto P, e siano z1, z2, z3 le quote corrispondenti. 
   // L'equazione del piano passante per i tre vertici mappati in 3D e' la seguente: 

   // (x-x1)a + (y-y1)b + (z-z1)c = 0

   // in cui bisogna sostituire le due coordinate (xP,yP) di P e ricavare la z: 
   // z = ( -((xP-x1)a + (yP-y1)b) / c ) + z1

   // Calcolo i coefficienti a, b, c
   gsc_getCoeff3DPlane(p1, p2, p3, &a, &b, &c);

   if (c == 0) return GS_BAD; // non esiste il punto sul piano

   pt[Z] = (- ((pt[X] - p1[X]) * a + (pt[Y] - p1[Y]) * b) / c) + p1[Z];

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Find3DPtsOnCell                    <internal> */
/*+
   Funzione di appoggio alla ObjectOnGrid che trova
   i punti dei quattro vertici della cella. 
   Parametri:
   C_CGRID     *pClass;      puntatore alle classe della griglia
   AcGePoint3d &pt;          punto sul territorio la cui Z viene settata da
                             questa funzione
   C_PREPARED_CMD_LIST &CmdList; Comando preparato per l'interrogazione delle celle
   C_RB_LIST   &ColValues;   Lista di resbuf per lettura valori cella (già inizializzata)
   presbuf     pZValue;      puntatore a resbuf contenente la quota (già inizializzato)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_Find3DPtsOnCell(C_CGRID *pClass, AcGePoint3d &pt,
                        C_PREPARED_CMD_LIST &CmdList, C_RB_LIST &ColValues, presbuf pZValue)
{
   C_GRID    *pGrid = pClass->ptr_grid();
   long      Key;
   double    LimitX = pGrid->x + pGrid->dx * (pGrid->nx - 1);
   double    LimitY = pGrid->y + pGrid->dy * (pGrid->ny - 1);
   ads_point _pt, pt1, pt2, pt3, pt4;

   //         pt4---pt3
   //          |  /  |
   //          | /   |
   //         pt1---pt2

   _pt[X] = pt.x; _pt[Y] = pt.y;
   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della cella corrispondente
   if (pGrid->pt2key(_pt, &Key) == GS_BAD) return GS_BAD;
   if (pGrid->key2pt(Key, pt1, FALSE) == GS_BAD) return GS_BAD;
   if (pClass->query_data(Key, ColValues, &CmdList) == GS_BAD) return GS_BAD;
   if (gsc_rb2Dbl(pZValue, &(pt1[Z])) == GS_BAD) return GS_BAD;

   pt4[X] = pt1[X];
   pt2[Y] = pt1[Y];
   pt2[X] = pt3[X] = pt1[X] + pGrid->dx;
   pt3[Y] = pt4[Y] = pt1[Y] + pGrid->dy;

   // leggo le quote dalle celle adiacenti
   // se NON ci sono celle a sinistra o sopra
   if (pt3[X] > LimitX || pt3[Y] > LimitY) return GS_BAD;

   // leggo da DB pt3
   if (pClass->query_data(Key + pGrid->nx + 1, ColValues, &CmdList) == GS_BAD) return GS_BAD;
   if (gsc_rb2Dbl(pZValue, &(pt3[Z])) == GS_BAD) return GS_BAD;

   // Se si trova nel triangolo pt1-pt2-pt3
   if (((_pt[Y] - pt1[Y]) / (_pt[X] - pt1[X])) < 1)
   {
      // leggo da DB pt2
      if (pClass->query_data(Key + 1, ColValues, &CmdList) == GS_BAD) return GS_BAD;
      if (gsc_rb2Dbl(pZValue, &(pt2[Z])) == GS_BAD) return GS_BAD;
      if (gsc_getQuote3DPlane(pt1, pt2, pt3, _pt) == GS_BAD) return GS_BAD;
      pt.z = _pt[Z];
      
      return GS_GOOD;
   }
   
   // Se non si trovava nel triangolo pt1-pt2-pt3 si trova in quello pt1-pt3-pt4
   // leggo da DB pt4
   if (pClass->query_data(Key + pGrid->nx, ColValues, &CmdList) == GS_BAD) return GS_BAD;
   if (gsc_rb2Dbl(pZValue, &(pt4[Z])) == GS_BAD) return GS_BAD;
   if (gsc_getQuote3DPlane(pt1, pt3, pt4, _pt) == GS_BAD) return GS_BAD;
   pt.z = _pt[Z];
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_Add2DIntersectionInColumn          <internal> */
/*+
   Funzione che ricava una lista di punti di intersezione delle celle 
   di una colonna della griglia con una linea di cui sono noti
   i coefficienti "a" e "b". Si parte da una riga nota. I punti di
   intersezione sono aggiunti ad una lista esistente.
   Parametri:
   C_GRID *pGrid;         Definizione della griglia
   long Column;           Colonna in cui si devono trovare le intersezioni
   long *Row;             Riga iniziale
   long EndRow;           Riga finale
   double a;              Coefficiente a dell'equazione della retta
   double b;              Coefficiente b dell'equazione della retta
   C_POINT_LIST &PtList;  Lista ordinata dei punti di intersezione 
                          (che vanno da ptSource a ptDest)
   double *XLimit;        Opzionale. Limite X (non compreso) che non 
                          può essere superato (default = NULL).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*************************************************************/
int gsc_Add2DIntersectionInColumn(C_GRID *pGrid,
                                  long Column, long *Row, long EndRow,
                                  double a, double b,
                                  C_POINT_LIST &PtList, double *XLimit = NULL)
{
   long      NextRow;
   ads_point pt;
   C_POINT   *pPt;

   if (a > 0) // dal basso in alto
   {
      NextRow = *Row + 1;

      while (NextRow <= EndRow)
      {
         // prendo la coordinata Y della riga successiva
         pGrid->RowColumn2pt(Column, NextRow, pt, false);

         // Calcolo il punto di intersezione della retta passante per i 2 punti noti
         // con una retta orizzontale che rappresenta la riga della griglia
         if (a != (std::numeric_limits<double>::max)())
            pt[X] = (pt[Y] - b) / a;
         else
            pt[X] = b;

         // esco se si tratta di un'altra colonna
         if (pGrid->getColumn(pt) != Column) break;
         // esco se è stato impostato un limite X da non raggiungere
         if (XLimit && pt[X] >= *XLimit) break;

         // memorizzo il punto
         if ((pPt = new C_POINT(pt)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         PtList.add_tail(pPt);
         
         *Row = NextRow;
         NextRow++; // incremento la riga
      }
   }
   else // dall'alto in basso
   {
      NextRow = *Row - 1;

      while (NextRow >= EndRow)
      {
         // prendo la coordinata Y della riga attuale
         pGrid->RowColumn2pt(Column, *Row, pt, false);

         // Calcolo il punto di intersezione della retta passante per i 2 punti noti
         // con una retta orizzontale che rappresenta la riga della griglia
         if (a != (std::numeric_limits<double>::min)())
            pt[X] = (pt[Y] - b) / a;
         else
            pt[X] = b;

         // esco se si tratta di un'altra colonna
         if (pGrid->getColumn(pt) != Column) break;
         // esco se è stato impostato un limite X da non raggiungere
         if (XLimit && pt[X] >= *XLimit) break;

         // memorizzo il punto
         if ((pPt = new C_POINT(pt)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         PtList.add_tail(pPt);
         
         *Row = NextRow;
         NextRow--; // decremento la riga
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get2DIntersectionPtsBetweenLine      <internal> */
/*+
   Funzione che ricava una lista di punti di intersezione delle celle
   della griglia con una linea di cui sono noti i punti iniziale e finale
   (esclusi). La lista dei punti è ordinata partendo dal punto iniziale verso
   il punto finale (esclusi).
   Parametri:
   C_GRID *pGrid;         Definizione della griglia
   ads_point ptSource;    Punto iniziale della linea
   ads_point ptDest;      Punto finale della linea
   C_POINT_LIST &PtList;  Lista ordinata dei punti di intersezione 
                          (che vanno da ptSource a ptDest)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*************************************************************/
int gsc_get2DIntersectionPtsBetweenLine(C_GRID *pGrid,
                                        AcGePoint3d ptSource, AcGePoint3d ptDest,
                                        C_POINT_LIST &PtList)
{
   double    a, b, dummy;
   ads_point pt1, pt2, pt; // pt1 punto a sinistra e pt2 a destra
   long      Key, Row, Column, EndColumn, EndRow, NextColumn, NextRow;
   bool      Inverse = false;
   C_POINT   *pPt;

   PtList.remove_all();
   // Se i punti coincidono
   if (ptSource.x == ptDest.x && ptSource.y == ptDest.y) return GS_GOOD;

   if (ptSource.x < ptDest.x) // se la linea va da sinistra a destra
   {
      ads_2Dpoint_set_from_AcGePoint3d(ptSource, pt1);
      ads_2Dpoint_set_from_AcGePoint3d(ptDest, pt2);
   }
   else 
   if (ptSource[X] > ptDest[X]) // se la linea va da destra a sinistra
   {
      ads_2Dpoint_set_from_AcGePoint3d(ptDest, pt1);
      ads_2Dpoint_set_from_AcGePoint3d(ptSource, pt2);

      Inverse = true; // Alla fine bisogna invertire l'ordine dei punti
   }
   else // se la linea è verticale
   {
      ads_2Dpoint_set_from_AcGePoint3d(ptSource, pt1);
      ads_2Dpoint_set_from_AcGePoint3d(ptDest, pt2);

      if (pt1[Y] > pt2[Y])
      {
         dummy  = pt2[Y];
         pt2[Y] = pt1[Y];
         pt1[Y] = dummy;
         Inverse = true; // Alla fine bisogna invertire l'ordine dei punti
      }
   }

   if (pGrid->pt2key(pt1, &Key) == GS_BAD)
      return GS_GOOD; // punto esterno alla griglia
   Row    = pGrid->getRow(Key);
   Column = pGrid->getColumn(Key);

   if (pGrid->pt2key(pt2, &Key) == GS_BAD)
      return GS_GOOD; // punto esterno alla griglia
   EndRow    = pGrid->getRow(Key);
   EndColumn = pGrid->getColumn(Key);

   // Calcolo l'equazione della retta passante per 2 punti y = ax + b 
   if (ptSource[X] == ptDest[X]) // retta verticale
   {
      // Calcolo a
      if (ptSource[Y] < ptDest[Y]) // verso l'alto
         a = (std::numeric_limits<double>::max)();
      else // verso il basso
         a = (std::numeric_limits<double>::min)();

      b = ptSource[X];
   }
   else
   {
      // Calcolo a
      a = (pt1[Y] - pt2[Y]) / (pt1[X] - pt2[X]);
      // Calcolo b
      b = pt1[Y] - (a * pt1[X]);
   }

   NextColumn = Column + 1;
   while (NextColumn <= EndColumn)
   {
      pGrid->RowColumn2pt(NextColumn, Row, pt, false);

      // Calcolo il punto di intersezione della retta passante per i 2 punti noti
      // con una retta verticale che rappresenta la colonna della griglia
      pt[Y] = a * pt[X] + b;
      // Calcolo la riga di appartenenza di pt
      NextRow = pGrid->getRow(pt);

      if (NextRow != Row) // siamo in righe diverse
         // Cerco le intersezione nella colonna Column
         if (gsc_Add2DIntersectionInColumn(pGrid, Column, &Row, EndRow,
                                           a, b, PtList) == GS_BAD)
            return GS_BAD;

      if (!ads_2Dpoint_equal(pt, pt2))
      {
         // memorizzo il punto
         if ((pPt = new C_POINT(pt)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         PtList.add_tail(pPt);
      }

      Column = NextColumn;
      NextColumn++; // incremento la colonna
   }

   if (Row != EndRow)
      // Cerco le intersezione nella colonna Column fino a pt2[X] (non compreso)
      if (gsc_Add2DIntersectionInColumn(pGrid, Column, &Row, EndRow,
                                        a, b, PtList, &(pt2[X])) == GS_BAD)
         return GS_BAD;

   if (Inverse)
      PtList.Inverse();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_ObjectsOnGrid                       <external> */
/*+
   Funzione che sposta un oggetto "adagiandolo" sulla griglia.
   Parametri:
   (<prj> <cls> <sub> <selset> <z attrib>)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_ObjectsOnGrid(void)
{
   presbuf   arg = acedGetArgs();
   C_STRING  AttribZ;
   int       prj, cls, sub;
   C_CGRID   *pCls;
   C_SELSET  SelSet;

   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // gruppo di selezione degli oggetti
   if (arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SelSet << arg->resval.rlname;
   SelSet.ReleaseAllAtDistruction(GS_BAD);

   // nome dell'attributo contenente il valore Z dei punti
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribZ = arg->resval.rstring;

   if (pCls->ObjectOnGrid(SelSet, AttribZ) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}
/*********************************************************/
/*.doc C_CGRID::ObjectOnGrid                  <external> */
/*+
   Funzione che sposta un oggetto "adagiandolo" sulla griglia.
   C_SELSET &SelSet;    Gruppo di oggetti da spostare
   C_STRING &AttribZ;   Nome attributo griglia contenente la Z

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::ObjectOnGrid(C_SELSET &SelSet, C_STRING &AttribZ)
{
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           ColValues;
   presbuf             pZValue;
   long                i = 0, Accepted = 0;
   ads_name            ent, NewEnt;

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(CmdList) == GS_BAD) return GS_BAD;
   // Faccio una lettura solo per inizializzare <ColValues> e <pZValue>
   if (query_data(1, ColValues, &CmdList) == GS_BAD) return GS_BAD;
   if (!(pZValue = ColValues.CdrAssoc(AttribZ.get_name()))) return GS_BAD;

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1088)); // "Drape"
   StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(i);

      if (ObjectOnGrid(ent, NewEnt, CmdList, ColValues, pZValue) == GS_GOOD)
         Accepted++;
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   acutPrintf(gsc_msg(308), Accepted, i - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf(GS_LFSTR);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::ObjectOnGrid            <internal> */
/*+
   Funzione che sposta un oggetto "adagiandolo" sulla griglia.
   ads_name ent;         Oggetto grafico
   ads_name NewEnt;      Se NewEnt è valido (NewEnt[0] e NewEnt[1] <> NULL)
                         Allora è stato necessario creare un nuovo oggetto
                         (senza cancellare l'originale) 
                         altrimenti è stato modificato l'oggetto originale.
   C_PREPARED_CMD_LIST &CmdList; Comandi preparati per lettura dati griglia
   C_RB_LIST &ColValues;   Lista di resbuf per lettura dati griglia
   presbuf pZValue;        Puntatore a resbuf contenente la Z      

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::ObjectOnGrid(ads_name ent, ads_name NewEnt,
                          C_PREPARED_CMD_LIST &CmdList,
                          C_RB_LIST &ColValues, presbuf pZValue)
{
   AcDbObjectId objId;
   AcDbEntity   *pEnt;
   AcGePoint3d  Pt;

   ads_name_clear(NewEnt);

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;

   if (acdbOpenObject(pEnt, objId, AcDb::kForWrite) != Acad::eOk) return GS_BAD;

   if (pEnt->isKindOf(AcDb2dPolyline::desc())) // 2D entity type
   {
      ads_name dummy;

      // Creo una 3D entity type
      if (gsc_CloneTo3dPolyline((AcDb2dPolyline *) pEnt) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
      if (acdbEntLast(NewEnt) != RTNORM) { pEnt->close(); return GS_BAD; }
      if (ObjectOnGrid(NewEnt, dummy, CmdList, ColValues, pZValue) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
   }
   else
   if (pEnt->isKindOf(AcDb3dPolyline::desc())) // 3D entity type
   {
      AcDbObjectIterator   *pVertIter;
      AcDbObjectId         vertexObjId, prevVertexObjId, newVertexObjId;
      AcDb3dPolylineVertex *pVertex, *pPrevVertex, *pNewVertex;
      AcGePoint3d          PrevPt;
      bool                 FirstPt = true;
      C_POINT_LIST         PtList;

      // Aggiungo nuovi vertici in corrispondenza delle intersezioni
      // con i bordi delle celle
      pVertIter = ((AcDb3dPolyline *)pEnt)->vertexIterator();
      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForRead) == Acad::eOk)
         {
            if (FirstPt)
               FirstPt = false;
            else
                // calcolo eventuali intersezioni con la griglia
               if (gsc_get2DIntersectionPtsBetweenLine(ptr_grid(), 
                                                       pPrevVertex->position(),
                                                       pVertex->position(),
                                                       PtList) == GS_GOOD)
               {  // aggiungo i punti
                  C_POINT *pPt = (C_POINT *) PtList.get_head();

                  while (pPt)
                  {
                     if ((pNewVertex = new AcDb3dPolylineVertex()) == NULL)
                        { pVertex->close(); delete pVertIter; GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                     Pt.set(pPt->x(), pPt->y(), 0.0);
                     pNewVertex->setPosition(Pt);

                     if (((AcDb3dPolyline *)pEnt)->insertVertexAt(newVertexObjId,
                                                                  prevVertexObjId, 
                                                                  pNewVertex) != Acad::eOk)
                        { pVertex->close(); delete pVertIter; delete pNewVertex; return GS_BAD; }

                     pNewVertex->close();
                     pPrevVertex = pNewVertex;
                     prevVertexObjId = pNewVertex->objectId();

                     pPt = (C_POINT *) PtList.get_next();
                  }
               }

            pVertex->close();
            pPrevVertex = pVertex;
            prevVertexObjId = vertexObjId;
         }       
      }

      // Sposto i vertici adagiandoli sulla griglia
      pVertIter->start(); // mi posiziono sul primo vertice
      for (; !pVertIter->done(); pVertIter->step())
      {
         // Leggo vertice
         vertexObjId = pVertIter->objectId();
         if (acdbOpenObject(pVertex, vertexObjId, AcDb::kForWrite) == Acad::eOk)
         {
            Pt = pVertex->position();

            // trovo il punto della cella corrispondente
            if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
               pVertex->setPosition(Pt);
            pVertex->close();
         }         
      }
      delete pVertIter;
   }
   else
   if (pEnt->isKindOf(AcDbHatch::desc())) // per ora da non trattare
   {
      pEnt->close();
      return GS_BAD;
   }
   else      
   if (pEnt->isKindOf(AcDbArc::desc())) // funziona solo per archi paralleli al piano orizz.
   {
      Pt = ((AcDbArc *) pEnt)->center();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbArc *) pEnt)->setCenter(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {  //  Group entity
      Pt = ((AcDbAttribute *) pEnt)->position(); 
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbAttribute *) pEnt)->setPosition(Pt);
   }
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {  //  Group entity
      Pt = ((AcDbAttributeDefinition *) pEnt)->position();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbAttributeDefinition *) pEnt)->setPosition(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbBlockReference::desc()))
   {  //  Group entity
      Pt = ((AcDbBlockReference *) pEnt)->position();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbBlockReference *) pEnt)->setPosition(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbCircle::desc()))
   {
      Pt = ((AcDbCircle *) pEnt)->center();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbCircle *) pEnt)->setCenter(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbDimension::desc()))
   {
      Pt = ((AcDbDimension *) pEnt)->textPosition();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbDimension *) pEnt)->setTextPosition(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbEllipse::desc()))
   {
      Pt = ((AcDbEllipse *) pEnt)->center();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbEllipse *) pEnt)->setCenter(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbLeader::desc()))
   {
      int numVerts = ((AcDbLeader *) pEnt)->numVertices();

      for (int i = 0; i < numVerts; i++)
      {
         Pt = ((AcDbLeader *) pEnt)->vertexAt(i);
         // trovo il punto della cella corrispondente
         if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
            ((AcDbLeader *) pEnt)->setVertexAt(i, Pt);
      }
   }
   else
   if (pEnt->isKindOf(AcDbLine::desc()))
   {
      ads_name dummy;

      // Creo una 3D entity type
      if (gsc_CloneTo3dPolyline((AcDbLine *) pEnt) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
      if (acdbEntLast(NewEnt) != RTNORM) { pEnt->close(); return GS_BAD; }
      if (ObjectOnGrid(NewEnt, dummy, CmdList, ColValues, pZValue) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      Pt = ((AcDbMText *) pEnt)->location();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbMText *) pEnt)->setLocation(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbPoint::desc()))
   {
      Pt = ((AcDbPoint *) pEnt)->position();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbPoint *) pEnt)->setPosition(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbPolyline::desc()))
   {
      ads_name dummy;

      // Creo una 3D entity type
      if (gsc_CloneTo3dPolyline((AcDbPolyline *) pEnt) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
      if (acdbEntLast(NewEnt) != RTNORM) { pEnt->close(); return GS_BAD; }
      if (ObjectOnGrid(NewEnt, dummy, CmdList, ColValues, pZValue) == GS_BAD)
         { pEnt->close(); return GS_BAD; }
   }
   else
   if (pEnt->isKindOf(AcDbRay::desc()))
   {
      Pt = ((AcDbRay *) pEnt)->basePoint();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbRay *) pEnt)->setBasePoint(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      Pt = ((AcDbText *) pEnt)->position();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         if (((AcDbText *) pEnt)->horizontalMode() == AcDb::kTextLeft &&
             ((AcDbText *) pEnt)->verticalMode() == AcDb::kTextBase)
            ((AcDbText *) pEnt)->setPosition(Pt);
         else
            ((AcDbText *) pEnt)->setAlignmentPoint(Pt);
   }
   else
   if (pEnt->isKindOf(AcDbXline::desc()))
   {
      Pt = ((AcDbXline *) pEnt)->basePoint();
      // trovo il punto della cella corrispondente
      if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_GOOD)
         ((AcDbXline *) pEnt)->setBasePoint(Pt);
   }

   pEnt->close();

   C_CLASS   *pCls;
   long      Key;

   // Se l'oggetto appartiene a una classe di GEOsim
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)))
   {
      C_RB_LIST ColValues;

      if (ads_name_nil(NewEnt)) // Se non è stata creata una nuova entità
      {  // La aggiorno
         if (pCls->ptr_info()) // Se ha DB collegato
         {
            C_PREPARED_CMD_LIST CmdList;

            // Preparo i comandi di lettura dei dati della classe dal temp/old
            if (pCls->prepare_data(CmdList) == GS_BAD) return GS_BAD;

            if (pCls->getKeyValue(ent, &Key) == GS_BAD ||
                pCls->query_data(Key, ColValues, &CmdList) == GS_BAD ||
                pCls->upd_data(Key, ColValues, 
                               ((C_PREPARED_CMD *) CmdList.get_head())) == GS_BAD)
               return GS_BAD;
         }
         else // Se non ha DB collegato
            if (pCls->upd_data(ent, ColValues) == GS_BAD) return GS_BAD;
      }
      else // Se è stata creata una nuova entità
      {
         if (pCls->ptr_info()) // Se ha DB collegato
         {
            C_SELSET dummySS;
            ads_name dummy;

            // la aggrego all'originale
            dummySS.clear();
            dummySS.add(NewEnt);
            dummySS.get_selection(dummy);
            if (pCls->aggr_data(dummy, ent) == GS_BAD) return GS_BAD;
            // cancello l'originale
            if (pCls->erase_data(ent) == GS_BAD) return GS_BAD;
         }
         else // Se non ha DB collegato
         {
            if (pCls->ins_data(NewEnt, ColValues) == GS_BAD) return GS_BAD;
            if (pCls->erase_data(ent) == GS_BAD) return GS_BAD;
         }
      }
   }
   else // Se l'oggetto NON appartiene a una classe di GEOsim
      if (!ads_name_nil(NewEnt)) // Se è stata creata una nuova entità
         // cancello l'originale
         gsc_EraseEnt(ent);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::SetAttribEntity               <internal> */
/*+
   Funzione che setta il valore di un attributo di una entità di GEOsim
   leggendolo da una attributo della cella della griglia corrispondente.
   ads_name ent;         Oggetto grafico
   C_STRING &AttribDest; Nome dell'attributo destinazione.
   C_STRING &AttribZ;    Nome dell'attributo sorgente della griglia.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_SetAttribEntityFromGrid(void)
{
   presbuf   arg = acedGetArgs();
   C_STRING  SrcAttrib, DstAttrib;
   int       prj, cls, sub;
   C_CGRID   *pCls;
   C_SELSET  SelSet;

   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // gruppo di selezione degli oggetti
   if (arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SelSet << arg->resval.rlname;
   SelSet.ReleaseAllAtDistruction(GS_BAD);

   // nome dell'attributo destinazione
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   DstAttrib = arg->resval.rstring;

   // nome dell'attributo sorgente della griglia
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SrcAttrib = arg->resval.rstring;

   if (pCls->SetAttribEntity(SelSet, DstAttrib, SrcAttrib) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}
int C_CGRID::SetAttribEntity(C_SELSET &SelSet, C_STRING &DstAttrib, C_STRING &SrcAttrib)
{
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           ColValues;
   presbuf             pZValue;
   long                i = 0, Accepted = 0;
   ads_name            ent;

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(CmdList) == GS_BAD) return GS_BAD;
   // Faccio una lettura solo per inizializzare <ColValues> e <pZValue>
   if (query_data(1, ColValues, &CmdList) == GS_BAD) return GS_BAD;
   if (!(pZValue = ColValues.CdrAssoc(SrcAttrib.get_name()))) return GS_BAD;

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1063)); // Elaborazione oggetti grafici"
   StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(i);

      if (gsc_is_DABlock(ent) == GS_BAD) // Scarto i blocchi DA
         if (SetAttribEntity(ent, DstAttrib, CmdList, ColValues, pZValue) == GS_GOOD)
            Accepted++;
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   acutPrintf(gsc_msg(308), Accepted, i - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf(GS_LFSTR);

   return GS_GOOD;
}
int C_CGRID::SetAttribEntity(ads_name ent, C_STRING &DstAttrib,
                             C_PREPARED_CMD_LIST &CmdList,
                             C_RB_LIST &ColValues, presbuf pZValue)
{
   ads_point           point;
   AcGePoint3d         Pt;
   C_CLASS             *pCls;
   long                Key;
   C_RB_LIST           EntityColValues;
   C_PREPARED_CMD_LIST EntityCmdList;

   // Se l'oggetto NON appartiene a una classe di GEOsim
   if (!(pCls = GS_CURRENT_WRK_SESSION->find_class(ent))) return GS_BAD;
   if (!pCls->ptr_info()) // Se NON ha DB collegato
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (gsc_get_firstPoint(ent, point) == GS_BAD) return GS_BAD;
   AcGePoint3d_set_from_ads_point(point, Pt);

   // trovo il punto della cella corrispondente
   if (gsc_Find3DPtsOnCell(this, Pt, CmdList, ColValues, pZValue) == GS_BAD)
      return GS_BAD;

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (pCls->prepare_data(EntityCmdList) == GS_BAD) return GS_BAD;

   if (pCls->getKeyValue(ent, &Key) == GS_BAD ||
       pCls->query_data(Key, EntityColValues, &EntityCmdList) == GS_BAD)
      return GS_BAD;
   // Aggiorno il valore
   if (EntityColValues.CdrAssocSubst(DstAttrib.get_name(), pZValue) == GS_BAD)
      return GS_BAD;
   if (pCls->upd_data(Key, EntityColValues, 
                      ((C_PREPARED_CMD *) EntityCmdList.get_head())) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PER RICERCA FIUMI E CRINALI-MONTI
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_AllocInitMultiDimMatrixGrid        <external> */
/*+
   La funzione alloca una matrice multidimensionale per il calcolo
   dei fiumi e crinali e la inizializza al numero più basso double.
   Ogni cella viene divisa in 2 triangoli: il trinagolo 1 e quello 2.
        +-----+
        | 2 / |
        | / 1 |
        +-----+
   Ogni triangolo deve memorizzare un valore per ciascun cateto:
   cateto 1, cateto 2, cateto 3.
                                                      1
   per il               +        per il            +-----+
   triangolo 1 ->   3 / |        triangolo 2 ->   2|   /  
                    /   | 2                        | / 3  
                  +-----+                          +
                     1                       
   Parametri:
   double len;
   double ang1;
   double ang2;
   N.B. Alloca memoria.
-*/  
/*************************************************************/
int gsc_AllocInitMultiDimMatrixGrid(C_GRID *pGrid, double **Matrix, long *MatrixLen)
{
   *MatrixLen = (pGrid->nx * pGrid->ny) + 1; // conto anche il record 0
   if ((*Matrix = new double[*MatrixLen, 2, 3]) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   // inizializzo vettore con il minimo per un double
   for (long i = 0; i < *MatrixLen; i++)
      for (long j = 0; i < 2; i++)
         for (long k = 0; i < 3; i++)
            (*Matrix)[i, j, k] = (std::numeric_limits<double>::min)();

   return GS_GOOD;
}
void gsc_DeallocMultiDimMatrixGrid(double *Matrix)
{
   if (Matrix) delete [] Matrix;
}


/*********************************************************/
/*.doc gsc_getAreaTriang                       <external> */
/*+
   La funzione calcola l'area del triangolo i cui sono noti la
   lunghezza di un cateto e gli angoli a inizio e fine cateto.
   Parametri:
   double len;
   double ang1;
   double ang2;
-*/  
/*************************************************************/
double gsc_getAreaTriang(double len, double ang1, double ang2)
{
   //                 /^\
   //               / h|  \
   //             /    |    \
   //     ang1  /---a--+--b---\ ang2
   //                 len
   // la tang di ang1 = h/a -> a = h/tg(ang1)
   // la tang di ang2 = h/b -> b = h/tg(ang2)
   // len = a + b
   // ne consegue che:
   // len = (h/tg(ang1)) + (h/tg(ang2))
   // len = h * (1/tg(ang1) + 1/tg(ang2))
   // h = len / (1/tg(ang1) + 1/tg(ang2))
   // Area = len * h / 2
   
   double h;

   h = len / (1 / tan(ang1) + 1 / tan(ang2));
   return len * h / 2;
}


/*********************************************************/
/*.doc gsc_getPerc3DSides                     <external> */
/*+
   La funzione calcola 3 coefficienti che stabiliscono la percentuale 
   di acqua che dovrebbe scorrere per il lato 1-2, 2-3, 3-1 del triangolo
   passante per tre punti noti.
            pt4---pt3
             |  /  |
             | /   | l
            pt1---pt2
                b

   Per il triangolo 1-3-4 i punti da passare sono in sequenza p3, p4, p1

   Parametri:
   ads_point p1;
   ads_point p2;
   ads_point p3;
   double    *Perc_1_2;
   double    *Perc_2_3;
   double    *Perc_3_1;
-*/  
/*************************************************************/
void gsc_getPerc3DSides(ads_point p1, ads_point p2, ads_point p3,
                        double *Perc_1_2, double *Perc_3_2, double *Perc_3_1)
{
   double Coeff_1_2, Coeff_3_2, ang, b, l;
   double area, area_3_2, area_1_2;
   double ang3, ang2, ang1;

   // Siano P1=(x1,y1), P2=(x2,y2), P3=(x3,y3) i tre vertici del triangolo
   // e siano z1, z2, z3 le quote corrispondenti. 
   // il coefficiente angolare del segmento che va da P1 a P2 = z1-z2
   // il coefficiente angolare del segmento che va da P3 a P2 = z3-z2

   Coeff_1_2 = p1[Z] - p2[Z];
   Coeff_3_2 = p3[Z] - p2[Z];

   // se il coefficiente angolare Coeff_1_2 > 0 esce acqua dal lato 3-2 e forse 3-1
   if (Coeff_1_2 > 0)
   {
      // se il coefficiente angolare Coeff_3_2 = 0 esce acqua solo dal lato 3-2
      if (Coeff_3_2 == 0)
      {
         *Perc_3_2 = 100;
         *Perc_1_2 = *Perc_3_1 = 0.0;
         return;
      }

      b    = fabs(p2[X] - p1[X]);
      l    = fabs(p3[Y] - p2[Y]);
      area = b * l / 2;
      ang3 = atan(b / l); // angolo in p3

      // se il coefficiente angolare Coeff_3_2 > 0 esce acqua dai lati 3-2 e 1-2
      if (Coeff_3_2 > 0)
      {
         ang       = atan(Coeff_1_2 / Coeff_3_2);
         area_3_2  = gsc_getAreaTriang(l, ang, ang3);
         *Perc_3_2 = 100.0 * area_3_2 / area;
         *Perc_1_2 = 100.0 - *Perc_3_2;
         *Perc_3_1 = 0.0;
         return;
      }
      // altrimenti esce acqua dai lati 3-2 e 3-1
      ang = atan(Coeff_1_2 / (-1 * Coeff_3_2));
      if (ang >= ang3) // tutta l'acqua cade su lato 3-2
      {
         *Perc_3_2 = 100.0;
         *Perc_1_2 = *Perc_3_1 = 0.0;
         return;
      }

      ang2      = PI / 2; // angolo in p2 = 90 gradi
      area_3_2  = gsc_getAreaTriang(l, ang2, ang);
      *Perc_3_2 = 100.0 * area_3_2 / area;
      *Perc_3_1 = 100.0 - *Perc_3_2;
      *Perc_1_2 = 0.0;
      return;
   }

   // se il coefficiente angolare Coeff_1_2 = 0 esce acqua sul lato 1-2 e 3-1
   if (Coeff_1_2 == 0)
   {
      // se il coefficiente angolare Coeff_3_2 > 0 esce acqua solo dal lato 1-2
      if (Coeff_3_2 > 0)
      {
         *Perc_1_2 = 100;
         *Perc_3_2 = *Perc_3_1 = 0.0;
         return;
      }
      // se il coefficiente angolare Coeff_3_2 < 0 esce acqua solo dal lato 3-1
      if (Coeff_3_2 < 0)
      {
         *Perc_3_1 = 100;
         *Perc_3_2 = *Perc_1_2 = 0.0;
         return;
      }
      // altrimenti il triangolo è piano
      *Perc_1_2 = *Perc_3_2 = *Perc_3_1 = 100.0 / 3.0; // 1/3 per lato
      return;
   }

   // altrimenti se il coefficiente angolare Coeff_1_2 < 0 esce acqua sul lato 1-2 e 3-1
   // se il coefficiente angolare Coeff_3_2 = 0 esce acqua solo dal lato 3-1
   if (Coeff_3_2 == 0)
   {
      *Perc_3_1 = 100;
      *Perc_1_2 = *Perc_3_2 = 0.0;
      return;
   }

   b    = fabs(p2[X] - p1[X]);
   l    = fabs(p3[Y] - p2[Y]);
   area = b * l / 2;
   ang1 = atan(l / b); // angolo in p1

   // se il coefficiente angolare Coeff_3_2 > 0 esce acqua dai lati 3-1 e 1-2
   if (Coeff_3_2 > 0)
   {
      ang = atan(Coeff_3_2 / (-1 * Coeff_1_2));
      if (ang >= ang1) // tutta l'acqua cade su lato 1-2
      {
         *Perc_1_2 = 100.0;
         *Perc_3_2 = *Perc_3_1 = 0.0;
         return;
      }

      ang2      = PI / 2; // angolo in p2 = 90 gradi
      area_1_2  = gsc_getAreaTriang(b, ang, ang2);
      *Perc_1_2 = 100.0 * area_1_2 / area;
      *Perc_3_1 = 100.0 - *Perc_1_2;
      *Perc_3_2 = 0.0;
      return;
   }
   // altrimenti tutta l'acqua cade su lato 3-1
   *Perc_3_1 = 100.0;
   *Perc_1_2 = *Perc_3_2 = 0.0;
   return;
}


/*********************************************************/
/*.doc gsc_DisplayValleyOrRidge               <internal> */
/*+
   Funzione di appoggio alla C_CGRID::DisplayValleyOrRidge
   che calcola dove sono i fiumi o i crinali.
   Parametri:
   C_CGRID *pClass;      classe griglia
   double *Vector;       Vettore rappresentante la griglia
   long KeyMin;          Codice iniziale della cella da verificare
   long KeyMax;          Codice finale della cella da verificare
   long      *i;         contatore
   bool Valley;          Flag, se = TRUE il programma valuta le valli altrimenti i
                         crinali dei monti (default = TRUE)
   int Mode;             Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                         (= EXTRACTION) crea oggetti grafici (default = PREVIEW)
            pt4---pt3---pt_right
             | D/  | C/
             | / A | /
            pt1---pt2
             | B/
             | /
            pt_Bott



  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_DisplayValleyOrRidge(C_CGRID *pClass, double *Vector,
                             long KeyMin, long KeyMax, C_FAS &FAS, 
                             long *i, bool Valley = TRUE, int Mode = PREVIEW)
{
   long         Key = KeyMin;
   ads_point    pt1, pt2, pt3, pt4, pt_bott, pt_right;
   C_GRID       *pGrid = pClass->ptr_grid();
   double       LimitX = pGrid->getXLimit();
   double       LimitY = pGrid->getYLimit();
   double       Perc_1_2, Perc_3_2, Perc_3_1, dummy1, dummy2, PercToCompare;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1082)); // "Visualizzazione della griglia"
   StatusLineMsg.Init(gsc_msg(408), MEDIUM_STEP); // ogni 100 "%ld celle elaborate."

   while (Key <= KeyMax)
   {
      // se il valore è il minimo per un double significa valore nullo
      if (Vector[Key] == (std::numeric_limits<double>::min)())
         { Key++; continue; }

      // Mi ricavo le coordinate del punto in basso a sinistra della cella
      if (pGrid->key2pt(Key, pt1, FALSE) == GS_BAD) return GS_BAD;
      pt1[Z] = Vector[Key];

      // non ci sono celle a destra
      if ((pt2[X] = pt1[X] + pGrid->dx) >= LimitX) break;
      // se il valore è il minimo per un double significa valore nullo
      if ((pt2[Z] = Vector[Key + 1]) == (std::numeric_limits<double>::min)())
         { Key++; continue; }
      pt2[Y] = pt1[Y];

      // non ci sono celle sopra
      if ((pt3[Y] = pt1[Y] + pGrid->dy) >= LimitY) break;
      // se il valore è il minimo per un double significa valore nullo
      if ((pt3[Z] = Vector[Key + pGrid->nx + 1]) == (std::numeric_limits<double>::min)())
         { Key++; continue; }
      pt3[X] = pt2[X];

      // esiste il triangolo A e ne ricavo le percentuali della sessione di raccolta per lato
      gsc_getPerc3DSides(pt1, pt2, pt3, &Perc_1_2, &Perc_3_2, &Perc_3_1);

      // se cerco le valli con il triangolo B deve uscire acqua 
      // dal lato 1_2 del triangolo A
      // oppure se cerco i crinali con il triangolo B NON deve uscire acqua
      // dal lato 1_2 del triangolo A
      if ((Valley && Perc_1_2 > 0) || (!Valley && Perc_1_2 == 0))
         // se ci sono celle sotto
         if ((pt_bott[Y] = pt1[Y] - pGrid->dy) >= pGrid->y &&
            // se il valore è il minimo per un double significa valore nullo
            (pt_bott[Z] = Vector[Key - pGrid->nx]) != (std::numeric_limits<double>::min)())
         { 
            // esiste il triangolo B e ne ricavo le percentuali
            // della sessione di raccolta per lato
            pt_bott[X] = pt1[X];
            gsc_getPerc3DSides(pt2, pt1, pt_bott, &PercToCompare, &dummy1, &dummy2);
           
            // se cerco le valli con il triangolo A deve uscire acqua 
            // dal lato 2_1 del triangolo B
            // oppure se cerco i crinali con il triangolo A NON deve uscire acqua
            // dal lato 2_1 del triangolo B
            if ((Valley && PercToCompare > 0) || (!Valley && PercToCompare == 0))
               // Disegno il tratto
               gsc_insert_pline(pt1, pt2,  FAS.layer, &FAS.color, FAS.line,
                                FAS.line_scale, FAS.width);
         }

      // se cerco le valli con il triangolo C deve uscire acqua 
      // dal lato 3_2 del triangolo A
      // oppure se cerco i crinali con il triangolo C NON deve uscire acqua
      // dal lato 3_2 del triangolo A
      if ((Valley && Perc_3_2 > 0) || (!Valley && Perc_3_2 == 0))
         // se ci sono celle a destra
         if ((pt_right[X] = pt3[X] + pGrid->dx) < LimitX &&
            // se il valore è il minimo per un double significa valore nullo
             (pt_right[Z] = Vector[Key + 1 + pGrid->nx + 1]) != (std::numeric_limits<double>::min)())
         { 
            // esiste il triangolo C e ne ricavo le percentuali
            // della sessione di raccolta per lato
            pt_right[Y] = pt3[Y];
            gsc_getPerc3DSides(pt_right, pt3, pt2, &dummy1, &PercToCompare, &dummy2);

            // se cerco le valli con il triangolo A deve uscire acqua 
            // dal lato 3_2 del triangolo C
            // oppure se cerco i crinali con il triangolo A NON deve uscire acqua
            // dal lato 3_2 del triangolo C
            if ((Valley && PercToCompare > 0) || (!Valley && PercToCompare == 0))
               // Disegno il tratto
               gsc_insert_pline(pt3, pt2,  FAS.layer, &FAS.color, FAS.line,
                                FAS.line_scale, FAS.width);
         }

      // se cerco le valli con il triangolo D deve uscire acqua 
      // dal lato 3_1 del triangolo A
      // oppure se cerco i crinali con il triangolo D NON deve uscire acqua
      // dal lato 3_1 del triangolo A
      if ((Valley && Perc_3_1 > 0) || (!Valley && Perc_3_1 == 0))
         // se il valore è il minimo per un double significa valore nullo
         if ((pt4[Z] = Vector[Key + pGrid->nx]) != (std::numeric_limits<double>::min)())
         {
            // esiste il triangolo D e ne ricavo le percentuali
            // della sessione di raccolta per lato
            pt4[X] = pt1[X];
            pt4[Y] = pt3[Y];
            gsc_getPerc3DSides(pt3, pt4, pt1, &dummy1, &dummy2, &PercToCompare);

            // se cerco le valli con il triangolo A deve uscire acqua 
            // dal lato 3_1 del triangolo D
            // oppure se cerco i crinali con il triangolo A NON deve uscire acqua
            // dal lato 3_1 del triangolo D
            if ((Valley && PercToCompare > 0) || (!Valley && PercToCompare == 0))
               // Disegno il tratto
               gsc_insert_pline(pt3, pt1,  FAS.layer, &FAS.color, FAS.line,
                                FAS.line_scale, FAS.width);
         }

      StatusLineMsg.Set(++(*i)); // "%ld celle elaborate."

      Key++;
   }
   StatusLineMsg.End(gsc_msg(408), *i); // "%ld celle elaborate."

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_DisplayValleyOrRidge                 <external> */
/*+
   Funzione LISP per visualizzare le valli (i fiumi) o i crinali
   della griglia.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   (<prj><cls><sub><AttribZ><fas>[<pt1><pt2>[Valley[<Display mode>]]])

   <fas> = (<color>nil<line><style><layer>nil<width><elevation><scale>)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_DisplayValleyOrRidge(void)
{
   presbuf    arg = acedGetArgs();
   ads_point  pt1, pt2;
   C_STRING   AttribZ;
   C_FAS      FAS;
   int        prj, cls, sub, DisplayMode = PREVIEW;
   C_CGRID    *pCls;
   bool       pt1Exist = FALSE, Location = FALSE, Valley = TRUE;
   
   acedRetNil();
   
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // nome dell'attributo contenente il valore Z dei punti
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribZ = arg->resval.rstring;

   // FAS
   if (!(arg = arg->rbnext) || FAS.from_rb(arg) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // avanzo all'elemento successivo
   if ((arg = gsc_scorri(arg)) == NULL) 
	   { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((arg = arg->rbnext))
   {
      // punto inferiore della finestra
      if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
      if ((arg = arg->rbnext))
      {
         if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
         if ((arg = arg->rbnext))
         {  // Cerca valli o crinali
            if (arg->restype == RTNIL) Valley = FALSE;

            if ((arg = arg->rbnext))
               // Modalità di visualizzazione
               gsc_rb2Int(arg, &DisplayMode);
         }
      }
   }

   if (pCls->DisplayValleyOrRidge(AttribZ, FAS,
                                  (Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                                  Valley, DisplayMode) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}

/*********************************************************/
/*.doc C_CGRID::DisplayValleyOrRidge          <external> */
/*+
   Funzione per visualizzare i crinali (spartiacque) o le valli (fiumi)
   delle montagne tramite polilinee.
   N.B.: Gli oggetti grafici generati non saranno oggetti di GEOsim.
   Parametri:
   C_STRING &AttribZ;    Nome dell'attributo da utilizzare per leggere la
                         coordinata Z.
   C_FAS &FAS;           Caratteristiche grafiche della visualizzazione
   ads_point pt1;        Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                         in cui visualizzare le valli o i crinali, Rappresenta il punto 
                         a sinistra in basso (default = NULL)
   ads_point pt2;        Rappresenta il punto a destra in alto (default = NULL)
   bool Valley;          Flag, se = TRUE il programma valuta le valli altrimenti i
                         crinali dei monti
   int Mode;             Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                         (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::DisplayValleyOrRidge(C_STRING &AttribZ, C_FAS &FAS,
                                  ads_point pt1, ads_point pt2,
                                  bool Valley, int Mode)
{
   C_RECT              Rect;
   ads_point           pt, _pt2;
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           ColValues;
   long                i = 0, KeyMin, KeyMax;
   double              *Vector;
   long                VectorLen;

   // Se non esiste il layer lo creo
   if (gsc_crea_layer(FAS.layer) == GS_BAD) return GS_BAD;
  
   if (LoadToMemory(AttribZ, &Vector, &VectorLen) == GS_BAD) return GS_BAD;

   // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
   grid.getExtension(Rect, true);

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         { free(Vector); return GS_GOOD; }
   }

   // Mi ricavo le coordinate del punto (in basso a sinistra)
   // della prima cella da visualizzare impostando id inferiore
   if (grid.pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       grid.key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      { free(Vector); return GS_BAD; }
   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (grid.pt2key(_pt2, &KeyMax) == GS_BAD) { free(Vector); return GS_BAD; }

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   acutPrintf(GS_LFSTR);
   gsc_startTransaction();

   try
   {
      // Ciclo sulla Y
      do
      {
         if (gsc_DisplayValleyOrRidge(this, Vector, KeyMin, KeyMax, FAS,
                                      &i, Valley, Mode) == GS_BAD)
            AfxThrowUserException();

         // imposto id inferiore e superiore
         KeyMin += grid.nx;
         KeyMax += grid.nx;
         pt[Y] += grid.dy;
      }
      while (pt[Y] <= Rect.Top());
   } // fine try

   catch (...) // any type of exception
   {
      acutPrintf(GS_LFSTR);
      acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
      acutPrintf(GS_LFSTR);
      gsc_abortTransaction();

      return GS_BAD;
   }

   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(408), i); // "%ld celle elaborate."
   acutPrintf(GS_LFSTR);
   gsc_endTransaction();

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   double tempo2 = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FINe FUNZIONI PER RICERCA FIUMI E CRINALI-MONTI
// INIZIO FUNZIONI PER RICERCA BACINI
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_cella_scolante                     <internal> */
/*+
  Funzione che verifica che la cella della matrice non possa scolare
  acqua sulle celle adiacenti calcolando la quota minima tra le quote
  delle celle adiacenti più alte della cella corrente.
  C_DBL_MATRIX &Matrix;    Matrice contenente le quote delle celle
  long Col;                Colonna attuale
  long Row;                Riga attuale
  double *Zmin;            Quota minima;
                           1) Se le celle adiacenti sono con la stessa
                           quota allora Zmin = (std::numeric_limits<double>::max)())
                           2) Se la quota della cella non è valida
                           allora Zmin = (std::numeric_limits<double>::min)())

  Restituisce true se la cella scola altrimenti false. 
-*/  
/*************************************************************/
bool gsc_cella_scolante(C_DBL_MATRIX &Matrix, long Col, long Row, double *Zmin)
{
   double Z, ZVal;

   // leggo quota cella corrente
   if ((ZVal = Matrix.get(Col, Row)) == (std::numeric_limits<double>::min)())
   {
      // Se quota non valida
      *Zmin = (std::numeric_limits<double>::min)();
      return false;
   }

   *Zmin = (std::numeric_limits<double>::max)();

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col - 1, Row - 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col - 1, Row)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col - 1, Row + 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col, Row - 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col, Row + 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col + 1, Row - 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col + 1, Row)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;
   
   // Se si tratta di una quota valida
   if ((Z = Matrix.get(Col + 1, Row + 1)) != (std::numeric_limits<double>::min)())
      if (Z < ZVal) return true;
      else if (*Zmin > Z && Z > ZVal) *Zmin = Z;

   return false;
}


/*********************************************************/
/*.doc gsc_CercaZMinCelleAdiacentiNonScolanti <internal> */
/*+
  Funzione che, partendo da una cella, cerca le celle intorno ad essa con la stessa
  quota che non abbiano la possibilità di scolare acqua.
  C_DBL_MATRIX &Matrix;     Matrice contenente le quote delle celle
  long Col;                 Colonna attuale
  long Row;                 Riga attuale
  C_LONG_LIST &OutMatrix;   Lista contenente le celle non scolanti individuate
  C_BOOL_MATRIX &Visited;   Matrice di uso interno contenente le celle già
                            visitate

  Restituisce la quota minima tra le celle scolanti adiacenti se trovate
  altrimenti restituisce il minimo per un double. 
  N.B. La funzione non visita le celle nei bordi della matrice perchè
  prima di chiamare questa funzione si deve inizializzare la matrice 
  <Visited> con i bordi = true.
-*/  
/*************************************************************/
double gsc_CercaZMinCelleAdiacentiNonScolanti(C_DBL_MATRIX &Matrix, 
                                              long Col, long Row,
                                              C_LONG_LIST &OutMatrix,
                                              C_BOOL_MATRIX &Visited)
{
   double ZVal, Zmin, ZminAdj;

   // Per problemi di stack overflow fermo la ricorsività quando 
   // OutMatrix contiene 1000 elementi
   if (OutMatrix.get_count() == 1000) return (std::numeric_limits<double>::min)();

   // Se la cella era già stata visitata
   if (Visited.get(Col, Row)) return (std::numeric_limits<double>::min)();
   // La segno come visitata
   Visited.set(Col, Row, true);

   // leggo quota cella corrente (che non deve essere nulla)
   if ((ZVal = Matrix.get(Col, Row)) == (std::numeric_limits<double>::min)())
      return (std::numeric_limits<double>::min)();

   // Verifico se la cella scola calcolando la quota minima tra le celle adiacenti
   if (gsc_cella_scolante(Matrix, Col, Row, &Zmin))
      return (std::numeric_limits<double>::min)();

   // La segno come cella non scolante
   OutMatrix.add_tail_long(Row * Matrix.get_nCols() + Col);

   // Visito le celle adiacenti aventi la stessa quota
   if (Matrix.get(Col - 1, Row - 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col - 1, Row - 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col - 1, Row) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col - 1, Row,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col - 1, Row + 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col - 1, Row + 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col, Row - 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col, Row - 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col, Row + 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col, Row + 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col + 1, Row - 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col + 1, Row - 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col + 1, Row) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col + 1, Row,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   if (Matrix.get(Col + 1, Row + 1) == ZVal)
      if ((ZminAdj = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, Col + 1, Row + 1,
                                                            OutMatrix, Visited)) != (std::numeric_limits<double>::min)())
         if (Zmin > ZminAdj) Zmin = ZminAdj;

   return Zmin;
}


/*********************************************************/
/*.doc gsc_MatrixCorrectionForCelleNonScolanti <internal> */
/*+
  Funzione che corregge una matrice per evitare che esistano celle
  non scolanti. I bordi non possono essere corretti.
  C_DBL_MATRIX &Matrix;     Matrice contenente le quote delle celle

  Restituisce la quota minima tra le celle scolanti adiacenti se trovate
  altrimenti restituisce il minimo per un double. 
  N.B. La funzione non visita le celle nei bordi della matrice perchè
  prima di chiamare questa funzione si deve inizializzare la matrice 
  <Visited> con i bordi = true.
-*/  
/*************************************************************/
int gsc_MatrixCorrectionForCelleNonScolanti(C_DBL_MATRIX &Matrix)
{
   long          Col, Row, Step = 1, Qty;
   int           i, Tot;
   C_LONG_LIST   OutMatrix;
   C_LONG        *pCell;
   C_BOOL_MATRIX Visited(Matrix.get_nCols(), Matrix.get_nRows());
   double        Zmin, Zval;
   bool          Stop;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(456)); // "Modellazione per scolamento acquee"

   StatusLineMsg.Init();
   Tot = (Matrix.get_nCols() - 2) * (Matrix.get_nRows() - 2);

   do
   {
      StatusLineMsg.Set(gsc_msg(457), Step++); // "Passo %ld..."
      C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(NULL);
      StatusBarProgressMeter.Init(Tot);

      Stop      = true;
      i         = 0;
      Qty       = 0;

      for (Row = 1; Row < Matrix.get_nRows() - 1; Row++)
      {
         // se l'utente vuole bloccare l'estrazione
         if (acedUsrBrk()) return GS_CAN;

         for (Col = 1; Col < Matrix.get_nCols() - 1; Col++)
         {
            StatusBarProgressMeter.Set(++i);

            // Verifico se la cella scola calcolando la quota minima tra le celle adiacenti
            // in questo caso non ci sono correzioni da fare
            // Se Zmin = min di un double significa che aveva quota non valida
            if (gsc_cella_scolante(Matrix, Col, Row, &Zmin) ||
                Zmin == (std::numeric_limits<double>::min)()) continue;

            // inizializzo la matrice con i bordi a true e l'interno a false
            // perchè non voglio intervenire sui bordi
            Visited.init(false);
            Visited.setColumn(0, true);
            Visited.setColumn(Matrix.get_nCols() - 1, true);
            Visited.setRow(0, true);
            Visited.setRow(Matrix.get_nRows() - 1, true);

// acutPrintf("\nsegno %d", i);

            // Cerco la quota minima intorno alle celle non scolanti
            Zmin = gsc_CercaZMinCelleAdiacentiNonScolanti(Matrix, 
                                                          Col, Row,
                                                          OutMatrix,
                                                          Visited);

            if (OutMatrix.get_count() > 0)
            {
               Qty += OutMatrix.get_count();
               Stop = false;

               // Se la quota minima è il massimo per un double significa
               // che le celle non scolanti non avevano intorni celle più alte
               if (Zmin == (std::numeric_limits<double>::max)())
               {
                  Zval = Matrix.get(Col, Row);
                  // alzo la quota minima di 1/1000 della quota della cella
                  Zmin = Zval + Zval / 1000;
               }

               // alzo le celle non scolanti
               pCell = (C_LONG *) OutMatrix.get_head();
               while (pCell)
               {
                  Matrix.set1DArray(pCell->get_id(), Zmin);
                  pCell = (C_LONG *) OutMatrix.get_next();
               }

               // svuoto la lista delle celle non scolanti                         
               OutMatrix.remove_all();
            }
         }
      }
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      if (Qty) acutPrintf(gsc_msg(458), Qty); // "\nCorrette %ld celle...\n"
   }
   while (!Stop); // il ciclo continua finchè ci sono correzioni

	acutPrintf(gsc_msg(111)); // "\nTerminato.\n"

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetValleyMatrix                    <internal> */
/*+
  Funzione che setta una matrice in cui si memorizza per ciascuna cella 
  la cella in cui l'acqua andrà a scolare. Per ciascuna cella si sceglie
  la cella adiacente più bassa.
  C_DBL_MATRIX &Matrix;     Matrice contenente le quote delle celle
  C_DBL_MATRIX &OutMatrix;  Matrice di uscita

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*************************************************************/
int gsc_SetValleyMatrix(C_DBL_MATRIX &Matrix, C_DBL_MATRIX &OutMatrix)
{
   long   Col, Row, Refused = 0, ColCell, RowCell, Tot, i = 0;
   double Zmin, Zval;

   C_STRING Title(gsc_msg(459)); // "Determinazione flussi di scolo"
   acutPrintf(_T("\n%s\n"), Title.get_name());

   if (OutMatrix.alloc(Matrix.get_nCols(), Matrix.get_nRows()) == GS_BAD)
      return GS_BAD;
   OutMatrix.init((std::numeric_limits<double>::min)());
     
   Tot = Matrix.get_nCols() * Matrix.get_nRows();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(Tot);

   for (Row = 0; Row < Matrix.get_nRows(); Row++)
      for (Col = 0; Col < Matrix.get_nCols(); Col++)
      {
         StatusBarProgressMeter.Set(++i);

         Zmin = (std::numeric_limits<double>::max)();

         // Verifico le celle adiacenti

         if ((Col > 0) && (Row > 0) &&
             (Zval = Matrix.get(Col - 1, Row - 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col - 1;
            RowCell = Row - 1;
            Zmin    = Zval;
         }

         if ((Col > 0) &&
             (Zval = Matrix.get(Col - 1, Row)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col - 1;
            RowCell = Row;
            Zmin    = Zval;
         }

         if ((Col > 0) && (Row < Matrix.get_nRows() - 1) &&
             (Zval = Matrix.get(Col - 1, Row + 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col - 1;
            RowCell = Row + 1;
            Zmin    = Zval;
         }

         if ((Row > 0) &&
             (Zval = Matrix.get(Col, Row - 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col;
            RowCell = Row - 1;
            Zmin    = Zval;
         }

         if ((Row < Matrix.get_nRows() - 1) &&
             (Zval = Matrix.get(Col, Row + 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col;
            RowCell = Row + 1;
            Zmin    = Zval;
         }

         if ((Col < Matrix.get_nCols() - 1) && (Row > 0) &&
             (Zval = Matrix.get(Col + 1, Row - 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col + 1;
            RowCell = Row - 1;
            Zmin    = Zval;
         }

         if ((Col < Matrix.get_nCols() - 1) && 
             (Zval = Matrix.get(Col + 1, Row)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col + 1;
            RowCell = Row;
            Zmin    = Zval;
         }

         if ((Col < Matrix.get_nCols() - 1) && (Row < Matrix.get_nRows() - 1) &&
             (Zval = Matrix.get(Col + 1, Row + 1)) != (std::numeric_limits<double>::min)() &&
             Zval < Zmin)
         {
            ColCell = Col + 1;
            RowCell = Row + 1;
            Zmin    = Zval;
         }

         if (Zmin >= Matrix.get(Col, Row))
            Refused++; // cella non scolante
         else OutMatrix.set(Col, Row, RowCell * Matrix.get_nCols() + ColCell);
      }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::SetHydrologyFlow              <external> */
/*+
  Funzione che setta per ciascuna cella della regione indicata
  la cella a valle (in cui scarica acqua).
  C_STRING &AttribZ;       Nome dell'attributo da utilizzare per leggere la
                           coordinata Z.
  ads_point pt1;           Punto in basso a sinistra
  ads_point pt2;           Punto in alto a destra
  C_STRING &DownstreamCellAttrib;  Nome dell'attributo da utilizzare per memorizzare
                                   il codice della cella a valle.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::SetHydrologyFlow(C_STRING &AttribZ, ads_point pt1, ads_point pt2,
                              C_STRING &DownstreamCellAttrib)
{
   C_GRID       *pInfoGrid = ptr_grid();
   C_RECT       Rect;
   C_DBL_MATRIX Matrix, FlowMatrix;
   int          res;
   long         LeftBottomKey, i, Tot;

   if (pt1 && pt2 && !gsc_2Dpoint_equal(pt1, pt2)) // Solo nella zona
      Rect.Set(pt1, pt2);
   else
      // Estensioni della griglia sottraendo un piccolo offset all'angolo alto-destro
      pInfoGrid->getExtension(Rect, true);

   // Carico le quote delle celle in memoria
   if (LoadToMatrix(AttribZ, Rect.BottomLeft.point, Rect.TopRight.point, Matrix, &LeftBottomKey) == GS_BAD)
      return GS_BAD;

   // Controllo che tutte le celle abbiano la quota inizializzata
   Tot = Matrix.get_nCols() * Matrix.get_nRows();
   for (i = 0; i < Tot; i++)
      if (Matrix.get1DArray(i) == (std::numeric_limits<double>::min)())
         { GS_ERR_COD = eGSCannotLoadValues; return GS_BAD; }

   // Correggo la matrice perchè tutte le celle possano scolare
   if ((res = gsc_MatrixCorrectionForCelleNonScolanti(Matrix)) != GS_GOOD)
      return res;
   // Determino una matrice con le direzioni dei flussi
   if (gsc_SetValleyMatrix(Matrix, FlowMatrix) == GS_BAD) return GS_BAD;

   // Nelle celle di FlowMatrix sostituisco i valori presenti (posizioni nel vettore)
   // con i codici delle celle (gs_id)
   gsc_MatrixVettPos2Key(FlowMatrix, pInfoGrid, LeftBottomKey);

   // Aggiorno la griglia
   if (UpdFromMatrix(Rect.BottomLeft.point, Rect.TopRight.point, FlowMatrix, DownstreamCellAttrib) == GS_BAD)
      return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetCountUpstreamCellsMatrix        <internal> */
/*+
  Funzione che setta una matrice in cui si memorizza per ciascuna cella 
  il conteggio delle celle a monte.
  C_DBL_MATRIX &Matrix;     Matrice contenente le direzioni dei flussi
  C_DBL_MATRIX &OutMatrix;  Matrice di uscita

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*************************************************************/
int gsc_SetCountUpstreamCellsMatrix(C_DBL_MATRIX &Matrix, C_DBL_MATRIX &OutMatrix)
{
   bool         Stop;
   long         Tot, i, Step = 1;
   double       OldVal, ReceiveVal;
   C_DBL_MATRIX Received; // matrice che memorizza la quantità ricevuta 
                          // ad ogni passaggio
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(461)); // "Conteggio celle a monte"

   if (OutMatrix.alloc(Matrix.get_nCols(), Matrix.get_nRows()) == GS_BAD)
      return GS_BAD;
   OutMatrix.init(0);

   // Inizializzo la matrice che memorizza quanto riceve ciascuna cella = 1
   if (Received.alloc(Matrix.get_nCols(), Matrix.get_nRows()) == GS_BAD)
      return GS_BAD;
   Received.init(1);

   Tot = Matrix.get_nCols() * Matrix.get_nRows();
   StatusLineMsg.Init();

   do
   {
      Stop = true;
      StatusLineMsg.Set(gsc_msg(457), Step++); // "Passo %ld..."

      for (i = 0; i < Tot; i++)
      {
         ReceiveVal = Received.get1DArray(i);
         // Se la cella ha qualcosa da dare a qualcuno nella matrice
         if (ReceiveVal > 0 && 
             Matrix.get1DArray(i) != (std::numeric_limits<double>::min)())
         {
            Stop = false;
            
            // versa il contenuto alla cella Matrix[i] che conta le celle a monte
            OldVal = OutMatrix.get1DArray((long) Matrix.get1DArray(i));
            OutMatrix.set1DArray((long) Matrix.get1DArray(i), OldVal + ReceiveVal);
            Received.set1DArray(i, 0);

            // aggiorna il contenuto della cella Received
            OldVal = Received.get1DArray((long) Matrix.get1DArray(i));
            Received.set1DArray((long) Matrix.get1DArray(i), OldVal + ReceiveVal);
         }
      }
   }
   while (Stop == false);

	acutPrintf(gsc_msg(111)); // "\nTerminato.\n"

   return GS_GOOD;
}


/**********************************************************/
/*.doc gsc_MatrixKey2VettPos                   <internal> */
/*+
  Funzione che trasforma i codici (gs_id) contenuti nelle celle di
  una matrice in posizioni all'interno del vettore della matrice.
  C_GRID *pInfoGrid;    Struttura che descrive le caratteristiche della 
                        matrice completa della classe
  long LeftBottomKey;   Codice della cella in basso a sinstra della
                        matrice caricata da una zona scelta dall'utente
  C_DBL_MATRIX &Matrix; Matrice da trasformare
-*/  
/*************************************************************/
void gsc_MatrixKey2VettPos(C_GRID *pInfoGrid, long LeftBottomKey, C_DBL_MATRIX &Matrix)
{
   long AbsCol, AbsRow, Key, Col, Row;

   // Ricavo coordinate nella matrice originale della classe
   long RelCol = pInfoGrid->getColumn(LeftBottomKey);
   long RelRow = pInfoGrid->getRow(LeftBottomKey);

   // Determino le posizioni delle celle all'interno della matrice
   // partendo dai codici (gs_id) delle stesse
   long Tot = Matrix.get_nCols() * Matrix.get_nRows();
   for (long i = 0; i < Tot; i++)
      if ((Key = (long) Matrix.get1DArray(i)) != 0)
      {
         AbsCol = pInfoGrid->getColumn(Key);
         AbsRow = pInfoGrid->getRow(Key);

         Col = AbsCol - RelCol;
         Row = AbsRow - RelRow;

         if (Col >= 0 && Col < Matrix.get_nCols() &&
             Row >= 0 && Row < Matrix.get_nRows())
            Matrix.set1DArray(i, Row * Matrix.get_nCols() + Col);
         else
            Matrix.set1DArray(i, (std::numeric_limits<double>::min)());
      }
      else
         Matrix.set1DArray(i, (std::numeric_limits<double>::min)());
}


/**********************************************************/
/*.doc gsc_MatrixVettPos2Key                   <internal> */
/*+
  Funzione che trasforma le posizioni all'interno del vettore della matrice
  in codici (gs_id) delle celle.
  C_GRID *pInfoGrid;    Struttura che descrive le caratteristiche della 
                        matrice completa della classe
  long LeftBottomKey;   Codice della cella in basso a sinstra della
                        matrice caricata da una zona scelta dall'utente
  C_DBL_MATRIX &Matrix; Matrice da trasformare
-*/  
/*************************************************************/
void gsc_MatrixVettPos2Key(C_DBL_MATRIX &Matrix, C_GRID *pInfoGrid, long LeftBottomKey)
{
   long   Col, Row, Key;
   double val;

   // Nelle celle di FlowMatrix sostituisco i valori presenti (posizioni nel vettore)
   // con i codici delle celle (gs_id)
   for (long x = 0; x < Matrix.get_nCols(); x++)
      for (long y = 0; y < Matrix.get_nRows(); y++)
         if ((val = Matrix.get(x, y)) != (std::numeric_limits<double>::min)())
         {
            Matrix.getColRowFrom1DArray((long) val, &Col, &Row);
            Key = LeftBottomKey + (Row * pInfoGrid->nx) + Col;
            Matrix.set(x, y, Key);
         }
}

/**********************************************************/
/*.doc C_CGRID::SetHydrologyCountUpstreamCells <external> */
/*+
  Funzione che setta per ciascuna cella della regione indicata
  il conteggio delle celle a monte (da cui proviene acqua).
  C_STRING &DownstreamCellAttrib;      Nome dell'attributo che memorizza
                                       il codice della cella a valle.
  ads_point pt1;                       Se <> NULL insieme a pt2 definisce la zona 
                                       delle celle da calcolare. Rappresenta il punto 
                                       a sinistra in basso
  ads_point pt2;                       Punto in alto a destra
  C_STRING &CountUpstreamCellsAttrib;  Nome dell'attributo da utilizzare per memorizzare
                                       il conteggio delle celle a monte.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::SetHydrologyCountUpstreamCells(C_STRING &DownstreamCellAttrib, 
                                            ads_point pt1, ads_point pt2,
                                            C_STRING &CountUpstreamCellsAttrib)
{
   C_RECT       Rect;
   ads_point    _pt1, _pt2;
   C_DBL_MATRIX Matrix, CountUpstreamCellsMatrix;
   long         LeftBottomKey;

   if (pt1 && pt2)
   {
      Rect.Set(pt1, pt2);
      // Mi assicuro che _pt1 sia il punto in basso-destra e _pt2 in alto-sinistra
      if (pt1[X] < pt2[X])
         { _pt1[X] = pt1[X]; _pt2[X] = pt2[X]; }
      else
         { _pt1[X] = pt2[X]; _pt2[X] = pt1[X]; }

      if (pt1[Y] < pt2[Y])
         { _pt1[Y] = pt1[Y]; _pt2[Y] = pt2[Y]; }
      else
         { _pt1[Y] = pt2[Y]; _pt2[Y] = pt1[Y]; }
   }
   else    // Estensioni dela griglia
   {
      _pt1[X] = grid.x; _pt1[Y] = grid.y;
      _pt2[X] = grid.x + (grid.dx * grid.nx) - (grid.dx / 10);
      _pt2[Y] = grid.y + (grid.dy * grid.ny) - (grid.dy / 10);
   }

   // Carico le quote delle celle in memoria
   if (LoadToMatrix(DownstreamCellAttrib, _pt1, _pt2, Matrix, &LeftBottomKey) == GS_BAD)
      return GS_BAD;

   // Nelle celle di Matrix sostituisco i valori presenti (gs_id)
   // con le posizioni delle celle nel vettore
   gsc_MatrixKey2VettPos(ptr_grid(), LeftBottomKey, Matrix);

   // Determino una matrice con i conteggi delle celle a monte di ciascuna cella
   if (gsc_SetCountUpstreamCellsMatrix(Matrix, CountUpstreamCellsMatrix) == GS_BAD)
      return GS_BAD;

   // Aggiorno la griglia
   if (UpdFromMatrix(_pt1, _pt2, CountUpstreamCellsMatrix, CountUpstreamCellsAttrib) == GS_BAD)
      return GS_BAD;
   
   return GS_GOOD;
}


/**********************************************************/
/*.doc gsc_GetCatchmentAreaCells               <internal> */
/*+
  Funzione che ricava le celle del bacino a monte di una cella
  nota.
  C_DBL_MATRIX &Matrix; Matrice con le direzioni dei flussi
  long Col;             Colonna da cui partire per la ricerca
  long Row;             Riga da cui partire per la ricerca
  C_LINK_SET &ResultLS; LinkSet risultato della ricerca

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
void gsc_GetCatchmentAreaCells(C_DBL_MATRIX &Matrix, long Col, long Row, 
                               C_LINK_SET &ResultLS)
{
   long   Val = Col + (Matrix.get_nCols() * Row);
   double FlowVal;

   // Aggiungo posizione nel vettore
   ResultLS.ptr_KeyList()->add(&Val);

   // Verifico le celle adiacenti
   if ((Col > 0) && (Row > 0) &&
       (FlowVal = Matrix.get(Col - 1, Row - 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col - 1, Row - 1, ResultLS);

   if ((Col > 0) &&
       (FlowVal = Matrix.get(Col - 1, Row)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col - 1, Row, ResultLS);

   if ((Col > 0) && (Row < Matrix.get_nRows() - 1) &&
       (FlowVal = Matrix.get(Col - 1, Row + 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col - 1, Row + 1, ResultLS);

   if ((Row > 0) &&
      (FlowVal = Matrix.get(Col, Row - 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col, Row - 1, ResultLS);

   if ((Row < Matrix.get_nRows() - 1) &&
       (FlowVal = Matrix.get(Col, Row + 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col, Row + 1, ResultLS);

   if ((Col < Matrix.get_nCols() - 1) && (Row > 0) &&
       (FlowVal = Matrix.get(Col + 1, Row - 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col + 1, Row - 1, ResultLS);

   if ((Col < Matrix.get_nCols() - 1) && 
       (FlowVal = Matrix.get(Col + 1, Row)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col + 1, Row, ResultLS);

   if ((Col < Matrix.get_nCols() - 1) && (Row < Matrix.get_nRows() - 1) &&
       (FlowVal = Matrix.get(Col + 1, Row + 1)) != (std::numeric_limits<double>::min)() &&
       FlowVal == Val)
      gsc_GetCatchmentAreaCells(Matrix, Col + 1, Row + 1, ResultLS);
}


/**********************************************************/
/*.doc C_CGRID::GetCatchmentAreaCells          <external> */
/*+
  Funzione che ricava le celle del bacino a monte di una cella
  nota.
  long     Key;                     Codice della cella da cui partire
  C_STRING &DownstreamCellAttrib;   Nome dell'attributo che memorizza
                                    il codice della cella a valle.
  C_LINK_SET &ResultLS;             LinkSet risultato della ricerca

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::GetCatchmentAreaCells(long Key, C_STRING &DownstreamCellAttrib, C_LINK_SET &ResultLS)
{
   C_GRID       *pInfoGrid = ptr_grid();
   C_DBL_MATRIX Matrix;
   ads_point    pt1, pt2;
   long         LeftBottomKey;

   ResultLS.clear();

   // Calcolo del estensioni della griglia
   pt1[X] = pInfoGrid->x;
   pt1[Y] = pInfoGrid->y;
   pt2[X] = pInfoGrid->getXLimit();
   pt2[Y] = pInfoGrid->getYLimit();

   // Carico le quote delle celle in memoria
   if (LoadToMatrix(DownstreamCellAttrib, pt1, pt2, Matrix, &LeftBottomKey) == GS_BAD) return GS_BAD;

   // Nelle celle di Matrix sostituisco i valori presenti (gs_id)
   // con le posizioni delle celle nel vettore
   gsc_MatrixKey2VettPos(ptr_grid(), LeftBottomKey, Matrix);

   // Ricavo la colonna e la riga della cella di partenza
   long Col = pInfoGrid->getColumn(Key);
   long Row = pInfoGrid->getRow(Key);
   
   // Determino le celle a monte di quella nota
   gsc_GetCatchmentAreaCells(Matrix, Col, Row, ResultLS);

   C_BLONG *pItem = (C_BLONG *) ResultLS.ptr_KeyList()->go_top();
   while (pItem)
   {
      Matrix.getColRowFrom1DArray(pItem->get_key(), &Col, &Row);
      Key = LeftBottomKey + (Row * pInfoGrid->nx) + Col;
      pItem->set_key(Key);

      pItem = (C_BLONG *) ResultLS.ptr_KeyList()->go_next();
   }

   ResultLS.cls = id.code;
   ResultLS.sub = id.sub_code;

   return GS_GOOD;
}


/**********************************************************/
/*.doc gsc_GetFlowDownCells                    <internal> */
/*+
  Funzione che ricava le celle che descrivono il flusso verso il basso
  di una cella nota.
  C_DBL_MATRIX &Matrix; Matrice con le direzioni dei flussi
  long Pos;             Posizione (nel vettore) della cella da cui partire per la ricerca
  C_LINK_SET &ResultLS; LinkSet risultato della ricerca

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
void gsc_GetFlowDownCells(C_DBL_MATRIX &Matrix, long Pos, C_LINK_SET &ResultLS)
{
   double FlowVal;

   // Aggiungo posizione nel vettore
   ResultLS.ptr_KeyList()->add(&Pos);

   if ((FlowVal = Matrix.get1DArray(Pos)) != (std::numeric_limits<double>::min)() &&
       ((long) FlowVal) != Pos) // una cella non può puntare su se stessa
      gsc_GetFlowDownCells(Matrix, (long) FlowVal, ResultLS);
}


/**********************************************************/
/*.doc C_CGRID::GetFlowDownCells               <external> */
/*+
  Funzione che ricava le celle che descrivono il flusso verso il basso
  di una cella nota.
  long     Key;                     Codice della cella da cui partire
  C_STRING &DownstreamCellAttrib;   Nome dell'attributo che memorizza
                                    il codice della cella a valle.
  C_LINK_SET &ResultLS;             LinkSet risultato della ricerca

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::GetFlowDownCells(long Key, C_STRING &DownstreamCellAttrib, C_LINK_SET &ResultLS)
{
   C_GRID       *pInfoGrid = ptr_grid();
   C_DBL_MATRIX Matrix;
   ads_point    pt1, pt2;
   long         LeftBottomKey, Col, Row;

   ResultLS.clear();

   // Calcolo del estensioni della griglia
   pt1[X] = pInfoGrid->x;
   pt1[Y] = pInfoGrid->y;
   pt2[X] = pInfoGrid->getXLimit();
   pt2[Y] = pInfoGrid->getYLimit();

   // Carico le quote delle celle in memoria
   if (LoadToMatrix(DownstreamCellAttrib, pt1, pt2, Matrix, &LeftBottomKey) == GS_BAD) return GS_BAD;

   // Nelle celle di Matrix sostituisco i valori presenti (gs_id)
   // con le posizioni delle celle nel vettore
   gsc_MatrixKey2VettPos(ptr_grid(), LeftBottomKey, Matrix);
   
   // Determino le celle a valle di quella nota
   gsc_GetFlowDownCells(Matrix, Key - 1, ResultLS);

   C_BLONG *pItem = (C_BLONG *) ResultLS.ptr_KeyList()->go_top();
   while (pItem)
   {
      Matrix.getColRowFrom1DArray(pItem->get_key(), &Col, &Row);
      Key = LeftBottomKey + (Row * pInfoGrid->nx) + Col;
      pItem->set_key(Key);

      pItem = (C_BLONG *) ResultLS.ptr_KeyList()->go_next();
   }

   ResultLS.cls = id.code;
   ResultLS.sub = id.sub_code;

   return GS_GOOD;
}


//////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PER RICERCA BACINI
//////////////////////////////////////////////////////////////////////////



/*********************************************************/
/*.doc gsc_GridEvid                          <external> */
/*+
  Questa funzione evidenzia le celle di una classe griglia.
  Parametri:
  C_CLASS *pCls;          puntatore alla classe madre degli oggetti da evidenziare
  C_LINK_SET &ResultLS;   Lista di codici delle celle da evidenziare
  bool ByPoints;          Flag, se = TRUE l'evidenziazione sarà tramite punti atrimenti
                          tramite celle rettangolari
  C_STRING &AttribZ;      Attributo contenente la quota (non utilizzato 
                          se la stringa è nulla)
  C_FAS *pFAS;            Cratteristiche grafice (default = NULL)
  int Mode;               Se = PREVIEW non crea oggetti ACAD (più veloce) altrimenti 
                          (= EXTRACTION) crea oggetti grafici (default = PREVIEW)

  Restituisce RTERROR in caso di errore altrimenti RTNORM.
-*/  
/*********************************************************/
int gsc_GridEvid(C_CGRID *pCls, C_LINK_SET &ResultLS,
                 bool ByPoints, C_STRING &AttribZ, C_FAS *pFAS, int Mode)
{
   if (ByPoints)
      return pCls->DisplayPts(ResultLS.ptr_KeyList(),
                              (AttribZ.get_name()) ? &AttribZ : NULL,
                              pFAS, TRUE, Mode);        // Centro
   else
      return pCls->DisplayCells(ResultLS.ptr_KeyList(),
                                (AttribZ.get_name()) ? &AttribZ : NULL,
                                pFAS, Mode);
   
   return GS_GOOD;
}