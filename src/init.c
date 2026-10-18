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

#define D32RealSeg(P)   ((((uLong) (P)) >> 4) & 0xFFFF)
#define D32RealOff(P)   (((uLong) (P)) & 0xF)

unsigned short PaletteCycleFirst=0,PaletteCycleLast=0,DontCyclePal=0;

void MakeMegaTable(short Percent);
char *CopyrightStringForInternalReview="MOAS & DPP (C) 1992-1995 Thomas Holz";

struct
{
  unsigned long SektorNum;        /* Boot-Sector */
  unsigned short AnzRead;         /* Read 1 Sector */
  unsigned char *IoBuffer;        /* Pointer to the Buffer */
} Boot;

void InitAll(void)
{
  ReadLicence();
  Screen=D32DosMemAlloc(65000*3); Info1=Screen+65000; Info2=Info1+65000; //freiwillig im DOS !
  Palette=MALLOC(768*129); Paletten=Palette+768;
  PRedrawTmpBak=MALLOC(6000);
  SmallTextLine=MALLOC(340*(5+2+2));  memset(SmallTextLine,0,340*(5+2+2));
  VMouseBuf=MALLOC(40*44);
  InitDialogs();             //InitDigi();
  Init320Tab();              InitScreenData();
  InitVars();                InitPersons();
  InitPictureData();         InitOntimes();
  InitBreakVector();         InitMapData();
  InitSystemVars();          InitFont();
  InitMarker();              LoadPack();
  DayLightR=DayLightG=DayLightB=0;
#ifdef DOS
  if (!_setvideomode(_MRES256COLOR)) InternalError(1003);
  inp(0x3d4+6);
  outp(0x3c0,0x11); outp(0x3c0,0);  /* FrameColor := 0  */
  outp(0x3c0,0x20);                 /* Reset Controller */
#else
  if (PlatformInit() != 0)
    InternalError(1003);
#endif
  AsmQPalSet(Palette);
}
unsigned long DosGetDiskIdNum(void)
{
#ifdef DOS
  union REGS Regs;
  struct SREGS sRegs;

  Boot.SektorNum=0;
  Boot.AnzRead=1;
  Boot.IoBuffer=MALLOC(1024);       /* 2 x 512 Bytes */
  memset(Boot.IoBuffer,255,1024);

  Regs.h.al=2; /* c: */
  Regs.w.cx=0xffff; /* Use structure */
  Regs.w.bx=((unsigned long)&(Boot.SektorNum))&65535;
  sRegs.ds=((unsigned long)&(Boot.SektorNum))>>16;
  int386x(0x25,&Regs,&Regs,&sRegs);
  if (memcmp(Boot.IoBuffer,Boot.IoBuffer+512,512)==0) InternalError(1047);

  Boot.SektorNum=*((unsigned long*)(Boot.IoBuffer+39));
  FREE(Boot.IoBuffer);
  return(Boot.SektorNum);
#else
  return 0;
#endif
}
void InitMarker(void)
{
  short c;
  for (c=0;c<50;c++) Marker[c].MarkTyp=Unused;
}
void InitOntimes(void)
{
  short c;
  for (c=0;c<10;c++) Ontimes[c].TimeLeft=0;
}
void Init320Tab(void)
{
  short c;

  for (Tab320[c=0]=0;c<199;c++) Tab320[c+1]=Tab320[c]+320;
  memset(Palette,0,768);
}
void InitSystemVars(void)
{
  short c;

  for (c=0;c<AnzVars;c++) Var[c]=0;
  for (c=0;c<5;c++) SayWindow[c].RefPrs=-1;        /*Say Windows lschen*/
  for (c=0;c<10;c++) SayProperties[c].PrsNum=-1;   /*Say Properties lschen*/
  for (c=0;c<MAX_PARSE;c++) PowerParse[c].Nummer=-1; /* PowerParser lschen*/
  V_Colored=1;  V_InventarState=1;
}
void InitVars(void)
{
  char *Fun=(char *)0x00417l;

  WalkingAllowed=PlayerHasControl=Packing=1;
  GhostMode=-1;
  *Fun=32;  /* Numlock */
  TotalSpeed=3; SpeechSpeed=8;
  KeyAbortState=0;
  Mouse.x=16;  Mouse.y=10;
  Window.Status=None;   AutoSaveGame=SpaceState=AnzDescription=0;
  MegaTable=NULL;
  Mouse.CursorState=1;
}
void InitBreakVector(void)
{
#ifdef DOS
  signal(SIGBREAK,SIG_IGN);
  signal(SIGINT,SIG_IGN);

  outp(0x40,0xff); outp(0x40,0x1f);
  RealIrq9=_dos_getvect(0x9);
  RealIrq1c=_dos_getvect(0x8);
  //RealSoundIrq=_dos_getvect(SoftIRQ[Config.IrqNum]);
  _dos_setvect(0x9,KeyboardIrq);
  _dos_setvect(0x8,MyTickerIrq);
  //_dos_setvect(SoftIRQ[Config.IrqNum],MySoundIrq);
  //StartSoundProcessing();
#endif
}
void InitPersons(void)
{
  short c;

  for (c=0;c<MAX_PRS;c++)
    Person[c].Num=-1;
}
void InitMapData(void)
{
  short ifil;

  ifil=DataOpen(2);   ChkOpen("?",READ);

  ChkRead(ifil,(char *)&AnzProgramme,2);
  ChkRead(ifil,(char *)&AnzBilder,2);
  ChkRead(ifil,(char *)&AnzInventar,2);
  ChkRead(ifil,(char *)&AnzPhasen,2);

  Programm=(struct MAPFILE *)MALLOC(AnzProgramme * (long)sizeof( struct MAPFILE ));
  Bilder  =(struct MAPFILE *)MALLOC(AnzBilder    * (long)sizeof( struct MAPFILE ));
  Inventar=(struct MAPFILE *)MALLOC(AnzInventar  * (long)sizeof( struct MAPFILE ));
  Phase   =(struct MAPFILE *)MALLOC(AnzPhasen    * (long)sizeof( struct MAPFILE ));

  ChkRead(ifil,(char *)Programm,AnzProgramme * sizeof( struct MAPFILE ));
  ChkRead(ifil,(char *)Bilder  ,AnzBilder    * sizeof( struct MAPFILE ));
  ChkRead(ifil,(char *)Inventar,AnzInventar  * sizeof( struct MAPFILE ));
  ChkRead(ifil,(char *)Phase   ,AnzPhasen    * sizeof( struct MAPFILE ));

//printf("Bilder[0].x=%3li  Bilder[0].y=%3li  Bilder[0].Anz=%3li\n",Bilder[0].x,Bilder[0].y,Bilder[0].Anz);
//printf("Bilder[1].x=%3li  Bilder[1].y=%3li  Bilder[1].Anz=%3li\n",Bilder[1].x,Bilder[1].y,Bilder[1].Anz);
//printf("Bilder[2].x=%3li  Bilder[2].y=%3li  Bilder[2].Anz=%3li\n",Bilder[2].x,Bilder[2].y,Bilder[2].Anz);
//printf("Phase[0].x=%3li   Phase[0].y=%3li   Phase[0].Anz=%3li\n",Phase[0].x,Phase[0].y,Phase[0].Anz);
//printf("Phase[1].x=%3li   Phase[1].y=%3li   Phase[1].Anz=%3li\n",Phase[1].x,Phase[1].y,Phase[1].Anz);
//printf("AnzVars=%i\n",AnzVars);
//getch();
  Var=(short *)MALLOC(AnzVars*2);
  ChkClose(ifil);
  LadePhase(0);
  LadePhase(1);
}
void InitScreenData(void)
{
  memset(Screen,0,64000);
  memset(Info1,0,64000);
  memset(Info2,0,64000);
}
void InitFont(void)
{
  short ifil,c,d,f,Tmp;

  ifil=ChkOpen("PIZZZA.XXX",READ);
  if (ifil<0) ifil=ChkOpen("GAME.XXX",READ);
  for (f=0;f<7;f++)
  {
    ChkRead(ifil,&Font[f].SizeY,1);
    ChkRead(ifil,(char*)&Tmp,2);        /* Read Anz of Colums */
    Font[f].Data=MALLOC(Tmp*3);         /* Each Column has 3 Bytes */
    ChkRead(ifil,Font[f].Data,Tmp*3);
    for (c=d=0;c<Tmp;c++)               /* Count the gaps (0,0,0) */
      if (Font[f].Data[c*3]==0) d++;    /* between the chars */
    Font[f].Begin=(short*)MALLOC(4*d);  /* and allocate an integer */
    Font[f].Width=Font[f].Begin+d;      /* for each gap */
    for (c=0,d=-1;c<Tmp-1;c++)
    {
      if (Font[f].Data[c*3]==0)         /* A new character begins */
      {
        d++;
        Font[f].Begin[d]=c*3+3;         /* Remember its position */
        Font[f].Width[d]=0;             /* And begin to count the Width */
        continue;
      }
      Font[f].Width[d]++;
    }
  }
  ChkClose(ifil);
}
void InitPictureData(void)
{
  short c;

  for (c=0;c<10;c++) PicturesOnScreen[c].Typ=-1;
}
void InitDialogs(void)
{
  short c;

  for (c=0;c<20;c++)
    Dialog[c].ReferenzNummer=-1;
}
void MakeMegaTable(short Percent)
{
  long c,d;

  if (Percent==-1)
  {
    if (MegaTable!=NULL) FREE(MegaTable);
    GhostMode=-1;        MegaTable=NULL;
    return;
  }
  if (MegaTable==NULL)
  {
    GhostMode=1;
    if (MegaTable==NULL) MegaTable=MALLOC(8192);
    memset(MegaTable+4096,0,4096);

    for (c=0;c<256;c++)
      if (c<PaletteCycleFirst || c>PaletteCycleLast)
        MegaTable[4096+(Palette[c*3]>>2)+(((US)Palette[c*3+1]>>2)<<4)+(((US)Palette[c*3+2]>>2)<<8)]=c;
    CoreMakeMegaTable(MegaTable);

    for (c=0;c<128;c++)
    {
      for (d=0;d<256;d++) { Paletten[c*768+d]    =(Palette[d*3  ]*c/128)>>2;
                            Paletten[c*768+d+256]=(Palette[d*3+1]*c/128)>>2;
                            Paletten[c*768+d+512]=(Palette[d*3+2]*c/128)>>2; }
    }
  }
}
void CoreMakeMegaTable(unsigned char *Table3D)
{
  register char r,g,b,Ok=1;
  register unsigned char *p;

  while (Ok)
  {
    Ok=0;
    memcpy(Table3D,Table3D+4096,4096);
    p=Table3D;
    for (b=0;b<16;b++)
      for (g=0;g<16;g++)
        for (r=16;r;r--,p++)
          if (!*p)
          {
            Ok=1;
            if (p[-256] && b) p[4096]=p[-256];
            else if (p[-16] && g) p[4096]=p[-16];
            else if (p[256] && b<15) p[4096]=p[256];
            else if (p[16] && g<15) p[4096]=p[16];
            else if (p[-1] && 16>r) p[4096]=p[-1];
            else if (p[1] && 1<r) p[4096]=p[1];
          }
  }
}
void ChkAuthorisation(void)
{
  short ifil;
  CHKSUM File;

  ifil=ChkOpen("PIZZZA.MAP",READ);
  if (ifil<0) ifil=ChkOpen("GAME.MAP",READ);
  ChkRead(ifil,(char*)&File,4);
  ChkClose(ifil);
  if (DosGetDiskIdNum()!=(File^0xABCD5678)) InternalError(1001);
}
void ReadLicence(void)
{
  short ifil=ChkOpen("Licence.Dat",READ);
  char Buffer[80];
  CHKSUM Check;

  if (ifil<=0) InternalError(1001);
  ChkRead(ifil,Buffer,filelength(ifil)-4);
  printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n%s%s\n\n\n\n\n\n\n\n\n\n\n\n\n","                                          \0\0\0"+strlen(Buffer)/2,Buffer);
  if (read(ifil,(char*)&Check,sizeof( CHKSUM ))!=sizeof( CHKSUM )) InternalError(1001);
  if (Check!=GetChk()) InternalError(1001);
  close(ifil);
}
