#ifndef MOAS_H_
#define MOAS_H_

//============================================================================================
//  MOAS - Mouse Orientiertes Adventure System
//  Copyright (C) 2000 bei Thomas Holz
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//--------------------------------------------------------------------------------------------
// Sie können mich erreichen unter thomas_holz@mail.com
// Bitte senden Sie mir KEINE:
//  - Werbung
//  - Anfragen wie denn bestimme Codesegmente funktionieren
//  - Anfragen wie man denn das Problem x lösen könnte
//--------------------------------------------------------------------------------------------
// Für die tysischen Fragen: Das Programm ist in C geschrieben. Heute würde ich den Profis
// C++ empfehlen. Für die Grafik (für Windows) ist DirektX für Profis gut. Anfänger werden
// daran verzweifeln. Da ist Java oder Visual Basic besser.
//--------------------------------------------------------------------------------------------
#define AsmQPalSet QuickSetPalette
#define CHKSUM uint32_t
#define USHORT unsigned short
#define uShort unsigned short
#define uLong unsigned long
#define UCHAR unsigned char
#define uChar unsigned char
#ifdef DOS
#define VGA_ADRESSE ((char*)0xa0000)
#else
/* Screen = clean BG; Display = composited frame (DOS VGA plane) */
#define VGA_ADRESSE ((char*)Display)
#endif
#define MODE  0
#define READ  1
#define WRITE 2
#define AnzVars Programm[0].x
#define US unsigned short
#define MAX_PARSE 50
#define MAX_PRS   20
#define DLG_ON     0
#define DLG_OFF    1
#define DLG_ALWAYS 2
#define DLG_EMPTY  3
#ifdef DOS
#define Clearscreen() memset((char*)0xA0000,0,64000)
#else
#define Clearscreen() do { if (Screen) memset(Screen,0,64000); } while(0)
#endif

#define V_Verb                    Var[0]
#define V_Par1                    Var[1]
#define V_Par2                    Var[2]
#define V_SaveState               Var[3]
#define V_Seconds                 Var[4]
#define V_Minutes                 Var[5]
#define V_Hours                   Var[6]
#define V_SecondsElapsed          Var[7]
#define V_Points                  Var[8]
#define V_TotalPoints             Var[9]
#define V_TimeSinceLastSave       Var[10]
#define V_PointsSiceLastSave      Var[11]
#define V_MinutesSinceLastPoint   Var[12]
#define V_Colored                 Var[13]
#define V_InventarState           Var[14]
#define V_LockVga                 Var[15]
#define V_NoWalk                  Var[16]
#define V_IsoInv                  Var[17]
#define V_Master                  Var[18]
#define V_PersonSize              Var[19]
#define V_CursorState             Var[20]
#define V_Keyboard                Var[21]
#define V_Play                    Var[22]
#define V_Volume                  Var[23]
#define V_TalkBaseY               Var[24]

#define SIEHE       4
#define BEWEGE      7
#define BENUTZE     9
#define SPRICH     10
#define M_SPEECH   50             /* Message: Speech */
#define M_WALK     51             /* Message: Walking */
#define M_STUPID   53             /* Message: 2 stupid 2 Walk */
#define C_WALK     52             /* Command: Walk! (ist activated when    */
#define ENTRY     999             /*   Player can't walk but is ordered to */

#ifdef DOS
#define OutpWait(Port,Value); { short c; for (c=0;c<10000;c++) if ((inp(0x22c)&0x80)==0) break; outp(Port,Value); }
#else
#define OutpWait(Port,Value) ((void)0)
#endif
#define Real(PrsNum)            GetRealPersonNum(PrsNum)

#include <signal.h>
//#include <conio.h>
#include <math.h>
#include <fcntl.h>
//#include <graph.h>
//#include <io.h>
//#include <i86.h>
//#include <dos.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "platform.h"
#include <unistd.h>
#include <stdint.h>

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Desktop: -fcommon merges these header definitions.
 * Emscripten: one TU defines (MOAS_DEFINE_GLOBALS), others see extern. */
#if defined(__EMSCRIPTEN__)
#  if defined(MOAS_DEFINE_GLOBALS)
#    define MOAS_G /* define storage */
#  else
#    define MOAS_G extern
#  endif
#else
#  define MOAS_G /* tentative common */
#endif


MOAS_G MOAS_G struct I_LINE
{
  short x1,y1,x2,y2,Dir,Side,Parent;
} iLine[600];

MOAS_G MOAS_G struct LAUFREIHE
{
  short x,y;
} LaufReihe[30];

MOAS_G uChar *Screen,*Display,*Info1,*Info2,AutoSaveGame;
MOAS_G uChar *PRedrawTmpBak,*VMouseBuf;  /*Virtual Mouse*/
MOAS_G uShort PaletteCycleFirst,PaletteCycleLast,DontCyclePal,CycleWait;

MOAS_G struct PERSON
{
  short Num;    /* Number On Disk [-1 = Unused] */
  short PhaseNum;
  long  xs,ys,Anz;
  unsigned char *Phase;
  short Sequence[8][12];   /* Direction * Phases */
  char  Counter,Dir,Boden,GhostLevel;
  short x,y,z;
  short TargetX,TargetY;
  short gtx,gty;          /* Generell Target for iWalk */
  char iWalk;             /* 0/1 = Off/On */
  char BaseSpeed,StepSpeed,PhaseSpeed; /* je 0..10 */
  USHORT Bits;            /* Siehe LANGUAGE.DOK */
} Person[MAX_PRS+1];  /*==>+<== Mallocen*/

MOAS_G struct FONT /*A..Zá- .!,:;'"0..9<>()[]a..z*/
{
  char SizeY;
  unsigned char *Data;
  short *Begin,*Width;
} Font[7];

MOAS_G struct MAPFILE
{
  char FileNum;
  long FilePos,Length;
  uChar *Data;     /* fr Programm[0] x=AnzVars */
  long x,y,Anz;    /* for Raum,Bild: Picsize; for Inventar x=ObjPos,y=ScrPos */
  char InUse;      /* Only for Phase */
  CHKSUM Check;
} *Programm,*Bilder,*Inventar,*Phase;

MOAS_G struct SAY_WINDOW
{
  short RefPrs;               /* -1 = Unused, sonst Eintrag in Properties */
  short x,y,xs,ys,x1,x2;
  char *Data,Font;
  char *Text;
  short Time;
  short wx,wy;                /* Additional Coordinates */
} SayWindow[5];

MOAS_G struct SAY_PROPERTIES
{
  short PrsNum;               /* -1 = Unused */
  unsigned char TxtColor;
  char StdFont;
} SayProperties[10];

MOAS_G struct MOUSE
{
  short x,y;                          /* Maus Position                        */
  short Left,Right,Middle;            /* Maus Knpfe   1=gedrckt             */
  short LastLeft,LastRight,LastMiddle;/* Mausstatus zuletzt                   */
  short ClickX,ClickY;                /* Position beim letzten drcken        */
  short ClickObject;                  /* Objekt beim letzten drcken          */
  short ReleaseX,ReleaseY;            /* Position beim letzten loslassen      */
  short CursorState;                  /* Cursor an, aus oder virtuell  (012)  */
  char BewegeMode;                    /* -1 = Nein, sonst CursorPhase         */
} Mouse;

MOAS_G short AnzProgramme,AnzBilder,AnzInventar,AnzPhasen;
MOAS_G short DayLightR,DayLightG,DayLightB;

MOAS_G struct MARKER
{
  short x1,y1,x2,y2;
  unsigned short Ref;
  enum MARKTYP { Label,Jumper,Unused } MarkTyp;
} Marker[50];

MOAS_G struct WALK_AREA
{
  short x1,y1,x2,y2,tx,ty;
} WalkArea[20];

MOAS_G struct
{
  short x1,y1,x2,y2,XPos,YPos;
  enum { Open,None,Enhanced } Status;
} Window;

MOAS_G struct
{
  short Num;
  enum PICTYP {Normal,InfoTyp1,InfoTyp2} Typ;
} PicturesOnScreen[10];

MOAS_G struct ONTIME
{
  unsigned short TimeLeft;
  unsigned short Proc,Adresse;
} Ontimes[10];

MOAS_G struct
{
  short Number;
  char *Name;
} Description[500];

MOAS_G short AnzDescription,TimeCounter;
MOAS_G short *Var;
MOAS_G char GhostMode,ThingAtCursor;
MOAS_G short CurrentProc,TotalSpeed,SpeechSpeed;
MOAS_G unsigned short Tab320[200];
MOAS_G char PlayerHasControl,WalkingAllowed,Eingeblendet;

MOAS_G char KeyAbortState,SpaceState;  /* 0=Off 1=Waiting 2=Abort! */
MOAS_G unsigned char KeyScanCode,LastKeyScanCode;  // aktueller Scancode des Keyboards
MOAS_G short anz_codes;
MOAS_G char *codes[257];
MOAS_G unsigned char *Palette;    /* Die Palette des aktuellen Bildes */
MOAS_G unsigned char *Paletten;   /* 16 umsortierte Paletten fr schnelle Effekte */
MOAS_G unsigned char *MegaTable;
MOAS_G char *SmallTextLine;
MOAS_G short SmallTextLineLength;

MOAS_G struct DIALOG
{
  short ReferenzNummer;  /* -1 = Leer */
  uShort TextProc;
  struct INHALT
  {
    unsigned short TextAdd;
    short RefNum;
    char Typ,StdTyp;
    short Querverweis;
  } Inhalt[30];
} Dialog[20];

/*==>+<== Rename 2 DlgWinRef */
MOAS_G struct REF_TABLE         /* Reference Table for the Dialog Window */
{
  char Num;              /* Num : Number of the Dialog Option */
  unsigned char y1,y2;   /* y1  : Y-Beginning of the Option in the Window */
} RefTable[12];          /* y1  : Y-Ending of the Option in the Window */

MOAS_G struct POWER_PARSE
{
  short Nummer;          /* -1 = Leer */
  uChar *String;         /* undefined wenn leer */
} PowerParse[MAX_PARSE];

MOAS_G char Packing;
MOAS_G char RoomName[40];

/*************************************************/
/** DigiSound - Daten ** Anfang ******************/
/*************************************************/

  MOAS_G uChar DMA_ADRESS_Maske;
  MOAS_G uChar DMA_ADRESS_LoescheFF;
  MOAS_G uChar DMA_ADRESS_Modus;
  MOAS_G uChar DMA_ADRESS_Adresse;
  MOAS_G uChar DMA_ADRESS_Seite;
  MOAS_G uChar DMA_ADRESS_Zaehler;
  MOAS_G uChar ExitCommunicator;  // 0=Nothing 1=DMA, ok to exit ?  2=DMA says ok

  MOAS_G short VolumeTab[32*256];
  MOAS_G long StepTable[4*16];
  MOAS_G short CurrentLine,SongBegin,SongEnd,ForNextValue,ForNextPosition;
  MOAS_G uChar *SampleBuffer0,*SampleBuffer1,*SampleBuffer2;
  MOAS_G short *OverdriveBuffer;

  MOAS_G uShort SampleSize;
  MOAS_G uChar DefVol[10],DefIns[10],DefNote[10];
  MOAS_G uLong DefInsPos[10];
  MOAS_G struct SONG
  {
    uShort Col[8];   /* iiii:ivvv::vvoo:nnnn -> Instrument, Oktave,Volume, Note */
  } Song[8*1024];

  MOAS_G struct SAMPLE
  {
    uChar Name[40];
    long Rate;
    long Size;
    uChar *Data;
  } Sample[64];

  MOAS_G struct CONFIG
  {
    short DmaNum,IrqNum,BaseNum;
  } Config;

  MOAS_G uChar SoftIRQ[16];
  MOAS_G uLong SpeechSize;                                        // Speech stuff
  MOAS_G uChar *SpeechData;
  MOAS_G uShort SpeechVolume,GlobalVolume;                        // [0..256]
  MOAS_G uChar OverdriveRemapper[5130];

#ifdef DOS
  void __interrupt __far MySoundIrq(void);
#endif
  void SoundMixer(const short c);
  void ApplyOverdrive(void);

/*************************************************/
/** DigiSound - Daten ** Ende ********************/
/*************************************************/

/* MOAS.C              INTERPRETER          ûû */
/* IO.C                DISK-ROUTINEN        ûû */
/* PRINT.C             PRINTROUTINEN        ûû */
/* GRAFIX.C            GRAFIKROUTINEN       û- */
/* IF.C                IF-ABFRAGEN          û- */
/* MISC.C              SONSTIGES            ûû */
/* INIT.C              INITIALISIERROUTINEN û- */
/* CHK.C               CHECKSUMMENROUTINEN  û- */
/* MOUSE.C             MAUSROUTINEN         û- */
/* HERCBAK.C           HERCULESROUTINEN     û- */


CHKSUM GetChk(void);
CHKSUM MkChk(unsigned char *Array,short Length);
char *DescriptionText(short Num);
char *MALLOC(long Size);
char *UnPack(unsigned char *string);

US IfPersonIsIn(unsigned short Adresse,unsigned short JumpAdresse,short Person,short x1,short y1,short x2, short y2);
US IfWasntHere(unsigned short Adresse,unsigned short VarNum,unsigned short JumpAdresse);
US ParseIf(unsigned short Adresse,unsigned short JumpAdresse,short Variable,short z1,short z2,short z3,short z4);
short Boden(short c);
short CharNum(unsigned char Char);
short ChkOpen(char *FileName,char Mode);
short DataOpen(short Num);
short Execute(short ProcNum,unsigned short Adresse);
short GetCursorPosInWindow(void);
short GetPlayerPersonNumber(void);
short GetRealPersonNum(short Number);
short IsCrossed(struct I_LINE a,struct I_LINE b);
short IsDirectWalkPossible(short PrsNum,short sx,short sy,short tx,short ty);
short IsMousePresent(void);
short IsPersonPositionOk(short PrsNum);
short IsPositionOk(short PrsNum,short x,short y);
short IsWalkPossible(short PrsNum,short px,short py,short tx,short ty);
short MoasIf(short Zahl1,short Zahl2,short Mode,short Operator);
short MoasIf2(short Zahl1,short Zahl2,short Mode,short Operator,short MainMode,short Zahl3,short Zahl4,short Operator2);
short MoasIf3(short Zahl1,short Zahl2,short Mode,short Operator,short MainMode,short Zahl3,short Zahl4,short Operator2,short MainMode2,short Zahl5,short Zahl6,short Operator3);
short PowerSelectFromWindow(void);
short PreUnpack(char *Data,short *CodesUsed);
short RealStringLength(char *String,char FontNum);
short SelectFromWindow(void);
short ShiftCharLeft(char c1,char c2,short FontNum);
short WhatsAt(short x,short y);
#ifdef DOS
void (interrupt far *RealIrq1c)();
void (interrupt far *RealIrq9)();
void (interrupt far *RealSoundIrq)();
#endif
void *D32DosMemAlloc(long size);
void AddArea(short x1,short y1,short x2,short y2,unsigned short Ref,enum MARKTYP MarkTyp);
void AddDescription(short Num,char *Text);
void AddDialog(short RefNum);
void AddOntimeJump(unsigned short Seconds,unsigned short Proc,unsigned short Adresse);
void AddOption(short DlgName,short RefNum,short State,short Querverweis,short Proc,short Adresse);
void AddPerson(short Prs,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask);
void AddPictureToTab(short Num,enum PICTYP Typ);
void AddPowerParse(short Nummer,char *String);
void AddSayWindow(short PrsNum,short wx,short wy,char *Text);
void Adress(short Prs,short Ref);
void AreaChk(unsigned char *Array,long Length);
void AsmQPalSet(char *Palette);
void AsmSinglePalSet(char r,char g,char b);
void Blend(char Step,char Dir);
void Blend2(short r,short g,short b,short Speed);
void CheapOverblend(void);
void CheckFile(unsigned char a,unsigned char b,unsigned char c,unsigned char d);
void CheckKopie(short PrsNum);
void ChkAuthorisation(void);
void ChkClose(short xfil);
void ChkRead(short xfil,char *Target,long Length);
void ChkWrite(short xfil,char *Target,long Length);
void CloseSay(short Window);
void CloseWindow(void);
void ClrChk(void);
void CodeText(short Code,char *Text);
void ConditionalWalk(short PrsNum,char Mode,short x1,short y1,short x2,short y2,short tx,short ty);
void CoreMakeMegaTable(unsigned char *Table3D);
void DisplayBox(unsigned char *Data,short x,short y,short xs,short ys);
void DisplayMessage(char *Message,short Color);
void DisplayMouseText(short Color);
void ExecuteTheProgram(void);
void FREE(char *p);
void FileMenu(void);
void FreeSomeMemory(void);
void GetClickOrEnter(short Time);
void Init320Tab(void);
void InitAll(void);
void InitBreakVector(void);
void InitDialogs(void);
void InitDigi(void);
void InitFont(void);
void InitMapData(void);
void InitMarker(void);
void InitOntimes(void);
void InitPalette(void);
void InitPersons(void);
void InitPictureData(void);
void InitScreenData(void);
void InitSystemVars(void);
void InitVars(void);
void InternalError(short num);
void IveGotTooMuchTime(void);
void KompliziereLaufReihe(void);
void LadeBereich(char FileNum,long FilePos,long Length,char *Data,CHKSUM CheckSum);
void LadeBild(short Num);
void LadeInfoBild(short Num,enum PICTYP Typ);
void LadeInventar(short Num);
void LadePhase(short Num);
void LadeProgramm(short Num);
void LargePrint(char *Message,char *Text,short TxtColor,short VgaXs);
void LetUserUseIcons(void);
void Line(short Col,short x1,short y1,short x2,short y2);
void LoadGame(void);
void LoadMusic(short Num);
void LoadPack(void);
void LoescheKopie(short PrsNum);
void LookKeys(void);
void MacheLaufReihe(short Side0,short Side1,short sx,short sy);
void MakeMegaTable(short Percent);
void MixPics(void);
void MouseCheck(void);
void MouseRead(void);
void NeueZeile(void);
void NextPersonPhase(short PrsNum);
void OpenWindow(char x1,char y,char x2);
void OverBlend(short BlendMode);
void PowerBlend(short Mode);
void PowerRedrawArea(short x1,short y1,short x2,short y2);
void PrintCharacter(char Char);
void PrintSentence(char *Text);
void PrintSilbe(unsigned char *Text,char *Next);
void PrintVar(short VarNum);
void PrintWord(char *Text);
void PutCharAt(short CharNum,char *Vga,char FontNum,char c1,char c2,short VgaXs);
void QuickSetPalette(char *Palette);
void QuickSetSinglePal(short Color,short r,short g,short b);
void ReTextSayWin(short WinNum);
void ReadLicence(void);
void Rectangle3D(unsigned char *Vga,long x1,long y1,long x2,long y2,uChar c1,uChar c2,long VgaXs);
void RedrawAllPersons(void);
void RedrawCompleteScreen(void);
void RedrawPerson(short c);
void RemoveAllPersons(void);
void RemoveDescriptions(void);
void RemoveDialog(short RefNum);
void RemovePerson(short Num);
void RemoveStatists(void);
void ResetDialog(short RefNum);
void Restart(void);
void SaveGame(short CallerID);
void SayWait(short Win);
void SelectFromDialog(short VarNum,short DlgNum,char Respond);
void SetMousePos(short x,short y);
void SetObject(short ObjNum,short Pos);
void SetPerson(short Prs,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask);
void SetPersonProperties(short c,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask);
void SetVar(short VarNum,short Value);
void Shake(short Milliseconds,short Staerke);
void ShowHercBak(void);
void ShowMarker(void);
void SingleChk(unsigned char);
void SmallPrint(char *Message,char *Text,short TxtColor,short VgaXs);
void SmartOverblend(short BlendMode);
void SortPersons(void);
void StartSoundProcessing(void);
void StopAllWalking(void);
void StopPerson(short PrsNum);
void TeakEffekt(void);
void TestIfPersonIsInTrap(void);
void ToggleDialogPoint(short DialogNum,short RefNum,short Mode);
void ToggleScreen(short Mode);
void TurnInventar(short State);
void UnMarkArea(enum MARKTYP MarkTyp,short Ref);
void UniversalUnpack(UCHAR *Source,UCHAR *Target,long Size,UCHAR Glass,long xs);
void UnpackPicture(UCHAR *Source,short y,USHORT Size);
void UpDateOntimes(void);
void UpDatePersonMemory(void);
void UpDateSayWindows(void);
void UpDateWalking(void);
void VMouse(short Type);
void VereinfacheLaufReihe(short PrsNum);
void Wait(short Milliseconds);
void WaitWhileSomeBodyIsWalking(short PersonNum);
void WalkClick(short x,short y);
void WalkPerson(short PrsNum,short tx,short ty);
void WillBeCalledEverySecond(void);
void WinEnhance(void);
#ifdef DOS
void __interrupt __far KeyboardIrq(void);
#endif
void iWalk(short PrsNum,short tx,short ty);
#ifdef DOS
void interrupt MySoundIrq(void);
void interrupt MyTickerIrq(void);
#endif
short GetTicker(void);

void MrFree(char *p,char *File,short Line);                         ///
char *MrMalloc(long Size,char *File,short Line);                    ///
                                                                    ///
#define MALLOC(Size) MrMalloc(Size,__FILE__,__LINE__)               ///
#define FREE(Size) MrFree(Size,__FILE__,__LINE__)                   ///
MOAS_G short MallocFile;                                                   ///


#endif /* MOAS_H_ */
