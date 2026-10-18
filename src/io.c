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
#include "person_disk.h"
#include "save_disk.h"
#ifndef DOS
#include "platform.h"
#endif

#include "datapath.h"

#ifdef DOS
void (interrupt far *RealIrq1c)();
#endif

#ifndef DOS
static void WritePersonsDisk(short ofil)
{
  short i, di, dj;
  PersonDisk disk;
  for (i = 0; i < MAX_PRS + 1; i++) {
    memset(&disk, 0, sizeof(disk));
    disk.Num = Person[i].Num;
    disk.PhaseNum = Person[i].PhaseNum;
    disk.xs = (int32_t)Person[i].xs;
    disk.ys = (int32_t)Person[i].ys;
    disk.Anz = (int32_t)Person[i].Anz;
    for (di = 0; di < 8; di++)
      for (dj = 0; dj < 12; dj++)
        disk.Sequence[di][dj] = Person[i].Sequence[di][dj];
    disk.Counter = Person[i].Counter;
    disk.Dir = Person[i].Dir;
    disk.Boden = Person[i].Boden;
    disk.GhostLevel = Person[i].GhostLevel;
    disk.x = Person[i].x;
    disk.y = Person[i].y;
    disk.z = Person[i].z;
    disk.TargetX = Person[i].TargetX;
    disk.TargetY = Person[i].TargetY;
    disk.gtx = Person[i].gtx;
    disk.gty = Person[i].gty;
    disk.iWalk = Person[i].iWalk;
    disk.BaseSpeed = Person[i].BaseSpeed;
    disk.StepSpeed = Person[i].StepSpeed;
    disk.PhaseSpeed = Person[i].PhaseSpeed;
    disk.Bits = Person[i].Bits;
    ChkWrite(ofil, (char *)&disk, sizeof(disk));
  }
}

static void ReadPersonsDisk(short ifil)
{
  short i, di, dj;
  PersonDisk disk;
  for (i = 0; i < MAX_PRS + 1; i++) {
    ChkRead(ifil, (char *)&disk, sizeof(disk));
    memset(&Person[i], 0, sizeof(Person[i]));
    Person[i].Num = disk.Num;
    Person[i].PhaseNum = disk.PhaseNum;
    Person[i].xs = disk.xs;
    Person[i].ys = disk.ys;
    Person[i].Anz = disk.Anz;
    for (di = 0; di < 8; di++)
      for (dj = 0; dj < 12; dj++)
        Person[i].Sequence[di][dj] = disk.Sequence[di][dj];
    Person[i].Counter = disk.Counter;
    Person[i].Dir = disk.Dir;
    Person[i].Boden = disk.Boden;
    Person[i].GhostLevel = disk.GhostLevel;
    Person[i].x = disk.x;
    Person[i].y = disk.y;
    Person[i].z = disk.z;
    Person[i].TargetX = disk.TargetX;
    Person[i].TargetY = disk.TargetY;
    Person[i].gtx = disk.gtx;
    Person[i].gty = disk.gty;
    Person[i].iWalk = disk.iWalk;
    Person[i].BaseSpeed = disk.BaseSpeed;
    Person[i].StepSpeed = disk.StepSpeed;
    Person[i].PhaseSpeed = disk.PhaseSpeed;
    Person[i].Bits = disk.Bits;
    Person[i].Phase = NULL;
  }
}

static void WriteMarkersDisk(short ofil)
{
  short i;
  MarkerDisk d;
  for (i = 0; i < 50; i++) {
    d.x1 = Marker[i].x1; d.y1 = Marker[i].y1;
    d.x2 = Marker[i].x2; d.y2 = Marker[i].y2;
    d.Ref = Marker[i].Ref;
    d.MarkTyp = (int16_t)Marker[i].MarkTyp;
    ChkWrite(ofil, (char *)&d, sizeof(d));
  }
}

static void ReadMarkersDisk(short ifil)
{
  short i;
  MarkerDisk d;
  for (i = 0; i < 50; i++) {
    ChkRead(ifil, (char *)&d, sizeof(d));
    Marker[i].x1 = d.x1; Marker[i].y1 = d.y1;
    Marker[i].x2 = d.x2; Marker[i].y2 = d.y2;
    Marker[i].Ref = d.Ref;
    Marker[i].MarkTyp = (enum MARKTYP)d.MarkTyp;
  }
}

static void WritePicsDisk(short ofil)
{
  short i;
  PicOnScreenDisk d;
  for (i = 0; i < 10; i++) {
    d.Num = PicturesOnScreen[i].Num;
    d.Typ = (int16_t)PicturesOnScreen[i].Typ;
    ChkWrite(ofil, (char *)&d, sizeof(d));
  }
}

static void ReadPicsDisk(short ifil)
{
  short i;
  PicOnScreenDisk d;
  for (i = 0; i < 10; i++) {
    ChkRead(ifil, (char *)&d, sizeof(d));
    PicturesOnScreen[i].Num = d.Num;
    PicturesOnScreen[i].Typ = d.Typ;
  }
}

static void WriteDialogsDisk(short ofil)
{
  short i, j;
  DialogDisk d;
  for (i = 0; i < 20; i++) {
    d.ReferenzNummer = Dialog[i].ReferenzNummer;
    d.TextProc = Dialog[i].TextProc;
    for (j = 0; j < 30; j++) {
      d.Inhalt[j].TextAdd = Dialog[i].Inhalt[j].TextAdd;
      d.Inhalt[j].RefNum = Dialog[i].Inhalt[j].RefNum;
      d.Inhalt[j].Typ = Dialog[i].Inhalt[j].Typ;
      d.Inhalt[j].StdTyp = Dialog[i].Inhalt[j].StdTyp;
      d.Inhalt[j].Querverweis = Dialog[i].Inhalt[j].Querverweis;
    }
    ChkWrite(ofil, (char *)&d, sizeof(d));
  }
}

static void ReadDialogsDisk(short ifil)
{
  short i, j;
  DialogDisk d;
  for (i = 0; i < 20; i++) {
    ChkRead(ifil, (char *)&d, sizeof(d));
    Dialog[i].ReferenzNummer = d.ReferenzNummer;
    Dialog[i].TextProc = d.TextProc;
    for (j = 0; j < 30; j++) {
      Dialog[i].Inhalt[j].TextAdd = d.Inhalt[j].TextAdd;
      Dialog[i].Inhalt[j].RefNum = d.Inhalt[j].RefNum;
      Dialog[i].Inhalt[j].Typ = d.Inhalt[j].Typ;
      Dialog[i].Inhalt[j].StdTyp = d.Inhalt[j].StdTyp;
      Dialog[i].Inhalt[j].Querverweis = d.Inhalt[j].Querverweis;
    }
  }
}

static void WriteSayPropsDisk(short ofil)
{
  short i;
  SayPropDisk d;
  for (i = 0; i < 10; i++) {
    d.PrsNum = SayProperties[i].PrsNum;
    d.TxtColor = SayProperties[i].TxtColor;
    d.StdFont = SayProperties[i].StdFont;
    ChkWrite(ofil, (char *)&d, sizeof(d));
  }
}

static void ReadSayPropsDisk(short ifil)
{
  short i;
  SayPropDisk d;
  for (i = 0; i < 10; i++) {
    ChkRead(ifil, (char *)&d, sizeof(d));
    SayProperties[i].PrsNum = d.PrsNum;
    SayProperties[i].TxtColor = d.TxtColor;
    SayProperties[i].StdFont = d.StdFont;
  }
}

static void WriteSayWindowsDisk(short ofil)
{
  short c;
  SayWindowDisk d;
  for (c = 0; c < 5; c++) {
    memset(&d, 0, sizeof(d));
    d.RefPrs = SayWindow[c].RefPrs;
    d.x = SayWindow[c].x; d.y = SayWindow[c].y;
    d.xs = SayWindow[c].xs; d.ys = SayWindow[c].ys;
    d.x1 = SayWindow[c].x1; d.x2 = SayWindow[c].x2;
    d.Font = SayWindow[c].Font;
    d.Time = SayWindow[c].Time;
    d.wx = SayWindow[c].wx; d.wy = SayWindow[c].wy;
    d.hasData = (SayWindow[c].RefPrs != -1 && SayWindow[c].Data) ? 1 : 0;
    d.hasText = (SayWindow[c].RefPrs != -1 && SayWindow[c].Text) ? 1 : 0;
    ChkWrite(ofil, (char *)&d, sizeof(d));
    if (d.hasData)
      ChkWrite(ofil, SayWindow[c].Data, SayWindow[c].xs * SayWindow[c].ys);
    if (d.hasText) {
      short h = (short)(strlen(SayWindow[c].Text) + 1);
      ChkWrite(ofil, (char *)&h, 2);
      ChkWrite(ofil, SayWindow[c].Text, h);
    }
  }
}

static void ReadSayWindowsDisk(short ifil)
{
  short c;
  SayWindowDisk d;
  for (c = 0; c < 5; c++) {
    ChkRead(ifil, (char *)&d, sizeof(d));
    SayWindow[c].RefPrs = d.RefPrs;
    SayWindow[c].x = d.x; SayWindow[c].y = d.y;
    SayWindow[c].xs = d.xs; SayWindow[c].ys = d.ys;
    SayWindow[c].x1 = d.x1; SayWindow[c].x2 = d.x2;
    SayWindow[c].Font = d.Font;
    SayWindow[c].Time = d.Time;
    SayWindow[c].wx = d.wx; SayWindow[c].wy = d.wy;
    SayWindow[c].Data = NULL;
    SayWindow[c].Text = NULL;
    if (d.RefPrs != -1) {
      if (d.hasData) {
        SayWindow[c].Data = MALLOC(SayWindow[c].xs * SayWindow[c].ys);
        ChkRead(ifil, SayWindow[c].Data, SayWindow[c].xs * SayWindow[c].ys);
      }
      if (d.hasText) {
        short h = 0;
        ChkRead(ifil, (char *)&h, 2);
        if (h > 0) {
          SayWindow[c].Text = MALLOC(h);
          ChkRead(ifil, SayWindow[c].Text, h);
        }
      }
    }
  }
}

#endif

char *_FRAGEZEICHEN="?";
char *_MOAS_CFG="MOAS.CFG";

unsigned short FrequencyTab[]={ 363,385,408,432,458,485,514,544,577,611,647,686};
unsigned char CellTab[]={ 0x00,0x03, 0x01,0x04, 0x02,0x05, 0x08,0x0b, 0x09,0x0c, 0x0a,0x0d, 0x10,0x13, 0x11,0x14, 0x12,0x15 };

short DataOpen(short Num)
{
  short FileHandle;
  char Name[10];

  sprintf(Name,"PIZZZA.%i",Num);
#ifdef DOS
  FileHandle=open(Name,O_RDONLY|O_BINARY);
#else
  FileHandle=MoasOpenData(Name);
#endif
  if (FileHandle<0)
  {
    sprintf(Name,"GAME.%i",Num);
#ifdef DOS
    FileHandle=open(Name,O_RDONLY|O_BINARY);
#else
    FileHandle=MoasOpenData(Name);
#endif
  }

  if (FileHandle<0) {
#ifndef DOS
    MoasReportMissingData(Name);
#else
    puts(Name); getch();
#endif
    InternalError(1002);
  }

  return(FileHandle);
}
void LadeProgramm(short Num)
{
  if (Num < 0)
    return;
  if (Num>=AnzProgramme) {
    fprintf(stderr, "moas: LadeProgramm(%d) out of range (AnzProgramme=%d)\n",
            (int)Num, (int)AnzProgramme);
    InternalError(1025);
  }
  if (Programm[Num].Data) return;
  if (Programm[Num].Length <= 0) {
    Programm[Num].Data = (uChar *)MALLOC(4);
    memset(Programm[Num].Data, 0, 4);
    return;
  }
  /* +4 pad: script decoder may peek past last byte on stub (len=1) entries */
  Programm[Num].Data=(char *)MALLOC(Programm[Num].Length + 4);
  memset(Programm[Num].Data + Programm[Num].Length, 0, 4);
  LadeBereich(Programm[Num].FileNum,Programm[Num].FilePos,Programm[Num].Length,Programm[Num].Data,Programm[Num].Check);
}
void LadePhase(short Num)
{
  register long Length;
  char *Tmp;

  if (Num < 0)
    return; /* unused person / not yet assigned */
  if (Num>=AnzPhasen) {
    fprintf(stderr, "moas: LadePhase(%d) out of range (AnzPhasen=%d)\n",
            (int)Num, (int)AnzPhasen);
    InternalError(1022);
  }
  if (Phase[Num].Data) return;
  if (Phase[Num].Length <= 0 || Phase[Num].x <= 0 || Phase[Num].y <= 0) {
    fprintf(stderr, "moas: LadePhase(%d): empty/invalid (len=%ld x=%ld y=%ld)\n",
            (int)Num, (long)Phase[Num].Length, (long)Phase[Num].x, (long)Phase[Num].y);
    Phase[Num].Data = (uChar *)MALLOC(1);
    return;
  }

  Length=Phase[Num].x*Phase[Num].y*Phase[Num].Anz;
  if (Length <= 0) Length = 1;

  Phase[Num].Data=(char *)MALLOC(Length+5);
  Tmp=MALLOC(Phase[Num].Length+5);
  LadeBereich(Phase[Num].FileNum,Phase[Num].FilePos,Phase[Num].Length,Tmp,Phase[Num].Check);
  UniversalUnpack(Tmp+1,Phase[Num].Data,Phase[Num].Length-1,254,Phase[Num].x);
  FREE(Tmp);
}
void DisplayBox(unsigned char *Data,short x,short y,short xs,short ys)
{
  register short cx,cy;
  register char *Vga=VGA_ADRESSE;

  for (cy=0;cy<ys;cy++)
    for (cx=0;cx<xs;cx++)
      if (*Data!=255)
        Vga[cx+x+Tab320[cy+y]]=*Data++;
      else Data++;
}
void LadeBild(short Num)
{
  short c;

  if (Num>0)
  {
    AddPictureToTab(Num,Normal);
    if (Num>=AnzBilder) InternalError(1023);
    Bilder[Num].Data=(char *)MALLOC(Bilder[Num].Length+5);
    LadeBereich(Bilder[Num].FileNum,Bilder[Num].FilePos,Bilder[Num].Length,Bilder[Num].Data,Bilder[Num].Check);
    if (Bilder[Num].x&16384)
    {
      Bilder[Num].Length-=768;
      memcpy(Palette,Bilder[Num].Data+Bilder[Num].Length,768);
      for (c=3;c<768;c+=3)
        if (Palette[c]==Palette[c-3] && Palette[c+1]==Palette[c-2] && Palette[c+2]==Palette[c-1])
        if (Palette[c]==63 && Palette[c+1]>50 && Palette[c+1]<60 && Palette[c+2]==63)
        {
          Palette[c-3]=rand()%64;
          Palette[c-2]=rand()%64;
          Palette[c-1]=rand()%64;
        }
      for (c=0;c<12;c++) Palette[738+c]=Palette[738+c+12]+3;
      for (c=0;c<MAX_PRS;c++)
       if (Person[c].Num!=-1 && (Person[c].Bits & 33536))
       {
         MakeMegaTable(1);
         break;
       }
      if (Eingeblendet) QuickSetPalette(Palette);
    }
    UnpackPicture(Bilder[Num].Data+1,0,Bilder[Num].Length-1);
    FREE(Bilder[Num].Data);
    PowerRedrawArea(0,0,319,Bilder[Num].y);
    if (Bilder[Num].x&16384) Bilder[Num].Length+=768;
  }
  else
  {
    char *Tmp=Bilder[Num].Data;

    if (Tmp==NULL)
    {
      Bilder[Num].Data=(char *)MALLOC(Bilder[Num].x*Bilder[Num].y+5);
      Tmp=Bilder[Num].Data+Bilder[Num].x*Bilder[Num].y-Bilder[Num].Length+5;
      LadeBereich(Bilder[Num].FileNum,Bilder[Num].FilePos,Bilder[Num].Length,Tmp,Bilder[Num].Check);
      UniversalUnpack(Tmp+1,Bilder[Num].Data,Bilder[Num].Length-1,254,Bilder[Num].x);
    }
    DisplayBox(Bilder[Num].Data,0,122,Bilder[Num].x,Bilder[Num].y);
  }
}
// °±²Û Infobild Typ 1 oder 2 laden. Typ muá InfoTyp1 oder InfoTyp2 sein Û²±° //
void LadeInfoBild(short Num,enum PICTYP Typ)
{
  unsigned short c;

  AddPictureToTab(Num,Typ);
  if (Num>=AnzBilder) InternalError(1023);
  if (Bilder[Num].x&16384) InternalError(1011);
  Bilder[Num].Data=(char *)MALLOC(Bilder[Num].Length);
  LadeBereich(Bilder[Num].FileNum,Bilder[Num].FilePos,Bilder[Num].Length,Bilder[Num].Data,Bilder[Num].Check);
  if (Typ==InfoTyp1)
    UniversalUnpack(Bilder[Num].Data+1,Info1,Bilder[Num].Length-1,254,320);
  else
    UniversalUnpack(Bilder[Num].Data+1,Info2,Bilder[Num].Length-1,254,320);
  FREE(Bilder[Num].Data);

  for (c=0;c<MAX_PRS;c++)
    if ((Person[c].Bits & (1024+768)) && Person[c].Num!=-1)
    {
      Person[c].Boden=Boden(c);
      LoescheKopie(c);
      CheckKopie(c);
    }
}
void LadeInventar(short Num)
{
  char *Tmp;

  if (Num>=AnzInventar) InternalError(1024);
  if (Inventar[Num].Data) return;

  Inventar[Num].Data=(char *)MALLOC(32*24+5);
  Tmp=MALLOC(Inventar[Num].Length+5);
  LadeBereich(Inventar[Num].FileNum,Inventar[Num].FilePos,Inventar[Num].Length,Tmp,Inventar[Num].Check);
  UniversalUnpack(Tmp+1,Inventar[Num].Data,Inventar[Num].Length-1,254,32);
  FREE(Tmp);
}
void LadeBereich(char FileNum,long FilePos,long Length,char *Data,CHKSUM CheckSum)
{
  short ifil;

  /* Empty / stub catalog entries (Length 0 or 1) appear in GAME.2; skip. */
  if (Length <= 0) {
    return;
  }
  if (FileNum < 1) {
    fprintf(stderr,
            "moas: LadeBereich: bad FileNum=%d pos=%ld len=%ld (skipping stub)\n",
            (int)FileNum, (long)FilePos, (long)Length);
#ifndef DOS
    fprintf(stderr, "  data dir: %s\n", MoasDataDir());
#endif
    /* Original data never uses FileNum 0 for real resources; treat as empty. */
    if (Length <= 1)
      return;
    InternalError(1002);
  }
  if (Data == NULL) {
    fprintf(stderr, "moas: LadeBereich: NULL buffer FileNum=%d len=%ld\n",
            (int)FileNum, (long)Length);
    InternalError(1000);
  }

  ifil=DataOpen(FileNum);     ChkOpen(_FRAGEZEICHEN,READ);
  if (ifil < 0) {
#ifndef DOS
    char name[32];
    snprintf(name, sizeof(name), "GAME.%d", (int)FileNum);
    MoasReportMissingData(name);
#endif
    InternalError(1002);
  }
  if (lseek(ifil,FilePos,SEEK_SET)==-1) InternalError(1027);
  ChkRead(ifil,Data,Length);
  close(ifil);
  if (GetChk()!=CheckSum) InternalError(1007);
}
void LoadSaveMenu(char *Text)
{
  unsigned short x,y,c,d,h,hh;
  short ifil;
  char String[40];
  unsigned char *Vga=VGA_ADRESSE,*p;

  OpenWindow(1,2,24);
  for (c=0;c<15;c++) NeueZeile();
  LargePrint(Text,((char*)VGA_ADRESSE)+17+320*20,253,320);
  for (d=0;d<143;d+=142)
    for (c=0;c<5;c++)
      if (c+d/142*5!=9 || *Text=='L')
      {
        Line(252,19+d,33+c*30,59+d,33+c*30);
        Line(252,19+d,33+c*30,19+d,58+c*30);
        Line(250,60+d,36+c*30,60+d,59+c*30);
        Line(250,20+d,59+c*30,60+d,59+c*30);
        sprintf(String,"GAME%i.SAV",c+d/142*5);
        ifil=open(String,O_RDONLY
#ifdef DOS
                  |O_BINARY
#endif
          );
        if (ifil>0)
        {
          for (h=0;h<25;h++)
            read(ifil,Vga+20+d+(34+h+c*30)*320,40);
          read(ifil,(char*)&h,2);
          read(ifil,(char*)&hh,2);
          sprintf(String,"%i von %i",h,hh);
          SmallPrint(String,((char*)VGA_ADRESSE)+60+d+(48+c*30)*320,253,320);
          read(ifil,String,40);
          p=strtok(String," "); x=y=0;
          while (p!=NULL)
          {
            if (x!=0) x+=RealStringLength(" ",0);
            if (RealStringLength(p,0)+x>100) { x=0; y+=6; }
            SmallPrint(p,((char*)VGA_ADRESSE)+60+d+x+(34+y+c*30)*320,253,320);
            x+=RealStringLength(p,0);
            p=strtok(NULL," ");
          }
          close(ifil);
        }
      }
  PlayerHasControl=WalkingAllowed=0;
  VMouse(1);
  while (Mouse.Left!=0 || Mouse.LastLeft!=1) MouseCheck();
  CloseWindow();
  PlayerHasControl=WalkingAllowed=1;
}
void SaveGame(short CallerID)
{
  unsigned short c,d;
  short h,ofil;
  char Name[20],*MiniPic;
  unsigned char *Vga=VGA_ADRESSE;

  if (CallerID==0) LoadSaveMenu("Speichern:");
  for (d=0;d<143;d+=142)
    for (c=0;c<5;c++)
      if ((Mouse.x>=19+d && Mouse.y>=33+c*30 && Mouse.x<=60+d && Mouse.y<=60+c*30 && c+d/142*5!=9 && CallerID==0) || (CallerID==1 && c+d/142*5==9))
      {
        MiniPic=MALLOC(1000);
        sprintf(Name,"GAME%i.SAV",c+d/142*5);
        ofil=ChkOpen(Name,WRITE);
        for (d=0;d<200;d+=8)
          for (c=0;c<320;c+=8)
            MiniPic[d/8*40+c/8]=(Palette[Vga[c+d*320]*3]+Palette[Vga[c+d*320]*3+1]+Palette[Vga[c+d*320]*3+2])/12;
        ChkWrite(ofil,MiniPic,1000);
        ChkWrite(ofil,(char*)&V_Points,2);
        ChkWrite(ofil,(char*)&V_TotalPoints,2);
        ChkWrite(ofil,RoomName,40);
#ifndef DOS
        WritePicsDisk(ofil);
        WritePersonsDisk(ofil);
        WriteMarkersDisk(ofil);
#else
        ChkWrite(ofil,(char*)PicturesOnScreen,sizeof(PicturesOnScreen));
        ChkWrite(ofil,(char*)Person,sizeof(Person));
        ChkWrite(ofil,(char*)Marker,sizeof(Marker));
#endif
        ChkWrite(ofil,(char*)Ontimes,sizeof(struct ONTIME)*10);
        ChkWrite(ofil,(char *)Var,AnzVars*2);
        ChkWrite(ofil,(char *)&CurrentProc,2);
        ChkWrite(ofil,(char *)&GhostMode,1);
        ChkWrite(ofil,(char *)&TotalSpeed,2); ChkWrite(ofil,(char *)&SpeechSpeed,2);
        ChkWrite(ofil,(char *)&DayLightR,2);  ChkWrite(ofil,(char *)&DayLightG,2);
        ChkWrite(ofil,(char *)&DayLightB,2);
        ChkWrite(ofil,(char *)&PaletteCycleFirst,2);
        ChkWrite(ofil,(char *)&PaletteCycleLast,2);
#ifndef DOS
        WriteDialogsDisk(ofil);
        WriteSayPropsDisk(ofil);
#else
        ChkWrite(ofil,(char *)Dialog,sizeof(struct DIALOG)*20);
        ChkWrite(ofil,(char*)SayProperties,sizeof(struct SAY_PROPERTIES)*10);
#endif
        for (c=0;c<AnzInventar;c++)
        {
          ChkWrite(ofil,(char*)&Inventar[c].x,sizeof (Inventar[c].x));
          ChkWrite(ofil,(char*)&Inventar[c].y,sizeof (Inventar[c].y));
        }
#ifndef DOS
        WriteSayWindowsDisk(ofil);
#else
        for (c=0;c<5;c++)
        {
          ChkWrite(ofil,(char*)&SayWindow[c],sizeof(struct SAY_WINDOW));
          if (SayWindow[c].RefPrs!=-1)
          {
            if (SayWindow[c].Data) ChkWrite(ofil,SayWindow[c].Data,SayWindow[c].xs*SayWindow[c].ys);
            if (SayWindow[c].Text)
            {
              h=strlen(SayWindow[c].Text)+1;
              ChkWrite(ofil,(char*)&h,2);
              ChkWrite(ofil,SayWindow[c].Text,h);
            }
          }
        }
#endif
        ChkClose(ofil);
        FREE(MiniPic);
        return;
      }
}
void LoadGame(void)
{
  unsigned short c,d;
  short h,ifil;
  enum MARKTYP Typ;
  char Name[20];

  LoadSaveMenu("Laden:");
  for (d=0;d<143;d+=142)
    for (c=0;c<5;c++)
      if (Mouse.x>=19+d && Mouse.y>=33+c*30 && Mouse.x<=60+d && Mouse.y<=60+c*30)
      {
        sprintf(Name,"GAME%i.SAV",c+d/142*5);
        ifil=open(Name,O_RDONLY
#ifdef DOS
                  |O_BINARY
#endif
          );
        if (ifil<0) return; else close(ifil);
        ifil=ChkOpen(Name,READ);
        V_LockVga=1;
        RemoveAllPersons();
        CloseSay(255);
        Blend(4,0);
        for (c=0;c<251;c++) ChkRead(ifil,RoomName,4); /*Dummies*/
        ChkRead(ifil,RoomName,40);
#ifndef DOS
        ReadPicsDisk(ifil);
        ReadPersonsDisk(ifil);
        ReadMarkersDisk(ifil);
#else
        ChkRead(ifil,(char*)PicturesOnScreen,sizeof(PicturesOnScreen));
        ChkRead(ifil,(char*)Person,sizeof(Person));
        ChkRead(ifil,(char*)Marker,sizeof(Marker));
#endif
        ChkRead(ifil,(char*)Ontimes,sizeof(struct ONTIME)*10);
        ChkRead(ifil,(char *)Var,AnzVars*2);
        ChkRead(ifil,(char *)&CurrentProc,2);
        ChkRead(ifil,(char *)&GhostMode,1);
        ChkRead(ifil,(char *)&TotalSpeed,2); ChkRead(ifil,(char *)&SpeechSpeed,2);
        ChkRead(ifil,(char *)&DayLightR,2);  ChkRead(ifil,(char *)&DayLightG,2);
        ChkRead(ifil,(char *)&DayLightB,2);
        ChkRead(ifil,(char *)&PaletteCycleFirst,2);
        ChkRead(ifil,(char *)&PaletteCycleLast,2);
#ifndef DOS
        ReadDialogsDisk(ifil);
        ReadSayPropsDisk(ifil);
#else
        ChkRead(ifil,(char *)Dialog,sizeof(struct DIALOG)*20);
        ChkRead(ifil,(char*)SayProperties,sizeof(struct SAY_PROPERTIES)*10);
#endif
        for (c=0;c<AnzInventar;c++)
        {
          ChkRead(ifil,(char*)&Inventar[c].x,sizeof (Inventar[c].x));
          ChkRead(ifil,(char*)&Inventar[c].y,sizeof (Inventar[c].y));
        }
#ifndef DOS
        ReadSayWindowsDisk(ifil);
#else
        for (c=0;c<5;c++)
        {
          ChkRead(ifil,(char*)&SayWindow[c],sizeof(struct SAY_WINDOW));
          if (SayWindow[c].RefPrs!=-1)
          {
            h=strlen(SayWindow[c].Text);
            if (SayWindow[c].Data)
            {
              SayWindow[c].Data=MALLOC(SayWindow[c].xs*SayWindow[c].ys);
              ChkRead(ifil,SayWindow[c].Data,SayWindow[c].xs*SayWindow[c].ys);
            }
            if (SayWindow[c].Text)
            {
              ChkRead(ifil,(char*)&h,2);
              SayWindow[c].Text=MALLOC(h);
              ChkRead(ifil,SayWindow[c].Text,h);
            }
          }
        }
#endif
        ChkClose(ifil);
        if (GhostMode==1)
        {
          MakeMegaTable(-1);
          MakeMegaTable(1);
        }
        for (c=0;c<MAX_PRS;c++)
          if (Person[c].Num!=-1)
          {
            LadePhase(Person[c].PhaseNum);
            if (Person[c].Bits & (1024+768))
              Person[c].Phase=MALLOC(Person[c].xs*Person[c].ys*Person[c].Anz);
            else Person[c].Phase=Phase[Person[c].PhaseNum].Data;
            LoescheKopie(c);
            CheckKopie(c);
          }

        Clearscreen();     InitScreenData();
        for (c=0;c<10;c++)
          if (PicturesOnScreen[c].Typ!=-1)
          {
            Typ=PicturesOnScreen[c].Typ; PicturesOnScreen[c].Typ=-1;
            if (Typ==Normal) LadeBild(PicturesOnScreen[c].Num);
            else if (Typ==InfoTyp1) LadeInfoBild(PicturesOnScreen[c].Num,InfoTyp1);
            else if (Typ==InfoTyp2) LadeInfoBild(PicturesOnScreen[c].Num,InfoTyp2);
          }

        PowerRedrawArea(0,0,319,199);
        if (V_InventarState) TurnInventar(1);
        Blend(4,1);
        return;
      }
}
void CheckFile(unsigned char a,unsigned char b,unsigned char c,unsigned char d)
{
  short ifil,AnzRead;
  CHKSUM NewCheck;
  char *p;

  p=MALLOC(4000);
//ifil=open(_pgmptr,O_RDONLY|O_BINARY);          /* ==>+<== */
  if (ifil<0) InternalError(1033);
  ClrChk();

  while (1)
  {
    AnzRead=read(ifil,p,4000);
    if (AnzRead==0) break;
    AreaChk(p,AnzRead);
  }
  close(ifil);
  NewCheck=GetChk();
  if (NewCheck!=(a+((CHKSUM)b*256)+((CHKSUM)c*65536)+((CHKSUM)d*256*65536)))
    InternalError(1033);
  FREE(p);
}
void FileMenu(void)
{
  short x,y=0,Redraw=1;

  DontCyclePal++;
  while (y!=-1)
  {
    if (Redraw)
    {
      LadeBild(0);
      if (y==0) x=GlobalVolume/4+13;
      if (y==1) x=SpeechSpeed*3+10;
      if (y==2) x=TotalSpeed*10+3;
      _setcolor(5);
#ifdef DOS
      _rectangle(_GFILLINTERIOR,x-2,y*21+145,x+2,y*21+153);
#endif
      Rectangle3D(VGA_ADRESSE,x-2,y*21+145,x+2,y*21+153,4,6,320);
    }
    switch(getch())
    {
      case 27:
      case 13:
      case 32:y=-1;  break;
      case 0:switch(getch())
             {
               case 'P':y++; Redraw=1; break;
               case 'H':y--; Redraw=1; break;
               case 'M':if (y==0) GlobalVolume+=12;
                        if (y==1) SpeechSpeed++;
                        if (y==2) TotalSpeed++;
                        Redraw=1; break;
               case 'K':if (y==0) GlobalVolume-=12;
                        if (y==1) SpeechSpeed--;
                        if (y==2) TotalSpeed--;
                        Redraw=1; break;

             }
             if (y<0) y=0;
             if (y>2) y=2;
             if (GlobalVolume>300) {GlobalVolume=0;Redraw=0;}
             if (GlobalVolume>255) {GlobalVolume=255;Redraw=0;}
             if (TotalSpeed==0) {TotalSpeed=1;Redraw=0;}
             if (SpeechSpeed==0) {SpeechSpeed=1;Redraw=0;}
             if (TotalSpeed==9) {TotalSpeed=8;Redraw=0;}
             if (SpeechSpeed==25) {SpeechSpeed=24;Redraw=0;}
             break;
    }
  }
  DontCyclePal--;
  PowerRedrawArea(0,122,114,199);
}

#ifdef DOS
/*                                     */
/* Interrupt Servies im Protected Mode */
/*                                     */
void __interrupt /*__far*/ MyTickerIrq(void)
{
  static uChar Tmp,x,y,r,g,b;
  static short c,Von,Bis,CycleCounter;

  /* Generelles */
  TimeCounter++;

  /* Das PalettenScrolling */
  if (DontCyclePal==0 && PaletteCycleFirst<PaletteCycleLast && PaletteCycleFirst<256 && PaletteCycleLast<256)
  {
    CycleCounter++;
    if (CycleCounter>CycleWait)
    {
      CycleCounter=0;

      Von=PaletteCycleFirst*3;
      Bis=PaletteCycleLast*3;

      Tmp=Palette[Bis];
      x=Palette[Bis+1];
      y=Palette[Bis+2];

      for (c=Bis-1;c>=Von;c--)
        Palette[c+3]=Palette[c];

      Palette[Von]=Tmp;
      Palette[Von+1]=x;
      Palette[Von+2]=y;

      for (c=PaletteCycleFirst;c<=PaletteCycleLast;c++)
      {
        r=Palette[c+c+c];
        g=Palette[c+c+c+1];
        b=Palette[c+c+c+2];

        if (V_Colored==0) r=g=b=(r+g+b)/3;

        outp(0x3c8,c);
        outp(0x3c9,r*DayLightR/100);
        outp(0x3c9,g*DayLightG/100);
        outp(0x3c9,b*DayLightB/100);
      }
    }
  }

  /* Die Verwaltung */
  // if ((TimeCounter&7)==0) _chain_intr( RealIrq1c );
  //                 else ((long*)0x0046c)[0]--;
  if ((TimeCounter&7)!=0) ((long*)0x0046c)[0]--;
  _chain_intr( RealIrq1c );
}
#endif

#ifdef DOS
void __interrupt __far KeyboardIrq(void)
{
  KeyScanCode=inp(0x60);

  if (KeyAbortState==1 && KeyScanCode==1)  /* Abort possible && ESC pressed */
  {
    KeyAbortState=2;     /* Sequenz abbrechen */
    outp(0x60,0);        /* Unfreiwillige Wiederholung ausschliessen */
  }

  _chain_intr( RealIrq9 );
}
#endif

short GetTicker(void)
{
#ifdef DOS
  kbhit();
  return(TimeCounter);
#else
  return PlatformGetTicker();
#endif
}
