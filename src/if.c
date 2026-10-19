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

/* interne If-Routinen und die intelligent-Walk Algorythmen */

short DirX[]= {  0, 1, 1, 1, 0,-1,-1,-1 };
short DirY[]= { -1,-1, 0, 1, 1, 1, 0,-1 };

unsigned short IfWasntHere(unsigned short Adresse,unsigned short VarNum,unsigned short JumpAdresse)
{
  if (Var[VarNum]==0)
  {
    SetVar(VarNum,1);
    return(Adresse+5);
  }
  return(JumpAdresse);
}
unsigned short IfPersonIsIn(US Adresse,unsigned short JumpAdresse,short Prs,short x1,short y1,short x2, short y2)
{
  short x,y;

  Prs=GetRealPersonNum(Prs);
  x=Person[Prs].x+Person[Prs].xs/2;
  y=Person[Prs].y+Person[Prs].ys;

  if (y<y1 || y>y2 || x<x1 || x>x2) return(JumpAdresse);
  return(Adresse+13);  /* Wenn wahr ... */
}
unsigned short ParseIf(unsigned short Adresse,unsigned short JumpAdresse,short Variable,short z1,short z2,short z3,short z4)
{
  if (Var[Variable]==z1 || Var[Variable]==z2 ||
      Var[Variable]==z3 || Var[Variable]==z4) return(Adresse+13);
  return(JumpAdresse);
}
short MoasIf(short Zahl1,short Zahl2,short Mode,short Operator)
{
  if ((Mode & 2)==0) Zahl2=Var[Zahl2];
  if ((Mode & 1)==0) Zahl1=Var[Zahl1];
  else
  {
    Zahl1-=1000;
    if (Zahl1<0 || Zahl1>AnzInventar) InternalError(1031);
    if ((Zahl2>0 && Inventar[Zahl1].x==Zahl2)            ||
       (Zahl2==-50 && Inventar[Zahl1].x==-3)           ||
       (Zahl2==-51 && Inventar[Zahl1].x==CurrentProc)  ||
       (Zahl2==-52 && Inventar[Zahl1].x!=-3)           ||
       (Zahl2==-53 && Inventar[Zahl1].x!=CurrentProc)  ||
       (Zahl2==-54 && Inventar[Zahl1].x==-2)           ||
       (Zahl2==-55 && Inventar[Zahl1].x==-1)) return(1);
    return(0);
  }
  if ((Operator==-60 && Zahl1 >  Zahl2) ||
      (Operator==-61 && Zahl1 <  Zahl2) ||
      (Operator==-62 && Zahl1 == Zahl2) ||
      (Operator==-63 && Zahl1 != Zahl2) ||
      (Operator==-64 && Zahl1 >= Zahl2) ||
      (Operator==-65 && Zahl1 <= Zahl2)) return(1);
  return(0);
}
short MoasIf2(short Zahl1,short Zahl2,short Mode,short Operator,short MainMode,short Zahl3,short Zahl4,short Operator2)
{
  switch (MainMode)
  {
    case 0:if (MoasIf(Zahl1,Zahl2,Mode & 3,Operator) && MoasIf(Zahl3,Zahl4,(Mode/4)&3,Operator2)) return(1);
           break;
    case 1:if (MoasIf(Zahl1,Zahl2,Mode & 3,Operator) || MoasIf(Zahl3,Zahl4,(Mode/4)&3,Operator2)) return(1);
  }
  return(0);
}
short MoasIf3(short Zahl1,short Zahl2,short Mode,short Operator,short MainMode,short Zahl3,short Zahl4,short Operator2,short MainMode2,short Zahl5,short Zahl6,short Operator3)
{
  switch (MainMode2)
  {
    case 0:if (MoasIf2(Zahl1,Zahl2,Mode,Operator,MainMode,Zahl3,Zahl4,Operator2) && MoasIf(Zahl5,Zahl6,Mode/16,Operator3)) return(1);
           break;
    case 1:if (MoasIf2(Zahl1,Zahl2,Mode,Operator,MainMode,Zahl3,Zahl4,Operator2) || MoasIf(Zahl5,Zahl6,Mode/16,Operator3)) return(1);
  }
  return(0);
}
void ConditionalWalk(short PrsNum,char Mode,short x1,short y1,short x2,short y2,short tx,short ty)
{
  short x,y,Prs;

  Prs=GetRealPersonNum(PrsNum);
  x=Person[Prs].x+Person[Prs].xs/2;
  y=Person[Prs].y+Person[Prs].ys;
  if ((x<x1 || x>x2 || y<y1 || y>y2) ^ Mode)
  {
    WalkPerson(PrsNum,tx,ty);
    Prs=GetRealPersonNum(PrsNum);
    Person[Prs].x=Person[Prs].TargetX;
    Person[Prs].y=Person[Prs].TargetY;
  }
}
void iWalk(short PrsNum,short tx,short ty)
{
  short b,c,d,AnzILines=0,x=0,y=0,Real;
  short Pass=0,Start,End,LastAnz;
  short mtx,mty;                      /*Minor target - Zwischenziel */

  Real=GetRealPersonNum(PrsNum);
  Person[Real].iWalk=0;
  if (IsPositionOk(Real,tx,ty)==0) return;
  if (IsPositionOk(Real,Person[Real].x,Person[Real].y)==0) return;
  if (tx==Person[Real].x && ty==Person[Real].y) return;
  if (IsDirectWalkPossible(PrsNum,Person[Real].x,Person[Real].y,tx,ty))
  {
    Person[Real].TargetX=tx;
    Person[Real].TargetY=ty;
    return;
  }

  mtx=tx; mty=ty;
  for (d=0;d<10;d++)    // 10 Iteration chances
    for (c=0;c<20;c++)  // for max. 20 WalkAreas
      if (WalkArea[c].ty!=-1)
        if (WalkArea[c].x1<=mtx+Person[Real].xs/2 && WalkArea[c].y1<=mty+Person[Real].ys && WalkArea[c].x2>=mtx+Person[Real].xs/2 && WalkArea[c].y2>=mty+Person[Real].ys)
          if (Person[Real].x+Person[Real].xs/2<WalkArea[c].x1 || Person[Real].y+Person[Real].ys<WalkArea[c].y1 || Person[Real].x+Person[Real].xs/2>WalkArea[c].x2 || Person[Real].y+Person[Real].ys>WalkArea[c].y2)
            if (WalkArea[c].tx!=Person[Real].x+Person[Real].xs/2 || WalkArea[c].ty!=Person[Real].y+Person[Real].ys)
  //        if (mtx!=WalkArea[c].tx-Person[Real].xs/2 || mty!=WalkArea[c].ty-Person[Real].ys)
  //        if (IsDirectWalkPossible(PrsNum,Person[Real].x,Person[Real].y,WalkArea[c].tx-Person[Real].xs/2,WalkArea[c].ty-Person[Real].ys)==0)
            {
              mtx=WalkArea[c].tx-Person[Real].xs/2;
              mty=WalkArea[c].ty-Person[Real].ys;
              break;
            }

  while (1)
  {
    if (Pass==0)
    {
      Start=-3;
      End=-1;
    }
    else
    {
      Start=LastAnz;
      End=AnzILines;
    }
    LastAnz=AnzILines;
    for (b=Start;b<End;b++)
    {
      if (b==-3) { x=Person[Real].x; y=Person[Real].y; }
      if (b==-2) { x=mtx; y=mty; }
      if (b>=0)  { x=iLine[b].x2;  y=iLine[b].y2; }

      for (c=0;c<8;c++)
        if (Pass==0 || (c!=iLine[b].Dir && abs(c-iLine[b].Dir)!=4))
        {
          for (d=0;;d+=Person[Real].StepSpeed)
          {
            if (IsPositionOk(Real,x+DirX[c]*d,y+DirY[c]*d)==0)
              break;
            if (x+DirX[c]*d+Person[Real].xs/2<0 ||
                x+DirX[c]*d+Person[Real].xs/2>319 ||
                y+DirY[c]*d+Person[Real].ys<0 ||
                y+DirY[c]*d+Person[Real].ys>199) break;
          }
          d-=Person[Real].StepSpeed;
          if (d>1)
          {
//          _setcolor(0);
  //        _moveto(x+Person[Real].xs/2,y+Person[Real].ys);
    //      _lineto(x+DirX[c]*d+Person[Real].xs/2,y+DirY[c]*d+Person[Real].ys);
            iLine[AnzILines].x1=x;
            iLine[AnzILines].y1=y;
            iLine[AnzILines].x2=x+DirX[c]*d;
            iLine[AnzILines].y2=y+DirY[c]*d;
            iLine[AnzILines].Dir=c;
            if (b==-3) iLine[AnzILines].Side=0;
            else if (b==-2) iLine[AnzILines].Side=1;
            else if (b>=0)  iLine[AnzILines].Side=iLine[b].Side;
            if (b<0) iLine[AnzILines].Parent=-1;
                else iLine[AnzILines].Parent=b;

            for (d=0;d<AnzILines;d++)
              if (iLine[d].Side!=iLine[AnzILines].Side)
                if (IsCrossed(iLine[AnzILines],iLine[d]))
                {
                  short mul;

                  if (iLine[AnzILines].Side==1)
                  {
                    b=d; d=AnzILines; AnzILines=b;
                  }
                  if (abs(iLine[AnzILines].Dir-iLine[d].Dir)==4 || iLine[AnzILines].Dir==iLine[d].Dir)
                    mul=0;
                  else
                    mul=(iLine[d].x1*DirY[iLine[d].Dir]-iLine[d].y1*DirX[iLine[d].Dir]-
                         iLine[AnzILines].x1*DirY[iLine[d].Dir]+iLine[AnzILines].y1*DirX[iLine[d].Dir])
                        /(DirX[iLine[AnzILines].Dir]*DirY[iLine[d].Dir]-DirY[iLine[AnzILines].Dir]*DirX[iLine[d].Dir]);
                  MacheLaufReihe(AnzILines,d,iLine[AnzILines].x1+mul*DirX[iLine[AnzILines].Dir],iLine[AnzILines].y1+mul*DirY[iLine[AnzILines].Dir]);
                  KompliziereLaufReihe();
                  VereinfacheLaufReihe(PrsNum);
                  Person[Real].TargetX=LaufReihe[1].x;
                  Person[Real].TargetY=LaufReihe[1].y;
                  Person[Real].gtx=tx;
                  Person[Real].gty=ty;
                  Person[Real].iWalk=1;
                  return;
                }

            AnzILines++;
            if (AnzILines>=599)
            {
              V_Par1=Person[Real].Num;
              V_Verb=M_STUPID;
              ExecuteTheProgram();
              return;
            }
          }
        }
    }
    Pass++;
  }
}
short IsCrossed(struct I_LINE a,struct I_LINE b)
{
  long s1,s2;

  if (min(a.x1,a.x2)>max(b.x1,b.x2) ||
      min(a.y1,a.y2)>max(b.y1,b.y2) ||
      min(b.x1,b.x2)>max(a.x1,a.x2) ||
      min(b.y1,b.y2)>max(a.y1,a.y2)) return(0);
  if (abs(a.Dir-b.Dir)==4 || a.Dir==b.Dir)
  {
    if (((a.Dir==0 || a.Dir==4) && a.x1==b.x1) ||
        ((a.Dir==2 || a.Dir==6) && a.y1==b.y1) ||
        ((a.Dir==3 || a.Dir==7) && a.y1-b.y1==a.x1-b.x1) ||
        ((a.Dir==1 || a.Dir==5) && a.y1-b.y1==b.x1-a.x1))
    {
      if (a.x1!=a.x2)
        if ((b.x1>=a.x1 && b.x1<=a.x2) || (b.x1>=a.x2 && b.x1<=a.x1) || (b.x2>=a.x1 && b.x2<=a.x2) || (b.x2>=a.x2 && b.x2<=a.x1))
          return(1);
      if (a.y1!=a.y2)
        if ((b.y1>=a.y1 && b.y1<=a.y2) || (b.y1>=a.y2 && b.y1<=a.y1) || (b.y2>=a.y1 && b.y2<=a.y2) || (b.y2>=a.y2 && b.y2<=a.y1))
          return(1);

      return(0);
    }
  }
  a.x1>>=1; a.x2>>=1; b.x1>>=1; b.x2>>=1;
  a.y1>>=1; a.y2>>=1; b.y1>>=1; b.y2>>=1;

  s1=(a.x1-a.x2)*(a.y1-b.y1)-(a.y1-a.y2)*(a.x1-b.x1);
  s2=(a.x1-a.x2)*(a.y1-b.y2)-(a.y1-a.y2)*(a.x1-b.x2);
  if ((s1>0 && s2>0) || (s1<0 && s2<0)) return(0);
  s1=(b.x1-b.x2)*(b.y1-a.y1)-(b.y1-b.y2)*(b.x1-a.x1);
  s2=(b.x1-b.x2)*(b.y1-a.y2)-(b.y1-b.y2)*(b.x1-a.x2);
  if ((s1>0 && s2>0) || (s1<0 && s2<0)) return(0);
  return(1);
}
short IsDirectWalkPossible(short PrsNum,short sx,short sy,short tx,short ty)
{
  short Real,Speed,Old,Advance;

  Real=GetRealPersonNum(PrsNum);
  Speed=Person[Real].StepSpeed;
  while (sx!=tx || sy!=ty)
  {
    Advance=0;

    if (sx!=tx)
    {
      Old=sx;
      if (abs(sx-tx)<Speed) tx=sx;
      else if (sx<tx) sx+=Speed;
                 else sx-=Speed;
      if (IsPositionOk(Real,sx,sy)) Advance=1; else sx=Old;
    }

    if (sy!=ty)
    {
      Old=sy;
      if (abs(sy-ty)<Speed) ty=sy;
      else if (sy<ty) sy+=Speed;
                 else sy-=Speed;
      if (IsPositionOk(Real,sx,sy)) Advance=1; else sy=Old;
    }
    if (Advance==0) return(0);
  }
  return(1);
}
void InsertInLaufReihe(short x,short y)
{
  short c;

  for (c=28;c>=0;c--)
    LaufReihe[c+1]=LaufReihe[c];
  LaufReihe[0].x=x;
  LaufReihe[0].y=y;
}
void MacheLaufReihe(short Side0,short Side1,short sx,short sy)
{
  short c;

  LaufReihe[0].x=-1;
  InsertInLaufReihe(sx,sy);
  while (1)
  {
    InsertInLaufReihe(iLine[Side0].x1,iLine[Side0].y1);
    if (iLine[Side0].Parent!=-1) Side0=iLine[Side0].Parent;
                            else break;
  }
  for (c=0;c<30;c++) if (LaufReihe[c].x==-1) break;
  while (1)
  {
    LaufReihe[c].x=iLine[Side1].x1;
    LaufReihe[c].y=iLine[Side1].y1; c++;
    if (iLine[Side1].Parent!=-1) Side1=iLine[Side1].Parent;
                            else break;
  }
  LaufReihe[c].x=-1;
}
void VereinfacheLaufReihe(short PrsNum)
{
  short c,d,DidSomething=1;
  short x,y,Step=5;

  while (DidSomething==1 || Step==5)
  {
    if (DidSomething==0) Step=1;
    DidSomething=0;
    for (c=0;c<30;c++)
    {
      if (LaufReihe[c].x==-1 || LaufReihe[c+1].x==-1 || LaufReihe[c+2].x==-1)
        break;
      if (IsDirectWalkPossible(PrsNum,LaufReihe[c].x,LaufReihe[c].y,LaufReihe[c+2].x,LaufReihe[c+2].y)==1)
      {
        for (d=c+1;d<29;d++)
          LaufReihe[d]=LaufReihe[d+1];
        DidSomething=1;
        c=-1;
      }
    }
    for (c=0;c<30;c++)
    {
      if (LaufReihe[c].x==-1 || LaufReihe[c+1].x==-1 || LaufReihe[c+2].x==-1)
        break;
      x=(LaufReihe[c].x+LaufReihe[c+2].x)/2;
      y=(LaufReihe[c].y+LaufReihe[c+2].y)/2;
      while  (LaufReihe[c+1].x-Step>=x && IsDirectWalkPossible(PrsNum,LaufReihe[c].x,LaufReihe[c].y,LaufReihe[c+1].x-Step,LaufReihe[c+1].y)==1
                                       && IsDirectWalkPossible(PrsNum,LaufReihe[c+1].x-Step,LaufReihe[c+1].y,LaufReihe[c+2].x,LaufReihe[c+2].y)==1)
      { LaufReihe[c+1].x-=Step; DidSomething=1; }
      while  (LaufReihe[c+1].x+Step<=x && IsDirectWalkPossible(PrsNum,LaufReihe[c].x,LaufReihe[c].y,LaufReihe[c+1].x+Step,LaufReihe[c+1].y)==1
                                       && IsDirectWalkPossible(PrsNum,LaufReihe[c+1].x+Step,LaufReihe[c+1].y,LaufReihe[c+2].x,LaufReihe[c+2].y)==1)
      { LaufReihe[c+1].x+=Step; DidSomething=1; }
      while  (LaufReihe[c+1].y-Step>=y && IsDirectWalkPossible(PrsNum,LaufReihe[c].x,LaufReihe[c].y,LaufReihe[c+1].x,LaufReihe[c+1].y-Step)==1
                                       && IsDirectWalkPossible(PrsNum,LaufReihe[c+1].x,LaufReihe[c+1].y-Step,LaufReihe[c+2].x,LaufReihe[c+2].y)==1)
      { LaufReihe[c+1].y-=Step; DidSomething=1; }
      while  (LaufReihe[c+1].y+Step<=y && IsDirectWalkPossible(PrsNum,LaufReihe[c].x,LaufReihe[c].y,LaufReihe[c+1].x,LaufReihe[c+1].y+Step)==1
                                       && IsDirectWalkPossible(PrsNum,LaufReihe[c+1].x,LaufReihe[c+1].y+Step,LaufReihe[c+2].x,LaufReihe[c+2].y)==1)
      { LaufReihe[c+1].y+=Step; DidSomething=1; }
    }
  }
}
void KompliziereLaufReihe(void)
{
  short c,d,l;

  for (c=1;c<30;c++)
  {
    if (LaufReihe[c].x==-1) break;
    for (l=0;l<30;l++)
      if (LaufReihe[l].x==-1) break;
    if (l>=29) break;
    for (d=29;d>c;d--)
      LaufReihe[d]=LaufReihe[d-1];
    LaufReihe[c].x=(LaufReihe[c-1].x+LaufReihe[c+1].x)/2;
    LaufReihe[c].y=(LaufReihe[c-1].y+LaufReihe[c+1].y)/2;
    c++;
  }
}

