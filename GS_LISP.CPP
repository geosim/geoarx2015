/**********************************************************
Name: GS_LISP
                                   
Module description: Interprete LISP per GEOsim
            
Author: Roberto Poltini, Stefano Bellio

(c) Copyright 2001-2015 by IREN ACQUA GAS  S.p.A.

**********************************************************/

/*

AutoLISP limita le stringhe individuali a 132 chr
\" nelle stringhe

quante variabili si riescono a definire (solo 10??)

se una funzione ritorna nil in una gestione stringhe, cosa succede?

cambiare lettura per piu' di 32000 chr


Funzioni implementate:        da sviluppare o 
                                  testare

aritmetiche:

(+ [number number] ...)
(- [number number] ...)
(* [number number] ...)
(/ [number number] ...)
(1+ number)            
(1- number)            
(abs number)           
(atan num1 [num2])                  X
(cos angle)                         X
(exp number)                        X
(expt base power)
(fix number)                        X
(gcd int1 int2)                     X
(log number)                        X
(max number number ...)
(min number number ...)
(sin angle)                         X
(sqrt number)                       X

trattamento stringhe:         da sviluppare o 
                                  testare

(alltrim string)
(at string chars [start] [length])
(ltrim string)
(rtrim string)
(trim string)
(strcase string [which])
(strcat string1 [string2] ...)
(strlen [string] ...)
(subtran string waht with)
(substr string start [length])

uguaglianza e condizioni:     da sviluppare o 
                                  testare

(= numstr [numstr])
(/= numstr [numstr] ...)
(< numstr [numstr] ...)
(<= numstr [numstr] ...)
(> numstr [numstr] ...)
(>= numstr [numstr] ...)
(and expr ...)
(cond (test1 result1 ...) ...)
(eq expr1 expr2)                    X
(equal expr1 expr2 [fuzz])
(if testexpr thenexpr [elseexpr])
(or expr)
(repeat int expr ...)               X
(while testexpr expr ...)

gestione liste:               da sviluppare o 
                                  testare

(append list ...)                   X
(assoc item alist)                  X
(car list)                          X
(cdr list)                          X
(cons new-first-element list)       X
(foreach name list expr ...)        X
(last list)                         X
(length list)                       X
(list expr ...)                     X
(member expr list)                  X
(nth n list)                        X

gestione simboli:             da sviluppare o 
                                  testare

(not item)
(null item)
(set sym expr)                      X
(setq sym1 expr1 [sym2 expr2] ...)
(type item)                         X

gestione funzioni:            da sviluppare o 
                                  testare

(load file-name)
(defun sym argument-list expr ...)
(progn [expr] ...)

gestione errori:              da sviluppare o 
                                  testare

(exit)                              X

funzioni di conversione:      da sviluppare o 
                                  testare

(angtof string [mode])              X
(angtos angle [mode [precision]])   X
(ascii string)
(atof string)
(atoi string)
(chr integer)
(itoa int)
(rtos number [mode [precision]])

gestione file:                da sviluppare o 
                                  testare

(close file-desc)                   X
(findfile filename)                 X
(copyfile filename)                 X
(open filename mode)                X
(read-char [file-desc])             X
(read-line [file-desc])             X
(write-char num [file-desc])        X
(write-line string [file-desc])     X

varie:                        da sviluppare o 
                                  testare

(ver)
(gs_mem)
(whoami)       Login utente corrente
(getguid)      ritorna una stringa univoca

data e ora:

(date)         Data corrente
(time)         Ora corrente
(datetime)     Data e ora corrente
(year date)    Anno numerico esteso della data
(month date)   Mese della data
(day date)     Giorno della data

funzioni di calcolo grafico:

(changeblock blockname)
(changecolor numcolor)
(changecolorattr nomeattr numcolor)
(changedynblockprop propname propvalue [What])
(changeelev numelevation)
(changehatch hatchname ColorNumber Scale Rotation Unit LayerName)
(changehtext numhtext)
(changehtextattr nomeattr numhtext)
(changelayer layername [What])
(changelayerattr nomeattr layername)
(changelinetype linetypename)
(changerotation numrotation [unit])
(changescale numscale)
(changetextstyle textstylename)
(changetextstyleattr nomeattr textstylename)
(changethickness numthickness)
(changewidth numwidth)
(moveattr nomeattr modalità [offset1[offset2[Flag_generaz[allineam_oriz[allineam_vert
          [controllo_rotz[controllo_sovrapposiz]]]]]]])
(movesheet modalità [offset1[offset2[controllo_rotz[controllo_sovrapposiz]]]])
(movesheet X [Y [Z [rotz]]]])
(visattr nomeattr modalità)
(vissheet modalità)
(gs_curr_op)
(coord_conv pt src dest)

funzioni sql:

(lookup connectionstr sqlstm)
*/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>      // per isspace e isdigit
#include <math.h>       // per pow

#include "rxdefs.h"   
#include "adslib.h"   
#include "adsdlg.h"   
#include <adeads.h>

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"   
#include "gs_list.h"   
#include "gs_init.h" 
#include "gs_utily.h"   
#include "gs_resbf.h"   
#include "gs_class.h"   
#include "gs_graph.h"
#include "gs_lisp.h"
#include "gs_prjct.h"     // prototipi funzioni progetti
#include "gs_area.h"
#include "gs_query.h"
#include "gs_attbl.h"     // gestione blocchi attributi visibili


/**************************************************************************/
/*  DEFINES  */
/**************************************************************************/


#define MAX_LEN_STMNT  256       // max lunghezza contenuto variabile (in caratteri)
#define LEN_TMP_STR    150       // lunghezza vettore tmp_stream
                                 // usato solo per ads_getstring() basterebbe
                                 // lungo solo 133
#define SCREEN_LISP  TRUE        // Prevede messaggi di errore
#define LISP_PREC      9         // Numero di decimali nei calcoli GEOlisp


/**********************************************************************/
/*   GLOBAL VARIABLES */
/**********************************************************************/

C_NODE *GS_CALC_CLASS=NULL; // puntatore pubblico dedicato al calcolo dei campi calcolati
                            // vedi funzioni di calcolo di dati da grafica.

static size_t Curr_ptr  = 0;  // puntatore a Curr_stream
                                                                           
static C_VAR_LIST Private_var;
static C_STR_LIST Function_list;                                                                           


// Il seguente vettore contiene i nomi delle funzioni GEOLisp 
// per leggere i dati dalla grafica con i loro valori di default
GEOLispFunction VectorFromGraphGEOLispFunctions[] =
//   Nome funzione                    Valore di default
//                         tipo dato, valore in stringa, descrizione
{  
   {GS_LISP_LENGTH,           _T("real"),  _T("1"),   _T("")},    // Lunghezza totale
   {GS_LISP_AREA,             _T("real"),  _T("1.0"), _T("")},    // Area totale
   
   {GS_LISP_COLOR,            _T("real"),  _T("1.0"), _T("")},    // Colore

   {GS_LISP_BLOCKNAME,        _T("string"), _T("?"),  _T("")},    // Nome del blocco

   {GS_LISP_INS_POINT,        _T("point"), _T("0,0,0"), _T("")},  // Punto di inserimento
   {GS_LISP_X_INS_POINT,      _T("real"),  _T("0"),   _T("")},    // X punto di inserimento
   {GS_LISP_Y_INS_POINT,      _T("real"),  _T("0"),   _T("")},    // Y punto di inserimento
   {GS_LISP_Z_INS_POINT,      _T("real"),  _T("0"),   _T("")},    // Z punto di inserimento

   {GS_LISP_SPOINT,           _T("point"), _T("0,0,0"), _T("")},  // Punto iniziale
   {GS_LISP_X_SPOINT,         _T("real"),  _T("0"),   _T("")},    // X punto iniziale
   {GS_LISP_Y_SPOINT,         _T("real"),  _T("0"),   _T("")},    // Y punto iniziale
   {GS_LISP_Z_SPOINT,         _T("real"),  _T("0"),   _T("")},    // Z punto iniziale

   {GS_LISP_EPOINT,           _T("point"), _T("0,0,0"), _T("")},  // Punto finale
   {GS_LISP_X_EPOINT,         _T("real"),  _T("0"),   _T("")},    // X punto finale
   {GS_LISP_Y_EPOINT,         _T("real"),  _T("0"),   _T("")},    // Y punto finale
   {GS_LISP_Z_EPOINT,         _T("real"),  _T("0"),   _T("")},    // Z punto finale

   {GS_LISP_CENTROID,         _T("point"), _T("0,0,0"), _T("")},  // Punto centroide
   
   {GS_LISP_START_WIDTH,      _T("real"),  _T("0"),   _T("")},    // Larghezza iniziale
   {GS_LISP_END_WIDTH,        _T("real"),  _T("0"),   _T("")},    // Larghezza finale

   {GS_LISP_LAYER,            _T("string"),_T("0"),   _T("")},    // Nome layer

   {GS_LISP_HEIGHT,           _T("real"),  _T("1.0"), _T("")},       // Altezza testo
   {GS_LISP_STYLE,            _T("string"), _T("Standard"), _T("")}, // Nome dello stile testo

   {GS_LISP_ELEVATION,        _T("real"),  _T("0"),   _T("")},    // Elevazione

   {GS_LISP_THICKNESS,        _T("real"),  _T("0"),   _T("")},    // Spessore della linea
   {GS_LISP_LINETYPE,         _T("string"), _T("Continuous"), _T("")}, // Nome del tipo di linea

   {GS_LISP_ROTATION,         _T("real"),  _T("0"),   _T("")},    // Rotazione in radianti
   {GS_LISP_ROTATION_DEGREE,  _T("real"),  _T("0"),   _T("")},    // Rotazione in gradi

   {GS_LISP_WINDOW,           _T("string"),_T("0,0,0;0,0,0"), _T("")}, // Finestra contenente l'entità

   {GS_LISP_INITIAL_NODE,     _T("long"),  _T("0"),   _T("")},    // Codice nodo iniziale (info topologica)
   {GS_LISP_FINAL_NODE,       _T("long"),  _T("0"),   _T("")}     // Codice nodo finale (info topologica)
};
long VectorFromGraphGEOLispFunctionsLen() { return (long) ELEMENTS(VectorFromGraphGEOLispFunctions); }


// Il seguente vettore contiene i nomi delle funzioni GEOLisp 
// per leggere i dati dalla grafica con i loro valori di default
GEOLispFunction VectorToGraphGEOLispFunctions[] =
//   Nome funzione         Valore di default
//                         tipo dato, valore in stringa, descrizione
{  
   {GS_LISP_CHANGEBLOCK,         _T(""), _T(""), _T("(CHANGEBLOCK \"BlockName\")")},
   {GS_LISP_CHANGECOLOR,         _T(""), _T(""), _T("(CHANGECOLOR ColorNumber)")},
   {GS_LISP_CHANGECOLORATTR,     _T(""), _T(""), _T("(CHANGECOLORATTR \"AttributeName\" ColorNumber)")},
   {GS_LISP_CHANGEDYNBLOCKPROP,  _T(""), _T(""), _T("(CHANGEDYNBLOCKPROP \"PropertyName\" PropertyValue)")},
   {GS_LISP_CHANGEELEV,          _T(""), _T(""), _T("(CHANGEELEV Elevation")},
   {GS_LISP_CHANGEHATCH,         _T(""), _T(""), _T("(CHANGEHATCH \"HatchName\" ColorNumber Scale Rotation Unit \"LayerName\")")},
   {GS_LISP_CHANGEHTEXT,         _T(""), _T(""), _T("(CHANGEHTEXT TextHeight")},
   {GS_LISP_CHANGEHTEXTATTR,     _T(""), _T(""), _T("(CHANGEHTEXTATTR \"AttributeName\" TextHeight)")},
   {GS_LISP_CHANGELAYER,         _T(""), _T(""), _T("(CHANGELAYER \"LayerName\" [What])")},
   {GS_LISP_CHANGELAYERATTR,     _T(""), _T(""), _T("(CHANGELAYERATTR \"AttributeName\" \"LayerName\")")},
   {GS_LISP_CHANGELINETYPE,      _T(""), _T(""), _T("(CHANGELINETYPE \"LineTypeName\")")},
   {GS_LISP_CHANGEROTATION,      _T(""), _T(""), _T("(CHANGEROTATION Rotation Unit)")},
   {GS_LISP_CHANGESCALE,         _T(""), _T(""), _T("(CHANGESCALE Scale)")},
   {GS_LISP_CHANGETEXTSTYLE,     _T(""), _T(""), _T("(CHANGETEXTSTYLE \"TextStyleName\")")},
   {GS_LISP_CHANGETEXTSTYLEATTR, _T(""), _T(""), _T("(CHANGETEXTSTYLEATTR \"AttributeName\" \"TextStyleName\")")},
   {GS_LISP_CHANGETHICKNESS,     _T(""), _T(""), _T("(CHANGETHICKNESS Thickness)")},
   {GS_LISP_CHANGEWIDTH,         _T(""), _T(""), _T("(CHANGEWIDTH Width)")},
   {GS_LISP_MOVEATTR,            _T(""), _T(""), _T("(MOVEATTR \"AttributeName\" Mode OffSetX OffSetY GenerationFlag HorizAlign VertAlign RotCheck OverlapCheck)")},
   {GS_LISP_MOVESHEET,           _T(""), _T(""), _T("(MOVESHEET Mode OffSetX OffSetY RotCheck OverlapCheck)")},
   {GS_LISP_VISATTR,             _T(""), _T(""), _T("(VISATTR \"AttributeName\" Mode)")},
   {GS_LISP_VISSHEET,            _T(""), _T(""), _T("(VISSHEET Mode)")}
};
long VectorToGraphGEOLispFunctionsLen() { return (long) ELEMENTS(VectorToGraphGEOLispFunctions); }


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  LspTempo=0, LspTempo1=0, LspTempo2=0, LspTempo3=0, LspTempo4=0, LspTempo5=0, LspTempo6=0, LspTempo7=0;
   double  LspTempo8=0, LspTempo9=0, LspTempo10=0, LspTempo11=0, LspTempo12=0, LspTempo13=0, LspTempo14=0;
#endif


///////////////////////////////////////////////////////////////////////////
//PRIVATE FUNCTION

TCHAR *num2str(int ttype, double val1, int sig); // Alloca stringa in uscita

TCHAR *gsc_LISP_MathOperator(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                             size_t *ptr, const TCHAR *file_name, int flag,
                             long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                             C_STRING &Operator);
TCHAR *gsc_LISP_RelationalOperator(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                   size_t *ptr, const TCHAR *file_name, int flag,
                                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                   C_STRING &Operator);
TCHAR *gsc_LISP_AutoIncrement(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                              size_t *ptr, const TCHAR *file_name, int flag,
                              long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                              C_STRING &Operator);
TCHAR *gsc_LISP_ABS(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_Trim(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                     C_STRING &Operator);
TCHAR *gsc_LISP_AND_OR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                       C_STRING &Operator);
TCHAR *gsc_LISP_ASCII(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_AT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                   size_t *ptr, const TCHAR *file_name, int flag,
                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_AlphaToNumber(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                              size_t *ptr, const TCHAR *file_name, int flag,
                              long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                              C_STRING &Operator);
TCHAR *gsc_LISP_ChangeEntityGraphProperty(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                          size_t *ptr, const TCHAR *file_name, int flag,
                                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                          C_STRING &Operator);
TCHAR *gsc_LISP_ChangeAttrGraphProperty(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                        size_t *ptr, const TCHAR *file_name, int flag,
                                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                        C_STRING &Operator);
TCHAR *gsc_LISP_CHR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_COND(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_COORD_CONV(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                           size_t *ptr, const TCHAR *file_name, int flag,
                           long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_COPYFILE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_DATETIME(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                         C_STRING &Operator);
TCHAR *gsc_LISP_DAY(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_DEFUN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_EQUAL(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_EXPT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_GETGUID(int screen, const TCHAR *stream, size_t *ptr);
TCHAR *gsc_LISP_GS_CURR_OP(int screen, const TCHAR *stream, size_t *ptr);
TCHAR *gsc_LISP_GS_MEM(const TCHAR *stream, size_t *ptr);
TCHAR *gsc_LISP_IF(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                   size_t *ptr, const TCHAR *file_name, int flag,
                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_ITOA(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_LOOKUP(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_LOAD(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_MAX_MIN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                        C_STRING &Operator);
TCHAR *gsc_LISP_MONTH(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_MoveAttr(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_MoveSheet(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                          size_t *ptr, const TCHAR *file_name, int flag,
                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_VisAttrib(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                          size_t *ptr, const TCHAR *file_name, int flag,
                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_VisSheet(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_NOT_NULL(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_PROGN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_RTOS(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_SETQ(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_STRCASE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_STRCAT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_STRLEN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_SUBSTR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_STRTRAN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_VER(int screen, const TCHAR *stream, size_t *ptr);
TCHAR *gsc_LISP_WHILE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);
TCHAR *gsc_LISP_WHOAMI(int screen, const TCHAR *stream, size_t *ptr);
TCHAR *gsc_LISP_YEAR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken);


/*+
-------------------------------------------------------------

int gs_calc_attr(void);
       Funzione richiamata da lisp per il calcolo dei campi
       calcolati di una entita' dipendenti da un certo attributo.
       Riceve codice progetto, classe, sottoclasse, una stringa
       con il nome dell'attributo ed una lista contenete il record
       dell'entita'.
       La funzione ritorna al lisp la lista del record con i valori
       ricalcolati, oppure nil in caso di errore. 
       Ritorna al C RTERROR in caso di errore RTNORN altrimenti.
  
-------------------------------------------------------------
-*/

int gs_calc_attr(void)
{
   presbuf   arg;
   C_RB_LIST ColValues;
   int       prj, cls, sub;
   TCHAR     *attr;

   acedRetNil();

   // Legge nella lista dei parametri: prj cls sub
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Legge nella lista dei parametri: attr
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   attr      = arg->resval.rstring;
   ColValues = arg->rbnext;
   
   if (gsc_calc_attr(prj, cls, sub, attr, ColValues) == GS_BAD) return RTERROR;

   ColValues.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc (new 2) gsc_chk_attr                   <internal> */
/*+
  Funzione richiamata dal C per la verifica della validita' 
  di un campo di una entita'.
  Riceve il nome dell'attributo, i valori degli attributi sotto forma di 
  resbuf, una espressione o funzione lisp e un nome di file
  (anche NULL) dove, eventualmente, ricercare le funzioni 
  lisp non trovate.
  La funzione lisp di validita' deve ritornare T o NIL, altri valori 
  sono considerati errori.
  Parametri:
  const TCHAR *attr;     Nome attributo
  presbuf   NewValue;    Nuovo valore dell'attributo
  C_RB_LIST &ColValues;  Valori della scheda
  const TCHAR *stream;   Funzione di validità
  const TCHAR *file;     File contenente la funzione di validità

  La funzione ritorna GS_GOOD se il valore e' valido, GS_BAD 
  in caso contrario, oppure -1 in caso di errore. 
-*/  
/*********************************************************/
int gsc_chk_attr(const TCHAR *attr, presbuf NewValue, C_RB_LIST &ColValues, 
                 const TCHAR *stream, const TCHAR *file)
{
   TCHAR   value[2 * MAX_LEN_FIELD];
   int     ret, i = 0;
   size_t  ptr;
   presbuf rbAttr, pName, pVal, rb;

   if (!stream || !attr || !NewValue) 
      { GS_ERR_COD = eGSInvalidArg; return -1; }

   // Alloco variabili lisp per tutti gli attributi tranne per <attr>
   while ((rbAttr = ColValues.nth(i++)))
   {
      pName = gsc_nth(0, rbAttr);
      pVal = gsc_nth(1, rbAttr);

      // Conversione resbuf in stringa lisp
      if (rb2lspstr(pVal, value) == GS_BAD) { release_var(_T("GEOsim")); return -1; }

      if (gsc_strcmp(pName->resval.rstring, attr, FALSE) != 0)
         if (alloc_var(SCREEN_LISP, stream, &ptr, _T('L'), _T("GEOsim"), pName->resval.rstring, value, NULL) == GS_BAD) 
            { release_var(_T("GEOsim")); return -1; }
   }
   // Alloco variabile lisp per <attr>
   // Conversione resbuf in stringa lisp
   if (rb2lspstr(NewValue, value) == GS_BAD) { release_var(_T("GEOsim")); return -1; }
   if (alloc_var(SCREEN_LISP, stream, &ptr, _T('L'), _T("GEOsim"), attr, value, NULL) == GS_BAD) 
      { release_var(_T("GEOsim")); return -1; }

   if ((rb = gsc_exec_lisp(_T("GEOsim"), stream, file)) == NULL)
      { release_var(_T("GEOsim")); GS_ERR_COD = eGSInvalidValid; return -1; }

   if (rb->restype == RTNIL)
   {
      ret = GS_BAD;
      GS_ERR_COD = eGSInvalidValid;
   }
   else if (rb->restype == RTT)
      ret = GS_GOOD;
   else
      { ret = -1; GS_ERR_COD = eGSInvRBType; }
   
   acutRelRb(rb);
   release_var(_T("GEOsim")); 

   return ret;      
}  


/*+
-------------------------------------------------------------
int gsc_calc_attr(int prj,int cls,int sub,const TCHAR *attr, C_RB_LIST &ColValues)
int C_ATTRIB_LIST::calc(const TCHAR *attr,resbuf *record)

 Funzione richiamata dal C per il calcolo dei dei campi
 calcolati di una entita' dipendenti da un certo attributo.
 Riceve codice progetto, classe, sottoclasse, una stringa
 con il nome dell'attributo ed una lista contenete il record
 dell'entita'.
 La funzione ritorna la stessa lista resbuf del record con 
 i valori ricalcolati, oppure NULL in caso di errore. 
 Data la ATTRIB_LIST si puo' richiamare direttamente la
 primitiva calc.
       
-------------------------------------------------------------
-*/
int gsc_calc_attr(int prj, int cls, int sub, const TCHAR *attr, C_RB_LIST &ColValues)
{
   C_CLASS       *cls_punt;
   C_ATTRIB_LIST *list;
   C_INFO        *p_info;

   if (!attr) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if ((cls_punt = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;
   if (!(list = cls_punt->ptr_attrib_list()) || !(p_info = cls_punt->ptr_info()))
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   return list->calc(attr, ColValues, MODIFY);
}


/*********************************************************/
/*.doc C_ATTRIB_LIST::calc_all                <internal> */
/*+
  Questa funzione ricalcola tutti gli attributi calcolati.
  Parametri:
  C_RB_LIST &ColValues;    Lista dei valori di una scheda ((<nome1><val1>)...)
  int Op;                  Operazione attuale (INSERT, MODIFY; default = MODIFY,
  C_SELSET *GlobalSelSet;  Gruppo di selezione dell'entità attuale (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Deve essere già stata chiamata la "C_ATTRIB_LIST::init_ADOType"
-*/  
/*********************************************************/
int C_ATTRIB_LIST::calc_all(C_RB_LIST &ColValues, int Op, C_SELSET *GlobalSelSet)
{
   C_ATTRIB *punt;
   C_SELSET SelSet;
   int      result = GS_GOOD;
   presbuf  rb1, rb2;
   TCHAR    value[2 * MAX_LEN_FIELD];
   size_t   ptr = 0;
   long     ss_count = 0;

   // se l'operazione è INSERT allora verifico anche
   // l'esistenza di funzioni di calcolo per i default 
   if (is_calculated() == GS_BAD && (Op != INSERT || is_DefCalculated() == GS_BAD))
      return GS_GOOD;

   if (is_from_graph() == GS_GOOD) // ha almeno un attributo che proviene dalla grafica
   {
      if (GlobalSelSet == NULL)  // se non è passato come parametro, lo ricavo
      {
         if (GS_CALC_CLASS) // se è stata setta la classe corrente da calcolare
            if (((C_CLASS *) GS_CALC_CLASS)->get_SelSet(ColValues, SelSet) == GS_BAD)
               return GS_BAD;
      }
      else
         // Copio il GlobalSelSet in SelSet e 
         if (GlobalSelSet->copy(SelSet) == GS_BAD) return GS_BAD;

      // lo filtro per oggetto grafici (senza DA)
      if (SelSet.intersectType(GRAPHICAL) == GS_BAD) return GS_BAD;
      // scarto i riempimenti
      if (SelSet.subtract(_T("HATCH")) == GS_BAD) return GS_BAD;
      // numero di oggetti grafici
      if ((ss_count = SelSet.length()) < 0) return GS_BAD;
   }

   // carico le variabili locali a "GEOsim"
   punt = (C_ATTRIB *) get_head();
   while (punt)
   {
      // se non è calcolato
      // se l'operazione è INSERT allora verifico anche
      // l'esistenza di funzioni di calcolo per i default 
      if (punt->is_calculated() == GS_BAD && (Op != INSERT || punt->is_DefCalculated() == GS_BAD))
      {        
         // ricerca del valore
         rb2 = ColValues.CdrAssoc(punt->get_name());

         // Conversione resbuf in stringa lisp
         if (rb2lspstr(rb2, value, punt->len, punt->dec) == GS_BAD)
            { result = GS_BAD; break; }
         if (alloc_var(SCREEN_LISP, NULL, &ptr, _T('L'), _T("GEOsim"), punt->get_name(), value, NULL) == GS_BAD) 
            { result = GS_BAD; break; }
      }

      punt = (C_ATTRIB *) punt->get_next();   
   }
   
   if (result != GS_GOOD)
      { release_var(_T("GEOsim")); return result; }

   C_INT_VOIDPTR *pCalcOrderAttribPtr = (C_INT_VOIDPTR *) CalcOrderAttribPtrList.get_head();
   while (pCalcOrderAttribPtr)
   {
      punt = (C_ATTRIB *) pCalcOrderAttribPtr->get_VoidPtr();
      
      // se l'operazione è INSERT allora verifico anche
      // l'esistenza di funzioni di calcolo per i default 
      if (punt->is_calculated() == GS_GOOD || (Op == INSERT && punt->is_DefCalculated() == GS_GOOD))
      {
         rb2 = ColValues.CdrAssoc(punt->get_name());

         // valore proveniente da grafica
         if (punt->is_from_graph() == GS_GOOD)
         {
            if (ss_count > 0 && GS_CALC_CLASS) // ci sono oggetti in grafica
            {
               if ((rb1 = ((C_CLASS *)GS_CALC_CLASS)->get_graphical_data(SelSet, punt->calc_func,
                                                                         punt->len, punt->dec, 
                                                                         NULL, false)) == NULL)
                  { result = GS_BAD; break; }

               // sostituzione del risultato
               if (((C_CLASS *)GS_CALC_CLASS)->CheckValue(punt, rb1) == GS_BAD)
                  { acutRelRb(rb1); result = GS_BAD; break; }
               else
                  if (gsc_sostitutebuf(rb1, rb2) == GS_BAD)
                     { acutRelRb(rb1); result = GS_BAD; break; }
               acutRelRb(rb1);   
            }
         }
         else // valore NON proveniente da grafica
         {
            // se l'operazione corrente è di modifica della struttura della tabella della classe
            // allora non devo calcolare i campi che dipendono dalle seguenti funzioni lisp:
            // (whoami), (date), (time), (datetime)
            if (Op == MODIFY_ON_STRUCT_UPD &&
                (gsc_is_param(punt->calc_file, punt->calc_func, _T("WHOAMI")) == GS_GOOD ||
                 gsc_is_param(punt->calc_file, punt->calc_func, _T("DATE")) == GS_GOOD ||
                 gsc_is_param(punt->calc_file, punt->calc_func, _T("TIME")) == GS_GOOD ||
                 gsc_is_param(punt->calc_file, punt->calc_func, _T("DATETIME")) == GS_GOOD))
            {
            }
            else
            {
               if ((rb1 = gsc_exec_lisp(_T("GEOsim"), punt->calc_func, punt->calc_file)) == NULL)
                  { result = GS_BAD; break; }

               // sostituzione del risultato
               if (punt->CheckValue(rb1) == GS_BAD)
                  rb2->restype = RTNIL;
               else
                  if (gsc_sostitutebuf(rb1, rb2) == GS_BAD)
                     { acutRelRb(rb1); result = GS_BAD; break; }
               acutRelRb(rb1);
            }
         }

         // Conversione resbuf in stringa lisp
         if (rb2lspstr(rb2, value, punt->len, punt->dec) == GS_BAD) { result = GS_BAD; break; }
         if (alloc_var(SCREEN_LISP, NULL, &ptr, _T('L'), _T("GEOsim"), punt->get_name(), value, NULL) == GS_BAD) 
            { result = GS_BAD; break; }
      }
      
      pCalcOrderAttribPtr = (C_INT_VOIDPTR *) pCalcOrderAttribPtr->get_next();   
   }

   release_var(_T("GEOsim")); 

   return result;   
}


/*********************************************************/
/*.doc C_ATTRIB_LIST::calc <internal> */
/*+                                                                       
  Ricalcola tutti gli attributi che dipendono dal cambiamento di un attributo.
  Parametri:
  const TCHAR *attr;       Nome attributo modificato
  presbuf    ColValues;    Lista ((<nome colonna><valore>) ...)
  						         di tutti gli attributi della classe
  int Op;                  Operazione attuale (INSERT, MODIFY; default = MODIFY,

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Gli attributi calcolati da grafica NON vengono ricalcolati ma devono
        già essere inizializzati !
        Deve essere già stata chiamata la "C_ATTRIB_LIST::init_ADOType".
-*/  
/*********************************************************/
int C_ATTRIB_LIST::calc(const TCHAR *attr, C_RB_LIST &ColValues, int Op)
{
   C_ATTRIB      *punt, *punt2;
   TCHAR         *stream, *attrib_name, value[2 * MAX_LEN_FIELD];
   int           flag = 0, i, result = GS_GOOD;
   size_t        ptr = 0;
   presbuf       rb1, rb2;
   C_INT_VOIDPTR *pCalcOrderAttribPtr = (C_INT_VOIDPTR *) CalcOrderAttribPtrList.get_head();

   while (pCalcOrderAttribPtr)
   {
      punt = (C_ATTRIB *) pCalcOrderAttribPtr->get_VoidPtr();
      
      // se l'operazione è INSERT allora verifico anche
      // l'esistenza di funzioni di calcolo per i default 
      if (punt->is_calculated() == GS_GOOD || (Op == INSERT && punt->is_DefCalculated() == GS_GOOD))
      {
         stream = punt->calc_func;
         // se attr è parametro della funzione
         if (gsc_is_param(punt->calc_file, stream, attr) == GS_GOOD)
         {
            if (flag == 0) // CARICO LE VARIABILI LOCALI A "GEOsim"
            {
               flag = 1;
               
               i = 0;
               while ((rb1 = ColValues.nth(i++)) != NULL)
               {
                  rb2 = gsc_nth(0, rb1);

                  if (rb2->restype != RTSTR) 
                     { GS_ERR_COD = eGSInvRBType; result = GS_BAD; break; }
                  attrib_name = rb2->resval.rstring;   // Nome variabile = attributo
               
                  if ((punt2 = (C_ATTRIB *) search_name(attrib_name)) == NULL)
                     { result = GS_BAD; break; }

                  rb2 = gsc_nth(1, rb1);
                  // Conversione resbuf in stringa lisp
                  if (rb2lspstr(rb2, value, punt2->len, punt2->dec) == GS_BAD)
                     { result = GS_BAD; break; }
                  if (alloc_var(SCREEN_LISP, NULL, &ptr, _T('L'), _T("GEOsim"), attrib_name, value, NULL)==GS_BAD) 
                     { result = GS_BAD; break; }
               } 

               if (result == GS_BAD) break;
            }   // End if flag
            if ((rb1 = gsc_exec_lisp(_T("GEOsim"), punt->calc_func, punt->calc_file)) == NULL)
               { result = GS_BAD; GS_ERR_COD = eGSInvalidCalc; break; }

            // sostituzione del risultato
            rb2 = ColValues.CdrAssoc(punt->get_name());
            if (punt->CheckValue(rb1) == GS_BAD)
               rb2->restype = RTNIL;
            else
               if (gsc_sostitutebuf(rb1, rb2) == GS_BAD)
                  { acutRelRb(rb1); result = GS_BAD; break; }
            
            // Conversione resbuf in stringa lisp
            if (rb2lspstr(rb2, value, punt->len, punt->dec) == GS_BAD)
               { result = GS_BAD; break; }
            if (alloc_var(SCREEN_LISP, NULL, &ptr, _T('L'), _T("GEOsim"), punt->get_name(), value, NULL) == GS_BAD) 
               { result = GS_BAD; break; }

            acutRelRb(rb1);   
         }
         else
         {
            pCalcOrderAttribPtr = (C_INT_VOIDPTR *) pCalcOrderAttribPtr->get_next();
            continue;
         }
      }   //  End if calc

      pCalcOrderAttribPtr = (C_INT_VOIDPTR *) pCalcOrderAttribPtr->get_next();   
   }   // End while

   if (flag) release_var(_T("GEOsim")); // Scarico le variabili locali "GEOsim"
   
   return result;      
} 


/*********************************************************/
/*.doc C_ATTRIB_LIST::validate_all            <internal> */
/*+
  Questa funzione verifica eventuali funzioni di validità dei dati di una lista resbuf
  Parametri:
  C_RB_LIST &ColValues;    Lista valori del record ((nomecampo-valore)...)
  C_ATTRIB **pWrongAttrib; Attributo errato (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Deve essere già stata chiamata la "C_ATTRIB_LIST::init_ADOType"
-*/  
/*********************************************************/
int C_ATTRIB_LIST::validate_all(C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib)
{
   C_ATTRIB *punt, *punt2;
   TCHAR    *stream, *attrib_name, value[2 * MAX_LEN_FIELD];
   int      i = 0, result = GS_GOOD;
   size_t   ptr;
   presbuf  rb1, rb2;

   // verifico l'esistenza di funzioni di validità
   if (is_validated() == GS_BAD) return GS_GOOD;

   if (pWrongAttrib) *pWrongAttrib = NULL;

   // carico le variabili locali a "GEOsim"
   while ((rb1 = ColValues.nth(i++)) != NULL)
   {
      rb2 = gsc_nth(0, rb1);

      if (rb2->restype != RTSTR) 
         { GS_ERR_COD = eGSInvRBType; result = GS_BAD; break; }
      attrib_name = rb2->resval.rstring;   // Nome variabile = attributo
      
      if ((punt2 = (C_ATTRIB *) search_name(attrib_name)) == NULL)
         { result = GS_BAD; break; }

      rb2 = gsc_nth(1, rb1);
      // Conversione resbuf in stringa lisp
      if (rb2lspstr(rb2, value, punt2->len, punt2->dec) == GS_BAD)
         { result = GS_BAD; break; }
      if (alloc_var(SCREEN_LISP, NULL, &ptr, _T('L'), _T("GEOsim"), attrib_name, value, NULL)==GS_BAD) 
         { result = GS_BAD; break; }
   }

   if (result == GS_BAD)
      { release_var(_T("GEOsim")); GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   punt = (C_ATTRIB *) get_head();
   while (punt)
   { 
      if (punt->is_validated() == GS_GOOD)
      {
         stream = punt->valid_func;

         if ((rb1 = gsc_exec_lisp(_T("GEOsim"), stream, punt->valid_file)) == NULL)
            { result = GS_BAD; break; }

         if (rb1->restype == RTNIL)
         {
            acutRelRb(rb1);
            result = GS_BAD;
            break;
         }
         acutRelRb(rb1);
      }
      
      punt = (C_ATTRIB *) punt->get_next();   
   }   // End while

   release_var(_T("GEOsim")); // Scarico le variabili locali "GEOsim"

   if (result == GS_BAD)
   {
      if (pWrongAttrib) *pWrongAttrib = punt;
      GS_ERR_COD = eGSInvalidValid;
   }

   return result;
}  


/*********************************************************/
/*.doc C_ATTRIB_LIST::validate                <internal> */
/*+                                                                       
  Verifica eventuali funzioni di validità per tutti gli attributi che 
  dipendono dal cambiamento di un attributo.
  Parametri:
  const TCHAR *attr;       Nome attributo modificato
  presbuf    ColValues;    Lista ((<nome colonna><valore>) ...)
  						         di tutti gli attributi della classe
  C_ATTRIB **pWrongAttrib; Attributo errato (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Deve essere già stata chiamata la "C_ATTRIB_LIST::init_ADOType"
-*/  
/*********************************************************/
int C_ATTRIB_LIST::validate(const TCHAR *attr, C_RB_LIST &ColValues, C_ATTRIB **pWrongAttrib)
{
   C_ATTRIB *punt, *punt2;
   TCHAR     *stream, *attrib_name, value[2 * MAX_LEN_FIELD];
   int      flag = 0, i, result = GS_GOOD;
   size_t   ptr;
   presbuf  rb1, rb2;

   if (pWrongAttrib) *pWrongAttrib = NULL;

   punt = (C_ATTRIB *) get_head();
   while (punt)
   { 
      if (punt->is_validated() == GS_GOOD)
      {
         stream = punt->valid_func;
         if (flag == 0) // CARICO LE VARIABILI LOCALI A "GEOsim"
         {
            flag = 1;
            ptr  = 0;

            i    = 0;
            while ((rb1 = ColValues.nth(i++)) != NULL)
            {
               rb2 = gsc_nth(0, rb1);

               if (rb2->restype != RTSTR) 
                  { GS_ERR_COD = eGSInvRBType; result = GS_BAD; break; }
               attrib_name = rb2->resval.rstring;   // Nome variabile = attributo
               
               if ((punt2 = (C_ATTRIB *) search_name(attrib_name)) == NULL)
                  { result = GS_BAD; break; }

               rb2 = gsc_nth(1, rb1);
               // Conversione resbuf in stringa lisp
               if (rb2lspstr(rb2, value, punt2->len, punt2->dec) == GS_BAD)
                  { result = GS_BAD; break; }
               if (alloc_var(SCREEN_LISP, stream, &ptr, _T('L'), _T("GEOsim"), attrib_name, value, NULL)==GS_BAD) 
                  { result = GS_BAD; break; }
            }

            if (result == GS_BAD) break;
         }
         
         // se attr è parametro della funzione
         if (gsc_is_param(punt->valid_file, stream, attr) == GS_GOOD)
         {
            if ((rb1 = gsc_exec_lisp(_T("GEOsim"), stream, punt->valid_file)) == NULL)
               { result = GS_BAD; break; }

            if (rb1->restype == RTNIL)
            {
               acutRelRb(rb1);
               result = GS_BAD;
               break;
            }
            acutRelRb(rb1);
         }
      }

      punt = (C_ATTRIB *) punt->get_next();
   }   // End while

   if (flag) release_var(_T("GEOsim")); // Scarico le variabili locali "GEOsim"

   if (result == GS_BAD)
   {
      if (pWrongAttrib) *pWrongAttrib = punt;
      GS_ERR_COD = eGSInvalidValid;
   }

   return result;
}


/*********************************************************/
/*.doc C_CLASS::graph_calc <internal> */
/*+                                                                       
  Calcola la funzione di modifica dalla grafica.
  Parametri:
  C_RB_LIST &ColValues;    Lista ((<nome colonna><valore>) ...)
  			                  di tutti gli attributi della classe
  C_SELSET *SelSet;        Gruppo di link agli oggetti grafici ed ai blocchi DA

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::graph_calc(C_RB_LIST &ColValues, C_SELSET *SelSet)
{
   TCHAR         *attrib_name, value[2 * MAX_LEN_FIELD];
   int           i, result = GS_GOOD;
   size_t        ptr;
   presbuf       rb1, rb2;
   C_FAS         *pFas = ptr_fas();
   C_ATTRIB_LIST *pAttribList = ptr_attrib_list();
   C_ATTRIB      *pAttrib;

   if (!pFas || pFas->is_graph_calculated() == GS_BAD || !pAttribList) return GS_GOOD;

   if (GS_CURRENT_WRK_SESSION == NULL) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD || !pFas) return GS_BAD;

   // non verifico is_extracted() == GS_GOOD perchè durante l'estrazione 
   // con riallineamneto automatico la classe non risulta ancora estratta
   // ma questa funzione non deve rifiutare il calcolo grafico
   if (id.abilit == GSUpdateableData)
   {        
      if (SelSet == NULL)  // se non è passato come parametro, lo ricavo
      {
         if (get_SelSet(ColValues, GS_SELSET) == GS_BAD)
            { GS_SELSET.clear(); return GS_BAD; }
      }
      else
         SelSet->copy(GS_SELSET);

      GS_CALC_CLASS = this;

      i = 0;
      while ((rb1 = ColValues.nth(i++)) != NULL)
      {
         rb2 = gsc_nth(0, rb1);
         if (rb2->restype != RTSTR) 
         {
            result = GS_BAD;
            GS_ERR_COD = eGSInvRBType;
            break;
         }
         attrib_name = rb2->resval.rstring;   // Nome variabile=attributo

         if ((pAttrib = (C_ATTRIB *) pAttribList->search_name(attrib_name, FALSE)) == NULL)
            { result = GS_BAD; break; }

         rb2 = gsc_nth(1, rb1);
         // Conversione resbuf in stringa lisp
         if (rb2lspstr(rb2, value, pAttrib->len, pAttrib->dec) == GS_BAD)
            { result = GS_BAD; break; }
         if (alloc_var(SCREEN_LISP, pFas->fun_graph_calc, &ptr, _T('L'), _T("GEOsim"),
                       attrib_name, value, NULL) == GS_BAD) 
            { result = GS_BAD; break; }
      }   // End for 

      if ((rb1 = gsc_exec_lisp(_T("GEOsim"), pFas->fun_graph_calc, pFas->file_graph_calc)) != NULL)
         acutRelRb(rb1);

      release_var(_T("GEOsim")); 
   }

   GS_SELSET.clear();  // rilascio il SelSet globale
   GS_CALC_CLASS = NULL;

   return result;      
}


/*********************************************************/
/*.doc rb2lspstr <internal> */
/*+
  Questa funzione converte il valore di un resbuf in una stringa
  utilizzabile all'interprete lisp.
  Parametri:
  resbuf *rb;     valore in forma di resbuf
  TCHAR  *value;  stringa di ritorno già allocata a 2 * MAX_LEN_FIELD !!!
  int    len;     usato solo in caso di numero reale (default = -1)
  int    dec;     usato solo in caso di numero reale (default = -1)
      
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int rb2lspstr(resbuf *rb, TCHAR *value, int len, int dec)
{ 
   if (!rb || !value) return GS_BAD;
   
   switch(rb->restype)
   {
      case RTNIL : case RTNONE : case RTVOID : 
        wcscpy(value, _T("nil")); break;

      case RTT :
        wcscpy(value, _T("T")); break;
        
      case RTSTR :
      {
         TCHAR *dummy = rb->resval.rstring;
         
         if (wcslen(dummy) >= MAX_LEN_FIELD)
            { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
         dummy = stringto(dummy);
         gsc_strcpy(value, dummy, 2 * MAX_LEN_FIELD);
         free(dummy);
         break;
      }
      case RTLONG :
        gsc_ltoa(rb->resval.rlong, value, 10); break;
        
      case RTREAL :
      {
         TCHAR *dummy;

         if (len == -1 && dec == -1)
         {
            if ((dummy = gsc_tostring(rb->resval.rreal)) == NULL) return GS_BAD;
         }
         else
            if ((dummy = gsc_tostring(rb->resval.rreal, len, dec)) == NULL) return GS_BAD;

         gsc_strcpy(value, dummy, 2 * MAX_LEN_FIELD);
         break;
      }   
      case RTSHORT :
        gsc_itoa(rb->resval.rint, value, 10); break;
      default :
        GS_ERR_COD = eGSInvRBType; return GS_BAD;
   }  // End switch   

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_is_param <internal> */
/*+
  Questa funzione riceve il nome del file contentente una funzione lisp
  (che può essere stringa nulla) e una stringa rappresentante il corpo di
  una funzione lisp (o il nome di una funzione contenuta nel file).
  Lo scopo è controllare se "par" è un parametro usato dalla funzione lisp.
  Parametri:
  const TCHAR *calc_file;   nome del file
  const TCHAR *calc_func;   corpo o nome della funzione
  const TCHAR *par;         token da controllare se usato dalla funzione

  Restituisce GS_GOOD in caso affermativo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_is_param(const TCHAR *calc_file, const TCHAR *calc_func, const TCHAR *par)
{
   // se si tratta di una funzione di lettura della grafica
   if (gsc_is_from_graph(calc_func) == GS_GOOD) return GS_BAD;

   if (gsc_HowManyToken(calc_func, calc_file, par) > 0) return GS_GOOD;
   else return GS_BAD;
}

/*+
-------------------------------------------------------------

resbuf *gsc_exec_lisp(const TCHAR *call, const TCHAR *stream, const TCHAR *name_file )
       Riceve una stringa contenente una chiamata ad una 
       funzione oppure una espressione lisp. Se incorre una 
       chiamata a funzione non esistente si carica il file.
       E' necessario inserire anche il nome della procedura chiamante,
       stringa vuota se si tratta del main.
       Si esegue il codice lisp traducendo il risultato in resbuf.
  Parametri:
  const TCHAR *call;        ambiente in cui si erano dichiarate tutte le variabili
                           usate da stream
  const TCHAR *stream;      codice geolisp da eseguire
  const TCHAR *name_file;   Nome del file da caricare per trovare la funzione lisp
                           richiamata da stream
  int screen;              Se TRUE visualizza i messaggi di errore altrimenti no

  Ritorna NULL in caso di errore.
-------------------------------------------------------------
-*/
resbuf *gsc_exec_lisp(const TCHAR *call, const TCHAR *stream, const TCHAR *name_file,
                      int screen)
{
	C_STRING func, result, tmp_stream;
	TCHAR    *string;
	resbuf   *ret;
   size_t   ptr = 0, len_init_Curr_stream = 0;
  
   if (!stream || !call) { GS_ERR_COD = eGSInvalidArg; return NULL; } 
   tmp_stream = stream;

   if (conv_stream(tmp_stream.get_name()) == GS_BAD) return NULL;

	len_init_Curr_stream = wcslen(tmp_stream.get_name());
   if (len_init_Curr_stream == 0)  return NULL;

   while (ptr < len_init_Curr_stream)
   {
	  	result.paste(what_is(screen, call, tmp_stream.get_name(), &ptr, name_file, 0));  // eseguo
	  	if (result.get_name() == NULL) return NULL;

      del_comment(tmp_stream.get_name(), &ptr);
   }

   string = result.get_name();

   if (gsc_strcmp(string, _T("nil")) == 0)
   { 
      ret = acutBuildList(RTNIL, 0);
   }
   else if (gsc_strcmp(string, _T("T")) == 0)
   {
      ret = acutBuildList(RTT, 0);
   }
   else if (isnumber(string))
   {
      ret = acutBuildList(RTREAL, _wtof(string), 0);
   } 
   else if (isstring(string))
   {
      if (tostring(string) == NULL) return NULL;
      ret = acutBuildList(RTSTR, string, 0);
   }
   else 
      { GS_ERR_COD=eGSInvLispVarType; ret=NULL; }
  
   if (!ret) GS_ERR_COD = eGSOutOfMem; 
   
   return ret;   
}


/*********************************************************/
/*.doc gsc_WhatIsGraphModified <internal> */
/*+
  Questa funzione  esamina il corpo della funzione di calcolo grafico 
  restituendo quali caratteristiche grafiche che vengono modificate (solo per
  le principali) sotto forma di un codice a bit codificato 
  come GraphSettingsEnum.  
  Parametri:
  TCHAR *file_graph_calc;    nome del file contenente la funzione GEOlisp
  TCHAR *fun_graph_calc;     nome o corpo della funzione GEOlisp

  Restituisce codice a bit in caso di successo altrimenti restituisce -1.
  N.B. deve essere già stato caricato un eventuale GSL.GSL dal progetto della classe
-*/  
/*********************************************************/
long gsc_WhatIsGraphModified(const TCHAR *fun_graph_calc, const TCHAR *file_graph_calc)
{
	long     Flag = 0;
	C_STRING result, tmp_stream;
	int      screen = TRUE;
   size_t   ptr = 0, len_init_Curr_stream = 0;
  
   if (!fun_graph_calc) return 0; 
   tmp_stream = fun_graph_calc;
   if (conv_stream(tmp_stream.get_name()) == GS_BAD) return -1;

	len_init_Curr_stream = wcslen(tmp_stream.get_name());
   if (len_init_Curr_stream == 0)  return 0;

   while (ptr < len_init_Curr_stream)
   {
	  	result.paste(what_is(screen, GS_EMPTYSTR, tmp_stream.get_name(), &ptr, file_graph_calc, 0, &Flag));  // eseguo
	  	if (result.get_name() == NULL) return -1;

      del_comment(tmp_stream.get_name(), &ptr);
   }

   return Flag;   
}
long gsc_WhatIsGraphModified(C_NODE *pclass)
{
	long      Flag = 0, Partial;
   C_FAS     *p_fas;
   C_CLASS   *pCls = (C_CLASS *) pclass;
   C_ID      *p_id = pCls->ptr_id();
   C_PROJECT *pPrj = (C_PROJECT *) pCls->ptr_id()->pPrj;

   switch (p_id->category)
   {
      case CAT_GROUP:
      {
         C_INT_INT *p_group = (C_INT_INT *) pCls->ptr_group_list()->get_head();

         while (p_group)
         {  
            // Ritorna il puntatore alla classe cercata
            if ((pCls = pPrj->find_class(p_group->get_key())) == NULL)
               return -1;

            if ((Partial = gsc_WhatIsGraphModified(pCls)) == -1) return -1;
            Flag    = Flag | Partial;

            p_group = (C_INT_INT *) p_group->get_next();     
         }
         return Flag;
      }
   }

   if (pCls->ptr_info() && (p_fas = pCls->ptr_fas()) && p_fas->is_graph_calculated() == GS_GOOD)
	{
      // Se siamo in una sessione di lavoro di un progetto diverso da quello della classe
      if (GS_CURRENT_WRK_SESSION && GS_CURRENT_WRK_SESSION->get_pPrj() != pCls->ptr_id()->pPrj)
      {
		   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
		   gsc_load_gsl(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_dir());
         Flag = gsc_WhatIsGraphModified(p_fas->fun_graph_calc, p_fas->file_graph_calc);
		   // ricarico GSL.GSL giusto
		   gs_gsl_reload();
      }
      else
         Flag = gsc_WhatIsGraphModified(p_fas->fun_graph_calc, p_fas->file_graph_calc);
   }

   return Flag;
}


/*********************************************************/
/*.doc gsc_HowManyToken <internal> */
/*+
  Questa funzione esamina il corpo della funzione restituendo quante volte
  compare il token noto
  Parametri:
  const TCHAR *file_graph_calc;    nome del file contenente la funzione GEOlisp
  const TCHAR *fun_graph_calc;     nome o corpo della funzione GEOlisp
  const TCHAR *Token               Token da cercare

  Restituisce il conteggio del token in caso di successo altrimenti restituisce -1.
-*/  
/*********************************************************/
int gsc_HowManyToken(const TCHAR *fun_graph_calc, const TCHAR *file_graph_calc,
                     const TCHAR *Token)
{
	C_STRING result, tmp_stream;
	int      screen = TRUE, Qty = 0;
   size_t   ptr = 0, len_init_Curr_stream = 0;
  
   if (!fun_graph_calc || !Token || wcslen(Token) == 0) return 0; 
   tmp_stream = fun_graph_calc;
   if (conv_stream(tmp_stream.get_name()) == GS_BAD) return -1;

	len_init_Curr_stream = wcslen(tmp_stream.get_name());
   if (len_init_Curr_stream == 0)  return 0;

   while (ptr < len_init_Curr_stream)
   {
	  	result.paste(what_is(screen, GS_EMPTYSTR, tmp_stream.get_name(), &ptr, file_graph_calc, 0, NULL,
	  	                     Token, &Qty));  // eseguo
	  	if (result.get_name() == NULL) return -1;

      del_comment(tmp_stream.get_name(), &ptr);
   }

   return Qty;   
}


/*************************************************************************/
/*.doc gslisp()                                                         */
/*+
   Comando che attiva l'interprete lisp.
-*/
/*************************************************************************/			   
void gslisp(void)
{
   C_STR result, Input_string;
   TCHAR *Curr_stream;
   int   loop, 
         screen = SCREEN_LISP;      // indica se GSLISP sta funzionando da linea di
                                    // comando (TRUE) o da altro programma (FALSE)
   size_t len_init_Curr_stream = 0;

   GEOsimAppl::CMDLIST.StartCmd();

   acutPrintf(_T("\n\n%s\n\n"), gsc_msg(474)); // "Interprete GEOsim Lisp versione ..."
   acutPrintf(_T("Type EXIT and press <ENTER> to quit.\n"));

   do
   {
      do
      {
         if ((loop = readscreen(screen, &Input_string))==0) break;
      }
      while ((len_init_Curr_stream = Input_string.len())<1);

      Curr_stream = Input_string.get_name();

      if (loop == 1)
      {
         while (Curr_ptr < len_init_Curr_stream)
         {            
            result.paste(what_is(screen, GS_EMPTYSTR/*"geomain"*/, Curr_stream, &Curr_ptr, NULL, 0));
            if (result.get_name() == NULL)
            { 
               acutPrintf(_T("%s\n"), Curr_stream);
               acutPrintf(_T("*Annullato*\n")); 
               break; 
            }
            
            del_comment(Curr_stream, &Curr_ptr);
         }
         if (result.len()>0)
            acutPrintf(_T("%s\n"), result.get_name());
      }

      if (loop == -1) { } // Errore di lettura da tastiera //
   }
   while (loop != 0);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*+
-------------------------------------------------------------
int readscreen(int screen)

    ritorna  0  se legge "EXIT"
    ritorna  1  se legge una stringa valida
    ritorna -1  in caso di errore.

-------------------------------------------------------------
-*/
//int readscreen(int screen)
int readscreen(int screen, C_STR *Input_string)
{
  size_t len;
  int    flag = 0, par_open = 0,
         special_char=FALSE; /* flag per indicare se il carattere corrente e'
                               speciale nella stringa e quindi bisogna
                               continuare il ciclo */

  TCHAR tmp_string[LEN_TMP_STR],  // Stringa in ingresso
        prompt[50], number[20],
        *tmp_stream;              // Usato per puntare 'Input_string.name'
  

  Curr_ptr = 0;
  Input_string->clear();    // Cancella ultima stringa 

  wcscpy(prompt, _T("GSLI[0]>"));
  while(1)
  {  
     if (ads_getstring(1, prompt, tmp_string)==RTNORM)
     {   
        par_open += count_par(tmp_string, &flag);

        len = wcslen(tmp_string);
        if (flag==0 && par_open>0)
        {
           tmp_string[len] = _T('\n'); // Aggiunge enter alla fine
           tmp_string[len + 1] = _T('\0');  
        }
        if (flag == 1)            // Toglie '\' alla fine di una stringa
        {
           if (tmp_string[len-1] == _T('\\')) tmp_string[len-1] = _T('\0');
        }
        
        //if (Input_string.cat(tmp_string)==GS_BAD) return -1;
        if (Input_string->cat(tmp_string)==GS_BAD) return -1;
        if (par_open == 0) break;
        if (par_open < 0) // errore: una parentesi destra è di troppo
        {
           acutPrintf(_T("\nExtra right parenthesis.\n*Cancelled*\n"));
           return -1;
        }
     }

     wcscpy(prompt, _T("GSLI["));
     wcscat(prompt, gsc_itoa(par_open, number, 10));
     wcscat(prompt, _T("]>"));
  }

  //tmp_stream = Input_string.get_name();
  tmp_stream = Input_string->get_name();

  if (conv_stream(tmp_stream)==GS_BAD) return -1;

  if (gsc_strcmp(tmp_stream, _T("EXIT")) == 0) return 0;

  return 1;
}
/*+
-------------------------------------------------------------
int count_par(const TCHAR *stream,int *flag)

 conta la differenza tra parentesi aperte e chiuse in una stringa 
 senza contare tutto cio' all'interno di una stringa o di un commento
 il parametro flag in ingresso indica se bisogna considerarsi 
 inizialmente all'interno di un commento (*flag=2) o di una stringa
 (*flag=1) o di nessuno di essi (*flag=0),
 in uscita indica una situazione di commento aperto e non chiuso o
 stringa aperta e non chiusa o tutto ok.

-------------------------------------------------------------
*/
int count_par(const TCHAR *stream,int *flag)
{
   int i = 0, open_par = 0, close_par = 0;

   // STRINGA APERTA
   if ((*flag) == 1)
   {
      while (stream[i] != _T('\"') && stream[i] != _T('\0'))
      {
         i++; 
         if (stream[i - 1] == _T('\\') && stream[i] != _T('\0')) 
            i++; 
      }

      if (stream[i] == _T('\0')) *flag = 1;
      else *flag = 0;      

      if (stream[i] == _T('\"'))  // Chiude virgolette
         i++; 
   }

   // COMMENTO APERTO
   if ((*flag) == 2)
   { 
      while( stream[i] != _T('\0') && 
             (stream[i++] != _T('|') || stream[i] != _T(';')));                          

      if (stream[i] == _T('\0')) *flag = 2;      
      else { i++; *flag = 0; }                   
   }

   /////////////////////////////
   while(stream[i] != _T('\0'))
   {
      switch (stream[i])
      {
         // STRINGHE
         case _T('\"'):
            do
            {
               i++; 
               if (stream[i - 1] == _T('\\') && stream[i] != _T('\0')) 
                  i++;
            }
            while (stream[i] != _T('\"') && stream[i] != _T('\0'));
            
            if (stream[i] == _T('\0')) *flag=1;      

            i++;    // VIRGOLETTE
            break;

         // COMMENTI
         case _T(';'):
            if (stream[i + 1] == _T('|'))
            {
               while (stream[i] != _T('\0') && 
                      (stream[i++] != _T('|') || stream[i] != _T(';')));                          

               if (stream[i] == _T('\0')) *flag = 2;
               else i++;
            }
            else
               while (stream[++i] != _T('\n') && stream[i] != _T('\0'));
            break;                              
                  
         // PARENTESI
         case _T('('):
            open_par++;
            i++;
            break;
         
         case _T(')'):
            close_par++;
            i++;
            break;
         
         default:
            i++;
      }
   } // END WHILE

   return (open_par - close_par);
}


/******************************************************************/
/*.doc what_is <internal>
/*+                                                            
  Funzione centrale dell'interprete lisp. 
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL (default = NULL) esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL (default = NULL) esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL (default = NULL) allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
  N.B.: Nel caso di funzioni di calcolo grafico la funzione non aggiunge il Selection Set
         in GEOsimAppl::SAVE_SS per salvataggio
  N.B.: Deve essere già stato caricato un eventuale GSL.GSL dal progetto della classe
-*/  
/*********************************************************/
TCHAR *what_is(int screen, const TCHAR *curr_fun, const TCHAR *stream,
               size_t *ptr, const TCHAR *file_name, int flag,
               long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   size_t   stream_len;
  
   // acutPrintf("CALL->WHAT_IS(\"%s\",\"%.25s\")\n",curr_fun,stream+(*ptr));

   del_comment(stream,ptr);
   
   cmd.clear();

   stream_len= wcslen(stream);
   if (*ptr >= stream_len) // return cmd.cut();
      return error(screen, stream, ptr, eGSInvLispFunc, _T("ERROR: Invalid function stream."));
  
   if (stream[(int)*ptr] == _T('('))
   {
      (*ptr)++;
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 1, 
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
      del_comment(stream, ptr);
   }

   if (stream[*ptr] == _T(')') || *ptr >= stream_len || stream[*ptr] == _T('\0'))
   {
      (*ptr)++;
      if (cmd.get_name() == NULL)
         return error(screen, stream, ptr, eGSInvLispFunc, _T("ERROR: Invalid function stream."));
      else
         return cmd.cut();
   }

   cmd.paste(reco(screen, stream, ptr));      // riconosce comando
   if (cmd.get_name() == NULL) return NULL;

   // se modalità TokenToSearch verifico se cmd = TokenToSearch
   if (TokenToSearch && wcslen(TokenToSearch) > 0 && QtyToken )
      if (gsc_strcmp(TokenToSearch, cmd.get_name(), FALSE) == 0) // non sensibile al maiuscolo/minuscolo
         (*QtyToken)++;

   if (cmd.comp(_T("nil")) == 0)
   {
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("T")) == 0)
   {
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("-")) == 0 ||
            cmd.comp(_T("+")) == 0 ||
            cmd.comp(_T("*")) == 0 ||
            cmd.comp(_T("/")) == 0)
   {
      cmd.paste(gsc_LISP_MathOperator(screen, curr_fun, stream,
                                      ptr, file_name, flag,
                                      WhatIsGraphModified, TokenToSearch, QtyToken,
                                      cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T(">")) == 0 ||
            cmd.comp(_T("<")) == 0 ||
            cmd.comp(_T(">=")) == 0 ||
            cmd.comp(_T("<=")) == 0 ||
            cmd.comp(_T("/=")) == 0 ||
            cmd.comp(_T("=")) == 0)
   {
      cmd.paste(gsc_LISP_RelationalOperator(screen, curr_fun, stream,
                                            ptr, file_name, flag,
                                            WhatIsGraphModified, TokenToSearch, QtyToken,
                                            cmd));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("1+")) == 0 || cmd.comp(_T("1-")) == 0)
   {
      cmd.paste(gsc_LISP_AutoIncrement(screen, curr_fun, stream,
                                       ptr, file_name, flag,
                                       WhatIsGraphModified, TokenToSearch, QtyToken,
                                       cmd));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("ABS")) == 0)
   {
      cmd.paste(gsc_LISP_ABS(screen, curr_fun, stream,
                             ptr, file_name, flag,
                             WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("ALLTRIM")) == 0 ||
           cmd.comp(_T("LTRIM")) == 0 ||
           cmd.comp(_T("TRIM")) == 0 ||
           cmd.comp(_T("RTRIM")) == 0)
   {
      cmd.paste(gsc_LISP_Trim(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken,
                              cmd));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("AND")) == 0 || cmd.comp(_T("OR")) == 0)   // (and expr ...) 
   {
      cmd.paste(gsc_LISP_AND_OR(screen, curr_fun, stream,
                                ptr, file_name, flag,
                                WhatIsGraphModified, TokenToSearch, QtyToken,
                                cmd));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("ASCII")) == 0)
   {
      cmd.paste(gsc_LISP_ASCII(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("AT")) == 0) // (at string char [start] [length]) come clipper
   {
      cmd.paste(gsc_LISP_AT(screen, curr_fun, stream,
                            ptr, file_name, flag,
                            WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("ATOF")) == 0 || cmd.comp(_T("ATOI")) == 0)
   {
      cmd.paste(gsc_LISP_AlphaToNumber(screen, curr_fun, stream,
                                       ptr, file_name, flag,
                                       WhatIsGraphModified, TokenToSearch, QtyToken,
                                       cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(GS_LISP_CHANGEBLOCK) == 0        || // (changeblock blockname)           
				cmd.comp(GS_LISP_CHANGECOLOR) == 0        || // (changecolor numcolor)
            cmd.comp(GS_LISP_CHANGEDYNBLOCKPROP) == 0 || // (changedynblockprop propname propvalue [What])
				cmd.comp(GS_LISP_CHANGEELEV) == 0         || // (changeelev  numelevation)
				cmd.comp(GS_LISP_CHANGEHATCH) == 0        || // (changehatch hatchname ColorNumber Scale Rotation Unit LayerName)
				cmd.comp(GS_LISP_CHANGEHTEXT) == 0        || // (changehtext numhtext)
				cmd.comp(GS_LISP_CHANGELAYER) == 0        || // (changelayer layername [What])
				cmd.comp(GS_LISP_CHANGELINETYPE) == 0     || // (changelinetype linetypename)
            cmd.comp(GS_LISP_CHANGEROTATION) == 0     || // (changerotation numrotation unit)
				cmd.comp(GS_LISP_CHANGESCALE) == 0        || // (changescale numscale)
				cmd.comp(GS_LISP_CHANGETEXTSTYLE) == 0    || // (changetextstyle textstylename)
				cmd.comp(GS_LISP_CHANGETHICKNESS) == 0    || // (changethickness numthickness)
            cmd.comp(GS_LISP_CHANGEWIDTH) == 0)          // (changewidth numwidth)
   {
      cmd.paste(gsc_LISP_ChangeEntityGraphProperty(screen, curr_fun, stream,
                                                   ptr, file_name, flag,
                                                   WhatIsGraphModified, TokenToSearch, QtyToken,
                                                   cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(GS_LISP_CHANGECOLORATTR) == 0 ||    // (changecolorattr nomeattr numcolor)
				cmd.comp(GS_LISP_CHANGELAYERATTR) == 0 ||    // (changelayerattr nomeattr layername)
				cmd.comp(GS_LISP_CHANGEHTEXTATTR) == 0 ||    // (changehtextattr nomeattr numhtext)
				cmd.comp(GS_LISP_CHANGETEXTSTYLEATTR) == 0)  // (changetextstyleattr nomeattr textstylename)
   {
      cmd.paste(gsc_LISP_ChangeAttrGraphProperty(screen, curr_fun, stream,
                                                 ptr, file_name, flag,
                                                 WhatIsGraphModified, TokenToSearch, QtyToken,
                                                 cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("CHR")) == 0)
   {
      cmd.paste(gsc_LISP_CHR(screen, curr_fun, stream,
                             ptr, file_name, flag,
                             WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("COND")) == 0) // (cond (test1 result1 ...) ...)
   {
      cmd.paste(gsc_LISP_COND(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("COORD_CONV")) == 0) // conversione coordinate di un punto
   {  // (coord_conv pt src dest)
      cmd.paste(gsc_LISP_COORD_CONV(screen, curr_fun, stream,
                                    ptr, file_name, flag,
                                    WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("COPYFILE")) == 0)
   {
      cmd.paste(gsc_LISP_COPYFILE(screen, curr_fun, stream,
                                  ptr, file_name, flag,
                                  WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("DATE")) == 0 ||   // (DATE) Data corrente nel formato corrente di GEOsim
            cmd.comp(_T("TIME")) == 0 ||   // (TIME) Ora corrente hh:mm:ss
            cmd.comp(_T("DATETIME")) == 0) // (DATETIME) Data e ora corrente hh:mm:ss
   {  
      cmd.paste(gsc_LISP_DATETIME(screen, curr_fun, stream,
                                  ptr, file_name, flag,
                                  WhatIsGraphModified, TokenToSearch, QtyToken,
                                  cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("DAY")) == 0)
   {   
      cmd.paste(gsc_LISP_DAY(screen, curr_fun, stream,
                             ptr, file_name, flag,
                             WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("DEFUN")) == 0)
   {
      cmd.paste(gsc_LISP_DEFUN(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("EQUAL")) == 0)
   {
      cmd.paste(gsc_LISP_EQUAL(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("EXPT")) == 0) // (expt base power)
   {
      cmd.paste(gsc_LISP_EXPT(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("GETGUID")) == 0)
   {   
      cmd.paste(gsc_LISP_GETGUID(screen, stream, ptr));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("GS_CURR_OP")) == 0) // codice operazione corrente di GEOsim
   {  // GS_CURRENT_OPERATION può assumere i seguenti valori:
      // NONE = 0, INSERT = 1, MODIFY = 2, ERASE = 3, INSERT_FIRST_DABLOCK = 4
      cmd.paste(gsc_LISP_GS_CURR_OP(screen, stream, ptr));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("GS_MEM")) == 0)
   {
      cmd.paste(gsc_LISP_GS_MEM(stream, ptr));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("IF")) == 0)
   {
      cmd.paste(gsc_LISP_IF(screen, curr_fun, stream,
                            ptr, file_name, flag,
                            WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("ITOA")) == 0)
   {
      cmd.paste(gsc_LISP_ITOA(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("LOOKUP")) == 0)     // (lookup connectionstr sqlstm)
   {
      cmd.paste(gsc_LISP_LOOKUP(screen, curr_fun, stream,
                                ptr, file_name, flag,
                                WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("LOAD")) == 0)     // (load file-name)
   {
      cmd.paste(gsc_LISP_LOAD(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("MAX")) == 0 || cmd.comp(_T("MIN")) == 0)
   {
      cmd.paste(gsc_LISP_MAX_MIN(screen, curr_fun, stream,
                                 ptr, file_name, flag,
                                 WhatIsGraphModified, TokenToSearch, QtyToken,
                                 cmd));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("MONTH")) == 0)
   {
      cmd.paste(gsc_LISP_MONTH(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(GS_LISP_MOVEATTR) == 0)
   {	// (moveattr nomeattr modalità [offset1[offset2[Flag_generaz
      // [allineam_oriz[allineam_vert[controllo_rotz[controllo_sovrapposiz]]]]]]])
      cmd.paste(gsc_LISP_MoveAttr(screen, curr_fun, stream,
                                  ptr, file_name, flag,
                                  WhatIsGraphModified, TokenToSearch, QtyToken));
	}
   // -------------------------------------------------------------------------
   else if(cmd.comp(GS_LISP_MOVESHEET) == 0)
   {	// (movesheet modalità [offset1[offset2[controllo_rotz[controllo_sovrapposiz]]]])
      // oppure
      // (movesheet X [Y [Z [rotz]]]])
      // entrambe le versioni devono avere lo stesso numero di parametri
      // e lo stesso numero di parametri obbligatori e opzionali
      // altrimenti i modi WhatIsGraphModified e TokenToSearch non vanno correttamente
      cmd.paste(gsc_LISP_MoveSheet(screen, curr_fun, stream,
                                   ptr, file_name, flag,
                                   WhatIsGraphModified, TokenToSearch, QtyToken));
	}
   // -------------------------------------------------------------------------
   else if(cmd.comp(GS_LISP_VISATTR) == 0)
   {	// (visattr nomeattr modalità)
      cmd.paste(gsc_LISP_VisAttrib(screen, curr_fun, stream,
                                   ptr, file_name, flag,
                                   WhatIsGraphModified, TokenToSearch, QtyToken));
	}
   // -------------------------------------------------------------------------
   else if(cmd.comp(GS_LISP_VISSHEET) == 0)
   {	// (vissheet modalità)
      cmd.paste(gsc_LISP_VisSheet(screen, curr_fun, stream,
                                  ptr, file_name, flag,
                                  WhatIsGraphModified, TokenToSearch, QtyToken));
	}
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("NOT")) == 0 ||
            cmd.comp(_T("NULL")) == 0)
   {
      cmd.paste(gsc_LISP_NOT_NULL(screen, curr_fun, stream,
                                  ptr, file_name, flag,
                                  WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("PROGN")) == 0)    // (progn [expr] ...)
   {
      cmd.paste(gsc_LISP_PROGN(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("RTOS")) == 0) // (rtos number [mode [precision]])
   {
      cmd.paste(gsc_LISP_RTOS(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("SETQ")) == 0)
   {
      cmd.paste(gsc_LISP_SETQ(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("STRCASE")) == 0)
   {
      cmd.paste(gsc_LISP_STRCASE(screen, curr_fun, stream,
                                 ptr, file_name, flag,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("STRCAT")) == 0)
   {
      cmd.paste(gsc_LISP_STRCAT(screen, curr_fun, stream,
                                ptr, file_name, flag,
                                WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("STRLEN")) == 0)
   {
      cmd.paste(gsc_LISP_STRLEN(screen, curr_fun, stream,
                                ptr, file_name, flag,
                                WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("STRTRAN")) == 0) // (strtran string what with)
   {
      cmd.paste(gsc_LISP_STRTRAN(screen, curr_fun, stream,
                                 ptr, file_name, flag,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("SUBSTR")) == 0) // (substr string start [length])  start è 1-indexed
   {
      cmd.paste(gsc_LISP_SUBSTR(screen, curr_fun, stream,
                                ptr, file_name, flag,
                                WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if(cmd.comp(_T("VER")) == 0)
   {
      cmd.paste(gsc_LISP_VER(screen, stream, ptr));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("WHILE")) == 0)
   {
      cmd.paste(gsc_LISP_WHILE(screen, curr_fun, stream,
                               ptr, file_name, flag,
                               WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("WHOAMI")) == 0)
   {
      cmd.paste(gsc_LISP_WHOAMI(screen, stream, ptr));
   }
   // -------------------------------------------------------------------------
   else if (cmd.comp(_T("YEAR")) == 0)
   {  
      cmd.paste(gsc_LISP_YEAR(screen, curr_fun, stream,
                              ptr, file_name, flag,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
   }
   else if (cmd.get_name() == NULL){}
   else if (isnumber(cmd.get_name())==TRUE)  // è un numero
   {  // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
      if (WhatIsGraphModified || TokenToSearch)
         cmd = _T("0");
   }
   else if (isstring(cmd.get_name())==TRUE)  // è una stringa
   {  // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
      if (WhatIsGraphModified || TokenToSearch)
         cmd = _T("nil");
   }
   else 
   {  
      if (cmd.comp(_T("NIL")) == 0) // se la presunta variabile o costante o funzione è NIL
         cmd = _T("nil");
      else
      {
         if (flag == 1) // se il token precedente era una '('
         { 
            C_VAR   *j;

            // verifico che sia una funzione...
            if ((j = is_var(GS_EMPTYSTR, cmd.get_name()))==NULL || j->ttype != _T('f')) 
               if (gsc_strlen(file_name) > 0)
               {  // provo a caricare la funzione dal file
                  C_STRING internal_cmd;

                  internal_cmd.paste(load_lisp(screen, stream, ptr, file_name, NULL,
                                      WhatIsGraphModified, TokenToSearch, QtyToken));
                  if (internal_cmd.get_name() == NULL) j = NULL;
                  else j = is_var(GS_EMPTYSTR, cmd.get_name());
               }
            
            if (j)  // eseguo questa funzione
            {
               cmd.paste(exec_proc(screen, cmd.get_name(), curr_fun, stream, ptr, file_name,
                                   WhatIsGraphModified, TokenToSearch, QtyToken));  // cerco funzione ed eseguo          
               return cmd.cut(); // era una funzione
            }
         }

         // verifico che sia una variabile o una costante
         // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
         if (WhatIsGraphModified || TokenToSearch)
            cmd = _T("0");
         else // modalità normale
            cmd.paste(find_var(screen,stream,ptr,curr_fun, cmd.get_name()) ); // cerco variabile
      }
   }
  
   return cmd.cut();
}


/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

TCHAR *reco(int screen, const TCHAR *stream, size_t *ptr)
{
  TCHAR chr_stmnt, *stmnt;
  int   i = 0,
        special_char=FALSE; /* flag per indicare se il carattere corrente e'
                               speciale nella stringa e quindi bisogna
                               continuare il ciclo */

  del_comment(stream, ptr);

  if ((stmnt = (TCHAR *) malloc(sizeof(TCHAR) * MAX_LEN_STMNT)) == NULL)
     return error(screen, stream, ptr, eGSOutOfMem, _T("Not enought memory"));
     
  if ((chr_stmnt = OK_string(stream[(int) *ptr])) == _T('\0'))
     { free(stmnt); return error(screen, stream, ptr, eGSInvLispFunc, _T("ERROR: Invalid function stream")); }

  if (chr_stmnt == _T('\"') && stream[(int) *ptr-1] != _T('\\'))
  {
     do
     {
        if (stream[(int) *ptr] == _T('\\') && !special_char) special_char = TRUE;
        else if(special_char) special_char = FALSE;

        if (i>=MAX_LEN_STMNT)
        { 
           free(stmnt);
           return error(screen, stream, ptr, eGSInvLispStmnt, _T("Variable name or statement too long"));
        }
        stmnt[i++] = stream[(int) (*ptr)++];
     }
     while(stream[(int) *ptr] != _T('\"') || (stream[(int) *ptr-1] == _T('\\') && special_char));
     stmnt[i++] = stream[(int) (*ptr)++];
   }
   else
     while ((chr_stmnt = OK_string(stream[(int) *ptr])) != _T('\'') &&
             chr_stmnt != _T('\"') && chr_stmnt!= _T('\0'))
     {
         if (i>=MAX_LEN_STMNT)
         { 
            free(stmnt);
            return error(screen, stream, ptr, eGSInvLispStmnt, _T("Variable name or statement too long"));
         }
         (*ptr)++;
         stmnt[i++] = chr_stmnt;
     }

   if (i==0)
   {
      free(stmnt);
      return error(screen, stream, ptr, eGSInvLispFunc, _T("ERROR: Invalid function stream"));
   }

   stmnt[i] = _T('\0');
 
   return stmnt;
}
/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

TCHAR OK_string(TCHAR lett)
{ 
  TCHAR *ret;
  TCHAR insieme_valido[] = _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\".1234567890_-+*/$=<>");

  // ROBY e le tonde ?
  ret = wcschr(insieme_valido, lett);

  if (ret==NULL) return _T('\0');
  else return *ret;
  
}
/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

void del_comment(const TCHAR *stream, size_t *ptr)
{
  while (isspace(stream[(int) *ptr])) (*ptr)++;
}   
/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

int isnumber(const TCHAR *num)
{
   size_t len, epos, i, point = 0;
  
   if (num == NULL) return FALSE;
  
   len = wcslen(num);
   if (len < 1) return FALSE;
  
   for (i = 0; i < len; i++)
      if (wcschr(_T("1234567890"), num[i]) == NULL) 
      {
         if (num[i] == _T('+') || num[i] == _T('-'))
         {
            if (i != 0) return FALSE;
         }
         else if (num[i] == _T('.'))
         {
            if (point == 0) point++;
            else return FALSE; 
         }
         else if (num[i] == _T('e') || num[i] == _T('E'))
         { // ESPONENTE
            if (i == 0) return FALSE;
            epos = i + 1; point = 0;
           
            for (i = epos; i < len; i++)
               if (wcschr(_T("1234567890"), num[i]) == NULL) 
               {
                  if (num[i] == _T('+') || num[i] == _T('-'))
                  {
                     if (i != epos) return FALSE;
                  }
                  else if (num[i] == _T('.'))
                  {
                     if (point==0) point++;
                     else return FALSE; 
                  }
                  else return FALSE;
               }
               break;
            } // END ESPONENTE
         else
            return FALSE;
      }

   return TRUE;
}


/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
int isstring(const TCHAR *num)
{
  size_t len;
  
  if (num==NULL) return FALSE;
  
  len = wcslen(num);
  if (len < 2) return FALSE;  // Devono esserci almeno due caratteri //
  if(num[0] == _T('\"') && num[len-1] == _T('\"')) return TRUE;

  return FALSE;
}


/*********************************************************/
/*.doc tostring                   <internal> */
/*+
  Questa funzione interpreta eventuali caratteri speciali e li
  sostituisce con il valore equivalente (es. "\\" -> "\"). Inoltre
  elimina il primo e l'ultimo carattere che devono essere virgolette. 
  Parametri
  TCHAR *string;     stringa da elaborare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione non sfonda perchè la stringa risultante sarà
       sicuramente più piccola dell'originale (modifica il parametro string)
-*/  
/*********************************************************/
TCHAR *tostring(TCHAR *string)
{
   C_STRING OctNum;
   size_t   len, i, j = 0;
   long     DecNum;
   
   if (!string) { GS_ERR_COD = eGSInvLispString; return NULL; }

   len  = wcslen(string) - 1;
   if (string[0] != _T('"') && string[len] != _T('"'))
      { GS_ERR_COD = eGSInvLispString; return NULL; }

   for (i = 1; i < len; i++)
   {
      switch (string[i])
      {
         case _T('\0'): // fine stringa
            break;
         case _T('\\'): // carattere speciale
            i++;
            switch (string[i])
            {
               case _T('\\'): // un solo slash
                  string[j++] = _T('\\');
                  break;
               case _T('"'):  // virgolette
                  string[j++] = _T('"');
                  break;
               case _T('t'):  // tab
                  string[j++] = _T('\t');
                  break;
               case _T('e'):  // esc
                  string[j++] = _T('\\');
                  break;
               case _T('n'):  // new line
                  string[j++] = _T('\n');
                  break;
               case _T('r'):  // return
                  string[j++] = _T('\r');
                  break;
               default:
               {
                  if (!iswdigit(string[i])) 
                     { GS_ERR_COD = eGSInvLispString; return NULL; }

                  OctNum.clear();
                  while (string[i] != _T('\0') && iswdigit(string[i]))
                     // continuo a leggere finchè ci sono numeri
                     OctNum += string[i++];
            
                  gsc_radix2decimal(OctNum.get_name(), 8, &DecNum);

                  TCHAR dummy = gsc_CharToUnicode((char) DecNum);
                  string[j++] = dummy;
               }
            }
            break;
         default:
            string[j++] = string[i];
            break;
      }
   }

   string[j] = _T('\0');
   
   return string;
}


/*+
-------------------------------------------------------------
TCHAR *tostring(const char *str)
     Riceve stringa in ingresso e ritorna un'altra stringa 
     (allocata internamente) traducendo i caratteri di controllo
	  interni del GEOlisp nel codice ASCII corrispondente
     Alloca un'altra stringa in uscita poiche' potrebbe 
     essere piu' lunga di quella in ingresso.
     Ritorna NULL in caso di errore.
-------------------------------------------------------------
*/
/*
TCHAR *tostring(TCHAR *str)
{
  int len, i, j=0;
  len = wcslen(str);

  if (str==NULL) 
//     return error(screen, stream, ptr, eGSInvLispString, _T("Invalid string"));
     { GS_ERR_COD = eGSInvLispString; return NULL; }
  
  if(str[0] == _T('\"') && str[(len-1)] == _T('\"'))
  {  
     len=(len-2);
     for(i=0;i<len;i++)
     {
        if(str[i+1] == _T('\\'))
        {
           if(str[i+2] == _T('e'))
           {
              str[j++] = 27;
           }
           else if(str[i+2] == _T('n'))
           {
              str[j++] = '\n';
           }
           else if(str[i+2] == _T('r'))
           {
              str[j++] = _T('\r');
           }
           else if(str[i+2] == _T('t'))
           {
              str[j++] = _T('\t');
           }
           else
           {
              str[j++] = str[i+2];  // if(str[i+1] == _T('\"'))
           }
           i++;
        }
        else
        {
           str[j++] = str[i+1];
        }
     }

     str[j] = _T('\0');

     return str;
  }
//  return error(screen, stream, ptr, eGSInvLispString, _T("Invalid string"));
  GS_ERR_COD=eGSInvLispString;
  return NULL;
}
*/


/*+
-------------------------------------------------------------
void error(screen, stream, ptr, int err,const TCHAR *what)

int err   : numero errore
const TCHAR *what: stringa errore
-------------------------------------------------------------
-*/
TCHAR *error(int screen, const TCHAR *stream, size_t *ptr, int err, const TCHAR *what)
{
  size_t i = 0, len;

  GS_ERR_COD=err;
  
  if (screen)
  {  
     acutPrintf(_T("\n\nError %d\n"), err);
     if (what != NULL) acutPrintf(_T("%s\n"), what);
     
     if (stream)
     {
        len = wcslen(stream);
        for(i = *ptr; i < len; i++)
        acutPrintf(_T("%c"), stream[i]);
        acutPrintf(GS_LFSTR);
     }
  }

  //gsc_error_msg();

  return NULL;
}

/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
int jump_fun(int screen, const TCHAR *stream, size_t *ptr)
{
  int    num_par=0;
  size_t len = wcslen(stream);

  del_comment(stream, ptr);

  // salto una stringa
  if (stream[(int) *ptr] == _T('\"'))
  {
     (*ptr)++;
     while((stream[(int) *ptr] != _T('\"')) || stream[(int) *ptr - 1] == _T('\\'))
     {
        if (*ptr > len)
        {
           error(screen, stream, ptr, eGSLispStreamErr, _T("Invalid End stream reached"));
           return GS_BAD;
        }
        (*ptr)++;
        }
     (*ptr)++;

     return GS_GOOD; // esco subito
  }
  else if(stream[(int) *ptr] == _T('('))
  {
     (*ptr)++;
     num_par++;
  }
  else if(stream[(int) *ptr] == _T(')')) return GS_GOOD;
  else
  {
     while (stream[(int) *ptr] != _T(')') && stream[(int) *ptr] != _T(' '))
     {
        if(stream[(int) *ptr] == _T('\"'))
        {
           (*ptr)++;
           while((stream[(int) *ptr] != _T('\"')) || stream[(int) *ptr - 1] == _T('\\'))
           {
              if (*ptr > len)
              {
                 error(screen, stream, ptr, eGSLispStreamErr, _T("Invalid End stream reached"));
                 return GS_BAD;
              }
              (*ptr)++;
           }
           (*ptr)++;
        }
        else
        {
           (*ptr)++;
        }
      }

      del_comment(stream, ptr);
  
      return GS_GOOD;
   }

   while (num_par > 0)
   {
      if (stream[(int) *ptr] == _T('\"'))
      {  // vado fino a fine stringa

         (*ptr)++;
         while (stream[(int) *ptr] != _T('\"'))
         {
            if (stream[(int) *ptr] == _T('\\')) // carattere di controllo
               (*ptr)++; // vado al carattere successivo (che sarà saltato)
            
            // vado al carattere successivo
            (*ptr)++;
            if (*ptr >= len)
            {
               error(screen, stream, ptr, eGSLispStreamErr, _T("Invalid End stream reached"));
               return GS_BAD;
            }
         }

         (*ptr)++;
      }
      if(stream[(int) *ptr] == _T('(')) num_par++;
      if(stream[(int) *ptr] == _T(')')) num_par--;
      (*ptr)++;
      if (*ptr > len)
      {
         error(screen, stream, ptr, eGSLispStreamErr, _T("Invalid End stream reached"));
         return GS_BAD;
      }
   }                                 
   del_comment(stream, ptr);
   return GS_GOOD;
}


/*********************************************************/
/*.doc num2str                                <internal> */
/*+
  Questa funzione converte un numero in stringa.
  Parametri:
  int ttype;      flag; se = 1 indica forma esponenziale
  double val1;    numero da convertire
  int sig;        numero di decimali

  Restituisce la stringa allocata dinamicamente in caso di successo,
  altrimenti stringa vuota. 
-*/  
/*********************************************************/
TCHAR *num2str(int ttype, double val1, int sig)
{
   TCHAR  fmt[5], s_sig[5], *StrNum, *p_sep;
   size_t pos;
   
   if ((StrNum = (TCHAR *) malloc(sizeof(TCHAR) * (sig + 30))) == NULL)
//      return error(screen, stream, ptr, eGSOutOfMem, _T("Not enought memory"));
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   wcscpy(fmt, _T("%."));
   gsc_itoa(sig, s_sig, 10);
   wcscat(fmt, s_sig);
   wcscat(fmt, (ttype == 1 ? _T("E") : _T("f")));
   if (sig > 0)
      swprintf(StrNum, sig + 30, fmt, val1);
   else
   {  // roby: il lisp tronca, non arrotonda
      double IntPart;
      modf(val1, &IntPart);
      swprintf(StrNum, sig + 30, fmt, IntPart);
   }

   pos = wcslen(StrNum) - 1;
   if ((p_sep = wcschr(StrNum, _T('.'))) != NULL) // ha dei decimali
   {
      while (&(StrNum[pos]) != p_sep && StrNum[pos] == _T('0')) StrNum[pos--] = _T('\0');

      if (StrNum[pos] == _T('.')) StrNum[pos] = _T('\0');
   }

   return StrNum;
}


/*+
-------------------------------------------------------------
TCHAR *stringto(const TCHAR *str)
     Riceve stringa in ingresso e ritorna un'altra stringa 
     (allocata internamente) traducendo i caratteri di controllo
     al formato \n o \t etc.
     Alloca un'altra stringa in uscita poiche' potrebbe 
     essere piu' lunga di quella in ingresso.
     Ritorna stringa vuota "" in caso di errore.
-----------------------------------------------------------
*/
TCHAR *stringto(const TCHAR *str)
{
   size_t   i, len = wcslen(str);
   C_STRING cmd1;
   
   cmd1 = _T("\"");
   
   for (i = 0; i < len; i++)
   {
      if (str[i] == _T('\033')) // ESC
      {
         cmd1 += _T("\\e");
      }
      else if(str[i] == _T('\n'))
      {
         cmd1 += _T("\\n");
      }
      else if(str[i] == _T('\r'))
      {
         cmd1 += _T("\\r");
      }
      else if(str[i] == _T('\t'))
      {
         cmd1 += _T("\\t");
      }
      else if(str[i] == _T('\"'))
      {
         cmd1 += _T("\\\"");
      }
      else if(str[i] == _T('\\'))
      {
         cmd1 += _T("\\\\");
      }
      else
         cmd1 += str[i];
   }

   cmd1 += _T("\"");

   return cmd1.cut();
}


//-----------------------------------------------------------------------//
//////////////////  C_VAR  INIZIO  ////////////////////////////////////////
//-----------------------------------------------------------------------//


// costruttore
C_VAR::C_VAR(const TCHAR *pr_na, const TCHAR *na) : C_NODE()
{
   ttype = 0; func_stream = NULL; value = NULL;

   if (pr_na != NULL)
   {
      proc_name = (TCHAR *) malloc(sizeof(TCHAR) * (wcslen(pr_na) + 1)); 
      wcscpy(proc_name, pr_na);
   }
   else proc_name=NULL;

   if (na!=NULL)
   {
      name = (TCHAR * )malloc(sizeof(TCHAR) * (wcslen(na) + 1));
      wcscpy(name, na);
   }
   else name=NULL;
}


// costruttore
C_VAR::C_VAR(C_STR *func_str, const TCHAR *va, const TCHAR *pr_na, const TCHAR *na, int ty) : C_NODE ()
{
   func_stream = func_str; ttype = ty;

   if (va!=NULL)
   {
      value = (TCHAR *) malloc(sizeof(TCHAR) * (wcslen(va) + 1));
      wcscpy(value, va);
   }
   else value=NULL;

   if (pr_na!=NULL)
   {
      proc_name=(TCHAR *) malloc(sizeof(TCHAR) * (wcslen(pr_na) + 1)); 
      wcscpy(proc_name, pr_na);
   }
   else proc_name=NULL;

   if (na!=NULL)
   {
      name = (TCHAR *) malloc(sizeof(TCHAR) * (wcslen(na) + 1));
      wcscpy(name,na);
   }
   else name=NULL;
}    


// distruttore
C_VAR::~C_VAR()
{
   if (value!=NULL) free(value);
   if (proc_name!=NULL) free(proc_name);
   if (name!=NULL) free(name);
}


int C_VAR::set(C_STR *func_str, const TCHAR *va,int ty)
{
   func_stream = func_str; ttype = ty;
 
   if (va != NULL)
   {
      if ((value = (TCHAR *) realloc(value, (wcslen(va) + 1) * sizeof(TCHAR))) == NULL)
         return GS_BAD;
      wcscpy(value, va);
   }
   else 
   {
      if (value != NULL) free(value); 
      value = NULL;
   }

   return GS_GOOD;
}


TCHAR* C_VAR::get_name() { return name; }


//-----------------------------------------------------------------------//
//////////////////   C_VAR   FINE  ////////////////////////////////////////
//-----------------------------------------------------------------------//


/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
C_VAR *is_var(const TCHAR *proc, const TCHAR *name)
{
   return Private_var.search_var(proc, name);
}

/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
int alloc_var(int screen, const TCHAR *stream, size_t *ptr, int pub, const TCHAR *proc,
              const TCHAR *name, const TCHAR *val, C_STR* func_str)
{
   C_VAR *curr_var = NULL;
   int   type;

   curr_var = is_var(proc, name);    // E' var locale
   if (curr_var == NULL && pub != _T('L'))
      if ((curr_var = is_var(GS_EMPTYSTR, name)) != NULL) // E' var globale o funzione
         if (curr_var->func_stream != NULL)   // E' funzione
         {
            Function_list.remove(curr_var->func_stream);
            Private_var.remove(curr_var);
            curr_var=NULL;
         } 

   if (curr_var == NULL)
   {  // Alloca nuova variabile
      curr_var = new C_VAR(proc,name);
      Private_var.add_tail((C_NODE*)curr_var);
   }

   // DEFINISCO IL TIPO
   if(func_str != NULL)
     type = _T('f');
   else if(val == NULL || val[0] == _T('\0'))
     type = _T('n');
   else if(isnumber(val))
     type = _T('n');
   else if(gsc_strcmp(val, _T("T")) == 0)
     type = _T('l');
   else if(gsc_strcmp(val, _T("nil")) == 0)
   {
      type = _T('l');
      /* se funzione ('f') si cancella in Function_list */
      Function_list.remove(curr_var->func_stream);
      // Non si può eliminare una variabile se questa viene messa a nil
      // Infatti se siamo in una funzione dove viene dichiarata una variabile locale che viene
      // messa a nil (1) poi viene messa a T (2), nel passaggio 1
      // viene rimossa dall'elenco delle variabili locali. Nel passaggio 2 viene ricreata ma
      // a livello globale !
      //Private_var.remove(curr_var);
      //return GS_GOOD;
   }
   else if(isstring(val))   
      type = _T('s');
   else
   {
      error(screen, stream, ptr, eGSInvLispVarType, _T("Invalid variable type"));
      return GS_BAD;
   }

   if (curr_var->set(func_str, val, type) == GS_BAD)
   {
      error(screen, stream, ptr, eGSOutOfMem, _T("Not enought memory"));
      return GS_BAD;
   }
   
   return GS_GOOD;
}                                      

/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

int release_var(const TCHAR *proc)
{
   C_VAR *punt;
   
   punt = (C_VAR *) Private_var.get_head();

   while (punt != NULL)
   {
      if(gsc_strcmp(punt->proc_name, proc) == 0 &&
         punt->ttype != _T('f'))
      {
         Private_var.remove(punt);
         punt = (C_VAR *) Private_var.get_cursor();
      }
      else punt = (C_VAR *) Private_var.get_next();
   }

   return GS_GOOD;
}
/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/

TCHAR *find_var(int screen, const TCHAR *stream, size_t *ptr, 
                const TCHAR *proc, const TCHAR *name)
{
   C_STRING val;
   C_VAR    *j;

   val.clear();

   // cerco fra le variabili locali
   j = is_var(proc, name);

   if (j != NULL && j->ttype != _T('f')) 
   {
      val = j->value;
      return val.cut();
   }
   else       //se non locale allora cerco fra le pubbliche
   {
      j = is_var(GS_EMPTYSTR/*"geomain"*/, name);
      if(j != NULL && j->ttype != _T('f')) 
      {
         val = j->value;
         return val.cut();
      }
   }

   // ritorno "nil" (roby)
   val = _T("nil");
   return val.cut();

//   val.set_name("FATAL ERROR Unknown Item : "); 
//   val.cat(name);
//   return error(screen, stream, ptr, eGSInvLispVar, val.get_name());
}


/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
TCHAR *exec_proc(int screen, const TCHAR *proc, const TCHAR *call_proc,
                 const TCHAR *stream, size_t *ptr, const TCHAR *file_name,
                 long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, cmd1;
   C_VAR    *j;
   size_t   end_f;
   size_t   f_ptr=0;
   TCHAR    *Function_stream;

   // cmd.set_name("nil");

   if ((j = is_var(GS_EMPTYSTR, proc)) == NULL || j->ttype != _T('f')) 
      if (gsc_strlen(file_name) > 0)
      {
         cmd.paste(load_lisp(screen, stream, ptr, file_name, NULL,
                             WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;
         j = is_var(GS_EMPTYSTR, proc);
      }

   if (j != NULL && j->ttype == _T('f'))
   {
      Function_stream = j->func_stream->get_name();
      end_f = wcslen(Function_stream);
      
      /* controllare che / abbia blank prima e dopo se esistono variabili */
      del_comment(Function_stream, &f_ptr);
      if (Function_stream[(int) f_ptr] == _T('('))
      {
         f_ptr++;
         del_comment(Function_stream, &f_ptr);
         while (Function_stream[(int) f_ptr] != _T(')') &&
                Function_stream[(int) f_ptr] != _T('/'))
         {
            cmd.paste(reco(screen, Function_stream, &f_ptr));  // nome argomento funzione
            if (cmd.get_name()==NULL) return NULL;

            del_comment(Function_stream, &f_ptr);
            cmd1.paste(what_is(screen, call_proc,stream,ptr, file_name, 0,
                       WhatIsGraphModified, TokenToSearch, QtyToken)); // valore argomento funzione
            if (cmd1.get_name()==NULL) return NULL;
            /* memorizzare */
            if (alloc_var(screen, stream, ptr, _T('L'), proc, cmd.get_name(), cmd1.get_name(), NULL) == GS_BAD)
            {
               release_var(proc);
               return NULL;
            }
            del_comment(Function_stream, &f_ptr);
         }
         if (Function_stream[(int) f_ptr] == _T('/')) f_ptr++;
         
         del_comment(Function_stream,&f_ptr);
         while(Function_stream[(int) f_ptr] != _T(')'))
         {
            cmd.paste(reco(screen,Function_stream, &f_ptr));  /* nome variabili private funzione */
            if (cmd.get_name()==NULL) return NULL;

            del_comment(Function_stream, &f_ptr);
            // memorizzare
            if (alloc_var(screen, stream, ptr, _T('L'), proc, cmd.get_name(), GS_EMPTYSTR, NULL) == GS_BAD)
            {
               release_var(proc);
               return NULL;
            }
         }
         f_ptr++;
      }
      else
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ( expected in function "));

      del_comment(Function_stream, &f_ptr);

      /* non e' detto che se non e' ( e' un errore */
      /*if(Function_stream[f_ptr] != _T('(')) error(screen, stream, ptr, eGSInvLispBracket, _T("Character ( expected in function "));*/

      cmd = _T("nil");

      while (f_ptr < end_f)
      {
         cmd.paste(what_is(screen, proc, Function_stream, &f_ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) 
         {
            release_var(proc);
            return NULL;
         }
         del_comment(Function_stream, &f_ptr);
      }
      release_var(proc);
      return cmd.cut();
   }

   cmd1 = _T("FATAL ERROR Unknown Function : ");
   cmd1.cat(proc);

   return error(screen, stream, ptr, eGSInvLispFunc, cmd1.get_name());
}


/*+
-------------------------------------------------------------
-------------------------------------------------------------
*/
C_STR *copy_function(int screen, const TCHAR *stream, size_t *ptr, long pt1, long pt2)
{
   C_STR *new_func;

   new_func = new C_STR;
   if (new_func != NULL) 
   { 
      if (new_func->set_name(stream, (int)pt1, (int)pt2) == GS_BAD)
         { delete new_func; new_func=NULL; }
      else Function_list.add_tail(new_func); 
   }
   
   if (new_func == NULL)
      error(screen, stream, ptr, eGSOutOfMem, _T("Not enought memory for functions"));
   
   return new_func;
}

/*+
-------------------------------------------------------------
load_lisp

legge file lisp di input, carica la variabile Curr_stream e trasforma
i caratteri in maiuscolo (non quelli fra "")
Esegue il file
-------------------------------------------------------------
-*/
TCHAR *load_lisp(int screen, const TCHAR *stream, size_t *ptr, const TCHAR *name_file,
                 const TCHAR *other_file, long *WhatIsGraphModified,
                 const TCHAR *TokenToSearch, int *QtyToken)
{
  FILE     *handle;
  int      i = 0, j = 0;
  size_t   t_chars = 0, end_strm = 0;
  long     filelen;
  C_STRING load_stream,result;
  TCHAR    *tmp_stream = NULL;
  C_STRING full_path;
  int      special_char = FALSE; /* flag per indicare se il carattere corrente e'
                                  speciale nella stringa e quindi bisogna
                                  continuare il ciclo */
  bool     Unicode;
  
  if (!name_file) return NULL;

  full_path = name_file;
  tostring(full_path.get_name());

  if ((handle = gsc_fopen(full_path, _T("r"), ONETEST, &Unicode)) == NULL)
  {
     full_path = _T("Error Opening File ");
     full_path += name_file;
     return error(screen, stream, ptr, eGSOpenFile, full_path.get_name());
  }
  filelen = gsc_filesize(handle);
  if((tmp_stream = (TCHAR *) calloc((size_t)(filelen+2), sizeof(TCHAR))) == NULL)
  {
     gsc_fclose(handle);
     return error(screen, stream, ptr, eGSOutOfMem, _T("Not enought memory"));
  }

  t_chars = gsc_fread(tmp_stream, (size_t) filelen, handle, Unicode);
  gsc_fclose(handle);
  tmp_stream[t_chars] = _T('\0');

  load_stream = tmp_stream;

  free(tmp_stream);   
  tmp_stream = load_stream.get_name();
  
  if (conv_stream(tmp_stream) == GS_BAD) return NULL;
  
  t_chars = wcslen(tmp_stream) - 1;

  /*******************************************************
  Si potrebbe anche non attaccare tmp_stream a Curr_stream, passare
  tmp_stream a what_is e spostare free(..) dopo il ciclo while;
  end_strm andrebbe inizializzato a 0.
  ********************************************************/

  while (end_strm < t_chars)  // da controllare
  {
     result.paste(what_is(screen, GS_EMPTYSTR/*"geomain"*/, tmp_stream, &end_strm, 
                          other_file, 0, WhatIsGraphModified,
                          TokenToSearch, QtyToken));
     if (result.get_name() == NULL) return NULL;
     del_comment(tmp_stream, &end_strm);
  }

  return result.cut();
}


/*+
-------------------------------------------------------------

int  conv_stream(TCHAR *tmp_stream)
    in una stringa toglie tutti i commenti  
    converte '\t' e '\n' in blank e toglie tutti gli spazi
    superflui, converte tutto in uppercase tutto cio' al di fuori 
    delle stringhe "..".
    Non fa nessun controllo e ritorna sempre GS_GOOD.
  
-------------------------------------------------------------
-*/
int conv_stream(TCHAR *tmp_stream)
{
   size_t i = 0, j = 0, len;

   if (tmp_stream == NULL) 
//      return error(screen, stream, ptr, eGSLispStreamErr, _T("Invalid input stream"=);
      { GS_ERR_COD = eGSLispStreamErr; return NULL; }

   len = wcslen(tmp_stream);
   
   while (tmp_stream[i] != _T('\0'))
   {
      switch (tmp_stream[i])
      {
         case _T(' ') :
         case _T('\t') : 
         case _T('\n') : 
            if (i>0 && tmp_stream[i-1] == _T(' ')) i++;
            else { tmp_stream[j++] = _T(' '); i++; }
            break;

         // STRINGHE
         case _T('\"') :
            do
            {
               tmp_stream[j++] = tmp_stream[i++]; 
               if ( tmp_stream[i-1] == _T('\\') && tmp_stream[i] != _T('\0')) 
                  { tmp_stream[j++] = tmp_stream[i++]; }
            }
            while( tmp_stream[i] != _T('\"') && tmp_stream[i] != _T('\0'));

            if (tmp_stream[i] == _T('\"'))  // Chiude virgolette
               { tmp_stream[j++] = _T('\"'); i++; }
            break;

         // NIL -> nil
         case _T('n') :
            // se esiste un carattere prima dell'ipotetico "nil" e
            // questo carattere è un carattere alfabetico
            if ( (i > 0 && OK_string(tmp_stream[i - 1]) != _T('\0')) ||
               // se esiste un carattere dopo l'ipotetico "nil" e
               // questo carattere è un carattere alfabetico
                 (i + 3 < len && OK_string(tmp_stream[i+3]) != _T('\0')) )
            {
               tmp_stream[i]= towupper(tmp_stream[i]);
               tmp_stream[j++]=tmp_stream[i++];
               break;
            }
                              
            if ( (len<i+2) || 
                 (tmp_stream[i+1] == _T('i') && tmp_stream[i+2] == _T('l')) )
            {
               tmp_stream[j++] = _T('n'); i++;
               tmp_stream[j++] = _T('i'); i++;
               tmp_stream[j++] = _T('l'); i++;
            }
            else
            {
               tmp_stream[i]   = towupper(tmp_stream[i]);   
               tmp_stream[j++] = tmp_stream[i++];
            }
            break;

         // COMMENTI 
         case _T(';') :
            if (tmp_stream[i+1] == _T('|'))
            {
               while( tmp_stream[i] != _T('\0') && 
                      (tmp_stream[i++] != _T('|') || tmp_stream[i] != _T(';')) );                          
                        
               if (tmp_stream[i] != _T('\0')) i++;
            }
            else
               while(tmp_stream[++i] != _T('\n') && tmp_stream[i] != _T('\0'));
            break;                              
                 
// ALTRI CARATTERI ///////////////////////////////////
         default :   
            tmp_stream[i]   = towupper(tmp_stream[i]);
            tmp_stream[j++] = tmp_stream[i++];
            break;
         }
      }                      // END WHILE //
   tmp_stream[j] = _T('\0');
   gsc_alltrim(tmp_stream);

   return GS_GOOD;      
}

void gsGslReload(void)
{
   GEOsimAppl::CMDLIST.StartCmd();
   if (gs_gsl_reload() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}
int gs_gsl_reload(void)
{
   if (gs_gsl_gs_reload() == GS_BAD) return RTERROR;
   if (gs_gsl_prj_reload() == GS_BAD) return RTERROR;

   return RTNORM;
}

void gsGslGsReload(void)
{
   GEOsimAppl::CMDLIST.StartCmd();
   if (gs_gsl_gs_reload() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}
int gs_gsl_gs_reload(void)
{
   if (gsc_load_gsl(GEOsimAppl::GEODIR.get_name()) == GS_BAD) return RTERROR;

   return RTNORM;
}

void gsGslPrjReload(void)
{
   GEOsimAppl::CMDLIST.StartCmd();
   if (gs_gsl_prj_reload() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}
int gs_gsl_prj_reload(void)
{
   if (GS_CURRENT_WRK_SESSION)
      { gsc_load_gsl(GS_CURRENT_WRK_SESSION->get_pPrj()->get_dir()); }
   return RTNORM;
}

int gsc_load_gsl(TCHAR *dove)
{
   C_STRING pathfile;

   // se esiste GS_LISP_FILE a livello di dove (GEOsimAppl::GEODIR, ...) lo carico
   pathfile = dove;
   pathfile += _T('\\');
   pathfile += GS_LISP_FILE;
   pathfile.strtran(_T("\\"), _T("/"));
                                                          
   if (gsc_path_exist(pathfile) == GS_GOOD)
   {
		C_STRING dummy;

      acutPrintf(_T("%s%s..."), gsc_msg(281), pathfile.get_name()); // "\nCaricamento file GEOsim Lisp "

		// la funzione load_lisp vuole la path del file come stringa tra "
		dummy = _T('"');
		dummy += pathfile;
		dummy += _T('"');
      if (load_lisp(FALSE, NULL, 0, dummy.get_name(), GS_EMPTYSTR) == NULL)
         acutPrintf(gsc_msg(282));   // "\nFallito.\n"
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getPntRtzOnObj
/*+                                                                       
  Calcola il punto e la rotazione relativo ad un oggetto grafico.

  Parametri:
  ads_name	ent;            nome entità 
  ads_point point;          punto calcolato                 (output)
  double		*angle;         angolo di rotazione in radianti (output)
  const TCHAR *mode;        se "S" (Start) il punto è relativo all'inizio dell'oggetto
                            se "E" (End) il punto è relativo alla fine dell'oggetto
                            se "M" (Middle) il punto è relativo alla punto medio 
                            dell'oggetto
                            se "MML" (Middle Max Length) il punto è relativo alla 
                            punto medio del tratto più lungo dell'oggetto
                            se "C" (Centroid) Centroide (per oggetti lineari chiusi) 
  double		offset1;        distanza dal punto scelto tramite il parametro <mode>
                            se <mode> = "E" offset è in direzione inversa.                           
  double		offset2;        distanza dall'oggetto (perpendicolarmente al tratto)
  int       check_rotz;     se GS_GOOD il testo non sarà mai capovolto (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_getPntRtzOnObj(ads_name ent, ads_point point, double *angle,
                       const TCHAR *mode, double offset1, double offset2, int check_rotz)
{
   C_RB_LIST EntList;
   C_STRING  UpperMode(mode);
   presbuf p;

   UpperMode.toupper();

   if ((EntList << acdbEntGet(ent)) == NULL)
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
   if ((p = EntList.SearchType(0)) == NULL || p->resval.rstring == NULL) 
      { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

   // Se si tratta di oggetti nodali verrà considerato solo il punto di inserimento.
   if (gsc_strcmp(p->resval.rstring, _T("TEXT")) == 0 ||
       gsc_strcmp(p->resval.rstring, _T("MTEXT")) == 0 ||
       gsc_strcmp(p->resval.rstring, _T("INSERT")) == 0 ||
       gsc_strcmp(p->resval.rstring, _T("CIRCLE")) == 0 ||
       gsc_strcmp(p->resval.rstring, _T("ELLIPSE")) == 0)
   {
      if (!(p = EntList.SearchType(10))) { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }
      ads_point_set(p->resval.rpoint, point);
      *angle = 0.0;
      if ((p = EntList.SearchType(50))) *angle = p->resval.rreal;
      ads_polar(point, *angle, offset1, point);
   }
   else
   {
      if (UpperMode.comp(_T("C")) == 0)
      { // modalità Centroid: centro dell'oggetto; rotazione = 0
         if (gsc_get_centroidpoint(ent, point) == GS_BAD) return GS_BAD;
         *angle = 0.0;
         check_rotz = GS_BAD;
      }
      else if (UpperMode.comp(_T("S")) == 0)
      { // modalità Start: inizio dell'oggetto
         if (gsc_getPtOnLinearObj(ent, offset1, point, angle) == GS_BAD) return GS_BAD;
      }
      else if (UpperMode.comp(_T("E")) == 0)
      { // modalità End: fine dell'oggetto
         C_REAL_LIST LenParts;
         C_REAL      *pLenPart;
         double      Len = 0;

         // Calcolo la lunghezza totale
         if (gsc_getLenParts(ent, LenParts) == GS_BAD) return GS_BAD;
         pLenPart = (C_REAL *) LenParts.get_head();
         while (pLenPart)
         {
            Len += pLenPart->get_key_double();
            pLenPart = (C_REAL *) pLenPart->get_next();
         }
         // offset1 è inteso nella nella direzione inversa (verso il punto iniziale)
         if (gsc_getPtOnLinearObj(ent, Len - offset1, point, angle) == GS_BAD) return GS_BAD;
      }
      else if (UpperMode.comp(_T("M")) == 0)
      { // modalità Middle: punto medio della polilinea        
         C_REAL_LIST LenParts;
         C_REAL      *pLenPart;
         double      Len = 0;

         // Calcolo la lunghezza totale
         if (gsc_getLenParts(ent, LenParts) == GS_BAD) return GS_BAD;
         pLenPart = (C_REAL *) LenParts.get_head();
         while (pLenPart)
         {
            Len += pLenPart->get_key_double();
            pLenPart = (C_REAL *) pLenPart->get_next();
         }
         if (gsc_getPtOnLinearObj(ent, Len / 2 + offset1, point, angle) == GS_BAD) return GS_BAD;
      }
      else if (UpperMode.comp(_T("MML")) == 0)
      { // modalità Middle Max Len: punto medio del tratto più lungo della polilinea
         C_REAL_LIST LenParts;
         C_REAL      *pLenPart;
         double      Len = 0;
         int         Pos = 1, i = 1;

         // Calcolo la lunghezza totale
         if (gsc_getLenParts(ent, LenParts) == GS_BAD) return GS_BAD;
         pLenPart = (C_REAL *) LenParts.get_head();
         while (pLenPart)
         {
            if (pLenPart->get_key_double() > Len)
            {
               Len = pLenPart->get_key_double();
               Pos = i; // posizione del tratto più lungo
            }
            i++;
            pLenPart = (C_REAL *) pLenPart->get_next();
         }

         Len      = 0;
         pLenPart = (C_REAL *) LenParts.get_head();
         for (i = 1; i < Pos; i++)
         {
            Len += pLenPart->get_key_double();
            pLenPart = (C_REAL *) pLenPart->get_next();
         }
         Len += (pLenPart->get_key_double() / 2);
    
         if (gsc_getPtOnLinearObj(ent, Len + offset1, point, angle) == GS_BAD) return GS_BAD;
      }
      else
      {
         GS_ERR_COD = eGSInvalidArg;
         return GS_BAD;
      }
   }
   
   if (check_rotz == GS_GOOD)
   {
      // calcolo punto con offset2
      if (*angle > gsc_grd2rad(90) && *angle <= gsc_grd2rad(270))
      {
         ads_polar(point, *angle - gsc_grd2rad(90), offset2, point);
         *angle = *angle - gsc_grd2rad(180);
      }
      else
         ads_polar(point, *angle + gsc_grd2rad(90), offset2, point);
   }
   else
      ads_polar(point, *angle + gsc_grd2rad(90), offset2, point);

   return GS_GOOD;
}
int gsc_getPntRtzOnObj(AcDbEntity *pEnt, ads_point point, double *angle,
                       const TCHAR *mode, double offset1, double offset2, int check_rotz)
{
   C_STRING UpperMode(mode);

   UpperMode.toupper();

   // Se si tratta di oggetti nodali verrà considerato solo il punto di inserimento.
   if (pEnt->isKindOf(AcDbText::desc()) ||
       pEnt->isKindOf(AcDbMText::desc()) ||
       pEnt->isKindOf(AcDbBlockReference::desc()) ||
       pEnt->isKindOf(AcDbCircle::desc()) ||
       pEnt->isKindOf(AcDbEllipse::desc()))
   {
      if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;

      *angle = 0.0;
      gsc_get_rotation(pEnt, angle);
      ads_polar(point, *angle, offset1, point);
   }
   else
   {
      if (UpperMode.comp(_T("C")) == 0)
      { // modalità Centroid: centro dell'oggetto; rotazione = 0
         if (gsc_get_centroidpoint(pEnt, point) == GS_BAD) return GS_BAD;
         *angle = 0.0;
         check_rotz = GS_BAD;
      }
      else if (UpperMode.comp(_T("S")) == 0)
      { // modalità Start: inizio dell'oggetto
         AcGePoint3d pt;
         if (gsc_getPtOnLinearObj(pEnt, offset1, pt, angle) == GS_BAD) return GS_BAD;
         point[X] = pt.x; point[Y] = pt.y; point[Z] = pt.z;
      }
      else if (UpperMode.comp(_T("E")) == 0)
      { // modalità End: fine dell'oggetto
         double      Len = gsc_getLength(pEnt); // Calcolo la lunghezza totale
         AcGePoint3d pt;

         // offset1 è inteso nella nella direzione inversa (verso il punto iniziale)
         if (gsc_getPtOnLinearObj(pEnt, Len - offset1, pt, angle) == GS_BAD) return GS_BAD;
         point[X] = pt.x; point[Y] = pt.y; point[Z] = pt.z;
      }
      else if (UpperMode.comp(_T("M")) == 0)
      { // modalità Middle: punto medio della polilinea        
         double      Len = gsc_getLength(pEnt); // Calcolo la lunghezza totale
         AcGePoint3d pt;

         if (gsc_getPtOnLinearObj(pEnt, Len / 2 + offset1, pt, angle) == GS_BAD) return GS_BAD;
         point[X] = pt.x; point[Y] = pt.y; point[Z] = pt.z;
      }
      else if (UpperMode.comp(_T("MML")) == 0)
      { // modalità Middle Max Len: punto medio del tratto più lungo della polilinea
         C_REAL_LIST LenParts;
         C_REAL      *pLenPart;
         double      Len = 0;
         int         Pos = 1, i = 1;
         AcGePoint3d pt;

         // Calcolo la lunghezza totale
         if (gsc_getLenParts(pEnt, LenParts) == GS_BAD) return GS_BAD;
         pLenPart = (C_REAL *) LenParts.get_head();
         while (pLenPart)
         {
            if (pLenPart->get_key_double() > Len)
            {
               Len = pLenPart->get_key_double();
               Pos = i; // posizione del tratto più lungo
            }
            i++;
            pLenPart = (C_REAL *) pLenPart->get_next();
         }

         Len      = 0;
         pLenPart = (C_REAL *) LenParts.get_head();
         for (i = 1; i < Pos; i++)
         {
            Len += pLenPart->get_key_double();
            pLenPart = (C_REAL *) pLenPart->get_next();
         }
         Len += (pLenPart->get_key_double() / 2);
    
         if (gsc_getPtOnLinearObj(pEnt, Len + offset1, pt, angle) == GS_BAD) return GS_BAD;
         point[X] = pt.x; point[Y] = pt.y; point[Z] = pt.z;
      }
      else
      {
         GS_ERR_COD = eGSInvalidArg;
         return GS_BAD;
      }
   }
   
   if (check_rotz == GS_GOOD)
   {
      // calcolo punto con offset2
      if (*angle > gsc_grd2rad(90) && *angle <= gsc_grd2rad(270))
      {
         ads_polar(point, *angle - gsc_grd2rad(90), offset2, point);
         *angle = *angle - gsc_grd2rad(180);
      }
      else
         ads_polar(point, *angle + gsc_grd2rad(90), offset2, point);
   }
   else
      ads_polar(point, *angle + gsc_grd2rad(90), offset2, point);

   return GS_GOOD;
}


/**********************************************************/
/*.doc gs_LoadFuncFromGraphList                <external> */
/*+
  Questa funzione LISP carica la lista ordinata dei nomi delle funzioni 
  per il calcolo di proprietà grafiche in formato 
  ((<func1> "") (<func2> "") ...)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_LoadFuncFromGraphList(void)
{
   C_RB_LIST   res;
   C_2STR_LIST FuncNameRemList;
   C_2STR      *pFuncNameRem;
   
   acedRetNil();

   for (long i = 0; i < VectorFromGraphGEOLispFunctionsLen(); i++)
   {
      if ((pFuncNameRem = new C_2STR(VectorFromGraphGEOLispFunctions[i].Name, GS_EMPTYSTR)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      FuncNameRemList.add_tail(pFuncNameRem);
   }

   if (FuncNameRemList.get_count() > 0)
   {
      FuncNameRemList.sort_name(TRUE, TRUE); // sensitive e ascending
      res << FuncNameRemList.to_rb();
      res.LspRetList();
   }

   return RTNORM;
}


/**********************************************************/
/*.doc gs_LoadFuncToGraphList                  <external> */
/*+
  Questa funzione LISP carica la lista ordinata dei nomi delle funzioni 
  per la modifica di proprietà grafiche in formato 
  ((<func1> <descr1>) (<func2> <descr2>) ...)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_LoadFuncToGraphList(void)
{
   C_RB_LIST   res;
   C_2STR_LIST FuncNameRemList;
   C_2STR      *pFuncNameRem;
   
   acedRetNil();

   for (long i = 0; i < VectorToGraphGEOLispFunctionsLen(); i++)
   {
      if ((pFuncNameRem = new C_2STR(VectorToGraphGEOLispFunctions[i].Descr, GS_EMPTYSTR)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      FuncNameRemList.add_tail(pFuncNameRem);
   }

   if (FuncNameRemList.get_count() > 0)
   {
      FuncNameRemList.sort_name(TRUE, TRUE); // sensitive e ascending
      res << FuncNameRemList.to_rb();
      res.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_LoadFuncNameRemList                <external> */
/*+
  Questa funzione carica la lista dei nomi delle funzioni contenute in un
  file lisp o GEOlisp. Saranno letti anche eventuali commenti che seguono 
  alla riga della definizione della funzione lisp
  Parametri:
  const TCHAR *Path;             Path del file lisp
  C_2STR_LIST &FuncNameRemList;  Lista dei nomi delle funzioni (con argomenti)
                                 e dei commenti. Ad esempio dato il codice lisp:
                                 (defun MyFun (param1 / dummy)
                                 ; commento 
                                 ; alla mia funzione
                                 Verra creato un elemento della lista contenente
                                 "(MyFun param1)" e "commento\nalla mia funzione"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_LoadFuncNameRemList(void)
{
   presbuf     arg = acedGetArgs();
   C_RB_LIST   res;
   C_2STR_LIST FuncNameRemList;
   
   acedRetNil();
   // path completa di un file lisp
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_LoadFuncNameRemList(arg->resval.rstring, FuncNameRemList) == GS_BAD)
      return RTERROR;

   if (FuncNameRemList.get_count() > 0)
   {
      res << FuncNameRemList.to_rb();
      res.LspRetList();
   }

   return RTNORM;
}
int gsc_LoadFuncNameRemList(const TCHAR *Path, C_2STR_LIST &FuncNameRemList)
{
   FILE     *file;
   int      Stop;
   size_t   ptr;
   C_STRING Row, Buffer, FuncName, FuncRem;
   TCHAR    *pToken;
   C_2STR   *pFuncNameRem;

   if (!Path) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if ((file = gsc_fopen(Path, _T("r"), ONETEST)) == NULL) return GS_BAD;
   FuncNameRemList.remove_all();

   while (gsc_readline(file, Row) == GS_GOOD)
   {
      if ((pToken = Row.get_name()) == NULL) continue;
      gsc_strsep(pToken, _T('\0'), _T(';')); // elimino i commenti
      ptr = 0;
      del_comment(pToken, &ptr);     // salto gli spazi

      // Se incomincia con "("
      if (pToken[ptr] == _T('('))
      {
         ptr++;
         del_comment(pToken, &ptr); // salto gli spazi
         if (gsc_strstr(pToken + ptr, _T("DEFUN"), FALSE) == pToken + ptr) // funzione lisp
         {
            ptr += (long) wcslen(_T("DEFUN"));
            del_comment(pToken, &ptr); // salto gli spazi
            Buffer = gsc_strtran(pToken + ptr, _T("("), _T(" ")); // elimino "("
            pToken = Buffer.get_name();
            ptr = 0;

            Stop = (gsc_strsep(pToken + ptr, _T('\0'), _T('/')) == 0) ? FALSE : TRUE;
            if (!Stop) Stop = (gsc_strsep(pToken + ptr, _T('\0'), _T(')')) == 0) ? FALSE : TRUE;

            FuncName = _T('(');
            FuncName += pToken + ptr;
            FuncName.alltrim();
            
            // leggo finchè trovo un "/" o una ")"
            while (!Stop && gsc_readline(file, Row) == GS_GOOD)
            {
               pToken = Row.get_name();
               gsc_strsep(pToken, _T('\0'), _T(';')); // elimino i commenti
               ptr = 0;
               del_comment(pToken, &ptr);     // salto gli spazi

               Stop = (gsc_strsep(pToken + ptr, _T('\0'), _T('/')) == 0) ? FALSE : TRUE;
               if (!Stop) Stop = (gsc_strsep(pToken + ptr, _T('\0'), _T(')')) == 0) ? FALSE : TRUE;

               FuncName += _T(' ');      
               FuncName += pToken + ptr;      
            }
            FuncName.alltrim();
            FuncName += _T(')');

            // leggo i commenti che seguono
            Stop = FALSE;
            FuncRem.clear();
            while (!Stop && gsc_readline(file, Row) == GS_GOOD)
            {
               Row.alltrim();
               if (Row.get_chr(0) == _T(';'))
               {
                  Buffer = Row.get_name() + 1;
                  Buffer.alltrim();
                  if (FuncRem.len() > 0) FuncRem += _T(' ');
                  FuncRem += Buffer.get_name();
               }
               else Stop = TRUE;
            }

            if ((pFuncNameRem = new C_2STR(FuncName.get_name(), FuncRem.get_name())) == NULL)
               { gsc_fclose(file); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            FuncNameRemList.add_tail(pFuncNameRem);
         }
      }
   }
   gsc_fclose(file);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_VAR_LIST   INIZIO      //////////////////////////////
//-----------------------------------------------------------------------//


C_VAR_LIST::C_VAR_LIST() : C_LIST() {}

C_VAR_LIST::~C_VAR_LIST() {}  // chiama ~C_LIST

C_VAR *C_VAR_LIST::search_var(const TCHAR *pr_na, const TCHAR *na)
{
   C_VAR *punt;
   punt = (C_VAR *) get_head();
   while (punt != NULL)
   {  // Confronto insensitive
      if (gsc_strcmp(punt->proc_name, pr_na, GS_BAD) == 0 && 
          gsc_strcmp(punt->name, na, GS_BAD) == 0) break;
      punt = (C_VAR *) get_next();
   }
   return punt;
}


//-----------------------------------------------------------------------//
//////////////////    C_VAR_LIST   FINE      //////////////////////////////
/////   FUNZIONI PER L'INTERPRETE LISP   -   INIZIO                   /////
//-----------------------------------------------------------------------//


/******************************************************************/
/*.doc gsc_LISP_MathOperator                              <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta l'operatore matematico +, -, *, /.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_MathOperator(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                             size_t *ptr, const TCHAR *file_name, int flag,
                             long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                             C_STRING &Operator)
{
   C_STRING cmd;
   double   val1 = 0.0, val2 = 0.0;
   size_t   stream_len = gsc_strlen(stream);

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
   {
   }
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         val1 = _wtof(cmd.get_name());
      else if(cmd.comp(_T(")")) == 0)
      {
         cmd = _T("0");
         return cmd.cut();
      }
      else return error(screen, stream, ptr,eGSLispNumExpected, _T("Number expected in operation function "));
   }

   do
   {
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
   
      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
      {
         if(cmd.comp(_T(")")) == 0) break;
         else del_comment(stream,ptr);
      }
      else // modalità normale
      {
         if (isnumber(cmd.get_name()))
         {
            val2 = _wtof(cmd.get_name());
   
            switch (Operator.get_chr(0))
            {
               case _T('+'):
                  val1 = val1 + val2;
                  break;
               case _T('-'):
                  val1 = val1 - val2;
                  break;
               case _T('/'):
                  if (val2 == 0)
                     return error(screen, stream, ptr, eGSLisDividebyZero, _T("Divide by zero"));
                  val1 = val1 / val2;
                  break;
               case _T('*'):
                  val1 = val1 * val2;
                  break;
               default:
                  return error(screen, stream, ptr, eGSInvLispArg, _T("FATAL ERROR "));
            }
            del_comment(stream,ptr);
         }
         else if(cmd.comp(_T(")")) == 0)
         {
            if (val2 == 0 && Operator.get_chr(0) == _T('-')) val1 = 0 - val1;
            break;
         }
         else
            return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in operation function "));
      }
   }
   while (stream[(int) *ptr] != _T(')') && *ptr <stream_len);

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      cmd.paste(num2str(2, val1, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_RelationalOperator                        <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta l'operatore relazionale 
  >, <, >=, <=, /=, =.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_RelationalOperator(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                   size_t *ptr, const TCHAR *file_name, int flag,
                                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                   C_STRING &Operator)
{
   C_STRING     cmd, cmd1;
   double       val1 = 0.0, val2 = 0.0;
   size_t       stream_len = gsc_strlen(stream);
   DataTypeEnum val1DataType; // adNumeric, adChar o adBoolean (per nil e T)

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
      {
         val1 = cmd.tof();
         val1DataType = adNumeric;
      }
      else if (isstring(cmd.get_name()))
      {
         cmd = tostring(cmd.get_name());
         val1DataType = adChar;
      }
      else if (cmd.comp(_T("nil")) == 0 || cmd.comp(_T("T")) == 0)
         val1DataType = adBoolean;
      else
         return error(screen, stream, ptr, eGSInvLispArg, _T("Char o number expected"));
   }

   del_comment(stream, ptr);

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
   {
      if (stream[(int) *ptr] == _T(')') || *ptr >= stream_len || stream[(int) *ptr] == _T('\0'))
      {
         // se modalità WhatIsGraphModified ritorno sempre valore "nil"
         cmd = _T("nil");
         return cmd.cut();
      }
   }
   else
   {
      if (stream[(int) *ptr] == _T(')') || *ptr >= stream_len || stream[(int) *ptr] == _T('\0'))
      {
         cmd = _T("T");
         return cmd.cut();
      }
   }
   
   do
   {
      cmd1.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                         WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd1.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (!WhatIsGraphModified && !TokenToSearch)
      {
         if (isnumber(cmd1.get_name()))
         {
            switch (val1DataType)
            {
               case adNumeric:
                  val2 = cmd1.tof();
                  if (Operator.comp(_T(">")) == 0)
                  {
                     if (val1 <= val2)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<")) == 0)
                  {
                     if (val1 >= val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T(">=")) == 0)
                  {
                     if (val1 < val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<=")) == 0)
                  {
                     if (val1 > val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("/=")) == 0)
                  {
                     if (val1 == val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("=")) == 0)
                  {
                     if (val1 != val2)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else
                     return error(screen, stream, ptr, eGSInvLispArg, _T("Relation Operator expected"));

                  break;

               case adChar:
                  return error(screen, stream, ptr, eGSInvLispArg, _T("Wrong argument type for comparison"));

               case adBoolean:
                  if (cmd.comp(_T("T")) == 0)
                     return error(screen, stream, ptr, eGSInvLispArg, _T("Wrong argument type for comparison"));
                  if (Operator.comp(_T("<")) == 0 || Operator.comp(_T("<=")) == 0 || Operator.comp(_T("=")) == 0)
                     { cmd = _T("nil"); return cmd.cut(); }
            }
         }
         else if (isstring(cmd1.get_name()) == TRUE)
         {
            cmd1 = tostring(cmd1.get_name());
   
            switch (val1DataType)
            {
               case adNumeric:
                  return error(screen, stream, ptr, eGSInvLispArg, _T("Wrong argument type for comparison"));
   
               case adChar:
                  if (Operator.comp(_T(">")) == 0)
                  {
                     if (cmd.comp(cmd1) <= 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<")) == 0)
                  {
                     if (cmd.comp(cmd1) >= 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T(">=")) == 0)
                  {
                     if (cmd.comp(cmd1) < 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<=")) == 0)
                  {
                     if (cmd.comp(cmd1) > 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if(Operator.comp(_T("/=")) == 0)
                  {
                     if (cmd.comp(cmd1) == 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if(Operator.comp(_T("=")) == 0)
                  {
                     if (cmd.comp(cmd1) != 0)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }

                  break;

               case adBoolean:
                  if (cmd.comp(_T("T")) == 0)
                     return error(screen, stream, ptr, eGSInvLispArg, _T("Wrong argument type for comparison"));
                  if (Operator.comp(_T("<")) == 0 || Operator.comp(_T("<=")) == 0 || Operator.comp(_T("=")) == 0)
                     { cmd = _T("nil"); return cmd.cut(); }
            }
         }
         else if (cmd1.comp(_T("nil")) == 0 || cmd1.comp(_T("T")) == 0)
         {
            switch (val1DataType)
            {
               case adNumeric:
               case adChar:
                  if (Operator.comp(_T(">")) == 0 || Operator.comp(_T(">=")) == 0)
                     { cmd = _T("nil"); return cmd.cut(); }
                  break;

               case adBoolean:
                  val1 = (cmd.comp(_T("T")) == 0) ? 1 : 0;
                  val2 = (cmd1.comp(_T("T")) == 0) ? 1 : 0;

                  if (Operator.comp(_T(">")) == 0)
                  {
                     if (val1 <= val2)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<")) == 0)
                  {
                     if (val1 >= val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T(">=")) == 0)
                  {
                     if (val1 < val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("<=")) == 0)
                  {
                     if (val1 > val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("/=")) == 0)
                  {
                     if (val1 == val2) 
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
                  else if (Operator.comp(_T("=")) == 0)
                  {
                     if (val1 != val2)
                        { cmd = _T("nil"); return cmd.cut(); }
                  }
            }
         }
         else
            return error(screen, stream, ptr, eGSInvLispArg, _T("FATAL ERROR "));
      }

      del_comment(stream, ptr);
   }
   while (stream[(int) *ptr] != _T(')') && *ptr < stream_len);

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
      cmd = _T("T");

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_AutoIncrement                             <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta l'operatore di autoincremente 1+ e 
  quello di autodecremento 1-.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_AutoIncrement(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                              size_t *ptr, const TCHAR *file_name, int flag,
                              long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                              C_STRING &Operator)
{
   C_STRING cmd;
   double   val1 = 0.0;

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         val1 = cmd.tof();
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("A number is required"));

      val1 = ((Operator.comp(_T("1+")) == 0) ? (val1 + 1) : (val1 - 1));

      cmd.paste(num2str(2, val1, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_ABS                                       <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione ABS.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_ABS(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   double   val1 = 0.0;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function ABS"));
   
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if(isnumber(cmd.get_name()))
         val1 = fabs(cmd.tof());
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function ABS"));
   
      cmd.paste(num2str(2, val1, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_Trim                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta le funzioni alltrim,
  rtrim, ltrim e trim.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_Trim(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                     C_STRING &Operator)
{
   C_STRING cmd;
   double   val1 = 0.0;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in TRIM function "));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));
   }

   if (Operator.comp(_T("ALLTRIM")) == 0)
      cmd.alltrim();
   else if (Operator.comp(_T("LTRIM")) == 0)
      cmd.ltrim();
   else if (Operator.comp(_T("RTRIM")) == 0 || Operator.comp(_T("TRIM")) == 0)
      cmd.rtrim();
   else
      return error(screen, stream, ptr, eGSInvLispArg, _T("FATAL ERROR"));

   cmd.paste(stringto(cmd.get_name()));
   if (cmd.get_name() == NULL) return NULL;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_AND_OR                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta gli operatori AND e OR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_AND_OR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                       C_STRING &Operator)
{
   C_STRING cmd;
   int      val1 = 1;    // parentesi aperta prima di AND
   int      val2 = 0;    // parentesi chiuse - per controllare fine AND
   size_t   stream_len = gsc_strlen(stream);

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))   // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered "));
      
   do
   {
      if (stream[(int) *ptr] == _T('('))
      {
         val1++;
         (*ptr)++;
      }
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
   
      // se modalità WhatIsGraphModified o TokenToSearch valuto tutte le condizioni
      if (cmd.comp(_T("nil")) == 0 && Operator.comp(_T("AND")) == 0 &&
          !WhatIsGraphModified && !TokenToSearch)
      {
         while (val1 != val2)
         {
            if (stream[(int) *ptr] == _T('(')) val1++;
            if (stream[(int) *ptr] == _T(')')) val2++;
            if (!(val1 == val2)) (*ptr)++;
            if (*ptr > stream_len)
               return error(screen, stream, ptr, eGSLispStreamErr, _T("End stream reached in function AND"));
         }
         cmd = _T("nil");
         return cmd.cut();
      }

      // se modalità WhatIsGraphModified o TokenToSearch valuto tutte le condizioni
      if (!(cmd.comp(_T("nil")) == 0) && Operator.comp(_T("OR")) == 0 &&
          !WhatIsGraphModified && !TokenToSearch)
      {
         while (val1 != val2)
         {
            if (stream[(int) *ptr] == _T('(')) val1++;
            if (stream[(int) *ptr] == _T(')')) val2++;
            if (!(val1 == val2)) (*ptr)++;
            if (*ptr > stream_len)
               return error(screen, stream, ptr, eGSLispStreamErr, _T("End stream reached in function OR"));
         }
         cmd = _T("T");
         return cmd.cut();
      }
   
      del_comment(stream, ptr);
      while (stream[(int) *ptr] == _T(')') && !(val1 == val2))
      {
         val2++;
         if(!(val1 == val2)) (*ptr)++;  // what_is deve sempre uscire con )
         del_comment(stream, ptr);
      }
   }
   while (!(val1 == val2));
   
   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      if (Operator.comp(_T("AND")) == 0)
         cmd = _T("T");
      else
         cmd = _T("nil");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_ASCII                                     <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione ASCII.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_ASCII(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   double   val1;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr,eGSInvLispBracket, _T("Character ) encountered in function ASCII"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
      {
         cmd = tostring(cmd.get_name());
         val1 = (cmd.get_chr(0) == _T('\\') ? (int) cmd.get_chr(1) : (int) cmd.get_chr(0));
      }
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Not a character in function ASCII"));

      cmd.paste(num2str(2, val1, 0));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_AT                                        <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione AT.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_AT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                   size_t *ptr, const TCHAR *file_name, int flag,
                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, str, str1;
   int      val1, val2, len;
   bool     found;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function AT."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function AT."));
   }

   str = cmd;
   
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
         str1 = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function AT."));
   }

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      val1 = 1;
   else
   {
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { val1 = 1; }
      else // modalità normale
      {
         if (isnumber(cmd.get_name()))
            val1 = cmd.toi();
         else
            return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function AT."));
   
         if (val1 < 1) val1 = 1;
         if ((size_t) val1 > str.len()) val1 = (int) str.len();
      }
   }
   
   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      val2 = (int) str.len() - val1 + 1;
   else
   {
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
   
      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if (isnumber(cmd.get_name()))
            val2 = cmd.toi();
         else
            return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function AT."));
   
         if (val2 < 1) val2 = 1;
         if ((size_t) val2 > str.len() - val1 + 1) val2 = (int) str.len() - val1 + 1;
      }
   }
   
   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      cmd = _T("0");
      if (str.len() > 0 && str1.len() > 0)
         for (int i = val1; (size_t) i < (val2 + val1 - str1.len() + 1); i++)
         {
            if (str.get_chr(i - 1) == str1.get_chr(0))
            {
               found = true;
               len   = (int) str1.len() - 1;
               for (int count = 0; count < len; count++)
               {
                  if (str.get_chr(i + count) != str1.get_chr(count + 1))
                  {
                     found = false;
                     break;
                  }
               }
               if (found)
               {
                  cmd = i;
                  break;
               }
            }
         }
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_AlphaToNumber                             <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione ATOF e ATOI.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_AlphaToNumber(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                              size_t *ptr, const TCHAR *file_name, int flag,
                              long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                              C_STRING &Operator)
{
   C_STRING cmd;
   double   val1;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered in function ATOF or ATOI"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      cmd.alltrim();
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));

      val1 = (Operator.comp(_T("ATOI")) == 0) ? cmd.toi() : cmd.tof();

      cmd.paste(num2str(2, val1, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_ChangeEntityGraphProperty                 <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_CHANGEBLOCK,
  GS_LISP_CHANGECOLOR, GS_LISP_CHANGEDYNBLOCKPROP, GS_LISP_CHANGEELEV,
  GS_LISP_CHANGEHATCH, GS_LISP_CHANGEHTEXT, GS_LISP_CHANGELAYER,
  GS_LISP_CHANGELINETYPE, GS_LISP_CHANGEROTATION, GS_LISP_CHANGESCALE,
  GS_LISP_CHANGETEXTSTYLE, GS_LISP_CHANGETHICKNESS, GS_LISP_CHANGEWIDTH.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_ChangeEntityGraphProperty(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                          size_t *ptr, const TCHAR *file_name, int flag,
                                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                          C_STRING &Operator)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function CHANGE..."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
   {
      if (WhatIsGraphModified)
      {
		   if (Operator.comp(GS_LISP_CHANGEBLOCK) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSBlockNameSetting; // blocco
		   else if (Operator.comp(GS_LISP_CHANGECOLOR) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSColorSetting; // colore
		   else if (Operator.comp(GS_LISP_CHANGEDYNBLOCKPROP) == 0)
         {
            *WhatIsGraphModified = *WhatIsGraphModified | GSBlockNameSetting; // proprietà dinamica blocco

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
            }

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il terzo parametro
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
            }
         }
		   else if (Operator.comp(GS_LISP_CHANGEELEV) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSElevationSetting; // elevazione
		   else if (Operator.comp(GS_LISP_CHANGEHATCH) == 0)
         {
            *WhatIsGraphModified = *WhatIsGraphModified | GSHatchNameSetting; // riempimento

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro (colore)
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
               if (cmd.comp(_T("nil"), FALSE) != 0)
                  *WhatIsGraphModified = *WhatIsGraphModified | GSHatchColorSetting; // colore riempimento

               del_comment(stream, ptr);
               if (stream[(int) *ptr] != _T(')')) // Se esiste anche il terzo parametro (scala)
               {
                  cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                    WhatIsGraphModified, TokenToSearch, QtyToken));
                  if (cmd.get_name() == NULL) return NULL;
                  if (cmd.comp(_T("nil"), FALSE) != 0)
                     *WhatIsGraphModified = *WhatIsGraphModified | GSHatchScaleSetting; // scala riempimento

                  del_comment(stream, ptr);
                  if (stream[(int) *ptr] != _T(')')) // Se esiste anche il quarto parametro (rotazione)
                  {
                     cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                       WhatIsGraphModified, TokenToSearch, QtyToken));
                     if (cmd.get_name() == NULL) return NULL;
                     if (cmd.comp(_T("nil"), FALSE) != 0)
                        *WhatIsGraphModified = *WhatIsGraphModified | GSHatchRotationSetting; // rotazione riempimento

                     del_comment(stream, ptr);
                     if (stream[(int) *ptr] != _T(')')) // Se esiste anche il quinto parametro (unita rotazione)
                     {
                        cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                          WhatIsGraphModified, TokenToSearch, QtyToken));
                        if (cmd.get_name() == NULL) return NULL;

                        del_comment(stream, ptr);
                        if (stream[(int) *ptr] != _T(')')) // Se esiste anche il quinto parametro (piano)
                        {
                           cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                             WhatIsGraphModified, TokenToSearch, QtyToken));
                           if (cmd.get_name() == NULL) return NULL;
                           if (cmd.comp(_T("nil"), FALSE) != 0)
                              *WhatIsGraphModified = *WhatIsGraphModified | GSHatchLayerSetting; // piano riempimento
                        }
                     }
                  }
               }
            }
         }
			else if (Operator.comp(GS_LISP_CHANGEHTEXT) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSTextHeightSetting;     // altezza testo
		   else if (Operator.comp(GS_LISP_CHANGELAYER) == 0)
         {
            *WhatIsGraphModified = *WhatIsGraphModified | GSLayerSetting; // piano

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
            }
         }
		   else if (Operator.comp(GS_LISP_CHANGELINETYPE) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSLineTypeSetting; // tipolinea 
		   else if (Operator.comp(GS_LISP_CHANGEROTATION) == 0)
         {
            *WhatIsGraphModified = *WhatIsGraphModified | GSRotationSetting; // rotazione

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
            }
         }
		   else if (Operator.comp(GS_LISP_CHANGESCALE) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSBlockScaleSetting; // scala
		   else if (Operator.comp(GS_LISP_CHANGETEXTSTYLE) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSTextStyleSetting; // stile testo
		   else if (Operator.comp(GS_LISP_CHANGETHICKNESS) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSThicknessSetting; // thickness
         else if (Operator.comp(GS_LISP_CHANGEWIDTH) == 0)
            *WhatIsGraphModified = *WhatIsGraphModified | GSWidthSetting; // larghezza polilinea
      }

      cmd = _T("0");
      if (cmd.get_name() == NULL) return NULL;
   }
   else // modalità normale
   {
      C_SELSET GraphObjs;
      C_FAS    FAS;
      long     FAS_flag;
      double   val1;

	   // recupero il gruppo di selezione degli oggetti grafici correnti
      if (GS_SELSET.copy(GraphObjs) == GS_BAD) return NULL;

		if (Operator.comp(GS_LISP_CHANGEBLOCK) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEBLOCK); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
		      gsc_strcpy(FAS.block, cmd.get_name(), MAX_LEN_BLOCKNAME);
            tostring(FAS.block);
		      FAS_flag = GSBlockNameSetting;		// flag per blocco
            cmd = FAS.block;
         }
		}
		else if (Operator.comp(GS_LISP_CHANGECOLOR) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGECOLOR); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1      = cmd.toi();
		      FAS.color.setAutoCADColorIndex((int) val1);
		      FAS_flag  = GSColorSetting; // flag per colore
            cmd.paste(num2str(2, val1, 0));
         }
		}
		else if (Operator.comp(GS_LISP_CHANGEDYNBLOCKPROP) == 0)
      {
         TCHAR    PropName[MAX_LEN_BLOCKNAME], PropValueStr[MAX_LEN_BLOCKNAME];
         double   PropValueNum;
         bool     PropValueBool, IsBool = false, IsChar = false, IsNum = false;

   	   // recupero il gruppo di selezione degli oggetti grafici correnti
         if (GS_SELSET.copy(GraphObjs) == GS_BAD) return NULL;

         if (!isstring(cmd.get_name()))
            return error(screen, stream, ptr, eGSLispStrExpected, _T("Property name expected in function CHANGEDYNBLOCKPROP."));

         gsc_strcpy(PropName, cmd.get_name(), MAX_LEN_BLOCKNAME);
         tostring(PropName);

         del_comment(stream, ptr);
         if (stream[(int) *ptr] == _T(')'))
            return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function CHANGEDYNBLOCKPROP."));

         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;

         if (cmd.comp(_T("T")) == 0 || cmd.comp(_T("nil")) == 0)
         {
            PropValueBool = (cmd.comp(_T("T")) == 0) ? true : false;
            IsBool = true;
         }
         else if (isstring(cmd.get_name()))
         {
            gsc_strcpy(PropValueStr, cmd.get_name(), MAX_LEN_BLOCKNAME);
            tostring(PropValueStr);
            IsChar = true;
         }
         else if (isnumber(cmd.get_name()))
         {
            PropValueNum = _wtof(cmd.get_name());
            IsNum = true;
         }

         if (stream[(int) *ptr] != _T(')')) // Se esiste anche il terzo parametro
         {
            cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
            if (cmd.get_name() == NULL) return NULL;
            if (isstring(cmd.get_name()))
            {
               cmd = tostring(cmd.get_name());

               if (gsc_strcmp(cmd.get_name(), _T("DABLOCK"), FALSE) == 0)
               { // scarto gli oggetti grafici principali
                  if (GraphObjs.intersectType(DA_BLOCK) == GS_BAD) return NULL;
               }
               else // scarto i blocchi DA
                  if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;
            }
            else
               return error(screen, stream, ptr, eGSInvLispBracket, _T("Third parameter not valid in function CHANGEDYNBLOCKPROP..."));
         }

         int      i = 0;
         ads_name ent;

         // ciclo gli oggetti grafici (solo i blocchi)
         while (GraphObjs.entname(i++, ent) == GS_GOOD)
            if (gsc_isblock(ent) == GS_GOOD)
               if (IsBool) gsc_setDynBlkProperty(ent, PropName, PropValueBool);
               else if (IsNum) gsc_setDynBlkProperty(ent, PropName, PropValueNum);
               else if (IsChar) gsc_setDynBlkProperty(ent, PropName, PropValueStr);
      
         cmd = _T("T");

         return cmd.cut();
		}
		else if (Operator.comp(GS_LISP_CHANGEELEV) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEELEV); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1          = cmd.tof();
 		      FAS.elevation = val1;
  		      FAS_flag      = GSElevationSetting;		// flag per elevazione
            cmd.paste(num2str(2, val1, LISP_PREC));

         }
		}
		else if (Operator.comp(GS_LISP_CHANGEHATCH) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
		      gsc_strcpy(FAS.hatch, cmd.get_name(), MAX_LEN_HATCHNAME);
            tostring(FAS.hatch);
		      FAS_flag = GSHatchNameSetting; // flag per riempimento
            cmd = FAS.hatch;

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro (colore)
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;

               if (cmd.comp(_T("nil"), FALSE) != 0) // se = nil è da saltare
                  if (!isnumber(cmd.get_name()))
                  {
                     acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
                     cmd = _T("nil");
                  }
                  else
                  {
                     val1            = cmd.toi();
                     FAS.hatch_color.setAutoCADColorIndex((int) val1);
                     FAS_flag        += GSHatchColorSetting; // flag per colore del riempimento
                     cmd.paste(num2str(2, val1, 0));
                  }

               del_comment(stream, ptr);
               if (stream[(int) *ptr] != _T(')')) // Se esiste anche il terzo parametro (scala)
               {
                  cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                    WhatIsGraphModified, TokenToSearch, QtyToken));
                  if (cmd.get_name() == NULL) return NULL;

                  if (cmd.comp(_T("nil"), FALSE) != 0) // se = nil è da saltare
                     if (!isnumber(cmd.get_name()))
                     {
                        acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
                        cmd = _T("nil");
                     }
                     else
                     {
                        val1            = cmd.tof();
                        FAS.hatch_scale = (int) val1;
                        FAS_flag        += GSHatchScaleSetting; // flag per la scala del riempimento
                        cmd.paste(num2str(2, val1, 0));
                     }

                  del_comment(stream, ptr);
                  if (stream[(int) *ptr] != _T(')')) // Se esiste anche il quarto parametro (rotazione)
                  {
                     cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                       WhatIsGraphModified, TokenToSearch, QtyToken));
                     if (cmd.get_name() == NULL) return NULL;

                     if (cmd.comp(_T("nil"), FALSE) != 0) // se = nil è da saltare
                        if (!isnumber(cmd.get_name()))
                        {
                           acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
                           cmd = _T("nil");
                        }
                        else
                        {
                           val1               = cmd.tof();
                           FAS.hatch_rotation = (int) val1;
                           FAS_flag           += GSHatchRotationSetting; // flag per la rotazione del riempimento
                           cmd.paste(num2str(2, val1, 0));
                        }

                     del_comment(stream, ptr);
                     if (stream[(int) *ptr] != _T(')')) // Se esiste anche il quinto parametro (unita rotazione)
                     {
                        cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                          WhatIsGraphModified, TokenToSearch, QtyToken));
                        if (cmd.get_name() == NULL) return NULL;

                        if (cmd.comp(_T("nil"), FALSE) != 0) // se = nil è da saltare
                           if (!isnumber(cmd.get_name()))
                           {
                              acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
                              cmd = _T("nil");
                           }
                           else
                           {
                              val1  = cmd.toi();
                              if (val1 != GSCounterClockwiseDegreeUnit)
                              {  // Converto in gradi senso antiorario
                                 C_DBGPH_INFO dummy;

                                 dummy.rotation_unit = (RotationMeasureUnitsEnum) (int) val1;
                                 FAS.hatch_rotation = dummy.ToCounterClockwiseDegreeUnit(FAS.hatch_rotation);
                              }
                           }

                        del_comment(stream, ptr);
                        if (stream[(int) *ptr] != _T(')')) // Se esiste anche il sesto parametro (piano)
                        {
                           cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                             WhatIsGraphModified, TokenToSearch, QtyToken));
                           if (cmd.get_name() == NULL) return NULL;

                           if (cmd.comp(_T("nil"), FALSE) != 0) // se = nil è da saltare
                              if (!isstring(cmd.get_name()))
                              {
                                 acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHATCH); // "\nAttenzione: parametro non valido per la funzione "
                                 cmd = _T("nil");
                              }
                              else
                              {
                                 gsc_strcpy(FAS.hatch_layer, cmd.get_name(), MAX_LEN_LAYERNAME);
                                 tostring(FAS.hatch_layer);
                                 FAS_flag += GSHatchLayerSetting; // flag per piano del riempimento
                                 cmd = FAS.hatch_layer;
                              }
                        }
                     }
                  }
               }
            }
         }
		}
		else if (Operator.comp(GS_LISP_CHANGEHTEXT) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHTEXT); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1       = cmd.tof();
 		      FAS.h_text = val1;
  		      FAS_flag   = GSTextHeightSetting; // flag per altezza testo
            cmd.paste(num2str(2, val1, LISP_PREC));
         }
		}
		else if (Operator.comp(GS_LISP_CHANGELAYER) == 0)
		{
         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGELAYER); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
		      gsc_strcpy(FAS.layer, cmd.get_name(), MAX_LEN_LAYERNAME);
            tostring(FAS.layer);
		      FAS_flag = GSLayerSetting;		// flag per piano
            cmd = FAS.layer;

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
            {
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
               if (isstring(cmd.get_name()))
               {
                  cmd = tostring(cmd.get_name());
                  if (gsc_strcmp(cmd.get_name(), _T("GRAPHICAL"), FALSE) == 0)
                  { // scarto i blocchi DA
                    if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;
                  }
                  else
                  if (gsc_strcmp(cmd.get_name(), _T("DABLOCK"), FALSE) == 0)
                  { // scarto gli oggetti grafici principali
                    if (GraphObjs.intersectType(DA_BLOCK) == GS_BAD) return NULL;
                  }
                  else
                  {
                     acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGELAYER); // "\nAttenzione: parametro non valido per la funzione "
                     cmd = _T("nil");
                  }
               }
               else
               {
                  acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGELAYER); // "\nAttenzione: parametro non valido per la funzione "
                  cmd = _T("nil");
               }
            }
         }
		}
		else if (Operator.comp(GS_LISP_CHANGELINETYPE) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGELINETYPE); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
		      gsc_strcpy(FAS.line, cmd.get_name(), MAX_LEN_LINETYPENAME);
            tostring(FAS.line);
		      FAS_flag = GSLineTypeSetting; // flag per tipolinea
            cmd = FAS.line;
         }
		}
		else if (Operator.comp(GS_LISP_CHANGEROTATION) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEROTATION); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1         = cmd.tof();
		      FAS.rotation = val1;
		      FAS_flag     = GSRotationSetting;		// flag per rotazione
            cmd.paste(num2str(2, val1, LISP_PREC));

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
            {
               // leggo l'unità di misura della rotazione
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;

               if (!isnumber(cmd.get_name()))
               {
                  acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEROTATION); // "\nAttenzione: parametro non valido per la funzione "
                  cmd = _T("nil");
               }
               else
               {
                  val1 = cmd.toi();
                  if (val1 != GSCounterClockwiseDegreeUnit)
                  {  // Converto in gradi senso antiorario
                     C_DBGPH_INFO dummy;

                     dummy.rotation_unit = (RotationMeasureUnitsEnum) (int) val1;
                     FAS.rotation = dummy.ToCounterClockwiseDegreeUnit(FAS.rotation);
                  }
               }
            }
         }
		}
		else if (Operator.comp(GS_LISP_CHANGESCALE) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGESCALE); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1           = cmd.tof();
		      FAS.line_scale = FAS.block_scale = val1;
            // flag per scala per blocco e tipolinea
		      FAS_flag  = GSBlockScaleSetting + GSLineTypeScaleSetting;
            cmd.paste(num2str(2, val1, LISP_PREC));
         }
		}
		else if (Operator.comp(GS_LISP_CHANGETEXTSTYLE) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGETEXTSTYLE); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
		      gsc_strcpy(FAS.style, cmd.get_name(), MAX_LEN_TEXTSTYLENAME);
            tostring(FAS.style);
		      FAS_flag = GSTextStyleSetting; // flag per stile testo
            cmd = FAS.style;
         }
		}
		else if (Operator.comp(GS_LISP_CHANGETHICKNESS) == 0)
		{
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGETHICKNESS); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1          = cmd.tof();
   		   FAS.thickness = val1;
   			FAS_flag      = GSWidthSetting; // flag per spessore
            cmd.paste(num2str(2, val1, LISP_PREC));
         }
		}
      else if (Operator.comp(GS_LISP_CHANGEWIDTH) == 0)
      {
	      // scarto i blocchi DA
         if (GraphObjs.intersectType(GRAPHICAL) == GS_BAD) return NULL;

         // anche se si è verificato un errore devo andare avanti nella valutazione
         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%sCHANGEWIDTH"), gsc_msg(329)); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            val1      = cmd.tof();
   	      FAS.width = val1;
   	      FAS_flag  = GSWidthSetting; // flag per larghezza
            cmd.paste(num2str(2, val1, LISP_PREC));
         }
		}

      if (cmd.comp(_T("nil")) != 0)
      {
         if (GS_CALC_CLASS && ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()) // se è stata setta la classe corrente da calcolare
            FAS.convertUnit(((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()->get_UnitCoordConvertionFactorFromClsToWrkSession());

         // applico la modifica della FAS
         if (gsc_modifyEntToFas(GraphObjs, &FAS, FAS_flag) == GS_BAD) return NULL;
      }
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_ChangeAttrGraphProperty                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_CHANGECOLORATTR,
  GS_LISP_CHANGELAYERATTR, GS_LISP_CHANGEHTEXTATTR, GS_LISP_CHANGETEXTSTYLEATTR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.
  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_ChangeAttrGraphProperty(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                                        size_t *ptr, const TCHAR *file_name, int flag,
                                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                                        C_STRING &Operator)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function CHANGE...ATTR."));

   // lettura nome attributo
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
   {
      del_comment(stream, ptr);
      if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
      {
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;
      }
      cmd = _T("0");
   }
   else // modalità normale
   {
      C_STRING       Attrib_name;
      C_SELSET       DABlocks;
      ads_name       ent;
      long           i = 0;
      C_ATTRIB_BLOCK DABlock;

      if(!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Attrib name expected in function CHANGE...ATTR."));
      Attrib_name = tostring(cmd.get_name());
      Attrib_name.toupper();

      del_comment(stream, ptr);
      if(stream[(int) *ptr] == _T(')'))
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function CHANGE...ATTR."));

      // lettura secondo parametro
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      // ricavo un gruppo di selezione per i blocchi DA
      if (GS_SELSET.copy(DABlocks) == GS_BAD || DABlocks.intersectType(DA_BLOCK) == GS_BAD)
         return NULL;

	   if (Operator.comp(GS_LISP_CHANGECOLORATTR) == 0)
      {
         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGECOLORATTR); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            C_COLOR color;

            color.setAutoCADColorIndex(cmd.toi());

            // ciclo i blocchi DA
            while (DABlocks.entname(i++, ent) == GS_GOOD)
            {
               DABlock.set_ent_name(ent);
               if (DABlock.set_color_attrib(color, Attrib_name.get_name()) == GS_BAD) return NULL;
            }
         }
      }
      else
	   if (Operator.comp(GS_LISP_CHANGEHTEXTATTR) == 0)
      {
         if (!isnumber(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGEHTEXTATTR); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            double hText = cmd.tof();

            if (GS_CALC_CLASS && ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()) // se è stata setta la classe corrente da calcolare
            {
               double UnitConvFactor = ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()->get_UnitCoordConvertionFactorFromClsToWrkSession();
               if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
                  hText = hText * UnitConvFactor;
            }

            // ciclo i blocchi DA
            while (DABlocks.entname(i++, ent) == GS_GOOD)
            {
               DABlock.set_ent_name(ent);
               if (DABlock.set_h_text_attrib(hText, Attrib_name.get_name()) == GS_BAD) return NULL;
            }
         }
      }
      else
	   if (Operator.comp(GS_LISP_CHANGELAYERATTR) == 0)
      {
         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGELAYERATTR); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            C_STRING Layer;

            Layer = tostring(cmd.get_name());

            // Verifico il layer e se non esiste lo creo.
            if (gsc_crea_layer(Layer.get_name()) == GS_BAD) return NULL;

            // ciclo i blocchi DA
            while (DABlocks.entname(i++, ent) == GS_GOOD)
            {
               DABlock.set_ent_name(ent);
               if (DABlock.set_layer_attrib(Layer.get_name(), Attrib_name.get_name()) == GS_BAD) return NULL;
            }
         }
      }
      else
	   if (Operator.comp(GS_LISP_CHANGETEXTSTYLEATTR) == 0)
      {
         if (!isstring(cmd.get_name()))
         {
            acutPrintf(_T("%s%s"), gsc_msg(329), GS_LISP_CHANGETEXTSTYLEATTR); // "\nAttenzione: parametro non valido per la funzione "
            cmd = _T("nil");
         }
         else
         {
            C_STRING TextStyle;

            TextStyle = tostring(cmd.get_name());

            // ciclo i blocchi DA
            while (DABlocks.entname(i++, ent) == GS_GOOD)
            {
               DABlock.set_ent_name(ent);
               if (DABlock.set_style_attrib(TextStyle.get_name(), Attrib_name.get_name()) == GS_BAD) return NULL;
            }
         }
      }

      cmd = _T("T");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_CHR                                       <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione CHR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.
  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_CHR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function CHR"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
      {
         TCHAR buf[2];

         buf[0] = cmd.toi();
         buf[1] = _T('\0');
         cmd.set_name(buf);
      }
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Not a number in function CHR"));

      cmd.paste(stringto(cmd.get_name()));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_COND                                       <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione COND.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.
  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_COND(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   int      isFunction;
   int      _count1;    // parentesi apert2 prima di condizione COND
   int      _count2;    // parentesi chiuse fine condizione COND
   int      count1 = 1;     // parentesi aperta prima di COND
   int      count2 = 0;     // parentesi chiuse - per controllare fine COND
   bool     sig = false;

   do
   {
      del_comment(stream, ptr);
      if (stream[(int) *ptr] != _T('('))
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ( expected in function COND.")); // prima CONDizione

      count1++; // parentesi aperta della condizione

      (*ptr)++;                            // sono su prima cond
      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')'))
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COND.\nLogical expression expected."));

      // valuta la condizione
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch devo entrare comunque
      // in tutti i casi del COND
      if (WhatIsGraphModified || TokenToSearch)
      {
         del_comment(stream, ptr);

         if (stream[(int) *ptr] == _T(')'))
            return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COND."));

         _count1 = 1;    // parentesi aperta prima di condizione COND
         _count2 = 0;    // parentesi chiuse - per controllare fine condizione COND

         do
         {
            if (stream[(int) *ptr] == _T('(')) // funzione
            {
               _count1++;
               (*ptr)++;
               isFunction = 1;
            }
            else // costante o variabile
               isFunction = 0;
            
            // valuta
            cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, isFunction,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
            if (cmd.get_name() == NULL) return NULL;

            del_comment(stream, ptr);
            while (stream[(int) *ptr] == _T(')') && !(_count1 == _count2))
            {
               _count2++;
               if (!(_count1 == _count2)) (*ptr)++;  // what_is deve sempre uscire con )
               del_comment(stream, ptr);
            }
         }
         while (!(_count1 == _count2));

         count2++; // parentesi chiusa della condizione
         (*ptr)++; // vado al token successivo
         del_comment(stream, ptr);
      }
      else
      {
         if (cmd.comp(_T("nil")) != 0 && sig == false)  // condizione TRUE (solo una volta)
         {
            del_comment(stream, ptr);

            if (stream[(int) *ptr] == _T(')'))
               return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COND."));

            _count1 = 1;    // parentesi aperta prima di condizione COND
            _count2 = 0;    // parentesi chiuse - per controllare fine condizione COND

            sig = true;

            do
            {
               if (stream[(int) *ptr] == _T('(')) // funzione
               {
                  _count1++;
                  (*ptr)++;
                  isFunction = 1;
               }
               else // costante o variabile
                  isFunction = 0;
               
               // valuta
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, isFunction,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name() == NULL) return NULL;
               
               del_comment(stream, ptr);
               while (stream[(int) *ptr] == _T(')') && _count1 != _count2)
               {
                  _count2++;
                  if (_count1 != _count2) (*ptr)++;  // what_is deve sempre uscire con )
                  del_comment(stream, ptr);
               }
            }
            while (_count1 != _count2);

            count2++; // parentesi chiusa della condizione
            (*ptr)++; // vado al token successivo
            del_comment(stream, ptr);

            // vado in fondo al COND saltando le altre condizioni
            while (count1 > count2)
            {
               if (jump_fun(screen, stream, ptr)==GS_BAD) return NULL;
               if (stream[(int) *ptr] == _T(')'))
						count2++; 
            }
				// Devo posizionarmi sull'ultima chiusa tonda del cond e non oltre
         }
         else
         {
            // salta la condizione
            while (count1 > (count2 + 1)) // deve uscire dal ciclo con una differenza tra le aperte e le chiuse = 1
            {
               if (jump_fun(screen, stream, ptr)==GS_BAD) return NULL;
               if (stream[(int) *ptr] == _T(')')) count2++;
            }
            (*ptr)++;
         }
      }
      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')')) count2++;
   }
   while (count1 > count2);

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_COORD_CONV                                <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione COORD_CONV.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.
  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_COORD_CONV(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                           size_t *ptr, const TCHAR *file_name, int flag,
                           long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   // (coord_conv pt src dest)
   C_STRING cmd;
   C_STRING  SrcCoord, DestCoord;
   ads_point pt;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COORD_CONV"));

   // lettura punto
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if(!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Point expected in function COORD_CONV."));
      if (gsc_Str2Pt(tostring(cmd.get_name()), pt) == GS_BAD)
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Point expected in function COORD_CONV."));
   }

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COORD_CONV"));

   // lettura sistema di coordinate sorgente
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Source coordinate system expected in function COORD_CONV."));
      SrcCoord = tostring(cmd.get_name());
      SrcCoord.toupper();
      if (ade_projsetsrc(SrcCoord.get_name()) != RTNORM)
         return error(screen, stream, ptr, eGSInvLispArg, _T("Source coordinate system not valid in function COORD_CONV."));
   }

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COORD_CONV"));

   // lettura sistema di coordinate sorgente
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      ads_point result;
      TCHAR     *dummy;

      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Destination coordinate system expected in function COORD_CONV."));
      DestCoord = tostring(cmd.get_name());
      DestCoord.toupper();
      if (ade_projsetdest(DestCoord.get_name()) != RTNORM)
         return error(screen, stream, ptr, eGSInvLispArg, _T("Destination coordinate system not valid in function COORD_CONV."));

      if (ade_projptforward(pt, result) != RTNORM)
         return error(screen, stream, ptr, eGSInvLispArg, _T("Coordinate system conversion not valid in function COORD_CONV."));

      dummy = gsc_tostring(result);
      cmd.paste(stringto(dummy));
      free(dummy);

      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_COPYFILE                                 <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione COORD_CONV.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.
  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_COPYFILE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   // (coord_conv pt src dest)
   C_STRING cmd, cmd1;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr,eGSInvLispBracket, _T("Character ) encountered in function COPYFILE"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function COPYFILE.\nSecond expression expected."));

   cmd1.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                      WhatIsGraphModified, TokenToSearch, QtyToken));    // expr2
   if (cmd1.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      if (isstring(cmd.get_name()) && isstring(cmd1.get_name()))
      {
         cmd = tostring(cmd.get_name());
         cmd1 = tostring(cmd1.get_name());
      }
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Not a character in function COPYFILE"));

      if (gsc_copyfile(cmd.get_name(), cmd1.get_name()) == GS_BAD) cmd = _T("nil");
      else cmd = _T("T");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_DATETIME                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione DATE, TIME e
  DATETIME.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_DATETIME(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                         C_STRING &Operator)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
   {
      C_STRING Msg;

      Msg = _T("Parameter not expected in function ");
      Msg += Operator;
      return error(screen, stream, ptr, eGSInvLispBracket, Msg.get_name());
   }

   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      C_STRING DateTime;

      if (Operator.comp(_T("DATE")) == 0)
         gsc_current_Date(DateTime); // data corrente
      else if (Operator.comp(_T("TIME")) == 0)
         gsc_current_Time(DateTime); // ora corrente
      else if (Operator.comp(_T("DATETIME")) == 0)
         gsc_current_DateTime(DateTime); // data/ora corrente

      cmd.paste(stringto(DateTime.get_name()));
   
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_DAY                                       <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione DAY.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_DAY(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                    size_t *ptr, const TCHAR *file_name, int flag,
                    long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered in function DAY"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      int val1;

      cmd.alltrim();
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));

      if ((val1 = gsc_getDayDateTime(cmd.get_name())) == -1)
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Date expected"));

      cmd.paste(num2str(2, val1, 0));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_DEFUN                                     <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione DEFUN.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_DEFUN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   int      count1, count2;
   size_t   stream_len = gsc_strlen(stream);
   C_STR    *func_str;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))   // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function DEFUN."));
   if (stream[(int) *ptr] == _T('('))   // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ( encountered in function DEFUN."));
   
   count1 = 1;    // parentesi aperta prima di DEFUN
   count2 = 0;    // parentesi chiuse - per controllare fine DEFUN
   
   operat.paste(reco(screen, stream, ptr));  // nome funzione
   if (operat.get_name() == NULL) return NULL;
   
   del_comment(stream, ptr);
   
   long val1 = (long) (*ptr);  // inizio funzione
   
   // controllare che / abbia blank prima e dopo se esistono variabili
   // inizio parte che non serve, ma controlla
   if (stream[(int) *ptr] == _T('('))
   {
      (*ptr)++;
      while(stream[(int) *ptr] != _T(')') && stream[(int) *ptr] != _T('/'))
      {
         cmd.paste(reco(screen,stream, ptr));  // nome argomento funzione
         if (cmd.get_name() == NULL) return NULL;
         del_comment(stream, ptr);
      }
      if (stream[(int) *ptr] == _T('/')) (*ptr)++;
      while (stream[(int) *ptr] != _T(')'))
      {
         cmd.paste(reco(screen,stream, ptr));  // nome variabili private funzione
         if (cmd.get_name() == NULL) return NULL;
   
         del_comment(stream, ptr);
      }
      (*ptr)++;
   }
   else
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ( expected in function DEFUN."));
   // fine parte che non serve, ma controlla
   
   while( count1 != count2 && *ptr < stream_len)
   {
      if(stream[(int) *ptr] == _T('\"') && stream[(int) *ptr-1] != _T('\\'))
      {
         do { (*ptr)++; }
         while(stream[(int) *ptr] != _T('\"') || stream[(int) *ptr-1] == _T('\\'));
         (*ptr)++;
      }
   
      if (stream[(int) *ptr] == _T('(')) count1++;
      if (stream[(int) *ptr] == _T(')')) count2++;
      (*ptr)++;
   }
   
   if(count1 != count2 /*&& *ptr <= stream_len*/)
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Unmatched parentesis in function DEFUN."));
   else (*ptr)--;  // Ritorna sull'ultima parentesi chiusa
   
   long val2 = (long) (*ptr-1);      // fine funzione
   
   // carico nuova funzione in Function_list
   func_str = copy_function(screen, stream, ptr, val1, val2); 
   
   if (alloc_var(screen, stream, ptr, _T('P'), GS_EMPTYSTR, operat.get_name(), GS_EMPTYSTR, func_str) == GS_BAD)
      { delete func_str; return NULL; }
   cmd = operat; // ritorno nome funzione definita

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_EQUAL                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione EQUAL.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_EQUAL(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, cmd1, operat;
   double   val3;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function EQUAL.\nFirst expression expected."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));     // expr1
   if (cmd.get_name() == NULL) return NULL;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function EQUAL.\nSecond expression expected."));

   cmd1.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                      WhatIsGraphModified, TokenToSearch, QtyToken));    // expr2
   if (cmd1.get_name() == NULL) return NULL;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
   {
      operat.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));  // fuzz
      if (operat.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
      { }
      else
      {
         if (isnumber(operat.get_name()))
            val3 = _wtof(operat.get_name());
         else
            return error(screen, stream, ptr, eGSLispNumExpected, _T("Fuzz must be a number in function EQUAL."));
      }
   }

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
   {
      cmd = _T("nil");
      return cmd.cut();
   }

   if (isstring(cmd.get_name()) && isstring(cmd1.get_name()))
   {
      cmd = tostring(cmd.get_name());
      cmd1 = tostring(cmd1.get_name());
      if (cmd.comp(cmd1.get_name()) == 0)
      {
         cmd = _T("T");
         return cmd.cut();
      }
      else
      {
         cmd = _T("nil");
         return cmd.cut();
      }
   }
   else if (isnumber(cmd.get_name()) && isnumber(cmd1.get_name()))
   {
      double val1, val2;

      val1 = cmd.tof();
      val2 = cmd1.tof();
      if (val3 == 0.0)
      {
         if (val1 == val2)        
         {
            cmd = _T("T");
            return cmd.cut();
         }
      }
      else                                      
      {
         if ((fabs(val1 - val2) + val3) >= val3)         
         {
            cmd = _T("T");
            return cmd.cut();
         }
      }
   }
   else
      return error(screen, stream, ptr, eGSInvLispArg, _T("Bad expressions in function EQUAL."));
           
   cmd = _T("nil");

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_EXPT                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione EXPT.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_EXPT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   double   val1, val2;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')')) // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function EXPT."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken)); // base
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (!(isnumber(cmd.get_name())))
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function EXPT."));
      val1 = cmd.tof();
   }

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (!(isnumber(cmd.get_name())))
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function EXPT."));
      val2 = cmd.tof();                          // power

      val1 = pow(val1, val2);

      cmd.paste(num2str(2, val1, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }
   
   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_GETGUID                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GETGUID.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_GETGUID(int screen, const TCHAR *stream, size_t *ptr)
{
   C_STRING      cmd, guid;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Parameter not expected in function GETGUID"));

   if (gsc_getGUID(guid) == GS_BAD) cmd = _T("nil");
   else cmd.paste(stringto(guid.get_name()));

   if (cmd.get_name() == NULL) return NULL;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_GS_CURR_OP                                <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_CURR_OP.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_GS_CURR_OP(int screen, const TCHAR *stream, size_t *ptr)
{
   C_STRING cmd;

   // GS_CURRENT_OPERATION può assumere i seguenti valori:
   // NONE = 0, INSERT = 1, MODIFY = 2, ERASE = 3, INSERT_FIRST_DABLOCK = 4
   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Parameter not expected in function GS_CURR_OP"));

   cmd.paste(num2str(2, (double) GS_CURRENT_OPERATION, 0));
   if (cmd.get_name() == NULL) return NULL;
   
   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_GS_MEM                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_MEM.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_GS_MEM(const TCHAR *stream, size_t *ptr)
{
   C_STRING cmd;
   C_VAR    *punt_var;

   del_comment(stream, ptr);
  
   punt_var = (C_VAR *) Private_var.get_head();
   while (punt_var)
   {
      if (punt_var->proc_name != NULL) acutPrintf(_T(" %s"),  punt_var->proc_name);
      if (punt_var->name != NULL)      acutPrintf(_T("\t%s"), punt_var->name);
      if (punt_var->value != NULL)     acutPrintf(_T("\t%s"), punt_var->value);
      acutPrintf(_T("\t%c"), punt_var->ttype);
      if (punt_var->func_stream != NULL) 
         acutPrintf(_T("\t\"%s\""), punt_var->func_stream->get_name());
      acutPrintf(GS_LFSTR);
     
      punt_var = (C_VAR *) Private_var.get_next();
   }
   cmd = _T("nil");
   
   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_IF                                        <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione IF.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_IF(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                   size_t *ptr, const TCHAR *file_name, int flag,
                   long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   
   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')')) 
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) not allowed in function IF."));

   // valuta la condizione
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')')) return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) not allowed in function IF."));

   // se modalità WhatIsGraphModified O TokenToSearch valuto IF e anche ELSE
   if (WhatIsGraphModified || TokenToSearch)
   {  // valuta il corpo relativo all'IF
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')')) 
         { cmd = _T("nil"); return cmd.cut(); }
      // valuta il corpo relativo all'ELSE
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
   }
   else
   { // valutazione normale
      if (cmd.comp(_T("nil")) != 0) // se la condizione è TRUE
      {  // valuta il corpo relativo all'IF
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;
         // salta il corpo relativo all'ELSE
         if (jump_fun(screen, stream, ptr) == GS_BAD) return NULL;
      }
      else
      {  // salta il corpo relativo all'IF
         if (jump_fun(screen, stream, ptr) == GS_BAD) return NULL;
         if (stream[(int) *ptr] == _T(')'))
            { cmd = _T("nil"); return cmd.cut(); }
         // valuta il corpo relativo all'ELSE
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;
      }
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_ITOA                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione ITOA.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_ITOA(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   
   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function ITOA"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      double val1;

      if (isnumber(cmd.get_name()))
         val1 = cmd.toi();   // non necessario ma per sicurezza
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Not a number in function ITOA"));

      cmd.paste(num2str(2, val1, 0));
      if (cmd.get_name() == NULL) return NULL;
   }

   cmd.paste(stringto(cmd.get_name()));
   if (cmd.get_name() == NULL) return NULL;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_LOOKUP                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione LOOKUP.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_LOOKUP(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   // (lookup connectionstr sqlstm)
   del_comment(stream, ptr);

   if (stream[(int) *ptr] == _T(')')) // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function LOOKUP."));

   // lettura stringa di connessione o file UDL
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore nil
   if (WhatIsGraphModified || TokenToSearch)
   {
      del_comment(stream, ptr);
      if (stream[(int) *ptr] != _T(')')) // Se esiste anche il secondo parametro
      {
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;
      }
      cmd = _T("nil");
   }
   else // modalità normale
   {
      C_STRING       ConnStrUDLFile, SQL;
      C_RB_LIST      ColValues;
      presbuf        p;
      C_DBCONNECTION *pConn;

      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Connection string expected in function LOOKUP."));
      ConnStrUDLFile = tostring(cmd.get_name());
      if (gsc_path_exist(ConnStrUDLFile, GS_BAD) == GS_GOOD)
         // traduco dir assoluto in dir relativo
         if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return NULL;
   
      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')'))
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function LOOKUP."));

      // lettura secondo parametro
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("SQL statement expected in function LOOKUP."));

      SQL = tostring(cmd.get_name());

      if ((pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name())) == NULL)
         return NULL;

      if (pConn->ReadRows(SQL, ColValues) == GS_BAD) return NULL;
      if ((p = ColValues.nth(0)) == NULL) // leggo solo la prima riga
         cmd = _T("nil");
      else
         if ((p = gsc_nth(0, p)) == NULL) // leggo solo la prima colonna
            cmd = _T("nil");
         else
            if ((p = gsc_nth(1, p)) == NULL)
               cmd = _T("nil");
            else
            {
               if (p->restype == RTSTR)
                  cmd = p->resval.rstring;
               else
               {
                  double val;

                  gsc_rb2Dbl(p, &val);
                  cmd.paste(num2str(2, val, LISP_PREC));
               }

               cmd.paste(stringto(cmd.get_name()));
               if (cmd.get_name() == NULL) return NULL;
            }
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_LOAD                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione LOAD.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_LOAD(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);

   if (stream[(int) *ptr] == _T(')')) // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function LOAD."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));  // nome file lisp
   if (cmd.get_name() == NULL) return NULL;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) expected in function LOAD."));

   // se modalità WhatIsGraphModified o TokenToSearch allora è VIETATO l'uso della funzione LOAD !
   if (WhatIsGraphModified || TokenToSearch)
   {
      cmd.clear();
      return NULL;
   }
   else // modalità normale
   {
      // carico ed eseguo file lisp
      cmd.paste(load_lisp(screen, stream, ptr, cmd.get_name(),file_name,
                          WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_MAX_MIN                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione MAX e MIN.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)
  C_STRING &Operator;        Operatore

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_MAX_MIN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken,
                        C_STRING &Operator)
{
   C_STRING cmd;
   int      count1 = 1;    // parentesi aperta prima di MAX/MIN
   int      count2 = 0;    // parentesi chiuse - per controllare fine MAX/MIN
   int      i = 0;
   double   val1, val2;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')')) // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered "));

   do
   {
      if (stream[(int) *ptr] == _T('('))
      {
         count1++;
         (*ptr)++;
      }
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      i++;        // conta valori funzione max o min

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if (!(isnumber(cmd.get_name()))) 
            return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected "));

         val2 = _wtof(cmd.get_name());
         if (i > 1)
         {
            val2 = (Operator.comp(_T("MAX")) == 0 ? max(val1, val2) : min(val1, val2));
         }
      }
      del_comment(stream, ptr);
      while (stream[(int) *ptr] == _T(')') && !(count1 == count2))
      {
         count2++;
         if (!(count1 == count2)) (*ptr)++;  // what_is deve sempre uscire con )
         del_comment(stream, ptr);
      }
      val1 = val2;
   }
   while (!(count1 == count2));

   if (i < 2)
      return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected"));

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      cmd.paste(num2str(2, val2, LISP_PREC));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_MONTH                                     <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione MONTH.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_MONTH(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered in function MONTH"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      int val1;

      cmd.alltrim();
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));

      if ((val1 = gsc_getMonthDateTime(cmd.get_name())) == -1)
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Date expected"));

      cmd.paste(num2str(2, val1, 0));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_MoveAttr                                  <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_MOVEATTR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_MoveAttr(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
 	// (moveattr nomeattr modalità [offset1[offset2[Flag_generaz
   // [allineam_oriz[allineam_vert[controllo_rotz[controllo_sovrapposiz]]]]]]])
   C_STRING  cmd, Attrib_name, Mode, NoOverlapMode;
   ads_point p_ins;
   short     GenFlag = 0, HorzAlign = 0, VertAlign = 0;
   int       RotCheck = GS_GOOD;
   double    AbsRotation = 9999;
   double    val1, val2;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function MOVEATTR."));

   // lettura nome attributo
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if(!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Attrib name expected in function MOVEATTR."));
      Attrib_name = tostring(cmd.get_name());
      gsc_toupper(Attrib_name.get_name());
   }

   // lettura modalità di calcolo
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
      {
         Mode = tostring(cmd.get_name());
         gsc_toupper(Mode.get_name());
      }
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Mode expected in function MOVEATTR."));
   }

   val1 = val2 = 0.0; // valori di default

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
   {
      // lettura offset1
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name()==NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if(isnumber(cmd.get_name()))
            val1 = cmd.tof();
         else
            return error(screen, stream, ptr, eGSLispNumExpected, _T("First offset expected in function MOVEATTR"));
      }

      del_comment(stream, ptr);
      if (stream[(int) *ptr] != _T(')'))
      {
         // lettura offset2
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (isnumber(cmd.get_name()))
               val2 = cmd.tof();
            else
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Second offset expected in function MOVEATTR"));
         }

         del_comment(stream, ptr);
         if (stream[(int) *ptr] != _T(')'))
         {
            // lettura GenFlag
            cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                              WhatIsGraphModified, TokenToSearch, QtyToken));
            if (cmd.get_name()==NULL) return NULL;

            // se modalità WhatIsGraphModified o TokenToSearch non valuto
            if (WhatIsGraphModified || TokenToSearch)
               { }
            else // modalità normale
            {
               if (isnumber(cmd.get_name()))
                  GenFlag = _wtoi(cmd.get_name());
               else
                  return error(screen, stream, ptr, eGSLispNumExpected, _T("GenFlag expected in function MOVEATTR"));
            }

            del_comment(stream, ptr);
            if (stream[(int) *ptr] != _T(')'))
            {
               // lettura HorzAlign
               cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                 WhatIsGraphModified, TokenToSearch, QtyToken));
               if (cmd.get_name()==NULL) return NULL;

               // se modalità WhatIsGraphModified o TokenToSearch non valuto
               if (WhatIsGraphModified || TokenToSearch)
                  { }
               else // modalità normale
               {
                  if (isnumber(cmd.get_name()))
                     HorzAlign = _wtoi(cmd.get_name());
                  else
                     return error(screen, stream, ptr, eGSLispNumExpected, _T("HorzAlign expected in function MOVEATTR"));
               }

               del_comment(stream, ptr);
               if (stream[(int) *ptr] != _T(')'))
               {
                  // lettura VertAlign
                  cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                    WhatIsGraphModified, TokenToSearch, QtyToken));
                  if (cmd.get_name()==NULL) return NULL;

                  // se modalità WhatIsGraphModified o TokenToSearch non valuto
                  if (WhatIsGraphModified || TokenToSearch)
                     { }
                  else // modalità normale
                  {
                     if (isnumber(cmd.get_name()))
                        VertAlign = _wtoi(cmd.get_name());
                     else
                        return error(screen, stream, ptr, eGSLispNumExpected, _T("VertAlign expected in function MOVEATTR"));
                  }

                  del_comment(stream, ptr);
                  if (stream[(int) *ptr] != _T(')'))
                  {
                     // lettura controllo_rotz
                     cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                       WhatIsGraphModified, TokenToSearch, QtyToken));
                     if (cmd.get_name()==NULL) return NULL;

                     // se modalità WhatIsGraphModified o TokenToSearch non valuto
                     if (WhatIsGraphModified || TokenToSearch)
                        { }
                     else // modalità normale
                     {
                        if (cmd.comp(_T("T")) == 0 || cmd.comp(_T("nil")) == 0)
                           RotCheck = (cmd.comp(_T("T")) == 0) ? GS_GOOD : GS_BAD;
                        else
                        if (isnumber(cmd.get_name())) // rotazione assoluta in gradi
                        {
                           AbsRotation = gsc_grd2rad(_wtof(cmd.get_name()));
                           RotCheck = GS_BAD;
                        }
                        else
                           return error(screen, stream, ptr, eGSLispNumExpected, _T("RotCheck expected in function MOVEATTR"));
                     }

                     // lettura "controllo sovrapposizione" (opzionale)
                     del_comment(stream, ptr);
                     if (stream[(int) *ptr] != _T(')'))
                     {
                        // lettura controllo_sovrapposiz
                        cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                                          WhatIsGraphModified, TokenToSearch, QtyToken));
                        if (cmd.get_name()==NULL) return NULL;

                        // se modalità WhatIsGraphModified o TokenToSearch non valuto
                        if (WhatIsGraphModified || TokenToSearch)
                           { }
                        else // modalità normale
                        {
                           if (isstring(cmd.get_name()))
                           {
                              NoOverlapMode = tostring(cmd.get_name());
                              NoOverlapMode.alltrim();
                              NoOverlapMode.toupper();
                           }
                           else
                              return error(screen, stream, ptr, eGSLispStrExpected, _T("Overlap control flag expected in function MOVESHEET."));
                        }
                     }
                  }
               }
            }
         }
      }
   }
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      C_SELSET   GraphObjs, DABlocks;
      ads_name   Obj, DABlock;
      C_STR_LIST attrib_name_list;
      ads_real   rotaz;

      // ricavo un gruppo di selezione per gli oggetti grafici e uno per i blocchi DA
      if (GS_SELSET.copy(GraphObjs) == GS_BAD || GraphObjs.intersectType(GRAPHICAL) == GS_BAD)
         return NULL;
      if (GS_SELSET.copy(DABlocks) == GS_BAD || DABlocks.intersectType(DA_BLOCK) == GS_BAD)
         return NULL;

      // ricavo solo il primo oggetto grafico !
      if (GraphObjs.entname(0, Obj) == GS_BAD) return NULL;

      if (GS_CALC_CLASS && ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()) // se è stata setta la classe corrente da calcolare
      {
         double UnitConvFactor = ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()->get_UnitCoordConvertionFactorFromClsToWrkSession();
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
         {
            val1 = val1 * UnitConvFactor;
            val2 = val2 * UnitConvFactor;
         }
      }

      // calcola posizione e rotazione
      if (gsc_getPntRtzOnObj(Obj, p_ins, &rotaz, Mode.get_name(), 
                             val1, val2, RotCheck) == GS_BAD)
         return NULL;

      // Se era stata specificata una rotazione assoluta dell'attributo
      if (AbsRotation != 9999) rotaz = AbsRotation;

      cmd = _T("T");

      // ricavo solo il primo blocco DA !
      if (DABlocks.entname(0, DABlock) == GS_GOOD)
      { // solo se esiste almeno un blocco DA
         C_ATTRIB_BLOCK Blk(DABlock);

         // cambio la modalità di inserimento
         if (Blk.set_ins_mode_attrib(Attrib_name.get_name(), &GenFlag,
                                     &HorzAlign, &VertAlign) == GS_BAD)
            cmd = _T("nil");
         else // sposto attributo
            if (Blk.move_attrib(Attrib_name.get_name(), p_ins, &rotaz) == GS_BAD)
               cmd = _T("nil");
            else
            if (NoOverlapMode.len() > 0)
            {  // cerco il punto dove l'attributo non si sovrapponga ad altri oggetti grafici
               ads_name  Attrib;
               ads_point RefPt, NewPt;

               if (gsc_getAttributeFromBlock(DABlock, Attrib_name.get_name(), Attrib) != GS_GOOD)
                     cmd = _T("nil");
               else
                  if (gsc_getPtNoOverlappedEnt(Attrib, NoOverlapMode.get_name(), RefPt, NewPt) == GS_GOOD &&
                      !ads_2Dpoint_equal(RefPt, NewPt))
                  {
                     p_ins[X] += (NewPt[X] - RefPt[X]);
                     p_ins[Y] += (NewPt[Y] - RefPt[Y]);
                     if (Blk.move_attrib(Attrib_name.get_name(), p_ins, &rotaz) == GS_BAD)
                        cmd = _T("nil");
                  }
            }
      }

      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}



/******************************************************************/
/*.doc gsc_LISP_MoveSheet                                 <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_MOVESHEET.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_MoveSheet(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                          size_t *ptr, const TCHAR *file_name, int flag,
                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   // (movesheet modalità [offset1[offset2[controllo_rotz[controllo_sovrapposiz]]]])
   // oppure
   // (movesheet X [Y [Z [rotz]]]])
   // entrambe le versioni devono avere lo stesso numero di parametri
   // e lo stesso numero di parametri obbligatori e opzionali
   // altrimenti i modi WhatIsGraphModified e TokenToSearch non vanno correttamente
   C_STRING  cmd, Attrib_name, Mode(_T("NONE")), NoOverlapMode;
   ads_point p_ins;
   short     GenFlag = 0, HorzAlign = 0, VertAlign = 0;
   int       RotCheck = GS_GOOD;
   double    AbsRotation = 9999;
   double    val1, val2;

   ads_point_clear(p_ins);

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function MOVESHEET."));

   // lettura modalità di calcolo
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
      {
         Mode = tostring(cmd.get_name());
         Mode.toupper();

         val1 = val2 = 0.0; // valori di default degli offset
      }
      else if (isnumber(cmd.get_name())) // punto esplicito
      {
         p_ins[X] = cmd.tof(); // coordinata X
         p_ins[Z] = 0.0; // valore di default coordinata Z
      }
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Mode expected in function MOVESHEET."));
   }

   // lettura offset1 (opzionale) oppure lettura coordinata Y (obbligatorio)
   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      if (Mode.comp(_T("NONE")) != 0) // caso offset1
      {
         // lettura offset1
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if(isnumber(cmd.get_name()))
               val1 = cmd.tof();
            else
               return error(screen, stream, ptr, eGSLispNumExpected, _T("First offset expected in function MOVESHEET"));
         }
      }
      else // caso coordinata Y
      {
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (!isnumber(cmd.get_name()))
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Y coordinate expected in function MOVESHEET"));

            p_ins[Y] = cmd.tof(); // coordinata Y
         }
      }

   // lettura offset2 (opzionale) oppure lettura coordinata Z (opzionale)
   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      if (Mode.comp(_T("NONE")) != 0) // caso offset2
      {
         // lettura offset2
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (isnumber(cmd.get_name()))
               val2 = cmd.tof();
            else
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Second offset expected in function MOVESHEET"));
         }
      }
      else // caso coordinata Z
      {
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (!isnumber(cmd.get_name()))
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Z coordinate expected in function MOVESHEET"));

            p_ins[Z] = cmd.tof(); // coordinata Z
         }
      }

   // lettura "controllo rotazione" (opzionale) oppure valore rotazione (opzionale)
   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      if (Mode.comp(_T("NONE")) != 0) // caso "controllo rotazione"
      {
         // lettura controllo_rotz
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (cmd.comp(_T("T")) == 0 || cmd.comp(_T("nil")) == 0)
               RotCheck = (cmd.comp(_T("T")) == 0) ? GS_GOOD : GS_BAD;
            else
            if (isnumber(cmd.get_name())) // rotazione assoluta in gradi
            {
               AbsRotation = gsc_grd2rad(cmd.tof());
               RotCheck = GS_BAD;
            }
            else
               return error(screen, stream, ptr, eGSLispNumExpected, _T("RotCheck expected in function MOVESHEET"));
         }
      }
      else // caso valore rotazione
      {
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name()==NULL) return NULL;

         // se modalità WhatIsGraphModified o TokenToSearch non valuto
         if (WhatIsGraphModified || TokenToSearch)
            { }
         else // modalità normale
         {
            if (!isnumber(cmd.get_name()))
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Rotation expected in function MOVESHEET"));

            AbsRotation = gsc_grd2rad(cmd.tof()); // valore rotazione
         }
      }

   // lettura "controllo sovrapposizione" (opzionale e comunque usato solo se MODE != NONE)
   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
   {
      // lettura controllo_sovrapposiz
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name()==NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if (isstring(cmd.get_name()))
         {
            NoOverlapMode = tostring(cmd.get_name());
            NoOverlapMode.alltrim();
            NoOverlapMode.toupper();
         }
         else
            return error(screen, stream, ptr, eGSLispStrExpected, _T("Overlap control flag expected in function MOVESHEET."));
      }
   }

   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      C_SELSET   GraphObjs, DABlocks;
      ads_name   Obj, DABlock;
      C_STR_LIST attrib_name_list;
      ads_real   rotaz = 0.0; // il valore di default = 0

      // ricavo un gruppo di selezione per gli oggetti grafici e uno per i blocchi DA
      if (GS_SELSET.copy(GraphObjs) == GS_BAD || GraphObjs.intersectType(GRAPHICAL) == GS_BAD)
         return NULL;
      if (GS_SELSET.copy(DABlocks) == GS_BAD || DABlocks.intersectType(DA_BLOCK) == GS_BAD)
         return NULL;

      // se la coordinata di inserimento deve essere calcolata
      if (Mode.comp(_T("NONE")) != 0)
      {
         if (Mode.comp(_T("C")) == 0)
         {
            // scarto i riempimenti
            if (GraphObjs.subtract(_T("HATCH")) == GS_BAD) return NULL;
            // modalità Centroid: centro dell'oggetto; rotazione = 0
            if (gsc_get_centroidpoint(GraphObjs, p_ins) == GS_BAD) return NULL;
            rotaz = 0.0;
         }
         else
         {
            // ricavo solo il primo oggetto grafico !
            if (GraphObjs.entname(0, Obj) == GS_BAD) return NULL;

            if (GS_CALC_CLASS && ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()) // se è stata setta la classe corrente da calcolare
            {
               double UnitConvFactor = ((C_CLASS *) GS_CALC_CLASS)->ptr_GphInfo()->get_UnitCoordConvertionFactorFromClsToWrkSession();
               if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
               {
                  val1 = val1 * UnitConvFactor;
                  val2 = val2 * UnitConvFactor;
               }
            }

		      // calcola posizione e rotazione di un blocco
            if (gsc_getPntRtzOnObj(Obj, p_ins, &rotaz, Mode.get_name(), 
                                   val1, val2, RotCheck) == GS_BAD)
               return NULL;
         }
      }

      // Se era stata specificata una rotazione assoluta del blocco
      if (AbsRotation != 9999) rotaz = AbsRotation;

      // ricavo solo il primo blocco DA !
      if (DABlocks.entname(0, DABlock) == GS_GOOD)
      {
         // solo se esiste almeno un blocco DA
         if (gsc_set_firstPoint(DABlock, p_ins, &rotaz, RotCheck) == GS_BAD)
            return NULL;

         if (NoOverlapMode.len() > 0)
         {  // cerco il punto dove DABlock non si sovrapponga ad altri oggetti grafici
            ads_point RefPt, NewPt;

            if (gsc_getPtNoOverlappedEnt(DABlock, NoOverlapMode.get_name(), RefPt, NewPt) == GS_GOOD &&
                !ads_2Dpoint_equal(RefPt, NewPt))
            {
               p_ins[X] += (NewPt[X] - RefPt[X]);
               p_ins[Y] += (NewPt[Y] - RefPt[Y]);
               if (gsc_set_firstPoint(DABlock, p_ins, &rotaz, RotCheck) == GS_BAD)
                  return NULL;
            }
         }
      }

      cmd = _T("T");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_VisAttrib                                 <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_VISATTR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_VisAttrib(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                          size_t *ptr, const TCHAR *file_name, int flag,
                          long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
	// (visattr nomeattr modalità)
   C_STRING cmd, Attrib_name;
   int      Mode = VISIBLE;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function VISATTR."));

   // lettura nome attributo
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if(!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Attrib name expected in function VISATTR."));
      Attrib_name = tostring(cmd.get_name());
      Attrib_name.toupper();
   }

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function VISATTR."));

   // lettura modalità di visualizzazione
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (cmd.comp(_T("nil")) == 0) Mode = INVISIBLE;
      else Mode = VISIBLE;
   }
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      C_SELSET       DABlocks;
      ads_name       ent;
      long           i = 0;
      C_ATTRIB_BLOCK DABlock;

      // ricavo un gruppo di selezione per i blocchi DA
      if (GS_SELSET.copy(DABlocks) == GS_BAD || DABlocks.intersectType(DA_BLOCK) == GS_BAD)
         return NULL;

      // ciclo i blocchi DA
      while (DABlocks.entname(i++, ent) == GS_GOOD)
      {
         DABlock.set_ent_name(ent);
         if (DABlock.set_vis_attrib(Mode, Attrib_name.get_name()) == GS_BAD) return NULL;
      }

      cmd = _T("T");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_VisSheet                                  <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione GS_LISP_VISSHEET.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_VisSheet(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
	// (vissheet modalità)
   C_STRING cmd;
   int      Mode = VISIBLE;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function VISSHEET."));

   // lettura modalità di visualizzazione
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name()==NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (cmd.comp(_T("nil")) == 0) Mode = INVISIBLE;
      else Mode = VISIBLE;
   }
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto ma ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      C_SELSET       DABlocks;
      ads_name       ent;
      long           i = 0;
      C_ATTRIB_BLOCK DABlock;

      // ricavo un gruppo di selezione per i blocchi DA
      if (GS_SELSET.copy(DABlocks) == GS_BAD || DABlocks.intersectType(DA_BLOCK) == GS_BAD)
         return NULL;

      // ciclo i blocchi DA
      while (DABlocks.entname(i++, ent) == GS_GOOD)
      {
         DABlock.set_ent_name(ent);
         if (DABlock.set_vis_attrib(Mode) == GS_BAD) return NULL;
      }

      cmd = _T("T");
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_NOT_NULL                                  <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione NOT e NULL.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_NOT_NULL(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                         size_t *ptr, const TCHAR *file_name, int flag,
                         long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function NOT"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // non faccio distinzione tra le varie modalità di lavoro
   // (WhatIsGraphModified, TokenToSearch o normale)
   if (cmd.comp(_T("nil")) == 0) cmd = _T("T");
   else cmd = _T("nil");

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_PROGN                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione PROGN.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_PROGN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;
   int      count1, count2;

   del_comment(stream, ptr);

   if (stream[(int) *ptr] == _T(')'))   // ???
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function PROGN."));

   count1 = 1;    // parentesi aperta prima di PROGN
   count2 = 0;    // parentesi chiuse - per controllare fine PROGN

   do
   {
      if (stream[(int) *ptr] == _T('(')) // si tratta di una chiamata a funzione
      {
         count1++;
         (*ptr)++;
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 1,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
      }
      else
         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));

      if (cmd.get_name() == NULL) return NULL;

      del_comment(stream, ptr);
      while(stream[(int) *ptr] == _T(')') && !(count1 == count2))
      {
         count2++;
         if (!(count1 == count2)) (*ptr)++;  // what_is deve sempre uscire con )
         del_comment(stream, ptr);
      }
   }
   while (!(count1 == count2));

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_RTOS                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione RTOS.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_RTOS(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   resbuf   rb;
   double   val1;
   int      val2, sig;
   size_t   stream_len = gsc_strlen(stream);

   acedGetVar(_T("LUNITS"), &rb);
   val2 = rb.resval.rint; // mode di default = LUNITS
   sig = LISP_PREC;       // precision di default = LISP_PREC

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function RTOS"));
   
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         val1 = cmd.tof();
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function RTOS"));
   }

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
   {
      // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
      if (WhatIsGraphModified || TokenToSearch)
         cmd = _T("nil");
      else
      {
         if (val2 < 1 || val2 > 2)
            return error(screen, stream, ptr, eGSInvLispArg, _T("Only scientific or decimal mode in function RTOS"));
         if (sig < 0)
            return error(screen, stream, ptr, eGSInvLispArg, _T("Precision >= 0 expected in function RTOS"));

         cmd.paste(num2str(val2, val1, sig));
         if (cmd.get_name() == NULL) return NULL;
         cmd.paste(stringto(cmd.get_name()));
         if (cmd.get_name() == NULL) return NULL;
      }
         
      return cmd.cut();
   }
   
   operat = cmd;
   
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         val2 = cmd.toi(); // mode
      else
      {
         val1 = 1;
         val2 = 0;
         while(val1 != val2)
         {
            if(stream[(int) *ptr] == _T('(')) val1++;
            if(stream[(int) *ptr] == _T(')')) val2++;
            if(!(val1 == val2)) (*ptr)++;
            if(*ptr > stream_len)
               return error(screen, stream, ptr, eGSLispStreamErr, _T("End stream reached in function RTOS"));
         }
         return operat.cut(); // bisogna controllare LUNIT
      }
   
      if (val2 < 1 || val2 > 2)
         return error(screen, stream, ptr, eGSInvLispArg, _T("Only scientific or decimal mode in function RTOS"));
   }

   del_comment(stream, ptr);
   if(stream[(int) *ptr] == _T(')'))
   {
      // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
      if (WhatIsGraphModified || TokenToSearch)
         cmd = _T("nil");
      else
      {
         if (sig < 0)
            return error(screen, stream, ptr, eGSInvLispArg, _T("Precision >= 0 expected in function RTOS"));

         cmd.paste(num2str((int)val2, val1, sig));
         if (cmd.get_name() == NULL) return NULL;
         cmd.paste(stringto(cmd.get_name()));
         if (cmd.get_name() == NULL) return NULL;
      }
         
      return cmd.cut();
   }
   
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         sig = cmd.toi();
      else
      {
         val1 = 1;
         val2 = 0;
         while (val1 != val2)
         {
            if (stream[*ptr] == _T('(')) val1++;
            if (stream[*ptr] == _T(')')) val2++;
            if (!(val1 == val2)) (*ptr)++;
            if (*ptr > stream_len)
               return error(screen, stream, ptr, eGSLispStreamErr, _T("End stream reached in function RTOS"));
         }
         return operat.cut(); // bisogna controllare LUPREC
      }
   
      if (sig < 0)
         return error(screen, stream, ptr, eGSInvLispArg, _T("Precision >= 0 expected in function RTOS"));
   
      cmd.paste(num2str(val2, val1, sig));
      if (cmd.get_name() == NULL) return NULL;
      cmd.paste(stringto(cmd.get_name()));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_SETQ                                      <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione SETQ.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_SETQ(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function SETQ."));

   do
   {
      operat.paste(reco(screen,stream, ptr));      // memorizzo nome variabile
      if (operat.get_name() == NULL) return NULL;

      // controllo validita'
      if (isnumber(operat.get_name()))
         return error(screen, stream, ptr, eGSInvLispVar, _T("Not a valid symbol in function SETQ."));

      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')'))
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function SETQ."));

      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      // controllo validita'
      if (!isnumber(cmd.get_name()) && !isstring(cmd.get_name()) && 
          cmd.comp(_T("nil")) != 0 && cmd.comp(_T("T")) != 0)
      {             
         if (is_var(curr_fun, cmd.get_name()) == NULL && is_var(/*"geomain"*/GS_EMPTYSTR, cmd.get_name()) == NULL)
         // cmd.set_name("nil");
         return error(screen, stream, ptr, eGSInvLispVar, _T("Not a valid expression in function SETQ."));
      }

      if (is_var(curr_fun, operat.get_name()) != NULL)
      {
         if (alloc_var(screen, stream, ptr, _T('L'), curr_fun, operat.get_name(), cmd.get_name(), NULL)==GS_BAD) 
            return NULL;
      }
      else
      {
         if (alloc_var(screen, stream, ptr, _T('P'), GS_EMPTYSTR/*"geomain"*/, operat.get_name(), cmd.get_name(), NULL)==GS_BAD) 
            return NULL;
      }
      del_comment(stream, ptr);
   }
   while (stream[(int) *ptr] != _T(')'));  // controllare

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_STRCASE                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione STRCASE.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_STRCASE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   bool     UpperCase;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function STRCASE."));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function STRCASE."));

      UpperCase = false;        // all lower case characters
   }

   del_comment(stream, ptr);

   if (stream[(int) *ptr] == _T(')'))
      UpperCase = true;
   else
   {
      operat.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
      if (operat.get_name() == NULL) return NULL;

      if (operat.comp(_T("nil")) == 0) UpperCase = true;
   }

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
      if (UpperCase) cmd.toupper();
      else cmd.tolower();

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_STRCAT                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione STRCAT.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_STRCAT(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, cmd1;
   size_t   stream_len = gsc_strlen(stream);

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function STRCAT"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));
   }
   del_comment(stream, ptr);

   while (stream[(int) *ptr] != _T(')') && *ptr < stream_len)
   {
      cmd1.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                         WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd1.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if (isstring(cmd1.get_name()))
            cmd1 = tostring(cmd1.get_name());
         else
            return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));
      }
      cmd.cat(cmd1.get_name());
      del_comment(stream, ptr);
   }

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "nil"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("nil");
   else // modalità normale
   {
      cmd.paste(stringto(cmd.get_name()));

      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_STRLEN                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione STRLEN.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_STRLEN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      {
         cmd = _T("0");
         return cmd.cut();
      }

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;
   
   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (!isstring(cmd.get_name()))
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));

      cmd = tostring(cmd.get_name());
      cmd = (long) cmd.len();
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_SUBSTR                                   <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione SUBSTR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_SUBSTR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                       size_t *ptr, const TCHAR *file_name, int flag,
                       long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   int      count1, count2;
   size_t   stream_len = gsc_strlen(stream);

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function SUBSTR"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function SUBSTR"));
   }

   operat = cmd;

   // lettura parametro "inizio" di SUBSTR
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else // modalità normale
   {
      if (isnumber(cmd.get_name()))
         count1 = cmd.toi();
      else
         return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function SUBSTR"));

      if ((size_t) count1 > operat.len()) // salta la valutazione del 3 parametro "lunghezza" di SUBSTR
      {
			count1 = 1;
			count2 = 0;
			while (count1 != count2)
			{
				if (stream[(int) *ptr] == _T('(')) count1++;
				if (stream[(int) *ptr] == _T(')')) count2++;
				if (!(count1 == count2)) (*ptr)++;
				if (*ptr > stream_len)
					return error(screen, stream, ptr, eGSLispStreamErr, _T("End stream reached in function SUBSTR."));
			}
         cmd.paste(stringto(_T("")));

         return cmd.cut();
      }

      if (count1 < 1)
         return error(screen, stream, ptr, eGSInvLispArg, _T("First number must be > 1 in function SUBSTR."));

      count2 = (int) operat.len() - count1 + 1;
   }
   del_comment(stream, ptr);

   if (stream[(int) *ptr] != _T(')') && *ptr < stream_len)
   {
      cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                        WhatIsGraphModified, TokenToSearch, QtyToken));
      if (cmd.get_name() == NULL) return NULL;

      // se modalità WhatIsGraphModified o TokenToSearch non valuto
      if (WhatIsGraphModified || TokenToSearch)
         { }
      else // modalità normale
      {
         if (cmd.comp(_T("nil")) != 0)
            if (isnumber(cmd.get_name()))
               count2 = cmd.toi();
            else
               return error(screen, stream, ptr, eGSLispNumExpected, _T("Number expected in function SUBSTR."));

         if (count2 < 0)
            return error(screen, stream, ptr, eGSInvLispArg, _T("Second number must be > 0 in function SUBSTR."));
      }
      del_comment(stream, ptr);
   }

   if (stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) expected in function SUBSTR."));

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      if (count2 > (signed)(operat.len()-count1+1)) count2 = (int) operat.len()-count1+1;

      // for(i=0;i<count2;i++) cmd[i] = operat[i+count1-1];
      // cmd[count2] = _T('\0');
      if (count2 > 0)
         cmd.set_name(operat.get_name(), count1 - 1, count1 + count2 - 2);
      else
         cmd = _T("");
  
      cmd.paste(stringto(cmd.get_name()));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_STRTRAN                                  <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione STRTRAN.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_STRTRAN(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                        size_t *ptr, const TCHAR *file_name, int flag,
                        long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, str, what, with;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) encountered in function STRTRAN"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function STRTRAN"));
   }
   str = cmd;

   // lettura parametro "what" di STRTRAN
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function STRTRAN"));
   }
   what = cmd;

   // lettura parametro "with" di STRTRAN
   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch non valuto
   if (WhatIsGraphModified || TokenToSearch)
      { }
   else
   {
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected in function STRTRAN"));
   }
   with = cmd;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      str.strtran(what.get_name(), with.get_name());
  
      cmd.paste(stringto(str.get_name()));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_VER                                       <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione VER.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_VER(int screen, const TCHAR *stream, size_t *ptr)
{
   C_STRING cmd;

   del_comment(stream, ptr);
   if(stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) expected in function VER."));

   cmd = gsc_msg(474); // "Interprete GEOsim Lisp versione ..."
   cmd.paste(stringto(cmd.get_name()));
   if (cmd.get_name() == NULL) return NULL;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_WHILE                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione WHILE.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_WHILE(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                      size_t *ptr, const TCHAR *file_name, int flag,
                      long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   int      sig, i = (int) *ptr;
   int      count1, count2;
   bool     esci = false;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) not allowed in function WHILE."));

   sig  = (int) *ptr;             // punto di ritorno per rivalutare

   count1 = 1;    // parentesi aperta prima di WHILE
   count2 = 0;    // parentesi chiuse - per controllare fine WHILE
   cmd = _T("nil");

   // valuta la condizione
   while (operat.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                               WhatIsGraphModified, TokenToSearch, QtyToken)) != NULL)
   {
      if (operat.get_name() == NULL) return NULL;

      // se nil allora fine del ciclo se modalitè non WhatIsGraphModified e non
      // TokenToSearch oppure 
      // se modalità WhatIsGraphModified o TokenToSearch devo entrare comunque una 
      // sola volta indipendentemente dalla condizione
      if ((operat.comp(_T("nil")) == 0 && !WhatIsGraphModified && !TokenToSearch) ||
          (esci && (WhatIsGraphModified || TokenToSearch)))
      {
         // vado fino in fondo al WHILE
         // if(stream[(int) *ptr] == _T('('))
         // {
         //    Open_par++;
         //    count1++;
         //    (*ptr)++;
         // }
         // del_comment(stream, ptr);
         // while(stream[(int) *ptr] == _T(')') && !(count1 == (count2+1)))
         // {
         //    Close_par++;
         //    count2++;
         //    if(count1 == (count2+1)) (*ptr)++;  // what_is deve sempre uscire con )
         //    del_comment(stream, ptr);
         // }
         // 
         // le righe sotto sono commentate perchè forse jump_fun non fà quello che serve in questo punto
         // allora ho inserito le righe sopra (stefano 16-10-96)
         *ptr = --sig;  // -- per retrocedere dalla (
         while(stream[(int) *ptr]   != _T('(') &&
               stream[(int) *ptr+1] != _T('W') &&
               stream[(int) *ptr+2] != _T('H') &&
               stream[(int) *ptr+3] != _T('I') &&
               stream[(int) *ptr+4] != _T('L') &&
               stream[(int) *ptr+5] != _T('E'))
            { (*ptr)--; }  // cerco la ( prima di WHILE
         if (jump_fun(screen, stream, ptr) == GS_BAD) return NULL;
         i = (int)*ptr;
         break;
      }
     
      del_comment(stream, ptr);
      if (stream[(int) *ptr] == _T(')')) 
         return error(screen, stream, ptr, eGSInvLispBracket, _T("Character ) not allowed in function WHILE."));

      do  // valuto il corpo del WHILE
      {
         if (stream[(int) *ptr] == _T('('))
         {
            count1++;
            (*ptr)++;
         }

         cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                           WhatIsGraphModified, TokenToSearch, QtyToken));
         if (cmd.get_name() == NULL) return NULL;

         del_comment(stream, ptr);
         while(stream[(int) *ptr] == _T(')') && !(count1 == (count2 + 1)))
         {
            count2++;
            if (count1 == (count2 + 1)) (*ptr)++;  // what_is deve sempre uscire con )
            del_comment(stream, ptr);
         }
      }
      while(!(count1 == (count2+1)) || stream[(int) *ptr] != _T(')'));

      i    = (int) *ptr;
      *ptr = sig;

      esci = true; // per uscire dopo un ciclo
   }
   *ptr = i;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_WHOAMI                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione WHOAMI.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;                 puntatore alla funzione da valutare

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_WHOAMI(int screen, const TCHAR *stream, size_t *ptr)
{
   C_STRING      cmd;
   C_INT_INT_STR usr;

   del_comment(stream, ptr);
   if (stream[(int) *ptr] != _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Parameter not expected in function WHOAMI"));

   if (gsc_whoami(&usr) == GS_BAD) cmd = _T("nil");
   else cmd.paste(stringto(usr.get_name()));

   if (cmd.get_name() == NULL) return NULL;

   return cmd.cut();
}


/******************************************************************/
/*.doc gsc_LISP_YEAR                                    <internal>
/*+                                                            
  Funzione dell'interprete lisp che valuta la funzione YEAR.
  E' una funzione ricorsiva con il compito di riconoscre i token dello stream 
  in ingresso ed eseguire le istruzioni corrispondenti.

  Parametri:
  int screen;               indica chi chiama what_is se = TRUE è chiamata da
                            linea di comando altriomenti da programma
  const TCHAR *curr_fun;     
  const TCHAR *stream;       programma LISP caricato in memoria
  size_t *ptr;               puntatore alla funzione da valutare
  const TCHAR *file_name;    eventuale file da cui caricare funzioni LISP
  int flag;                  se = 1 il token precedente era "("
  long *WhatIsGraphModified; Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla restituendo quali 
                             caratteristiche grafiche che vengono modificate (solo per
                             le principali) sotto forma di un codice a bit codificato 
                             come enum GraphSettingsEnum.
  TCHAR *TokenToSearch;      Se diverso da NULL esamina il corpo 
                             della funzione senza valutarla cercando il token (input)
  int *QtyToken;             Se diverso da NULL allora anche il parametro
                             "TokenToSearch" deve essere diverso da NULL. Memorizza il
                             conteggio di quante volte compare il token cercato (output)

  Ritorna il valore di ritorno della funzione lisp eseguita sotto forma di 
  stringa oppure NULL in caso di errore.
-*/  
/*********************************************************/
TCHAR *gsc_LISP_YEAR(int screen, const TCHAR *curr_fun, const TCHAR *stream,
                     size_t *ptr, const TCHAR *file_name, int flag,
                     long *WhatIsGraphModified, const TCHAR *TokenToSearch, int *QtyToken)
{
   C_STRING cmd, operat;
   int      val1;

   operat = cmd.get_name();

   del_comment(stream, ptr);
   if (stream[(int) *ptr] == _T(')'))
      return error(screen, stream, ptr, eGSInvLispBracket, _T("Invalid character ) encountered in function YEAR"));

   cmd.paste(what_is(screen, curr_fun, stream, ptr, file_name, 0,
                     WhatIsGraphModified, TokenToSearch, QtyToken));
   if (cmd.get_name() == NULL) return NULL;

   // se modalità WhatIsGraphModified o TokenToSearch ritorno sempre valore "0"
   if (WhatIsGraphModified || TokenToSearch)
      cmd = _T("0");
   else // modalità normale
   {
      cmd.alltrim();
      if (isstring(cmd.get_name()))
         cmd = tostring(cmd.get_name());
      else
         return error(screen, stream, ptr, eGSLispStrExpected, _T("String expected"));

      if ((val1 = gsc_getYearDateTime(cmd.get_name())) == -1)
         return error(screen, stream, ptr, eGSLispStrExpected, _T("Date expected"));

      cmd.paste(num2str(2, val1, 0));
      if (cmd.get_name() == NULL) return NULL;
   }

   return cmd.cut();
}
