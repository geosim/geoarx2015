/**********************************************************
Name: GS_THM

Module description: File contenente le funzioni per la gestione dei 
                    tematismi in Geosim 
            
Author: Poltini Roberto & Paolo De Sole (pochino...)

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.


Modification history.
              
Notes and restrictions on use.


**********************************************************/

/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <fcntl.h>       /*  per _open()  */
#include <sys\stat.h>    /*  per _open()  */
#include "adslib.h"
#include <adeads.h>
#include "adsdlg.h"       // gestione DCL
#include <dbapserv.h>     // per acdbHostApplicationServices()
#include <dbmain.h>       // per acCmColor
#include "AcExtensionModule.h" // per CAcModuleResourceOverride

#include "GSresource.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_utily.h"     // (gsc_strcat, gsc_tostring)
#include "gs_resbf.h"     // (gsc_nth)
#include "gs_error.h"     // codici errori
#include "gs_list.h"
#include "gs_init.h"      // direttori globali
#include "gs_netw.h"     
#include "gs_class.h"     // gestione classi di entità GEOsim
#include "gs_graph.h" 
#include "gs_thm.h"       // gestione tematismi e sistemi di coordinate

#include "d2hMap.h" // doc to help


// Dimensioni delle bitmap di preview dei blocchi
#define BLK_BITMAP_WIDTH  32
#define BLK_BITMAP_HEIGHT 32


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/


//----------------------------------------------------------------------------//
//    classe CBlockDlg - scelta di un blocco tra quelli caricati              //
//----------------------------------------------------------------------------//
class CBlockDlg : public CDialog
{
	DECLARE_DYNAMIC(CBlockDlg)

public:
	CBlockDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CBlockDlg();

// Dialog Data
	enum { IDD = IDD_BLOCKS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	CImageList m_ImageList;
	CListCtrl  m_BlkList;
   C_STR_LIST m_BlkNameList;
   CStatic    m_CurrBlockLbl;

   void InitializeImageList(void);

	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()

public:
   CString CurrentBlockName;
   C_STRING DwgPath;

   afx_msg void OnLvnItemchangedBlockList(NMHDR *pNMHDR, LRESULT *pResult);
   afx_msg void OnNMDblclkBlockList(NMHDR *pNMHDR, LRESULT *pResult);
   afx_msg void OnBnClickedOk();
   afx_msg void OnBnClickedCancel();
   afx_msg void OnBnClickedHelp();
};


// struttura usata per scambiare dati nella dcl
struct Common_Dcl_Struct
{
   TCHAR     actual[SYMBOL_NAME_LEN + 1]; // deve essere almeno MAX_LEN_BLOCKNAME, MAX_LEN_LAYERNAME
   TCHAR     Selected[TILE_STR_LIMIT];
   C_RB_LIST List;
};


// struttura usata per scambiare dati nella dcl "layer_models"
struct layer_models_Dcl_Struct
{
   int                prj;
   C_LAYER_MODEL_LIST LayerModelList;
};

// struttura usata per scambiare dati nella dcl "layer_model"
struct layer_model_Dcl_Struct
{
   bool     MultiLayer;
   C_STRING Name;
   C_COLOR  Color;
   C_STRING LineType;
   int      Lineweight;
   C_STRING Description;
   double   Transparency;
};


int gsc_SetCharactSingleLayer(const TCHAR *layer, const TCHAR *LineType = NULL,
                              C_COLOR *color = NULL, bool *isFrozen = NULL,
                              bool *isLocked = NULL, bool *isOff = NULL, 
                              double *Transparency = NULL, bool *ToRegen = NULL);
int gsc_SetCharactSingleLayer(AcDbLayerTableRecord *pLTRec, const TCHAR *LineType = NULL,
                              C_COLOR *color = NULL, bool *isFrozen = NULL,
                              bool *isLocked = NULL, bool *isOff = NULL, 
                              double *Transparency = NULL, bool *ToRegen = NULL);
presbuf gsc_layer_model_to_rb(AcDbLayerTableRecord &LayerRec);
int gsc_layer_model_from_rb(C_RB_LIST &List, AcDbLayerTableRecord &LayerRec);
int gsc_initLineWeightListForLayer(C_INT_STR_LIST &LineWeightDescrList);
int gsc_initTrasparencyListForLayer(C_REAL_LIST &TrasparencyList);

int gsc_getShapeNameNumbers(const TCHAR *ShxPathFile, C_INT_STR_LIST &ShapeNameNumbers);


/*****************************************************************************/
/*.doc gsc_getSymbolName                                          <external> */
/*+
  Questa funzione ricava il nome del simbolo (blocco, tipolinea, 
  layer, stile testo ...).
  Parametri:
  AcDbObjectId objId;   Identificativo del simbolo (input)

  Restituisce il nome del simbolo in caso di successo altrimenti NULL.
-*/  
/*****************************************************************************/
const ACHAR *gsc_getSymbolName(AcDbObjectId objId)
{
   AcDbSymbolTableRecord *pRec = NULL;
   const ACHAR           *pName = NULL;

   if (acdbOpenObject(pRec, objId, AcDb::kForRead) != Acad::eOk) return NULL;
   if (pRec->getName(pName) != Acad::eOk) { pRec->close(); return NULL; }
   if (pRec->close() != Acad::eOk) return NULL;
   
   return pName;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    COLORI    INIZIO      ///////////////////
//-----------------------------------------------------------------------//

// vettore contenente, per ogni codice colore autocad (0 - 255) la corrispondenza RGB
const BYTE C_COLOR::AutoCADLookUpTable[256][3] =
{
   // R    G    B
   { 	0	,	0	,	0	 }, // 	0	 = nero
   { 	255	,	0	,	0	 }, // 	1	 = rosso
   { 	255	,	255	,	0	 }, // 	2	 = giallo
   { 	0	,	255	,	0	 }, // 	3	 = verde
   { 	0	,	255	,	255	 }, // 	4	 = ciano
   { 	0	,	0	,	255	 }, // 	5	 = blu
   { 	255	,	0	,	255	 }, // 	6	 = fucsia
   { 	255	,	255	,	255	 }, // 	7	 = bianco
   { 	65	,	65	,	65	 }, // 	8	
   { 	128	,	128	,	128	 }, // 	9	
   { 	255	,	0	,	0	 }, // 	10	
   { 	255	,	170	,	170	 }, // 	11	
   { 	189	,	0	,	0	 }, // 	12	
   { 	189	,	126	,	126	 }, // 	13	
   { 	129	,	0	,	0	 }, // 	14	
   { 	129	,	86	,	86	 }, // 	15	
   { 	104	,	0	,	0	 }, // 	16	
   { 	104	,	69	,	69	 }, // 	17	
   { 	79	,	0	,	0	 }, // 	18	
   { 	79	,	53	,	53	 }, // 	19	
   { 	255	,	63	,	0	 }, // 	20	
   { 	255	,	191	,	170	 }, // 	21	
   { 	189	,	46	,	0	 }, // 	22	
   { 	189	,	141	,	126	 }, // 	23	
   { 	129	,	31	,	0	 }, // 	24	
   { 	129	,	96	,	86	 }, // 	25	
   { 	104	,	25	,	0	 }, // 	26	
   { 	104	,	78	,	69	 }, // 	27	
   { 	79	,	19	,	0	 }, // 	28	
   { 	79	,	59	,	53	 }, // 	29	
   { 	255	,	127	,	0	 }, // 	30	
   { 	255	,	212	,	170	 }, // 	31	
   { 	189	,	94	,	0	 }, // 	32	
   { 	189	,	157	,	126	 }, // 	33	
   { 	129	,	64	,	0	 }, // 	34	
   { 	129	,	107	,	86	 }, // 	35	
   { 	104	,	52	,	0	 }, // 	36	
   { 	104	,	86	,	69	 }, // 	37	
   { 	79	,	39	,	0	 }, // 	38	
   { 	79	,	66	,	53	 }, // 	39	
   { 	255	,	191	,	0	 }, // 	40	
   { 	255	,	234	,	170	 }, // 	41	
   { 	189	,	141	,	0	 }, // 	42	
   { 	189	,	173	,	126	 }, // 	43	
   { 	129	,	96	,	0	 }, // 	44	
   { 	129	,	118	,	86	 }, // 	45	
   { 	104	,	78	,	0	 }, // 	46	
   { 	104	,	95	,	69	 }, // 	47	
   { 	79	,	59	,	0	 }, // 	48	
   { 	79	,	73	,	53	 }, // 	49	
   { 	255	,	255	,	0	 }, // 	50	
   { 	255	,	255	,	170	 }, // 	51	
   { 	189	,	189	,	0	 }, // 	52	
   { 	189	,	189	,	126	 }, // 	53	
   { 	129	,	129	,	0	 }, // 	54	
   { 	129	,	129	,	86	 }, // 	55	
   { 	104	,	104	,	0	 }, // 	56	
   { 	104	,	104	,	69	 }, // 	57	
   { 	79	,	79	,	0	 }, // 	58	
   { 	79	,	79	,	53	 }, // 	59	
   { 	191	,	255	,	0	 }, // 	60	
   { 	234	,	255	,	170	 }, // 	61	
   { 	141	,	189	,	0	 }, // 	62	
   { 	173	,	189	,	126	 }, // 	63	
   { 	96	,	129	,	0	 }, // 	64	
   { 	118	,	129	,	86	 }, // 	65	
   { 	78	,	104	,	0	 }, // 	66	
   { 	95	,	104	,	69	 }, // 	67	
   { 	59	,	79	,	0	 }, // 	68	
   { 	73	,	79	,	53	 }, // 	69	
   { 	127	,	255	,	0	 }, // 	70	
   { 	212	,	255	,	170	 }, // 	71	
   { 	94	,	189	,	0	 }, // 	72	
   { 	157	,	189	,	126	 }, // 	73	
   { 	64	,	129	,	0	 }, // 	74	
   { 	107	,	129	,	86	 }, // 	75	
   { 	52	,	104	,	0	 }, // 	76	
   { 	86	,	104	,	69	 }, // 	77	
   { 	39	,	79	,	0	 }, // 	78	
   { 	66	,	79	,	53	 }, // 	79	
   { 	63	,	255	,	0	 }, // 	80	
   { 	191	,	255	,	170	 }, // 	81	
   { 	46	,	189	,	0	 }, // 	82	
   { 	141	,	189	,	126	 }, // 	83	
   { 	31	,	129	,	0	 }, // 	84	
   { 	96	,	129	,	86	 }, // 	85	
   { 	25	,	104	,	0	 }, // 	86	
   { 	78	,	104	,	69	 }, // 	87	
   { 	19	,	79	,	0	 }, // 	88	
   { 	59	,	79	,	53	 }, // 	89	
   { 	0	,	255	,	0	 }, // 	90	
   { 	170	,	255	,	170	 }, // 	91	
   { 	0	,	189	,	0	 }, // 	92	
   { 	126	,	189	,	126	 }, // 	93	
   { 	0	,	129	,	0	 }, // 	94	
   { 	86	,	129	,	86	 }, // 	95	
   { 	0	,	104	,	0	 }, // 	96	
   { 	69	,	104	,	69	 }, // 	97	
   { 	0	,	79	,	0	 }, // 	98	
   { 	53	,	79	,	53	 }, // 	99	
   { 	0	,	255	,	63	 }, // 	100	
   { 	170	,	255	,	191	 }, // 	101	
   { 	0	,	189	,	46	 }, // 	102	
   { 	126	,	189	,	141	 }, // 	103	
   { 	0	,	129	,	31	 }, // 	104	
   { 	86	,	129	,	96	 }, // 	105	
   { 	0	,	104	,	25	 }, // 	106	
   { 	69	,	104	,	78	 }, // 	107	
   { 	0	,	79	,	19	 }, // 	108	
   { 	53	,	79	,	59	 }, // 	109	
   { 	0	,	255	,	127	 }, // 	110	
   { 	170	,	255	,	212	 }, // 	111	
   { 	0	,	189	,	94	 }, // 	112	
   { 	126	,	189	,	157	 }, // 	113	
   { 	0	,	129	,	64	 }, // 	114	
   { 	86	,	129	,	107	 }, // 	115	
   { 	0	,	104	,	52	 }, // 	116	
   { 	69	,	104	,	86	 }, // 	117	
   { 	0	,	79	,	39	 }, // 	118	
   { 	53	,	79	,	66	 }, // 	119	
   { 	0	,	255	,	191	 }, // 	120	
   { 	170	,	255	,	234	 }, // 	121	
   { 	0	,	189	,	141	 }, // 	122	
   { 	126	,	189	,	173	 }, // 	123	
   { 	0	,	129	,	96	 }, // 	124	
   { 	86	,	129	,	118	 }, // 	125	
   { 	0	,	104	,	78	 }, // 	126	
   { 	69	,	104	,	95	 }, // 	127	
   { 	0	,	79	,	59	 }, // 	128	
   { 	53	,	79	,	73	 }, // 	129	
   { 	0	,	255	,	255	 }, // 	130	
   { 	170	,	255	,	255	 }, // 	131	
   { 	0	,	189	,	189	 }, // 	132	
   { 	126	,	189	,	189	 }, // 	133	
   { 	0	,	129	,	129	 }, // 	134	
   { 	86	,	129	,	129	 }, // 	135	
   { 	0	,	104	,	104	 }, // 	136	
   { 	69	,	104	,	104	 }, // 	137	
   { 	0	,	79	,	79	 }, // 	138	
   { 	53	,	79	,	79	 }, // 	139	
   { 	0	,	191	,	255	 }, // 	140	
   { 	170	,	234	,	255	 }, // 	141	
   { 	0	,	141	,	189	 }, // 	142	
   { 	126	,	173	,	189	 }, // 	143	
   { 	0	,	96	,	129	 }, // 	144	
   { 	86	,	118	,	129	 }, // 	145	
   { 	0	,	78	,	104	 }, // 	146	
   { 	69	,	95	,	104	 }, // 	147	
   { 	0	,	59	,	79	 }, // 	148	
   { 	53	,	73	,	79	 }, // 	149	
   { 	0	,	127	,	255	 }, // 	150	
   { 	170	,	212	,	255	 }, // 	151	
   { 	0	,	94	,	189	 }, // 	152	
   { 	126	,	157	,	189	 }, // 	153	
   { 	0	,	64	,	129	 }, // 	154	
   { 	86	,	107	,	129	 }, // 	155	
   { 	0	,	52	,	104	 }, // 	156	
   { 	69	,	86	,	104	 }, // 	157	
   { 	0	,	39	,	79	 }, // 	158	
   { 	53	,	66	,	79	 }, // 	159	
   { 	0	,	63	,	255	 }, // 	160	
   { 	170	,	191	,	255	 }, // 	161	
   { 	0	,	46	,	189	 }, // 	162	
   { 	126	,	141	,	189	 }, // 	163	
   { 	0	,	31	,	129	 }, // 	164	
   { 	86	,	96	,	129	 }, // 	165	
   { 	0	,	25	,	104	 }, // 	166	
   { 	69	,	78	,	104	 }, // 	167	
   { 	0	,	19	,	79	 }, // 	168	
   { 	53	,	59	,	79	 }, // 	169	
   { 	0	,	0	,	255	 }, // 	170	
   { 	170	,	170	,	255	 }, // 	171	
   { 	0	,	0	,	189	 }, // 	172	
   { 	126	,	126	,	189	 }, // 	173	
   { 	0	,	0	,	129	 }, // 	174	
   { 	86	,	86	,	129	 }, // 	175	
   { 	0	,	0	,	104	 }, // 	176	
   { 	69	,	69	,	104	 }, // 	177	
   { 	0	,	0	,	79	 }, // 	178	
   { 	53	,	53	,	79	 }, // 	179	
   { 	63	,	0	,	255	 }, // 	180	
   { 	191	,	170	,	255	 }, // 	181	
   { 	46	,	0	,	189	 }, // 	182	
   { 	141	,	126	,	189	 }, // 	183	
   { 	31	,	0	,	129	 }, // 	184	
   { 	96	,	86	,	129	 }, // 	185	
   { 	25	,	0	,	104	 }, // 	186	
   { 	78	,	69	,	104	 }, // 	187	
   { 	19	,	0	,	79	 }, // 	188	
   { 	59	,	53	,	79	 }, // 	189	
   { 	127	,	0	,	255	 }, // 	190	
   { 	212	,	170	,	255	 }, // 	191	
   { 	94	,	0	,	189	 }, // 	192	
   { 	157	,	126	,	189	 }, // 	193	
   { 	64	,	0	,	129	 }, // 	194	
   { 	107	,	86	,	129	 }, // 	195	
   { 	52	,	0	,	104	 }, // 	196	
   { 	86	,	69	,	104	 }, // 	197	
   { 	39	,	0	,	79	 }, // 	198	
   { 	66	,	53	,	79	 }, // 	199	
   { 	191	,	0	,	255	 }, // 	200	
   { 	234	,	170	,	255	 }, // 	201	
   { 	141	,	0	,	189	 }, // 	202	
   { 	173	,	126	,	189	 }, // 	203	
   { 	96	,	0	,	129	 }, // 	204	
   { 	118	,	86	,	129	 }, // 	205	
   { 	78	,	0	,	104	 }, // 	206	
   { 	95	,	69	,	104	 }, // 	207	
   { 	59	,	0	,	79	 }, // 	208	
   { 	73	,	53	,	79	 }, // 	209	
   { 	255	,	0	,	255	 }, // 	210	
   { 	255	,	170	,	255	 }, // 	211	
   { 	189	,	0	,	189	 }, // 	212	
   { 	189	,	126	,	189	 }, // 	213	
   { 	129	,	0	,	129	 }, // 	214	
   { 	129	,	86	,	129	 }, // 	215	
   { 	104	,	0	,	104	 }, // 	216	
   { 	104	,	69	,	104	 }, // 	217	
   { 	79	,	0	,	79	 }, // 	218	
   { 	79	,	53	,	79	 }, // 	219	
   { 	255	,	0	,	191	 }, // 	220	
   { 	255	,	170	,	234	 }, // 	221	
   { 	189	,	0	,	141	 }, // 	222	
   { 	189	,	126	,	173	 }, // 	223	
   { 	129	,	0	,	96	 }, // 	224	
   { 	129	,	86	,	118	 }, // 	225	
   { 	104	,	0	,	78	 }, // 	226	
   { 	104	,	69	,	95	 }, // 	227	
   { 	79	,	0	,	59	 }, // 	228	
   { 	79	,	53	,	73	 }, // 	229	
   { 	255	,	0	,	127	 }, // 	230	
   { 	255	,	170	,	212	 }, // 	231	
   { 	189	,	0	,	94	 }, // 	232	
   { 	189	,	126	,	157	 }, // 	233	
   { 	129	,	0	,	64	 }, // 	234	
   { 	129	,	86	,	107	 }, // 	235	
   { 	104	,	0	,	52	 }, // 	236	
   { 	104	,	69	,	86	 }, // 	237	
   { 	79	,	0	,	39	 }, // 	238	
   { 	79	,	53	,	66	 }, // 	239	
   { 	255	,	0	,	63	 }, // 	240	
   { 	255	,	170	,	191	 }, // 	241	
   { 	189	,	0	,	46	 }, // 	242	
   { 	189	,	126	,	141	 }, // 	243	
   { 	129	,	0	,	31	 }, // 	244	
   { 	129	,	86	,	96	 }, // 	245	
   { 	104	,	0	,	25	 }, // 	246	
   { 	104	,	69	,	78	 }, // 	247	
   { 	79	,	0	,	19	 }, // 	248	
   { 	79	,	53	,	59	 }, // 	249	
   { 	51	,	51	,	51	 }, // 	250	
   { 	80	,	80	,	80	 }, // 	251	
   { 	105	,	105	,	105	 }, // 	252	
   { 	130	,	130	,	130	 }, // 	253	
   { 	190	,	190	,	190	 }, // 	254	
   { 	255	,	255	,	255	 } // 	255	
};


C_COLOR::C_COLOR()
{
   mColorMethod = None;
}
C_COLOR::~C_COLOR()
{}

C_COLOR& C_COLOR::operator = (const C_COLOR &color)
{
   mColor_value = color.mColor_value;
   mColorMethod = color.mColorMethod;
   return *this;
}

bool C_COLOR::operator == (const C_COLOR &color)
{
   COLORREF RGB1, RGB2;

   if (getRGB(&RGB1) == GS_BAD)
      return (mColorMethod == color.mColorMethod) ? true : false;

   if (color.getRGB(&RGB2) == GS_BAD) return false;

   return (RGB1 == RGB2) ? true : false;
}

bool C_COLOR::operator != (const C_COLOR &color)
{
   COLORREF RGB1, RGB2;

   if (getRGB(&RGB1) == GS_BAD)
      return (mColorMethod != color.mColorMethod) ? true : false;

   if (color.getRGB(&RGB2) == GS_BAD) return true;

   return (RGB1 != RGB2) ? true : false;
}


void C_COLOR::setByLayer(void)    { mColorMethod = ByLayer; }
void C_COLOR::setByBlock(void)    { mColorMethod = ByBlock; }
void C_COLOR::setForeground(void) { mColorMethod = Foreground; }
void C_COLOR::setNone(void)       { mColorMethod = None; }
C_COLOR::ColorMethodEnum C_COLOR::getColorMethod(void) const { return mColorMethod; }


/*********************************************************/
/*.doc C_COLOR::setRGB                        <external> */
/*+
  Questa funzione imposta il colore in formato RGB.
  Parametri:
  COLORREF in;   Codice RGB
  oppure
  const BYTE red;    Componente rossa del colore RGB
  const BYTE green;  Componente verde del colore RGB
  const BYTE blue;   Componente blu del colore RGB
-*/  
/*********************************************************/
int C_COLOR::setRGB(COLORREF in)
{
   mColor_value.Color = in;
   mColorMethod = ByRGBColor;
   
   return GS_GOOD;
}
int C_COLOR::setRGB(const BYTE red, const BYTE green, const BYTE blue)
{
   mColor_value.Color = RGB(red, green, blue);
   mColorMethod = ByRGBColor;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setRed                        <external> */
/*+
  Questa funzione imposta la componente rossa del colore.
  Parametri:
  const BYTE red;    Componente rossa del colore RGB
-*/  
/*********************************************************/
int C_COLOR::setRed(const BYTE red)
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         BYTE green, blue;

         getGreen(&green);
         getBlue(&blue);
         mColor_value.Color = RGB(red, green, blue);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         Color = RGB(red, GetGValue(Color & 0xffff), GetBValue(Color));
         RGB2AutoCADColorIndex(Color, &mColor_value.AutoCADColorIndex);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setGreen                      <external> */
/*+
  Questa funzione imposta la componente verde del colore.
  Parametri:
  const BYTE green;    Componente verde del colore RGB
-*/  
/*********************************************************/
int C_COLOR::setGreen(const BYTE green)
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         BYTE red, blue;

         getRed(&red);
         getBlue(&blue);
         mColor_value.Color = RGB(red, green, blue);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         Color = RGB(GetRValue(Color), green, GetBValue(Color));
         RGB2AutoCADColorIndex(Color, &mColor_value.AutoCADColorIndex);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setBlue                       <external> */
/*+
  Questa funzione imposta la componente blu del colore.
  Parametri:
  const BYTE blue;    Componente blu del colore RGB
-*/  
/*********************************************************/
int C_COLOR::setBlue(const BYTE blue)
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         BYTE red, green;

         getRed(&red);
         getGreen(&green);
         mColor_value.Color = RGB(red, green, blue);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         Color = RGB(GetRValue(Color), GetGValue(Color & 0xffff), blue);
         RGB2AutoCADColorIndex(Color, &mColor_value.AutoCADColorIndex);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getRGB                       <external> */
/*+
  Questa funzione ritorna il colore in formato RGB.
  Parametri:
  COLORREF *out;    output

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getRGB(COLORREF *out) const
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         *out = mColor_value.Color;
         break;
      }
      case ByAutoCADColorIndex:
      {
         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, out);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getRed                        <external> */
/*+
  Questa funzione ritorna la componente blu del colore.
  Parametri:
  BYTE *red;    Componente blu del colore RGB

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/
/*********************************************************/
int C_COLOR::getRed(BYTE *red) const
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         *red = GetRValue(mColor_value.Color);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         *red = GetRValue(Color);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getGreen                      <external> */
/*+
  Questa funzione ritorna la componente verde del colore.
  Parametri:
  BYTE *green;    Componente verde del colore RGB

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/
/*********************************************************/
int C_COLOR::getGreen(BYTE *green) const
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         *green = GetGValue(mColor_value.Color & 0xffff);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         *green = GetGValue(Color & 0xffff);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getBlue                       <external> */
/*+
  Questa funzione ritorna la componente blu del colore.
  Parametri:
  BYTE *blue;    Componente blu del colore RGB

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/
/*********************************************************/
int C_COLOR::getBlue(BYTE *blue) const
{
   switch (mColorMethod)
   {
      case ByRGBColor:
      {
         *blue = GetBValue(mColor_value.Color);
         break;
      }
      case ByAutoCADColorIndex:
      {
         COLORREF Color;

         AutoCADColorIndex2RGB(mColor_value.AutoCADColorIndex, &Color);
         *blue = GetBValue(Color);
         break;
      }
      default:
      {
         GS_ERR_COD = eGSInvalidColor; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getAutoCADColorIndex          <external> */
/*+
  Questa funzione restituisce il colore in formato ACAD.
  Parametri:
  int in;        Codice colore autocad (output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getAutoCADColorIndex(int *AutoCADColorIndex) const
{
   switch (mColorMethod)
   {
      case ByLayer:
         *AutoCADColorIndex = 256;
         break;
      case ByBlock:
         *AutoCADColorIndex = 0;
         break;
      case ByRGBColor:
         if (RGB2AutoCADColorIndex(mColor_value.Color, AutoCADColorIndex) == GS_BAD) return GS_BAD;
         break;
      case ByAutoCADColorIndex:
         *AutoCADColorIndex = mColor_value.AutoCADColorIndex;
         break;
      case Foreground:
         *AutoCADColorIndex = 7;
         break;
      default:
         GS_ERR_COD = eGSInvalidColor;
         return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getAutoCADColorIndex          <external> */
/*+
  Questa funzione imposta il colore in formato ACAD.
  Parametri:
  int in;        Codice colore autocad (input)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setAutoCADColorIndex(int in)
{
   if (in < 0 || in > 256) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }

   mColor_value.AutoCADColorIndex = in;
   switch (in)
   {
      case 0:
         mColorMethod = ByBlock;
         break;
      case 7:
         mColorMethod = Foreground;
         break;
      case 256:
         mColorMethod = ByLayer;
         break;
      default:
         mColorMethod = ByAutoCADColorIndex;
   }
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getHTMLColor                  <external> */
/*+
  Questa funzione restituisce il colore in formato HTML
  che è fomato dal carattere # seguito dal colore in esadecimale
  (es. #FF0000 = rosso).
  Parametri:
  C_STRING &HTMLColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getHTMLColor(C_STRING &HTMLColor) const
{
   COLORREF color;
   TCHAR    szHex[7 + 1];

   if (getRGB(&color) == GS_BAD) return GS_BAD;
   swprintf_s(szHex, 8, _T("#%02X%02X%02X"), GetRValue(color), GetGValue(color & 0xffff), GetBValue(color));
   HTMLColor = szHex;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setHTMLColor                  <external> */
/*+
  Questa funzione imposta il colore in formato HTML
  che è fomato dal carattere # seguito dal colore in esadecimale
  (es. #FF0000 = rosso).
  Parametri:
  C_STRING &HTMLColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setHTMLColor(C_STRING &HTMLColor)
{
   BYTE     red, green, blue;
   COLORREF color;

   if (HTMLColor.len() != 7) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   if (HTMLColor.get_chr(0) != _T('#')) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   // salto il carattere iniziale #
   color = (COLORREF) wcstol(HTMLColor.get_name() + 1, NULL, 16);
   red   = LOBYTE((color)>>16);
   green = LOBYTE(((WORD)(color & 0xffff)) >> 8);
   blue  = LOBYTE(color);
   mColor_value.Color = RGB(red, green, blue);
   mColorMethod = ByRGBColor;
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getHexColor                   <external> */
/*+
  Questa funzione restituisce il colore in formato esadecimale
  (es. FF0000 = rosso).
  Parametri:
  C_STRING &HexColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getHexColor(C_STRING &HexColor) const
{
   COLORREF color;
   TCHAR szHex[6 + 1];

   if (getRGB(&color) == GS_BAD) return GS_BAD;
   swprintf_s(szHex, 7, _T("%02X%02X%02X"), GetRValue(color), GetGValue(color & 0xffff), GetBValue(color));
   HexColor = szHex;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setHexColor                   <external> */
/*+
  Questa funzione imposta il colore in formato esadecimale
  (es. FF0000 = rosso).
  Parametri:
  C_STRING &HexColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setHexColor(C_STRING &HexColor)
{
   BYTE     red, green, blue;
   COLORREF color;

   if (HexColor.len() != 6) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   color = (COLORREF) wcstol(HexColor.get_name(), NULL, 16);
   red   = LOBYTE((color)>>16);
   green = LOBYTE(((WORD)(color & 0xffff)) >> 8);
   blue  = LOBYTE(color);
   mColor_value.Color = RGB(red, green, blue);
   mColorMethod = ByRGBColor;
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getRGBDecColor                <external> */
/*+
  Questa funzione restituisce il colore in formato stringa con
  i componenti RGB in decimale separati da un carattere (es. "255 0 0" = rosso).
  Parametri:
  C_STRING &RGBDecColor;
  const TCHAR Sep;         opzionale, carattere separatore (default ',')

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getRGBDecColor(C_STRING &RGBDecColor, const TCHAR Sep) const
{
   COLORREF color;

   if (getRGB(&color) == GS_BAD) return GS_BAD;
   RGBDecColor = (int) GetRValue(color);
   RGBDecColor += Sep;
   RGBDecColor += (int) GetGValue(color & 0xffff);
   RGBDecColor += Sep;
   RGBDecColor += (int) GetBValue(color);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setRGBDecColor                <external> */
/*+
  Questa funzione imposta il colore in formato stringa con
  i componenti RGB in decimale separati da un carattere (es. "255 0 0" = rosso).
  Parametri:
  C_STRING &RGBDecColor;
  const TCHAR Sep;         opzionale, carattere separatore (default ' ')

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setRGBDecColor(C_STRING &RGBDecColor, const TCHAR Sep)
{
   C_STRING _RGBDecColor(RGBDecColor), StrNum;
   TCHAR    *str;
   int      Num;
   BYTE     red, green, blue;

   str     = _RGBDecColor.get_name();
   // se numero non corretto di informazioni
   if (gsc_strsep(str, _T('\0')) != 2) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   StrNum = str;
   if (StrNum.isDigit() == GS_BAD) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   Num = StrNum.toi();
   if (Num < 0 || Num > 255) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   red = (BYTE) Num;

   while (*str != _T('\0')) str++; str++;
   StrNum = str;
   if (StrNum.isDigit() == GS_BAD) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   Num = StrNum.toi();
   if (Num < 0 || Num > 255) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   green = (BYTE) Num;

   while (*str != _T('\0')) str++; str++;
   StrNum = str;
   if (StrNum.isDigit() == GS_BAD) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   Num = StrNum.toi();
   if (Num < 0 || Num > 255) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   blue = (BYTE) Num;

   return setRGB(red, green, blue);
}


/*********************************************************/
/*.doc C_COLOR::getRGBDXFColor                <external> */
/*+
  Questa funzione restituisce il colore in formato stringa con
  i componenti RGB in decimale separati da un carattere (es. "255 0 0" = rosso).
  Parametri:
  COLORREF *RGBDXFColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::getRGBDXFColor(COLORREF *RGBDXFColor) const
{
   BYTE red, green, blue;

   if (getRed(&red) == GS_BAD || getGreen(&green) == GS_BAD || getBlue(&blue) == GS_BAD)
      return GS_BAD;

   *RGBDXFColor = RGB(blue, green, red); // blue e rosso vanno invertiti

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::setRGBDXFColor                <external> */
/*+
  Questa funzione imposta il colore in formato stringa con
  i componenti RGB in decimale separati da un carattere (es. "255 0 0" = rosso).
  Parametri:
  COLORREF RGBDXFColor;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setRGBDXFColor(COLORREF RGBDXFColor)
{
   BYTE red, green, blue;

   red   = LOBYTE((RGBDXFColor)>>16);
   green = LOBYTE(((WORD)(RGBDXFColor & 0xffff)) >> 8);
   blue  = LOBYTE(RGBDXFColor);
   mColor_value.Color = RGB(red, green, blue);
   mColorMethod = ByRGBColor;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getResbuf                     <external> */
/*+
  Questa funzione restituisce un resbuf contenente il colore in formato 
  numerico (RTSHORT) se il colore era con codice autocad oppure in formato
  stringa se il colore era in formato RGB (es. "255,0,0" = rosso).

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD;
  N.B. Alloca memoria
-*/  
/*********************************************************/
presbuf C_COLOR::getResbuf(void) const
{
   switch (mColorMethod)
   {
      case ByLayer:
         return acutBuildList(62, 256, 0); // dxf code x colore autocad
      case ByBlock:
         return acutBuildList(62, 0, 0);
      case ByRGBColor:
      {
         COLORREF RGBDXFColor;

         getRGBDXFColor(&RGBDXFColor);
         return acutBuildList(420, (long) RGBDXFColor, 0); // dxf code x colore RGB
      }
      case ByAutoCADColorIndex:
         return acutBuildList(62, mColor_value.AutoCADColorIndex, 0); // dxf code x colore autocad
      case Foreground:
         return acutBuildList(62, 7, 0); // dxf code x colore autocad
   }

   return NULL;
}


/*********************************************************/
/*.doc C_COLOR::setResbuf                     <external> */
/*+
  Questa funzione setta il colore leggendo un resbuf contenente il colore in formato 
  numerico (RTSHORT) se il colore era con codice autocad oppure in formato
  stringa se il colore era in formato RGB (es. "255,0,0" = rosso).
  Parametri:
  presbuf pRb;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setResbuf(presbuf pRb)
{ 
   switch (pRb->restype)
   {
      case RTSHORT:
      case 62: // dxf code x colore autocad
         switch (pRb->resval.rint)
         {
            case 0:
               setByBlock();
               return GS_GOOD;
            case 256:
               setByLayer();
               return GS_GOOD;
            case 7:
               setForeground();
               return GS_GOOD;
            default:
               return setAutoCADColorIndex(pRb->resval.rint);
         }
         break;
      case RTLONG:
      case 420: // dxf code x colore RGB
         return setRGBDXFColor((COLORREF) pRb->resval.rlong);
      case RTSTR:
         return setString(pRb->resval.rstring);
      default:
         return GS_BAD;
   }
}


/*********************************************************/
/*.doc C_COLOR::getString                     <external> */
/*+
  Questa funzione restituisce una stringa contenente il colore in formato 
  stringa che è un numero da 0 a 256 se il colore era con codice autocad
  oppure in formato stringa "R,G,B" se il colore era in formato RGB.
  Parametri:
  C_STRING &StrColor;   (out)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD;
-*/  
/*********************************************************/
int C_COLOR::getString(C_STRING &StrColor) const
{
   switch (mColorMethod)
   {
      case ByLayer:
         StrColor = _T("256");
         return GS_GOOD;
      case ByBlock:
         StrColor = _T("0");
         return GS_GOOD;
      case ByRGBColor:
         getRGBDecColor(StrColor, _T(','));
         return GS_GOOD;
      case ByAutoCADColorIndex:
         StrColor = mColor_value.AutoCADColorIndex;
         return GS_GOOD;
      case Foreground:
         StrColor = _T("7");
         return GS_GOOD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc C_COLOR::setString                     <external> */
/*+
  Questa funzione setta il colore leggendo una stringa contenente il colore in formato 
  stringa che è un numero da 0 a 256 se il colore era con codice autocad
  oppure in formato stringa "R,G,B" se il colore era in formato RGB.
  Parametri:
  const TCHAR *StrColor;
  oppure
  C_STRING &StrColor

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::setString(const TCHAR *StrColor)
{
   C_STRING Buff(StrColor);
   return setString(Buff);
}
int C_COLOR::setString(C_STRING &StrColor)
{ 
   if (StrColor.isDigit() == GS_GOOD)
      return setAutoCADColorIndex(StrColor.toi());
   else
   if (StrColor.comp(gsc_msg(24), FALSE) == 0 || StrColor.comp(_T("BYBLOCK"), FALSE) == 0) // "DABLOCCO"       
      setByBlock();
   else 
   if (StrColor.comp(gsc_msg(23), FALSE) == 0 || StrColor.comp(_T("BYLAYER"), FALSE) == 0) // "DALAYER"
      setByLayer();
   else
      return setRGBDecColor(StrColor, _T(','));

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::getColorName                <external> */
/*+
  Questa funzione restituisce il nome del colore se possibile
  altrimenti NULL.
-*/  
/*********************************************************/
const TCHAR *C_COLOR::getColorName(void) const
{
   C_STRING HexColor;
   COLORREF Color;

   // leggo il colore in formato esadecimale perchè in quel formato il blu è il byte più basso
   if (getHexColor(HexColor) == GS_BAD) return GS_BAD;
   Color = (COLORREF) wcstol(HexColor.get_name(), NULL, 16);
   
   switch (Color)
   {
      // DA: http://codicicolorihtml.altervista.org/codici-colori-rgb
      case 0x7FFFD4: // 	Acquamarina
         return gsc_msg(501);
      case 0xE52B50: // 	Amaranto
         return gsc_msg(502);
      case 0xFFBF00: // 	Ambra
         return gsc_msg(503);
      case 0x9966CC: // 	Ametista
         return gsc_msg(504);
      case 0xFFA500: // 	Arancione
         return gsc_msg(505);
      case 0xFF9900: // 	Arancione fiamma
         return gsc_msg(506);
      case 0x708090: // 	Ardesia
         return gsc_msg(507);
      case 0xC0C0C0: // 	Argento
         return gsc_msg(508);
      case 0x7BA05B: // 	Asparago
         return gsc_msg(509);
      case 0xFFFFF0: // 	Avorio
         return gsc_msg(510);
      case 0x007FFF: // 	Azzurro
         return gsc_msg(511);
      case 0xABCDEF: // 	Azzurro fiordaliso
         return gsc_msg(512);
      case 0x6397D0: // 	Azzurro Savoia
         return gsc_msg(513);
      case 0xF5F5DC: // 	Beige
         return gsc_msg(514);
      case 0x908435: // 	Beige-oliva chiaro
         return gsc_msg(515);
      case 0xFFFFFF: // 	Bianco
         return gsc_msg(516);
      case 0xFFFEEF: // 	Bianco antico
         return gsc_msg(517);
      case 0xFAEBD7: // 	Bianco di titanio
         return gsc_msg(518);
      case 0xFEFEE9: // 	Bianco di zinco
         return gsc_msg(519);
      case 0xFFE4C4: // 	Biscotto
         return gsc_msg(520);
      case 0x3D2B1F: // 	Bistro
         return gsc_msg(521);
      case 0x0000FF: // 	Blu
         return gsc_msg(522);
      case 0x4682B4: // 	Blu acciaio
         return gsc_msg(523);
      case 0xF0F8FF: // 	Blu alice
         return gsc_msg(539);
      case 0x0095B6: // 	Blu Bondi
         return gsc_msg(549);
      case 0x5F9EA0: // 	Blu Cadetto
         return gsc_msg(550);
      case 0x2A52BE: // 	Blu ceruleo
         return gsc_msg(551);
      case 0x1C39BB: // 	Blu di Persia
         return gsc_msg(552);
      case 0x003153: // 	Blu di Prussia
         return gsc_msg(553);
      case 0x1E90FF: // 	Blu Dodger
         return gsc_msg(554);
      case 0x003399: // 	Blu elettrico
         return gsc_msg(555);
      case 0x3A75C4: // 	Blu Klein
         return gsc_msg(556);
      case 0x000080: // 	Blu marino
         return gsc_msg(564);
      case 0x343A90: // 	Blu notte
         return gsc_msg(583);
      case 0x120A8F: // 	Blu oltremare
         return gsc_msg(584);
      case 0x4169E1: // 	Blu reale
         return gsc_msg(585);
      case 0x800000: // 	Bordeaux
         return gsc_msg(586);
      case 0xCD7F32: // 	Bronzo
         return gsc_msg(587);
      case 0x75663F: // 	Bronzo antico
         return gsc_msg(588);
      case 0xF0DC82: // 	Camoscio
         return gsc_msg(589);
      case 0x050402: // 	Carbone
         return gsc_msg(590);
      case 0x960018: // 	Carminio
         return gsc_msg(595);
      case 0xE0FFFF: // 	Carta da zucchero
         return gsc_msg(596);
      case 0xCD5C5C: // 	Castagno
         return gsc_msg(617);
      case 0x986960: // 	Castagno scuro
         return gsc_msg(634);
      case 0xDDADAF: // 	Castano chiaro
         return gsc_msg(656);
      case 0xD2B48C: // 	Catrame
         return gsc_msg(659);
      case 0x918151: // 	Catrame scuro
         return gsc_msg(662);
      case 0xACE1AF: // 	Celadon
         return gsc_msg(664);
      case 0x99CBFF: // 	Celeste
         return gsc_msg(670);
      case 0x007BA7: // 	Ceruleo
         return gsc_msg(684);
      case 0x08457E: // 	Ceruleo scuro
         return gsc_msg(685);
      case 0x7FFF00: // 	Chartreuse
         return gsc_msg(686);
      case 0x00FFFF: // 	Ciano
         return gsc_msg(688);
      case 0xDE3163: // 	Ciliegia
         return gsc_msg(689);
      case 0xD2691E: // 	Cioccolato
         return gsc_msg(690);
      case 0x0047AB: // 	Cobalto
         return gsc_msg(694);
      case 0xFFF5EE: // 	Conchiglia
         return gsc_msg(697);
      case 0xFF7F50: // 	Corallo
         return gsc_msg(769);
      case 0xFFFDD0: // 	Crema
         return gsc_msg(771);
      case 0xDC143C: // 	Cremisi
         return gsc_msg(772);
      case 0x1560BD: // 	Denim
         return gsc_msg(773);
      case 0x5E86C1: // 	Denim chiaro
         return gsc_msg(776);
      case 0xDF73FF: // 	Eliotropo
         return gsc_msg(777);
      case 0xC2B280: // 	Ecru
         return gsc_msg(778);
      case 0x6495ED: // 	Fiore di granturco
         return gsc_msg(779);
      case 0x008080: // 	Foglia di tè
         return gsc_msg(780);
      case 0xF400A1: // 	Fucsia
         return gsc_msg(781);
      case 0xE0AFEE: // 	Fucsia Bordesto Lillato
         return gsc_msg(782);
      case 0xEBB55F: // 	Fulvo
         return gsc_msg(783);
      case 0xDCDCDC: // 	Gainsboro
         return gsc_msg(784);
      case 0x00A86B: // 	Giada
         return gsc_msg(785);
      case 0xFFFF00: // 	Giallo
         return gsc_msg(786);
      case 0xF7E89F: // 	Giallo Napoli
         return gsc_msg(787);
      case 0xFFFF66: // 	Giallo pastello
         return gsc_msg(797);
      case 0xFFD800: // 	Giallo scuolabus
         return gsc_msg(820);
      case 0xD8BFD8: // 	Glicine
         return gsc_msg(822);
      case 0xC9A0DC: // 	Glicine
         return gsc_msg(822);
      case 0xF5DEB3: // 	Grano
         return gsc_msg(839);
      case 0x7B1B02: // 	Granata
         return gsc_msg(840);
      case 0xF7F7F7: // 	Grigio 5%
         return gsc_msg(841);
      case 0xEFEFEF: // 	Grigio 10%
         return gsc_msg(842);
      case 0xD2D2D2: // 	Grigio 20%
         return gsc_msg(843);
      case 0xB2B2B2: // 	Grigio 30%
         return gsc_msg(844);
      case 0x8F8F8F: // 	Grigio 40%
         return gsc_msg(847);
      case 0x808080: // 	Grigio 50%
         return gsc_msg(862);
      case 0x5F5F5F: // 	Grigio 60%
         return gsc_msg(863);
      case 0x4F4F4F: // 	Grigio 70%
         return gsc_msg(864);
      case 0x404040: // 	Grigio 75%
         return gsc_msg(865);
      case 0x2F2F2F: // 	Grigio 80%
         return gsc_msg(866);
      case 0x465945: // 	Grigio asparago
         return gsc_msg(867);
      case 0x2F4F4F: // 	Grigio ardesia scuro
         return gsc_msg(868);
      case 0x778899: // 	Grigio ardesia chiaro
         return gsc_msg(869);
      case 0xE4E5E0: // 	Grigio cenere
         return gsc_msg(870);
      case 0xD0AFAE: // 	Grigio rosso chiaro
         return gsc_msg(871);
      case 0xCADABA: // 	Grigio tè verde
         return gsc_msg(872);
      case 0xCC8899: // 	Incarnato prugna
         return gsc_msg(873);
      case 0x4B0082: // 	Indaco
         return gsc_msg(874);
      case 0x6F00FF: // 	Indaco elettrico
         return gsc_msg(875);
      case 0x310062: // 	Indaco scuro
         return gsc_msg(876);
      case 0x002FA7: // 	International Klein Blue
         return gsc_msg(877);
      case 0xC3B091: // 	Kaki
         return gsc_msg(878);
      case 0xF0E68C: // 	Kaki chiaro
         return gsc_msg(879);
      case 0xBDB76B: // 	Kaki scuro
         return gsc_msg(880);
      case 0xE30B5C: // 	Lampone
         return gsc_msg(881);
      case 0xE6E6FA: // 	Lavanda
         return gsc_msg(882);
      case 0xDABAD0: // 	Lavanda pallido
         return gsc_msg(883);
      case 0xFFF0F5: // 	Lavanda rosata
         return gsc_msg(884);
      case 0xFDE910: // 	Limone
         return gsc_msg(885);
      case 0xFFFACD: // 	Limone crema
         return gsc_msg(886);
      case 0xC8A2C8: // 	Lilla
         return gsc_msg(887);
      case 0xCCFF00: // 	Lime
         return gsc_msg(888);
      case 0xFAF0E6: // 	Lino
         return gsc_msg(889);
      case 0xFF00FF: // 	Magenta
         return gsc_msg(890);
      case 0xF984E5: // 	Magenta chiaro
         return gsc_msg(891);
      case 0xF8F4FF: // 	Magnolia
         return gsc_msg(892);
      case 0x993366: // 	Malva
         return gsc_msg(893);
      case 0x996666: // 	Malva chiaro
         return gsc_msg(894);
      case 0xFFCC00: // 	Mandarino
         return gsc_msg(895);
      case 0x964B00: // 	Marrone
         return gsc_msg(896);
      case 0xCD853F: // 	Marrone chiaro
         return gsc_msg(897);
      case 0x987654: // 	Marrone pastello
         return gsc_msg(898);
      case 0x993300: // 	Marrone-rosso
         return gsc_msg(899);
      case 0xDABDAB: // 	Marrone sabbia chiaro
         return gsc_msg(900);
      case 0x654321: // 	Marrone scuro
         return gsc_msg(901);
      case 0x990066: // 	Melanzana
         return gsc_msg(902);
      case 0xC04000: // 	Mogano
         return gsc_msg(903);
      case 0x000000: // 	Nero
         return gsc_msg(904);
      case 0xCC7722: // 	Ocra
         return gsc_msg(905);
      case 0xDA70D6: // 	Orchidea
         return gsc_msg(906);
      case 0x898437: // 	Oliva chiaro
         return gsc_msg(907);
      case 0xFFD700: // 	Oro
         return gsc_msg(908);
      case 0xCFB53B: // 	Oro vecchio
         return gsc_msg(909);
      case 0xDAA520: // 	Oro vivo
         return gsc_msg(910);
      case 0xB8860B: // 	Oro vivo smorto
         return gsc_msg(911);
      case 0xCC9966: // 	Ottone antico
         return gsc_msg(912);
      case 0xFFEFD5: // 	Papaia
         return gsc_msg(913);
      case 0xCCCCFF: // 	Pervinca
         return gsc_msg(914);
      case 0xFFE5B4: // 	Pesca
         return gsc_msg(915);
      case 0xFFDAB9: // 	Pesca scuro
         return gsc_msg(916);
      case 0xFFCC99: // 	Pesca-arancio
         return gsc_msg(917);
      case 0xFADFAD: // 	Pesca-giallo
         return gsc_msg(918);
      case 0xE5E4E2: // 	Platino
         return gsc_msg(919);
      case 0xB20000: // 	Porpora
         return gsc_msg(920);
      case 0x660066: // 	Prugna
         return gsc_msg(921);
      case 0xB87333: // 	Rame
         return gsc_msg(922);
      case 0xFFC0CB: // 	Rosa
         return gsc_msg(923);
      case 0xFF9966: // 	Rosa arancio
         return gsc_msg(924);
      case 0x997A8D: // 	Rosa Mountbatten
         return gsc_msg(925);
      case 0xFADADD: // 	Rosa pallido
         return gsc_msg(926);
      case 0xFFD1DC: // 	Rosa pastello
         return gsc_msg(927);
      case 0x500000: // 	Rosso sangue
         return gsc_msg(928);
      case 0xE75480: // 	Rosa scuro
         return gsc_msg(929);
      case 0xFC0FC0: // 	Rosa shocking
         return gsc_msg(930);
      case 0xDB244F: // 	Rosa medio
         return gsc_msg(931);
      case 0xFF007F: // 	Rosa vivo
         return gsc_msg(932);
      case 0xFF0000: // 	Rosso
         return gsc_msg(933);
      case 0xCC5500: // 	Rosso aragosta
         return gsc_msg(934);
      case 0xC41E3A: // 	Rosso cardinale
         return gsc_msg(935);
      case 0xCE3018: // 	Rosso fragola
         return gsc_msg(936);
      case 0xB22222: // 	Rosso mattone
         return gsc_msg(937);
      case 0xBD8E80: // 	Rosso mattone chiaro
         return gsc_msg(938);
      case 0xFF6088: // 	Rosso rosa
         return gsc_msg(939);
      case 0xD21F1B: // 	Rosso pompeiano
         return gsc_msg(940);
      case 0xBA6262: // 	Rosso Tiziano
         return gsc_msg(941);
      case 0xC80815: // 	Rosso veneziano
         return gsc_msg(942);
      case 0xDB7093: // 	Rosso violetto chiaro
         return gsc_msg(943);
      case 0xC71585: // 	Rosso violaceo
         return gsc_msg(944);
      case 0xA61022: // 	Rosso fuoco
         return gsc_msg(945);
      case 0xFF6347: // 	Rosso pomodoro
         return gsc_msg(946);
      case 0x410012: // 	Rubino
         return gsc_msg(947);
      case 0xF4A460: // 	Sabbia
         return gsc_msg(948);
      case 0xFF8C69: // 	Salmone
         return gsc_msg(949);
      case 0xE9967A: // 	Salmone scuro
         return gsc_msg(950);
      case 0xFF2400: // 	Scarlatto
         return gsc_msg(951);
      case 0x560319: // 	Scarlatto scuro
         return gsc_msg(952);
      case 0x704214: // 	Seppia
         return gsc_msg(953);
      case 0xE97451: // 	Terra di Siena
         return gsc_msg(954);
      case 0x531B00: // 	Terra di Siena bruciata
         return gsc_msg(955);
      case 0xD0F0C0: // 	Tè verde
         return gsc_msg(956);
      case 0xBADBAD: // 	Tè verde scuro
         return gsc_msg(957);
      case 0x30D5C8: // 	Turchese
         return gsc_msg(958);
      case 0x08E8DE: // 	Turchese chiaro
         return gsc_msg(959);
      case 0x99FFCC: // 	Turchese pallido
         return gsc_msg(960);
      case 0x116062: // 	Turchese scuro
         return gsc_msg(961);
      case 0x00CCCC: // 	Uovo di pettirosso
         return gsc_msg(962);
      case 0x96DED1: // 	Uovo di pettirosso chiaro
         return gsc_msg(963);
      case 0x00FF00: // 	Verde
         return gsc_msg(964);
      case 0x00CC99: // 	Verde caraibi
         return gsc_msg(965);
      case 0x228b22: // 	Verde foresta
         return gsc_msg(966);
      case 0x66FF00: // 	Verde chiaro
         return gsc_msg(967);
      case 0xADFF2F: // 	Verde-giallo
         return gsc_msg(968);
      case 0x8FBC8F: // 	Verde mare chiaro
         return gsc_msg(969);
      case 0x2E8B57: // 	Verde marino
         return gsc_msg(970);
      case 0x3CB371: // 	Verde marino medio
         return gsc_msg(971);
      case 0x98FF98: // 	Verde menta
         return gsc_msg(972);
      case 0xDAFDDA: // 	Verde menta chiaro
         return gsc_msg(973);
      case 0xADDFAD: // 	Verde muschio
         return gsc_msg(974);
      case 0x808000: // 	Verde oliva
         return gsc_msg(975);
      case 0x6B8E23: // 	Verde olivastro
         return gsc_msg(976);
      case 0x909909: // 	Verde oliva-giallo
         return gsc_msg(977);
      case 0x556832: // 	Verde oliva scuro
         return gsc_msg(978);
      case 0x66FF66: // 	Verde pastello chiaro
         return gsc_msg(979);
      case 0x77DD77: // 	Verde pastello
         return gsc_msg(980);
      case 0x03C03C: // 	Verde pastello scuro
         return gsc_msg(981);
      case 0x01796F: // 	Verde pino
         return gsc_msg(982);
      case 0x00FF7F: // 	Verde primavera
         return gsc_msg(983);
      case 0x177245: // 	Verde primavera scuro
         return gsc_msg(984);
      case 0x013220: // 	Verde scuro
         return gsc_msg(985);
      case 0x50C878: // 	Verde smeraldo
         return gsc_msg(986);
      case 0x40826D: // 	Verde Veronese
         return gsc_msg(987);
      case 0xFF4D00: // 	Vermiglio
         return gsc_msg(988);
      case 0x8F00FF: // 	Viola
         return gsc_msg(989);
      case 0x991199: // 	Viola-melanzana
         return gsc_msg(990);
      case 0x423189: // 	Viola scuro
         return gsc_msg(991);
      case 0xF4C430: // 	Zafferano
         return gsc_msg(992);
      case 0xFF9933: // 	Zafferano profondo
         return gsc_msg(993);
      // DA: http://users.libero.it/luclep/itaint.htm
      case 0xFF1F35: // 	Anguria
         return gsc_msg(994);
      case 0xFF8100: // 	Arancio
         return gsc_msg(995);
      case 0xE12000: // 	Arancio bruciato
         return gsc_msg(996);
      case 0xE26200: // 	Arancio scuro
         return gsc_msg(997);
      case 0xE0E074: // 	Avocado
         return gsc_msg(998);
      case 0x101000: // 	Avocado scuro
         return gsc_msg(999);        
      case 0xFFFFD0: // 	Avorio
         return gsc_msg(1000);
      case 0x212100: // 	Bruno Van Dyck
         return gsc_msg(1001);
      case 0xD0B1A1: // 	Caffellatte
         return gsc_msg(1002);
      case 0x400000: // 	Castagna scuro
         return gsc_msg(1003);         
      case 0xBFBF00: // 	Certosa
         return gsc_msg(1004);
      case 0x622100: // 	Cioccolata
         return gsc_msg(1005);
      case 0xFFC281: // 	Crema
         return gsc_msg(771);
      case 0xA00000: // 	Cremisi scuro
         return gsc_msg(1006);
      case 0xC0C27C: // 	Creta        
         return gsc_msg(1007);
      case 0xFFFF80: // 	Giallo chiaro
         return gsc_msg(1008);
      case 0xE0A175: // 	Giallo di Marte
         return gsc_msg(1009);
      case 0x7F604F: // 	Grigio rosso scuro
         return gsc_msg(1010);
      case 0x82823F: // 	Grigio oliva   // arrivato fino a qui      
         return gsc_msg(1011);
      case 0xF1F1B4: // 	Lattemiele         
         return gsc_msg(1012);
      case 0xFFFF35: // 	Limone
         return gsc_msg(885);
      case 0xD2B06A: // 	Kaki
         return gsc_msg(878);
      case 0xFF1F10: // 	Mandarino
         return gsc_msg(895);
      case 0xBF4100: // 	Marrone chiaro
         return gsc_msg(897);
      case 0x824200: // 	Marrone scuro
         return gsc_msg(901);
      case 0x424200: // 	Marrone avana
         return gsc_msg(1013);
      case 0xC21212: // 	Mattone   
         return gsc_msg(1014);
      case 0x600000: // 	Mogano
         return gsc_msg(903);
      case 0xA13F00: // 	Nocciola
         return gsc_msg(1015);
      case 0xA16252: // 	Ocra bruna
         return gsc_msg(1016);
      case 0xFFBF18: // 	Oro
         return gsc_msg(908);
      case 0xA1A100: // 	Oliva chiaro
         return gsc_msg(907);
      case 0x626200: // 	Oliva scuro
         return gsc_msg(1017);
      case 0xFFFFC2: // 	Paglierino
         return gsc_msg(1018);
      case 0xFF9F71: // 	Pesca chiaro
         return gsc_msg(1019);
      case 0xFF8141: // 	Pesca
         return gsc_msg(915);
      case 0xFF421E: // 	Pesca scuro
         return gsc_msg(916);         
      case 0xFFE1DC: // 	Rosa tenue o incarnato         
         return gsc_msg(1020);
      case 0xE10000: // 	Rosso scuro         
         return gsc_msg(1021);
      case 0xFFE1B0: // 	Sabbia
         return gsc_msg(948);
      case 0xFFC0B6: // 	Sabbia rosata
         return gsc_msg(1022);
      case 0x806210: // 	Safari
         return gsc_msg(1023);
      case 0xFF8080: // 	Salmone
         return gsc_msg(949);
      case 0xFF9F9F: // 	Salmone chiaro
         return gsc_msg(1024);
      case 0xFF4040: // 	Salmone scuro
         return gsc_msg(950);
      case 0xFFE118: // 	Senape
         return gsc_msg(1025);
      case 0x604200: // 	Terra d'ombra
         return gsc_msg(1026);
      case 0x412000: // 	Terra d'ombra bruciata
         return gsc_msg(1027);
      case 0x200000: // 	Terra di Siena bruciata
         return gsc_msg(955);
      case 0xA11F12: // 	Terracotta
         return gsc_msg(1028);
      case 0xFFEFCE: // 	Vaniglia
         return gsc_msg(1029);
      case 0xF1F180: //    Verde limone
         return gsc_msg(1030);
      case 0xE1E140: //    Verde limone scuro
         return gsc_msg(1031);
      case 0xC20000: // 	Vermiglione
         return gsc_msg(1032);
      case 0xE01F25: // 	Vermiglione chiaro
         return gsc_msg(1033);
      case 0xE1E100: // 	Zolfo
         return gsc_msg(1034);
         
      default: 
         return GS_BAD;
   }
   return GS_BAD;
}


/*********************************************************/
/*.doc C_COLOR::RGB2AutoCADColorIndex         <external> */
/*+
  Questa funzione cerca il colore autocad più vicino al colore RGB in input.
  Parametri:
  COLORREF in;   Codice RGB (input)
  int *out;      Codice colore autocad (output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::RGB2AutoCADColorIndex(COLORREF in, int *out)
{
   BYTE r = GetRValue(in);
   BYTE g = GetGValue(in & 0xffff);
   BYTE b = GetBValue(in);
   int  diff, prev_diff, i, iBest;

   if ((prev_diff = (abs((int) C_COLOR::AutoCADLookUpTable[0][0] - (int) r) +
                     abs((int) C_COLOR::AutoCADLookUpTable[0][1] - (int) g) +
                     abs((int) C_COLOR::AutoCADLookUpTable[0][2] - (int) b))) == 0)
      { *out = 0; return GS_GOOD; }
   iBest = 0;

   for (i = 1; i < 256; i++)
   {
      if ((diff = (abs((int) C_COLOR::AutoCADLookUpTable[i][0] - (int) r) +
                   abs((int) C_COLOR::AutoCADLookUpTable[i][1] - (int) g) +
                   abs((int) C_COLOR::AutoCADLookUpTable[i][2] - (int) b))) == 0)
         { *out = i; return GS_GOOD; }

      if (diff < prev_diff)
      {
         iBest = i;
         prev_diff = diff;
      }
   }

   *out = iBest;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_COLOR::AutoCADColorIndex2RGB         <external> */
/*+
  Questa funzione restituisce il colore RGB corrispondente al colore autocad in input.
  Parametri:
  int  in;        Codice colore autocad (input)
  COLORREF *out;  Codice RGB (output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int C_COLOR::AutoCADColorIndex2RGB(int in, COLORREF *out)
{
   if (in < 0 || in > 255) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }

   const BYTE r = C_COLOR::AutoCADLookUpTable[in][0];
   const BYTE g = C_COLOR::AutoCADLookUpTable[in][1];
   const BYTE b = C_COLOR::AutoCADLookUpTable[in][2];

   *out = RGB(r, g, b);

   return GS_GOOD;
}

//---------------------------------------------------------------------------//
// INIZIO CLASSE C_COLOR_LIST                                                //
//---------------------------------------------------------------------------//

C_COLOR_LIST::C_COLOR_LIST():C_LIST() {}

C_COLOR_LIST::~C_COLOR_LIST() {}

int C_COLOR_LIST::add_tail_color(COLORREF Color)
{
   C_COLOR *p;

   if ((p = new C_COLOR()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   p->setRGB(Color);
   
   return add_tail(p);
}

int C_COLOR_LIST::add_tail_color(int AutoCADColorIndex)
{
   C_COLOR *p;

   if ((p = new C_COLOR()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   p->setAutoCADColorIndex(AutoCADColorIndex);
   
   return add_tail(p);
}


/*********************************************************/
/*.doc C_COLOR_LIST::getColorsFromTo          <external> */
/*+
  Questa funzione restituisce una lista di colori da un colore ad un altro.
  Parametri:
  C_COLOR &From;           Colore di partenza
  C_COLOR &To;             Colore di arrivo
  int nColors;             Numero di colori che si devono usare 
                           per la sfumatura

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_COLOR_LIST::getColorsFromTo(C_COLOR &From, C_COLOR &To, int nColors)
{
   BYTE            FromRed, FromGreen, FromBlue, ToRed, ToGreen, ToBlue;
   int             i, StepRed, StepGreen, StepBlue;
   COLORREF        fromColor, toColor;

   remove_all();
   if (nColors <= 0) return GS_GOOD;

   if (From.getRGB(&fromColor) == GS_BAD) return GS_BAD;
   FromRed   = GetRValue(fromColor);
   FromGreen = GetGValue(fromColor & 0xffff);
   FromBlue  = GetBValue(fromColor);

   if (To.getRGB(&toColor) == GS_BAD) return GS_BAD;
   ToRed   = GetRValue(toColor);
   ToGreen = GetGValue(toColor & 0xffff);
   ToBlue  = GetBValue(toColor);

   StepRed   = (ToRed - FromRed) / (nColors - 1);
   StepGreen = (ToGreen - FromGreen) / (nColors - 1);
   StepBlue  = (ToBlue - FromBlue) / (nColors - 1);

   if (add_tail_color(fromColor) == GS_BAD) // il primo colore è quello di partenza
      { remove_all(); return GS_BAD; }

   for (i = 1; i < nColors - 1; i++)
   {
      FromRed   += StepRed;
      FromGreen += StepGreen;
      FromBlue  += StepBlue;

      add_tail_color(RGB(FromRed, FromGreen, FromBlue));
   }

   if (add_tail_color(toColor) == GS_BAD) // l'ultimo colore è quello di arrivo
      { remove_all(); return GS_BAD; }

   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_set_CurrentColor                       <external> */
/*+
  Questa funzione setta il colore corrente.
  Parametri:
  C_COLOR color;   Colore da impostare come corrente
  C_COLOR *old;    Colore precedente (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentColor(C_COLOR &color, C_COLOR *old)
{
   resbuf   rb;
   C_STRING StrColor;

   rb.rbnext = NULL;

   // Memorizza vecchio valore in old
   if (old)
   {
      if (acedGetVar(_T("CECOLOR"), &rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      StrColor = rb.resval.rstring;

      if (StrColor.comp(_T("BYLAYER"), FALSE) == 0) old->setByLayer();
      else if (StrColor.comp(_T("BYBLOCK"), FALSE) == 0) old->setByBlock();
      else if (StrColor.isDigit() == GS_GOOD) old->setAutoCADColorIndex(StrColor.toi());
      else if (StrColor.at(_T("RGB:"), FALSE) == StrColor.get_name()) // RGB
      {
         C_STRING RGBColor(StrColor.get_name() + gsc_strlen(_T("RGB:")));
         old->setRGBDecColor(RGBColor, _T(','));
      }
   }                              

   switch (color.getColorMethod())
   {
      case C_COLOR::ByBlock:
         StrColor = _T("BYBLOCK");
         break;
      case C_COLOR::ByLayer:
         StrColor = _T("BYLAYER");
         break;
      case C_COLOR::Foreground:
         StrColor = _T("7");
         break;         
      case C_COLOR::ByAutoCADColorIndex:
      {
         int AutoCADColorIndex;

         if (color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) return GS_BAD;
         StrColor = AutoCADColorIndex;
         break;
      }
      case C_COLOR::ByRGBColor:
      {
         C_STRING RGBColor;

         if (color.getRGBDecColor(RGBColor, _T(',')) == GS_BAD) return GS_BAD;
         StrColor = _T("RGB:");
         StrColor += RGBColor;
      }
      default:
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   }

   rb.restype = RTSTR;
   rb.resval.rstring = StrColor.get_name();
   
   if (acedSetVar(_T("CECOLOR"), &rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_C_COLOR_to_AcCmColor               <external> */
/*+
  Questa funzione convetre un oggetto C_COLOR in AcCmColor.
  Parametri:
  const C_COLOR &in;
  AcCmColor &out;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_C_COLOR_to_AcCmColor(const C_COLOR &in, AcCmColor &out)
{
   switch (in.getColorMethod())
   {
      case C_COLOR::ByLayer:
         out.setColorMethod(AcCmEntityColor::kByLayer);
         return GS_GOOD;
      case C_COLOR::ByBlock:
         out.setColorMethod(AcCmEntityColor::kByBlock);
         return GS_GOOD;
      case C_COLOR::ByRGBColor:
      {
         BYTE red, green, blue;

         if (in.getRed(&red) == GS_BAD || in.getGreen(&green) == GS_BAD || in.getBlue(&blue) == GS_BAD)
            return GS_BAD;
         out.setRGB(red, green, blue);
         return GS_GOOD;
      }
      case C_COLOR::ByAutoCADColorIndex:
      {
         int AutoCADColorIndex;
         
         in.getAutoCADColorIndex(&AutoCADColorIndex);
         out.setColorIndex(AutoCADColorIndex);
         return GS_GOOD;
      }
      case C_COLOR::Foreground:
         //out.setColorMethod(AcCmEntityColor::kForeground); // non si capisce che colore sia
         out.setColorIndex(7); // per ora uso il colore 7
         return GS_GOOD;
      case C_COLOR::None:
         out.setColorMethod(AcCmEntityColor::kNone);
         return GS_GOOD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_AcCmColor_to_C_COLOR               <external> */
/*+
  Questa funzione convetre un oggetto AcCmColor in C_COLOR.
  Parametri:
  const AcCmColor &in;
  C_COLOR &out;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_AcCmColor_to_C_COLOR(const AcCmColor &in, C_COLOR &out)
{
   switch (in.colorMethod())
   {
      case AcCmEntityColor::kByLayer:
         out.setByLayer();
         return GS_GOOD;
      case AcCmEntityColor::kByBlock:
         out.setByBlock();
         return GS_GOOD;
      case AcCmEntityColor::kByColor:
      {
         BYTE red = in.red(), green = in.green(), blue = in.blue();

         out.setRGB(red, green, blue);
         return GS_GOOD;
      }
      case AcCmEntityColor::kByACI:
      {
         int AutoCADColorIndex = in.colorIndex();
         
         out.setAutoCADColorIndex(AutoCADColorIndex);
         return GS_GOOD;
      }
      case AcCmEntityColor::kForeground:
         out.setForeground();
         return GS_GOOD;
      case AcCmEntityColor::kNone:
         out.setNone();
         return GS_GOOD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_color                          <external> */
/*+
  Questa funzione setta il colore di un'entità.
  Parametri:
  AcDbEntity *pEnt;  Oggetto grafico
  C_COLOR &color;    Colore
  oppure
  ads_name ent;      Oggetto grafico
  C_COLOR &color;    Colore

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_color(AcDbEntity *pEnt, C_COLOR &color)
{
   AcCmColor _color;

   if (gsc_C_COLOR_to_AcCmColor(color, _color) == GS_BAD) return GS_BAD;

   if (pEnt->color() == _color) return GS_GOOD; // colore uguale

   Acad::ErrorStatus Res = pEnt->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (pEnt->setColor(_color) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   
   return GS_GOOD;
}
int gsc_set_color(ads_name ent, C_COLOR &color)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_color((AcDbEntity *) pObj, color) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_color                          <external> */
/*+
  Questa funzione legge il colore di un'entità.
  Parametri:
  AcDbEntity *pEnt;  Oggetto grafico
  C_COLOR &color;    Colore
  oppure
  ads_name ent;      Oggetto grafico
  C_COLOR &color;    Colore

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_get_color(AcDbEntity *pEnt, C_COLOR &color)
{
   return gsc_AcCmColor_to_C_COLOR(pEnt->color(), color);
}
int gsc_get_color(ads_name ent, C_COLOR &color)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_color((AcDbEntity *) pObj, color) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SetColorDialog                     <external> */
/*+
  Questa funzione permette la scelta di un colore tramite interfaccia grafica.
  Parametri:
  C_COLOR &Color;               On entry the default color on exit of OK user chosen color
  bool AllowMetaColor;          if TRUE the pseudo-colours "BYLAYER" and "BYBLOCK"
                                will be allowed
  const C_COLOR &CurLayerColor; is the color of the current layer, For use in the
                                color swatch if the color is set to "BYLAYER"

  Ritorna true in caso di successo altrimenti false
-*/  
/*********************************************************/
int gs_SetColorDialog(void)
{
   C_COLOR Color, CurLayerColor;
   bool    AllowMetaColor = true;
   presbuf arg = acedGetArgs();
 
   acedRetNil();
   if (arg)
   {
      Color.setResbuf(arg);
      if ((arg = arg->rbnext))
      {
         gsc_rb2Bool(arg, &AllowMetaColor);
         if ((arg = arg->rbnext))
            CurLayerColor.setResbuf(arg);
      }
   }
   if (gsc_SetColorDialog(Color, AllowMetaColor, CurLayerColor) == false)
      return RTNORM;
   C_RB_LIST RbList;
   if ((RbList << Color.getResbuf()) == NULL) return RTNORM;
   acedRetVal(RbList.get_head());

   return RTNORM;
}
bool gsc_SetColorDialog(C_COLOR &Color, bool AllowMetaColor, const C_COLOR &CurLayerColor)
{
   AcCmColor _Color, _CurLayerColor;

   if (Color.getColorMethod() != C_COLOR::None) gsc_C_COLOR_to_AcCmColor(Color, _Color);
   if (CurLayerColor.getColorMethod() != C_COLOR::None) gsc_C_COLOR_to_AcCmColor(CurLayerColor, _CurLayerColor);
   if (acedSetColorDialogTrueColor(_Color, AllowMetaColor, _CurLayerColor) == Adesk::kFalse)
      return false;

   return (gsc_AcCmColor_to_C_COLOR(_Color, Color) == GS_GOOD) ? true : false;

   //CColorDialog ColorDialog;
   //COLORREF     CurrentColor;

   //if (Color.getRGB(&CurrentColor) == GS_GOOD)
   //   ColorDialog.SetCurrentColor(CurrentColor);
   //if (ColorDialog.DoModal() != IDOK) return false;
   //Color.setRGB(ColorDialog.GetColor());
   //return GS_GOOD;
}


/*********************************************************/
/*.doc gs_ColorToString                       <external> */
/*+
  Questa funzione LISP converte un colore in stringa.
  Parametri:
  (cons <tipo colore> <codice colore>)
  dove
  <tipo colore> = 62 per codice colore autocad
                  420 per codice colore RGB
  <codice colore> = numero intero del colore se colore autocad [0-256]
                    stringa R,G,B se colore RGB

  Ritorna un stringa in caso di successo altrimenti nil
-*/  
/*********************************************************/
int gs_ColorToString(void)
{
   presbuf  arg = acedGetArgs();
   C_COLOR  Color;
   C_STRING StrColor;
 
   acedRetNil();
   Color.setResbuf(arg);
   if (Color.getString(StrColor) == GS_BAD) return RTNORM;
   acedRetStr(StrColor.get_name());

   return RTNORM;
}


/*********************************************************/
/*.doc gs_StringToColor                       <external> */
/*+
  Questa funzione LISP converte una stringa in colore.
  Parametri:
  (<strina colore>)
  ritorna:
  (cons <tipo colore> <codice colore>)

  Ritorna un stringa in caso di successo altrimenti nil
-*/  
/*********************************************************/
int gs_StringToColor(void)
{
   presbuf   arg = acedGetArgs();
   C_COLOR   Color;
   C_RB_LIST RbList;
 
   acedRetNil();
   if (!arg || arg->restype != RTSTR) return RTNORM;
   Color.setString(arg->resval.rstring);
   if ((RbList << Color.getResbuf()) == NULL) return RTNORM;
   acedRetVal(RbList.get_head());

   return RTNORM;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    COLORI    FINE        ///////////////////
//////////////////    GESTIONE    TIPILINEA    INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_getLTypeListFromFile               <internal> */
/*+
  Questa funzione restituisce una lista dei nomi e delle descrizioni dei
  tipi linea contenuti in un file .LIN (la lista non è ordinata alfabeticamente).
  Parametri:
  C_STRING    &PathFileLin;
  C_2STR_LIST &LTypeNameList;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getLTypeListFromFile(C_STRING &PathFileLin, C_2STR_LIST &LTypeNameList)
{
   C_STRING buffer, name, descr;
   FILE     *file;
   int      i;
   TCHAR    *str;

   LTypeNameList.remove_all();
   
   // Apertura file NON in modo UNICODE (i file .LIN sono ancora in ANSI)
   bool IsUnicode = false;
   if ((file = gsc_fopen(PathFileLin, _T("rb"), MORETESTS, &IsUnicode)) == NULL)
      return GS_BAD;

   while (gsc_readline(file, buffer, false) != WEOF) // leggo riga
      if (buffer.get_chr(0) == _T('*')) // se primo carattere '*' si tratta di tipo linea
      {
         str = buffer.get_name();
         i   = gsc_strsep(str, _T('\0'));
         name = str + 1;
         name.alltrim();

         if (i > 0) // lettura descrizione tipo linea
         {
            while (*str != _T('\0')) str++; str++;
            descr = str;
            descr.alltrim();
         }
         else
            descr.clear();

         if (LTypeNameList.add_tail_2str(name.get_name(), descr.get_name()) == GS_BAD)
            { gsc_fclose(file); return GS_BAD; }
      }

   return gsc_fclose(file);
}


/******************************************************************************/
/*.doc gsc_getCurrentGEOsimLTypeFilePath                           <external> */
/*+
  Questa funzione restituisce il percorso completo del file contenente le
  definizioni dei tipi linea di GEOsim. Il file si trova nella cartella THM
  della cartella server di GEOsim e si chiama GEOSIM.LIN se impostato in 
  Autocad il sistema di misurazione inglese (variabile MEASUREMENT=0) oppure
  GEOSIMISO.LIN se impostato in Autocad il sistema di misurazione metrico
  (variabile MEASUREMENT=1)
  Parametri:
  C_STRING &Path;    (output)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int gsc_getCurrentGEOsimLTypeFilePath(C_STRING &Path)
{
   resbuf Val;
  
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOTHMDIR;
   Path += _T('\\');
   Path += GEOTLINE;
   if (acedGetVar(_T("MEASUREMENT"), &Val) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   // Metrico
   if (Val.restype == RTSHORT && Val.resval.rint == 1) Path += _T("ISO");
   Path += _T(".LIN");

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getgeosimltype <external> */
/*+
  Questa funzione restituisce una lista dei tipi linea di GEOsim.
  ((<nome1> <descrizione1>) ...)
-*/  
/*********************************************************/
int gs_getgeosimltype(void)
{
   C_2STR_LIST lineTypeList;
   C_RB_LIST   ret;

   gsc_getGEOsimLType(lineTypeList);
   ret << lineTypeList.to_rb();
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getGEOsimLType                     <external> */
/*+
  Questa funzione restituisce una lista dei tipi linea di GEOsim (<nome1> <descrizione1>).

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getGEOsimLType(C_2STR_LIST &lineTypeList)
{
   C_STRING file_tline;
  
   if (gsc_getCurrentGEOsimLTypeFilePath(file_tline) == GS_BAD) return GS_BAD;

   if (gsc_path_exist(file_tline) == GS_BAD)
   {
      if (gsc_refresh_thm() == GS_BAD) return GS_BAD;
      if (gsc_path_exist(file_tline) == GS_BAD) return GS_BAD;
   }
   
   // Lettura dei nomi e delle descrizioni dei tipi linea nel file
   if (gsc_getLTypeListFromFile(file_tline, lineTypeList) == GS_BAD) return GS_BAD;

   // Aggiungo 2 tipi linea
   if (lineTypeList.add_tail_2str(_T("BYLAYER"), NULL) == GS_BAD) return GS_BAD; // "DALAYER" non funzione più da map 2013
   if (lineTypeList.add_tail_2str(_T("CONTINUOUS"), 
                                   _T("__________________________________________")) == GS_BAD)
      return GS_BAD;

   // ordino la lista degli elementi per nome
   lineTypeList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_validlinetype                      <external> */
/*+
  Questa funzione verifica la validità del tipo linea.
  Parametri:
  const TCHAR *line;  nome del tipo linea (dimensionato a MAX_LEN_LINETYPENAME)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validlinetype(const TCHAR *line)
{
   size_t      len;
   C_2STR_LIST GEOsimLineTypeNameList;

   if (line == NULL) { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }   
   len = wcslen(line);   
   if (len == 0 || len >= MAX_LEN_LINETYPENAME)
      { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }   

   // case insensitive
   if (gsc_strcmp(line, LINETYPE_BYLAYER, FALSE) == 0 || 
       gsc_strcmp(line, LINETYPE_BYBLOCK, FALSE) == 0)
      return GS_GOOD;

   if (gsc_strcmp(line, gsc_msg(23), FALSE) == 0)    // "DALAYER"
      return GS_GOOD;
   else
      if (gsc_strcmp(line, gsc_msg(24), FALSE) == 0) // "DABLOCCO"
         return GS_GOOD;

   if (gsc_getGEOsimLType(GEOsimLineTypeNameList) == GS_BAD)
      { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }
   if (GEOsimLineTypeNameList.search_name(line, FALSE) == NULL)  // case insensitive
      { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }

   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_load_linetype                      <external> */
/*+
  Questa funzione carica il tipolinea se non è già stato caricato.
  Parametri:
  const TCHAR *line;  nome del tipo linea (dimensionato a MAX_LEN_LINETYPENAME)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_load_linetype(const TCHAR *line)
{
   resbuf   *rb;
   C_STRING file;
   TCHAR    name[MAX_LEN_LINETYPENAME];

   gsc_strcpy(name, line, MAX_LEN_LINETYPENAME);

   if (gsc_validlinetype(name) == GS_BAD) return GS_BAD;

   gsc_alltrim(name);                              
   // case insensitive
   if (gsc_strcmp(name, gsc_msg(23), FALSE) == 0) wcscpy(name, LINETYPE_BYLAYER); // "DALAYER"
   if (gsc_strcmp(name, gsc_msg(24), FALSE) == 0) wcscpy(name, LINETYPE_BYBLOCK); // "DABLOCCO"

   if ((rb = acdbTblSearch(_T("LTYPE"), name, 0)) == NULL)
   {
      if (gsc_getCurrentGEOsimLTypeFilePath(file) == GS_BAD) return GS_BAD;

      AcDbDatabase      *pCurrDatabase;
      Acad::ErrorStatus es;
      
      // database corrente
      pCurrDatabase = acdbHostApplicationServices()->workingDatabase();
      if ((es = pCurrDatabase->loadLineTypeFile(name, file.get_name())) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }
   }
   else
      acutRelRb(rb);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_CurrentLineType                       <external> */
/*+
  Questa funzione setta il tipolinea corrente.
  Parametri:
  const TCHAR *line;  nome del tipo linea allocato a MAX_LEN_LINETYPENAME
  TCHAR       *old;   nome del tipo linea corrente precedente allocato a
                      MAX_LEN_LINETYPENAME (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentLineType(const TCHAR *line, TCHAR *old)
{
   resbuf   *rb;
   C_STRING file;
   TCHAR    name[MAX_LEN_LINETYPENAME];

   // Memorizza vecchio valore in old
   if (old)
   {
      if ((rb = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("CELTYPE"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTSTR || rb->resval.rstring == NULL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      gsc_strcpy(old, rb->resval.rstring, MAX_LEN_LINETYPENAME); 
      acutRelRb(rb); 
   }

   if (!line) return GS_GOOD;
   gsc_strcpy(name, line, MAX_LEN_LINETYPENAME);
   gsc_alltrim(name);                              
   if (gsc_strlen(name) == 0) return GS_GOOD;

   // case insensitive
   if (gsc_strcmp(name, gsc_msg(23), FALSE) == 0) wcscpy(name, LINETYPE_BYLAYER); // "DALAYER"
   if (gsc_strcmp(name, gsc_msg(24), FALSE) == 0) wcscpy(name, LINETYPE_BYBLOCK); // "DABLOCCO"

   // carica se non è gia stato caricato
   if (gsc_load_linetype(name) == GS_BAD) return GS_BAD;
   
   if ((rb = acutBuildList(RTSTR, name, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("CELTYPE"), rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "line_types" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_line_types_accept(ads_callback_packet *dcl)
{
   TCHAR val[10];
   int   *pos = (int *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("list"), val, 10) != RTNORM) return;
   *pos = _wtoi(val);
   ads_done_dialog(dcl->dialog, DLGOK);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "line_types" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_line_types_list(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_DOUBLE_CLICK) dcl_line_types_accept(dcl);
}

/*********************************************************/
/*.doc gs_ddsellinetype                       <external> */
/*+
  Questa funzione permette la scelta di un tipo linea tramite
  interfaccia a finestra
  Parametro:
  il tipo di linea di default (opzionale)
  
  oppure
  TCHAR *current[MAX_LEN_LINETYPENAME]; tipolinea corrente

  Restituisce una lista del tipo linea scelto.
  es. (<nome> <descrizione> <formato>)
-*/  
/*********************************************************/
int gs_ddsellinetype(void)
{
   TCHAR     current[MAX_LEN_LINETYPENAME] = GS_EMPTYSTR;
   presbuf   arg = acedGetArgs();
   C_RB_LIST ret;

   acedRetNil();

   if (arg && arg->restype == RTSTR) // leggo tipo linea corrente (opzionale)
      gsc_strcpy(current, arg->resval.rstring, MAX_LEN_LINETYPENAME);

   if ((ret << gsc_ddsellinetype(current)) != NULL)
      ret.LspRetList();

   return RTNORM;
}
presbuf gsc_ddsellinetype(const TCHAR *current)
{
   C_2STR_LIST GEOsimLineTypeNameList;
   C_2STR      *pGEOsimLineTypeName;
   TCHAR       strPos[10] = _T("0");
   int         status = DLGOK, dcl_file, pos = 0;
   ads_hdlg    dcl_id;
   C_STRING    Descr, path;

   if (gsc_getGEOsimLType(GEOsimLineTypeNameList) == GS_BAD) return NULL;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("line_types"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return NULL; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW, 0);
   pGEOsimLineTypeName = (C_2STR *) GEOsimLineTypeNameList.get_head();
   while (pGEOsimLineTypeName)
   {
      Descr = pGEOsimLineTypeName->get_name(); // nome
      
      if (current && gsc_strcmp(pGEOsimLineTypeName->get_name(), current, FALSE) == 0) 
         swprintf(strPos, 10, _T("%d"), pos);
      else pos++;
      
      Descr += _T("\t");
      if (pGEOsimLineTypeName->get_name2() != NULL)
         Descr += pGEOsimLineTypeName->get_name2(); // descrizione

      gsc_add_list(Descr);
      
      pGEOsimLineTypeName = (C_2STR *) GEOsimLineTypeNameList.get_next();
   }
   ads_end_list();

   ads_set_tile(dcl_id, _T("list"), strPos);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_line_types_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &pos);
   ads_action_tile(dcl_id, _T("list"),   (CLIENTFUNC) dcl_line_types_list);
   ads_client_data_tile(dcl_id, _T("list"), &pos);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_RB_LIST result;
      
      pGEOsimLineTypeName = (C_2STR *) GEOsimLineTypeNameList.getptr_at(pos + 1); // ordinal position (1 è il primo)
      result << acutBuildList(RTSTR, pGEOsimLineTypeName->get_name(), 0);
      if (pGEOsimLineTypeName->get_name2())
         result += pGEOsimLineTypeName->get_name2();
      else
         result += GS_EMPTYSTR;

      result.ReleaseAllAtDistruction(GS_BAD);

      return result.get_head();
   }
   else return NULL;
}


/*********************************************************/
/*.doc gsc_DefineTxtLineType                  <external> */
/*+
  Questa funzione definisce un tipo di linea con testo.
  Parametri:
  C_STRING &LineType;   Nome del tipo linea
  C_STRING &Txt;        Testo che compone la linea

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gs_DefineTxtLineType(void)
{
   C_STRING LineType, Txt;
   presbuf  arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTSTR) // leggo il nome del tipo linea
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   LineType = arg->resval.rstring;

   if ((arg = arg->rbnext)) // leggo il testo contenuto nel tipo linea
   {
      if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      Txt = arg->resval.rstring;
   }
   else // il testo contenuto nel tipo linea è uguale al nome del tipo linea
      Txt = LineType;

   if (gsc_DefineTxtLineType(LineType, Txt) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}
int gsc_DefineTxtLineType(C_STRING &LineType, C_STRING &Txt)
{
   FILE     *f;
   double   TrattoInv;
   C_STRING Buffer, Dir, FileTemp;
   resbuf   *rb;

   // Esco se già definito
   if ((rb = acdbTblSearch(_T("LTYPE"), LineType.get_name(), 0))) 
      { acutRelRb(rb); return GS_GOOD; }

   Dir = GEOsimAppl::CURRUSRDIR;
   Dir += _T('\\');
   Dir += GEOTEMPDIR;

   if (gsc_get_tmp_filename(Dir.get_name(), _T("GS_LINETYPE"), _T(".LIN"), FileTemp) == GS_BAD)
      return GS_BAD;

   switch (Txt.len())
   {
      case 1:  TrattoInv = -1.0;  break;
      case 2:  TrattoInv = -1.9;  break;
      case 3:  TrattoInv = -2.9;  break;
      case 4:  TrattoInv = -3.9;  break;
      case 5:  TrattoInv = -4.9;  break;
      case 6:  TrattoInv = -5.9;  break;
      case 7:  TrattoInv = -6.9;  break;
      case 8:  TrattoInv = -7.9;  break;
      case 9:  TrattoInv = -8.9;  break;
      case 10: TrattoInv = -9.9;  break;
      case 11: TrattoInv = -10.9; break;
      case 12: TrattoInv = -11.9; break;
      case 13: TrattoInv = -12.9; break;
      case 14: TrattoInv = -13.9; break;
      case 15: TrattoInv = -14.9; break;
      default:
         return GS_BAD;
   }

   if (Txt.at(_T('.')) || Txt.at(_T(','))) TrattoInv -= 0.5;

   Buffer = _T("*");
   Buffer += LineType;
   Buffer += _T(",");
   Buffer += LineType;
   Buffer += _T(" ----");
   Buffer += Txt;
   Buffer += _T("----");
   Buffer += Txt;
   Buffer += _T("----");
   Buffer += Txt;
   Buffer += _T("----");
   Buffer += Txt;
   Buffer += _T("--\n");

   // Apertura file NON in modo UNICODE (i file .LIN sono ancora in ANSI)
   bool IsUnicode = false;
   if ((f = gsc_fopen(FileTemp, _T("a"), MORETESTS, &IsUnicode)) == NULL) return GS_BAD;
   if (fwprintf(f, _T("%s"), Buffer.get_name()) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // Tratto visibil1 di 1.0 seguito da tratto invisibile di 0.25
   Buffer = _T("A,1.0,-0.25,[\"");
   Buffer += Txt;
   // Stile di testo Standard
   // Scala = "S=1.0", Rotazione relativa = "R=0.0", Offset X = "X=0.0", Offset Y = "Y=-0.5"
   Buffer += _T("\",STANDARD,S=1.0,R=0.0,X=0.0,Y=-0.5],");
   if (fwprintf(f, _T("%s%f"), Buffer.get_name(), TrattoInv) < 0)
      { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (gsc_fclose(f) == GS_BAD) return GS_BAD;
   
   AcDbDatabase *pCurrDatabase;
   
   // database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();
   if (pCurrDatabase->loadLineTypeFile(LineType.get_name(),
                                       FileTemp.get_name()) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidLineType; return GS_BAD; }

   gsc_delfile(FileTemp);

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_getLTScaleOnTxtLineType            <external> */
/*+
  Questa funzione restituisce il fattore di scala del tipo linea 
  creato con la funzione gsc_DefineTxtLineType in base alla lunghezza della linea
  affinchè si possa leggere il testo.
  Parametri:
  double ApproxLineLen; Lunghezza del tratto

  Ritorna il fattore di scala del tipo linea.
-*/  
/*********************************************************/
double gsc_getLTScaleOnTxtLineType(double LineLen)
{
   // Imposto la scala del testo in funzione della lunghezza del tratto di linea
   if (LineLen <= 1) return 0.01;
   else if (LineLen <= 10) return 0.1;
   else if (LineLen <= 100) return 1;
   else if (LineLen <= 1000) return 10;
   else if (LineLen <= 10000) return 100;
   else if (LineLen <= 100000) return 1000;

   return 1;
}

/*********************************************************/
/*.doc gsc_getLineTypeId                      <external> */
/*+
  Questa funzione restituisce l'identificativo di un tipolinea.
  Parametri:
  const TCHAR *LineType;               Nome del tipo linea
  AcDbObjectId &LineTypeId;            Identificativo del tipo linea (out)
  AcDbLinetypeTable *pLineTypeTable;   Tabella dei tipi linea (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getLineTypeId(const TCHAR *LineType, AcDbObjectId &LineTypeId,
                      AcDbLinetypeTable *pLineTypeTable)
{
   AcDbLinetypeTable *pMyLineTypeTable;
   Acad::ErrorStatus es;
    
   if (pLineTypeTable) pMyLineTypeTable = pLineTypeTable;
   else
      if (acdbHostApplicationServices()->workingDatabase()->getLinetypeTable(pMyLineTypeTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

   if ((es = pMyLineTypeTable->getAt(LineType, LineTypeId, Adesk::kFalse)) != Acad::eOk)
   {
      if (gsc_strcmp(LineType, gsc_msg(23), FALSE) == 0) // "DALAYER"
         es = pMyLineTypeTable->getAt(LINETYPE_BYLAYER, LineTypeId, Adesk::kFalse);
      else if (gsc_strcmp(LineType, gsc_msg(24), FALSE) == 0) // "DABLOCCO"
         es = pMyLineTypeTable->getAt(LINETYPE_BYBLOCK, LineTypeId, Adesk::kFalse);

      if (es != Acad::eOk)
         if (!pLineTypeTable)
         {
            pMyLineTypeTable->close();
            if (gsc_load_linetype(LineType) == GS_GOOD) // Carico il tipolinea se non ancora caricato
            {
               if (acdbHostApplicationServices()->workingDatabase()->getLinetypeTable(pMyLineTypeTable, AcDb::kForRead) != Acad::eOk)
                  return GS_BAD;
               es = pMyLineTypeTable->getAt(LineType, LineTypeId, Adesk::kFalse);
            }
         }
   }

   if (!pLineTypeTable) pMyLineTypeTable->close();

   if (es != Acad::eOk)
   {
      GS_ERR_COD = eGSInvalidLineType;
      return GS_BAD;
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_lineType                       <external> */
/*+
  Questa funzione setta il tipo linea di un'entità.
  Parametri:
  AcDbEntity *pEnt
  AcDbObjectId &LineTypeId;   Identificativo del tipo linea
  oppure
  ads_name ent;               Oggetto grafico
  const TCHAR *LineType;      Nome del tipo linea
  oppure
  ads_name ent;               Oggetto grafico
  AcDbObjectId &LineTypeId;      Identificativo del tipo linea

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_lineType(AcDbEntity *pEnt, AcDbObjectId &LineTypeId)
{
   if (pEnt->linetypeId() == LineTypeId) return GS_GOOD;

   Acad::ErrorStatus Res = pEnt->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (pEnt->setLinetype(LineTypeId) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;
}
int gsc_set_lineType(AcDbEntity *pEnt, const TCHAR *LineType)
{
   C_STRING entLineType;

   gsc_get_lineType(pEnt, entLineType);
   if (gsc_strcmp(entLineType.get_name(), LineType, FALSE) == 0) return GS_GOOD;

   Acad::ErrorStatus Res = pEnt->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (pEnt->setLinetype(LineType) != Acad::eOk)
   {
      AcDbObjectId LineTypeId;
      if (gsc_getLineTypeId(LineType, LineTypeId) == GS_BAD) return GS_BAD;
      return gsc_set_lineType(pEnt, LineTypeId);
   }

   return GS_GOOD;
}
int gsc_set_lineType(ads_name ent, const TCHAR *LineType)
{
   AcDbObjectId LineTypeId;

   if (gsc_getLineTypeId(LineType, LineTypeId) == GS_BAD) return GS_BAD;
   return gsc_set_lineType(ent, LineTypeId);
}
int gsc_set_lineType(ads_name ent, AcDbObjectId &LineTypeId)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_lineType((AcDbEntity *) pObj, LineTypeId) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


/*-------------------------------------------------------------
   int gsc_get_lineType(ads_name ent, C_STRING &LineType)     
-------------------------------------------------------------*/
int gsc_get_lineType(AcDbEntity *pEnt, C_STRING &LineType)
{
   if (pEnt->linetypeId() == AcDbObjectId::kNull) return GS_BAD;
   LineType.paste(pEnt->linetype());

   return GS_GOOD;
}
int gsc_get_lineType(ads_name ent, C_STRING &LineType)
{
   AcDbObjectId objId;
   AcDbEntity   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_lineType(pObj, LineType) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_lineType(C_SELSET &ss, C_STRING &LineType)
{
   ads_name ent;
   long     i = 0;

   while (ss.entname(i++, ent) == GS_GOOD)
      if (gsc_get_lineType(ent, LineType) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_get_ASCIIFromLineType              <internal> */
/*+
  Questa funzione ottiene la descrizione ASCII di un tipolinea 
  come nel formato in ACAD.LIN.
  Parametri:
  const TCHAR *LineType;               Nome del tipo linea (input)
  C_STRING &ASCII;                     Descrizione ASCII (output)
  AcDbLinetypeTable *pLineTypeTable;   Opzionale (input); tabella dei tipi linea (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_get_ASCIIFromLineType(const TCHAR *LineType, C_STRING &ASCII, AcDbLinetypeTable *pLineTypeTable)
{
   AcDbLinetypeTable       *pMyLineTypeTable = NULL;
   AcDbLinetypeTableRecord *pLineType = NULL;
   AcDbObjectId            shapeStyleId;
   Acad::ErrorStatus       es;
   const TCHAR             *pName;
   int                     i, len, Result = GS_BAD;
   
   if (pLineTypeTable) pMyLineTypeTable = pLineTypeTable;
   else
      if (acdbHostApplicationServices()->workingDatabase()->getLinetypeTable(pMyLineTypeTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

   do
   {
      if ((es = pMyLineTypeTable->getAt(LineType, pLineType, AcDb::kForRead,Adesk::kFalse)) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidLineType; break; }

      // the first item is an asterisk, followed by the name of the linetype, 
      // then a short description with an ASCII representation of the linetype.
      // es. *BORDO,Bordo __ __ . __ __ . __ __ . __ __ . __ __ .
      ASCII = GS_LFSTR;
      ASCII += _T('*');
      ASCII += LineType;
      ASCII += _T(',');
      if (pLineType->comments(pName) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidLineType; break; }
      ASCII += pName;
      ASCII += GS_LFSTR;
      
      // On the second line is where the guts of the linetype are actually defined.
      // Start with an "A", then a comma. 
      // es. A,.5,-.25,.5,-.25,0,-.25
      len = pLineType->numDashes();

      if (pLineType->isScaledToFit())
         ASCII += _T('S');
      else
         ASCII += _T('A');

      for (i = 0; i < len; i++)
      {
         ASCII += _T(',');
         // Next comes a positive numeral that represents the starting dash length
         // or a negative numeral to represent a space segment (or pen up condition)
         ASCII += pLineType->dashLengthAt(i);

         shapeStyleId = pLineType->shapeStyleAt(i);
         if (shapeStyleId != AcDbObjectId::kNull)
         {
            // se si tratta di testo
            if (pLineType->textAt(i, pName) == Acad::eOk)
            {
               // The format for adding text characters in a linetype description is as follows:
               // ["text",textstylename,transform] 
               // where transform can be any series of the following (each preceded by a comma):
               // R=## Relative rotation
               // A=## Absolute rotation
               // U=## Upright rotation
               // S=## Scale
               // X=## X offset
               // Y=## Y offset
               // es. ["HW",STANDARD,S=.1,R=0.0,X=-0.1,Y=-.05]
               AcDbTextStyleTableRecord *pTextStyleTableRecord;

               ASCII += _T(",[\"");
               ASCII += pName;
               ASCII += _T("\",");
               if (acdbOpenObject(pTextStyleTableRecord, shapeStyleId, AcDb::kForRead) != Acad::eOk)
                  { GS_ERR_COD = eGSInvalidLineType; break; }
               pTextStyleTableRecord->getName(pName);
               pTextStyleTableRecord->close();
               ASCII += pName;
            }
            else // si tratta di shape
            {
               // [shapename,shxfilename] or [shapename,shxfilename,transform] 
               // where transform is optional and can be any series of the following (each preceded by a comma):
               // R=## Relative rotation
               // A=## Absolute rotation
               // U=## Upright rotation
               // S=## Scale
               // X=## X offset
               // Y=## Y offset 
               // es. [BAT,ltypeshp.shx,r=180,x=.1,s=.1]
               AcDbTextStyleTableRecord *pTextStyleTableRecord;
               TCHAR                    ShxPathFile[511 + 1]; // vedi manuale acad (funzione "acedFindFile")
               C_INT_STR_LIST           ShapeNameNumbers;
               C_INT_STR                *pShapeNameNumber;
               int                      ShapeNumber;

               if (acdbOpenObject(pTextStyleTableRecord, shapeStyleId, AcDb::kForRead) != Acad::eOk)
                  { GS_ERR_COD = eGSInvalidLineType; break; }

               pTextStyleTableRecord->fileName(pName); // leggo nome file shx
               if (acedFindFile(pName, ShxPathFile) != RTNORM) // ricavo la path completa
                  { GS_ERR_COD = eGSInvalidLineType; break; }
               // leggo i numeri e i nomi delle forme contenute nello shape
               if (gsc_getShapeNameNumbers(ShxPathFile, ShapeNameNumbers) == GS_BAD)
                  break;

               ShapeNumber = pLineType->shapeNumberAt(i);
               if ((pShapeNameNumber = (C_INT_STR *) ShapeNameNumbers.search_key(ShapeNumber)) == NULL)
                  { GS_ERR_COD = eGSInvalidLineType; break; }

               // pTextStyleTableRecord->getName(pName); non usabile perchè vuoto

               ASCII += _T(",[");
               ASCII += pShapeNameNumber->get_name();
               ASCII += _T(',');
               ASCII += pName;
               pTextStyleTableRecord->close();
            }

            if (pLineType->shapeIsUprightAt(i))
               ASCII += _T(",U=");
            else
               if (pLineType->shapeIsUcsOrientedAt(i))
                  ASCII += _T(",A=");
               else
                  ASCII += _T(",R=");
            ASCII += pLineType->shapeRotationAt(i);

            ASCII += _T(",S=");
            ASCII += pLineType->shapeScaleAt(i);

            ASCII += _T(",X=");
            ASCII += pLineType->shapeOffsetAt(i).x;
            ASCII += _T(",Y=");
            ASCII += pLineType->shapeOffsetAt(i).y;
            ASCII += _T(']');
         }
      }

      Result = GS_GOOD;
   }
   while (0);

   if (!pLineTypeTable) pMyLineTypeTable->close();
   if (pLineType) pLineType->close();

   return Result;
}


/*********************************************************/
/*.doc gsc_save_lineType                      <external> */
/*+
  Questa funzione effettua il salvataggio di una definizione di
  tipo linea in un file .lin.
  Parametri:
  const TCHAR *LineType;               Nome del tipo linea
  const TCHAR *PathFileLin;            Path del file .lin
  AcDbLinetypeTable *pLineTypeTable;   Tabella dei tipi linea (default = NULL)
  FILE *f;                             puntatore a file .LIN (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_lineType(const TCHAR *LineType, const TCHAR *PathFileLin, 
                      AcDbLinetypeTable *pLineTypeTable = NULL,
                      FILE *f = NULL)
{
   C_STRING ASCII;
   FILE     *Myf;

   if (gsc_get_ASCIIFromLineType(LineType, ASCII, pLineTypeTable) == GS_BAD) return GS_BAD;

   if (f)
      Myf = f;
   else
   {
      bool IsUnicode = false; // Apertura file NON in modo UNICODE (i file .LIN sono ancora in ANSI)

      if ((Myf = gsc_fopen(PathFileLin, _T("a"), MORETESTS, &IsUnicode)) == NULL) return GS_BAD;
   }
   if (fwprintf(Myf, _T("%s"), ASCII.get_name()) < 0)
      { if (!f) gsc_fclose(Myf); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (!f) return gsc_fclose(Myf);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_save_lineTypes                     <external> */
/*+
  Questa funzione effettua il salvataggio di una lista di 
  definizione di tipi linea in un file .lin
  Parametri:
  C_STR_LIST &lineTypeList;  Lista dei nomi dei tipi di linea
  C_STRING &PathFileLin;     File .lin
  int MsgToVideo;            Flag, se = GS_GOOD stampa a video
                             i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_lineTypes(C_STR_LIST &lineTypeList, C_STRING &PathFileLin, int MsgToVideo)
{
   AcDbLinetypeTable *pLineTypeTable = NULL;
   C_STR             *pLineTypeName = (C_STR *) lineTypeList.get_head();
   C_2STR_LIST       oldLineTypeList;
   C_STR_LIST        savedLineTypeList;
   bool              IsUnicode = false; // Apertura file NON in modo UNICODE (i file .LIN sono ancora in ANSI)
   FILE              *f;

   if (acdbHostApplicationServices()->workingDatabase()->getLinetypeTable(pLineTypeTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   
   // Lettura dei nomi e delle descrizioni dei tipi linea nel file già esistenti
   gsc_getLTypeListFromFile(PathFileLin, oldLineTypeList);

   if ((f = gsc_fopen(PathFileLin, _T("a"), MORETESTS, &IsUnicode)) == NULL)
      { pLineTypeTable->close(); return GS_BAD; }

   while (pLineTypeName)
   {
      if (oldLineTypeList.search_name(pLineTypeName->get_name(), FALSE) == NULL)
      {
         if (gsc_save_lineType(pLineTypeName->get_name(), PathFileLin.get_name(), pLineTypeTable, f) == GS_BAD)
            { pLineTypeTable->close(); gsc_fclose(f); return GS_BAD; }
         savedLineTypeList.add_tail_str(pLineTypeName->get_name());
      }

      pLineTypeName = (C_STR *) lineTypeList.get_next();
   }

   pLineTypeTable->close();
   gsc_fclose(f);

   if (MsgToVideo == GS_GOOD)
   {
      // "\nLe definizioni dei seguenti tipilinea sono state scritte in %s:"
      acutPrintf(gsc_msg(1043), PathFileLin.get_name()); 

      pLineTypeName = (C_STR *) savedLineTypeList.get_head();
      while (pLineTypeName)
      {
         acutPrintf(_T("\n%s"), pLineTypeName->get_name());
         pLineTypeName = (C_STR *) savedLineTypeList.get_next();
      }
      acutPrintf(GS_LFSTR);
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    TIPILINEA    FINE     ///////////////////
//////////////////    GESTIONE    RIEMPIMENTI  INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_getHatchListFromFile               <internal> */
/*+
  Questa funzione restituisce una lista dei nomi e delle descrizioni dei
  riempimenti contenuti in un file .PAT (la lista non è ordinata alfabeticamente).
-*/  
/*********************************************************/
int gsc_getHatchListFromFile(C_STRING &PathFilePat, C_2STR_LIST &hatchList)
{
   C_STRING buffer, name, descr;
   int      i;
   FILE     *file;
   TCHAR    *str;

   hatchList.remove_all();

   // Apertura file NON in modo UNICODE (i file .PAT sono ancora in ANSI)
   bool IsUnicode = false;
   if ((file = gsc_fopen(PathFilePat, _T("rb"), MORETESTS, &IsUnicode)) == NULL)
      return GS_BAD;

   while (gsc_readline(file, buffer, false) != WEOF) // leggo riga
      if (buffer.get_chr(0) == _T('*')) // se primo carattere '*' si tratta di riempimento
      {
         str = buffer.get_name();
         i   = gsc_strsep(str, _T('\0'));
         name = str + 1;
         name.alltrim();

         if (i > 0) // lettura descrizione riempimento
         {
            while (*str != _T('\0')) str++; str++;
            descr = str;
            descr.alltrim();
         }
         else
            descr.clear();

         if (hatchList.add_tail_2str(name.get_name(), descr.get_name()) == GS_BAD)
            { gsc_fclose(file); return GS_BAD; }
      }

   return gsc_fclose(file);
}


/******************************************************************************/
/*.doc gsc_getCurrentGEOsimHatchFilePath                           <external> */
/*+
  Questa funzione restituisce il percorso completo del file contenente le
  definizioni dei riempimenti di GEOsim. Il file si trova nella cartella THM
  della cartella server di GEOsim e si chiama ACAD.PAT se impostato in 
  Autocad il sistema di misurazione inglese (variabile MEASUREMENT=0) oppure
  ACADISO.LIN se impostato in Autocad il sistema di misurazione metrico
  (variabile MEASUREMENT=1)
  Parametri:
  C_STRING &Path;          (output)
  bool     ClientVersion;  Flag che determiona se si tratta del file
                           sul server o la copia locale sul client

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************************************/
int gsc_getCurrentGEOsimHatchFilePath(C_STRING &Path, bool ClientVersion)
{
   resbuf Val;
  
   if (ClientVersion)
      Path = GEOsimAppl::WORKDIR;
   else
   {
      Path = GEOsimAppl::GEODIR;
      Path += _T('\\');
      Path += GEOTHMDIR;
   }
   Path += _T('\\');
   Path += GEOHATCH;
   if (acedGetVar(_T("MEASUREMENT"), &Val) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   // Metrico
   if (Val.restype == RTSHORT && Val.resval.rint == 1) Path += _T("ISO");
   Path += _T(".PAT");

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getgeosimhatch                      <external> */
/*+
  Questa funzione restituisce una lista dei tipi di riempimento.
  ((<nome1> <descrizione1>) ...)
-*/  
/*********************************************************/
int gs_getgeosimhatch(void)
{
   C_2STR_LIST hatchList;
   C_RB_LIST   ret;

   gsc_getGEOsimHatch(hatchList);   
   ret << hatchList.to_rb();
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getGEOsimHatch                     <external> */
/*+
  Questa funzione restituisce una lista dei tipi di riempimento.
  ((<nome1> <descrizione1>) ...)
-*/  
/*********************************************************/
int gsc_getGEOsimHatch(C_2STR_LIST &hatchList)
{
   C_STRING file_hatch;

   // Path del file dei riempimenti copiato sul client
   if (gsc_getCurrentGEOsimHatchFilePath(file_hatch, true) == GS_BAD) return GS_BAD;
   // Se non c'è sul client lo copio perchè serve ad AutoCAD
   if (gsc_path_exist(file_hatch) == GS_BAD)
   {
      if (gsc_refresh_thm() == GS_BAD) return GS_BAD;
      if (gsc_path_exist(file_hatch) == GS_BAD) return GS_BAD;
   }

   // Lettura dei nomi e delle descrizioni dei riempimenti nel file
   if (gsc_getHatchListFromFile(file_hatch, hatchList) == GS_BAD) return GS_BAD;

   // Aggiungo 1 riempimento
   if (hatchList.add_tail_2str(gsc_msg(328), NULL) == GS_BAD) return GS_BAD; // "SOLIDO"

   // ordino la lista degli elementi per nome
   hatchList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_validhatch                         <external> */
/*+
  Questa funzione verifica la validità del riempimento.
  Parametri:
  const TCHAR *hatch;  nome del riempimento

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validhatch(const TCHAR *hatch)
{                      
   size_t      len;
   C_2STR_LIST GEOsimHatchNameList;

   if (!hatch || (len = wcslen(hatch)) == 0) return GS_GOOD;

   if (len >= MAX_LEN_HATCHNAME) { GS_ERR_COD = eGSInvalidHatch; return GS_BAD; }

   // case insensitive
   if (gsc_strcmp(hatch, _T("_SOLID"), FALSE) == 0 ||   // Versione internazionale
       gsc_strcmp(hatch, gsc_msg(328), FALSE) == 0) return GS_GOOD; // "SOLIDO"

   if (gsc_getGEOsimHatch(GEOsimHatchNameList) == GS_BAD)
      { GS_ERR_COD = eGSInvalidHatch; return GS_BAD; }
   if (GEOsimHatchNameList.search_name(hatch, FALSE) == NULL)  // case insensitive
      { GS_ERR_COD = eGSInvalidHatch; return GS_BAD; }

   return GS_GOOD;
}  
      

/*********************************************************/
/*.doc gsc_printGEOsimHatchList               <internal> */
/*+
  Questa funzione stampa la lista dei riempimenti di GEOsim.
  (<nome1> <descrizione1>) ...

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_printGEOsimHatchList(void)
{
   C_2STR_LIST   hatchList;
   C_2STR       *pHatch;
   int           cont = 0, type;
   struct resbuf result;

   acutPrintf(gsc_msg(606));                       // "\nRiempimenti:\n"
   if (gsc_getGEOsimHatch(hatchList) == GS_BAD) return GS_BAD;
   
   pHatch = (C_2STR *) hatchList.get_head();
   while (pHatch)
   {
      acutPrintf(_T("\n  %-15s"), pHatch->get_name()); // nome riempimento
      if (pHatch->get_name2())
         acutPrintf(_T(" - %-50s"), pHatch->get_name2());    // descrizione riempimento
      
      if ((++cont) % DISPLAY_LIST_STEP == 0)
      { 
         acutPrintf(gsc_msg(162));  // "\nPremi un tasto per continuare" 
         if (ads_grread(10, &type, &result) != RTNORM) break;
      }
      
      pHatch = (C_2STR *) hatchList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_CurrentHatch                   <external> */
/*+
  Questa funzione setta il riempimento corrente.
  Parametri:
  const TCHAR *Hatch;  nome del riempimento allocato a MAX_LEN_HATCHNAME
  TCHAR       *old;    nome del riempimento corrente precedente allocato a
                       MAX_LEN_HATCHNAME (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentHatch(const TCHAR *Hatch, TCHAR *old)
{
   resbuf *rb;
   TCHAR  name[MAX_LEN_HATCHNAME];

   // Memorizza vecchio valore in old
   if (old)
   {
      if ((rb = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("HPNAME"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTSTR || rb->resval.rstring == NULL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      gsc_strcpy(old, rb->resval.rstring, MAX_LEN_HATCHNAME); 
      acutRelRb(rb); 
   }

   if (!Hatch) return GS_GOOD;
   gsc_strcpy(name, Hatch, MAX_LEN_HATCHNAME);
   gsc_alltrim(name);                              
   if (gsc_strlen(name) == 0) return GS_GOOD;

   if ((rb = acutBuildList(RTSTR, name, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }  
   if (acedSetVar(_T("HPNAME"), rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getCurrAcadAUNITS                  <external> */
/*+
  Questa funzione legge il sistema di misurazione degli angoli
  corrente di Autocad (vedi enum RotationMeasureUnitsEnum).
  In caso di errore ritorna GSNoneRotationUnit.
-*/  
/*********************************************************/
enum RotationMeasureUnitsEnum gsc_getCurrAcadAUNITS(void)
{
   resbuf rb;
   int    AUnit;

   // Leggo unità di misura delle rotazioni
   if (acedGetVar(_T("AUNITS"), &rb) != RTNORM)
      { GS_ERR_COD = eGSVarNotDef; return GSNoneRotationUnit; }
   if (gsc_rb2Int(&rb, &AUnit) == GS_BAD)
      { GS_ERR_COD = eGSVarNotDef; return GSNoneRotationUnit; }

   switch (AUnit)
   {
      case 0: // gradi decimali
         return GSCounterClockwiseDegreeUnit;
      case 3: // radianti
         return GSCounterClockwiseRadiantUnit;
   }

   return GSNoneRotationUnit;
}


/*********************************************************/
/*.doc gsc_set_CurrentHatchRotation           <external> */
/*+
  Questa funzione setta la rotazione corrente del riempimento
  (in gradi).
  Parametri:
  const double value;  rotazione del riempimento
  double       *old;   rotazione corrente del riempimento

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentHatchRotation(const double value, double *old)
{
   resbuf rb;
   double Rot;
   enum RotationMeasureUnitsEnum CurrAcadAUNITS;

   // Leggo unità di misura delle rotazioni
   if ((CurrAcadAUNITS = gsc_getCurrAcadAUNITS()) == GSNoneRotationUnit) return GS_BAD;
   // Converto in gradi
   switch (CurrAcadAUNITS)
   {
      case GSCounterClockwiseDegreeUnit:
         Rot = value;
         break;
      case GSCounterClockwiseRadiantUnit:
         Rot = gsc_grd2rad(value);
         break;
   }

   // Memorizza vecchio valore in old
   if (old)
   {
      if (acedGetVar(_T("HPANG"), &rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      if (gsc_rb2Dbl(&rb, old) == GS_BAD)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      switch (CurrAcadAUNITS)
      {
         case GSCounterClockwiseRadiantUnit:
            *old = gsc_rad2grd(*old);
            break;
      }
   }
   gsc_RbSubst(&rb, Rot);
   if (acedSetVar(_T("HPANG"), &rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_set_CurrentHatchScale                     <external> */
/*+
  Questa funzione setta la scala corrente del riempimento.
  Parametri:
  const double value;  scala del riempimento
  double       *old;   scala corrente del riempimento

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentHatchScale(const double value, double *old)
{
   resbuf rb;

   // Memorizza vecchio valore in old
   if (old)
   {
      if (acedGetVar(_T("HPSCALE"), &rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
      if (gsc_rb2Dbl(&rb, old) == GS_BAD)
         { GS_ERR_COD = eGSVarNotDef; return GS_BAD; }
   }
   gsc_RbSubst(&rb, value);
   if (acedSetVar(_T("HPSCALE"), &rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "hatches" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_hatches_accept(ads_callback_packet *dcl)
{
   TCHAR val[10];
   int   *pos = (int *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("list"), val, 10) != RTNORM) return;
   *pos = _wtoi(val);
   ads_done_dialog(dcl->dialog, DLGOK);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "hatches" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_hatches_list(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_DOUBLE_CLICK) dcl_hatches_accept(dcl);
}

/*********************************************************/
/*.doc gs_ddselhatch                          <external> */
/*+
  Questa funzione permette la scelta di un riempimento tramite
  interfaccia a finestra
  Parametro:
  il riempimento di default (opzionale)
  
  oppure
  const TCHAR *current[MAX_LEN_HATCHNAME]; riempimento corrente

  Restituisce una lista del riempimento scelto.
  es. (<nome> <descrizione>)
-*/  
/*********************************************************/
int gs_ddselhatch(void)
{
   TCHAR     current[MAX_LEN_HATCHNAME] = GS_EMPTYSTR;
   presbuf   arg = acedGetArgs();
   C_RB_LIST ret;

   acedRetNil();

   if (arg && arg->restype == RTSTR) // leggo tipo linea corrente (opzionale)
      gsc_strcpy(current, arg->resval.rstring, MAX_LEN_HATCHNAME);

   if ((ret << gsc_ddselhatch(current)) != NULL)
      ret.LspRetList();

   return RTNORM;
}
presbuf gsc_ddselhatch(const TCHAR *current)
{
   C_2STR_LIST GEOsimHatchNameList;
   C_2STR      *pGEOsimHatchName;
   TCHAR       strPos[10] = _T("0");
   int         status = DLGOK, dcl_file, pos = 0;
   ads_hdlg    dcl_id;
   C_STRING    Descr, path;

   if (gsc_getGEOsimHatch(GEOsimHatchNameList) == GS_BAD) return NULL;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("hatches"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return NULL; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW,0);
   pGEOsimHatchName = (C_2STR *) GEOsimHatchNameList.get_head();
   while (pGEOsimHatchName)
   {
      Descr = pGEOsimHatchName->get_name();  // nome
      
      if (current && gsc_strcmp(pGEOsimHatchName->get_name(), current, FALSE) == 0)
         swprintf(strPos, 10, _T("%d"), pos);
      else pos++;
      
      Descr += _T("\t");
      if (pGEOsimHatchName->get_name2() != NULL)
         Descr += pGEOsimHatchName->get_name2(); // descrizione

      gsc_add_list(Descr);

      pGEOsimHatchName = (C_2STR *) GEOsimHatchNameList.get_next();
   }
   ads_end_list();

   ads_set_tile(dcl_id, _T("list"), strPos);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_hatches_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &pos);
   ads_action_tile(dcl_id, _T("list"),   (CLIENTFUNC) dcl_hatches_list);
   ads_client_data_tile(dcl_id, _T("list"), &pos);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_RB_LIST result;

      pGEOsimHatchName = (C_2STR *) GEOsimHatchNameList.getptr_at(pos + 1); // ordinal position (1 è il primo)
      result << acutBuildList(RTSTR, pGEOsimHatchName->get_name(), 0);
      if (pGEOsimHatchName->get_name2())
         result += pGEOsimHatchName->get_name2();
      else
         result += GS_EMPTYSTR;

      result.ReleaseAllAtDistruction(GS_BAD);

      return result.get_head();
   }
   else return NULL;
}

/*****************************************************************************/
/*.doc gsc_set_hatch                                                         */
/*+                                                                       
  Cambia il riempimento con un altro ed opzionalmente setta rotazione, scala, 
  colore, layer.
  Parametri:
  ads_name ent;         entità riempimento
  const TCHAR *hatch;   Opzionale; nome riempimento (default = NULL)
  double *Rot;          Opzionale; Rotazione in gradi (default = NULL)
  double *Scale;        Opzionale; Scala (default = NULL)
  C_COLOR *Color;           Opzionale; colore (default = NULL)
  const TCHAR *Layer;   Opzionale; Layer (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/
/*****************************************************************************/
int gsc_set_hatch(AcDbEntity *pEnt, const TCHAR *hatch, double *Rot, double *Scale,
                  C_COLOR *Color, const TCHAR *Layer)
{
   AcDbHatch *pHatch;
   C_STRING  MyPattern, PrevPattern;
   bool      IsDifferent = false, MPolygonToClose = false, HatchToClose = false;
   double    PatternAngle;
   AcCmColor MyColor;

   // Se non è un riempimento o un poligono
   if (pEnt->isKindOf(AcDbHatch::desc())) pHatch = (AcDbHatch *) pEnt;
   else if (pEnt->isKindOf(AcDbMPolygon::desc())) pHatch = ((AcDbMPolygon *) pEnt)->hatch();
   else return GS_GOOD;

   if (hatch)
   {
      // Set hatch pattern to SolidFill type
      if (gsc_strcmp(hatch, _T("_SOLID"), FALSE) == 0 || // Versione internazionale
         gsc_strcmp(hatch, gsc_msg(328), FALSE) == 0) // "SOLIDO"
         MyPattern = _T("SOLID");
      else
         MyPattern = hatch;

      PrevPattern = pHatch->patternName();
      if (MyPattern.comp(PrevPattern, FALSE) != 0) IsDifferent = true;
   }
   if (Rot)
   {
      PatternAngle = gsc_grd2rad(*Rot);
      if (pHatch->patternAngle() != PatternAngle) IsDifferent = true;
   }
   if (Scale)
      if (pHatch->patternScale() != *Scale) IsDifferent = true;
   if (Color)
   {
      gsc_C_COLOR_to_AcCmColor(*Color, MyColor);
      if (pHatch->color() != MyColor) IsDifferent = true;
   }

   if (Layer)
   {
      C_STRING layerName;

      gsc_getLayer(pHatch, layerName);
      if (gsc_strcmp(Layer, layerName.get_name()) != 0) IsDifferent = true;
   }

   // Non c'è bisogno di cambiare niente
   if (!IsDifferent) return GS_GOOD;

   Acad::ErrorStatus Res;

   if (pEnt->isKindOf(AcDbMPolygon::desc()))
   {
      if ((Res = pEnt->upgradeOpen()) == Acad::eOk) MPolygonToClose = true;
      else if ( Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }

   if ((Res = pHatch->upgradeOpen()) == Acad::eOk) HatchToClose = true;
   else if (Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; if (MPolygonToClose) pEnt->close(); return GS_BAD; }

   // Se è stato specificato il riempimento
   if (hatch)
   {
      if (pHatch->setPattern(AcDbHatch::kPreDefined, MyPattern.get_name()) != Acad::eOk)
         { if (HatchToClose) pHatch->close(); if (MPolygonToClose) pEnt->close(); return GS_BAD; }
      // Valuto se il riempimento va bene oppure è troppo denso
      if (pHatch->evaluateHatch(true) != Acad::eOk)
      {
         pHatch->setPattern(AcDbHatch::kPreDefined, PrevPattern.get_name());
         if (HatchToClose) pHatch->close(); if (MPolygonToClose) pEnt->close();
         return GS_CAN;
      }
   }

   // Se è stata specificata la rotazione
   if (Rot)
      pHatch->setPatternAngle(PatternAngle);

   // Se è stata specificata la scala
   if (Scale)
      pHatch->setPatternScale(*Scale);

   // Se è stato specificato il colore
   if (Color)
      pHatch->setColor(MyColor);

   // Se è stato specificato il layer
   if (Layer)
      pHatch->setLayer(Layer);

   // Se poligono viene modificata una sottoentità hatch
   if (pEnt->isKindOf(AcDbMPolygon::desc()))
      pEnt->recordGraphicsModified(true); // c'è bisogno di ridisegnare l'intero poligono (regen alla chiusura)

   if (HatchToClose) pHatch->close(); if (MPolygonToClose) pEnt->close();

   return GS_GOOD;
}
int gsc_set_hatch(ads_name ent, const TCHAR *hatch, double *Rot, double *Scale, 
                  C_COLOR *Color, const TCHAR *Layer)
{
   AcDbObject   *pObj;
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }  
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidEED; return GS_BAD; }
   int Res = gsc_set_hatch((AcDbEntity *) pObj, hatch, Rot, Scale, Color, Layer);
   if (pObj->close() != Acad::eOk) return GS_BAD;

   return Res;
}


/*********************************************************/
/*.doc gsc_get_ASCIIFromHatch                 <internal> */
/*+
  Questa funzione ottiene la descrizione ASCII di un riempimento 
  come nel formato in ACAD.PAT.
  Parametri:
  AcDbHatch *pHatch;   Puntatore al riempimento (input)
  C_STRING &ASCII;     Descrizione ASCII (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_get_ASCIIFromHatch(AcDbHatch *pHatch, C_STRING &ASCII)
{
   int             i, len, dashesLen;
   double          angle, baseX, baseY, offsetX, offsetY, dist;
   AcGeDoubleArray dashes;
   C_STRING        StrNum;
   
   len = pHatch->numPatternDefinitions();

   // It has a header line with a name, which begins with an asterisk and is no more than 31 characters long,
   // and an optional description
   // es. *ANSI31, ANSI Iron, Brick, Stone masonry
   // The pattern name on the first line, *ANSI31, is followed by a description: ANSI Iron, Brick, Stone masonry
   ASCII = GS_LFSTR;
   ASCII += _T('*');
   ASCII += pHatch->patternName();
   ASCII += GS_LFSTR;

   // It also has one or more line descriptors of the following form:
   // angle, x-origin,y-origin, delta-x,delta-y,dash-1,dash-2, ...
   // es. 45, 0,0, 0,.125 
   // This definition specifies a line drawn at an angle of 45 degrees, 
   // that the first line of the family of hatch lines is to pass through the drawing origin (0,0),
   // and that the spacing between hatch lines of the family is to be 0.125 drawing units. 

   for (i = 0; i < len; i++)
   {
      if (pHatch->getPatternDefinitionAt(i, angle, baseX, baseY, offsetX, offsetY, dashes) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidHatch; return GS_BAD; }

      // Calcolo distanza y con pitagora
      dist = pow(fabs((offsetX * offsetX) + (offsetY * offsetY)), (double) 1/2); // radice quadrata
      dist = dist / pHatch->patternScale();
      if (angle == PI/2 || angle == PI*3/4) // linea verticale
      {
         offsetX = dist;
         offsetY = 0;
      }
      else
      {
         offsetX = 0;
         offsetY = dist;
      }

      StrNum.paste(gsc_tostring(gsc_rad2grd(angle - pHatch->patternAngle()), 50));
      ASCII += StrNum;
      ASCII += _T(", ");
      StrNum.paste(gsc_tostring(baseX, 50));
      ASCII += StrNum;
      ASCII += _T(", ");
      StrNum.paste(gsc_tostring(baseY, 50));
      ASCII += StrNum;
      ASCII += _T(", ");
      StrNum.paste(gsc_tostring(offsetX, 50));
      ASCII += StrNum;
      ASCII += _T(", ");
      StrNum.paste(gsc_tostring(offsetY, 50));
      ASCII += StrNum;

      dashesLen = dashes.length();
      for (int j = 0; j < dashesLen; j++)
      {
         ASCII += dashes.at(j);
         ASCII += _T(',');
      }
      ASCII += GS_LFSTR;
   }
   ASCII += GS_LFSTR;

   return GS_GOOD;
}
int gsc_get_ASCIIFromHatch(C_SELSET &ss, C_STRING &ASCII)
{
   ads_name ent;
   long     i = 0;

   while (ss.entname(i++, ent) == GS_GOOD)
      if (gsc_get_ASCIIFromHatch(ent, ASCII) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}
int gsc_get_ASCIIFromHatch(ads_name ent, C_STRING &ASCII)
{
   AcDbObjectId objId;
   AcDbEntity   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_ASCIIFromHatch(pObj, ASCII) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_get_ASCIIFromHatch(AcDbEntity *pEnt, C_STRING &ASCII)
{
   if (pEnt->isKindOf(AcDbHatch::desc()))
      return gsc_get_ASCIIFromHatch((AcDbHatch *) pEnt, ASCII);
   else
   if (pEnt->isKindOf(AcDbMPolygon::desc()))
      return gsc_get_ASCIIFromHatch(((AcDbMPolygon *) pEnt)->hatch(), ASCII);

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_save_hatch                         <external> */
/*+
  Questa funzione effettua il salvataggio di una definizione di
  riempimento in un file .pat.
  Parametri:
  AcDbHatch *pHatch;                   Puntatore al riempimto (input)
  const TCHAR *PathFilePat;            Path del file .lin
  FILE *f;                             puntatore a file .LIN (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_hatch(AcDbHatch *pHatch, const TCHAR *PathFilePat, 
                   FILE *f = NULL)
{
   C_STRING ASCII;
   FILE     *Myf;

   if (gsc_get_ASCIIFromHatch(pHatch, ASCII) == GS_BAD) return GS_BAD;

   if (f)
      Myf = f;
   else
   {
      bool IsUnicode = false; // Apertura file NON in modo UNICODE (i file .PAT sono ancora in ANSI)

      if ((Myf = gsc_fopen(PathFilePat, _T("a"), MORETESTS, &IsUnicode)) == NULL) return GS_BAD;
   }
   if (fwprintf(Myf, _T("%s"), ASCII.get_name()) < 0)
      { if (!f) gsc_fclose(Myf); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (!f) return gsc_fclose(Myf);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_save_hatches                       <external> */
/*+
  Questa funzione effettua il salvataggio di una lista di 
  riempimenti in un file .pat
  Parametri:
  C_2STR_LIST &HatchDescrList;  Lista dei nomi e descrizione ASCII dei riempimenti
  C_STRING &PathFilePat;        File .pat
  int MsgToVideo;               Flag, se = GS_GOOD stampa a video
                                i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_hatches(C_2STR_LIST &HatchDescrList, C_STRING &PathFilePat, int MsgToVideo)
{
   C_2STR *pHatchDescr = (C_2STR *) HatchDescrList.get_head();
   bool   IsUnicode = false; // Apertura file NON in modo UNICODE (i file .LIN sono ancora in ANSI)
   FILE   *f;

   if ((f = gsc_fopen(PathFilePat, _T("a"), MORETESTS, &IsUnicode)) == NULL) return GS_BAD;

   while (pHatchDescr)
   {
      if (fwprintf(f, _T("%s"), pHatchDescr->get_name2()) < 0)
         { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      pHatchDescr = (C_2STR *) HatchDescrList.get_next();
   }

   gsc_fclose(f);

   if (MsgToVideo == GS_GOOD)
   {
      // "\nLe definizioni dei seguenti riempimenti sono state scritte in %s:"
      acutPrintf(gsc_msg(1044), PathFilePat.get_name()); 

      pHatchDescr = (C_2STR *) HatchDescrList.get_head();
      while (pHatchDescr)
      {
         acutPrintf(_T("\n%s"), pHatchDescr->get_name());
         pHatchDescr = (C_2STR *) HatchDescrList.get_next();
      }
      acutPrintf(GS_LFSTR);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getInfoHatchSS                     <external> */
/*+                                                                       
  La funzione verifica se nel gruppo di selezione esiste un riempimento
  e restituisce alcune informazioni del primo che incontra (anche mpolygon).
  Parametri:
  ads_name SelSet;      Gruppo di selezione
  TCHAR    *Hatch;      nome riempimento (se usato va allocato a MAX_LEN_HATCHNAME) 
  double   *Scale;      scala
  double   *Rotation;   rotazione in radianti
  TCHAR    *Layer;      nome piano       (se usato va allocato a MAX_LEN_LAYERNAME)
  C_COLOR  *Color;      colore

  Restituisce GS_GOOD se esiste almeno un riempimento altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_getInfoHatchSS(C_SELSET &SelSet, TCHAR *Hatch, double *Scale, double *Rotation,
                       TCHAR *Layer, C_COLOR *Color)
{
   ads_name ss;
   SelSet.get_selection(ss);
   return gsc_getInfoHatchSS(ss, Hatch, Scale, Rotation, Layer, Color);
}
int gsc_getInfoHatchSS(ads_name SelSet, TCHAR *Hatch, double *Scale, double *Rotation,
                       TCHAR *Layer, C_COLOR *Color)
{
   ads_name ent;
   long     i = 0;

   if (!SelSet || ads_name_nil(SelSet))
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

	while (acedSSName(SelSet, i++, ent) == RTNORM)
      if (gsc_getInfoHatch(ent, Hatch, Scale, Rotation, Layer, Color) == GS_GOOD)
         return GS_GOOD;

   return GS_BAD;
}
int gsc_getInfoHatch(ads_name ent, TCHAR *Hatch, double *Scale, double *Rotation,
                     TCHAR *Layer, C_COLOR *Color)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_getInfoHatch((AcDbEntity *) pObj, Hatch, Scale, Rotation,
                       Layer, Color) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}
int gsc_getInfoHatch(AcDbEntity *pEnt, TCHAR *Hatch, double *Scale, double *Rotation,
                     TCHAR *Layer, C_COLOR *Color)
{
   if (pEnt->isKindOf(AcDbHatch::desc()))
   {
      if (Hatch) // nome riempimento
         gsc_strcpy(Hatch, ((AcDbHatch *) pEnt)->patternName(), MAX_LEN_HATCHNAME);
      if (Scale) // fattore di scala
         *Scale = ((AcDbHatch *) pEnt)->patternScale();
      if (Rotation) // rotazione
         *Rotation = ((AcDbHatch *) pEnt)->patternAngle();
   }
   else
   if (pEnt->isKindOf(AcDbSolid::desc()))
   {
      if (Hatch) // nome riempimento
         gsc_strcpy(Hatch, _T("SOLID"), MAX_LEN_HATCHNAME);
      if (Scale) // fattore di scala
         *Scale = 1.0;
      if (Rotation) // rotazione
         *Rotation = 0.0;
   }
   else
   if (pEnt->isKindOf(AcDbMPolygon::desc()))
   {
      AcDbHatch *pHatch = ((AcDbMPolygon *) pEnt)->hatch();
      if (pHatch)
      {
         if (gsc_getInfoHatch(pHatch, Hatch, Scale, Rotation, Layer, Color) != GS_GOOD)
            return GS_BAD;
         // poichè il riempimento di un mpolygon non restituisce valore per quanto riguarda
         // il layer allora lo eredito dall'mpolygon stesso
         if (Layer)
         {
            C_STRING layerName;
                  
            gsc_getLayer(pEnt, layerName);
            gsc_strcpy(Layer, layerName.get_name(), MAX_LEN_LAYERNAME);
         }

         return GS_GOOD;
      }
   }
   else // tipologia non supportata
      return GS_BAD;

   if (Layer) // nome piano
   {
      C_STRING layerName;
                  
      gsc_getLayer(pEnt, layerName);
      gsc_strcpy(Layer, layerName.get_name(), MAX_LEN_LAYERNAME);
   }
   if (Color) // colore
      gsc_get_color(pEnt, *Color);

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    RIEMPIMENTI  FINE     ///////////////////
//////////////////    GESTIONE    BLOCCHI      INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gs_LoadGEOsimThm                       <external> */
/*+
  Questa funzione LISP carica le definizioni dei tematismi di GEOsim dal
  file GEOsim "GEOTHM.DWG" e dai files utente opzionali che 
  sono tutti i DWG che iniziano per "USRTHM".
-*/  
/*********************************************************/
int gs_LoadGEOsimThm(void)
{
   C_STRING   Path;
   C_STR_LIST FileNameList;

   acedRetNil();

   // file standard di GEOsim
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOTHMDIR;
   Path += _T('\\');
   Path += GEOTHM;
   if (gsc_path_exist(Path) == GS_BAD) // il file deve esistere
      { GS_ERR_COD = eGSThmNotFound; return RTERROR; }
   acutPrintf(gsc_msg(301));     // "\nCaricamento tematismi GEOsim...\n"

   if (gsc_LoadThmFromDwg(Path.get_name()) == GS_BAD) return RTERROR;

   // files utente opzionali (tutti i files DWG che iniziano per "USRTHM")
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOTHMDIR;
   Path += _T('\\');
   Path += USRTHMS; 
   if (gsc_adir(Path.get_name(), &FileNameList, NULL, NULL, NULL, true) > 0)
   {
      C_STR *pFileName = (C_STR *) FileNameList.get_head();

      acutPrintf(gsc_msg(312));     // "\nCaricamento tematismi utente..."
      while (pFileName)
      {
         Path = GEOsimAppl::GEODIR;
         Path += _T('\\');
         Path += GEOTHMDIR;
         Path += _T('\\');
         Path += pFileName->get_name();
         if (gsc_LoadThmFromDwg(Path.get_name()) == GS_BAD) return RTERROR;

         pFileName = (C_STR *) FileNameList.get_next();
      }
   }

   // GEOsim da per scontato che al termine di questa funzione il disegno corrente sia cambiato
   // per forzare dbmod al valore diverso da 0 inserisco un punto e lo cancello
   ads_point Pt;
   AcDbPoint *pPoint;
   int       UndoState = gsc_SetUndoRecording(FALSE);

   ads_point_clear(Pt);
   if (gsc_insert_point(Pt, NULL, NULL, 0.0, NULL, &pPoint) == GS_GOOD)
      gsc_EraseEnt(pPoint->objectId());
   gsc_SetUndoRecording(UndoState);

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_getGEOsimBlockList                  <external> */
/*+
  Questa funzione restituisce una lista con i nomi dei blocchi di GEOsim.
  (<nome1> <nome2> ...)
-*/  
/*********************************************************/
int gs_getGEOsimBlockList(void)
{
   C_STR_LIST BlockNameList;
   C_RB_LIST  ret;

   if (gsc_getGEOsimBlockList(BlockNameList) == GS_BAD) acedRetNil();

   // ordino la lista degli elementi per nome
   BlockNameList.sort_name(TRUE, TRUE); // sensitive e ascending
   if ((ret << BlockNameList.to_rb()) == NULL) acedRetNil();
   else ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getGEOsimBlockList                 <external> */
/*+
  Questa funzione restituisce una lista con i nomi dei blocchi di GEOsim.
  Parametri:
  C_STR_LIST &BlockNameList;  Lista dei nomi dei blocchi (out)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getGEOsimBlockList(C_STR_LIST &BlockNameList)
{
   C_RB_LIST  DescrBlk;
   C_STRING   Path;
   C_STR_LIST FileNameList;
   C_STR_LIST PartialBlockNameList;

   // controllo che sia caricato almeno il blocco GEOSIM
   if ((DescrBlk << acdbTblSearch(_T("BLOCK"), _T("GEOSIM"), 0)) == NULL) return GS_BAD;
   // controllo che sia caricato almeno il blocco $T
   if ((DescrBlk << acdbTblSearch(_T("BLOCK"), _T("$T"), 0)) == NULL) return GS_BAD;

   // file standard di GEOsim che deve esistere
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOTHMDIR;
   Path += _T('\\');
   Path += GEOTHM;
   if (gsc_getBlockList(BlockNameList, Path.get_name()) == GS_BAD)
      { GS_ERR_COD = eGSThmNotFound; return GS_BAD; }

   // files utente opzionali (tutti i files DWG che iniziano per "USRTHM")
   Path = GEOsimAppl::GEODIR;
   Path += _T('\\');
   Path += GEOTHMDIR;
   Path += _T('\\');
   Path += USRTHMS; 
   if (gsc_adir(Path.get_name(), &FileNameList, NULL, NULL, NULL, true) > 0)
   {
      C_STR *pFileName = (C_STR *) FileNameList.get_head();

      while (pFileName)
      {
         Path = GEOsimAppl::GEODIR;
         Path += _T('\\');
         Path += GEOTHMDIR;
         Path += _T('\\');
         Path += pFileName->get_name();
         if (gsc_getBlockList(PartialBlockNameList, Path.get_name()) == GS_BAD)
            { GS_ERR_COD = eGSThmNotFound; return GS_BAD; }
         // Aggiungo la lista parziale a quella definitiva (senza duplicare i nomi)
         if (BlockNameList.add_tail_unique(PartialBlockNameList, FALSE) == GS_BAD)
            return GS_BAD;

         pFileName = (C_STR *) FileNameList.get_next();
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getBlockList                        <external> */
/*+
  Questa funzione restituisce una lista con i nomi dei blocchi 
  attualmente definiti o definiti in una disegno.
  Parametri:
  <Path di un DWG>

  Restituisce una lista (<nome1> <nome2> ...) in caso di successo altrimenti nil
-*/  
/*********************************************************/
int gs_getBlockList(void)
{
   presbuf    arg = acedGetArgs();
   C_STRING   DwgPath;
   C_STR_LIST BlockNameList;
   C_RB_LIST  ret;

   if (arg && arg->restype == RTSTR) DwgPath = arg->resval.rstring;

   if (gsc_getBlockList(BlockNameList, DwgPath.get_name()) == GS_BAD) acedRetNil();

   if ((ret << BlockNameList.to_rb()) == NULL) acedRetNil();
   else ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getBlockList                       <external> */
/*+
  Questa funzione restituisce una lista con i nomi dei blocchi di GEOsim.
  Parametri:
  C_STR_LIST &BlockNameList;  Lista dei nomi dei blocchi (out)
  const TCHAR *DwgPath;       Path del file DWG, se = NULL si intende la
                              sessione corrente di Autocad (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getBlockList(C_STR_LIST &BlockNameList, const TCHAR *DwgPath)
{
   BlockNameList.remove_all();

   if (!DwgPath) // leggo i blocchi dalla sessione corrente di Autocad
   {
      C_RB_LIST DescrBlk;
      presbuf   p;

      DescrBlk << ads_tblnext(_T("BLOCK"), 1); // prendo il primo blocco
      
      while (DescrBlk.get_head() != NULL)
      {
         p = DescrBlk.SearchType(2);

         // se non è un blocco nascosto
         // e non è un blocco $T
         if (p->resval.rstring[0] != _T('*') && 
             gsc_strcmp(p->resval.rstring, _T("$T")) != 0)
            if (BlockNameList.add_tail_str(p->resval.rstring) == GS_BAD) return GS_BAD;

         DescrBlk << ads_tblnext(_T("BLOCK"), 0); // prendo il successivo blocco
      }
   }
   else // leggo i blocchi dal DWG indicato
   {
      C_STRING               tmp_path, ext;
      AcDbDatabase           extDatabase(Adesk::kFalse);
      AcDbBlockTable         *pBlockTable;
      AcDbBlockTableIterator *pBlockTableIt;
      AcDbObjectId           ObjId; 
      AcDbBlockTableRecord   *pBlockTableRec;
      const TCHAR            *pName;

      tmp_path = DwgPath;
      // Controlla Correttezza Path
      if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione la aggiungo
         tmp_path += _T(".DWG");

      // leggo il dwg
      if (extDatabase.readDwgFile(tmp_path.get_name()) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

      // Lettura dei blocchi nel dwg
      if (extDatabase.getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;
      if (pBlockTable->newIterator(pBlockTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
         { pBlockTable->close(); return GS_BAD; }
      while (!pBlockTableIt->done())
      {
         if (pBlockTableIt->getRecordId(ObjId) != Acad::eOk)
            { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
         if (acdbOpenAcDbObject((AcDbObject *&) pBlockTableRec, ObjId, AcDb::kForRead) != Acad::eOk)
            { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
         pBlockTableRec->getName(pName);
         pBlockTableRec->close();

         // se non è un blocco nascosto
         if (pName[0] != _T('*'))
            if (BlockNameList.add_tail_str(pName) == GS_BAD) return GS_BAD;

         pBlockTableIt->step();
      }
      delete pBlockTableIt;
      pBlockTable->close();
   }

   // ordino la lista degli elementi per nome
   BlockNameList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getBlockListWithAttributes         <external> */
/*+
  Questa funzione restituisce una lista con i nomi dei blocchi 
  del disegno corrente aventi degli attributi.
  Parametri:
  C_STR_LIST &BlockNameList;

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gs_getBlockListWithAttributes(void)
{
   C_STR_LIST BlockList;

   if (gsc_getBlockListWithAttributes(BlockList) == GS_BAD ||
       BlockList.get_count() == 0)
      acedRetNil();
   else
   {
      C_RB_LIST ret;

      ret << BlockList.to_rb();
      ret.LspRetList();
   }

   return RTNORM;
}
int gsc_getBlockListWithAttributes(C_STR_LIST &BlockNameList)
{
   AcDbDatabase           *pCurrDatabase;
   AcDbBlockTable         *pBlockTable;
   AcDbBlockTableIterator *pBlockTableIt;
   AcDbObjectId           ObjId; 
   AcDbBlockTableRecord   *pBlockTableRec;
   const TCHAR            *pName;

   BlockNameList.remove_all();
   // database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

   // Lettura dei blocchi
   if (pCurrDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->newIterator(pBlockTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pBlockTable->close(); return GS_BAD; }
   while (!pBlockTableIt->done())
   {
      if (pBlockTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
      if (acdbOpenAcDbObject((AcDbObject *&) pBlockTableRec, ObjId, AcDb::kForRead) != Acad::eOk)
         { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
      pBlockTableRec->getName(pName);

      if (gsc_isBlockWithAttributes(pName, pBlockTableRec) == GS_GOOD)
         BlockNameList.add_tail(new C_STR(pName));

      pBlockTableRec->close();
      pBlockTableIt->step();
   }
   delete pBlockTableIt;
   pBlockTable->close();

   // ordino la lista degli elementi per nome
   BlockNameList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_isBlockWithAttributes         <external> */
/*+
  Questa funzione restituisce se la definizione del blocco nel disegno corrente
  contiene degli attributi.
  Parametri:
  const CHAR *block;                      nome blocco
  AcDbBlockTableRecord *pBlockTableRec;   opzionale, puntatore a struttura del blocco.
                                          se = NULL la funzione se lo ricava internamente (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_isBlockWithAttributes(const TCHAR *block, AcDbBlockTableRecord *pBlockTableRec)
{
   int                    res = GS_BAD;
   AcDbBlockTable         *pBlockTable;
   AcDbBlockTableRecord   *_pBlockTableRec;

   if (!pBlockTableRec)
   {
      // database corrente
       AcDbDatabase *pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

      // Lettura dei blocchi
      if (pCurrDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;
      if (pBlockTable->getAt(block, _pBlockTableRec, AcDb::kForRead) != Acad::eOk)
         { pBlockTable->close(); GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }
   }
   else
      _pBlockTableRec = pBlockTableRec;

   // se non è un blocco nascosto e ha degli attributi
   if (block[0] != _T('*') && _pBlockTableRec->hasAttributeDefinitions() == Adesk::kTrue)
      res = GS_GOOD;

   if (!pBlockTableRec)
   {
      _pBlockTableRec->close();
      pBlockTable->close();
   }

   return res;
}


/*********************************************************/
/*.doc gsc_getBlockAttributeDefs              <external> */
/*+
  Questa funzione restituisce la lista delle definzioni 
  degli attributi di un blocco nel seguente formati:
  ((<name><def><descr>) ...)
  Parametri:
  const TCHAR *block;      nome del blocco

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
  N.B. Alloca memoria.
-*/  
/*********************************************************/
presbuf gsc_getBlockAttributeDefs(const TCHAR *block)
{
   AcDbDatabase                 *pCurrDatabase;
   AcDbBlockTable               *pBlockTable;
   AcDbBlockTableRecord         *pBlockTableRec;
	AcDbBlockTableRecordIterator *pBlockTableRecIter = NULL;
   AcDbEntity                   *pEnt;
   AcDbAttributeDefinition      *pAttDef;
   int                          Result;
   C_RB_LIST	                 Stru;

   // database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

   // Lettura dei blocchi
   if (pCurrDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return NULL;
   if (pBlockTable->getAt(block, pBlockTableRec, AcDb::kForRead) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSInvalidBlock; return NULL; }
	// Iterate for all attribute definitions
	if (pBlockTableRec->newIterator(pBlockTableRecIter) != Acad::eOk)
      { pBlockTableRec->close(); pBlockTable->close(); return NULL; }

   if ((Stru += acutBuildList(RTLB, 0)) == NULL)
      { pBlockTableRec->close(); pBlockTable->close(); return NULL; }

   Result = GS_GOOD;
	while (!pBlockTableRecIter->done())
	{
		if (pBlockTableRecIter->getEntity(pEnt, AcDb::kForRead) != Acad::eOk)
         { Result = GS_BAD; break; }
		if ((pAttDef = AcDbAttributeDefinition::cast(pEnt)))
		{
         if ((Stru += acutBuildList(RTLB, 
                                    RTSTR, pAttDef->tag(),
                                    RTSTR, pAttDef->promptConst(),
                                    RTSTR, pAttDef->textStringConst(),
                                    RTLE, 0)) == NULL)
         { Result = GS_BAD; break; }
		}				
		pEnt->close();
		pBlockTableRecIter->step();
	}
	delete pBlockTableRecIter;
   pBlockTableRec->close();
   pBlockTable->close();

   if (Result == GS_GOOD)
   {
      Stru.ReleaseAllAtDistruction(GS_BAD);
      return Stru.get_head();
   }
   else
      return NULL;
}


/*********************************************************/
/*.doc gsc_validblock                         <external> */
/*+
  Questa funzione verifica la validità del nome del blocco.
  Parametri:
  const TCHAR *block;  nome del blocco

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validblock(const TCHAR *block)
{
   size_t    len;
   C_RB_LIST DescrBlk;

   if (block == NULL) { GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }   
   len = wcslen(block);   
   if (len == 0 || len >= MAX_LEN_BLOCKNAME)
      { GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }   

   if ((DescrBlk << acdbTblSearch(_T("BLOCK"), block, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidBlock; return GS_BAD; }   

   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_validrefblk                        <external> */
/*+
  Questa funzione verifica la validità del file contenente il 
  blocco di riferimento e il nome del blocco di riferimento stesso.
  Parametri:
  TCHAR *file_ref_block; nome del file contenente il blocco di riferimento
                         (ALLOCATO DINAMICAMENTE) che viene automaticamente
                         corretto sostituendo eventuale alias
  TCHAR *ref_block;      nome del blocco di riferimento

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_validrefblk(TCHAR **file_ref_block, const TCHAR *ref_block)
{
   size_t len;
   bool   Check = FALSE;

   // se non c'è il nome del blocco anche il file deve essere NULL
   if (!ref_block) Check = TRUE;
   else
   {
      len = wcslen(ref_block);
      if (len >= MAX_LEN_BLOCKNAME)
         { GS_ERR_COD = eGSInvalidRefBlock; return GS_BAD; }
      else
         if (len == 0) Check = TRUE;
   }

   if (Check)
      // se non c'è il nome del blocco anche il file deve essere NULL
      if (file_ref_block && *file_ref_block && wcslen(*file_ref_block) > 0)
      { GS_ERR_COD = eGSInvalidRefBlock; return GS_BAD; }   

   if (file_ref_block && *file_ref_block)
   {
      if (wcslen(*file_ref_block) > 0)
      {
         C_STRING path;

         path = *file_ref_block;
         if (gsc_nethost2drive(path) == GS_BAD) return GS_BAD;
         free(*file_ref_block);
         if ((*file_ref_block = gsc_tostring(path.get_name())) == NULL) return GS_BAD;
      }
   }

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////////
// CBlockDlg dialog INIZIO
/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CBlockDlg, CDialog)

CBlockDlg::CBlockDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBlockDlg::IDD, pParent)
{
}

CBlockDlg::~CBlockDlg()
{
}

void CBlockDlg::DoDataExchange(CDataExchange* pDX)
{
   CDialog::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_BLOCK_LIST, m_BlkList);
   DDX_Control(pDX, IDC_CURR_BLOCK, m_CurrBlockLbl);
}


BEGIN_MESSAGE_MAP(CBlockDlg, CDialog)
   ON_NOTIFY(LVN_ITEMCHANGED, IDC_BLOCK_LIST, &CBlockDlg::OnLvnItemchangedBlockList)
   ON_NOTIFY(NM_DBLCLK, IDC_BLOCK_LIST, &CBlockDlg::OnNMDblclkBlockList)
   ON_BN_CLICKED(IDOK, &CBlockDlg::OnBnClickedOk)
   ON_BN_CLICKED(IDCANCEL, &CBlockDlg::OnBnClickedCancel)
   ON_BN_CLICKED(IDHELP, &CBlockDlg::OnBnClickedHelp)
END_MESSAGE_MAP()


// CBlockDlg message handlers
BOOL CBlockDlg::OnInitDialog()
{
   BOOL    bResult = CDialog::OnInitDialog();
	int     iItem = 0, CurrItem = -1;
	LV_ITEM lvitem;

   // leggo la lista dei blocchi
   if (gsc_getBlockList(m_BlkNameList, DwgPath.get_name()) == GS_BAD)
      return bResult;

   InitializeImageList();

   C_STR *p = (C_STR *) m_BlkNameList.get_head();
   while (p)
   {
		lvitem.iItem    = iItem;
      lvitem.lParam   = (LPARAM) p->get_name();
   	lvitem.mask     = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
      lvitem.iImage   = iItem;
      lvitem.iSubItem = 0;
      lvitem.pszText  = p->get_name();
   	m_BlkList.InsertItem(&lvitem);

      if (CurrentBlockName.CompareNoCase(p->get_name()) == 0)
         CurrItem = iItem;

      iItem++;
      p = (C_STR *) m_BlkNameList.get_next();
   }

   if (CurrItem >= 0)
   {
      CString Msg;

      Msg = gsc_msg(468); // "Blocco corrente: "
      Msg += m_BlkList.GetItemText(CurrItem, 0); // Nome blocco
      m_CurrBlockLbl.SetWindowTextW(Msg);

      m_BlkList.EnsureVisible(CurrItem, FALSE);
      m_BlkList.SetItemState(CurrItem, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
      m_BlkList.SetFocus();
   }

	return bResult;
}

// Carica le bitmap dei blocchi
void CBlockDlg::InitializeImageList(void)
{
   // lista di base delle immagini
   HBITMAP hBmp;
   C_STR   *p;
   CBitmap *pMyBMP;
   CDC     *pDC = m_BlkList.GetDC();

   // creazione lista vuota
   m_ImageList.Create(BLK_BITMAP_WIDTH, BLK_BITMAP_HEIGHT,
                      ILC_COLORDDB, 
                      m_BlkNameList.get_count(), // n. bitmap
                      0);
   m_BlkList.SetImageList(&m_ImageList, LVSIL_NORMAL);

   // aggiungo gli elementi di base della lista
   p = (C_STR *) m_BlkNameList.get_head();
   while (p)
   {
      if ((hBmp = gsc_extractBMPBlock(p->get_name(), *pDC, DwgPath.get_name())) == NULL)
         if ((hBmp = gsc_extractBMPBlock(DEFAULT_BLOCK, *(m_BlkList.GetDC()),
                                         DwgPath.get_name())) == NULL)
            break;

      pMyBMP = CBitmap::FromHandle(hBmp);
      if (m_ImageList.Add(pMyBMP, (CBitmap *) NULL) == -1) break;

      p = (C_STR *) m_BlkNameList.get_next();
   }

   m_BlkList.ReleaseDC(pDC);
   return;
}
void CBlockDlg::OnLvnItemchangedBlockList(NMHDR *pNMHDR, LRESULT *pResult)
{
   LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

   *pResult = 0;

   if (pNMLV->iItem == -1) return; // nessuna selezione
   CurrentBlockName = m_BlkList.GetItemText(pNMLV->iItem, 0); // Nome blocco
   CString Msg;
   Msg = gsc_msg(468); // "Blocco corrente: "
   Msg += CurrentBlockName;
   m_CurrBlockLbl.SetWindowTextW(Msg);
}

void CBlockDlg::OnNMDblclkBlockList(NMHDR *pNMHDR, LRESULT *pResult)
{
   LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

   *pResult = 0;

   if (pNMLV->iItem == -1) return; // nessuna selezione
   CurrentBlockName = m_BlkList.GetItemText(pNMLV->iItem, 0); // Nome blocco
   OnBnClickedOk();
}

void CBlockDlg::OnBnClickedOk()
{
   // TODO: Add your control notification handler code here
   OnOK();
}

void CBlockDlg::OnBnClickedCancel()
{
   // TODO: Add your control notification handler code here
   OnCancel();
}

void CBlockDlg::OnBnClickedHelp()
{
   // TODO: Add your control notification handler code here
}


/*************************************************************************/
/*.doc gslui_selblock                                                    */
/*+
   Scelta di un blocco tra quelli caricati (uso di interfaccia grafica).
   Parametri:
   ([Blocco corrente][fileDWG])
-*/
/*************************************************************************/
int gs_uiselblock(void)
{
   presbuf arg = acedGetArgs(), p;
   TCHAR   *CurrentBlockName = NULL;
   C_STRING DwgPath;

   acedRetNil();
   if (arg)
   {
      if (arg->restype == RTSTR) CurrentBlockName = arg->resval.rstring;
      if ((arg = arg->rbnext) && arg->restype == RTSTR)
         DwgPath = arg->resval.rstring;
   }

   if ((p = gsc_uiSelBlock(CurrentBlockName, DwgPath.get_name())) != NULL)
   {
      acedRetStr(p->resval.rstring);
      acutRelRb(p);
   }

   return RTNORM;
}
presbuf gsc_uiSelBlock(const TCHAR *current, const TCHAR *DwgPath)
{
   // When resource from this ARX app is needed, just
   // instantiate a local CAcModuleResourceOverride
   CAcModuleResourceOverride myResources;

   CBlockDlg MyDlg;

   if (current) MyDlg.CurrentBlockName = current;
   MyDlg.DwgPath = DwgPath;

   if (MyDlg.DoModal() == IDOK)
   {
      C_RB_LIST result;

      if ((result << acutBuildList(RTSTR, MyDlg.CurrentBlockName, 0)) == NULL)
         return NULL;
      result.ReleaseAllAtDistruction(GS_BAD);

      return result.get_head();
   }
   else return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CBlockDlg dialog FINE
/////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_extractDIBBlock                <external> */
/*+
  Questa funzione estrae una DIB da un blocco se questo ha un'immagine di preview.
  Parametri:
  TCHAR *BlockName;    nome blocco
  const TCHAR *DwgPath; Path del file DWG contenente il blocco, se = NULL
                        si intende la sessione corrente di autocad (default = NULL)

  Restituisce un handle di DIB in caso di successo altrimenti NULL.
  N.B. Alloca memoria (da rilasciare con "GlobalFree")
-*/  
/*********************************************************/
//utility functions
WORD DibNumColors(VOID FAR * pv)
{
   INT                 bits;
   LPBITMAPINFOHEADER  lpbi;

   lpbi = ((LPBITMAPINFOHEADER)pv);

	if (lpbi->biClrUsed != 0)
		return (WORD)lpbi->biClrUsed;
   bits = lpbi->biBitCount;

   switch (bits)
   {
      case 1:
         return 2;
      case 4:
         return 16;
      case 8:
         return 256;
      default:
         /* A 24 bitcount DIB has no color table */
         return 0;
   }
}
static WORD PaletteSize(VOID FAR * pv)
{
   LPBITMAPINFOHEADER lpbi;
   WORD               NumColors;

   lpbi      = (LPBITMAPINFOHEADER)pv;
   NumColors = DibNumColors(lpbi);

   return (WORD)(NumColors * sizeof(RGBQUAD));
}
static HANDLE readDibFromMemory(LPVOID pDibSrc)
{
   LPBITMAPINFOHEADER pbmih = (LPBITMAPINFOHEADER)pDibSrc;

   DWORD totalSize = pbmih->biSize + pbmih->biSizeImage + (DWORD)PaletteSize(pbmih);

   HANDLE hdib = GlobalAlloc(GHND, totalSize);
   LPVOID pDibDst = GlobalLock(hdib);

   CopyMemory(pDibDst, pDibSrc, totalSize);

   GlobalUnlock(hdib);
   return hdib;
}

HANDLE gsc_extractDIBBlock(const TCHAR *BlockName, const TCHAR *DwgPath)
{	
   AcDbDatabase         extDatabase(Adesk::kFalse);
   AcDbBlockTable       *pBlockTable;
   AcDbBlockTableRecord *pBlockTableRec;
   Acad::ErrorStatus    Found;

   if (!DwgPath) // leggo i blocchi dalla sessione corrente di Autocad
   {
      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      Found = pBlockTable->getAt(BlockName, pBlockTableRec, AcDb::kForRead);
      pBlockTable->close();
   }
   else // leggo i blocchi dal DWG indicato
   {
      C_STRING tmp_path, ext;

      tmp_path = DwgPath;
      // Controlla Correttezza Path
      if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione la aggiungo
         tmp_path += _T(".DWG");

      // leggo il dwg
      if (extDatabase.readDwgFile(tmp_path.get_name()) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

      // Lettura dei blocchi nel dwg
      if (extDatabase.getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

      Found = pBlockTable->getAt(BlockName, pBlockTableRec, AcDb::kForRead);
      pBlockTable->close();
   }

   if (Found != Acad::eOk) { GS_ERR_COD = eGSInvalidBlock; return NULL; }

	if (pBlockTableRec->hasPreviewIcon())
	{
		AcDbBlockTableRecord::PreviewIcon previewIcon;

      HANDLE result;

		pBlockTableRec->getPreviewIcon(previewIcon);
		result = readDibFromMemory((LPVOID) previewIcon.asArrayPtr());
      pBlockTableRec->close();
      return result;
	}
   pBlockTableRec->close();

   return NULL;
}

HBITMAP gsc_extractBMPBlock(const TCHAR *BlockName, HDC hdc, const TCHAR *DwgPath)
{	
   AcDbDatabase         extDatabase(Adesk::kFalse);
   AcDbBlockTable       *pBlockTable;
   AcDbBlockTableRecord *pBlockTableRec;
   Acad::ErrorStatus    Found;

   if (!DwgPath) // leggo i blocchi dalla sessione corrente di Autocad
   {
      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      Found = pBlockTable->getAt(BlockName, pBlockTableRec, AcDb::kForRead);
      pBlockTable->close();
   }
   else // leggo i blocchi dal DWG indicato
   {
      C_STRING tmp_path, ext;

      tmp_path = DwgPath;
      // Controlla Correttezza Path
      if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione la aggiungo
         tmp_path += _T(".DWG");

      // leggo il dwg
      if (extDatabase.readDwgFile(tmp_path.get_name()) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

      // Lettura dei blocchi nel dwg
      if (extDatabase.getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

      Found = pBlockTable->getAt(BlockName, pBlockTableRec, AcDb::kForRead);
      pBlockTable->close();
   }

   if (Found != Acad::eOk) { pBlockTableRec->close(); GS_ERR_COD = eGSInvalidBlock; return NULL; }

	if (pBlockTableRec->hasPreviewIcon())
	{
		AcDbBlockTableRecord::PreviewIcon previewIcon;

		pBlockTableRec->getPreviewIcon(previewIcon);

      BITMAPINFOHEADER ih;

      memcpy(&ih, previewIcon.asArrayPtr(), sizeof(ih));
      #ifdef _WIN64
         size_t memsize = sizeof(BITMAPINFOHEADER) + ((1i64 << ih.biBitCount) * sizeof(RGBQUAD));
      #else
         size_t memsize = sizeof(BITMAPINFOHEADER) + ((1 << ih.biBitCount) * sizeof(RGBQUAD));
      #endif
      LPBITMAPINFO bi = (LPBITMAPINFO)malloc(memsize);
      memcpy(bi, previewIcon.asArrayPtr(), memsize);

      HBITMAP hbm = CreateDIBitmap(hdc, &ih, CBM_INIT, 
                                   previewIcon.asArrayPtr() + memsize, bi, DIB_RGB_COLORS);
      free(bi);
      pBlockTableRec->close();

      return hbm;
   }

   pBlockTableRec->close();

   return NULL;
}


/*********************************************************/
/*.doc gsc_save_block                         <external> */
/*+
  Questa funzione effettua il salvataggio di una definizione di blocco
  in un database grafico autocad.
  Parametri:
  const TCHAR *BlockName;     Nome del blocco
  AcDbDatabase &extDatabase;  Database grafico di autocad

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_block(const TCHAR *BlockName, AcDbDatabase &extDatabase)
{
   AcDbDatabase         *pCurrDatabase;
   AcDbBlockTable       *pBlockTable;
   AcDbBlockTableRecord *pBlockTableRec;
   AcDbObjectIdArray    ObjectIds;
   AcDbObjectId         ObjId;
   AcDbIdMapping        mMap;
   AcDbObjectId         recordId = AcDbObjectId::kNull;

   // Lettura dei blocchi nel database esterno
   if (extDatabase.getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   // Se il blocco è già definito
   if (pBlockTable->getAt(BlockName, pBlockTableRec, AcDb::kForRead) == Acad::eOk)
   {
      pBlockTableRec->close();
      pBlockTable->close();
      return GS_GOOD;
   }   

   // Ricavo ID della tabella dei blocchi del database esterno
   ObjId = pBlockTable->objectId();
   pBlockTable->close();

   // leggo definizione blocco dal database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

   if (pCurrDatabase->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->getAt(BlockName, recordId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvalidBlock; pBlockTable->close(); return GS_BAD; }
   ObjectIds.append(recordId);

   pBlockTable->close();

   if (extDatabase.wblockCloneObjects(ObjectIds, acdbSymUtil()->blockModelSpaceId(&extDatabase), 
       mMap, AcDb::kDrcIgnore) != Acad::eOk)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_save_blocks                        <external> */
/*+
  Questa funzione effettua il salvataggio di una lista di 
  definizione di blocchi in un file DWG autocad.
  Parametri:
  C_STR_LIST &BlockNameList;  Lista dei nomi dei blocchi
  C_STRING &OutDwg;           file DWG autocad
  int MsgToVideo;             Flag, se = GS_GOOD stampa a video
                              i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_save_blocks(C_STR_LIST &BlockNameList, C_STRING &OutDwg, int MsgToVideo)
{
   AcDbDatabase *pTempDb;
   C_STR        *pBlockName = (C_STR *) BlockNameList.get_head();
   C_STRING     tmp_path(OutDwg);
   Acad::ErrorStatus es;

   // Controlla correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 

   if (gsc_path_exist(OutDwg) == GS_GOOD)
   { 
      pTempDb = new AcDbDatabase(Adesk::kFalse);
      // leggo il dwg
      if (pTempDb->readDwgFile(tmp_path.get_name(), _SH_DENYRW) != Acad::eOk)
         { delete pTempDb; GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   }
   else
      pTempDb = new AcDbDatabase(Adesk::kTrue);

   while (pBlockName)
   {
      if (gsc_save_block(pBlockName->get_name(), *pTempDb) == GS_BAD) return GS_BAD;

      pBlockName = (C_STR *) BlockNameList.get_next();
   }

   // salvo il db temporaneo in un dwg 
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
	if ((es = pTempDb->saveAs(tmp_path.get_name())) != Acad::eOk)
   {
      delete pTempDb;
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
      return GS_BAD;
   }
   delete pTempDb;
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   if (MsgToVideo == GS_GOOD)
   {
      // "\nLe definizioni dei seguenti blocchi sono state scritte in %s:"
      acutPrintf(gsc_msg(403), OutDwg.get_name()); 

      pBlockName = (C_STR *) BlockNameList.get_head();
      while (pBlockName)
      {
         acutPrintf(_T("\n%s"), pBlockName->get_name());
         pBlockName = (C_STR *) BlockNameList.get_next();
      }
      acutPrintf(GS_LFSTR);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_explode                            <external> */
/*+
  Questa funzione effettua l'esplosione di un oggetto grafico.
  La funzione cancella l'oggetto originale e inserisce in grafica
  gli oggetti che lo compongono.
  Parametri:
  ads_name ent;    oggetto grafico
  bool Recursive;	 Flag, se vero continua a esplodere finchè è possibile
					    (default = false)
  bool TryAcadCmd; Flag, se vero allora qualora nonn funzioni il metodo
                         explode si prova il comando autocad "_.EXPLODE"

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_explode(ads_name ent, bool Recursive, bool TryAcadCmd)
{
   AcDbObjectId     objId;
   AcDbEntity       *pEntity;
   AcDbVoidPtrArray entitySet;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   if (acdbOpenAcDbEntity(pEntity, objId, AcDb::kForWrite) != Acad::eOk) return GS_BAD;
   if (gsc_explode(pEntity, Recursive) != GS_GOOD && TryAcadCmd)
   { 
      pEntity->close();

      // Il metodo explode non funziona per blocchi con scala x e y non uniforme
      // Autodesk consiglia di usare il comando EXPLODE
      return gsc_callCmd(_T("_.EXPLODE"), RTENAME, ent, 0);
   }

   if (pEntity->close() != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}
int gsc_explode(AcDbEntity *pEntity, bool Recursive, AcDbBlockTableRecord *pBlockTableRecord,
                AcDbVoidPtrArray *pExplodedSet)
{
   AcDbVoidPtrArray     entitySet;
   int                  length;
   AcDbBlockTableRecord *pInternalBlockTableRecord = pBlockTableRecord;
   Acad::ErrorStatus    Res = pEntity->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (pEntity->explode(entitySet) != Acad::eOk) return GS_BAD;
   if ((length = entitySet.length()) == 0) return GS_BAD;

   if (pInternalBlockTableRecord == NULL)
   {
      AcDbBlockTable *pBlockTable;

      // Open the block table for read.
      if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, 
                                                                           AcDb::kForRead) != Acad::eOk)
         return GS_BAD;
      if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
         { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      pBlockTable->close();
   }

   for (int i = 0; i < length; i++)
   {
	   if (Recursive)
      {
         // se non si può esplodere ulteriormente
         if (gsc_explode((AcDbEntity *) entitySet[i], true, pInternalBlockTableRecord) != GS_GOOD)
         {
	         pInternalBlockTableRecord->appendAcDbEntity((AcDbEntity *) entitySet[i]);
            if (pExplodedSet) pExplodedSet->append(entitySet[i]);
         }
      }
	   else
      {
         pInternalBlockTableRecord->appendAcDbEntity((AcDbEntity *) entitySet[i]);
         if (pExplodedSet) pExplodedSet->append(entitySet[i]);
      }

      ((AcDbEntity *) entitySet[i])->close();
   }
   
   if (pBlockTableRecord == NULL) pInternalBlockTableRecord->close();

   // Non controllo l'esito di erase perchè pEntity potrebbe non esistere in grafica
   // nel caso di ricorsione
   pEntity->erase();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_explodeNoGraph                     <external> */
/*+
  Questa funzione effettua l'esplosione di un oggetto grafico.
  La funzione lascia inalterato l'oggetto originale e non
  inserisce in grafica gli oggetti che lo compongono.
  Parametri:
  ads_name ent;    oggetto grafico
  AcDbVoidPtrArray &ExplodedSet;
  bool Recursive;	 Flag, se vero continua a esplodere finchè è possibile
					    (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_explodeNoGraph(AcDbEntity *pEntity, AcDbVoidPtrArray &ExplodedSet, bool Recursive)
{
   AcDbVoidPtrArray entitySet;
   int              length, i;

   if (pEntity->explode(entitySet) != Acad::eOk) return GS_BAD;
   if ((length = entitySet.length()) == 0) return GS_BAD;

   for (i = 0; i < length; i++)
   {
	   if (Recursive)
      {
         // se non si può esplodere ulteriormente
         if (gsc_explodeNoGraph((AcDbEntity *) entitySet[i], ExplodedSet, true) == GS_BAD)
            ExplodedSet.append(entitySet[i]);
      }
      else
         ExplodedSet.append(entitySet[i]);

      ((AcDbEntity *) entitySet[i])->close();
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getBlockId                         <external> */
/*+
  Questa funzione restituisce l'identificativo di un blocco.
  Parametri:
  const TCHAR *BlockName;        Nome del blocco
  AcDbObjectId &BlockId;         Identificativo del blocco (out)
  AcDbBlockTable *pBlockTable;   Tabella dei blocchi (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getBlockId(const TCHAR *BlockName, AcDbObjectId &BlockId,
                   AcDbBlockTable *pBlockTable)
{
   AcDbBlockTable *pMyBlockTable;
   Acad::ErrorStatus es;
    
   if (pBlockTable) pMyBlockTable = pBlockTable;
   else
      if (acdbHostApplicationServices()->workingDatabase()->getBlockTable(pMyBlockTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

   if ((es = pMyBlockTable->getAt(BlockName, BlockId, Adesk::kFalse)) != Acad::eOk)
      GS_ERR_COD = eGSInvalidBlock;
   if (!pBlockTable) pMyBlockTable->close();

   return (es == Acad::eOk) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_set_block                          <external> */
/*+
  Questa funzione setta il blocco di un'entità AcDbBlockReference.
  Parametri:
  AcDbEntity *pEnt
  AcDbObjectId &BlockId;   Identificativo del blocco
  oppure
  ads_name ent;            Oggetto grafico
  const TCHAR *block;      Nome del blocco
  oppure
  ads_name ent;            Oggetto grafico
  AcDbObjectId &BlockId;   Identificativo del blocco

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_block(AcDbEntity *pEnt, AcDbObjectId &BlockId)
{
   if (!pEnt->isKindOf(AcDbBlockReference::desc())) return GS_GOOD;

   if (((AcDbBlockReference *) pEnt)->blockTableRecord() == BlockId) return GS_GOOD;

   Acad::ErrorStatus Res = ((AcDbBlockReference *) pEnt)->upgradeOpen();

   if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (((AcDbBlockReference *)pEnt)->setBlockTableRecord(BlockId) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return GS_GOOD;
}
int gsc_set_block(ads_name ent, const TCHAR *block)
{
   AcDbObjectId BlockId;

   if (gsc_getBlockId(block, BlockId) == GS_BAD) return GS_BAD;
   return gsc_set_block(ent, BlockId);
}
int gsc_set_block(ads_name ent, AcDbObjectId &BlockId)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_block((AcDbEntity *) pObj, BlockId) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    BLOCCHI  FINE         ///////////////////
//////////////////  GESTIONE  BLOCCHI DINAMICI INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_setDynBlkProperty                  <external> */
/*+
  Questa funzione setta una proprietà del blocco dinamico.
  Parametri:
  ads_name ent;            Oggetto grafico blocco dinamico
  const TCHAR *PropName;   Nome proprietà da cambiare
  
  bool PropValue;          Valore proprietà
  o
  const TCHAR *PropValue;
  o
  double PropValue;
  o
  ads_point PropValue;
-*/  
/*********************************************************/
int gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, const AcDbEvalVariant& PropValue)
{
   AcDbObjectId                       objId;
   AcDbDynBlockReferencePropertyArray Properties;
   AcDbDynBlockReferenceProperty      DynBlockProperty;
   int                                i;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   AcDbDynBlockReference DynBlk(objId);
   DynBlk.getBlockProperties(Properties);
   for (i = 0; i < Properties.length(); i++)
   {
      DynBlockProperty = Properties[i];

      if (DynBlockProperty.propertyName().compareNoCase(PropName) == 0)
         if (DynBlockProperty.readOnly() == false)
         {
            DynBlockProperty.setValue(PropValue);
            break;
         }
   }

   return GS_GOOD;
}
int gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, bool PropValue)
{
   AcDbEvalVariant Value((short) ((PropValue == false) ? 0 : 1)); 
   return gsc_setDynBlkProperty(ent, PropName, Value);
}
int gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, const TCHAR *PropValue)
{
   AcDbEvalVariant Value(PropValue);
   return gsc_setDynBlkProperty(ent, PropName, Value);
}
int gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, double PropValue)
{
   AcDbEvalVariant Value(PropValue);
   return gsc_setDynBlkProperty(ent, PropName, Value);
}
int gsc_setDynBlkProperty(ads_name ent, const TCHAR *PropName, ads_point PropValue)
{
   AcGePoint3d Pt(PropValue[X], PropValue[Y], PropValue[Z]);
   AcDbEvalVariant Value(Pt);
   return gsc_setDynBlkProperty(ent, PropName, Value);
}




/*********************************************************/
/*.doc gsc_insert_dwg                         <external> */
/*+
  Questa funzione inserisce la definizione di un blocco leggendo da dwg.
  Parametri:
  const TCHAR *DwgPath;   Path del file dwg
  const TCHAR *BlockName; Opzionale; Nome del blocco da creare.
                          Se non specificato viene usato il nome del file
                          (default = NULL)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_insert_dwg(const TCHAR *DwgPath, const TCHAR *BlockName)
{
   AcDbDatabase *pCurrDatabase, extDatabase(Adesk::kFalse);
   AcDbObjectId ObjID;
   C_STRING     tmp_path, Name, FileExt;

   tmp_path = DwgPath;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 

   gsc_splitpath(tmp_path, NULL, NULL, &Name, &FileExt);

   if (BlockName) Name = BlockName;

   // leggo il dwg
   if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYNO) != Acad::eOk)
   {
      if (FileExt.len() == 0) // se non ha estensione provo ad aggiungerla
         tmp_path += _T(".DWG");
      if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYNO) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   }

   if ((pCurrDatabase = acdbHostApplicationServices()->workingDatabase()) == NULL)
      return GS_BAD;
   if (pCurrDatabase->insert(ObjID, Name.get_name(), &extDatabase) != Acad::eOk)
      return GS_BAD;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE  BLOCCHI DINAMICI FINE   ///////////////////
//////////////////    GESTIONE    STILI TESTO INIZIO    ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gs_getgeosimstyle <external> */
/*+
  Questa funzione restituisce una lista con gli stili testo di GEOsim.
  ( <nome1> <nome2> ....)
-*/  
/*********************************************************/
int gs_getgeosimstyle(void)
{
   C_RB_LIST ret;

   ret = gsc_getgeosimstyle();
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getgeosimstyle <external> */
/*+
  Questa funzione restituisce una lista con gli stili testo cercandoli nella 
  tabella dei simboli di AutoCAD.
  ( <nome1> <nome2> ....)
-*/  
/*********************************************************/
presbuf gsc_getgeosimstyle(void)
{
   C_STR_LIST StyleNameList;
   C_STR      *pStyleName;
   C_RB_LIST  DescrStyle;
   presbuf    p;

   DescrStyle << ads_tblnext(_T("STYLE"), 1); // prendo il primo stile
   
   while (DescrStyle.get_head() != NULL)
   {
      p = DescrStyle.SearchType(2);

      if ((pStyleName = new C_STR(p->resval.rstring)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      StyleNameList.add_tail(pStyleName);

      DescrStyle << ads_tblnext(_T("STYLE"), 0); // prendo il successivo stile
   }
   // ordino la lista degli elementi per nome
   StyleNameList.sort_name(TRUE, TRUE); // sensitive e ascending

   return StyleNameList.to_rb();
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "dim_styles" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_dim_styles_accept(ads_callback_packet *dcl)
{
   TCHAR val[10];
   int   *pos = (int *) dcl->client_data;

   if (ads_get_tile(dcl->dialog, _T("list"), val, 10) != RTNORM) return;
   *pos = _wtoi(val);
   ads_done_dialog(dcl->dialog, DLGOK);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "dim_styles" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_dim_styles_list(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_DOUBLE_CLICK) dcl_dim_styles_accept(dcl);
}


/*********************************************************/
/*.doc gsc_validtextstyle                     <external> */
/*+
  Questa funzione verifica la validità dello stile di testo.
  Parametri:
  const TCHAR *style;  nome del riempimento

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validtextstyle(const TCHAR *style)
{
   size_t    len;
   C_RB_LIST DescrStyle;

   if (style == NULL) { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }   
   len = wcslen(style);   
   if (len == 0 || len >= MAX_LEN_TEXTSTYLENAME)
      { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }   

   if ((DescrStyle << acdbTblSearch(_T("STYLE"), style, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }   

   return GS_GOOD;
}  


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "text_styles" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_text_styles_accept(ads_callback_packet *dcl)
{
   TCHAR val[10];
   int   *pos = (int *) dcl->client_data;

   if (ads_get_tile(dcl->dialog, _T("list"), val, 10) != RTNORM) return;
   *pos = _wtoi(val);
   ads_done_dialog(dcl->dialog, DLGOK);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "text_styles" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_text_styles_list(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_DOUBLE_CLICK) dcl_text_styles_accept(dcl);
}


/*********************************************************/
/*.doc gs_ddseltextstyles                     <external> */
/*+
  Questa funzione permette la scelta di uno stile di testo tramite
  interfaccia a finestra
  Parametro:
  lo stile di testo di default (opzionale)
  
  oppure
  const TCHAR *current[MAX_LEN_TEXTSTYLENAME]; stile di testo corrente

  Restituisce il nome dello stile di testo scelto.
-*/  
/*********************************************************/
int gs_ddseltextstyles(void)
{
   TCHAR   current[MAX_LEN_TEXTSTYLENAME] = GS_EMPTYSTR;
   presbuf arg = acedGetArgs(), p;

   acedRetNil();

   if (arg && arg->restype == RTSTR) // leggo lo stile di testo corrente (opzionale)
      gsc_strcpy(current, arg->resval.rstring, MAX_LEN_TEXTSTYLENAME);
   if ((p = gsc_ddseltextstyles(current)) != NULL)
   {
      acedRetStr(p->resval.rstring);
      acutRelRb(p);
   }
   return RTNORM;
}
presbuf gsc_ddseltextstyles(const TCHAR *current)
{
   TCHAR     strPos[10] = _T("0");
   int       status = DLGOK, dcl_file, pos = 0;
   ads_hdlg  dcl_id;
   C_RB_LIST value_list;
   presbuf   arg = acedGetArgs(), p;
   C_STRING  Descr, path;

   if ((value_list << gsc_getgeosimstyle()) == NULL) return NULL;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("text_styles"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return NULL; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW,0);
   p = value_list.get_head();
   while (p && p->restype == RTSTR)
   {
      Descr = p->resval.rstring;  // nome
      
      if (current && gsc_strcmp(p->resval.rstring, current, FALSE) == 0)
         swprintf(strPos, 10, _T("%d"), pos);
      else pos++;
      
      p = p->rbnext;
      gsc_add_list(Descr);
   }
   ads_end_list();

   ads_set_tile(dcl_id, _T("list"), strPos);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_text_styles_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &pos);
   ads_action_tile(dcl_id, _T("list"),   (CLIENTFUNC) dcl_text_styles_list);
   ads_client_data_tile(dcl_id, _T("list"), &pos);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_RB_LIST result;
      presbuf   dummy = acutBuildList(RTLB, 0);

      value_list.link_head(dummy);
      p = gsc_nth(pos, value_list.get_head());
      if ((result << gsc_copybuf(p)) == NULL) return NULL;

      result.ReleaseAllAtDistruction(GS_BAD);

      return result.get_head();
   }
   else return NULL;
}


/*********************************************************/
/*.doc gsc_load_textstyle                     <external> */
/*+
  Questa funzione carica lo stile di testo se non è già stato caricato
  cercando il corrispondente file nelle path di ricerca standard di AutoCAD
  e se dovesse fallire cerca il relativo file (prima .SHX e poi .TTF) nel 
  sotto-direttorio GEOTHMDIR di GEODIR.
  Parametro:
  const TCHAR *style;    lo stile di testo

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_load_textstyle(const TCHAR *style)
{
   resbuf   *rb;
   C_STRING file;
   TCHAR    name[MAX_LEN_TEXTSTYLENAME];

   gsc_strcpy(name, style, MAX_LEN_TEXTSTYLENAME);

   if (gsc_validtextstyle(name) == GS_BAD) return GS_BAD;

   if ((rb = acdbTblSearch(_T("STYLE"), name, 0)) == NULL)
   {
      if (gsc_callCmd(_T("_.-STYLE"), RTSTR, name, RTSTR, name, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR,
                      RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
      {
         C_STRING Prefix;

         Prefix = GEOsimAppl::GEODIR;
         Prefix += _T('\\');
         Prefix += GEOTHMDIR;
         Prefix += _T('\\');
         Prefix += name;
         
         file = Prefix;
         file += _T(".SHX");

         if (gsc_path_exist(file) == GS_BAD)
         {
            file = Prefix;
            file += _T(".TTF");
            if (gsc_path_exist(file) == GS_BAD)
               { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }
         }

         if (gsc_callCmd(_T("_.-STYLE"), RTSTR, name, RTSTR, name, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR,
                         RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
            { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }
      }
   }
   else acutRelRb(rb);
   
   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int gsc_set_CurrentTextStyle(const TCHAR *style, TCHAR *old)
{
   resbuf   *rb;
   C_STRING file;
   TCHAR    name[MAX_LEN_TEXTSTYLENAME];

   // Memorizza vecchio valore in old
   if (old)  
   {
      if ((rb = acutBuildList(RTNIL,0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("TEXTSTYLE"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTSTR || rb->resval.rstring == NULL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      gsc_strcpy(old, rb->resval.rstring, MAX_LEN_TEXTSTYLENAME); 
      acutRelRb(rb); 
   }                              

   if (!style) return GS_GOOD;
   gsc_strcpy(name, style, MAX_LEN_TEXTSTYLENAME);
   gsc_alltrim(name);                              
   if (gsc_strlen(name) == 0) return GS_GOOD;

   if (gsc_validtextstyle(name) == GS_BAD) return GS_BAD;

   // se non è caricato prova a caricarlo
   if (gsc_load_textstyle(name) == GS_BAD) return GS_BAD;
   
   if ((rb = acutBuildList(RTSTR, name, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   if (acedSetVar(_T("TEXTSTYLE"), rb) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; acutRelRb(rb); return GS_BAD; }

   acutRelRb(rb); 
   
   return GS_GOOD;
}


//-----------------------------------------------------------------------//
double gsc_get_CurrentTextSize(void)
{
   resbuf rb;
   double TxtSize;
   
   if (acedGetVar(_T("TEXTSIZE"), &rb) != RTNORM) { GS_ERR_COD = eGSVarNotDef; return 0; }
   if (gsc_rb2Dbl(&rb, &TxtSize) == GS_BAD) { GS_ERR_COD = eGSVarNotDef; return 0; }

   return TxtSize;
}


/****************************************************************************/
/*.doc gsc_get_charact_textstyle                                 <external> */
/*+
  Questa funzione legge le caratteristiche dello stile di testo.
  Parametri:
  const TCHAR *style;  Nome dello stile di testo
  int *FixedHText;     Altezza testo fissa  (default = NULL)
  int *Width;          Fattore di larghezza (default = NULL)
  int *Angle;          Angolo obliquo       (default = NULL)
  C_STRING *FontName;  Nome font primario   (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gsc_get_charact_textstyle(const TCHAR *style, double *FixedHText, double *Width,
                              double *Angle, C_STRING *FontName)
{
   C_RB_LIST List;
   presbuf   p;

   if ((List << acdbTblSearch(_T("STYLE"), style, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidTextStyle; return GS_BAD; }

   if (FixedHText)
      if ((p = List.SearchType(40)) != NULL) *FixedHText = p->resval.rreal;

   if (Width)
      if ((p = List.SearchType(41)) != NULL) *Width = p->resval.rreal;

   if (Angle)
      if ((p = List.SearchType(50)) != NULL) *Angle = p->resval.rreal;

   if (FontName)
      if ((p = List.SearchType(50)) != NULL && p->restype == RTSTR)
         FontName->set_name(p->resval.rstring);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getTextStyleId                     <external> */
/*+
  Questa funzione restituisce l'identificativo di uno stile di testo.
  Parametri:
  const TCHAR *StyleName;  Nome dello stile di testo
  AcDbObjectId &StyleId;   Identificativo dello stile di testo (out)
  AcDbTextStyleTable *pStyleTable; Opzionale, Tabella degli stili
                                   se = null usa la tabella del db corrente

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getTextStyleId(const TCHAR *StyleName, AcDbObjectId &StyleId, 
                       AcDbTextStyleTable *pStyleTable)
{
   AcDbTextStyleTable *pMyStyleTable;
   Acad::ErrorStatus es;
    
   if (pStyleTable) pMyStyleTable = pStyleTable;
   else
      if (acdbHostApplicationServices()->workingDatabase()->getTextStyleTable(pMyStyleTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

   if ((es = pMyStyleTable->getAt(StyleName, StyleId, Adesk::kFalse)) != Acad::eOk)
      GS_ERR_COD = eGSInvalidTextStyle;
   if (!pStyleTable) pMyStyleTable->close();

   return (es == Acad::eOk) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_set_textStyle                      <external> */
/*+
  Questa funzione setta uno stile di testo ad un oggetto grafico.
  Parametri:
  ads_name ent;            Oggetto grafico       
  const TCHAR *TextStyle;  Stile di testo

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_textStyle(AcDbEntity *pEnt, AcDbObjectId &textStyleId)
{
   if (pEnt->isKindOf(AcDbText::desc()))
   {
      if (textStyleId != ((AcDbText *)pEnt)->textStyle())
      {
         Acad::ErrorStatus Res = ((AcDbText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbText *)pEnt)->setTextStyle(textStyleId) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbMText::desc()))
   {
      if (textStyleId != ((AcDbMText *) pEnt)->textStyle())
      {
         Acad::ErrorStatus Res = ((AcDbMText *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbMText *)pEnt)->setTextStyle(textStyleId) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttribute::desc()))
   {
      if (textStyleId != ((AcDbAttribute *)pEnt)->textStyle())
      {
         Acad::ErrorStatus Res = ((AcDbAttribute *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttribute *)pEnt)->setTextStyle(textStyleId) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }
   else
   if (pEnt->isKindOf(AcDbAttributeDefinition::desc()))
   {
      if (textStyleId != ((AcDbAttributeDefinition *)pEnt)->textStyle())
      {
         Acad::ErrorStatus Res = ((AcDbAttributeDefinition *) pEnt)->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
         if (((AcDbAttributeDefinition *)pEnt)->setTextStyle(textStyleId) != Acad::eOk)
            { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      }
   }

   return GS_GOOD;
}
int gsc_set_textStyle(ads_name ent, const TCHAR *TextStyle)
{
   AcDbObjectId textStyleId;

   if (gsc_getTextStyleId(TextStyle, textStyleId) == GS_BAD) return GS_BAD;
   return gsc_set_textStyle(ent, textStyleId);
}
int gsc_set_textStyle(ads_name ent, AcDbObjectId &textStyleId)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_textStyle((AcDbEntity *) pObj, textStyleId) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    STILI TESTO  FINE     ///////////////////
//////////////////    GESTIONE    COORDINATE  INIZIO    ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_getgeosimcoord <external> */
/*+
  Questa funzione restituisce una lista con i codici dei
  sistemi di coordinate di GEOsim.
  C_STR_LIST &list;  Lista dei codici dei sistemi di coordinate

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getgeosimcoord(C_STR_LIST &list)
{
   C_RB_LIST CtgList, CoordSysList;
   presbuf   pCtg, pCoordSys;
   C_STR     *pCoordCode;

   list.remove_all();
   if ((CtgList << ade_projlistctgy()) == NULL) return GS_BAD;   

   pCtg = CtgList.get_head();
   while (pCtg && pCtg->restype == RTSTR)
   {                  
      CoordSysList << ade_projlistcrdsysts(pCtg->resval.rstring);
   
      pCoordSys = CoordSysList.get_head();
      while (pCoordSys)
      {
         if ((pCoordCode = new C_STR(pCoordSys->resval.rstring)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         list.add_tail(pCoordCode);

         pCoordSys = CoordSysList.get_next();
      }
      pCtg = CtgList.get_next();
   }

   return GS_GOOD;
}   
      
     
/*********************************************************/
/*.doc gs_getgeosimcoord <external> */
/*+
  Questa funzione restituisce una lista dei sistemi di coordinate di GEOsim.
  (<coord1> <coord2> ...)
-*/  
/*********************************************************/
int gs_getgeosimcoord(void)
{
   C_STR_LIST List;
   C_RB_LIST  ret;

   acedRetNil();
   if (gsc_getgeosimcoord(List) == GS_BAD) return RTERROR;
   if ((ret << List.to_rb()) == GS_BAD) return RTERROR;
   ret.LspRetList();

   return RTNORM;
}
      
     
/*********************************************************/
/*.doc gsc_get_category_coord <external> */
/*+
  Questa funzione restituisce la categoria a cui appartiene un sistema
  di coordinate di GEOsim.
  Parametro:
  const TCHAR *coord;     sistema di coordinate
  C_STRING   &Category;   categoria di appartenenza (out)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_get_category_coord(const TCHAR *coord, C_STRING &Category)
{
   C_RB_LIST CtgList, CoordSysList;
   presbuf   pCtg, pCoordSys;

   Category.clear();
   if ((CtgList << ade_projlistctgy()) == NULL) return GS_BAD;   

   pCtg = CtgList.get_head();
   while (pCtg && pCtg->restype == RTSTR)
   {                  
      CoordSysList << ade_projlistcrdsysts(pCtg->resval.rstring);
   
      pCoordSys = CoordSysList.get_head();
      while (pCoordSys)
      {  // non sensibile al maiuscolo
         if (gsc_strcmp(coord, pCoordSys->resval.rstring, FALSE) == 0)
            { Category = pCtg->resval.rstring; return GS_GOOD;}

         pCoordSys = CoordSysList.get_next();
      }
      pCtg = CtgList.get_next();
   }

   return GS_BAD;
}
      

/*********************************************************/
/*.doc printlistcoord <internal> */
/*+
  Questa funzione restituisce una lista dei sistemi di coordinate di GEOsim.

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int printlistcoord()
{
   C_STR_LIST    puntlist;
   C_STR         *p;
   int           cont = 0, type;
   struct resbuf result;

   acutPrintf(gsc_msg(161));                       // "\nSistemi di coordinate: "
   if (gsc_getgeosimcoord(puntlist) == GS_BAD) return GS_BAD;
   
   p = (C_STR *) puntlist.get_head();
   while (p)
   {
      acutPrintf(_T("\n  %-10s"), p->get_name());
      
      if ((++cont) % DISPLAY_LIST_STEP == 0)
      { 
         acutPrintf(gsc_msg(162));  // "\nPremi un tasto per continuare" 
         if (ads_grread(10, &type, &result) != RTNORM) break;
      }
      
      p = (C_STR *) puntlist.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_projsetwscode                      <internal> */
/*+
  Questa funzione setta il sistema di coordinate per la sessione corrente.
  Parametri:
  TCHAR *coordinate;  codice del sistema di coordinate

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_projsetwscode(TCHAR *coordinate)
{
   if (coordinate && wcslen(coordinate) > 0) 
      if (ade_projsetwscode(coordinate) != RTNORM)
         { GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_validcoord                         <external> */
/*+
  Questa funzione verifica la validità del codice del sistema 
  di coordinate.
  Parametri:
  const TCHAR *coord;  codice del sistema di coordinate

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validcoord(const TCHAR *coord)
{
   C_STRING CoordCopy(coord);

   CoordCopy.alltrim();
   if (CoordCopy.len() == 0) return GS_GOOD;
   if (CoordCopy.len() >= MAX_LEN_COORDNAME)
      { GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }   

   // ade_projgetctgyname non va se Acad è in fase di 
   // kUnloadDwgMsg, kQuitMsg, kUnloadAppMsg
   if (GEOsimAppl::LastAppMsgCode != AcRx::kUnloadDwgMsg &&
       GEOsimAppl::LastAppMsgCode != AcRx::kQuitMsg &&
       GEOsimAppl::LastAppMsgCode != AcRx::kUnloadAppMsg)
   {
      TCHAR *test = ade_projgetctgyname(CoordCopy.get_name());
      if (!test) { GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }   
      free(test);
   }

   return GS_GOOD;
}  


/*********************************************************/
/*.doc gs_validcoord                          <external> */
/*+
  Questa funzione LISP verifica la validità del codice del sistema 
  di coordinate.
  Parametri:
  lista resbuf = <codice coordinate>

  Ritorna RTNORM (T in LISP) in caso di successo altrimenti RTERROR (NIL in LISP)
-*/  
/*********************************************************/
int gs_validcoord(void)
{
   presbuf arg = acedGetArgs();
   
   acedRetNil();
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_validcoord(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_get_srid                           <external> */
/*+
  Questa funzione ottiene il codice di un sistema di coordinate (SRID) espresso 
  in una codifica nota (autocad, epsg, oracle, esri) partendo da un'altra codifica.
  Parametri:
  C_STRING &srid_in;             codice SRID in input
  GSSRIDTypeEnum SRIDType_in;    tipo SRID in input
  GSSRIDTypeEnum SRIDType_out;   tipo SRID in output
  C_STRING &srid_out;            codice SRID in output
  GSSRIDUnitEnum &unit_out;      unità di misura dello SRID

  Ritorna GS_GOOD se è stat trovato lo SRID altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_get_srid(C_STRING &srid_in, GSSRIDTypeEnum SRIDType_in,
                 GSSRIDTypeEnum SRIDType_out, C_STRING &srid_out, GSSRIDUnitEnum *unit_out)
{
   return gsc_get_srid(srid_in.get_name(), SRIDType_in, SRIDType_out, srid_out, unit_out);
}
int gsc_get_srid(const TCHAR *srid_in, GSSRIDTypeEnum SRIDType_in,
                 GSSRIDTypeEnum SRIDType_out, C_STRING &srid_out, GSSRIDUnitEnum *unit_out)
{
   C_STRING       statement, TableRef, StrSqlFmt(srid_in), DBMSName, SQLUpper;
   _RecordsetPtr  pRs;
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;
   presbuf        pSRID;

   if (gsc_getSRIDTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   DBMSName = pConn->get_DBMSName();
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   // ACCESS è l'unico stronzo a non supportare UPPER ma usa UCASE...
   // se incontro qualcuno che mi dice che l'SQL è uno standard gli tiro un pugno in faccia
   SQLUpper = (DBMSName.at(ACCESS_DBMSNAME, FALSE) != 0) ? _T("UCASE") : _T("UPPER");

   statement = _T("SELECT ");

   switch (SRIDType_out)
   {
      case GSSRID_Autodesk:
         statement += _T("AUTODESK");
         break;
      case GSSRID_EPSG:
         statement += _T("EPSG");
         break;
      case GSSRID_Oracle:
         statement += _T("ORACLE");
         break;
      case GSSRID_ESRI:
         statement += _T("ESRI");
         break;
      default:
         GS_ERR_COD = eGSInvalidCoord;
         return GS_BAD;
   }
   
   statement += _T(", UNIT FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += SQLUpper;
   statement += _T("(");

   switch (SRIDType_in)
   {
      case GSSRID_Autodesk:
         statement += _T("AUTODESK");
         break;
      case GSSRID_EPSG:
         statement += _T("EPSG");
         break;
      case GSSRID_Oracle:
         statement += _T("ORACLE");
         break;
      case GSSRID_ESRI:
         statement += _T("ESRI");
         break;
      default:
         GS_ERR_COD = eGSInvalidCoord;
         return GS_BAD;
   }

   statement += _T(") = ");
   statement += SQLUpper;
   statement += _T("(");
   if (pConn->Str2SqlSyntax(StrSqlFmt) == GS_BAD) return GS_BAD;
   statement += StrSqlFmt;
   statement += _T(")");

   // leggo la riga della tabella
   if (pConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);

   pSRID = gsc_nth(1, ColValues.nth(0));
   srid_out.paste(gsc_rb2str(pSRID)); // Converto SRID in stringa
   srid_out.alltrim();

   pSRID = gsc_nth(1, ColValues.nth(1));
   statement.paste(gsc_rb2str(pSRID)); // Converto UNIT in stringa
   statement.alltrim();
   statement.toupper();
   if (statement == _T("METER"))
      *unit_out = GSSRIDUnit_Meter;
   else if (statement == _T("DEGREE"))
      *unit_out = GSSRIDUnit_Degree;
   else if (statement == _T("MILE"))
      *unit_out = GSSRIDUnit_Mile;
   else if (statement == _T("INCH"))
      *unit_out = GSSRIDUnit_Inch;
   else if (statement == _T("FEET"))
      *unit_out = GSSRIDUnit_Feet;
   else if (statement == _T("KILOMETER"))
      *unit_out = GSSRIDUnit_Kilometer;
   else
      *unit_out = GSSRIDUnit_None;

   return (srid_out.len() == 0) ? GS_BAD : GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getCoordConv                       <external> */
/*+
  Questa funzione converte un punto da un sistema di coordinate autocad ad un altro.
  Parametri:  
  ads_point pt_src;
  TCHAR     *SrcCoordSystem;
  ads_point pt_dst;
  TCHAR     *DstCoordSystem;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getCoordConv(ads_point pt_src, TCHAR *SrcCoordSystem, ads_point pt_dst, TCHAR *DstCoordSystem)
{
   if (gsc_strlen(SrcCoordSystem) == 0 || gsc_strlen(DstCoordSystem) == 0)
      {  GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }
   if (ade_projsetsrc(SrcCoordSystem) != RTNORM)
      {  GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }
   if (ade_projsetdest(DstCoordSystem) != RTNORM)
      {  GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }
   if (ade_projptforward(pt_src, pt_dst) != RTNORM)
      {  GS_ERR_COD = eGSInvalidCoord; return GS_BAD; }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    COORDINATE    FINE    ///////////////////
//////////////////    GESTIONE    LAYER      INIZIO     ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_getActualLayerNameList             <external> */
/*+
  Questa funzione restituisce la lista dei layer correntemente definiti in ACAD.
  Parametri:
  C_STR_LIST &LayerNameList;    Lista dei nomi dei layer

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getActualLayerNameList(C_STR_LIST &LayerNameList)
{
   // Get a list of all of the layers
   AcDbLayerTable         *pTable;
   AcDbLayerTableIterator *pLayerIterator;
   AcDbObjectId           id;
   AcDbLayerTableRecord   *pLTRec;
   const TCHAR            *LayerName;

   LayerNameList.remove_all();
	if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pTable->newIterator(pLayerIterator, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pTable->close(); return GS_BAD; }

   while (!pLayerIterator->done())
   {
      if (pLayerIterator->getRecordId(id) != Acad::eOk)
         { delete pLayerIterator; pTable->close(); return GS_BAD; }

      if (acdbOpenAcDbObject((AcDbObject *&) pLTRec, id, AcDb::kForRead) != Acad::eOk)
         { delete pLayerIterator; pTable->close(); return GS_BAD; }

      pLTRec->getName(LayerName);
      LayerNameList.add_tail_str(LayerName);
         
      pLTRec->close();
      pLayerIterator->step();
   }

   delete pLayerIterator;
   pTable->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_validlayer                         <external> */
/*+
  Questa funzione verifica la validità del nome di un layer
  Parametri:
  const TCHAR *layer;    Nome del layer

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validlayer(const TCHAR *layer)
{
   size_t len;
   
   if (!layer) { GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }
   len = wcslen(layer);   
   if (len == 0 || len >= MAX_LEN_LAYERNAME)
      { GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }   
   
   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_getLayerId                         <external> */
/*+
  Questa funzione legge l'identificatore di un layer.
  Parametri:
  const TCHAR *layer;         Nome del layer (input)
  AcDbObjectId &Id;           Identificatore (output)
  AcDbLayerTable *pTable;     Tabella dei layer in cui cercare, se = NULL
                              usa il db corrente (default = NULL)
                                      
  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getLayerId(const TCHAR *layer, AcDbObjectId &Id, AcDbLayerTable *pTable)
{
	AcDbLayerTable *pMYTable;

   if (pTable) pMYTable = pTable;
   else 
   {
      AcDbDatabase *pDB = acdbHostApplicationServices()->workingDatabase();
   	if (pDB->getLayerTable(pMYTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   }

   if (pMYTable->getAt(layer, Id) != Acad::eOk)
      { if (!pTable) pMYTable->close(); return GS_BAD; }
   if (!pTable) pMYTable->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_is_lockedLayer                     <external> */
/*+
  Questa funzione verifica se il layer è bloccato.
  Parametri:
  const TCHAR *layer : Nome del layer

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_is_lockedLayer(const TCHAR *layer)
{
   C_RB_LIST Descr;
   presbuf   p;

   // leggo la sua descrizione
   if ((Descr << acdbTblSearch(_T("LAYER"), layer, 0)) == NULL) return GS_GOOD;

   if ((p = Descr.SearchType(70)) != NULL && p->resval.rint & 4) return GS_GOOD;

   return GS_BAD;
}

/*********************************************************/
/*.doc gsc_is_offLayer                        <external> */
/*+
  Questa funzione verifica se il layer è OFF/ON.
  Parametri:
  const TCHAR *layer : Nome del layer

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_is_offLayer(const TCHAR *layer)
{
   C_RB_LIST Descr;
   presbuf   p;

   // leggo la sua descrizione
   if ((Descr << acdbTblSearch(_T("LAYER"), layer, 0)) == NULL) return GS_GOOD;

   // per verificare se il piano è OFF si deve testare il codice che ritorna il
   // colore e verificare che è negativo.
   if ((p = Descr.SearchType(62)) != NULL && p->resval.rint < 0) return GS_GOOD;

   return GS_BAD;
}

/*********************************************************/
/*.doc gsc_is_freezeLayer                     <external> */
/*+
  Questa funzione verifica se il layer è congelato.
  Parametri:
  const TCHAR *layer : Nome del layer

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_is_freezeLayer(const TCHAR *layer)
{
   C_RB_LIST Descr;
   presbuf   p;

   // leggo la sua descrizione
   if ((Descr << acdbTblSearch(_T("LAYER"), layer, 0)) == NULL) return GS_GOOD;

   if ((p = Descr.SearchType(70)) != NULL && p->resval.rint & 1) return GS_GOOD;

   return GS_BAD;
}

/*********************************************************/
/*.doc gsc_crea_layer                     <external> */
/*+
  Questa funzione crea un nuovo layer.
  Parametri:
  const TCHAR *layer;         Nome del layer
  AcDbLayerTable *pTable;     Tabella dei layer in cui cercare, se = NULL
                              usa il db corrente (default = NULL)
  bool UseLayerModel;         Flag usato per utilizzare le caratteristiche del
                              modello di layer di GEOsim (se esistente);
                              default = true

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gs_crea_layer(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_crea_layer(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}
//int gsc_crea_layer(const TCHAR *layer)
//{
//   TCHAR           name[MAX_LEN_LAYERNAME];
//	AcDbLayerTable *pTable;
//   AcDbObjectId   layerId = AcDbObjectId::kNull;
//
//   gsc_strcpy(name, layer, MAX_LEN_LAYERNAME);
//   if (gsc_validlayer(name) == GS_BAD) return GS_BAD;
//
//   // Verifico la sua esistenza
//	if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
//      return GS_BAD;
//   // Use the overload of AcDbLayerTable::getAt() that returns the id
//	if (pTable->getAt(name, layerId, Adesk::kFalse) == Acad::eOk)
//      { pTable->close(); return GS_GOOD; }
//   pTable->close();
//
//   if (gsc_callCmd(_T("_.LAYER"), RTSTR, _T("_N"), RTSTR, name, RTSTR, GS_EMPTYSTR, 0) != RTNORM)
//      return GS_BAD;
//
//   return GS_GOOD;
//}

int gsc_crea_layer(const TCHAR *layer, AcDbLayerTable *pTable, bool UseLayerModel)
{
	AcDbLayerTable *pMYTable;

   if (pTable) pMYTable = pTable;
   else 
   {
      AcDbDatabase *pDB = acdbHostApplicationServices()->workingDatabase();
   	if (pDB->getLayerTable(pMYTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   }

   if (!pMYTable->has(layer))
   {
      if (pMYTable->upgradeOpen() != Acad::eOk)
         { if (!pTable) pMYTable->close(); GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }

      // Create a new layer table record using the layer name passed in
	   AcDbLayerTableRecord *pLayerRec = new AcDbLayerTableRecord;
	   if (pLayerRec->setName(layer) != Acad::eOk)
      {
         delete pLayerRec; 
         if (!pTable) pMYTable->close(); 
         GS_ERR_COD = eGSInvalidLayer;
         return GS_BAD;
      }

      // aggiungo nuovo layer
      if (pMYTable->add(pLayerRec) != Acad::eOk)
         { delete pLayerRec; if (!pTable) pMYTable->close(); return GS_BAD; }

      if (GS_CURRENT_WRK_SESSION && UseLayerModel)
      {
         C_LAYER_MODEL LayerModel;

         LayerModel.set_name(layer);
         if (gsc_getLayerModel(GS_CURRENT_WRK_SESSION->get_PrjId(), LayerModel) == GS_GOOD)
            LayerModel.to_LayerTableRecord(*pLayerRec);
      }

      pLayerRec->close();
   }

   if (!pTable) pMYTable->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_tmp_layer                      <external> */
/*+
  Questa funzione compone un nome di layer non ancora esistente
  Parametri:
  const TCHAR *prefix;   Prefisso   (es. "layer_")
  C_STRING &Layer;       Nome del layer (es. "layer_1")
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_get_tmp_layer(const TCHAR *prefix, C_STRING &Layer)
{
   int            i = 1;
	AcDbLayerTable *pTable;
   AcDbObjectId   layerId = AcDbObjectId::kNull;

	if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   do
   {
      // Compongo il nome del layer
      Layer = prefix;
      Layer += i++;     
   } // Verifico la sua esistenza
   while (pTable->getAt(Layer.get_name(), layerId, Adesk::kFalse) == Acad::eOk);
   pTable->close();

   return GS_GOOD;
}

/****************************************************************************/
/*.doc gs_set_CurrentLayer e gsc_set_CurrentLayer                <external> */
/*+
  Questa funzione setta il layer come corrente (se non esiste lo crea).
  Parametri:
  const TCHAR *layer;    nuovo layer
  TCHAR *old;            vecchio layer (già allocato a MAX_LEN_LAYERNAME)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gs_set_CurrentLayer(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_set_CurrentLayer(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();
   return RTNORM;
}
int gsc_set_CurrentLayer(const TCHAR *layer, TCHAR *old)
{
   AcDbDatabase   *pDB = acdbHostApplicationServices()->workingDatabase();
	AcDbLayerTable *pTable;
   AcDbObjectId   Id;
  
   // Memorizza vecchio valore in old
   if (old)
   {
      const ACHAR          *pName;
      AcDbLayerTableRecord *pRec;

      if (acdbOpenAcDbObject((AcDbObject *&) pRec, pDB->clayer(), AcDb::kForRead) != Acad::eOk)
         return GS_BAD;
      if (pRec->getName(pName) != Acad::eOk) { pRec->close(); return GS_BAD; }
      pRec->close();

      gsc_strcpy(old, pName, MAX_LEN_LAYERNAME); 
   }                              

	if (pDB->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   if (!pTable->has(layer))
   {
      if (gsc_crea_layer(layer, pTable) == GS_BAD)
         { pTable->close(); return GS_BAD; }
   }
   else
   {
      bool                 isFrozen = false, isLocked = false, isOff = false;
      int                  Flag = 0;
      AcDbLayerTableRecord *pRec;

      // Setto il layer scongelato e sbloccato
	   if (pTable->getAt(layer, pRec, AcDb::kForWrite, Adesk::kFalse) != Acad::eOk)
         { pTable->close(); return GS_BAD; }
      if (gsc_SetCharactSingleLayer(pRec, NULL, NULL, &isFrozen, &isLocked, &isOff) == GS_BAD)
         { pRec->close(); pTable->close(); return GS_BAD; }
      pRec->close();
   }

   // Leggo ID del layer e lo setto come corrente
   if (gsc_getLayerId(layer, Id, pTable) == GS_BAD) 
      { pTable->close(); return GS_BAD; }
   pTable->close();
   if (pDB->setClayer(Id) != Acad::eOk) { GS_ERR_COD = eGSAdsCommandErr;  return GS_BAD; }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_set_charact_layer <internal> */
/*+
  Questa funzione setta le caratterictiche grafiche (colore, tipolinea) del layer
  indicato (se non esiste lo crea).
  Parametri:
  const TCHAR *layer;                     nome layer
  TCHAR LineType[MAX_LEN_LINETYPENAME];   nome tipolinea    (default = NULL)
  int *color;                             codice colore     (default = NULL)
  int *Flag;                              Flag di layer     (default = NULL)
                                          1 = congelato
                                          2 = congelato per default nelle nuove finestre
                                          4 = bloccato
  int *LayerOff;                          layer OFF o ON    (default = NULL)
  bool *ToRegen;                          Settato a TRUE se è richiesta una rigenerazione
                                          al fine di rendere visibili gli oggetti nel caso
                                          di piano scongelato (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gsc_SetCharactSingleLayer(AcDbLayerTableRecord *pLTRec, const TCHAR *LineType,
                              C_COLOR *color, bool *isFrozen, bool *isLocked, bool *isOff, 
                              double *Transparency, bool *ToRegen)
{
   if (ToRegen) *ToRegen = FALSE;

   // Tipolinea
   if (gsc_strlen(LineType) > 0)
   {
      AcDbObjectId LineTypeId;

      gsc_load_linetype(LineType); // Carico il tipolinea se non ancora caricato
      if (gsc_getLineTypeId(LineType, LineTypeId) == GS_GOOD)
      if (pLTRec->linetypeObjectId() != LineTypeId)
         pLTRec->setLinetypeObjectId(LineTypeId);
   }

   // Colore
   if (color)
   {
      AcCmColor AcColor;
      
      if (gsc_C_COLOR_to_AcCmColor(*color, AcColor) == GS_GOOD)
         if (pLTRec->color() != AcColor)
            pLTRec->setColor(AcColor);
   }

   if (isFrozen)
      if (*isFrozen)
      {
         if (!pLTRec->isFrozen())
            pLTRec->setIsFrozen(TRUE);
      }
      else
         if (pLTRec->isFrozen())
         {
            pLTRec->setIsFrozen(FALSE);
            if (ToRegen) *ToRegen = TRUE;
         }
    
   if (isLocked)
      if (*isLocked)
      {
         if (!pLTRec->isLocked())
            pLTRec->setIsLocked(TRUE);
      }
      else
         if (pLTRec->isLocked())
            pLTRec->setIsLocked(FALSE);

   if (isOff)
      if (*isOff)
      {
         if (!pLTRec->isOff())
            pLTRec->setIsOff(TRUE);
      }
      else
         if (pLTRec->isOff())
            pLTRec->setIsOff(FALSE);

   if (Transparency)
      if (*Transparency != pLTRec->transparency().alphaPercent())
         pLTRec->transparency().setAlphaPercent(*Transparency);

   return GS_GOOD;
}
int gsc_SetCharactSingleLayer(const TCHAR *layer, const TCHAR *LineType,
                              C_COLOR *color, bool *isFrozen, bool *isLocked, bool *isOff, 
                              double *Transparency, bool *ToRegen)
{
	AcDbLayerTable       *pTable;
   AcDbLayerTableRecord *pLTRec;

   if (!layer) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (gsc_crea_layer(layer) == GS_BAD) return GS_BAD;

	if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
	if (pTable->getAt(layer, pLTRec, AcDb::kForWrite, Adesk::kFalse) != Acad::eOk)
      { pTable->close(); return GS_BAD; }
   pTable->close();

   if (gsc_SetCharactSingleLayer(pLTRec, LineType, color, isFrozen, isLocked, 
                                 isOff, Transparency, ToRegen) == GS_BAD)
      { pLTRec->close(); return GS_BAD; }
   pLTRec->close(); 

   return GS_GOOD;
}

int gsc_set_charact_layer(const TCHAR *layer, const TCHAR *LineType,
                          C_COLOR *color, bool *isFrozen, bool *isLocked, bool *isOff, 
                          double *Transparency, bool *ToRegen)
{
   C_RB_LIST List;

   if (!layer) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (gsc_strcmp(layer, _T("*")) == 0)
   {
      // Get a list of all of the layers
      AcDbObjectId           id;
      AcDbLayerTable         *pTable;
      AcDbLayerTableIterator *pLayerIterator;
      AcDbLayerTableRecord   *pLTRec;

	   if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;
      if (pTable->newIterator(pLayerIterator, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
         { pTable->close(); return GS_BAD; }

      while (!pLayerIterator->done())
      {
         if (pLayerIterator->getRecordId(id) != Acad::eOk)
            { delete pLayerIterator; pTable->close(); return GS_BAD; }

         if (acdbOpenAcDbObject((AcDbObject *&) pLTRec, id, AcDb::kForWrite) != Acad::eOk)
            { delete pLayerIterator; pTable->close(); return GS_BAD; }

         if (gsc_SetCharactSingleLayer(pLTRec, LineType, color, isFrozen, isLocked, isOff, 
                                       Transparency, ToRegen) == GS_BAD)
            { delete pLayerIterator; pTable->close(); pLTRec->close(); return GS_BAD; }
         
         pLTRec->close();

         pLayerIterator->step();
      }
      delete pLayerIterator;

      pTable->close();
   }
   else
      if (gsc_SetCharactSingleLayer(layer, LineType, color, isFrozen, isLocked, isOff, 
                                    Transparency, ToRegen) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}

/****************************************************************************/
/*.doc gsc_get_charact_layer <internal> */
/*+
  Questa funzione legge le caratteristiche grafiche (colore, tipolinea) del layer.
  Parametri:
  const TCHAR *layer;                     nome layer
  TCHAR LineType[MAX_LEN_LINETYPENAME];   nome tipolinea
  int *color;                             codice colore     (default = NULL)
  int *Flag;                              codice colore     (default = NULL)
  int *LayerOff;                          se layer spento     (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gsc_get_charact_layer(const TCHAR *layer, TCHAR LineType[MAX_LEN_LINETYPENAME], 
                          int *color, int *Flag, int *LayerOff)
{
   C_RB_LIST List;
   presbuf   p;

   if ((List << acdbTblSearch(_T("LAYER"), layer, 0)) == NULL)
      { GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }

   if ((p = List.SearchType(6)) != NULL)
      gsc_strcpy(LineType, p->resval.rstring, MAX_LEN_LINETYPENAME);

   if ((color || LayerOff) && (p = List.SearchType(62)) != NULL)
   {
      if (color) *color = abs(p->resval.rint);
      if (LayerOff) *LayerOff = (p->resval.rint < 0) ? TRUE : FALSE;
   }
   if (Flag && (p = List.SearchType(70)) != NULL) *Flag = p->resval.rint;

   return GS_GOOD;
}
void gsc_get_charact_layer(AcDbLayerTableRecord *pLTRec, TCHAR LineType[MAX_LEN_LINETYPENAME], 
                          C_COLOR *color, bool *isFrozen, bool *isLocked, bool *isOff,
                          double *Transparency)
{
   gsc_strcpy(LineType, gsc_getSymbolName(pLTRec->linetypeObjectId()), MAX_LEN_LINETYPENAME);  
   if (color) gsc_AcCmColor_to_C_COLOR(pLTRec->color(), *color);
   if (isFrozen) *isFrozen = pLTRec->isFrozen();
   if (isLocked) *isLocked = pLTRec->isLocked();
   if (isOff) *isOff = pLTRec->isOff();
   if (Transparency) *Transparency = pLTRec->transparency().alphaPercent();
}
int gsc_get_charact_layer(const TCHAR *layer, TCHAR LineType[MAX_LEN_LINETYPENAME], 
                          C_COLOR *color, bool *isFrozen, bool *isLocked, bool *isOff,
                          double *Transparency)
{
   AcDbDatabase         *pDB = acdbHostApplicationServices()->workingDatabase();
	AcDbLayerTable       *pLayerTable;
   AcDbLayerTableRecord *pLTRec;

	if (pDB->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
	if (pLayerTable->getAt(layer, pLTRec, AcDb::kForRead, Adesk::kFalse) != Acad::eOk)
      { pLayerTable->close(); GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }
   gsc_get_charact_layer(pLTRec, LineType, color, isFrozen, isLocked, isOff, Transparency);
   pLTRec->close();
   pLayerTable->close();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_save_layer_status                  <external> */
/*+
  Questa funzione salva su file lo stato dei layer attuali.
  Parametri:
  const TCHAR *FileName;  Path completa del file

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gs_save_layer_status(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_save_layer_status(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}
int gsc_save_layer_status(const TCHAR *FileName)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection, *LayersProfileSection;
   int       i = 1;
   C_STRING  LayerName, strColor;
   C_RB_LIST RbList;
   TCHAR     LineType[MAX_LEN_LINETYPENAME];
   C_COLOR   color;
   bool      isFrozen, isLocked, isOff;
   double    Transparency;
   TCHAR     buf[SEZ_PROFILE_LEN];
   AcDbObjectId           id;
   AcDbLayerTable         *pTable;
   AcDbLayerTableIterator *pLayerIterator;
   AcDbLayerTableRecord   *pLTRec;

   if (ProfileSections.add(_T("LAYERS")) == GS_BAD) return GS_BAD;
   LayersProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();

	if (acdbHostApplicationServices()->workingDatabase()->getLayerTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pTable->newIterator(pLayerIterator, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pTable->close(); return GS_BAD; }

   while (!pLayerIterator->done())
   {
      if (pLayerIterator->getRecordId(id) != Acad::eOk)
         { delete pLayerIterator; pTable->close(); return GS_BAD; }
      if (acdbOpenAcDbObject((AcDbObject *&) pLTRec, id, AcDb::kForRead) != Acad::eOk)
         { delete pLayerIterator; pTable->close(); return GS_BAD; }
      // Nome
      LayerName = gsc_getSymbolName(pLTRec->objectId());  
      gsc_get_charact_layer(pLTRec, LineType, &color, &isFrozen, &isLocked, &isOff, &Transparency);
      pLTRec->close();

      swprintf(buf, SEZ_PROFILE_LEN, _T("%d"), i++);
      LayersProfileSection->set_entry(buf, LayerName.get_name());

      if (ProfileSections.add(LayerName.get_name()) == GS_BAD)
         { delete pLayerIterator; pTable->close(); return GS_BAD; }
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
      // Nome
      ProfileSection->set_entry(_T("NAME"), LayerName.get_name());
      // Colore
      if (color.getColorMethod() == C_COLOR::ByRGBColor) color.getRGBDecColor(strColor, _T(','));
      else
      {
         int AutoCADColorIndex;
         color.getAutoCADColorIndex(&AutoCADColorIndex);
         strColor = AutoCADColorIndex;
      }
      ProfileSection->set_entry(_T("COLOR"), strColor.get_name());
      // Tipo di linea
      ProfileSection->set_entry(_T("LINETYPE"), LineType);
      // Congelato
      ProfileSection->set_entry(_T("ISFROZEN"), isFrozen);
      // Bloccato
      ProfileSection->set_entry(_T("ISLOCKED"), isLocked);
      // Spento
      ProfileSection->set_entry(_T("ISOFF"), isOff);
      // Trasparenza
      ProfileSection->set_entry(_T("TRANSPARENCY"), Transparency);

      pLayerIterator->step();
   }

   delete pLayerIterator;
   pTable->close();
   
   return gsc_write_profile(FileName, ProfileSections);
}

/*********************************************************/
/*.doc gsc_load_layer_status                  <external> */
/*+
  Questa funzione carica da file lo stato dei layer attuali.
  Parametri:
  const TCHAR *FileName;  Path completa del file

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gs_load_layer_status(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil(); 
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_load_layer_status(arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}
int gsc_load_layer_status(const TCHAR *FileName)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_2STR_BTREE            *pProfileEntries;
   C_B2STR                 *pProfileEntry;
   TCHAR                   StrNum[10]; 
   C_STRING                LayerName, LineType, strColor;
   int                     result = GS_GOOD, i = 1;
   C_COLOR                 Color;
   int                     PrevErr = GS_ERR_COD;
   bool                    isOff, isFrozen, isLocked, ToRegen;
   double                  Transparency;

   if (gsc_read_profile(FileName, ProfileSections) == GS_BAD) return GS_BAD;
   // il ciclo successivo può essere lungo soprattutto se deve creare nuovi layer
   acutPrintf(_T("\n%s"), gsc_msg(360)); // "Attendere..."

   do
   {
      swprintf(StrNum, 10, _T("%d"), i++);
      if (!(pProfileEntry = ProfileSections.get_entry(_T("LAYERS"), StrNum)))
      {
         // fine della lista dei layer
         GS_ERR_COD = PrevErr;
         break;
      }
      LayerName = pProfileEntry->get_name2();

      if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(LayerName.get_name())))
         { result = GS_BAD; break; }
      pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("COLOR"))))
         { result = GS_BAD; break; }
      if (strColor.isDigit() == GS_GOOD) Color.setAutoCADColorIndex(strColor.toi());
      else Color.setRGBDecColor(strColor, _T(','));
      // Tipo di linea
      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("LINETYPE"))))
         { result = GS_BAD; break; }
      LineType = pProfileEntry->get_name2();
      // Congelato
      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ISFROZEN"))))
         { result = GS_BAD; break; }
      isFrozen = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;
      // Bloccato
      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ISLOCKED"))))
         { result = GS_BAD; break; }
      isLocked = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;
      // Spento
      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ISOFF"))))
         { result = GS_BAD; break; }
      isOff = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;
      // Trasparenza
      if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("TRANSPARENCY"))))
         Transparency = 0;
      else
         Transparency = _wtof(pProfileEntry->get_name2());

      if (gsc_set_charact_layer(LayerName.get_name(), LineType.get_name(), 
                                &Color, &isFrozen, &isLocked, &isOff, &Transparency, &ToRegen) == GS_BAD)
         { result = GS_BAD; break; }                                      
   }
   while (1);

   if (ToRegen)
      acutPrintf(gsc_msg(440)); // \nE' necessario rigenerare la grafica..."

   return result;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layers" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layers_accept(ads_callback_packet *dcl)
{
   TCHAR val[MAX_LEN_LAYERNAME], *actual;
   Common_Dcl_Struct *common_struct = (Common_Dcl_Struct *) dcl->client_data;

   actual = common_struct->actual;
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if (ads_get_tile(dcl->dialog, _T("edit"), val, MAX_LEN_LAYERNAME) != RTNORM) return;
   if (gsc_validlayer(val) == GS_GOOD)
   {
      gsc_strcpy(actual, val, MAX_LEN_LAYERNAME);
      ads_done_dialog(dcl->dialog, DLGOK);
   }
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidLayer));  // "*Errore* Layer non valido"
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layers" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layers_list(ads_callback_packet *dcl)
{
   TCHAR     val[10];
   presbuf   p;
   Common_Dcl_Struct *common_struct = (Common_Dcl_Struct *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   
   if (ads_get_tile(dcl->dialog, _T("list"), val, 10) != RTNORM) return;
   if ((p = common_struct->List.getptr_at(_wtoi(val) + 1)) == NULL) return;

   ads_set_tile(dcl->dialog, _T("edit"), p->resval.rstring);
      
   if (dcl->reason == CBR_DOUBLE_CLICK) dcl_layers_accept(dcl);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layers" su controllo "edit"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layers_edit(ads_callback_packet *dcl)
{
   if (dcl->reason == CBR_LOST_FOCUS) return;

   dcl_layers_accept(dcl);
}

/*********************************************************/
/*.doc gs_ddsellayers                         <external> */
/*+
  Questa funzione permette la scelta di un layer tramite
  interfaccia a finestra
  Parametro:
  il tipo di linea di default (opzionale)
  
  oppure
  TCHAR *current[MAX_LEN_LAYERNAME]; layer corrente

  Restituisce un resbuf con il nome del layer scelto in caso di successo
  altrimenti NULL.
-*/  
/*********************************************************/
int gs_ddsellayers(void)
{
   TCHAR   current[MAX_LEN_LAYERNAME] = GS_EMPTYSTR;
   presbuf arg = acedGetArgs(), p;

   acedRetNil();

   if (arg && arg->restype == RTSTR) // leggo layer corrente (opzionale)
      gsc_strcpy(current, arg->resval.rstring, MAX_LEN_LAYERNAME);

   if ((p = gsc_ddsellayers(current)) != NULL)
   {
      acedRetStr(p->resval.rstring);
      acutRelRb(p);
   }
   return RTNORM;
}
presbuf gsc_ddsellayers(TCHAR *current)
{
   C_STRING   path;
   TCHAR      strPos[10] = _T("0");
   int        status = DLGOK, dcl_file, pos = 0;
   ads_hdlg   dcl_id;
   C_RB_LIST  DescrLayer;
   presbuf    p;
   C_STR_LIST LayerNameList;
   Common_Dcl_Struct common_struct;

   // leggo la lista dei layer attuale
   if (gsc_getActualLayerNameList(LayerNameList) == GS_BAD) return GS_BAD;
   // ordino la lista degli elementi per nome
   LayerNameList.sort_name(TRUE, TRUE); // sensitive e ascending
   common_struct.List << LayerNameList.to_rb();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("layers"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return NULL; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW,0);

   p = common_struct.List.get_head();
   while (p && p->restype == RTSTR)
   {
      if (current && gsc_strcmp(p->resval.rstring, current, FALSE) == 0)
         swprintf(strPos, 10, _T("%d"), pos);
      else pos++;

      gsc_add_list(p->resval.rstring);
      
      p = p->rbnext;
   }

   ads_end_list();

   ads_set_tile(dcl_id, _T("list"), strPos);
   wcscpy(common_struct.actual, GS_EMPTYSTR);
   if (current)
      gsc_strcpy(common_struct.actual, current, MAX_LEN_LAYERNAME);

   ads_set_tile(dcl_id, _T("edit"), common_struct.actual);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_layers_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &common_struct);
   ads_action_tile(dcl_id, _T("list"),   (CLIENTFUNC) dcl_layers_list);
   ads_client_data_tile(dcl_id, _T("list"), &common_struct);
   ads_action_tile(dcl_id, _T("edit"),   (CLIENTFUNC) dcl_layers_edit);
   ads_client_data_tile(dcl_id, _T("edit"), &common_struct);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_RB_LIST result;

      if ((result << acutBuildList(RTSTR, common_struct.actual,0)) == NULL) return NULL;
      result.ReleaseAllAtDistruction(GS_BAD);

      return result.get_head();
   }
   else return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "multiSelectionLayers" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_multiSelectionLayers_accept(ads_callback_packet *dcl)
{
   Common_Dcl_Struct *common_struct = (Common_Dcl_Struct *) dcl->client_data;
   
   if (ads_get_tile(dcl->dialog, _T("list"), common_struct->Selected, TILE_STR_LIMIT) != RTNORM) return;
   ads_done_dialog(dcl->dialog, DLGOK);
}
/*********************************************************/
/*.doc gsc_ddMultiSelectLayers                <external> */
/*+
  Questa funzione permette la scelta multipla di layers tramite
  interfaccia a finestra.
  Parametro:
  C_STR_LIST *in_layerList;   opzionale, Lista di layer tra cui selezionare
                              se = NULL verranno visualizzati tutti i layer di ACAD
  C_STR_LIST *out_layerList;  Lista di layer selezionati

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_ddMultiSelectLayers(C_STR_LIST *in_layerList, C_STR_LIST &out_layerList)
{
   C_STRING   path;
   int        status = DLGOK, dcl_file;
   ads_hdlg   dcl_id;
   C_RB_LIST  DescrLayer;
   presbuf    p;
   C_STR_LIST LayerNameList;
   Common_Dcl_Struct common_struct;

   out_layerList.remove_all();
   if (in_layerList == NULL)
   {  // leggo la lista dei layer attuale
      if (gsc_getActualLayerNameList(LayerNameList) == GS_BAD) return GS_BAD;
   }
   else
      in_layerList->copy(&LayerNameList);

   // ordino la lista degli elementi per nome
   LayerNameList.sort_name(TRUE, TRUE); // sensitive e ascending
   common_struct.List << LayerNameList.to_rb();

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("multiSelectionLayers"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW,0);

   p = common_struct.List.get_head();
   while (p && p->restype == RTSTR)
   {
      gsc_add_list(p->resval.rstring);     
      p = p->rbnext;
   }

   ads_end_list();

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_multiSelectionLayers_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &common_struct);
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_INT_LIST posList;
      C_INT      *pPos;
      presbuf    pRb;

      posList.from_str(common_struct.Selected, _T(' '));
      pPos = (C_INT *) posList.get_head();
      while (pPos)
      {
         if ((pRb = common_struct.List.getptr_at(pPos->get_key() + 1)) == NULL) return GS_BAD;
         out_layerList.add_tail_str(pRb->resval.rstring);
         pPos = (C_INT *) posList.get_next();
      }

      return GS_GOOD;
   }
   
   return GS_BAD;
}


//---------------------------------------------------------------------------//
// class C_LAYER_SETTINGS                                                     //
//---------------------------------------------------------------------------//

C_LAYER_SETTINGS::C_LAYER_SETTINGS() : C_2STR() {}

C_LAYER_SETTINGS::~C_LAYER_SETTINGS() {}

double C_LAYER_SETTINGS::get_min_scale(void) { return name.tof(); }
int C_LAYER_SETTINGS::set_min_scale(double in)
{
   if (in < 0) return GS_BAD;
   name = in;
   return GS_GOOD;
}


int C_LAYER_SETTINGS::set_path(const TCHAR *in) { return set_name2(in); }
TCHAR* C_LAYER_SETTINGS::get_path(void) { return get_name2(); }

int C_LAYER_SETTINGS::apply(void)
{
   return gsc_load_layer_status(get_path());
}


//---------------------------------------------------------------------------//
// class C_LAYER_SETTINGS - FINE                                             //
// class C_LAYER_DISPLAY_MODEL - INIZIO                                       //
//---------------------------------------------------------------------------//


C_LAYER_DISPLAY_MODEL::C_LAYER_DISPLAY_MODEL() : C_2STR_LIST() {}

C_LAYER_DISPLAY_MODEL::~C_LAYER_DISPLAY_MODEL() {}

TCHAR *C_LAYER_DISPLAY_MODEL::get_path(void)
   { return Path.get_name(); }

TCHAR *C_LAYER_DISPLAY_MODEL::get_descr(void)
   { return Descr.get_name(); }

int C_LAYER_DISPLAY_MODEL::set_descr(const TCHAR *in)
   { return Descr.set_name(in); }

void C_LAYER_DISPLAY_MODEL::clear()
{
   Path.clear();
   Descr.clear();
   remove_all();
}

int C_LAYER_DISPLAY_MODEL::copy(C_LAYER_DISPLAY_MODEL &out)
{
   C_LAYER_SETTINGS *pLyrSettings, *pNew;

   out.Path  = Path;
   out.Descr = Descr;
   out.remove_all();

   pLyrSettings = (C_LAYER_SETTINGS *) get_head();
   while (pLyrSettings)
   {
      if ((pNew = new C_LAYER_SETTINGS()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }      
      out.add_tail(pNew);
      pNew->set_min_scale(pLyrSettings->get_min_scale());
      pNew->set_path(pLyrSettings->get_path());

      pLyrSettings = (C_LAYER_SETTINGS *) get_next();
   }

   return GS_GOOD;
}

int C_LAYER_DISPLAY_MODEL::save(C_STRING &_Path)
{
   C_LAYER_SETTINGS *pLyr = (C_LAYER_SETTINGS *) get_head();
   C_STRING         Entry, Value;

   // Cancello eventuali intervalli esistenti
   if (gsc_path_exist(_Path) == GS_GOOD)
      if (gsc_delete_sez(_Path.get_name(), _T("Intervals")) == GS_BAD &&
         GS_ERR_COD != eGSSezNotFound)
         return GS_BAD;

   while (pLyr)
   {
      Entry = pLyr->get_min_scale();
      Value = pLyr->get_path();
      gsc_drive2nethost(Value);

      if (gsc_set_profile(_Path, _T("Intervals"), Entry.get_name(), Value.get_name()) == GS_BAD)
         return GS_BAD;

      pLyr = (C_LAYER_SETTINGS *) get_next();
   }

   if (gsc_set_profile(_Path, _T("Header"), _T("Description"), Descr.get_name()) == GS_BAD)
      return GS_BAD;

   Path = _Path;

   return GS_GOOD;
}

int C_LAYER_DISPLAY_MODEL::load(C_STRING &_Path)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_2STR_BTREE            *pProfileEntries;
   C_B2STR                 *pProfileEntry;
   C_STRING                Value;

   remove_all();

   // leggo la lista degli intervalli
   if (gsc_read_profile(_Path, ProfileSections) == GS_BAD) return GS_BAD;
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(_T("Intervals"))))
      return GS_BAD;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   pProfileEntry = (C_B2STR *) pProfileEntries->go_top();
   while (pProfileEntry)
   {
      Value = pProfileEntry->get_name();
      gsc_nethost2drive(Value);
      add_tail_2str(Value.get_name(), pProfileEntry->get_name2());

      pProfileEntry = (C_B2STR *) pProfileEntries->go_next();
   }
   sort_nameByNum(); // ordino per limite inferiore degli intervalli

   if ((pProfileEntry = ProfileSections.get_entry(_T("Header"), _T("Description"))))
      Descr = pProfileEntry->get_name2();
   Path = _Path;

   return GS_GOOD;
}

int C_LAYER_DISPLAY_MODEL::apply(double ZoomFactor)
{
   C_LAYER_SETTINGS *pLyr = (C_LAYER_SETTINGS *) get_head();

   if ((pLyr = searchByZoomFactor((long) ZoomFactor)))
      return pLyr->apply();

   return GS_GOOD;
}

C_LAYER_SETTINGS* C_LAYER_DISPLAY_MODEL::searchByZoomFactor(double ZoomFactor)
{
   C_LAYER_SETTINGS *pLyr = (C_LAYER_SETTINGS *) get_tail();

   if (ZoomFactor == 0) // leggo quello attuale di Autocad
   {
      resbuf res;

      // leggo il valore di VIEWSIZE (altezza della finestra) 
      if (acedGetVar(_T("VIEWSIZE"), &res) != RTNORM) return GS_BAD;
      ZoomFactor = res.resval.rreal;
   }

   // parto dal fondo e torno indietro
   while (pLyr)
   {
      // Se è superiore o uguale al fattore minimo di scala
      if (ZoomFactor >= pLyr->get_min_scale()) break;

      pLyr = (C_LAYER_SETTINGS *) get_prev();
   }

   // il primo fattore minimo di scala significa -infinito
   if (!pLyr) pLyr = (C_LAYER_SETTINGS *) get_head();

   return pLyr;
}


//---------------------------------------------------------------------------//
// class C_LAYER_DISPLAY_MODEL - FINE                                        //
// class C_LAYER_MODEL         - INIZIO                                      //
//---------------------------------------------------------------------------//


C_LAYER_MODEL::C_LAYER_MODEL() : C_STR() 
{
   Color.setAutoCADColorIndex(7);
   LineType   = _T("CONTINUOUS");
   LineWeight = AcDb::kLnWtByLwDefault;
   TransparencyPercent = 0; // totalmente opaco
}

C_LAYER_MODEL::~C_LAYER_MODEL() {}

int C_LAYER_MODEL::set_color(C_COLOR &in) { Color = in; return GS_GOOD; }
void C_LAYER_MODEL::get_color(C_COLOR &out) { out = Color; }

int C_LAYER_MODEL::set_linetype(const TCHAR *in) { LineType = in; return GS_GOOD; }
TCHAR *C_LAYER_MODEL::get_linetype(void) { return LineType.get_name(); }

int C_LAYER_MODEL::set_lineweight(AcDb::LineWeight in) { LineWeight = in; return GS_GOOD; }
AcDb::LineWeight C_LAYER_MODEL::get_lineweight(void) { return LineWeight; }

int C_LAYER_MODEL::set_plotstyle(const TCHAR *in) { PlotStyle = in; return GS_GOOD; }
TCHAR *C_LAYER_MODEL::get_plotstyle(void) { return PlotStyle.get_name(); }

int C_LAYER_MODEL::set_description(const TCHAR *in) { Description = in; return GS_GOOD; }
TCHAR *C_LAYER_MODEL::get_description(void) { return Description.get_name(); }

int C_LAYER_MODEL::set_TransparencyPercent(double percent)
{ 
   if (TransparencyPercent < 0 || TransparencyPercent > 100) return GS_BAD;
   TransparencyPercent = percent;
   return GS_GOOD; 
}
double C_LAYER_MODEL::get_TransparencyPercent(void) { return TransparencyPercent; }


/*********************************************************/
/*.doc C_LAYER_MODEL::from_rb                 <external> */
/*+
  Questa funzione inizializza l'oggetto C_LAYER_MODEL con le 
  proprietà lette sottoforma di lista di coppie di resbuf ((<prop> <val>) ...).
  (("NAME" <name>) [("COLOR" <color>)] [("LINE_TYPE" <lineType>)] 
   [("LINE_WEIGHT" <lineWeight>)] [("PLOT_STYLE_NAME" <plotStyleName>)]
   [("DESCRIPTION" <description>)] [("TRANSPARENCY_PERCENT" <trasparency>)])
  Parametri:
  C_RB_LIST &List;                  Lista delle proprietà (input)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LAYER_MODEL::from_rb(C_RB_LIST &List)
{
   presbuf p;

   if ((p = List.CdrAssoc(_T("NAME"))) != NULL && p->restype == RTSTR) // nome Layer
      set_name(p->resval.rstring);

   if ((p = List.CdrAssoc(_T("COLOR"))) != NULL) // Colore Layer
      Color.setResbuf(p);

   if ((p = List.CdrAssoc(_T("LINE_TYPE"))) != NULL && p->restype == RTSTR) // tipolinea Layer
      set_linetype(p->resval.rstring);

   if ((p = List.CdrAssoc(_T("LINE_WEIGHT"))) != NULL) // Spessore Layer
   {
      int LineWeight;

      if (gsc_rb2Int(p, &LineWeight) == GS_GOOD)
         set_lineweight((AcDb::LineWeight) LineWeight);
   }

   if ((p = List.CdrAssoc(_T("PLOT_STYLE_NAME"))) != NULL && p->restype == RTSTR) // Stile di stampa Layer
      set_plotstyle(p->resval.rstring);

   if ((p = List.CdrAssoc(_T("DESCRIPTION"))) != NULL && p->restype == RTSTR) // Descrizione Layer
      set_description(p->resval.rstring);

   if ((p = List.CdrAssoc(_T("TRANSPARENCY_PERCENT"))) != NULL) // Percentuale di trasparenza Layer
   {
      double Percent;

      if (gsc_rb2Dbl(p, &Percent) == GS_GOOD) set_TransparencyPercent(Percent);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_LAYER_MODEL::to_rb                   <external> */
/*+
  Questa funzione restituisce le proprietà lette
  sottoforma di lista di coppie di resbuf ((<proprietà> <valore>) ...)
  Parametri:

  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf C_LAYER_MODEL::to_rb(void)
{
   C_RB_LIST List;
   C_STRING  _Name, _LineType, _PlotStyle, _Description;

   _Name        = (gsc_strlen(get_name()) > 0) ? get_name() : GS_EMPTYSTR;
   _LineType    = (gsc_strlen(get_linetype()) > 0) ? get_linetype() : GS_EMPTYSTR;
   _PlotStyle   = (gsc_strlen(get_plotstyle()) > 0) ? get_plotstyle() : GS_EMPTYSTR;
   _Description = (gsc_strlen(get_description()) > 0) ? get_description() : GS_EMPTYSTR;

   if ((List << acutBuildList(RTLB,
                                 RTLB,
                                    RTSTR, _T("NAME"),
                                    RTSTR, _Name.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("COLOR"),
                              0)) == NULL) return NULL;
   if ((List += Color.getResbuf()) == NULL) return NULL;
   if ((List += acutBuildList(   RTLE,
                                 RTLB,
                                    RTSTR, _T("LINE_TYPE"),
                                    RTSTR, _LineType.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("LINE_WEIGHT"),
                                    RTSHORT, (int) get_lineweight(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("PLOT_STYLE_NAME"),
                                    RTSTR, _PlotStyle.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("DESCRIPTION"),
                                    RTSTR, _Description.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("TRANSPARENCY_PERCENT"),
                                    RTREAL, get_TransparencyPercent(),
                                 RTLE,
                              RTLE,
                              0)) == NULL) return NULL;

   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


/*********************************************************/
/*.doc C_LAYER_MODEL::to_LayerTableRecord     <external> */
/*+
  Questa funzione restituisce le proprietà lette
  sottoforma di oggetto AcDbLayerTableRecord.
  Parametri:
  AcDbLayerTableRecord &LayerRec; output

  Restituisce GS_GOOD di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LAYER_MODEL::to_LayerTableRecord(AcDbLayerTableRecord &LayerRec)
{
   // nome Layer
   if (gsc_strlen(get_name()) > 0)
      if (LayerRec.setName(get_name()) != Acad::eOk) return GS_BAD;

   // colore Layer
   AcCmColor _color;
   if (gsc_C_COLOR_to_AcCmColor(Color, _color) == GS_GOOD)
      LayerRec.setColor(_color);

   // tipolinea Layer
   if (gsc_strlen(get_linetype()) > 0)
   {
      AcDbObjectId _LineTypeId;

      gsc_load_linetype(get_linetype()); // Carico il tipolinea se non ancora caricato
      if (gsc_getLineTypeId(get_linetype(), _LineTypeId) == GS_GOOD)
         LayerRec.setLinetypeObjectId(_LineTypeId);
   }

   // spessore linea Layer
   LayerRec.setLineWeight(get_lineweight());

   // stile di stampa Layer
   if (gsc_strlen(get_plotstyle()) > 0)
      LayerRec.setPlotStyleName(get_plotstyle());

   // descrizione Layer
   if (gsc_strlen(get_description()) > 0)
      LayerRec.setDescription(get_description());

   // percentuale di trasparenza
   if (get_TransparencyPercent() != -999) // valore non valido che significa non settato
   {
      AcCmTransparency _transparency(1.0 - (get_TransparencyPercent() / 100.0));
      LayerRec.setTransparency(_transparency);
   }

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_LAYER_MODEL::to_LayerTableRecord     <external> */
/*+
  Questa funzione setta le proprietà del modello leggendolo dal corrispondente
  layer in AcDbLayerTableRecord.
  Parametri:
  const TCHAR *layer;               Nome del layer
  oppure
  AcDbLayerTableRecord &LayerRec;   Oggetto layer

  Restituisce GS_GOOD di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LAYER_MODEL::from_LayerTableRecord(const TCHAR *layer)
{
   AcDbDatabase         *pDB = acdbHostApplicationServices()->workingDatabase();
	AcDbLayerTable       *pLayerTable;
   AcDbLayerTableRecord *pLTRec;

	if (pDB->getLayerTable(pLayerTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
	if (pLayerTable->getAt(layer, pLTRec, AcDb::kForRead, Adesk::kFalse) != Acad::eOk)
      { pLayerTable->close(); GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }
   from_LayerTableRecord(*pLTRec);
   pLTRec->close();
   pLayerTable->close();
   return GS_GOOD;
}
int C_LAYER_MODEL::from_LayerTableRecord(AcDbLayerTableRecord &LayerRec)
{
   const ACHAR *pName;

   // nome Layer
   LayerRec.getName(pName);
   set_name(pName);

   // colore Layer
   gsc_AcCmColor_to_C_COLOR(LayerRec.color(), Color);

   // tipolinea Layer
   LineType = gsc_getSymbolName(LayerRec.linetypeObjectId());

   // spessore linea Layer
   LineWeight = LayerRec.lineWeight();

   // stile di stampa Layer
   PlotStyle.paste(LayerRec.plotStyleName());

   // descrizione Layer
   Description = LayerRec.description();

   // percentuale di trasparenza
   TransparencyPercent = (int) ((1 - LayerRec.transparency().alphaPercent()) * 100);

   return GS_GOOD;
}

//---------------------------------------------------------------------------//
// class C_LAYER_MODEL         - FINE                                        //
// class C_LAYER_MODEL_LIST    - INIZIO                                      //
//---------------------------------------------------------------------------//


C_LAYER_MODEL_LIST::C_LAYER_MODEL_LIST() : C_STR_LIST() {}

C_LAYER_MODEL_LIST::~C_LAYER_MODEL_LIST() {}


/*********************************************************/
/*.doc C_LAYER_MODEL_LIST::to_rb              <external> */
/*+
  Questa funzione restituisce la lista dei modelli di layer.
  Parametri:

  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
resbuf *C_LAYER_MODEL_LIST::to_rb(void)
{
   C_RB_LIST     List;
   C_LAYER_MODEL *pLayerModel = (C_LAYER_MODEL *) get_head();

   while (pLayerModel)
   {
      if ((List += pLayerModel->to_rb()) == NULL) return GS_BAD;
      pLayerModel = (C_LAYER_MODEL *) get_next();
   }

   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


//---------------------------------------------------------------------------//
// Funzioni per entità                                                       //
//---------------------------------------------------------------------------//


/***************************************************************************/
/*.doc gsc_setLayer                                                        */
/*+                                                                       
  Funzione che cambia il layer ad un oggetto grafico.
  Parametri:
  ads_name  ent;
  TCHAR *Layer;      Layer
  bool CheckLayer;   Opzionale, se = true controlla l'esistenza 
                     ed eventualmente crea il layer (default = true)
  oppure
  AcDbObjectId &objId;
  TCHAR *Layer;      Layer (se non esiste lo crea)
  bool CheckLayer;   Opzionale, se = true controlla l'esistenza 
                     ed eventualmente crea il layer (default = true)
  oppure
  AcDbEntity *pEnt;
  TCHAR *Layer;      Layer (se non esiste lo crea)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************************/
int gsc_setLayer(ads_name ent, const TCHAR *Layer)
{
   AcDbObjectId objId;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return gsc_setLayer(objId, Layer);
}
int gsc_setLayer(AcDbObjectId &objId, const TCHAR *Layer)
{
   AcDbEntity *pEnt;
   int         Res;

   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   Res = gsc_setLayer(pEnt, Layer);
   pEnt->close();

   return Res;
}
int gsc_setLayer(AcDbEntity *pEnt, const TCHAR *Layer)
{
   TCHAR *entLayer = pEnt->layer(); // alloca una stringa
   if (gsc_strcmp(entLayer, Layer) != 0)
   {
      acutDelString(entLayer);
      Acad::ErrorStatus Res = pEnt->upgradeOpen();      
      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

      if (pEnt->setLayer(Layer) != Acad::eOk)
      {   // provo a creare il layer
         if (gsc_crea_layer(Layer) != GS_GOOD)
            { GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }
         // riprovo a impostare il layer
         if (pEnt->setLayer(Layer) != Acad::eOk)
            { GS_ERR_COD = eGSInvalidLayer; return GS_BAD; }
      }
   }
   else
      acutDelString(entLayer);

   return GS_GOOD;
}


/***************************************************************************/
/*.doc gsc_getLayer                                                        */
/*+                                                                       
  Funzione che legge il layer di un oggetto grafico.
  Parametri:
  ads_name ent;
  TCHAR    **Layer;      Nome layer
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria.
-*/  
/***************************************************************************/
int gsc_getLayer(ads_name ent, C_STRING &Layer)
{
   TCHAR *_layer;

   if (gsc_getLayer(ent, &_layer) == GS_BAD) return GS_BAD;
   Layer.paste(_layer);
   return GS_GOOD;
}
int gsc_getLayer(ads_name ent, TCHAR **Layer)
{
   AcDbObjectId objId;
   AcDbEntity   *pEnt;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pEnt, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }   
   *Layer = pEnt->layer(); // alloca memoria
   pEnt->close();

   return GS_GOOD;
}
void gsc_getLayer(AcDbEntity *pEnt, C_STRING &Layer)
   { Layer.paste(pEnt->layer()); } // alloca memoria


/***************************************************************************/
/*.doc gsc_getDependeciesOnLayer                                           */
/*+                                                                       
  Funzione che legge i riferimenti al layer specificato.
  Parametri:
  const TCHAR *Layer;      Nome layer
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria.
-*/  
/***************************************************************************/
int gs_getDependeciesOnLayer(void)
{
   C_INT_STR_LIST ItemList;
   presbuf        arg = acedGetArgs();
   C_RB_LIST      ret;

   acedRetNil();

   if (!arg || arg->restype != RTSTR) // leggo il nome del layer
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_getDependeciesOnLayer(arg->resval.rstring, ItemList) == GS_BAD) return RTERROR;

   if ((ret << ItemList.to_rb()) == NULL) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}
int gsc_getDependeciesOnLayer(const TCHAR *Layer, C_INT_STR_LIST &ItemList)
{
   AcDbDatabase           *pCurrDatabase;
   AcDbBlockTable         *pBlockTable;
   AcDbBlockTableIterator *pBlockTableIt;
   AcDbObjectId           ObjId; 
   AcDbBlockTableRecord   *pBlockTableRec;
   AcDbBlockTableRecordIterator *pBlockTableRecIt;
   const TCHAR            *pName;
   C_INT_STR              *pItem;
   C_STRING               layerName;

   ItemList.remove_all();

   // database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

   // Inizio lettura dei blocchi
   if (pCurrDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pBlockTable->newIterator(pBlockTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pBlockTable->close(); return GS_BAD; }

   while (!pBlockTableIt->done())
   {
      if (pBlockTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
      if (acdbOpenAcDbObject((AcDbObject *&) pBlockTableRec, ObjId, AcDb::kForRead) != Acad::eOk)
         { delete pBlockTableIt; pBlockTable->close(); return GS_BAD; }
      pBlockTableRec->getName(pName);

      pBlockTableRec->newIterator(pBlockTableRecIt);
      while (!pBlockTableRecIt->done())
      {
         AcDbEntity *pEnt = NULL;
         pBlockTableRecIt->getEntity(pEnt, AcDb::kForRead);

         gsc_getLayer(pEnt, layerName);
         if (gsc_strcmp(layerName.get_name(), Layer, FALSE) == 0) // case insensitive
         {
            if ((pItem = new C_INT_STR(GSBlockNameSetting, pName)) == NULL)
            {
               pEnt->close();
               delete pBlockTableRecIt;
               delete pBlockTableIt; 
               pBlockTableRec->close();
               pBlockTable->close();
               GS_ERR_COD = eGSOutOfMem; 
               return GS_BAD; 
            }
            ItemList.add_tail(pItem);
         }

         pEnt->close();
         pBlockTableRecIt->step();
      }
      delete pBlockTableRecIt;
      pBlockTableRec->close();

      pBlockTableIt->step();
   }
   delete pBlockTableIt;
   pBlockTable->close();
   // Fine lettura dei blocchi

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_layer_model_from_rb              <internal> */
/*+
  Questa funzione restituisce l'oggetto AcDbLayerTableRecord con le 
  proprietà lette sottoforma di lista di coppie di resbuf ((<prop> <val>) ...).
  (("NAME" <name>) [("COLOR" <color>)] [("LINE_TYPE" <lineType>)] 
   [("LINE_WEIGHT" <lineWeight>)] [("PLOT_STYLE_NAME" <plotStyleName>)]
   [("DESCRIPTION" <description>)] [("TRANSPARENCY_PERCENT" <trasparency>)])

  Parametri:
  C_RB_LIST &List;                  Lista delle proprietà (input)
  AcDbLayerTableRecord &LayerRec;   Oggetto Layer (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_layer_model_from_rb(C_RB_LIST &List, AcDbLayerTableRecord &LayerRec)
{
   presbuf p;

   if ((p = List.CdrAssoc(_T("NAME"))) != NULL && p->restype == RTSTR) // nome Layer
      if (LayerRec.setName(p->resval.rstring) != Acad::eOk) return GS_BAD;

   if ((p = List.CdrAssoc(_T("COLOR"))) != NULL) // Colore Layer
   {
      C_COLOR color;

      if (color.setResbuf(p) == GS_GOOD)
      {
         AcCmColor dummy;
         
         if (gsc_C_COLOR_to_AcCmColor(color, dummy) == GS_GOOD)
            LayerRec.setColor(dummy);
      }
   }

   if ((p = List.CdrAssoc(_T("LINE_TYPE"))) != NULL && p->restype == RTSTR) // tipolinea Layer
   {
      AcDbObjectId LineTypeId;

      gsc_load_linetype(p->resval.rstring); // Carico il tipolinea se non ancora caricato
      if (gsc_getLineTypeId(p->resval.rstring, LineTypeId) == GS_GOOD)
         LayerRec.setLinetypeObjectId(LineTypeId);
   }

   if ((p = List.CdrAssoc(_T("LINE_WEIGHT"))) != NULL) // Spessore Layer
   {
      int LineWeight;

      if (gsc_rb2Int(p, &LineWeight) == GS_GOOD)
         LayerRec.setLineWeight((AcDb::LineWeight) LineWeight);
   }

   if ((p = List.CdrAssoc(_T("PLOT_STYLE_NAME"))) != NULL && p->restype == RTSTR) // Stile di stampa Layer
      LayerRec.setPlotStyleName(p->resval.rstring);

   if ((p = List.CdrAssoc(_T("TRANSPARENCY_PERCENT"))) != NULL) // Trasparenza Layer
   {
      double Transparency;

      if (gsc_rb2Dbl(p, &Transparency) == GS_GOOD)
      {
         AcCmTransparency _trasparency(Transparency);    
         LayerRec.setTransparency(_trasparency);
      }
   }

   if ((p = List.CdrAssoc(_T("DESCRIPTION"))) != NULL && p->restype == RTSTR) // Descrizione Layer
   {
      LayerRec.upgradeOpen();
      LayerRec.setDescription(p->resval.rstring);
      LayerRec.close();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_layer_model_to_rb                  <internal> */
/*+
  Questa funzione restituisce le proprietà lette da un oggetto AcDbLayerTableRecord
  sottoforma di lista di coppie di resbuf ((<proprietà> <valore>) ...)
  Parametri:
  AcDbLayerTableRecord &LayerRec;   Oggetto Layer (output)

  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf gsc_layer_model_to_rb(AcDbLayerTableRecord &LayerRec)
{
   C_RB_LIST        List;
   const TCHAR      *Layer = NULL;
   C_COLOR          Color;
   int              LineWeight;
   AcCmTransparency Transparency;
   C_STRING         LineType, PlotStyleName, Description;

   LayerRec.getName(Layer);

   gsc_AcCmColor_to_C_COLOR(LayerRec.color(), Color);

   LineType   = gsc_getSymbolName(LayerRec.linetypeObjectId());
   if (LineType.len() == 0) LineType = GS_EMPTYSTR;

   LineWeight = (int) LayerRec.lineWeight();

   PlotStyleName.paste(LayerRec.plotStyleName());
   if (PlotStyleName.len() == 0) PlotStyleName = GS_EMPTYSTR;

   Transparency = LayerRec.transparency();

   Description.paste(LayerRec.description());
   if (Description.len() == 0) Description = GS_EMPTYSTR;

   if ((List << acutBuildList(RTLB,
                                 RTLB,
                                    RTSTR, _T("NAME"),
                                    RTSTR, Layer,
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("COLOR"),
                              0)) == NULL) return NULL;
   if ((List += Color.getResbuf()) == NULL) return NULL;
   if ((List += acutBuildList(   RTLE,
                                 RTLB,
                                    RTSTR, _T("LINE_TYPE"),
                                    RTSTR, LineType.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("LINE_WEIGHT"),
                                    RTSHORT, LineWeight,
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("PLOT_STYLE_NAME"),
                                    RTSTR, PlotStyleName.get_name(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("TRANSPARENCY_PERCENT"),
                                    RTREAL, Transparency.alphaPercent(),
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("DESCRIPTION"),
                                    RTSTR, Description.get_name(),
                                 RTLE,
                              RTLE,
                              0)) == NULL) return NULL;

   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


/*********************************************************/
/*.doc gsc_getLayerModelList              <external> */
/*+
  Questa funzione restituisce un vettore di AcDbLayerTableRecord leggendolo
  dai prototipi di GEOsim.
  Parametri:
  int prj;                            Codice progetto (input)
  C_LAYER_MODEL_LIST &LayerModelList; Lista di modelli di layer (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getLayerModelList(int prj, C_LAYER_MODEL_LIST &LayerModelList)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef;
   _RecordsetPtr  pRs;
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;
   C_LAYER_MODEL  *pLayerModel;

   LayerModelList.remove_all();

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T("ORDER BY NAME");

   // leggo la riga della tabella
   if (pConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD; 

   // scorro l'elenco dei layer
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      if ((pLayerModel = new C_LAYER_MODEL()) == NULL)
         { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (pLayerModel->from_rb(ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); delete pLayerModel; return GS_BAD; }
      LayerModelList.add_tail(pLayerModel);

      gsc_Skip(pRs);
   }

   return gsc_DBCloseRs(pRs);
}


/*********************************************************/
/*.doc gs_getLayerModelList                   <external> */
/*+
  Questa funzione LISP restituisce la lista dei modelli di layer
  di GEOsim o nil in caso di errore.
  Parametri:
  (<prj>)                         Codice progetto (input)
-*/  
/*********************************************************/
int gs_getLayerModelList(void)
{
   int                prj;
   C_LAYER_MODEL_LIST LayerModelList;
   presbuf            arg = acedGetArgs();
   C_RB_LIST          Result;

   acedRetNil();

   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) // leggo il codice del progetto
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_getLayerModelList(prj, LayerModelList) != GS_GOOD) return RTERROR;
   if ((Result << LayerModelList.to_rb()) == NULL) return RTERROR;

   Result.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsddLayerModelList                     <external> */
/*+
  Questa comando gestisce tramite interfaccia grafica
  la lista dei modelli di piano di GEOsim.
-*/  
/*********************************************************/
void gsddLayerModelList(void)
{
   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_ddLayerModelList() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gsc_ddInitLayerModelListCtrl           <internal> */
/*+
  Questa funzione inizializza il controllo "list " della
  finestra dei modelli di piani di GEOsim.
  Parametri:
  C_LAYER_MODEL_LIST &LayerModelList; Lista dei modelli di layer
  ads_hdlg           dcl_id;          handle della finestra

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_ddInitLayerModelListCtrl(C_LAYER_MODEL_LIST &LayerModelList, 
                                 ads_hdlg dcl_id)
{
   C_LAYER_MODEL  *pLayerModel = (C_LAYER_MODEL *) LayerModelList.get_head();
   C_INT_STR_LIST LineWeightDescrList;
   C_INT_STR      *pLineWeightDescr;
   const TCHAR    *Layer = NULL;
   C_COLOR        Color;
   C_STRING       dummy, Description, strColor;
   

   // inizializzo la lista degli spessori di linea
   if (gsc_initLineWeightListForLayer(LineWeightDescrList) == GS_BAD) return GS_BAD;

   ads_start_list(dcl_id, _T("list"), LIST_NEW, 0);
   while (pLayerModel)
   {
      dummy = pLayerModel->get_name();
      dummy += _T("\t");

      pLayerModel->get_color(Color);
      if (Color.getColorMethod() == C_COLOR::ByRGBColor)
      {
         C_STRING strColor;

         Color.getRGBDecColor(strColor, _T(','));
         dummy += strColor;
      }
      else
      {
         int AutoCADColorIndex;

         Color.getAutoCADColorIndex(&AutoCADColorIndex);
         dummy += AutoCADColorIndex;
      }
      dummy += _T("\t");

      dummy += pLayerModel->get_linetype();
      dummy += _T("\t");
      if ((pLineWeightDescr = (C_INT_STR *) LineWeightDescrList.search_key((int) pLayerModel->get_lineweight())))
         dummy += pLineWeightDescr->get_name();
      dummy += _T("\t");
      dummy += pLayerModel->get_TransparencyPercent();
      dummy += _T("\t");
      dummy += pLayerModel->get_description();
      
      gsc_add_list(dummy);

      pLayerModel = (C_LAYER_MODEL *) LayerModelList.get_next();
   }
   ads_end_list();

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "prj_name"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_prj_name(ads_callback_packet *dcl)
{
   layer_models_Dcl_Struct *pDclStru = (layer_models_Dcl_Struct *) dcl->client_data;
   TCHAR                   buff[10];
   C_PROJECT               *pPrj;

   pDclStru->LayerModelList.remove_all();
   pDclStru->prj = 0;

   ads_get_tile(dcl->dialog, _T("prj_name"), buff, 10); 
   if ((pPrj = gsc_GetPopDwnPrjList(_wtoi(buff))) != NULL)
   {
      pDclStru->prj = pPrj->get_key();
      gsc_getLayerModelList(pDclStru->prj, pDclStru->LayerModelList);
   }

   // inizializzo il controllo "list"
   gsc_ddInitLayerModelListCtrl(pDclStru->LayerModelList, dcl->dialog);

   return;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "new"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_new(ads_callback_packet *dcl)
{
   layer_models_Dcl_Struct *pDclStru = (layer_models_Dcl_Struct *) dcl->client_data;
   int                     i = 0;
   C_LAYER_MODEL           LayerModel;
   C_STRING                NewName;
   C_LAYER_MODEL           *pLayerModel;

   if (pDclStru->prj == 0) return;

   // Trovo un nuovo nome
   do
   {
      i++;
      NewName = gsc_msg(26); // "Nuovo modello"
      NewName += _T(" ");
      NewName += i;

      pLayerModel = (C_LAYER_MODEL *) pDclStru->LayerModelList.get_head();
      while (pLayerModel)
      {
         if (NewName.comp(pLayerModel->get_name(), FALSE) == 0) break;
         pLayerModel = (C_LAYER_MODEL *) pDclStru->LayerModelList.get_next();
      }     
   }
   while (pLayerModel);
   
   LayerModel.set_name(NewName.get_name());
   if (gsc_ddLayerModel(LayerModel) == GS_GOOD)
      if (gsc_addLayerModel(pDclStru->prj, LayerModel) == GS_GOOD)
      {
         if (gsc_getLayerModelList(pDclStru->prj, pDclStru->LayerModelList) == GS_GOOD)
            // inizializzo il controllo "list"
            gsc_ddInitLayerModelListCtrl(pDclStru->LayerModelList, dcl->dialog);
      }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "edit"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_edit(ads_callback_packet *dcl)
{
   layer_models_Dcl_Struct *pDclStru = (layer_models_Dcl_Struct *) dcl->client_data;
   TCHAR                   val[TILE_STR_LIMIT];
   C_INT_LIST              SelectedList;
   C_INT                   *pSelected;
   C_RB_LIST               List, _List;
   C_STRING                _name;
   C_LAYER_MODEL           *pLayerModel;

   if (pDclStru->prj == 0) return;

   if (ads_get_tile(dcl->dialog, _T("list"), val, TILE_STR_LIMIT) != RTNORM) return;
   SelectedList.from_str(val, _T(' '));
   if (SelectedList.get_count() == 0) return;

   pSelected = (C_INT *) SelectedList.get_head();
   List << ((C_LAYER_MODEL *) pDclStru->LayerModelList.goto_pos(pSelected->get_key() + 1))->to_rb();
   while ((pSelected = (C_INT *) pSelected->get_next()))
   {
      _List << ((C_LAYER_MODEL *) pDclStru->LayerModelList.goto_pos(pSelected->get_key() + 1))->to_rb();
      // Confronto i campi annullando quelli diversi
      List.SubstRTNONEtoDifferent(_List); // no case sensitive
   }

   if (gsc_ddLayerModel(List, (SelectedList.get_count() > 1) ? true : false) != GS_GOOD) return;

   pSelected = (C_INT *) SelectedList.get_head();
   while (pSelected)
   {
      pLayerModel = (C_LAYER_MODEL *) pDclStru->LayerModelList.goto_pos(pSelected->get_key() + 1);
      // lo copio in _name perchè la funzione from_rb modifica il nome del layer
      _name = pLayerModel->get_name();

      if (pLayerModel->from_rb(List) != GS_GOOD) return;
      if (gsc_setLayerModel(pDclStru->prj, _name.get_name(), *pLayerModel) != GS_GOOD)
          return;

      pSelected = (C_INT *) pSelected->get_next();
   }

   if (gsc_getLayerModelList(pDclStru->prj, pDclStru->LayerModelList) == GS_GOOD)
   {
      // inizializzo il controllo "list"
      gsc_ddInitLayerModelListCtrl(pDclStru->LayerModelList, dcl->dialog);
      // mi riposiziono sulla prima riga precedentemente selezionata
      if ((pSelected = (C_INT *) SelectedList.get_head()) && 
          pSelected->get_key() <= pDclStru->LayerModelList.get_count())
      {
         gsc_itoa(pSelected->get_key(), val);
         ads_set_tile(dcl->dialog, _T("list"), val);
      }
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "list"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_list(ads_callback_packet *dcl)
{
   if (dcl->reason != CBR_DOUBLE_CLICK) return;
   dcl_layer_models_edit(dcl);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "erase"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_erase(ads_callback_packet *dcl)
{
   layer_models_Dcl_Struct *pDclStru = (layer_models_Dcl_Struct *) dcl->client_data;
   int                     conf;
   TCHAR                   val[TILE_STR_LIMIT];
   const TCHAR             *Layer = NULL;
   C_INT_LIST              SelectedList;
   C_INT                   *pSelected;

   if (pDclStru->prj == 0) return;

   if (ads_get_tile(dcl->dialog, _T("list"), val, TILE_STR_LIMIT) != RTNORM) return;
   SelectedList.from_str(val, _T(' '));
   if (SelectedList.get_count() == 0) return;

   // "Cancellare il/i modello/i selezionati ?"
   if (gsc_ddgetconfirm(gsc_msg(27), &conf) != GS_GOOD || conf != GS_GOOD) return;

   pSelected = (C_INT *) SelectedList.get_head();
   while (pSelected)
   {
      gsc_delLayerModel(pDclStru->prj,
                        ((C_LAYER_MODEL *) pDclStru->LayerModelList.goto_pos(pSelected->get_key() + 1))->get_name());
      pSelected = (C_INT *) SelectedList.get_next();
   }

   if (gsc_getLayerModelList(pDclStru->prj, pDclStru->LayerModelList) == GS_GOOD)
   {  // inizializzo il controllo "list"
      gsc_ddInitLayerModelListCtrl(pDclStru->LayerModelList, dcl->dialog);
      // mi riposiziono sulla prima riga precedentemente selezionata
      if ((pSelected = (C_INT *) SelectedList.get_head()) &&
          pSelected->get_key() <= pDclStru->LayerModelList.get_count())
      {
         gsc_itoa(pSelected->get_key(), val);
         ads_set_tile(dcl->dialog, _T("list"), val);
      }
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "fromACAD"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_from_ACAD(ads_callback_packet *dcl)
{
   layer_models_Dcl_Struct *pDclStru = (layer_models_Dcl_Struct *) dcl->client_data;
   C_LAYER_MODEL           LayerModel;
   C_STR_LIST              LayerNameList, LayerNameListToCreate;
   C_STR                   *pLayerName;

   if (pDclStru->prj == 0) return;

   // ricavo la lista dei layer di acad che non sono definiti dai modelli di layer di geosim

   // leggo la lista dei layer attuale
   if (gsc_getActualLayerNameList(LayerNameList) == GS_BAD) return;
   pLayerName = (C_STR *) pDclStru->LayerModelList.get_head();
   while (pLayerName)
   {
      LayerNameList.remove_name(pLayerName->get_name(), FALSE);
      pLayerName = (C_STR *) pDclStru->LayerModelList.get_next();
   }

   if (gsc_ddMultiSelectLayers(&LayerNameList, LayerNameListToCreate) != GS_GOOD) return;

   pLayerName = (C_STR *) LayerNameListToCreate.get_head();
   while (pLayerName)
   {
      // provo a caricare le caratteristiche dal layer autocad (se esiste)
      LayerModel.from_LayerTableRecord(pLayerName->get_name());

      if (gsc_addLayerModel(pDclStru->prj, LayerModel) == GS_GOOD)
      {
         if (gsc_getLayerModelList(pDclStru->prj, pDclStru->LayerModelList) == GS_GOOD)
            // inizializzo il controllo "list"
            gsc_ddInitLayerModelListCtrl(pDclStru->LayerModelList, dcl->dialog);
      }

      pLayerName = (C_STR *) LayerNameListToCreate.get_next();
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Gestionemodellideipiani); } 
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_models" su controllo "Close"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_models_Close(ads_callback_packet *dcl)
   { ads_done_dialog(dcl->dialog, DLGOK); }


/*********************************************************/
/*.doc gsc_ddLayerModelList                   <external> */
/*+
  Questa funzione edita tramite interfaccia grafica
  la lista dei modelli di piano di GEOsim.
  La funzione è riservata a superutenti.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_ddLayerModelList(void)
{
   layer_models_Dcl_Struct DclStru;
   int                     status = DLGOK, dcl_file;
   C_STRING                path;
   ads_hdlg                dcl_id;

   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   DclStru.prj = 0;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("layer_models"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   gsc_SetPopDwnPrjList(dcl_id, _T("prj_name"));
   if (GEOsimAppl::PROJECTS.get_count() == 1)
   {
      ads_callback_packet dcl;

      dcl.client_data = &DclStru;
      dcl.dialog      = dcl_id;
      ads_set_tile(dcl_id, _T("prj_name"), _T("1")); 
      dcl_layer_models_prj_name(&dcl);
   }

   ads_action_tile(dcl_id, _T("prj_name"), (CLIENTFUNC) dcl_layer_models_prj_name); // scelta progetto
   ads_client_data_tile(dcl_id, _T("prj_name"), &DclStru);
   ads_action_tile(dcl_id, _T("new"), (CLIENTFUNC) dcl_layer_models_new); // nuovo modello
   ads_client_data_tile(dcl_id, _T("new"), &DclStru);
   ads_action_tile(dcl_id, _T("edit"), (CLIENTFUNC) dcl_layer_models_edit); // modifica modello corrente
   ads_client_data_tile(dcl_id, _T("edit"), &DclStru);
   ads_action_tile(dcl_id, _T("list"), (CLIENTFUNC) dcl_layer_models_list); // modifica modello corrente
   ads_client_data_tile(dcl_id, _T("list"), &DclStru);
   ads_action_tile(dcl_id, _T("erase"), (CLIENTFUNC) dcl_layer_models_erase); // Cancella modello corrente
   ads_client_data_tile(dcl_id, _T("erase"), &DclStru);
   ads_action_tile(dcl_id, _T("fromACAD"), (CLIENTFUNC) dcl_layer_models_from_ACAD); // aggiungi modelli da layer ACAD
   ads_client_data_tile(dcl_id, _T("fromACAD"), &DclStru);
   ads_action_tile(dcl_id, _T("help"),  (CLIENTFUNC) dcl_layer_models_help);
   ads_action_tile(dcl_id, _T("Close"), (CLIENTFUNC) dcl_layer_models_Close);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getLayerModel                  <external> */
/*+
  Questa funzione restituisce un oggetto AcDbLayerTableRecord leggendolo
  dai prototipi di GEOsim.
  Parametri:
  int prj;                   Codice progetto (input)
  C_LAYER_MODEL &LayerModel; Oggetto modello di layer (input/output) con almeno 
                             la proprietà name già impostata

  Restituisce GS_GOOD in caso di successo, GS_CAN se non esiste altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getLayerModel(int prj, C_LAYER_MODEL &LayerModel)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef, dummy(LayerModel.get_name());
   _RecordsetPtr  pRs;
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;
  
   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE NAME=");
   if (pConn->Str2SqlSyntax(dummy) == GS_BAD) return GS_BAD; // Correggo la stringa secondo la sintassi SQL roby 2015
   statement += dummy;

   // leggo la riga della tabella
   if (pConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;
          
   if (gsc_isEOF(pRs) == GS_GOOD) // Se non c'è
      { gsc_DBCloseRs(pRs); return GS_CAN; }

   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) 
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   return LayerModel.from_rb(ColValues);
}


/*********************************************************/
/*.doc gsc_addLayerModel                  <external> */
/*+
  Questa funzione aggiunge un modello di layer di GEOsim.
  Parametri:
  int prj;                   Codice progetto (input)
  C_LAYER_MODEL &LayerModel; Oggetto modello di layer (input)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_addLayerModel(int prj, C_LAYER_MODEL &LayerModel)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef;
   _RecordsetPtr  pRs;
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   if ((ColValues << LayerModel.to_rb()) == NULL) return GS_BAD;

   // inserisco nuova riga
   return pConn->InsRow(TableRef.get_name(), ColValues);
      
   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_addLayerModel                       <external> */
/*+
  Questa funzione LISP aggiunge un modello di layer di GEOsim.
  Parametri:
  <prj>(("NAME" <name>) [("COLOR" <color>)] [("LINE_TYPE" <lineType>)] 
        [("LINE_WEIGHT" <lineWeight>)] [("PLOT_STYLE_NAME" <plotStyleName>)]
        [("DESCRIPTION" <description>)] [("TRANSPARENCY_PERCENT" <trasparency>)])
-*/  
/*********************************************************/
int gs_addLayerModel(void)
{
   int           prj;
   presbuf       arg = acedGetArgs();
   C_RB_LIST     List;
   C_LAYER_MODEL LayerModel;

   acedRetNil();

   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) // leggo il codice del progetto
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   List = arg->rbnext;

   if (LayerModel.from_rb(List) == GS_BAD) return RTERROR;
   if (gsc_addLayerModel(prj, LayerModel) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_setLayerModel                  <external> */
/*+
  Questa funzione modifica un layer prototipo di GEOsim esistente.
  Parametri:
  int prj;                    Codice progetto (input)
  const TCHAR *ModelName;     Nome del modello da modificare
  C_LAYER_MODEL &LayerModel;  Oggetto modello di layer (input)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_setLayerModel(int prj, const TCHAR *ModelName, C_LAYER_MODEL &LayerModel)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef, dummy(ModelName);
   _RecordsetPtr  pRs;
   C_DBCONNECTION *pConn;
   C_RB_LIST      ColValues;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE NAME=");
   if (pConn->Str2SqlSyntax(dummy) == GS_BAD) return GS_BAD; // Correggo la stringa secondo la sintassi SQL roby 2015
   statement += dummy;

   // leggo la riga della tabella
   if (pConn->ExeCmd(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD) return GS_BAD;

   if ((ColValues << LayerModel.to_rb()) == NULL) return GS_BAD;

   if (gsc_isEOF(pRs) == GS_GOOD) // Se non c'è
   {
      gsc_DBCloseRs(pRs); 
      return GS_BAD;
   }
   else
   {
      // modifico la riga del cursore
      if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;
      if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   }
      
   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_setLayerModel                       <external> */
/*+
  Questa funzione LISP modifica un layer prototipo di GEOsim esistente.
  Parametri:
  <prj><OldName>(("NAME" <newName>) [("COLOR" <newColor>)] [("LINE_TYPE" <newLineType>)] 
        [("LINE_WEIGHT" <newLineWeight>)] [("PLOT_STYLE_NAME" <newPlotStyleName>)]
        [("DESCRIPTION" <newDescription>)] [("TRANSPARENCY_PERCENT" <trasparency>)])
-*/  
/*********************************************************/
int gs_setLayerModel(void)
{
   int           prj;
   presbuf       arg = acedGetArgs();
   TCHAR         *ModelName;
   C_RB_LIST     List;
   C_LAYER_MODEL LayerModel;

   acedRetNil();

   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) // leggo il codice del progetto
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(arg = arg->rbnext) || arg->restype != RTSTR) // leggo il nome del modello
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ModelName = arg->resval.rstring;

   List = arg->rbnext;

   if (LayerModel.from_rb(List) == GS_BAD) return RTERROR;
   if (gsc_setLayerModel(prj, ModelName, LayerModel) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_delLayerModel                  <external> */
/*+
  Questa funzione cancella un layer prototipo di GEOsim.
  Parametri:
  int prj;                Codice progetto (input)
  const TCHAR *ModelName; Nome del modello (input)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_delLayerModel(int prj, const TCHAR *ModelName)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef, dummy(ModelName);
   C_DBCONNECTION *pConn;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) != GS_GOOD) return GS_BAD;

   statement = _T("DELETE FROM ");
   statement += TableRef;
   statement += _T(" WHERE NAME=");
   if (pConn->Str2SqlSyntax(dummy) == GS_BAD) return GS_BAD; // Correggo la stringa secondo la sintassi SQL roby 2015
   statement += dummy;

   // ccancella la riga della tabella
   return pConn->ExeCmd(statement);
}


/*********************************************************/
/*.doc gs_ddLayerModel                        <external> */
/*+
  Questa funzione LISP edita tramite interfaccia grafica
  le proprietà di un modello di piano di GEOsim.
  Parametri:
  (("NAME" <name>) [("COLOR" <color>)] [("LINE_TYPE" <lineType>)] 
   [("LINE_WEIGHT" <lineWeight>)] [("PLOT_STYLE_NAME" <plotStyleName>)]
   [("DESCRIPTION" <description>)] [("TRANSPARENCY_PERCENT" <trasparency>)])

  Restituisce una lista descrittiva del modello di piano in caso di uscita
  con il tasto OK altrimenti nil.
-*/  
/*********************************************************/
int gs_ddLayerModel(void)
{
   C_RB_LIST     ret;
   C_LAYER_MODEL LayerModel;

   acedRetNil();
   
   ret << gsc_rblistcopy(acedGetArgs());
   if (LayerModel.from_rb(ret) == GS_BAD) return RTERROR;
   if (gsc_ddLayerModel(LayerModel) == GS_GOOD)
      if (ret << LayerModel.to_rb()) ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_initLineWeightListForLayer          <internal> */
/*+
  Questa funzione inizializza la lista degli spessori delle linee 
  per i piani con relative descrizioni.
  Parametri:
  C_INT_STR_LIST &LineWeightDescrList;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD;
-*/  
/*********************************************************/
int gsc_initLineWeightListForLayer(C_INT_STR_LIST &LineWeightDescrList)
{
   C_INT_STR *pLineWeightDescr;

   LineWeightDescrList.remove_all();

   // lineweights are in 100ths of a millimeter
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWtByLwDefault, gsc_msg(25)); // "Default"
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt000, _T("0.00 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt005, _T("0.05 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt009, _T("0.09 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt013, _T("0.13 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt015, _T("0.15 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt018, _T("0.18 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt020, _T("0.20 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt025, _T("0.25 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt030, _T("0.30 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt040, _T("0.40 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt050, _T("0.50 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt053, _T("0.53 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt060, _T("0.60 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt070, _T("0.70 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt080, _T("0.80 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt090, _T("0.90 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt100, _T("1.00 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt106, _T("1.06 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt120, _T("1.20 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt140, _T("1.40 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt158, _T("1.58 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt200, _T("2.00 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);
   pLineWeightDescr = new C_INT_STR(AcDb::kLnWt211, _T("2.11 mm"));
   LineWeightDescrList.add_tail(pLineWeightDescr);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_initTrasparencyListForLayer        <internal> */
/*+
  Questa funzione inizializza la lista delle percentuali di trasparenza
  per i piani [0-90].
  Parametri:
  C_REAL_LIST &TrasparencyList;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD;
-*/  
/*********************************************************/
int gsc_initTrasparencyListForLayer(C_REAL_LIST &TrasparencyList)
{
   C_REAL *pTrasparency;

   TrasparencyList.remove_all();

   pTrasparency = new C_REAL(0);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(10);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(20);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(30);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(40);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(50);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(60);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(70);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(80);
   TrasparencyList.add_tail(pTrasparency);
   pTrasparency = new C_REAL(90);
   TrasparencyList.add_tail(pTrasparency);

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_model" su controllo "le_layer"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_model_le_layer(ads_callback_packet *dcl)
{
   TCHAR     StrValue[MAX_LEN_LAYERNAME];
   C_RB_LIST DescrLayer;

   ads_get_tile(dcl->dialog, _T("e_layer"), StrValue, MAX_LEN_LAYERNAME); // edit box layer
   if ((DescrLayer << gsc_ddsellayers(StrValue)) == NULL) return;
   if (DescrLayer.get_head() != NULL)
      ads_set_tile(dcl->dialog, _T("e_layer"), DescrLayer.get_head()->resval.rstring);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_model" su controllo "le_color"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_model_le_color(ads_callback_packet *dcl)
{ 
   C_COLOR Value, CurLayerColor;
   TCHAR   StrValue[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("e_color"), StrValue, TILE_STR_LIMIT); // edit box colore
   Value.setString(StrValue);

   // scelta del colore
   CurLayerColor.setForeground();
   if (gsc_SetColorDialog(Value, false, CurLayerColor) == GS_GOOD)
   {
      C_STRING StrColor;

      Value.getString(StrColor);
      ads_set_tile(dcl->dialog, _T("e_color"), StrColor.get_name());
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "layer_model" su controllo "le_linet"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_layer_model_le_linet(ads_callback_packet *dcl)
{
   TCHAR     StrValue[MAX_LEN_LAYERNAME];
   C_RB_LIST DescrLayer;

   ads_get_tile(dcl->dialog, _T("e_linet"), StrValue, MAX_LEN_LAYERNAME); // edit box tipolinea
   if ((DescrLayer << gsc_ddsellinetype(StrValue)) == NULL) return;
   if (DescrLayer.get_head() != NULL)
      ads_set_tile(dcl->dialog, _T("e_linet"), DescrLayer.get_head()->resval.rstring);
}
// click su tasto HELP
static void CALLB dcl_layer_model_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Gestionemodellideipiani); } 
static void CALLB dcl_layer_model_OK(ads_callback_packet *dcl)
{ // ACCEPT
   ads_hdlg               hdlg = dcl->dialog;
   TCHAR                  value[TILE_STR_LIMIT];
   int                    intValue;
   layer_model_Dcl_Struct *pDclStru = (layer_model_Dcl_Struct *) dcl->client_data;

   ads_get_tile(hdlg, _T("e_layer"), value, TILE_STR_LIMIT - 1);
   gsc_alltrim(value);
   if (pDclStru->MultiLayer == false) // Verifico correttezza del nome
      if (gsc_strlen(value) == 0)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(800)); // "*Errore* Piano non valido."
         return;
      }
   pDclStru->Name = value;

   ads_get_tile(hdlg, _T("e_color"), value, TILE_STR_LIMIT - 1);
   gsc_alltrim(value);
   if (gsc_strlen(value) > 0)
   {
      C_STRING strColor(value);

      if (strColor.isDigit() == GS_GOOD) pDclStru->Color.setAutoCADColorIndex(strColor.toi());
      else pDclStru->Color.setRGBDecColor(strColor, _T(','));
   }
   else
      if (pDclStru->MultiLayer == false) // Correggo il colore
         pDclStru->Color.setForeground();
      else 
         pDclStru->Color.setNone(); // valore non valido che significa non settato

   ads_get_tile(hdlg, _T("e_linet"), value, TILE_STR_LIMIT - 1);
   gsc_alltrim(value);
   pDclStru->LineType = value;
   if (pDclStru->MultiLayer == false)
      if (gsc_strlen(value) == 0) // Correggo il tipo linea
         pDclStru->LineType = _T("CONTINUOUS");

   // Spessori di linea
   C_INT_STR_LIST LineWeightDescrList;
   C_INT_STR      *pLineWeightDescr;

   ads_get_tile(hdlg, _T("e_lineweight"), value, TILE_STR_LIMIT - 1);
   intValue = _wtoi(value);

   // inizializzo la lista degli spessori di linea
   gsc_initLineWeightListForLayer(LineWeightDescrList);

   pDclStru->Lineweight = -999; // valore non valido che significa non settato
   if ((pLineWeightDescr = (C_INT_STR *) LineWeightDescrList.goto_pos(intValue)) == NULL) // 1 per il primo elemento
   {
      if (pDclStru->MultiLayer == false)
         pDclStru->Lineweight = (int) AcDb::kLnWtByLwDefault;
   }
   else
      pDclStru->Lineweight = pLineWeightDescr->get_key();

   // Trasparenza
   C_REAL_LIST TrasparencyList;
   C_REAL      *pTrasparency;

   ads_get_tile(hdlg, _T("e_trasparency"), value, TILE_STR_LIMIT - 1);
   intValue = _wtoi(value);

   // inizializzo la lista delle percentuali di trasparenza
   gsc_initTrasparencyListForLayer(TrasparencyList);

   pDclStru->Transparency = -999; // valore non valido che significa non settato
   if ((pTrasparency = (C_REAL *) TrasparencyList.goto_pos(intValue)) == NULL) // 1 per il primo elemento
   {
      if (pDclStru->MultiLayer == false)
         pDclStru->Transparency = 0; // Opaco
   }
   else
      pDclStru->Transparency = pTrasparency->get_key_double();

   ads_get_tile(hdlg, _T("e_description"), value, TILE_STR_LIMIT - 1);
   pDclStru->Description = value;

   ads_done_dialog(hdlg, DLGOK);
}


/*********************************************************/
/*.doc gsc_ddLayerModel                        <external> */
/*+
  Questa funzione edita tramite interfaccia grafica
  le proprietà di un modello di piano di GEOsim.
  Parametri:
  C_LAYER_MODEL &LayerModel; Oggetto modello di layer
  bool MultiLayer;           Se true significa che si stanno settando
                             le caratteristiche di più layer

  Restituisce GS_GOOD in caso di uscita con il tasto OK,
  GS_CAN in caso di uscita con il tasto Cancel e GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_ddLayerModel(C_LAYER_MODEL &LayerModel)
{
   int       res;
   C_RB_LIST ModelProperties;
   
   if ((ModelProperties << LayerModel.to_rb()) == NULL)
      return GS_BAD;

   if ((res = gsc_ddLayerModel(ModelProperties, false)) == GS_GOOD)
      LayerModel.from_rb(ModelProperties);

   return res;
}
int gsc_ddLayerModel(C_RB_LIST &ModelProperties, bool MultiLayer)
{
   presbuf                p;
   C_STRING               path, dummyStr;
   ads_hdlg               dcl_id;
   int                    dummyInt, status = DLGOK, dcl_file, pos;
   double                 dummyDbl;
   C_INT_STR_LIST         LineWeightDescrList;
   C_INT_STR              *pLineWeightDescr;
   C_REAL_LIST            TrasparencyList;
   C_REAL                 *pTrasparency;
   layer_model_Dcl_Struct DclStru;

   DclStru.MultiLayer = true;
   // inizializzo la lista degli spessori di linea
   if (gsc_initLineWeightListForLayer(LineWeightDescrList) == GS_BAD) return GS_BAD;
   // inizializzo la lista delle percentuali di trasparenza
   if (gsc_initTrasparencyListForLayer(TrasparencyList) == GS_BAD) return GS_BAD;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;

   ads_new_dialog(_T("layer_model"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   if (MultiLayer)
   {
      ads_mode_tile(dcl_id, _T("e_layer"), MODE_DISABLE);
      ads_mode_tile(dcl_id, _T("le_layer"), MODE_DISABLE);
   }
   else
   {
      if ((p = ModelProperties.CdrAssoc(_T("NAME"))) && p->restype == RTSTR)
         ads_set_tile(dcl_id, _T("e_layer"), p->resval.rstring);
   }

   if ((p = ModelProperties.CdrAssoc(_T("COLOR"))))
   {
      C_COLOR Color;

      if (Color.setResbuf(p) == GS_GOOD)
      {
         Color.getString(dummyStr);
         ads_set_tile(dcl_id, _T("e_color"), dummyStr.get_name());
      }
   }

   if ((p = ModelProperties.CdrAssoc(_T("LINE_TYPE"))) && p->restype == RTSTR)
      ads_set_tile(dcl_id, _T("e_linet"), p->resval.rstring);

   // Spessori di linea
   ads_start_list(dcl_id, _T("e_lineweight"), LIST_NEW, 0);
   pLineWeightDescr = (C_INT_STR *) LineWeightDescrList.get_head();
   gsc_add_list(_T("")); // una riga vuota
   while (pLineWeightDescr)
   {
      gsc_add_list(pLineWeightDescr->get_name());
      pLineWeightDescr = (C_INT_STR *) LineWeightDescrList.get_next();
   }
   ads_end_list();
   dummyStr = _T("0");
   if ((p = ModelProperties.CdrAssoc(_T("LINE_WEIGHT"))) && gsc_rb2Int(p, &dummyInt) == GS_GOOD)
      if ((pos = LineWeightDescrList.getpos_key(dummyInt)) > 0)
         dummyStr = pos; // getpos_key restituisce 1 per il primo elemento
   ads_set_tile(dcl_id, _T("e_lineweight"), dummyStr.get_name());

   // Trasparenza
   ads_start_list(dcl_id, _T("e_trasparency"), LIST_NEW, 0);
   pTrasparency = (C_REAL *) TrasparencyList.get_head();
   gsc_add_list(_T("")); // una riga vuota
   while (pTrasparency)
   {
      dummyStr = pTrasparency->get_key_double();
      gsc_add_list(dummyStr);
      pTrasparency = (C_REAL *) TrasparencyList.get_next();
   }
   ads_end_list();
   dummyStr = _T("0");
   if ((p = ModelProperties.CdrAssoc(_T("TRANSPARENCY_PERCENT"))) && gsc_rb2Dbl(p, &dummyDbl) == GS_GOOD)
      if ((pos = TrasparencyList.getpos_key_double(dummyDbl)) > 0)
         dummyStr = pos; // getpos_key restituisce 1 per il primo elemento
   ads_set_tile(dcl_id, _T("e_trasparency"), dummyStr.get_name());

   if ((p = ModelProperties.CdrAssoc(_T("DESCRIPTION"))) && p->restype == RTSTR)
      ads_set_tile(dcl_id, _T("e_description"), p->resval.rstring);
  
   ads_action_tile(dcl_id, _T("le_layer"), (CLIENTFUNC) dcl_layer_model_le_layer); // scelta layer
   ads_action_tile(dcl_id, _T("le_color"), (CLIENTFUNC) dcl_layer_model_le_color); // scelta colore
   ads_action_tile(dcl_id, _T("le_linet"), (CLIENTFUNC) dcl_layer_model_le_linet); // scelta tipolinea
   ads_action_tile(dcl_id, _T("help"),  (CLIENTFUNC) dcl_layer_model_help);
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_layer_model_OK);
   ads_client_data_tile(dcl_id, _T("accept"), &DclStru);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      C_STRING dummy;

      if (DclStru.Name.len() > 0)
         ModelProperties.CdrAssocSubst(_T("NAME"), DclStru.Name.get_name());

      if (DclStru.Color.getColorMethod() == C_COLOR::ByAutoCADColorIndex)
      {
         DclStru.Color.getAutoCADColorIndex(&dummyInt);
         ModelProperties.CdrAssocSubst(_T("COLOR"), dummyInt);
      }
      else if (DclStru.Color.getColorMethod() == C_COLOR::ByRGBColor)
      {
         DclStru.Color.getRGBDecColor(dummyStr, _T(','));
         ModelProperties.CdrAssocSubst(_T("COLOR"), dummyStr.get_name());
      }

      if (!MultiLayer || (MultiLayer && DclStru.LineType.len() > 0))
         ModelProperties.CdrAssocSubst(_T("LINE_TYPE"), (DclStru.LineType.len() == 0) ? GS_EMPTYSTR : DclStru.LineType.get_name());
      if (DclStru.Lineweight != -999) // valore non valido che significa non settato
         ModelProperties.CdrAssocSubst(_T("LINE_WEIGHT"), DclStru.Lineweight);
      if (!MultiLayer || (MultiLayer && DclStru.Description.len() > 0))
         ModelProperties.CdrAssocSubst(_T("DESCRIPTION"), (DclStru.Description.len() == 0) ? GS_EMPTYSTR : DclStru.Description.get_name());
      if (DclStru.Transparency != -999) // valore non valido che significa non settato
         ModelProperties.CdrAssocSubst(_T("TRANSPARENCY_PERCENT"), DclStru.Transparency);
   }

   return status;
}


//-----------------------------------------------------------------------//
//////////////////    GESTIONE    LAYER    FINE         ///////////////////
//////////////////  GESTIONE  STILI QUOTATURE  INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gs_getgeosimDimStyle <external> */
/*+
  Questa funzione restituisce una lista degli stili di quotatura di GEOsim.
  (<nome1><nome2> ...)
-*/  
/*********************************************************/
int gs_getgeosimDimStyle(void)
{
   C_STR_LIST StyleNameList;

   if (gsc_getgeosimDimStyles(StyleNameList) == GS_BAD) acedRetNil();
   else
   {
      C_RB_LIST ret(StyleNameList.to_rb());
      ret.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getgeosimDimStyles                 <external> */
/*+
  Questa funzione restituisce una lista degli stili di quotatura
  ordinata cercandoli nella tabella di AutoCAD.
  Parametri:
  C_STR_LIST &StyleNameList; Lista degli stili di quotatura (out)

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_getgeosimDimStyles(C_STR_LIST &StyleNameList)
{
   // Get a list of all of the Dim Styles
	AcDbDimStyleTable         *pTable;
   AcDbDimStyleTableIterator *pDimStyleIterator;
   AcDbObjectId              id;
   AcDbDimStyleTableRecord   *pDimStyleRec;
   const ACHAR               *Name;
   C_STR                     *pStyleName;

   StyleNameList.remove_all();

	if (acdbHostApplicationServices()->workingDatabase()->getDimStyleTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pTable->newIterator(pDimStyleIterator, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pTable->close(); return GS_BAD; }

   while (!pDimStyleIterator->done())
   {
      if (pDimStyleIterator->getRecordId(id) != Acad::eOk)
         { delete pDimStyleIterator; pTable->close(); return GS_BAD; }

      if (acdbOpenAcDbObject((AcDbObject *&) pDimStyleRec, id, AcDb::kForRead) != Acad::eOk)
         { delete pDimStyleIterator; pTable->close(); return GS_BAD; }
      if (pDimStyleRec->getName(Name) != Acad::eOk)
         { pDimStyleRec->close(); delete pDimStyleIterator; pTable->close(); return GS_BAD; }
      pDimStyleRec->close();

      if ((pStyleName = new C_STR(Name)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      StyleNameList.add_tail(pStyleName);

      pDimStyleIterator->step();
   }
   delete pDimStyleIterator;

   pTable->close();
   
   // ordino la lista degli elementi per nome
   StyleNameList.sort_name(TRUE, TRUE); // sensitive e ascending

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_validDimStyle                      <external> */
/*+
  Questa funzione verifica la validità del nome di uno stile
  di quotatura.
  Parametri:
  const TCHAR *DimStyle;    Nome dello stile di quotatura

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_validDimStyle(const TCHAR *DimStyle)
{
   size_t            len;
	AcDbDimStyleTable *pTable;
   bool              Exist;

   if (DimStyle == NULL) { GS_ERR_COD = eGSInvalidDimStyle; return GS_BAD; }   
   len = wcslen(DimStyle);   
   if (len == 0 || len >= MAX_LEN_DIMSTYLENAME)
      { GS_ERR_COD = eGSInvalidDimStyle; return GS_BAD; }   
  
	if (acdbHostApplicationServices()->workingDatabase()->getDimStyleTable(pTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   Exist = pTable->has(DimStyle);
   pTable->close();

   if (!Exist) { GS_ERR_COD = eGSInvalidDimStyle; return GS_BAD; }   

   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_getDimStyleId                     <external> */
/*+
  Questa funzione restituisce l'identificativo di uno stile di quotatura.
  Parametri:
  const TCHAR *DimStyleName;  Nome dello stile di quotatura
  AcDbObjectId &DimStyleId;   Identificativo dello stile di quotatura (out)
  AcDbDimStyleTable *pStyleTable; Opzionale, Tabella degli stili
                                  se = null usa la tabella del db corrente

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_getDimStyleId(const TCHAR *DimStyleName, AcDbObjectId &DimStyleId,
                      AcDbDimStyleTable *pDimStyleTable)
{
    AcDbDimStyleTable *pMyDimStyleTable;
    Acad::ErrorStatus es;
    
   if (pDimStyleTable) pMyDimStyleTable = pDimStyleTable;
   else
      if (acdbHostApplicationServices()->workingDatabase()->getDimStyleTable(pMyDimStyleTable, AcDb::kForRead) != Acad::eOk)
         return GS_BAD;

   if ((es = pMyDimStyleTable->getAt(DimStyleName, DimStyleId, Adesk::kFalse)) != Acad::eOk)
      GS_ERR_COD = eGSInvalidDimStyle;
   if (!pDimStyleTable) pMyDimStyleTable->close();

    return (es == Acad::eOk) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_set_CurrentDimStyle                <external> */
/*+
  Questa funzione setta lo stile di quotatura corrente.
  Parametri:
  C_STRING &DimStyle;   Nome dello stile di quotatura
  C_STRING *old;  Nome del tipo linea corrente
                    

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/*********************************************************/
int gsc_set_CurrentDimStyle(C_STRING &DimStyle, C_STRING *old)
{
   AcDbDatabase *pDB = acdbHostApplicationServices()->workingDatabase();
   AcDbObjectId DimStyleId;

   // Memorizza vecchio valore in old
   if (old)
   {
      resbuf *rb;

      if ((rb = acutBuildList(RTNIL, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (acedGetVar(_T("DIMSTYLE"), rb) != RTNORM)
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }

      if (rb->restype != RTSTR || rb->resval.rstring == NULL)   
         { GS_ERR_COD = eGSVarNotDef; acutRelRb(rb); return GS_BAD; }
      old->set_name(rb->resval.rstring); 
      acutRelRb(rb); 
   }

   DimStyle.alltrim();
   if (DimStyle.len() == 0) return GS_GOOD;

   if (gsc_getDimStyleId(DimStyle.get_name(), DimStyleId) == GS_BAD) return GS_BAD;
   if (pDB->setDimstyle(DimStyleId) != Acad::eOk) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_ddSelDimStyles                      <external> */
/*+
  Questa funzione permette la scelta di uno stile di quotatura
  tramite interfaccia a finestra DCL
  Parametro:
  lo stile di quotatura di default (opzionale) 
  oppure
  const TCHAR *current[MAX_LEN_TEXTSTYLENAME]; stile di quotatura corrente

  Restituisce il nome dello stile di quotatura scelto.
-*/  
/*********************************************************/
int gs_ddSelDimStyles(void)
{
   TCHAR   current[MAX_LEN_TEXTSTYLENAME] = GS_EMPTYSTR, *pStr;
   presbuf arg = acedGetArgs();

   acedRetNil();

   if (arg && arg->restype == RTSTR) // leggo lo stile di testo corrente (opzionale)
      gsc_strcpy(current, arg->resval.rstring, MAX_LEN_TEXTSTYLENAME);
   if ((pStr = gsc_ddSelDimStyles(current)) != NULL)
   {
      acedRetStr(pStr);
      free(pStr);
   }
   return RTNORM;
}
// N.B. Alloca memoria
TCHAR *gsc_ddSelDimStyles(const TCHAR *current)
{
   TCHAR      strPos[10] = _T("0");
   int        status = DLGOK, dcl_file, pos = 0;
   ads_hdlg   dcl_id;
   C_STR_LIST DimStyles;
   C_STR      *pDimStyle;
   C_STRING   path;

   if (gsc_getgeosimDimStyles(DimStyles) == GS_BAD) return NULL;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_THM.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return NULL;

   ads_new_dialog(_T("dim_styles"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; return NULL; }

   ads_start_list(dcl_id, _T("list"), LIST_NEW,0);
   pDimStyle = (C_STR *) DimStyles.get_head();
   while (pDimStyle)
   {
      gsc_add_list(pDimStyle->get_name());

      if (current && gsc_strcmp(pDimStyle->get_name(), current, FALSE) == 0)
         swprintf(strPos, 10, _T("%d"), pos);
      else pos++;
      
      pDimStyle = (C_STR *) pDimStyle->get_next();
   }
   ads_end_list();

   ads_set_tile(dcl_id, _T("list"), strPos);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_dim_styles_accept);
   ads_client_data_tile(dcl_id, _T("accept"), &pos);
   ads_action_tile(dcl_id, _T("list"),   (CLIENTFUNC) dcl_dim_styles_list);
   ads_client_data_tile(dcl_id, _T("list"), &pos);

   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGOK)
   {
      ACHAR *DimStyleName = DimStyles.goto_pos(pos + 1)->get_name();
      return gsc_tostring(DimStyleName);
   }
   else return NULL;
}


/***************************************************************************/
/*.doc gsc_set_dimStyle                                                    */
/*+                                                                       
  Funzione che cambia lo stile di quotatura ad un oggetto grafico.
  Parametri:
  ads_name  ent;
  AcDbObjectId &dimStyleId;   Stile di quotatura
  oppure
  AcDbObjectId &objId;
  TCHAR *DimStyle;   Stile di quotatura
  oppure
  AcDbEntity *pEnt;
  TCHAR *DimStyle;   Stile di quotatura
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************************/
int gsc_set_dimStyle(AcDbEntity *pEnt, AcDbObjectId &dimStyleId)
{
   if (!pEnt->isKindOf(AcDbDimension::desc()))
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (dimStyleId != ((AcDbDimension *)pEnt)->dimensionStyle())
   {
      Acad::ErrorStatus Res = ((AcDbDimension *) pEnt)->upgradeOpen();

      if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
      if (((AcDbDimension *)pEnt)->setDimensionStyle(dimStyleId) != Acad::eOk)
         { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   }

   return GS_GOOD;
}
int gsc_set_dimStyle(ads_name ent, const TCHAR *DimStyle)
{
   AcDbObjectId dimStyleId;

   if (gsc_getDimStyleId(DimStyle, dimStyleId) == GS_BAD) return GS_BAD;
   return gsc_set_dimStyle(ent, dimStyleId);
}
int gsc_set_dimStyle(ads_name ent, AcDbObjectId &dimStyleId)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_set_dimStyle((AcDbEntity *) pObj, dimStyleId) == GS_BAD)
      { pObj->close(); return GS_BAD; }
   pObj->close();

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  GESTIONE  STILI QUOTATURE   FINE    ///////////////////
//////////////////    GESTIONE    TEMATISMI    INIZIO   ///////////////////
//-----------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_refresh_thm                        <external> */
/*+
  Questa funzione permette il caricamento dei files .TTF, .SHX
  dal sotto-direttorio THM di GS_GEODIR (server) nel proprio direttorio
  di lavoro WORKDIR. Il file GEOHATCH viene copiato in WORKDIR con
  i nomi standard ACAD.PAT e ACADISO.PAT.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
void gsrefreshthm(void) // comando
{
   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_refresh_thm() == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   
   return GEOsimAppl::CMDLIST.EndCmd();
}
int gsc_refresh_thm(void)
{
   C_STRING Prefix, Dest, Src;

   // copio ACAD.LIN in GEOSIM.LIN e ACADISO.LIN in GEOSIMISO.LIN
   // nella subdirectory GEOTHMDIR di GEODIR (se ancora non ci sono)
   Prefix = GEOsimAppl::GEODIR;
   Prefix += _T('\\');
   Prefix += GEOTHMDIR;
   Prefix += _T('\\');
   Dest = Prefix;
   Dest += GEOTLINE;
   Dest += _T(".LIN");
   if (gsc_path_exist(Dest) == GS_BAD)
   {
      TCHAR Source[511 + 1]; // vedi manuale acad (funzione "acedFindFile")

      if (acedFindFile(_T("ACAD.LIN"), Source) == RTNORM)
         if (gsc_copyfile(Source, Dest.get_name()) == GS_BAD) return GS_BAD;
   }
   Dest = Prefix;
   Dest += GEOTLINE;
   Dest += _T("ISO.LIN");
   if (gsc_path_exist(Dest) == GS_BAD)
   {
      TCHAR Source[511 + 1]; // vedi manuale acad (funzione "acedFindFile")

      if (acedFindFile(_T("ACADISO.LIN"), Source) == RTNORM)
         if (gsc_copyfile(Source, Dest.get_name()) == GS_BAD) return GS_BAD;
   }

   // copio ACAD.PAT e ACADISO.PAT
   // nella subdirectory GEOTHMDIR di GEODIR se ancora non ci sono
   Dest = Prefix;
   Dest += GEOHATCH;
   Dest += _T(".PAT");
   if (gsc_path_exist(Dest) == GS_BAD)
   {
      TCHAR Source[511 + 1]; // vedi manuale acad (funzione "acedFindFile")

      if (acedFindFile(_T("ACAD.PAT"), Source) == RTNORM)
         if (gsc_copyfile(Source, Dest.get_name()) == GS_BAD) return GS_BAD;
   }
   // copio ACAD.PAT nella directory GEOWORK
   Src = GEOsimAppl::WORKDIR;
   Src += _T('\\');
   Src += GEOHATCH;
   Src += _T(".PAT");
   if (gsc_copyfile(Dest, Src) == GS_BAD) return GS_BAD;

   Dest = Prefix;
   Dest += GEOHATCH;
   Dest += _T("ISO.PAT");
   if (gsc_path_exist(Dest) == GS_BAD)
   {
      TCHAR Source[511 + 1]; // vedi manuale acad (funzione "acedFindFile")

      if (acedFindFile(_T("ACADISO.PAT"), Source) == RTNORM)
         if (gsc_copyfile(Source, Dest.get_name()) == GS_BAD) return GS_BAD;
   }
   // copio GEOSIMISO.PAT in ACADISO.PAT nella directory GEOWORK
   Src = GEOsimAppl::WORKDIR;
   Src += _T('\\');
   Src += GEOHATCH;
   Src += _T("ISO.PAT");
   if (gsc_copyfile(Dest, Src) == GS_BAD) return GS_BAD;

   Prefix = GEOsimAppl::GEODIR;
   Prefix += _T('\\');
   Prefix += GEOTHMDIR;

   // True Type Font
   if (gsc_copydir(Prefix.get_name(), _T("*.TTF"), GEOsimAppl::WORKDIR.get_name(), NORECURSIVE) == GS_BAD)
      return GS_BAD;
   // Stili testo
   if (gsc_copydir(Prefix.get_name(), _T("*.SHX"), GEOsimAppl::WORKDIR.get_name(), NORECURSIVE) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_LoadThmFromDwg                     <internal> */
/*+
  Carica i layer, gli stili testo, i tipi linea, gli stili di quotatura
  e i blocchi da un disegno esterno.
  Parametri:
  const TCHAR *DwgPath;          path completa del file DWG

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_LoadThmFromDwg(const TCHAR *DwgPath)
{
   C_STRING                tmp_path;
   AcDbDatabase            extDatabase(Adesk::kFalse), *pCurrDatabase;
   AcDbSymbolTable         *pSymbolTable   = NULL; 
   AcDbSymbolTableIterator *pSymbolTableIt = NULL;
   AcDbObjectId            ObjId; 
   AcDbObjectIdArray       ObjectIds;
   AcDbIdMapping           mMap;
   int                     UndoState = gsc_SetUndoRecording(FALSE); // disabilito UNDO
   Acad::ErrorStatus       Res;

   // database corrente
   pCurrDatabase = acdbHostApplicationServices()->workingDatabase();

   tmp_path = DwgPath;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) 
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }

   // leggo il dwg
   if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYNO) != Acad::eOk)
   {
      C_STRING ext;

      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione provo ad aggiungerla
         tmp_path += _T(".DWG");
      if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYNO) != Acad::eOk)
         { gsc_SetUndoRecording(UndoState); GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Lettura degli stili testo nel dwg
   if (extDatabase.getTextStyleTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo ID della tabella degli stili testo del database corrente
   if (pCurrDatabase->getTextStyleTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjId = pSymbolTable->objectId();
   pSymbolTable->close();

   // Clono gli stili testo
   if (pCurrDatabase->wblockCloneObjects(ObjectIds, ObjId, mMap, AcDb::kDrcIgnore) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei tipi-linea nel dwg
   if (extDatabase.getLinetypeTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo ID della tabella dei tipi-linea del database corrente
   pCurrDatabase->getLinetypeTable(pSymbolTable, AcDb::kForRead);
   ObjId = pSymbolTable->objectId();
   pSymbolTable->close();

   // Clono i tipi-linea
   if (pCurrDatabase->wblockCloneObjects(ObjectIds, ObjId, mMap, AcDb::kDrcIgnore) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei layers nel dwg
   if (extDatabase.getLayerTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo ID della tabella dei layer del database corrente
   pCurrDatabase->getLayerTable(pSymbolTable, AcDb::kForRead);
   ObjId = pSymbolTable->objectId();
   pSymbolTable->close();

   // Clono i layer
   if (pCurrDatabase->wblockCloneObjects(ObjectIds, ObjId, mMap, AcDb::kDrcIgnore) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura degli stili di quotatura nel dwg
   if (extDatabase.getDimStyleTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo ID degli stili di quotatura del database corrente
   pCurrDatabase->getDimStyleTable(pSymbolTable, AcDb::kForRead);
   ObjId = pSymbolTable->objectId();
   pSymbolTable->close();

   // Per chiamate consecutive alla funzione wblockCloneObjects l'ultima chiamata 
   // deve obbligatoriamente avere l'ultimo parametro (deferXlation) = false
   // quindi importo prima gli stili di quotatura che richiedono deferXlation = true
   // e per ultimo i blocchi con deferXlation = false (tanto gli stili di quotatura 
   // non fanno riferimento a blocchi)
   // Clono gli stili di quotatura (il quinto parametro = true indicating whether or not ID translation should be done)
   if ((Res = pCurrDatabase->wblockCloneObjects(ObjectIds, ObjId, mMap, AcDb::kDrcIgnore, true)) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei blocchi nel dwg
   if (extDatabase.getBlockTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); gsc_SetUndoRecording(UndoState); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo ID della tabella dei blocchi del database corrente
   pCurrDatabase->getBlockTable(pSymbolTable, AcDb::kForRead);
   ObjId = pSymbolTable->objectId();
   pSymbolTable->close();

   // Clono i blocchi
   if (pCurrDatabase->wblockCloneObjects(ObjectIds, ObjId, mMap, AcDb::kDrcIgnore) != Acad::eOk)
      { gsc_SetUndoRecording(UndoState); return GS_BAD; }
   ObjectIds.setLogicalLength(0);

   gsc_SetUndoRecording(UndoState); // Ripristina situazione UNDO precedente

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_PurgeDwg                           <internal> */
/*+
  Elimina i tematismi (layer, gli stili testo, i tipi linea, gli stili di quotatura
  e i blocchi) non usati nel disegno esterno.
  Parametri:
  const TCHAR *DwgPath;          path completa del file DWG

  Ritorna GS_GOOD in caso di inserimento corretto o GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_PurgeDwg(const TCHAR *DwgPath)
{
   C_STRING                tmp_path;
   AcDbDatabase            extDatabase(Adesk::kFalse);
   AcDbSymbolTable         *pSymbolTable   = NULL; 
   AcDbSymbolTableIterator *pSymbolTableIt = NULL;
   AcDbObjectId            ObjId; 
   AcDbObjectIdArray       ObjectIds;
   AcDbObject              *pObj;
   int                     len, i;
   long                    nPurged = 0;

   tmp_path = DwgPath;
   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;

   // leggo il dwg
   // _SH_DENYWR = open for read/write and allow no sharing
   if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYWR) != Acad::eOk)
   {
      C_STRING ext;

      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione provo ad aggiungerla
         tmp_path += _T(".DWG");
 
      // _SH_DENYWR = open for read/write and allow no sharing
      if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYWR) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Lettura degli stili testo nel dwg
   if (extDatabase.getTextStyleTable(pSymbolTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo quali oggetti sono eliminabili e li elimino
   if (extDatabase.purge(ObjectIds) == Acad::eOk && (len = ObjectIds.length()) > 0)     
      for (i = 0; i < len; i++)
         if (acdbOpenObject(pObj, ObjectIds[i], AcDb::kForWrite, Adesk::kTrue) == Acad::eOk)
         {
            pObj->erase();
            pObj->close();
            nPurged++;
         }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei tipi-linea nel dwg
   if (extDatabase.getLinetypeTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo quali oggetti sono eliminabili e li elimino
   if (extDatabase.purge(ObjectIds) == Acad::eOk && (len = ObjectIds.length()) > 0)     
      for (i = 0; i < len; i++)
         if (acdbOpenObject(pObj, ObjectIds[i], AcDb::kForWrite, Adesk::kTrue) == Acad::eOk)
         {
            pObj->erase();
            pObj->close();
            nPurged++;
         }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei layers nel dwg
   if (extDatabase.getLayerTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo quali oggetti sono eliminabili e li elimino
   if (extDatabase.purge(ObjectIds) == Acad::eOk && (len = ObjectIds.length()) > 0)     
      for (i = 0; i < len; i++)
         if (acdbOpenObject(pObj, ObjectIds[i], AcDb::kForWrite, Adesk::kTrue) == Acad::eOk)
         {
            pObj->erase();
            pObj->close();
            nPurged++;
         }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura degli stili di quotatura nel dwg
   if (extDatabase.getDimStyleTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo quali oggetti sono eliminabili e li elimino
   if (extDatabase.purge(ObjectIds) == Acad::eOk && (len = ObjectIds.length()) > 0)     
      for (i = 0; i < len; i++)
         if (acdbOpenObject(pObj, ObjectIds[i], AcDb::kForWrite, Adesk::kTrue) == Acad::eOk)
         {
            pObj->erase();
            pObj->close();
            nPurged++;
         }
   ObjectIds.setLogicalLength(0);

   ////////////////////////////////////////////////////////////////////////////
   // Lettura dei blocchi nel dwg
   if (extDatabase.getBlockTable(pSymbolTable, AcDb::kForRead) != Acad::eOk)
      return GS_BAD;
   if (pSymbolTable->newIterator(pSymbolTableIt, Adesk::kTrue, Adesk::kTrue) != Acad::eOk)
      { pSymbolTable->close(); return GS_BAD; }
   while (!pSymbolTableIt->done())
   {
      if (pSymbolTableIt->getRecordId(ObjId) != Acad::eOk)
         { delete pSymbolTableIt; pSymbolTable->close(); return GS_BAD; }
      ObjectIds.append(ObjId);
      pSymbolTableIt->step();
   }
   delete pSymbolTableIt;
   pSymbolTable->close();

   // Ricavo quali oggetti sono eliminabili e li elimino
   if (extDatabase.purge(ObjectIds) == Acad::eOk && (len = ObjectIds.length()) > 0)     
      for (i = 0; i < len; i++)
         if (acdbOpenObject(pObj, ObjectIds[i], AcDb::kForWrite, Adesk::kTrue) == Acad::eOk)
         {
            pObj->erase();
            pObj->close();
            nPurged++;
         }
   ObjectIds.setLogicalLength(0);

   if (nPurged > 0)
      if (extDatabase.saveAs(tmp_path.get_name()) != Acad::eOk)
         return GS_BAD;

   acutPrintf(gsc_msg(450), nPurged); // "\nEliminati %ld elemento/i dal disegno."

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_getShapeNameNumbers                                    <internal> */
/*+
  Questa funzione ricava il numero e il nome delle forme contenute nel
  file shx indicato.
  Parametri:
  const TCHAR *ShxPathFile;         Percorso del dile shx
  C_INT_STR_LIST &ShapeNameNumbers; Lista delle posizioni e dei nomi delle forme

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int gsc_getShapeNameNumbers(const TCHAR *ShxPathFile, C_INT_STR_LIST &ShapeNameNumbers)
{
   typedef struct
   {
      Adesk::UInt16 iShapeNumber;     // shape number
      Adesk::UInt16 iDefBytes;        // number of data bytes in shape, 2000 max
      Adesk::UInt8  *lpszShapeName;    // pointer to shapename
      Adesk::UInt8  *lpszSpecBytes;    // pointer to shape specification bytes
   } SHAPESTRUCT;

   C_STRING tmp_path(ShxPathFile);
   
   ShapeNameNumbers.remove_all();

   // Controlla Correttezza Path
   if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 

   Adesk::UInt8  SHAPE10[] = "AutoCAD-86 shapes 1.0";
   Adesk::UInt8  SHAPE11[] = "AutoCAD-86 shapes 1.1";

   FILE *fshx = _tfopen(tmp_path.get_name(), _T("rb"));

   if (!fshx) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

   Adesk::UInt8 desc[32];
   fread (desc,sizeof(desc),1,fshx);
   if (!strstr((char *)desc, (char *)SHAPE10) && !strstr((char *)desc, (char *)SHAPE11))
   {  // It is not shape file !
      GS_ERR_COD = eGSInvalidPath; 
      fclose(fshx);
      return GS_BAD;
   }
   fseek (fshx, (long)0x1c, SEEK_SET);
   Adesk::UInt16 iNumShapes = 0;
   fread (&iNumShapes, sizeof (iNumShapes), 1, fshx);
   SHAPESTRUCT *lpShapeList, *lpShape;
   lpShapeList = (SHAPESTRUCT *)calloc(iNumShapes, sizeof (*lpShapeList));
   for (int i=0; i < iNumShapes; i++)
   {
      lpShape = lpShapeList + i;
      fread(&(lpShape->iShapeNumber), sizeof (lpShape->iShapeNumber), 1, fshx);
      fread(&(lpShape->iDefBytes), sizeof (lpShape->iDefBytes), 1, fshx);
   }
   if (lpShapeList->iShapeNumber != 0)
   { // it is shape file
      for (int i = 0; i < iNumShapes; i++)
      {
         lpShape = lpShapeList + i;
         lpShape->lpszShapeName = (unsigned char *)(malloc (lpShape->iDefBytes));
         fread (lpShape->lpszShapeName, sizeof (*lpShape->lpszShapeName), lpShape->iDefBytes, fshx);
         AcString sname = lpShape->lpszShapeName;
         //acutPrintf(_T("\nShape N%d = %s"), i, sname.kACharPtr());
         ShapeNameNumbers.add_tail_int_str((int) lpShape->iShapeNumber, sname.kACharPtr());
         free(lpShape->lpszShapeName);
      }
   }
   else
   {  // It is not shape file !
      free(lpShapeList);
      GS_ERR_COD = eGSInvalidPath; 
      fclose(fshx);
      return GS_BAD;
   }
   
   free(lpShapeList);
   fclose(fshx);

   return GS_GOOD;
}