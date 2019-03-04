/*************************************************************************/
/*   GS_TOPO.H                                                           */
/*************************************************************************/


#ifndef _gs_topo_h
#define _gs_topo_h 1

#ifndef _gs_list_h
   #include "gs_list.h" 
#endif

#ifndef _gs_class_h
   #include "gs_class.h" 
#endif

#ifndef _gs_filtr_h
   #include "gs_filtr.h" 
#endif


//---------------------------------------------------------------------------//
// Struttura per caricare la topologia di tipo rete in memoria
// Struttura per memorizzare per ciascun link:
// codice sottoclasse, codice GEOsim
// la posizione nel vettore dei nodi del nodo iniziale, finale,
// costo per percorrere il link dal nodo iniziale a quello finale
// costo per percorrere il link dal nodo finale a quello iniziale
//---------------------------------------------------------------------------//
struct TOPONET_LINK
{
   int  sub;          // codice sottoclasse
   long gs_id;        // codice GEOsim
   bool visited;      // link già visitato
   long InitialNode;  // posizione nel vettore dei nodi del nodo iniziale
   long FinalNode;    // posizione nel vettore dei nodi del nodo finale
   double In2Fi_Cost; // costo per percorrere il link dal nodo iniziale a quello finale
   double Fi2In_Cost; // costo per percorrere il link dal nodo finale a quello iniziale
   void *Punt;        // puntatore opzionale da utilizzare per aggiungere ulteriori
                      // informazioni (da allocare e disallocare manualmente)
};
DllExport void gsc_freeTopoNetLink(TOPONET_LINK **LinksVett, long *nLinks);
DllExport void gsc_freeTopoNetLinkAuxInfo(TOPONET_LINK *LinksVett, long nLinks);
DllExport long gsc_searchTopoNetLink(TOPONET_LINK *LinksVett, long nLinks, long gs_id);


//---------------------------------------------------------------------------//
// Struttura per caricare la topologia di tipo rete in memoria
// Struttura per memorizzare per ciascun nodo:
// codice sottoclasse, codice GEOsim
// la lista dei link e dei costi per andare su quel link
//---------------------------------------------------------------------------//
struct TOPONET_NODE
{
   int  sub;                    // codice sottoclasse
   long gs_id;                  // codice GEOsim
   bool visited;                // nodo già visitato
   C_LONG_REAL_LIST *pLinkList; // puntatore alla lista dei link e dei costi per andare su quel link
   void *Punt;                  // puntatore opzionale da utilizzare per aggiungere ulteriori
                                // informazioni (da allocare e disallocare manualmente)
};
DllExport void gsc_freeTopoNetNode(TOPONET_NODE **NodesVett, long *nNodes);
DllExport void gsc_freeTopoNetNodeAuxInfo(TOPONET_NODE *NodesVett, long nNodes);
DllExport long gsc_searchTopoNetNode(TOPONET_NODE *NodesVett, long nNodes, long gs_id);


//---------------------------------------------------------------------------//
// MEMORIZZA UN B-ALBERO DEI PUNTI E, PER CIASCUNO DI QUESTI, UNA LISTA      //
// DEGLI OGGETTI PUNTUALI CHE HANNO QUEL PUNTO DI INSERIMENTO                //
//---------------------------------------------------------------------------//
class C_BPOINT_OBJS : public C_BNODE
{
   protected :  
      ads_point       pt;
      C_CLS_PUNT_LIST EntList;

   public :
      DllExport C_BPOINT_OBJS();
      DllExport C_BPOINT_OBJS(ads_namep ent);
      DllExport C_BPOINT_OBJS(ads_pointp in);
      DllExport virtual ~C_BPOINT_OBJS();

      DllExport int get_point(ads_point out);
      DllExport int set_point(ads_point in);
      DllExport C_CLS_PUNT_LIST *ptr_EntList(void);
      DllExport int add_ent2EntList(ads_name ent);
      DllExport int del_ent2EntList(ads_name ent);
      DllExport int add_pClsPunt2EntList(C_CLS_PUNT *pClsPunt);      

      // funzione che stabilisce l'ordine degli elementi (ordine per X, Y, Z)
      DllExport int compare(const void *in);
     
      // funzione che stampa
      void print(int *level = NULL);
};


class C_POINT_OBJS_BTREE : public C_BTREE
{
   public :
      DllExport C_POINT_OBJS_BTREE();
      DllExport virtual ~C_POINT_OBJS_BTREE();  // chiama ~C_BTREE

      // alloc item
      DllExport C_BNODE* alloc_item(const void *in);
      DllExport int add_ent(ads_name ent);
      DllExport int add_pClsPunt(C_CLS_PUNT *pClsPunt);
      DllExport int add_SelSet(C_SELSET &SelSet);
};


//----------------------------------------------------------------------------//
//    class C_TOPOLOGY                                                        //
//    Classe per la gestione delle topologie GEOsim                           //
//----------------------------------------------------------------------------//


class C_TOPOLOGY : public C_INT
{
   protected :
                   // il codice topologia è in <key> ereditato da C_INT
   int      type;  // tipo di topologia (TYPE_NODE, TYPE_POLYLINE, TYPE_SURFACE)
   int      Temp;  // Se GS_GOOD indica che la topologia è temporanea
   C_EXTERN *pCls; // puntatore a classe esterna a cui si riferisce la topologia

   _RecordsetPtr    RsInsertTopo;            // RecordSet di inserimento in tabella Topologia
   C_PREPARED_CMD   CmdSelectWhereLinkTopo;  // Comando di select in tabella Topologia cercando un link
   _CommandPtr      CmdSelectWhereNodeTopo;  // Comando di select in tabella Topologia 
                                             // cercando un nodo
   _CommandPtr      CmdSelectWhereNodesTopo; // Comando di select in tabella Topologia
                                             // cercando links tra 2 nodi
   C_PREPARED_CMD_LIST CmdSelectWhereKeySubList; // Lista di comandi per cercare la sottoclasse 
                                                 // di un nodo avente codice entità conosciuto

   // Liste di nodi e links usate internamente alle funzioni di analisi
   C_INT_LONG_LIST m_VisitedNodes; 
   C_INT_LONG_LIST m_VisitedLinks;

   // Lista di nodi e dei links usati per analizzare la topologia rete
   // (risultato ad esempio del precorso + breve o della propagazione in rete)
   C_INT_LONG_LIST NetNodes; 
   C_INT_LONG_LIST NetLinks;

   private :
      int PrepareInsertTopo(void);
      int PrepareSelectWhereLinkTopo(void);
      int SelectWhereLinkTopo(long Key, _RecordsetPtr &pRs, int *IsRsCloseable);

      int PrepareSelectWhereNodeTopo(void);
      int PrepareSelectWhereNodesTopo(void);

      int PrepareSelectWhereKeyNodalSubList(void);
      void Terminate(void);
      void ClearNetStructure();

      int GetOptimizedNetPathWithOrderedNodes(C_LONG_LIST &OrderedNodes);
      int GetOptimizedNetPathWithCasualNodes(C_LONG_LIST &CasualNodes);

   public :
DllExport C_TOPOLOGY();
DllExport virtual ~C_TOPOLOGY();   // chiama ~C_NODE

      double NetCost;         // Costo di un percorso di rete
      long   NetFinalNode;    // Nodo finale in un percorso di rete

      // Vettori di topologia di nodi e links usate dalle funzioni di analisi
      TOPONET_NODE *NodesVett;
      long         nNodesVett;
      TOPONET_LINK *LinksVett;
      long         nLinksVett;

DllExport int  get_type();
DllExport int  set_type(int in);
DllExport int  is_temp();
DllExport int  set_temp(int in);
DllExport C_EXTERN *get_cls();
DllExport int  set_cls(C_CLASS *pMotherCls);
DllExport C_INT_LONG_LIST *ptr_NetNodes();
DllExport C_INT_LONG_LIST *ptr_NetLinks();

DllExport int  getTopoTabInfo(C_DBCONNECTION **pConn, C_STRING *TableRef = NULL,
                              int Create = GS_BAD);
DllExport int  getTopoTabName(C_DBCONNECTION *pConn, int prj, int cls,
                              const TCHAR *Catalog, const TCHAR *Schema, C_STRING &TableRef);
      
DllExport int  create(int CreateNewNodes = FALSE, int SubNewNodes = 0,
                      bool Interactive = true);
          int  EraseIsolatedNodes(C_SELSET &SelSet, int CounterToVideo = GS_BAD);
DllExport int  EraseIsolatedNodes(C_POINT_OBJS_BTREE &PtObjsBTree, int CounterToVideo = GS_BAD);
          int  remove(void);
DllExport int  reindex(void);
DllExport int  copy(TCHAR *NewTableRef);
      
DllExport presbuf elemadj(long elem_id, int adj_type);
DllExport presbuf LinksBetween(long id1, long id2);
DllExport long elemqty(int elem_type);

DllExport void InitForNetAnalysis();
DllExport int LoadInMemory(C_2STR_INT_LIST *pCostSQLList = NULL);

DllExport int GetNetPropagation(long iNode, double ActualCost = 0.0);
DllExport int GetShortestNetPath(long iNode, double ActualCost = 0.0);
DllExport int GetOptimizedNetPath(C_LONG_LIST &Nodes, bool Ordered);

DllExport int  editlink(int elem_sub, long elem_id, ads_name elem_ent);
DllExport int  editlink(int elem_sub, long elem_id, ads_name elem_ent, C_RB_LIST &AdjNodes);
DllExport int  editlink(int elem_sub, long elem_id, ads_name elem_ent,
                        int init_node_sub, long init_node_id,
                        int final_node_sub, long final_node_id);
DllExport int  editdelelem(long elem_id, int elem_type, ads_name elem_ent = NULL);

          int OLD2TEMP(void);
          int synchronize(int CounterToVideo = GS_BAD);
};

int gsc_getNodesInWindow(ads_point pt1, ads_point pt2, C_SELSET &SelSet, int CheckOn2D = GS_GOOD,
                         bool OnlyOnVisibleLayers = true);
DllExport int gsc_getNodesOnPt(ads_point point, C_SELSET &SelSet, int CheckOn2D = GS_GOOD,
                               C_SELSET *PunctualSS = NULL, bool OnlyOnVisibleLayers = true);
DllExport int gsc_get_PunctualSS(C_SELSET &PunctualSS, bool OnlyOnVisibleLayers = true);
DllExport int gsc_OverlapValidation(ads_point point, C_CLASS *p_class, const TCHAR *Handle = NULL);
DllExport int gsc_OverlapValidation(ads_point point, C_CLASS *p_class,
                                    C_POINT_OBJS_BTREE &PtObjsBTree, const TCHAR *Handle = NULL);

int gsc_get_AdjNode(C_SUB *pLinkSub, ads_point pt, C_SUB **pNodeSub, ads_name ent);
int gsc_insert_new_node(C_SUB *pSub, ads_point pt, ads_name entity, long *key_attrib,
                        C_RB_LIST &default_value, int IsDefCalc, _RecordsetPtr &pRsSub);

int gs_topocreate(void);


#endif