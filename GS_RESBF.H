/**********************************************************/
/*   GS_RESBF.H                                            */
/**********************************************************/


#ifndef _gs_resbf_h
#define _gs_resbf_h 1

DllExport presbuf gsc_copybuf      (presbuf sorc);
DllExport presbuf gsc_rblistcopy   (presbuf lista);
DllExport int 	   gsc_sostitutebuf (presbuf sorc, presbuf dest);
DllExport presbuf gsc_scorri       (presbuf lista);
          presbuf gsc_scorcopy     (presbuf, presbuf);
DllExport presbuf gsc_listcopy     (presbuf lista, presbuf *p = NULL);

DllExport int     gsc_length       (presbuf);

DllExport presbuf gsc_nth          (int, presbuf);

DllExport presbuf gsc_nthcopy      (int, presbuf);

DllExport presbuf gsc_car          (presbuf);
DllExport presbuf gsc_cdr          (presbuf);
DllExport int     gsc_equal        (presbuf a, presbuf b, int sensitive = TRUE);
          presbuf gsc_assoccopy    (presbuf, presbuf);
          presbuf gsc_assoccopy    (char *stringa, presbuf lista);
          presbuf gsc_assoccopy    (TCHAR *stringa, presbuf lista);

DllExport presbuf gsc_assoc        (presbuf Key, presbuf lista, int sensitive = TRUE);
DllExport presbuf gsc_assoc        (const char *stringa, presbuf lista, int sensitive = TRUE);
DllExport presbuf gsc_assoc        (const TCHAR *stringa, presbuf lista, int sensitive = TRUE);

DllExport presbuf gsc_CdrAssoc     (const TCHAR *stringa, presbuf lista, int sensitive = TRUE);

DllExport int     gsc_RbSubst      (presbuf p_rb, long new_value);
DllExport int     gsc_RbSubst      (presbuf p_rb, int new_value);
DllExport int     gsc_RbSubst      (presbuf p_rb, double new_value);
DllExport int     gsc_RbSubst      (presbuf p_rb, const char *new_value);
DllExport int     gsc_RbSubst      (presbuf p_rb, const TCHAR *new_value);
DllExport int     gsc_RbSubst      (presbuf p_rb, presbuf new_value);
DllExport int     gsc_RbSubstNIL   (presbuf p_rb);
DllExport int     gsc_RbSubstT     (presbuf p_rb);
DllExport int     gsc_RbSubstNONE  (presbuf p_rb);

DllExport int     gsc_CdrAssocSum  (char *stringa, presbuf lista, presbuf new_value);
DllExport int     gsc_CdrAssocSum  (TCHAR *stringa, presbuf lista, presbuf new_value);
DllExport int     gsc_CdrAssocSub  (char *stringa, presbuf lista, presbuf new_value);
DllExport int     gsc_CdrAssocSub  (TCHAR *stringa, presbuf lista, presbuf new_value);

          presbuf gsc_rbsearch     (presbuf indice, presbuf lista, int sensitive = TRUE);
DllExport presbuf gsc_rbsearch     (char *stringa, presbuf lista, int sensitive = TRUE);
DllExport presbuf gsc_rbsearch     (TCHAR *stringa, presbuf lista, int sensitive = TRUE);
DllExport presbuf gsc_rbsearch     (int type, presbuf lista);
          presbuf gsc_rbforlsp     (presbuf *rb);

DllExport presbuf gsc_EEDsearch    (char *str, presbuf lista);
DllExport presbuf gsc_EEDsearch    (TCHAR *str, presbuf lista);
DllExport presbuf gsc_lspforrb     (presbuf *rb);
DllExport presbuf gsc_str2rb       (const char *str);
DllExport presbuf gsc_str2rb       (const TCHAR *str);
DllExport presbuf gsc_str2rb       (C_STRING &str);
DllExport TCHAR   *gsc_rb2str      (presbuf rb);
DllExport TCHAR   *gsc_rb2str      (presbuf rb, int digits, int dec = -1);
DllExport int     gsc_rb2Int       (presbuf rb, int *n);
DllExport int     gsc_rb2Lng       (presbuf rb, long *n);
DllExport int     gsc_rb2Dbl       (presbuf rb, double *n);
DllExport int     gsc_rb2Pt        (presbuf rb, ads_point pt);
DllExport int     gsc_rb2Bool      (presbuf rb, bool *n);

int gsc_rbExeMathOp(presbuf rbVal, presbuf rbQty, const char *Op, bool Perc = FALSE);
int gsc_rbExeMathOp(presbuf rbVal, presbuf rbQty, const TCHAR *Op, bool Perc = FALSE);
         
          void    gsc_printlist     (const struct resbuf *a);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


extern presbuf GS_RESBUF;        // resbuf pubblico di uso generico


#endif
