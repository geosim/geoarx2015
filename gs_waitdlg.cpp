// gs_waitdlg.cpp : file di implementazione
//

#include "stdafx.h"

#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_init.h"
#include "gs_waitdlg.h"


#define LABEL_HEIGHT    16
#define PROGRESS_HEIGHT 20
#define CAPTION_HEIGHT  40
#define Y_SPACE_BETWEEN_WAIT_COMPONENT 5


void gsc_displayTextOnProgressBar(CProgressCtrl &Progress, TCHAR *Text = NULL, bool FillSolidRect = false)
{
   CDC   *pDC = Progress.GetDC();
   CRect ClientRect;

   // setto il fonte normale (di default viene bold)
   //CFont* pOldFont = pDC->GetCurrentFont();
   CFont* pOldFont = pDC->SelectObject(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));

   pDC->SetBkMode(TRANSPARENT);
   pDC->SetTextColor(RGB(0,0,0)); // nero
   Progress.GetClientRect(ClientRect);
   if (FillSolidRect)
      pDC->FillSolidRect(ClientRect, ::GetSysColor(COLOR_3DFACE));
   if (Text== NULL)
      pDC->DrawText(_T(""), ClientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
   else
      pDC->DrawText(Text, ClientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
   
   pDC->SelectObject(pOldFont);
   Progress.ReleaseDC(pDC);
}

void gsc_displayTextOnProgressBar(CProgressCtrl *pProgress, WaitDlgComponent *pWaitDlgComponent)
{
   if (pWaitDlgComponent->MarqueeBar == false)
      pProgress->SetPos(pWaitDlgComponent->Prcnt);

   gsc_displayTextOnProgressBar(*pProgress,
                                pWaitDlgComponent->BarText.get_name(), 
                                pWaitDlgComponent->MarqueeBar);      
}


////////////////////////////////////////////////////////////////////////////////
//    class WaitDlgComponent                                                  //      
////////////////////////////////////////////////////////////////////////////////


// costruttore
WaitDlgComponent::WaitDlgComponent(void)
{
   MarqueeBar       = false;
   InitialTime.SetStatus(COleDateTime::null);
   Total            = 1;
   Prcnt            = -1;
   PercVis          = true;
   EstimatedTimeVis = true;
   Visible          = false;
   Threshold        = 2; // 2 sec.
}

WaitDlgComponent::WaitDlgComponent(WaitDlgComponent &item)
{
   (*this) = item;
}


/*************************************************************************/
/*.doc WaitDlgComponent::WaitDlgComponent                     <external> */
/*+
  Costruttore di un componente di attesa progress bar normale.
  Parametri:
  C_STRING &_LabelText;    titolo del componente
  bool _PercVis;           flag per vedere la percentuale
  bool _EstimatedTimeVis;   flag per vedere il tempo mancante
-*/  
/*************************************************************************/
WaitDlgComponent::WaitDlgComponent(C_STRING &_LabelText, bool _PercVis, bool _EstimatedTimeVis)
{
   ::WaitDlgComponent();
   SetProgressBarStyle(_LabelText, _PercVis, _EstimatedTimeVis);
}


/*************************************************************************/
/*.doc WaitDlgComponent::WaitDlgComponent                     <external> */
/*+
  Costruttore di un componente di attesa progress bar di tipo marquee.
  Parametri:
  C_STRING &_LabelText;    titolo del componente
-*/  
/*************************************************************************/
WaitDlgComponent::WaitDlgComponent(C_STRING &_LabelText)
{
   ::WaitDlgComponent();
   SetMarqueeBarStyle(_LabelText);
}
WaitDlgComponent::~WaitDlgComponent(void) {}

void WaitDlgComponent::operator = (WaitDlgComponent &in)
{
   LabelText        = in.LabelText;
   BarText          = in.BarText;
   MarqueeBar       = in.MarqueeBar;
   InitialTime      = in.InitialTime;
   Total            = in.Total;
   Prcnt            = in.Prcnt;
   PercVis          = in.PercVis;
   EstimatedTimeVis = in.EstimatedTimeVis;
   Visible          = in.Visible;
   Threshold        = in.Threshold;
}


/*************************************************************************/
/*.doc WaitDlgComponent::SetBar_Perc                          <external> */
/*+
  Aggiorna la percentuale di avanzamento e aggiorna il testo sulla barra.
  Parametri:
  int NewPrcnt;    nuova percentuale elaborata

  Ritorna true se la percentuale è cambiata rispetto a quella precedente
-*/  
/*************************************************************************/
bool WaitDlgComponent::SetBar_Perc(int NewPrcnt)
{
   if (NewPrcnt > Prcnt)
   {
      Prcnt = NewPrcnt;
      getUpdatedBarText(BarText);

      return true;
   }
   return false;
}


/*************************************************************************/
/*.doc WaitDlgComponent::getUpdatedBarText                    <external> */
/*+
  Ritorna il testo aggiornato da visualizzare
-*/  
/*************************************************************************/
void WaitDlgComponent::getUpdatedBarText(C_STRING &UpdatedBarText)
{
   UpdatedBarText.clear();
   if (PercVis && Prcnt > 0)
   {
      UpdatedBarText += Prcnt;
      UpdatedBarText += _T("%");
   }
   if (EstimatedTimeVis)
   {
      COleDateTimeSpan Span = getEstimatedTime();
      if (Span.GetStatus() == COleDateTimeSpan::valid)
      {
         C_STRING SpanMsg;
         
         if (PercVis && Prcnt > 0) UpdatedBarText += _T(" - ");
         gsc_getDateTimeSpanFormat(Span, SpanMsg);
         UpdatedBarText += gsc_msg(490); // "Circa "
         UpdatedBarText += SpanMsg;
         UpdatedBarText += gsc_msg(489); // " al termine."
      }
   }
}


/*************************************************************************/
/*.doc WaitDlgComponent::getEstimatedTime                      <external> */
/*+
  Ritorna il tempo ancora da attendere alla fine dell'attesa.
-*/  
/*************************************************************************/
COleDateTimeSpan WaitDlgComponent::getEstimatedTime(void)
{
   COleDateTimeSpan Span;

   if (Prcnt > 0)
      Span = (double (COleDateTime::GetCurrentTime() - InitialTime)) * (100 - Prcnt) / Prcnt;
   else
      Span.SetStatus(COleDateTimeSpan::null);

   return Span;
}


/*************************************************************************/
/*.doc WaitDlgComponent::getTotalEstimatedTime                 <external> */
/*+
  Ritorna il tempo totale da attendere alla fine dell'attesa.
-*/  
/*************************************************************************/
COleDateTimeSpan WaitDlgComponent::getTotalEstimatedTime(void)
{
   COleDateTimeSpan Span;

   if (Prcnt > 0)
      Span = (double (COleDateTime::GetCurrentTime() - InitialTime)) * 100 / Prcnt;
   else
      Span.SetStatus(COleDateTimeSpan::null);

   return Span;
}


/*************************************************************************/
/*.doc WaitDlgComponent::getVisible                          <external> */
/*+
  Ritorna se il componente deve essere visibile.
-*/  
/*************************************************************************/
bool WaitDlgComponent::getVisible(void)
{
   if (!Visible)
   {
      if (MarqueeBar)
         Visible = (BarText.len() > 0) ? true : false;
      else
      {
         COleDateTimeSpan Span = getTotalEstimatedTime();

         if (Span.GetStatus() == COleDateTimeSpan::valid)
            if (Span.GetTotalSeconds() > Threshold) Visible = true;
      }
   }

   return Visible;
}

void WaitDlgComponent::ForceVisible(void)
   { Visible = true; }


/*************************************************************************/
/*.doc WaitDlgComponent::SetBar                               <external> */
/*+
  Aggiorna l'avanzamento impostando i-esimo elemento da elaborare e
  aggiorna il testo sulla barra.
  Parametri:
  long i;    i-esimo elemento da elaborare

  Ritorna true se l'avanzamneto è cambiato rispetto a quella precedente
-*/  
/*************************************************************************/
bool WaitDlgComponent::SetBar(long i)
{
   if (MarqueeBar)
   {
      if ((i % Threshold) == 0) // ogni Threshold volte
      {
         C_STRING Msg;

         if (Format.get_name())
            Msg.set_name_formatted(Format.get_name(), i);
         else
            Msg = i;

         return SetBar_Text(Msg);
      }
      return false;
   }
   else
   {
      int Prcnt = (int) ((double) i * 100 / Total);
      return SetBar_Perc(Prcnt);
   }
}


/*************************************************************************/
/*.doc WaitDlgComponent::SetBar_Text                          <external> */
/*+
  Aggiorna il testo sulla barra.
  Parametri:
  C_STRING &Msg;    nuovo testo

  Ritorna true se il testo è cambiato rispetto a quello precedente
-*/  
/*************************************************************************/
bool WaitDlgComponent::SetBar_Text(C_STRING &Msg)
{
   if (Msg == BarText) return false;
   BarText = Msg;

   return true;
}


/*************************************************************************/
/*.doc WaitDlgComponent::SetProgressBarStyle                  <external> */
/*+
  Setta lo stile del componente di attesa come progress bar normale.
  Da usare quando si conosce il totale.
  Parametri:
  C_STRING &_LabelText;    titolo del componente
  bool _PercVis;           flag per vedere la percentuale
  bool _EstimatedTimeVis;   flag per vedere il tempo mancante
-*/  
/*************************************************************************/
void WaitDlgComponent::SetProgressBarStyle(C_STRING &_LabelText, bool _PercVis, bool _EstimatedTimeVis)
{
   MarqueeBar = false;
   LabelText = _LabelText;
   PercVis  = _PercVis;
   EstimatedTimeVis = _EstimatedTimeVis;
}


/*************************************************************************/
/*.doc WaitDlgComponent::SetMarqueeBarStyle                   <external> */
/*+
  Setta lo stile del componente di attesa come barra marqueeBar che va avanti
  e indietro. Da usare quando non si conosce il totale.
  Parametri:
  C_STRING &_LabelText;    titolo del componente
  bool _PercVis;           flag per vedere la percentuale
  bool _EstimatedTimeVis;   flag per vedere il tempo mancante
-*/  
/*************************************************************************/
void WaitDlgComponent::SetMarqueeBarStyle(C_STRING &_LabelText)
{
   MarqueeBar = true;
   LabelText = _LabelText;
}


/*************************************************************************/
/*.doc WaitDlgComponent::Init                                 <external> */
/*+
  Inizializza il conteggio e il tempo di attesa.
  Da usare appena prima del ciclo di elaborazione.
  Parametri:
  long _Total;
-*/  
/*************************************************************************/
void WaitDlgComponent::Init(long _Total)
{
   MarqueeBar  = false;
   Total       = (_Total <= 0) ? 1 : _Total;
   Prcnt       = -1;
   InitialTime = COleDateTime::GetCurrentTime();
   BarText.clear();
}


////////////////////////////////////////////////////////////////////////////////
//  finestra di dialogo CWaitDlg                                              //      
////////////////////////////////////////////////////////////////////////////////


IMPLEMENT_DYNAMIC(CWaitDlg, CDialogEx)

CWaitDlg::CWaitDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CWaitDlg::IDD, pParent)
{
}

CWaitDlg::~CWaitDlg()
{
   C_INT_VOIDPTR    *p;
   WaitDlgComponent *pWaitDlgComponent;
   CStatic          *pLabel;
   CProgressCtrl     *pProgress;

   p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_head();
   while (p)
   {
      if ((pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr()))
         delete pWaitDlgComponent;

      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_next();
   }

   p = (C_INT_VOIDPTR *) m_LabelList.get_head();
   while (p)
   {
      if ((pLabel = (CStatic *) p->get_VoidPtr()))
         delete pLabel;

      p = (C_INT_VOIDPTR *) m_LabelList.get_next();
   }

   p = (C_INT_VOIDPTR *) m_ProgressList.get_head();
   while (p)
   {
      if ((pProgress = (CProgressCtrl *) p->get_VoidPtr()))
         delete pProgress;

      p = (C_INT_VOIDPTR *) m_ProgressList.get_next();
   }
}

void CWaitDlg::DoDataExchange(CDataExchange* pDX)
{
   CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CWaitDlg, CDialogEx)
END_MESSAGE_MAP()


// gestori di messaggi CWaitDlg
BOOL CWaitDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


int CWaitDlg::Init(WaitDlgComponent &WaitComponent)
{
   m_WaitDlgComponentList.add_tail_int_voidptr(m_WaitDlgComponentList.get_count() + 1, new WaitDlgComponent(WaitComponent));
   RefreshCtrls();

   return getWaitDlgComponentCount();
}


bool CWaitDlg::RefreshCtrls(void)
{
   int              Y;
   bool             ToUpdate = false, ToShow = false;
   WaitDlgComponent *pWaitDlgComponent;
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();

   // parto dal fondo tornando indietro per vedere l'ultimo componente che deve essere visibile
   while (p)
   {
      pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
      if (pWaitDlgComponent->getVisible()) // se deve essere visibile
         break;
      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_prev();
   }

   // tutti i componenti da p fino all'inizio devono essere visibili
   while (p)
   {
      pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
      pWaitDlgComponent->ForceVisible();

      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_prev();
   }

   Y = 0;
   p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_head();
   while (p)
   {
      pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
      if (RefreshCtrl(p, &Y)) ToUpdate = true;

      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_next();
   }

   if (ToUpdate)
   {
      CRect rcItem;

      GetWindowRect(rcItem);
      Y = CAPTION_HEIGHT + Y + Y_SPACE_BETWEEN_WAIT_COMPONENT;
      SetWindowPos(NULL, 0, 0, rcItem.Width(), Y, SWP_NOMOVE);
      gsc_CenterDialog(this);

      ShowWindow(SW_SHOW);
      RedrawWindow(NULL, NULL, RDW_UPDATENOW);
      UpdateWindow();
   }

   return ToUpdate;
}


bool CWaitDlg::RefreshCtrl(C_INT_VOIDPTR *p, int *Y)
{
   WaitDlgComponent *pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
   CRect            rcItem;
   int              X = 0, ControlWidth, key = p->get_key();
   CProgressCtrl    *pProgress;
   C_INT_VOIDPTR    *pItem;
   bool             result = false;

   if (pWaitDlgComponent->getVisible() == false) return false;

   // se deve essere visibile
   if (IsWindow(m_hWnd) == FALSE)
      if (Create(CWaitDlg::IDD, GetParent()) == FALSE) return false;

   if (*Y > 0) // se non è il primo componente visibile
      *Y += Y_SPACE_BETWEEN_WAIT_COMPONENT; // spazio tra i componenti di attesa

   GetClientRect(rcItem);
   ControlWidth = rcItem.Width();

   if (pWaitDlgComponent->LabelText.len() > 0)
   {
      CStatic *pLabel;

      if (!(pItem = (C_INT_VOIDPTR *) m_LabelList.search_key(key)))
      {
         pLabel = new CStatic();

         m_LabelList.add_tail_int_voidptr(key, pLabel);
         rcItem.SetRect(X, *Y, X + ControlWidth, *Y + LABEL_HEIGHT);

         pLabel->Create(pWaitDlgComponent->LabelText.get_name(), WS_VISIBLE, rcItem, this, 0xffff);

         result = true;
      }
      *Y += LABEL_HEIGHT;
   }

   if (!(pItem = (C_INT_VOIDPTR *) m_ProgressList.search_key(key)))
   {
      pProgress = new CProgressCtrl();
      m_ProgressList.add_tail_int_voidptr(key, pProgress);
      rcItem.SetRect(X, *Y, X + ControlWidth, *Y + PROGRESS_HEIGHT);
      pProgress->Create(WS_VISIBLE | WS_BORDER, rcItem, this, 0xffff);
      if (pWaitDlgComponent->MarqueeBar)
         pProgress->SetMarquee(TRUE, 30);
      else
         pProgress->SetMarquee(FALSE, 0);
      SetWindowTheme(pProgress->m_hWnd, L" ", L" "); // XP Style per evitare baco di ritardo nella visualizzazione
      pProgress->SetRange32(0, 100);

      gsc_displayTextOnProgressBar(pProgress, pWaitDlgComponent);
      result = true;
   }
   *Y += PROGRESS_HEIGHT;

   return result;
}


/*************************************************************************/
/*.doc CWaitDlg::RefreshPrevBarTextOnTimeChanged              <internal> */
/*+
  Ridisegna i testi delle progress bar tralasciando l'ultima.
  Il testo delle progress bar precedenti (rispetto l'ultima) può essere cambiato
  se queste devono visualizzare il tempo stimato.
-*/  
/*************************************************************************/
void CWaitDlg::RefreshPrevBarTextOnTimeChanged(void)
{
   WaitDlgComponent *pWaitDlgComponent;
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();

   // parto dal penultimo componente di attesa
   p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_prev();
   while (p)
   {
      pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
      if (pWaitDlgComponent->EstimatedTimeVis)
      {
         C_STRING UpdatedBarText;
         pWaitDlgComponent->getUpdatedBarText(UpdatedBarText);
         if (pWaitDlgComponent->BarText != UpdatedBarText)
         {
            pWaitDlgComponent->BarText = UpdatedBarText;
            if ((p = (C_INT_VOIDPTR *) m_ProgressList.search_key(p->get_key())))
            {
               CProgressCtrl *pProgress = (CProgressCtrl *) p->get_VoidPtr();
               if (pProgress)
                  gsc_displayTextOnProgressBar(pProgress, pWaitDlgComponent);
            }
         }
      }

      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_prev();
   }
}


int CWaitDlg::End(void)
{
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();
   int              key;
   WaitDlgComponent *pWaitDlgComponent;

   if (!p) return getWaitDlgComponentCount();
   key = p->get_key();

   m_WaitDlgComponentList.remove_at();
   pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
   if (pWaitDlgComponent) delete pWaitDlgComponent;

   if ((p = (C_INT_VOIDPTR *) m_LabelList.search_key(key)))
   {
      CStatic *pLabel = (CStatic *) p->get_VoidPtr();

      if (pLabel)
      {
         pLabel->ShowWindow(SW_HIDE);
         delete pLabel;
         m_LabelList.remove_at();
      }
   }

   if ((p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key)))
   {
      CProgressCtrl *pProgress = (CProgressCtrl *) p->get_VoidPtr();

      if (pProgress)
      {
         pProgress->ShowWindow(SW_HIDE);
         delete pProgress;
         m_ProgressList.remove_at();
      }
   }

   if (getWaitDlgComponentCount() == 0)
   {         
      if (IsWindow(m_hWnd)) ShowWindow(SW_HIDE);
   }
   else
   {
      // resize dialog
      int   Y = 0;
      CRect rcItem;

      p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_head();
      while (p)
      {
         pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
         Y += getWaitDlgComponentHeight(*pWaitDlgComponent);
         p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_next();
      }
      Y += m_WaitDlgComponentList.get_count() * Y_SPACE_BETWEEN_WAIT_COMPONENT; // spazio tra i componenti di attesa

      GetWindowRect(rcItem);
      SetWindowPos(NULL, 0, 0, rcItem.Width(), CAPTION_HEIGHT + Y, SWP_NOMOVE);
      gsc_CenterDialog(this);
   }

   return getWaitDlgComponentCount();
}

int CWaitDlg::getWaitDlgComponentHeight(WaitDlgComponent &item)
{
   int height = 0;

   if (item.LabelText.len() > 0) height += LABEL_HEIGHT; // labelHeight
   height += PROGRESS_HEIGHT; // progressHeight
   return height;
}


void CWaitDlg::Set(C_STRING &Msg)
{
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();
   int              key;
   WaitDlgComponent *pWaitDlgComponent;

   if (!p) return;
   key = p->get_key();
   pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();

   p = NULL;
   if (pWaitDlgComponent->getVisible() == true && 
       !(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key)))
       RefreshCtrls();

   if (pWaitDlgComponent->SetBar_Text(Msg))
   {
      if (!p)
         if (!(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key))) return;
      CProgressCtrl *pProgress = (CProgressCtrl *) p->get_VoidPtr();
      if (pProgress)
      {
         gsc_displayTextOnProgressBar(pProgress, pWaitDlgComponent);
         //RefreshPrevBarTextOnTimeChanged();
         RedrawWindow(NULL, NULL, RDW_UPDATENOW);
         Invalidate();
         UpdateWindow();
      }
   }
}


void CWaitDlg::Set(long i)
{
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();
   int              key;
   WaitDlgComponent *pWaitDlgComponent;
   bool             ToUpdate;

   if (!p) return;
   key = p->get_key();
   pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();
   ToUpdate = pWaitDlgComponent->SetBar(i);

   p = NULL;
   if (pWaitDlgComponent->getVisible() == true && 
       !(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key)))
      RefreshCtrls();

   if (ToUpdate)
   {
      if (!p)
         if (!(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key))) return;
      CProgressCtrl *pProgress = (CProgressCtrl *) p->get_VoidPtr();
      if (pProgress)
      {
         gsc_displayTextOnProgressBar(pProgress, pWaitDlgComponent);
         //RefreshPrevBarTextOnTimeChanged();
         RedrawWindow(NULL, NULL, RDW_UPDATENOW);
         UpdateWindow();
      }
   }
}

void CWaitDlg::SetPerc(int NewPrcnt)
{
   C_INT_VOIDPTR    *p = (C_INT_VOIDPTR *) m_WaitDlgComponentList.get_tail();
   int              key;
   WaitDlgComponent *pWaitDlgComponent;

   if (!p) return;
   key = p->get_key();
   pWaitDlgComponent = (WaitDlgComponent *) p->get_VoidPtr();

   p = NULL;
   if (pWaitDlgComponent->getVisible() == true && 
       !(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key)))
       RefreshCtrls();

   if (pWaitDlgComponent->SetBar_Perc(NewPrcnt))
   {
      if (!p)
         if (!(p = (C_INT_VOIDPTR *) m_ProgressList.search_key(key))) return;
      CProgressCtrl *pProgress = (CProgressCtrl *) p->get_VoidPtr();
      if (pProgress)
      {
         gsc_displayTextOnProgressBar(pProgress, pWaitDlgComponent);
         //RefreshPrevBarTextOnTimeChanged();
         RedrawWindow(NULL, NULL, RDW_UPDATENOW);
         UpdateWindow();
      }
   }
}

int CWaitDlg::getWaitDlgComponentCount(void)
{
   return m_WaitDlgComponentList.get_count();
}


////////////////////////////////////////////////////////////////////////////////
//    class C_WAIT_DLG_INSTANCE                                               //      
////////////////////////////////////////////////////////////////////////////////

CWaitDlg *C_WAIT_DLG_INSTANCE::pWaitDlg = NULL;

// costruttore
C_WAIT_DLG_INSTANCE::C_WAIT_DLG_INSTANCE(void)
{}

C_WAIT_DLG_INSTANCE::~C_WAIT_DLG_INSTANCE(void)
{
   if (pWaitDlg) delete pWaitDlg;
}

int C_WAIT_DLG_INSTANCE::Init(WaitDlgComponent &item)
{
   int n = 0;

   if (!pWaitDlg)
   {
      // When resource from this ARX app is needed, just
      // instantiate a local CAcModuleResourceOverride
      CAcModuleResourceOverride myResources;
      CWnd *pAcadWindow = CWnd::FromHandlePermanent(adsw_acadMainWnd()), *pParentWnd;

      if (pAcadWindow->GetActiveWindow())
         pParentWnd = pAcadWindow->GetActiveWindow();
      else
         pParentWnd = pAcadWindow;

      if ((pWaitDlg = new CWaitDlg(pParentWnd)) == NULL) return 0;
   }
   n = pWaitDlg->Init(item);
   
   return n;
}

int C_WAIT_DLG_INSTANCE::End(void)
{
   int n = 0;

   if (pWaitDlg)
   {
      n = pWaitDlg->End();
      if (pWaitDlg->getWaitDlgComponentCount() == 0)
      {         
         pWaitDlg->DestroyWindow();
         delete pWaitDlg; pWaitDlg = NULL; 
      }
   }

   return n;
}


void C_WAIT_DLG_INSTANCE::Set(C_STRING &Msg)
{
   if (!pWaitDlg) return;
   pWaitDlg->Set(Msg);
}

void C_WAIT_DLG_INSTANCE::Set(long i)
{
   if (!pWaitDlg) return;
   pWaitDlg->Set(i);
}

void C_WAIT_DLG_INSTANCE::SetPerc(int NewPrcnt)
{
   if (!pWaitDlg) return;
   pWaitDlg->SetPerc(NewPrcnt);
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_WAIT_DLG_INSTANCE
// INIZIO FUNZIONI C_STATUSBAR_PROGRESSMETER
///////////////////////////////////////////////////////////////////////////


void C_STATUSBAR_PROGRESSMETER::internalInitialization(C_STRING &Title)
{
   initialized = false;
   m_WaitDlgComponent.SetProgressBarStyle(Title, true, true); // PercVis e EstimatedTimeVis
}
C_STATUSBAR_PROGRESSMETER::C_STATUSBAR_PROGRESSMETER(C_STRING &Title)
{
   internalInitialization(Title);
}
C_STATUSBAR_PROGRESSMETER::C_STATUSBAR_PROGRESSMETER(const TCHAR *format, ...)
{
   va_list args;

   if (!format)
   {
      C_STRING Title;
      internalInitialization(Title);
      return;
   }

   va_start(args, format);
   C_STRING Title(format, args);
   va_end(args);

   internalInitialization(Title);
}
C_STATUSBAR_PROGRESSMETER::~C_STATUSBAR_PROGRESSMETER()
   { End(); }
void C_STATUSBAR_PROGRESSMETER::Set(long i)
   { GEOsimAppl::WAIT_DLG_INSTANCE.Set(i); }
void C_STATUSBAR_PROGRESSMETER::Set_Perc(int Prcnt)
   { GEOsimAppl::WAIT_DLG_INSTANCE.SetPerc(Prcnt); }
void C_STATUSBAR_PROGRESSMETER::Init(long _Total, bool printTitle)
{
   if (initialized == true) return; // già inizializzato

   m_WaitDlgComponent.Init(_Total);
   GEOsimAppl::WAIT_DLG_INSTANCE.Init(m_WaitDlgComponent);
   initialized = true;

   if (printTitle && m_WaitDlgComponent.LabelText.get_name())
      acutPrintf(_T("\n%s"), m_WaitDlgComponent.LabelText.get_name());
}
void C_STATUSBAR_PROGRESSMETER::End(const TCHAR *formatEndTitle, ...)
{
   if (initialized == true)
   {
      GEOsimAppl::WAIT_DLG_INSTANCE.End();
      initialized = false; 
   }

   if (formatEndTitle)
   {
      va_list args;

      va_start(args, formatEndTitle);
      C_STRING EndTitle(formatEndTitle, args);
      va_end(args);
      if (EndTitle.get_name())
         acutPrintf(_T("\n%s"), EndTitle.get_name());
   }
}


///////////////////////////////////////////////////////////////////////////
// FINE   FUNZIONI C_STATUSBAR_PROGRESSMETER
// INIZIO FUNZIONI C_STATUSLINE_MESSAGE
///////////////////////////////////////////////////////////////////////////


void C_STATUSLINE_MESSAGE::internalInitialization(C_STRING &Title)
{
   initialized = false;
   m_WaitDlgComponent.SetMarqueeBarStyle(Title);
}
C_STATUSLINE_MESSAGE::C_STATUSLINE_MESSAGE(C_STRING &Title)
{
   internalInitialization(Title);
}
C_STATUSLINE_MESSAGE::C_STATUSLINE_MESSAGE(const TCHAR *format, ...)
{
   va_list args;

   if (!format)
   {
      C_STRING Title;
      internalInitialization(Title);
      return;
   }

   va_start(args, format);
   C_STRING Title(format, args);
   va_end(args);

   internalInitialization(Title);
}
C_STATUSLINE_MESSAGE::~C_STATUSLINE_MESSAGE()
   { End(); }
void C_STATUSLINE_MESSAGE::Set(const TCHAR *format, ...)
{
   va_list args;

   if (!format)
   {
      C_STRING Msg;
      return GEOsimAppl::WAIT_DLG_INSTANCE.Set(Msg);
   }

   va_start(args, format);
   C_STRING Msg(format, args);
   va_end(args);

   GEOsimAppl::WAIT_DLG_INSTANCE.Set(Msg);
}
void C_STATUSLINE_MESSAGE::Set(C_STRING &Msg)
   { GEOsimAppl::WAIT_DLG_INSTANCE.Set(Msg); }
void C_STATUSLINE_MESSAGE::Set(long i)
   { GEOsimAppl::WAIT_DLG_INSTANCE.Set(i); }

void C_STATUSLINE_MESSAGE::Init(const TCHAR *Format, int OptimizedEvery, bool printTitle)
{
   if (initialized == true) return; // già inizializzato

   m_WaitDlgComponent.Threshold = OptimizedEvery;
   m_WaitDlgComponent.Format = Format;
   GEOsimAppl::WAIT_DLG_INSTANCE.Init(m_WaitDlgComponent);
   initialized = true; 

   if (printTitle && m_WaitDlgComponent.LabelText.get_name())
      acutPrintf(_T("\n%s"), m_WaitDlgComponent.LabelText.get_name());
}
void C_STATUSLINE_MESSAGE::End(const TCHAR *formatEndTitle, ...)
{
   if (initialized == true)
   {
      GEOsimAppl::WAIT_DLG_INSTANCE.End();
      initialized = false; 
   }

   if (formatEndTitle)
   {
      va_list args;

      va_start(args, formatEndTitle);
      C_STRING EndTitle(formatEndTitle, args);
      va_end(args);
      if (EndTitle.get_name())
         acutPrintf(_T("\n%s"), EndTitle.get_name());
   }
}


///////////////////////////////////////////////////////////////////////////
// FINE   FUNZIONI C_STATUSLINE_MESSAGE
///////////////////////////////////////////////////////////////////////////
