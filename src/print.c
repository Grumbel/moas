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
#include "moas.h"
#include "platform.h"
#include "debug.h"

char _SPACE_STRING[]=" ";

unsigned char CharAvail[]=
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ\x8E\x99\x9A\xE1"
  "- .!?,:;'\"0123456789<>()[]"
  "abcdefghijklmnopqrstuvwxyz\x84\x94\x81";
/* CP850: A-Z, ÄÖÜß, punct, a-z, äöü — must stay single-byte for game data */
short CharAvailL=-1;

void OpenWindow(char x1,char y,char x2)
{
  short c;

  MoasDbg(MOAS_DBG_WINDOW, "OpenWindow(%d,%d,%d)", (int)x1, (int)y, (int)x2);
  DontCyclePal++;
  RefTable[0].Num=-1;
  if (Window.Status!=None) InternalError(1008);
  Mouse.CursorState=0;
  PowerRedrawArea(Mouse.x,Mouse.y,Mouse.x+44,Mouse.y+40);
  Window.Status=Open;

  Window.XPos=-1;
  Window.x1=((short)x1)*12+4;  /* Farben sind 250 bis 253 */
  Window.x2=((short)x2)*12+16;
  Window.YPos=Window.y1=Window.y2=((short)y)*7+5;

  Line(252,Window.x1-1,Window.y1-5,Window.x2,Window.y1-5); /*252=Hell*/
  Line(252,Window.x2+1,Window.y1-4,Window.x2+2,Window.y1-4);
  Line(252,Window.x2+2,Window.y1-3,Window.x2+2,Window.y1-3);
  Line(252,Window.x1-4,Window.y1-2,Window.x1-4,Window.y1);
  Line(252,Window.x1-3,Window.y1-4,Window.x1-3,Window.y1-3);
  Line(252,Window.x1-3,Window.y1-4,Window.x1-2,Window.y1-4);

  for (c=0;c<3;c++)
    Line(251,Window.x1-1-c,Window.y1-4+c,Window.x2+c,Window.y1-4+c);
  Line(251,Window.x1-3,Window.y1-1,Window.x2+3,Window.y1-1);
  Line(251,Window.x1-3,Window.y1,Window.x2+3,Window.y1);

  Line(250,Window.x1+1,Window.y1-2,Window.x2-2,Window.y1-2);
  Line(250,Window.x1,Window.y1-1,Window.x1,Window.y1-1);
  Line(250,Window.x1+1,Window.y1,Window.x1+1,Window.y1);
  Line(250,Window.x2-1,Window.y1-1,Window.x2-1,Window.y1-1);
}
void Line(short Col,short x1,short y1,short x2,short y2)
{
  unsigned short c,r;
  unsigned char *Vga=VGA_ADRESSE+x1+((unsigned short)y1)*320;

  if (x1>x2 || y1>y2) InternalError(-77);
  r=rand();
  if (x1==x2)
    for (c=y1;c<=y2;c++,Vga+=320)
    {
      if ((c&7)==0) r=rand();
      *Vga=Col-(r&4);
      r>>=1;
    }
  else
    for (c=x1;c<=x2;c++,Vga++)
    {
      if ((c&7)==0) r=rand();
      *Vga=Col-(r&4);
      r>>=1;
    }
}
void CloseWindow(void)
{
  MoasDbg(MOAS_DBG_WINDOW, "CloseWindow status=%d", (int)Window.Status);
  VMouse(3);
  if (Window.Status==None) return;
  DontCyclePal--;
  Window.Status=None;
  Mouse.CursorState=1;
  PowerRedrawArea(Window.x1-4,Window.y1-5,Window.x2+3,Window.y2+3);
}
void PrintSentence(char *Text)
{
  unsigned char Buffer[255],*Word=NULL;
  char ThereWasAlreadyWord=0;
  char AnzPointer=0;

  if (KeyAbortState==2) return;

  if (Window.Status!=Open) OpenWindow(1,1,24);
  if (Window.XPos!=-1)
    if (RealStringLength(_SPACE_STRING,1)>(Window.x2-Window.XPos))
      NeueZeile(); else if (Window.XPos!=Window.x1) PrintCharacter(32);

  strncpy((char *)Buffer, Text ? Text : "", sizeof(Buffer)-1);
  Buffer[sizeof(Buffer)-1] = 0;
  while (1)
  {
    if ((Word=strtok(AnzPointer ? NULL : Buffer,_SPACE_STRING))==NULL) break;
    AnzPointer++;
    if (strcmp(Word,"NW")==0) { NeueZeile(); ThereWasAlreadyWord=0; continue; }
    if (ThereWasAlreadyWord==1 && Window.XPos!=Window.x1)
      if (RealStringLength(_SPACE_STRING,1)>(Window.x2-Window.XPos)) NeueZeile();
      else PrintCharacter(' ');
    PrintWord(Word);   ThereWasAlreadyWord=1;
  }
}
void PrintWord(char *Text)
{
  short c,Length=RealStringLength(Text,1);

  if (Length>(Window.x2-Window.x1-1)) { fprintf(stderr,"PrintWord: text too long (%d > %d)\n",(int)Length,(int)(Window.x2-Window.x1-1)); return; }
  if (Window.XPos==-1 || Length>(Window.x2-Window.XPos-1)) NeueZeile();

  for (c=0;Text[c];c++)
  {
    if (Text[c]=='_') Text[c]=32;
    PrintCharacter(Text[c]);
  }
}
void PrintCharacter(char Char)
{
  short Coded=CharNum((unsigned char)Char);

  PutCharAt(Coded,((char*)VGA_ADRESSE)+Window.XPos+320*Window.YPos,1,253,251,320);
  Window.XPos+=Font[1].Width[Coded];
}
void NeueZeile(void)
{
  short c;

  if (Window.XPos!=-1) Window.YPos+=11;
  Window.y2+=11;

  for (c=0;c<=12;c++)
    Line(251,Window.x1,Window.YPos+c,Window.x2,Window.YPos+c);
/*  memset(VGA_ADRESSE+Window.x1+(c+Window.YPos)*320l,251,Window.x2-Window.x1+1);*/
  Line(252,Window.x1-4,Window.YPos-1,Window.x1-4,Window.YPos+11);
  Line(251,Window.x1-3,Window.YPos-1,Window.x1-3,Window.YPos+11);
  Line(251,Window.x1-2,Window.YPos-1,Window.x1-2,Window.YPos+11);
  Line(250,Window.x1-1,Window.YPos  ,Window.x1-1,Window.YPos+10);
  if (Window.YPos!=Window.y1)
  {
    Line(250,Window.x1-1,Window.YPos-1,Window.x1-1,Window.YPos-1);
    Line(251,Window.x1  ,Window.YPos-1,Window.x1  ,Window.YPos-1);

    Line(251,Window.x2-1,Window.YPos-1,Window.x2-1,Window.YPos-1);
    Line(252,Window.x2  ,Window.YPos-1,Window.x2  ,Window.YPos-1);
  }

  Line(252,Window.x2  ,Window.YPos  ,Window.x2  ,Window.YPos+9);
  Line(251,Window.x2+1,Window.YPos-1,Window.x2+1,Window.YPos+11);
  Line(251,Window.x2+2,Window.YPos-1,Window.x2+2,Window.YPos+11);
  Line(250,Window.x2+3,Window.YPos-2,Window.x2+3,Window.YPos+11);

  Line(252,Window.x1  ,Window.YPos+10,Window.x1  ,Window.YPos+10);
  Line(252,Window.x2-1,Window.YPos+10,Window.x2-1,Window.YPos+10);
  Line(252,Window.x1+1,Window.YPos+11,Window.x2-2,Window.YPos+11);
  Line(251,Window.x1-1,Window.YPos+10,Window.x1-1,Window.YPos+11);
  Line(251,Window.x1-3,Window.YPos+12,Window.x2+2,Window.YPos+12);
  Line(251,Window.x1-3,Window.YPos+13,Window.x2+2,Window.YPos+13);
  Line(250,Window.x1-1,Window.YPos+14,Window.x2,Window.YPos+14);
  Line(250,Window.x2+1,Window.YPos+13,Window.x2+2,Window.YPos+13);
  Line(250,Window.x2+2,Window.YPos+12,Window.x2+2,Window.YPos+12);
  Line(250,Window.x1-3,Window.YPos+13,Window.x1-2,Window.YPos+13);
  Line(250,Window.x1-3,Window.YPos+12,Window.x1-3,Window.YPos+12);

  Window.XPos=Window.x1;
  if (Window.YPos>184) InternalError(1016);
}
short CharNum(unsigned char Char)
{
  register short c;

  if (Char=='_' || Char=='@') Char=' ';

  if (CharAvailL==-1) CharAvailL=(short)strlen((const char *)CharAvail);
  for (c=0;c<CharAvailL;c++)
    if (Char==CharAvail[c]) return(c);

  /* Unknown glyph: do not abort the whole game (data may use rare bytes). */
  fprintf(stderr, "CharNum: unknown byte %u\n", (unsigned)Char);
  /* Prefer space if present, else first glyph. */
  for (c=0;c<CharAvailL;c++)
    if (CharAvail[c]==' ') return(c);
  return(0);
}
short RealStringLength(char *String,char FontNum)
{
  register short c,Length,l=strlen(String);

  if (String==NULL) return(-1);
  for (c=Length=0;c<l;c++)
  {
    Length+=Font[FontNum].Width[CharNum((unsigned char)String[c])];
    if (FontNum>1) Length-=ShiftCharLeft(String[c],String[c+1],FontNum);
  }
  return(Length);
}
void PrintVar(short VarNum)
{
  char Buffer[10];

  itoa(Var[VarNum],Buffer,10);
  PrintSentence(Buffer);
}
void DisplayMessage(char *Message,short Color)
{
  short c,Length;
  static char LastText[80];
  static short LastXs,LastColor;

  if (strcmp(Message,">???<")==0 && V_Master)
  {
    sprintf(Message,"Max free memory:%u",_memmax());
    Color=45;
  }
  if (strcmp(LastText,Message)==0 && Color==LastColor) return;
  LastColor=Color;
  strcpy(LastText,Message);
  do
  {
    Length=RealStringLength(LastText,0);
    if (Length>335) LastText[strlen(LastText)-1]=0;
  } while (Length>335);

  memset(SmallTextLine,0,3060);

  if (*LastText)
  {
    SmallTextLineLength=min(Length,320-1-4)+4;
    for (c=1;c<8;c++)
      memset(SmallTextLine+2+c*340,46,Length+2);
    Rectangle3D(SmallTextLine,1,0,Length+4,8,45,47,340);
    SmallTextLine[4+Length]=SmallTextLine[2721]=46;
    SmallPrint(LastText,SmallTextLine+679,Color,340);
  } else SmallTextLineLength=0;
  strcpy(LastText,Message);
  PowerRedrawArea(0,189,max(LastXs,Length)+5,198);
  LastXs=Length;
}
void SmallPrint(char *Message,char *Text,short TxtColor,short VgaXs)
{
  register short c,l,xp=3;

  l=strlen(Message);
  for (c=0;c<l;c++)
  {
    PutCharAt(CharNum((unsigned char)Message[c]),Text+xp,0,TxtColor,0,VgaXs);
    xp+=Font[0].Width[CharNum((unsigned char)Message[c])];
  }
}
void LargePrint(char *Message,char *Text,short TxtColor,short VgaXs)
{
  register short c,l,xp=3;

  l=strlen(Message);
  for (c=0;c<l;c++)
  {
    PutCharAt(CharNum((unsigned char)Message[c]),Text+xp,1,TxtColor,0,VgaXs);
    xp+=Font[1].Width[CharNum((unsigned char)Message[c])];
  }
}
void AddDialog(short RefNum)
{
  short c,d;

  for (c=0;c<20;c++) if (Dialog[c].ReferenzNummer==-1) break;
  if (c==20) InternalError(1037);
  Dialog[c].ReferenzNummer=RefNum;
  for (d=0;d<30;d++) Dialog[c].Inhalt[d].Typ=DLG_EMPTY;
}
void RemoveDialog(short RefNum)
{
  short c;

  for (c=0;c<20;c++)
    if (Dialog[c].ReferenzNummer==RefNum)
      Dialog[c].ReferenzNummer=-1;
  return;
}
void ResetDialog(short RefNum)
{
  short c,d;

  for (c=0;c<20;c++)
    if (Dialog[c].ReferenzNummer==RefNum)
    {
      for (d=0;d<30;d++)
        if (Dialog[c].Inhalt[d].Typ!=DLG_EMPTY) Dialog[c].Inhalt[d].Typ=Dialog[c].Inhalt[d].StdTyp;
      return;
    }
  InternalError(1039);
}
void AddOption(short DlgName,short RefNum,short State,short Querverweis,short Proc,short Adresse)
{
  short c,d;

  for (c=0;c<20;c++)
    if (Dialog[c].ReferenzNummer==DlgName)
    {
      for (d=0;d<30;d++) if (Dialog[c].Inhalt[d].Typ==DLG_EMPTY) break;
      if (d==30) InternalError(1041);
      switch(State)
      {
        case 1:State=DLG_ON; break;
        case 0:State=DLG_OFF; break;
        case 3:State=DLG_ALWAYS; break;
      }
      Dialog[c].TextProc=Proc;
      Dialog[c].Inhalt[d].Typ=Dialog[c].Inhalt[d].StdTyp=State;
      Dialog[c].Inhalt[d].Querverweis=Querverweis;
      Dialog[c].Inhalt[d].TextAdd=Adresse;
      Dialog[c].Inhalt[d].RefNum=RefNum;
      return;
    }
  InternalError(1040);
}
void ToggleDialogPoint(short DialogNum,short RefNum,short Mode)
{
  short c,d;

  for (c=0;c<20;c++)
    if (Dialog[c].ReferenzNummer==DialogNum)
    {
      for (d=0;d<30;d++)
        if (Dialog[c].Inhalt[d].RefNum==RefNum)
        {
          if (Mode!=5)
            if (Mode==1) Dialog[c].Inhalt[d].Typ=DLG_ON;
                    else Dialog[c].Inhalt[d].Typ=DLG_OFF;
          else if (Dialog[c].Inhalt[d].Typ==DLG_ON) Dialog[c].Inhalt[d].Typ=DLG_OFF;
            else if (Dialog[c].Inhalt[d].Typ==DLG_OFF) Dialog[c].Inhalt[d].Typ=DLG_ON;
        }
      return;
    }
  InternalError(1040);
}
void SelectFromDialog(short VarNum,short DlgNum,char Respond)
{
  char *Buffer;
  char AnzRefs=0;
  short c,d,e,Selection,qv;

  OpenWindow(0,V_TalkBaseY*26/200,25);  /* XL */

  for (c=0;c<20;c++) if (Dialog[c].ReferenzNummer==DlgNum) break;
  if (c==20) InternalError(1040);
  for (d=0;d<12;d++)
    RefTable[d].Num=-1;

  for (d=0;d<30;d++)
    if (Dialog[c].Inhalt[d].Typ==DLG_ON || Dialog[c].Inhalt[d].Typ==DLG_ALWAYS)
    {
      LadeProgramm(Dialog[c].TextProc);
      Buffer=UnPack(Programm[Dialog[c].TextProc].Data+Dialog[c].Inhalt[d].TextAdd);
      for (e=0;Buffer[e];e++)
        if (strncmp(Buffer+e," NW ",4)==0)
        {
          strcpy(Buffer+e,Buffer+e+3);
          e=-1;
        }
      RefTable[AnzRefs].Num=d;
      if (AnzRefs)
      {
        Window.YPos+=3;
        NeueZeile();
        Line(250,Window.x1-1,Window.YPos-2,Window.x2-1,Window.YPos-2);
        Line(252,Window.x1,Window.YPos-3,Window.x2-1,Window.YPos-3);
        Line(251,Window.x1,Window.YPos-4,Window.x2-1,Window.YPos-4);
        Line(251,Window.x1,Window.YPos+10,Window.x2-1,Window.YPos+10);
        Line(252,Window.x1-4,Window.YPos-2,Window.x1-4,Window.YPos-2);
        Line(251,Window.x1-3,Window.YPos-2,Window.x1-3,Window.YPos-2);
        Line(250,Window.x1-1,Window.YPos-4,Window.x1-1,Window.YPos-4);
        Line(252,Window.x2,Window.YPos-4,Window.x2,Window.YPos-3);
        Line(251,Window.x2+2,Window.YPos-2,Window.x2+2,Window.YPos-2);
        Window.y2+=3;
      }
      RefTable[AnzRefs].y1=Window.YPos;
      PrintSentence(Buffer);
      if (d!=49)
      {
        Buffer=UnPack(Programm[Dialog[c].TextProc].Data+Dialog[c].Inhalt[d+1].TextAdd);
        if (Buffer[0]=='+')
        {
          PrintSentence(Buffer+1);
          d++;
        }
      }
      RefTable[AnzRefs].y2=Window.YPos+11;   /* dependig on FontSize */
      AnzRefs++;
    }
  Line(251,Window.x1,Window.YPos+10,Window.x2-1,Window.YPos+10);
  Selection=RefTable[PowerSelectFromWindow()].Num;
  CloseWindow();        qv=Dialog[c].Inhalt[Selection].Querverweis;
  if (qv!=-1)
  {
    for (d=0;d<30;d++) if (Dialog[c].Inhalt[d].RefNum==qv) {qv=d; break; }
    if (Dialog[c].Inhalt[qv].Typ==DLG_ON) Dialog[c].Inhalt[qv].Typ=DLG_OFF;
      else if (Dialog[c].Inhalt[qv].Typ==DLG_OFF) Dialog[c].Inhalt[qv].Typ=DLG_ON;
  }
  if (Dialog[c].Inhalt[Selection].Typ==DLG_ON) Dialog[c].Inhalt[Selection].Typ=DLG_OFF;
    else if (Dialog[c].Inhalt[Selection].Typ==DLG_OFF) Dialog[c].Inhalt[Selection].Typ=DLG_ON;
  SetVar(VarNum,Dialog[c].Inhalt[Selection].RefNum);
  if (Respond)
  {
    AddSayWindow(-1,-1000,-1000,UnPack(Programm[Dialog[c].TextProc].Data+Dialog[c].Inhalt[Selection].TextAdd));
    SayWait(-1);
  }
}
short PowerSelectFromWindow(void)   /* RefTable is Global Parameter */
{
  long y=-99,LastY,cx,cy,y320;
  short has_refs = 0;
  short i;

  for (i = 0; i < 12; i++)
    if (RefTable[i].Num != -1) { has_refs = 1; break; }

  MoasDbg(MOAS_DBG_WAIT, "PowerSelectFromWindow has_refs=%d", (int)has_refs);
  WalkingAllowed=0;
  while (Mouse.Left | Mouse.Right) MouseCheck();
  MouseCheck();
  while (1)
  {
#ifndef DOS
    if (PlatformShouldQuit()) {
      WalkingAllowed = 1;
      return 0;
    }
#endif
    LastY=y;  y=GetCursorPosInWindow();
    if (LastY!=y)
    {
      VMouse(3);
      if (LastY!=-1)
        for (y320=(cy=RefTable[LastY].y1)*320;cy<RefTable[LastY].y2;cy++,y320+=320)
          for (cx=4;cx<315;cx++)
            if (((unsigned char *)VGA_ADRESSE)[cx+y320]==255)
              ((unsigned char *)VGA_ADRESSE)[cx+y320]=253;
      if (y!=-1)
        for (y320=(cy=RefTable[y].y1)*320;cy<RefTable[y].y2;cy++,y320+=320)
          for (cx=4;cx<315;cx++)
            if (((unsigned char *)VGA_ADRESSE)[cx+y320]==253)
              ((unsigned char *)VGA_ADRESSE)[cx+y320]=255;
      VMouse(1);
    }

    MouseCheck();

    if ((Mouse.Left | Mouse.Right)==0 && (Mouse.LastLeft | Mouse.LastRight))
    {
      short sel = GetCursorPosInWindow();
      if (sel != -1 || !has_refs)
      {
        WalkingAllowed=1;
        return (sel != -1) ? sel : 0;
      }
    }
    if (kbhit()) {
      int ch = getch();
      if (ch == 13 || ch == 10 || ch == 27) {
        WalkingAllowed = 1;
        return (y != -1 && y != -99) ? (short)y : 0;
      }
      if (ch == 0 && kbhit()) (void)getch();
    }
  }
}
void AddSayWindow(short PrsNum,short wx,short wy,char *Text)
{
  register short c,d;

  if (KeyAbortState==2) return;

  if (strncmp(Text,"CLR ",4)==0)
  {
    CloseSay(PrsNum);
    strcpy(Text,Text+4);
  }

  if (PrsNum==-1) PrsNum=Person[GetRealPersonNum(-1)].Num;
  for (c=0;c<5;c++)
    if (SayWindow[c].RefPrs==PrsNum) break;
  if (c==5)
    for (c=0;c<5;c++)
      if (SayWindow[c].RefPrs==-1) break;
  if (c==5) InternalError(1044);

  for (d=0;d<MAX_PRS;d++) if (Person[d].Num==PrsNum) break;

  if (SayWindow[c].RefPrs==-1)
  {
    SayWindow[c].wx=wx;
    SayWindow[c].wy=wy;

    for (d=0;d<10;d++)
      if (SayProperties[d].PrsNum==PrsNum) break;
    SayWindow[c].Font=SayProperties[d].StdFont;

    SayWindow[c].Text=MALLOC(strlen(Text)+1);

    SayWindow[c].Data=NULL;
    strcpy(SayWindow[c].Text,Text);
    SayWindow[c].RefPrs=PrsNum;
    ReTextSayWin(c);
  }
  else
  {
    char *Tmp;

    if (SayWindow[c].Text!=NULL)
    {
      Tmp=MALLOC(strlen(Text)+strlen(SayWindow[c].Text)+2); /* + space + NUL */
      strcpy(Tmp,SayWindow[c].Text);
      strcat(Tmp,_SPACE_STRING);
      FREE(SayWindow[c].Text);
    } else
    {
      Tmp=MALLOC(strlen(Text)+1);
      *Tmp=0;
    }
    strcat(Tmp,Text);
    SayWindow[c].Text=Tmp;
  }
}
void ReTextSayWin(short WinNum)
{
  register short c,d,tx,xp,yp=0,y,WinL;
  short RSL;
  char Buffer[202],LineL[20],AnzLines;
  short ox=SayWindow[WinNum].x,oy=SayWindow[WinNum].y,ox2=ox+SayWindow[WinNum].xs,oy2=oy+SayWindow[WinNum].ys;

  SayWindow[WinNum].Time=0;
  for (d=0;d<10;d++) if (SayProperties[d].PrsNum==SayWindow[WinNum].RefPrs) break;
  for (c=0;c<MAX_PRS;c++) if (Person[c].Num==SayWindow[WinNum].RefPrs) break;

  if (SayWindow[WinNum].wx<-500) SayWindow[WinNum].x=Person[c].x+Person[c].xs/2+SayWindow[WinNum].wx+1000;
    else SayWindow[WinNum].x=SayWindow[WinNum].wx;
  if (SayWindow[WinNum].wy<-500) SayWindow[WinNum].y=Person[c].y+SayWindow[WinNum].wy+1000;
    else SayWindow[WinNum].y=SayWindow[WinNum].wy;
  if (SayWindow[WinNum].x<70) SayWindow[WinNum].x=70;
  else if (SayWindow[WinNum].x>250) SayWindow[WinNum].x=250;
  if (SayWindow[WinNum].x<160) SayWindow[WinNum].xs=SayWindow[WinNum].x-2;
    else SayWindow[WinNum].xs=318-SayWindow[WinNum].x;
  SayWindow[WinNum].x-=SayWindow[WinNum].xs;
  SayWindow[WinNum].xs*=2;

  if (SayWindow[WinNum].Data) FREE(SayWindow[WinNum].Data);
  if (SayWindow[WinNum].Text[0]==0 || SayWindow[WinNum].Text==0)
  {
    if (SayWindow[WinNum].Text) FREE(SayWindow[WinNum].Text);
    SayWindow[WinNum].RefPrs=-1;
    PowerRedrawArea(ox,oy,ox2,oy2);
    return;
  }
  if (strncmp(SayWindow[WinNum].Text," NW ",4)==0)
    strcpy(SayWindow[WinNum].Text,SayWindow[WinNum].Text+4);

  while (SayWindow[WinNum].Text[0]==32) strcpy(SayWindow[WinNum].Text,SayWindow[WinNum].Text+1);
  strncpy(Buffer,SayWindow[WinNum].Text,200);  Buffer[199]=0;
  WinL=xp=strlen(Buffer);
  for (c=0;c<xp;c++)
    if (Buffer[c]==32 && Buffer[c+1]=='N' && Buffer[c+2]=='W' && Buffer[c+3]==32)
      { WinL=c; break; }

  for (AnzLines=tx=yp=0;tx<WinL;yp+=Font[SayWindow[WinNum].Font].SizeY,AnzLines++)
  {
    strncpy(Buffer,SayWindow[WinNum].Text+tx,200);  Buffer[199]=0;
    Buffer[WinL-tx]=0;

    RSL=RealStringLength(Buffer,SayWindow[WinNum].Font)+4;
    xp=strlen(Buffer)-1;
    while (RSL>SayWindow[WinNum].xs)
    {
      while (Buffer[xp]!=' ' && Buffer[xp]!=0 && *Buffer)
      {
        RSL-=Font[SayWindow[WinNum].Font].Width[CharNum((unsigned char)Buffer[xp])];
        RSL+=ShiftCharLeft(Buffer[xp-1],Buffer[xp],SayWindow[WinNum].Font);
        Buffer[xp--]=0;
      }
      if (Buffer[xp]==' ')
      {
        RSL-=Font[SayWindow[WinNum].Font].Width[CharNum((unsigned char)Buffer[xp])];
        RSL+=ShiftCharLeft(Buffer[xp-1],Buffer[xp],SayWindow[WinNum].Font);
        Buffer[xp--]=0;
      }
      if (*Buffer==0) InternalError(1046);
    }

    LineL[AnzLines]=xp+1;       tx+=xp+1;
    if (Font[SayWindow[WinNum].Font].SizeY==12) yp++;
  }

  SayWindow[WinNum].ys=yp;
  SayWindow[WinNum].Data=MALLOC(SayWindow[WinNum].xs*SayWindow[WinNum].ys);
  memset(SayWindow[WinNum].Data,0,SayWindow[WinNum].xs*SayWindow[WinNum].ys);

  SayWindow[WinNum].x1=SayWindow[WinNum].xs;
  SayWindow[WinNum].x2=0;
  for (yp=tx=y=0;y<AnzLines;y++)
  {
    strncpy(Buffer,SayWindow[WinNum].Text+tx,LineL[y]);
    Buffer[LineL[y]]=0;
    RSL=RealStringLength(Buffer,SayWindow[WinNum].Font)+4;
    xp=(SayWindow[WinNum].xs-RSL)/2;
    if (SayWindow[WinNum].x1>xp) SayWindow[WinNum].x1=xp;

    for (c=0;c<strlen(Buffer);c++)
    {
      PutCharAt(CharNum((unsigned char)Buffer[c]),SayWindow[WinNum].Data+xp+yp*SayWindow[WinNum].xs,SayWindow[WinNum].Font,
                SayProperties[d].TxtColor,1,SayWindow[WinNum].xs);
      xp+=Font[SayWindow[WinNum].Font].Width[CharNum((unsigned char)Buffer[c])];
      xp-=ShiftCharLeft(Buffer[c],Buffer[c+1],SayWindow[WinNum].Font);
      SayWindow[WinNum].Time+=15;
    }
    if (SayWindow[WinNum].x2<xp) SayWindow[WinNum].x2=xp;
    if (Font[SayWindow[WinNum].Font].SizeY==12) yp++;
    yp+=Font[SayWindow[WinNum].Font].SizeY;
    tx+=LineL[y];
  }
  for (y=0;y<AnzLines;y++)
    strcpy(SayWindow[WinNum].Text,SayWindow[WinNum].Text+LineL[y]);
  SayWindow[WinNum].y-=yp;
  if (SayWindow[WinNum].y<1) SayWindow[WinNum].y=1;

  for (c=0;c<MAX_PRS;c++) if (Person[c].Num==SayWindow[WinNum].RefPrs) break;
  if (SayWindow[WinNum].wx<-500)
  {
    SayWindow[WinNum].x=Person[c].x+Person[c].xs/2+SayWindow[WinNum].wx+1000;
    SayWindow[WinNum].x-=SayWindow[WinNum].xs/2;
    if (SayWindow[WinNum].x1+SayWindow[WinNum].x<2) SayWindow[WinNum].x=2-SayWindow[WinNum].x1;
    if (SayWindow[WinNum].x2+SayWindow[WinNum].x>318) SayWindow[WinNum].x=318-SayWindow[WinNum].x2;
  }

  PowerRedrawArea(min(ox,SayWindow[WinNum].x),min(oy,SayWindow[WinNum].y),max(SayWindow[WinNum].x+SayWindow[WinNum].xs,ox2),max(SayWindow[WinNum].y+SayWindow[WinNum].ys,oy2));

  if (SayWindow[WinNum].Text[0]==0 && c<MAX_PRS && Person[c].Bits&16)
  {
    short T0=Var[0],T1=Var[1],T2=Var[2];

    V_Verb=M_SPEECH;
    V_Par1=Person[c].Num;
    ExecuteTheProgram();

    Var[0]=T0; Var[1]=T1; Var[2]=T2;
  }
}
void UpDateSayWindows(void)
{
  static uChar LastExtra,LastMiddle;
#ifdef DOS
  uChar Extra=*((uChar*)0x417);
#else
  uChar Extra=0; /* Shift state: wire to SDL modifiers later if needed */
#endif
  short c,d;
  short ox,oy,ox2,oy2;

  for (c=0;c<5;c++)
    if (SayWindow[c].RefPrs!=-1)
    {
      for (d=0;d<MAX_PRS;d++) if (Person[d].Num==SayWindow[c].RefPrs) break;
      if (d!=MAX_PRS)
      {
        ox=SayWindow[c].x+SayWindow[c].x1;
        oy=SayWindow[c].y;
        ox2=SayWindow[c].x+SayWindow[c].x2;
        oy2=oy+SayWindow[c].ys;
        if (SayWindow[c].wy<-500) SayWindow[c].y=Person[d].y+SayWindow[c].wy+1000-SayWindow[c].ys;
        if (SayWindow[c].y<1) SayWindow[c].y=1;
        if (SayWindow[c].wx<-500)
        {
          SayWindow[c].x=Person[d].x+Person[d].xs/2+SayWindow[c].wx+1000;
          SayWindow[c].x-=SayWindow[c].xs/2;
          if (SayWindow[c].x1+SayWindow[c].x<2) SayWindow[c].x=2-SayWindow[c].x1;
          if (SayWindow[c].x2+SayWindow[c].x>318) SayWindow[c].x=318-SayWindow[c].x2;
          PowerRedrawArea(min(ox,SayWindow[c].x+SayWindow[c].x1),min(oy,SayWindow[c].y),max(SayWindow[c].x+SayWindow[c].x2,ox2),max(SayWindow[c].y+SayWindow[c].ys,oy2));
        }
      }
      if ((Extra & 3)==0 && Mouse.Middle==0)    /* Any Shift Key pressed ? */
      {
        SayWindow[c].Time-=((SpeechSpeed+1)>>1);
        if ((LastExtra & 3) || (LastMiddle)) SayWindow[c].Time=-1;
      }
      if (SayWindow[c].Time<0) ReTextSayWin(c);
    }
  LastExtra=Extra;
  LastMiddle=Mouse.Middle;
}
void Rectangle3D(unsigned char *Vga,long x1,long y1,long x2,long y2,uChar c1,uChar c2,long VgaXs)
{
  long c;

  y1*=VgaXs;
  y2*=VgaXs;
  for (c=x1;c<x2;c++)
  {
    Vga[c+y1]=c2;
    Vga[c+y2+1]=c1;
  }
  for (c=y1;c<y2;c+=VgaXs)
  {
    Vga[x1+c]=c2;
    Vga[x2+c+VgaXs]=c1;
  }
}
void SayWait(short Win)
{
  register short c,Ok=0;

  WinEnhance();
  if (Win==-1) Win=Person[GetRealPersonNum(-1)].Num;
  while (Ok==0 && KeyAbortState!=2)
  {
    MouseCheck();
    for (Ok=1,c=0;c<5;c++)
      if (SayWindow[c].RefPrs!=-1 && ((SayWindow[c].Text!=NULL && Win==SayWindow[c].RefPrs) || Win==32000))
        Ok=0;
  }
  CloseSay(Win);
}
short ShiftCharLeft(char c1,char c2,short FontNum)
{
  register short x,y,Shift=0,ys,BM1p,h1,h2;
  unsigned char *BM1,*BM2;

  if (FontNum==0) return(0);
  if (c1==32 || c1=='_' || c1=='@' || c1==0 || c2==32 || c2=='_' || c2=='@' || c2==0) return(0);

  ys=Font[FontNum].SizeY;
  BM1p=Font[FontNum].Width[CharNum(c1)]-1;
  BM1=Font[FontNum].Data+Font[FontNum].Begin[CharNum(c1)];
  BM2=Font[FontNum].Data+Font[FontNum].Begin[CharNum(c2)];

  while (Shift<30)
  {
    for (x=0;x<=Shift;x++,BM1+=3,BM2+=3)
    {
      h1=3*(BM1p-Shift);       /*Helpvar 1*/
      for (y=0;y<ys;y++)
      {
        h2=(192>>((y&3)*2));   /*Helpvar 2*/
        if ( (BM1[h1+(y>>2)] & h2) != (BM2[y>>2] & h2)
          && ((BM1[h1+(y>>2)] & h2) >> ((3-(y&3))*2)) != 3
          && ((BM2[y>>2] & h2) >> ((3-(y&3))*2)) != 3)      return(Shift);
        if (((BM1[h1+(y>>2)] & h2) >> ((3-(y&3))*2)) == 1 &&
                 ((BM2[y>>2] & h2) >> ((3-(y&3))*2)) == 1) return(Shift);
      }
    }
    Shift++;     BM1-=x+x+x; BM2-=x+x+x;
  }
  InternalError(1045);
  return(-1);
}
void CodeText(short Code,char *Text)
{
  short c,e,l;
  long d;
  char Buffer[10];

  l=strlen(Text);
  for (c=0;c<l;c++)
  {
    if (Text[c]>='a' && Text[c]<='z') Text[c]=Text[c]+'A'-'a';
    if (Text[c]!=32)
    {
      while (1)
      {
        d=e=rand()%400;
        d*=Code;
        while (d>=100) d/=10;
        if (d-11==Text[c]-'A') break;
      }
      if (V_Master) sprintf(Buffer,"%c%s",Text[c],(Text[c+1]==0 || Text[c+1]==32) ? " " : "");
      else sprintf(Buffer,"%i%c",e,(Text[c+1]==0 || Text[c+1]==32) ? 32 : ',');
      PrintWord(Buffer);
    }
  }
}
void WinEnhance(void)
{
  unsigned short x,y,y320;

  if (Window.Status==Open && Window.x2!=316)
  {
    for (x=0;x<300;x++)
    {
      for (y320=(y=Window.y1)*320;y<=Window.y2;y++,y320+=320)
        if (((VGA_ADRESSE)[Window.x2-x+y320-322]!=251) && ((VGA_ADRESSE)[Window.x2-x+y320-322]!=247)) break;
      if (y!=Window.y2+1) break;
    }
    for (y320=(y=Window.y1-4)*320;y<=Window.y2+4;y++,y320+=320)
      *((long*)(VGA_ADRESSE+Window.x2-x+y320-320))=
             *((long*)(VGA_ADRESSE+Window.x2+y320-320));
    PowerRedrawArea(Window.x2-x+4,Window.y1-5,Window.x2+3,Window.y2+3);
    PowerRedrawArea(Window.x2-x+1,Window.y1-5,Window.x2-x+2,Window.y1-5);
    PowerRedrawArea(Window.x2-x+3,Window.y1-5,Window.x2-x+3,Window.y1-3);
    PowerRedrawArea(Window.x2-x+1,Window.y2+3,Window.x2-x+2,Window.y2+3);
    PowerRedrawArea(Window.x2-x+3,Window.y2+1,Window.x2-x+3,Window.y2+3);

    Window.x2-=x;
    Window.Status=Enhanced;
  }
  if (Window.Status==Open || Window.Status==Enhanced) VMouse(1);
}
