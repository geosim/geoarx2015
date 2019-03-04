/**********************************************************/
/*   GS_ERROR.H                                           */
/**********************************************************/

#ifndef _gs_error_h
#define _gs_error_h 1

#ifndef _gs_list_h
   #include "gs_list.h"
#endif

DllExport extern int GS_ERR_COD;

DllExport extern TCHAR *gsc_msg(int code);
DllExport extern TCHAR *gsc_err(int code);

DllExport int gsc_print_error(void);

DllExport int get_GS_ERR_COD();
DllExport void set_GS_ERR_COD(int err);

presbuf gsc_get_GEOdiag(void);
int gsc_dderroradmin(const TCHAR *ErrType, const TCHAR *OptionalDescr = NULL);
int gsc_dderroradmin(C_RB_LIST &ErrorList, const TCHAR *OptionalDescr = NULL);
void gsc_PrintError(C_RB_LIST &ErrorList);


/**********************************************************/
/*   ERROR_CODES                                          */
/**********************************************************/

   // GENERICI
   #define eGSNoError                       0
   #define eGSOpenFile         	           1
   #define eGSReadFile                      5
   #define eGSWriteFile                     3
   #define eGSCloseFile                     4
   #define eGSInvalidPath                   2
   #define eGSNotDel                        6
   #define eGSVarNotDef                     7
   #define eGSOutOfMem                      8
   #define eGSPathNoExist                   9
   #define eGSPathAlreadyExisting         143
   #define eGSNotRen                       69
   #define eGSNotCopied                   150
   #define eGSFileNoExisting              181
   #define eGSFileLocked                  194
   #define eGSUnknown                      10
   #define eGSDDEFailed                    12
   #define eGSNoFileAssoc                  21
   #define eGSNoValidListSep              221

   // DATABASE
   #define eGSUDLFileNotFound              70
   #define eGSUDLPropertyNotFound          11
   #define eGSConnect                      13
   #define eGSDisconnect                   14
   #define eGSConstrStm                    15
   #define eGSPrepareStm                   16
   #define eGSConstrCsr                    17
   #define eGSInitPar                      19
   #define eGSExe                          20
   #define eGSExeNotSupported              22
   #define eGSTermCsr                      24
   #define eGSReadStruct                   25
   #define eGSReadRow                      26
   #define eGSSkip                         30
   #define eGSCurRow                       31
   #define eGSDelRow                       32
   #define eGSUpdRow                       33
   #define eGSExportFrm                    34
   #define eGSExportStruct                 35
   #define eGSCopyRows                     36
   #define eGSAddRow                       37
   #define eGSUnkwnDBMS                    38
   #define eGSUnkwnSQLFieldType            39
   #define eGSUnkwnDrvFieldType           189
   #define eGSNoCompatibFields            201
   #define eGSUpdStruct                    41
   #define eGSBadScript                    42
   #define eGSIntConstr                    43
   #define eGSConstrNativeStm              71
   #define eGSLocked                      130
   #define eGSErrEofFound                 164
   #define eGSCreateTable                 216
   #define eGSCreateIndex                 213
   #define eGSTableNotDel                 214
   #define eGSIndexNotDel                  47  
   #define eGSOpenSchema                  215
   #define eGSTableNotExisting            217
   #define eGSTableAlreadyExisting        218
   #define eGSInvTabName                  112 
   							     
   #define eGSInvODTabName                 45
   #define eGSInvODFieldName              231

   #define eGSInvalidCatalog              210  
   #define eGSInvalidSchema               211  
   							                  
   // PROGETTI GEOSIM
   #define eGSInvalidPrjCode               48
   #define eGSInvalidPrjName               49
   #define eGSNotCurrentPrj               107
   #define eGSPrjInUse                    126
   #define eGSPrjNotEmpty                 219
           					                  
   #define eGSListError                    50
   #define eGSInvRBType                    51
   							                  
   #define eGSInvalidOpCod                 52
   #define eGSInvalidLogin                 53
   #define eGSInvalidPwd                   54
   #define eGSOpNotAble                    55
   #define eGSInvalidUser                  56
   #define eGSErrGeopwd                    57
   #define eGSInvAdeUser                  193 
							                     
   #define eGSSezNotFound                  58
   #define eGSEntryNotFound                59
   #define eGSBuffNotSuf                   60
							                     
   #define eGSInvalidArg                   61

   // CLASSI GEOSIM
   #define eGSClassNotFound                62
   #define eGSInvClassCode                 63
   #define eGSInvClassName                 64
   #define eGSInvClassDescr               236
   #define eGSInvClassType                 65
   #define eGSInvalidClassCopy            179
   #define eGSClassInUse                  127
   #define eGSInvCategoryType              72
   #define eGSInvalidLevel                 66
   #define eGSInvalidConct                 83
   #define eGSInvalidGroup                 85
   #define eGSInvalidGrid                  86
   #define eGSInvalidSubClassList         162
   #define eGSGraphResourceAlreadyExisting 224
   #define eGSInvVersion                  237

   // CARATTERISTICHE GRAFICHE							                     
   #define eGSInvalidColor                 87
   #define eGSInvalidHatch                 88
   #define eGSInvalidLineType              89
   #define eGSInvalidTextStyle             90
   #define eGSInvalidLayer                 91
   #define eGSInvalidBlock                 92
   #define eGSInvalidWidth                 93
   #define eGSInvalidElevation            173
   #define eGSInvalidScale                 94
   #define eGSInvalidRotation             174
   #define eGSInvalidH_text                95
   #define eGSInvalidThickness             96
   #define eGSInvalidDir_dwg              172
   #define eGSInvalidVis_attrib            97
   #define eGSInvalidDynamSegment_attrib  225
   #define eGSInvalidCoord                 98
   #define eGSInvalidRefBlock             175
   #define eGSInvalidGraphCalc            170
   #define eGSInvalidDimStyle             223
   #define eGSInvalidKey                   99
   
   // ATTRIBUTI
   #define eGSInvalidCalc                 100
   #define eGSInvalidCalcOfCalc           204
   #define eGSInvalidValid                101
   #define eGSObbligFound                 183                                         
   #define eGSInvalidDef_attrib           103
   #define eGSMissingDefValue              27
   #define eGSInvAttribName                73
   #define eGSInvAttribType               139
   #define eGSInvAttribLen                140
   #define eGSInvAttribDec                141
   #define eGSInvAttribValue              142
   #define eGSInvAttribCaption             84
                                          
   #define eGSStringTooLong                67
   #define eGSNotAllocVar                  68

   #define eGSThmNotFound                 222

   // TOPOLOGIA
   #define eGSOverlapValidation            29
   #define eGSNoLinkToConnect             118
   #define eGSNoNodeToConnect              23
                                          
   // GEOLISP
   #define eGSInvLispBracket               74
   #define eGSInvLispArg                   75
   #define eGSInvLispVar                   76
   #define eGSInvLispFunc                 102
   #define eGSLispStreamErr                77
   #define eGSInvLispStmnt                 78
   #define eGSInvLispString                79
   #define eGSInvLispVarType               80
   #define eGSLispStrExpected              81
   #define eGSLispNumExpected              82
   #define eGSLispSelect                  121
   #define eGSLisDividebyZero             205
                                       
   #define eGSInvEntityOp                 104
   #define eGSAdsCommandErr               105
   #define eGSAbortDCL                    106
   #define eGSInvalidApp                  108
   #define eGSInvalidEED                  109
   
   // TABELLE SECONDARIE
   #define eGSInvSecCode                  110
   #define eGSInvSecName                  229
   #define eGSInvSecDescr                 230
   #define eGSInvSecType                  111
   #define eGSSecInUse                    128
   #define eGSNoSecondary                 160 
   #define eGSInvLinkSec                   44
   #define eGSSecIsReadOnly               238 
   #define eGSSecLocked                   239

   // RETE
   #define eGSInvHostName                 113
   #define eGSInvOpSys                    114
   #define eGSAliasAlreadyExisting        136
   #define eGSInvalidHost                 137
			
   // SESSIONI DI LAVORO
   #define eGSInvSessionName              115
   #define eGSInvSessionType              116
   #define eGSInvSessionStatus            117
   #define eGSInvSessionCode              119
	#define eGSSessionsFound               129
	#define eGSLockBySession               186
	#define eGSOpNotAbleInFrozenSession    191
	#define eGSReadOnlySession             220
   #define eGSNotCurrentSession           124
   #define eGSSessionInUse                228
						                        
   #define eGSUndoErr                     120
                                          
   #define eGSInvGraphObjct               122
   #define eGSNotClassExtr                198
                                          
   #define eGSDuplComp     	 			   125
                        
   // ASE
   #define eGSInitASI                     123
   #define eGSInitASE                     131
   #define eGSInvalidLPN                  132
   #define eGSInvalidLS                   133
   #define eGSInvalidLink                 134
   #define eGSLinkAlreadyExisting         135
   #define eGSLPNConnectASE               28
      
   #define eGSInvalidAdeIni               138
   #define eGSInvalidMapVar               212

   #define eGSDwgCannotActivate           144
   #define eGSDwgCannotAttach             145
   #define eGSDwgCannotDeactivate         158
   #define eGSDwgCannotDetach             159
   #define eGSCannotLoadDwgList           146
   #define eGSCannotSelectIntoDwgList     147
   #define eGSCannotSaveEntIntoDwg        148
   #define eGSCannotResizeDwgList         149
   #define eGSCannotExtract               190
                                          
   #define eGSObjectIsLockedByAnotherUser 151
   #define eGSClassIsReadOnly             18
   #define eGSClassLocked                 188
   #define eGSPartialEntExtract           178
   #define eGSReferenceToTemporaryEnts    202
   #define eGSMemberClassNotAlreadyExtracted 235

   #define eGSInvalidDateType             152
   #define eGSInvalidDateYear             153
   #define eGSInvalidDateMonth            154
   #define eGSInvalidDateDay              155

   #define eGSInvalidTimeType             234

   #define eGSCannotLoadSpatialQuery      156
   #define eGSQryCondNotDef               192
   #define eGSInvalidLogicType            157
   #define eGSErrorExecutingCommand       161
   #define eGSUnableToReadUserDbDriver    163
   #define eGSInvalidSqlStatement         164
   #define eGSCannotLoadValues            165
   #define eGSCannotOpenCreateFileDeleted 166
   #define eGSInvalidSelectionSet         167
   #define eGSCannotLoadSystemVariable    168
   #define eGSUnableToCopyEntity          169
   #define eGSInvalidCommand              171
   #define eGSInvalidLockObject           176
   #define eGSInvalidCsr                  177
   #define eGSLayerLocked                 196
   #define eGSMemberOfGroup               180
   #define eGSStringNull                  182
                                          
   #define eGSPaperSpaceFound             184
   #define eGSObjectIsFree                185
   #define eGSInvUsrCmd                   187
   #define eGSUnknowAutoCadLanguage       195

   #define eGSBadLocationQry              197
   #define eGSInvalidVisibSet             199
   #define eGSUnableLoadVisibSet          200

   #define eGSInvalidTopoType             203
   #define eGSNoCleanInit                 208
   #define eGSNoCleanStart                209
   
   #define eGSGEOsimObjFound              206
   #define eGSGEOsimObjNotFound           207

   // GESTIONE LICENZE
   #define eGSLicenseExceeded              46
   #define eGSNoLicense                    40

   #define eGSInvStartPt                  226
   #define eGSInvEndPt                    227

   // GESTIONE SET DI CLASSI
   #define eGSInvClsSetName               232
   #define eGSInvClsSetId                 233


/**********************************************************/
//  Last used code = 237
//  I seguenti codici errore sono disponibili:
// 
/**********************************************************/

#endif
