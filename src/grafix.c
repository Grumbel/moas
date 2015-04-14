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

struct PACK
{
  UCHAR Code,Real;
} *Code;

struct ABK
{
  signed short xp1,yp1,xp2,yp2;
  uChar Token;
} Abk[]=
{
   1,-1,+1,-1, 248,                     /* x+1 y-1       x+1 y-1  */
   0,-2, 0,-2, 249,                     /*     y-2           y-2  */
   0,-1, 0,-1, 250,                     /*     y-1           y-1  */
   0, 0, 0, 0, 0                        /* Token=0 ==> Last       */
};

void UniversalUnpack(UCHAR *Source,UCHAR *Target,long Size,UCHAR Glass,long xs)
{
  register long c,f,tCnt;
  register uChar e,Real;
  register uChar *StdSource;
  short CodesUsed[258];

  Source+=(e=PreUnpack(Source,CodesUsed));   Size-=e;
  StdSource=Target;
  tCnt=0;

  for (c=0;c<Size;c++,Source++,tCnt++)
  {
    if (*Source>=248)
    {
      Source++,c++;   f=(*((USHORT *)Source));
      switch(Source[-1])
      {
        case 251:memcpy(Target+tCnt,Target+tCnt-(f&2047)-16-(f>>11),(f>>11)+16);
                 tCnt+=(f>>11)+16-1; Source++;   c++;   break;
        case 252:memcpy(Target+tCnt,Target+tCnt-(f&16383)-12-(f>>14),(f>>14)+12);
                 tCnt+=(f>>14)+12-1; Source++;   c++;   break;
        case 253:memcpy(Target+tCnt,Target+tCnt-(f&16383)-8-(f>>14),(f>>14)+8);
                 tCnt+=(f>>14)+8-1;  Source++;   c++;   break;
        case 254:memcpy(Target+tCnt,Target+tCnt-(f&16383)-4-(f>>14),(f>>14)+4);
                 tCnt+=(f>>14)+4-1;  Source++;   c++;   break;
        case 255:memcpy(Target+tCnt,StdSource+f,Source[2]);
                 tCnt+=Source[2]-1;  Source+=2;  c+=2;  break;
        case 250:
        case 249:
        case 248:Target[0+tCnt]=Target[tCnt+xs*Abk[((short)Source[-1])-248].yp1+Abk[((short)Source[-1])-248].xp1];
                 Target[1+tCnt]=Target[tCnt+xs*Abk[((short)Source[-1])-248].yp2+Abk[((short)Source[-1])-248].xp2+1];
                 tCnt++;
                 Source--; c--;                                          break;
      }
    }
    else if (CodesUsed[(short)*Source]!=-1)
         {
           Real=CodesUsed[(short)(*Source)];
           Source++,c++;
           if (Real!=Glass)
           {
             if (*Source!=255) memset(Target+tCnt,Real,*Source+3);
               else memset(Target+tCnt,Real,513+3);
           }
           if (*Source!=255) tCnt+=*Source+2;
           else tCnt+=513+2;
         }
    else
    {
      if (*Source!=Glass) Target[tCnt]=*Source;
    }
  }
}
void UnpackPicture(UCHAR *Source,short y,USHORT Size)
{
  if (y<=200)
    UniversalUnpack(Source,Screen+Tab320[y],Size,0,320);
  else InternalError(1004);
}
short PreUnpack(char *Data,short *CodesUsed)                                    /*<QW>*/
{
  short d;
  short AnzCodes;

  AnzCodes=*((unsigned char *)Data);
  Code=(struct PACK*)(Data+1);

  for (d=0;d<256;d++)
    CodesUsed[d]=-1;
  for (d=0;d<AnzCodes;d++)
    CodesUsed[(short)Code[d].Code]=Code[d].Real;
  return(AnzCodes*2+1);
}
void PutCharAt(short CharNum,char *Vga,char FontNum,char c1,char c2,short VgaXs)
{
  register short x,y,xs,SizeY;
  register UCHAR *p,pp;

  xs=Font[FontNum].Width[CharNum];
  p =Font[FontNum].Data+Font[FontNum].Begin[CharNum];
  SizeY=Font[FontNum].SizeY;

  for (y=0;y<SizeY;y++,Vga+=VgaXs)
  {
    if (y==4 || y==8) p++;
    for (x=0;x<xs;x++)
    {
      pp=(p[x+x+x]<<((y&3)<<1))&192;
      if (pp == 128) Vga[x]=c2;
      else if (pp == 64) Vga[x]=c1;
    }
  }
}
short GetCursorPosInWindow(void)
{
  short c;

  if (Mouse.x>=Window.x1 && Mouse.x<Window.x2)
    for (c=0;c<12;c++)
      if (RefTable[c].Num!=-1 && RefTable[c].y1<=Mouse.y && RefTable[c].y2>=Mouse.y)
        return(c);
  return(-1);
}
void UpDatePersonMemory(void)
{
  short c;

  for (c=0;c<AnzPhasen;c++) Phase[c].InUse=0;
  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1)
      Phase[Person[c].PhaseNum].InUse=1;
}
void PowerRedrawArea(short x1,short y1,short x2,short y2)                       /*<QW>*/
{
  register long x,y;
  static long c,X1,Y1,X2,Y2,xs,ys,YFootPos,Add1,Add2,WinXs;
  static UCHAR *PrsData,*Back,*Info,*Tmp,GhostNotFlag,*LocalPaletten,*LocalPaletten2;

  if (V_LockVga || KeyAbortState==2) return;

  if (x1<0) x1=0; else if (x1>319) x1=319;
  if (x2<0) x2=0; else if (x2>319) x2=319;
  if (y2<0) y2=0; else if (y2>199) y2=199;
  if (y1<0) y1=0; else if (y1>199) y1=199;
  if (x1>x2 || y1>y2) return;

  WinXs=x2-x1+1;
  if (WinXs*(unsigned short)(y2-y1+2)>6000)
  {
    PowerRedrawArea(x1,(y1+y2)>>1,x2,y2);
    PowerRedrawArea(x1,y1,x2,(y1+y2)>>1);
    return;
  }
  Back=Screen+x1+Tab320[y1];

  for (Tmp=PRedrawTmpBak,y=y2-y1+1;y;y--,Back+=320,Tmp+=WinXs) memcpy(Tmp,Back,WinXs);

  for (c=0;c<MAX_PRS;c++)
  {
    if (Person[c].Num==-1) break;

    xs=Person[c].xs;
    ys=Person[c].ys;
    X1=max(x1,Person[c].x);   X2=min(x2,Person[c].x+xs-1);
    Y1=max(y1,Person[c].y);   Y2=min(y2+1,Person[c].y+ys);
    if (X2<X1 || Y2<Y1) continue;
    PrsData=Person[c].Phase+Person[c].Sequence[Person[c].Counter][Person[c].Dir]*xs*ys;
    Tmp=PRedrawTmpBak;
    if (Person[c].y<y1) PrsData+=(y1-Person[c].y)*xs;
    else if (Person[c].y>y1) Tmp+=(Person[c].y-y1)*WinXs;
    YFootPos=Person[c].y+ys+Person[c].z;
    if (YFootPos<0) continue;
    Info=Info1+X1+Tab320[Y1]-1;
    GhostNotFlag=!(MegaTable && Person[c].Bits & 32768);
    LocalPaletten=Paletten+Person[c].GhostLevel*768;
    LocalPaletten2=Paletten+(120-Person[c].GhostLevel)*768;
    if (Person[c].x>x1) Tmp+=(Add1=(Person[c].x-x1)); else Add1=0;
    if (Person[c].x<x1) PrsData+=(Add2=(x1-Person[c].x)); else Add2=0;
    if (Person[c].x+xs-1>x2) Add2+=(Person[c].x+xs-x2-1);
    if (Person[c].x+xs-1<x2) Add1+=(x2-Person[c].x-xs+1);

    X2-=(X1-1);
    Add2+=X2;

    if (Person[c].Bits & 64)
      for (y=Y2-Y1;y;y--)
      {
        memcpy(Tmp,PrsData,X2);
        PrsData+=Add2; Tmp+=WinXs;
      }
    else
    {
      Tmp--;
      PrsData--;

      if (GhostNotFlag)
        if (YFootPos>=256)
          for (y=Y2-Y1;y;y--)
          {
            for (x=X2;x;x--)
              if (PrsData[x]) Tmp[x]=PrsData[x];
            PrsData+=Add2; Tmp+=WinXs;
          }
        else
          for (y=Y2-Y1;y;y--)
          {
            for (x=X2;x;x--)
              if (PrsData[x] && ((uChar)YFootPos)>Info[x])
                Tmp[x]=PrsData[x];
            PrsData+=Add2; Tmp+=WinXs;  Info+=320;
          }
      else
        for (y=Y2-Y1;y;y--)
        {
          for (x=X2;x;x--)
            if (PrsData[x] && ((uChar)YFootPos)>Info[x])
            //if (PrsData[x] && YFootPos<Info[x]) //Scheint mir 'n Bug zu sein!
              Tmp[x]=MegaTable[LocalPaletten2[PrsData[x]]+LocalPaletten[Tmp[x]] + (((US)(LocalPaletten2[PrsData[x]+256]+LocalPaletten[Tmp[x]+256]))<<4) + (((US)(LocalPaletten2[PrsData[x]+512]+LocalPaletten[Tmp[x]+512]))<<8)];
          PrsData+=Add2; Tmp+=WinXs;  Info+=320;
        }
    }
  }

  for (c=0;c<AnzInventar;c++)
    if (Inventar[c].x==-3 && V_InventarState)
    {
      xs=(Inventar[c].y%10)*32;
      ys=(Inventar[c].y/10)*24;
      Tmp=PRedrawTmpBak; PrsData=Inventar[c].Data;
      X1=min(x2,xs+31)-max(x1,xs)+1; Y2=min(y2+1,ys+24)-max(y1,ys);
      if (ys<y1) PrsData+=(y1-ys)*32;
      else if (ys>y1) Tmp+=(ys-y1)*WinXs;

      Add2=0;
      if (xs>x1) Tmp+=(xs-x1);
      else if (xs<x1) PrsData+=(Add2=(x1-xs));
      if (xs+31>x2) Add2+=(xs+31-x2);

      Add2+=X1;
      if (X1>0 && Y2>0)
      {
        Tmp--;  PrsData--;
        for (y=Y2;y;y--)
        {
          for (x=X1; x; x--)
            if (PrsData[x]) Tmp[x]=PrsData[x];
          //memcpy(Tmp,PrsData,X1);
          Tmp+=WinXs;  PrsData+=32;
        }
      }
    }
  for (c=0;c<5;c++)
    if (SayWindow[c].RefPrs!=-1)
    {
      Tmp=PRedrawTmpBak-1;
      xs=SayWindow[c].xs;
      ys=SayWindow[c].ys;
      PrsData=SayWindow[c].Data;
      X1=min(x2,SayWindow[c].x+xs-1)-max(x1,SayWindow[c].x)+1;
      if (SayWindow[c].y<y1) PrsData+=(y1-SayWindow[c].y)*xs;
      else if (SayWindow[c].y>y1) Tmp+=(SayWindow[c].y-y1)*WinXs;
      if (SayWindow[c].x>x1) Tmp+=SayWindow[c].x-x1;
      if (SayWindow[c].x<x1) Add2=x1-SayWindow[c].x; else Add2=0;
      PrsData+=Add2-1;
      if (SayWindow[c].x+xs-1>x2) Add2+=(SayWindow[c].x+xs-x2-1);
      Y2=min(y2+1,SayWindow[c].y+ys) - max(y1,SayWindow[c].y);
      Add2+=X1;
      if (Y2>0 && X1>0)
        for (y=Y2;y;y--)
        {
          for (x=X1;x;x--)
            if (PrsData[x]) Tmp[x]=PrsData[x];
          PrsData+=Add2; Tmp+=WinXs;
        }
    }

  X1=max(x1,1);   X2=min(x2,SmallTextLineLength)-X1+1;  Y2=min(y2+1,198)-max(y1,189);
  if (Y2>0 && X2>0)
  {
    Tmp=PRedrawTmpBak+X1-x1; PrsData=SmallTextLine+X1;
    if (189<y1)      PrsData+=(y1-189)*340;
    else if (189>y1) Tmp+=(189-y1)*WinXs;
    for (y=Y2;y;y--)
    {
      memcpy(Tmp,PrsData,X2);
      Tmp+=WinXs;
      PrsData+=340;
    }
  }

  if (Mouse.CursorState==1 && V_CursorState==1 && Mouse.BewegeMode==-1)
  {
    X1=max(x1,Mouse.x); X2=min(x2,Mouse.x+43)-X1+1; Y2=min(y2+1,Mouse.y+40)-max(y1,Mouse.y);
    Tmp=PRedrawTmpBak+X1-x1-1;
    if (PlayerHasControl) PrsData=Phase[0].Data+X1-Mouse.x-1+1760*(short)ThingAtCursor;
                     else PrsData=Phase[0].Data+X1-Mouse.x-1+3520;
    if (Mouse.y<y1) PrsData+=(y1-Mouse.y)*44;
    else if (Mouse.y>y1) Tmp+=(Mouse.y-y1)*WinXs;
    if (Y2>0 && X2>0)
      for (y=Y2;y;y--)
      {
        for (x=X2;x;x--)
          if (PrsData[x]) Tmp[x]=PrsData[x];
        Tmp+=WinXs;
        PrsData+=44;
      }
  }

  if (Mouse.CursorState==1 && V_CursorState==1 && Mouse.BewegeMode!=-1)
  {
    X1=max(x1,Mouse.x-15); X2=min(x2,Mouse.x+14)-X1+1; Y2=min(y2+1,Mouse.y+15)-max(y1,Mouse.y-15);
    Tmp=PRedrawTmpBak+X1-x1-1;
    PrsData=Phase[1].Data+X1-Mouse.x+15-1+900*(short)Mouse.BewegeMode;
    if (Mouse.y-15<y1) PrsData+=(y1-(Mouse.y-15))*30;
    else if (Mouse.y-15>y1) Tmp+=(Mouse.y-15-y1)*WinXs;
    if (Y2>0 && X2>0)
      for (y=Y2;y;y--)
      {
        for (x=X2;x;x--)
          if (PrsData[x]) Tmp[x]=PrsData[x];
        Tmp+=WinXs;
        PrsData+=30;
      }
  }

  Back=(unsigned char *)VGA_ADRESSE+x1+Tab320[y1];
  for (Tmp=PRedrawTmpBak,y=y2-y1+1;y;y--,Back+=320,Tmp+=WinXs)
    memcpy(Back,Tmp,WinXs);
}
void Shake(short Milliseconds,short Staerke)
{
#ifdef DOS
  register unsigned short x,y;
  USHORT LastTime;
  union REGS Regs;

  while (Milliseconds>0)
  {
    Regs.h.ah=44;
    int386(0x21,&Regs,&Regs);
    if ((Regs.h.dl/10)!=LastTime)
      Milliseconds--;
    LastTime=(Regs.h.dl/10);

    x=rand()%(Staerke<<1);y=rand()%(Staerke<<1);
    outp(0x3d4,13); outp(0x3d5,(x+y*80)&255);
    outp(0x3d4,12); outp(0x3d5,(x+y*80)>>8);
  }
  outp(0x3d4,13); outp(0x3d5,0);
  outp(0x3d4,12); outp(0x3d5,0);
#endif
}

void QuickSetSinglePal(short Color,short r,short g,short b)
{
   short tmp;

   if (V_Colored==0) r=g=b=(r+g+b)/3;

   outp(0x3c8,Color);

   tmp=r*DayLightR/100;  if (tmp>63) tmp=63;  outp(0x3c9,tmp);
   tmp=g*DayLightG/100;  if (tmp>63) tmp=63;  outp(0x3c9,tmp);
   tmp=b*DayLightB/100;  if (tmp>63) tmp=63;  outp(0x3c9,tmp);
}
void QuickSetPalette(char *Pal)
{
  short c;

  for (c=0;c<256;c++,Pal+=3)
    QuickSetSinglePal(c,Pal[0],Pal[1],Pal[2]);
}
void Blend(char Step,char Dir)
{
  register short c,d,r,g,b;
  char *Back,*VgaPal;

  Back=MALLOC(768*2); VgaPal=Back+768;
  for (c=0;c<768;c+=3)
  {
    r=Palette[c];
    g=Palette[c+1];
    b=Palette[c+2];
    if (V_Colored==0) r=g=b=(r+g+b)/3;

    Back[c]  =r*DayLightR/100;
    Back[c+1]=g*DayLightG/100;
    Back[c+2]=b*DayLightB/100;
  }
                            /* ==>+<== */
  if (Eingeblendet=Dir)     /* Wird VgaPal noch sonst wo verwendet ? */
  {                         /* Kann man Sie nicht auch lokal managen ? */
    memset(VgaPal,0,768);
    for (c=Step;c<=64;c+=Step,kbhit())
    {
      for (d=0;d<768;d++) VgaPal[d]=(Back[d]*c)>>6;
      AsmQPalSet(VgaPal);
    }
    DontCyclePal=0;
  }
  else
  {
    DontCyclePal=1;
    for (c=64-Step;c>=0;c-=Step,kbhit())
    {
      for (d=0;d<768;d++) VgaPal[d]=(Back[d]*c)>>6;
      AsmQPalSet(VgaPal);
    }
  }
  FREE(Back);
}
void ToggleScreen(short Mode)
{
  USHORT Tmp;

  outp(0x3c4,0x01); Tmp=inp(0x3c5);

  if (Mode==1) Tmp=Tmp & 223;
     else      Tmp=Tmp | 32;
  outp(0x3c4,0x01); outp(0x3c5,Tmp);
}
void LoescheKopie(short PrsNum)
{
  register short x,y;

  if (Person[PrsNum].Phase!=Phase[Person[PrsNum].PhaseNum].Data)
    for (x=0;x<8;x++)
      for (y=0;y<12;y++)
        if (Person[PrsNum].Sequence[x][y]!=-1)
          Person[PrsNum].Phase[Person[PrsNum].Sequence[x][y]*Person[PrsNum].xs*Person[PrsNum].ys]=255;
}
void CheckKopie(short PrsNum)
{
  register long xs,ys,cx,cy,nxs,nys,x,y,a,b,c,d;
  unsigned char *Prs,*Real,*LocalPaletten,*LocalPaletten2;

  x=Person[PrsNum].Counter;
  y=Person[PrsNum].Dir;

  xs=Person[PrsNum].xs;
  ys=Person[PrsNum].ys;
  if (Person[PrsNum].Phase==Phase[Person[PrsNum].PhaseNum].Data) return;
  if (Person[PrsNum].Phase[Person[PrsNum].Sequence[Person[PrsNum].Counter][Person[PrsNum].Dir]*xs*ys]!=255) return;

  if (Person[PrsNum].Bits & 1024)
  {
    if (Person[PrsNum].Bits & 128)
    {
      nxs=xs*(128l-V_PersonSize)/128;
      nys=ys*(128l-V_PersonSize)/128;
    }
    else
    {
      nxs=xs*(128l-Person[PrsNum].Boden)/128;
      nys=ys*(128l-Person[PrsNum].Boden)/128;
    }

    Real=Phase[Person[PrsNum].PhaseNum].Data+Person[PrsNum].Sequence[x][y]*xs*ys;
    Prs=Person[PrsNum].Phase+Person[PrsNum].Sequence[x][y]*xs*ys;
    memset(Prs,0,xs*ys);
    Prs+=(xs-nxs)/2+(ys-nys)*xs;

    a=xs; b=nxs; c=ys; d=nys;
    for (cy=0;cy<nys;cy++)
    {
      for (cx=0;cx<nxs;cx++)
        Prs[cx]=Real[cx*a/b];
      Prs+=xs;
      Real+=(cy+1)*c/d*xs-cy*c/d*xs;
    }
    Prs-=xs;
    if (Prs>Person[PrsNum].Phase+(Person[PrsNum].Sequence[x][y]+1)*xs*ys)
      InternalError(3002);
  } else if (MegaTable && (Person[PrsNum].Bits & 768))
         {
           memcpy(Person[PrsNum].Phase+Person[PrsNum].Sequence[x][y]*xs*ys,
                  Phase[Person[PrsNum].PhaseNum].Data+Person[PrsNum].Sequence[x][y]*xs*ys,
                  xs*ys);
         }

  if (MegaTable && (Person[PrsNum].Bits & 768))
  {
    LocalPaletten=Paletten+Person[PrsNum].Boden*768;
    LocalPaletten2=Paletten+(128-Person[PrsNum].Boden)*768;

    Prs=Person[PrsNum].Phase+Person[PrsNum].Sequence[x][y]*xs*ys;
    if ((Person[PrsNum].Bits & 768)==256)
    {
      for (c=cy=0;cy<ys;cy++)
        for (cx=0;cx<xs;cx++,c++)
          if (Prs[c]) Prs[c]=MegaTable[LocalPaletten2[Prs[c]] + (((US)LocalPaletten2[Prs[c]+256])<<4) + (((US)LocalPaletten2[Prs[c]+512])<<8)];
    }
    if ((Person[PrsNum].Bits & 768)==512)
    {
      for (c=cy=0;cy<ys;cy++)
        for (cx=0;cx<xs;cx++,c++)
          if (Prs[c]) Prs[c]=MegaTable[LocalPaletten2[Prs[c]]+LocalPaletten[14] + (((US)(LocalPaletten2[Prs[c]+256]+LocalPaletten[270]))<<4) + (((US)(LocalPaletten2[Prs[c]+512]+LocalPaletten[526]))<<8)];
    }
  }
}
