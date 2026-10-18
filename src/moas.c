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
#include "debug.h"
#include "datapath.h"

char *__Benutze   ="BENUTZE %s";
char *__BenutzeMit="BENUTZE %s MIT %s";
char *__Siehe     ="SCHAU %s AN";
char *__Sprich    ="REDE MIT %s";
char *__Bewege    ="BEWEGE %s";
char *__BewegeMit ="BEWEGE %s IN RICHTUNG %s";

void AddWalkArea(short x1,short y1,short x2,short y2,short tx,short ty);

int main(int argc, char **argv)
{
//MallocFile=open("MrMalloc.Dok",O_TRUNC|O_CREAT|O_WRONLY|O_BINARY,S_IWRITE);
#ifndef DOS
  MoasParseArgs(argc, argv);
  MoasInitDataPath(argv && argv[0] ? argv[0] : "moas");
#else
  (void)argc; (void)argv;
#endif

  DontCyclePal=1;
  InitAll();
  DontCyclePal=0;
#ifndef DOS
  PlatformPresent();
#endif
  if (!moas_skip_intro) {
    MoasDbg(MOAS_DBG_SCRIPT, "Execute intro proc=3 addr=0");
    Execute(3, 0);
  } else {
    MoasDbg(MOAS_DBG_SCRIPT, "skipping intro (--skip-intro)");
  }
//ChkAuthorisation();
  {
    short proc = (moas_start_proc >= 0) ? (short)moas_start_proc : 0;
    unsigned short addr = (unsigned short)moas_start_addr;
    MoasDbg(MOAS_DBG_SCRIPT, "Execute main proc=%d addr=%u", (int)proc, (unsigned)addr);
    Execute(proc, addr);
  }
#ifndef DOS
  while (!PlatformShouldQuit()) {
    /* MouseCheck pumps events, presents, and delays on Linux. */
    MouseCheck();
  }
  PlatformShutdown();
#else
  while (1) {
    MouseCheck();
  }
#endif
  return 0;
}
short Execute(short ProcNum,unsigned short Adresse)
{
  unsigned char *Prg;
  short *Parameter;
  char LastControl=PlayerHasControl;

  PlayerHasControl=0;
  LadeProgramm(ProcNum);       Prg=Programm[ProcNum].Data;

  while (1)
  {
    if (Programm[ProcNum].Data==0) { LadeProgramm(ProcNum); Prg=Programm[ProcNum].Data; }
    if (Adresse>=Programm[ProcNum].Length)
    {
      PlayerHasControl=LastControl;
      return(0);
    }
    Parameter=(short*)(Prg+1+Adresse);
    if (moas_debug & MOAS_DBG_SCRIPT) {
      unsigned char op = Prg[Adresse];
      /* Log wait-ish / control ops at higher visibility; others only every op */
      if (op == 4 || op == 14 || op == 7 || op == 10 || op == 48 || op == 50 ||
          op == 60 || op == 61 || op == 79 || op == 23 || op == 29)
        MoasDbg(MOAS_DBG_SCRIPT, "proc=%d addr=%u op=%u p0=%d p1=%d",
                (int)ProcNum, (unsigned)Adresse, (unsigned)op,
                (int)Parameter[0], (int)Parameter[1]);
      else
        MoasDbg(MOAS_DBG_SCRIPT, "proc=%d addr=%u op=%u",
                (int)ProcNum, (unsigned)Adresse, (unsigned)op);
    }
    switch(Prg[Adresse])
    {
      case 0:SetVar(Parameter[0],Var[Parameter[0]]+Parameter[1]);
             Adresse+=5;                                                 break;
      case 1:SetVar(Parameter[0],Var[Parameter[0]]+Var[Parameter[1]]);
             Adresse+=5;                                                 break;
      case 2:AddPerson(Parameter[0],Prg[15+Adresse],Parameter[1],Parameter[2],
                       Parameter[3],Parameter[4],Parameter[5],Parameter[6]);
             Adresse+=16;                                                break;
      case 4:GetClickOrEnter(Parameter[0]);
             CloseWindow();
             Adresse+=3;                                                 break;
      case 6:if (Parameter[0]==0) TeakEffekt();
             Adresse+=3;                                                 break;
      case 7:CloseWindow();
             Adresse++;                                                  break;
      case 10:if (KeyAbortState==2)
              {
                KeyAbortState=0;
                PowerRedrawArea(0,0,319,199);
              } else KeyAbortState=0;       Adresse++;                   break;
/*    case 11:PowerBlend(Parameter[0]);
              Adresse+=3;                                                break;
*/    case 12:SayWait(Parameter[0]);
              Adresse+=3;                                                break;
      case 13:ConditionalWalk(Parameter[0],Parameter[1],Parameter[2],Parameter[3],Parameter[4],Parameter[5],Parameter[6],Parameter[7]);
              Adresse+=17;                                               break;
      case 14:GetClickOrEnter(Parameter[0]);
              Adresse+=3;                                                break;
      case 15:SetVar(Parameter[1],Person[GetRealPersonNum(Parameter[0])].x+Person[GetRealPersonNum(Parameter[0])].xs/2);
              Adresse+=5;                                                break;
      case 16:SetVar(Parameter[1],Person[GetRealPersonNum(Parameter[0])].y+Person[GetRealPersonNum(Parameter[0])].ys);
              Adresse+=5;                                                break;
      case 17:CurrentProc=Parameter[0];
              V_Verb=ENTRY; V_Par1=V_Par2=0;
              ProcNum=Parameter[0];        Adresse=0;
              LadeProgramm(ProcNum);       Prg=Programm[ProcNum].Data;
              break;
      case 18:if (MoasIf(Parameter[1],Parameter[3],Parameter[0],Parameter[2]))
                Adresse+=11;
              else Adresse=Parameter[4];                                 break;
      case 19:Person[GetRealPersonNum(Parameter[0])].GhostLevel=Parameter[1];
              RedrawPerson(GetRealPersonNum(Parameter[0]));
              Adresse+=5;                                                break;
      case 20:Adresse=IfPersonIsIn(Adresse,Parameter[5],Parameter[0],Parameter[1],
              Parameter[2],Parameter[3],Parameter[4]);      break;
      case 21:Adresse=IfWasntHere(Adresse,Parameter[0],Parameter[1]);    break;
      case 22:ProcNum=Parameter[0];        Adresse=Parameter[1];
              LadeProgramm(ProcNum);       Prg=Programm[ProcNum].Data;
              break;
      case 23:if (KeyAbortState==0)
                KeyAbortState=1;
              Adresse++;                                                 break;
      case 26:PrintVar(Parameter[0]);
              Adresse+=3;                                                break;
      case 27:AddArea(Parameter[0],Parameter[1],Parameter[2],Parameter[3],Parameter[4],Label);
              Adresse+=11;                                               break;
      case 28:AddArea(Parameter[0],Parameter[1],Parameter[2],Parameter[3],Parameter[4],Jumper);
              Adresse+=11;                                               break;
      case 29:if (KeyAbortState!=2)
              {
                short y;

                if (V_LockVga==0) Clearscreen();
                InitPictureData();
                InitScreenData();
                InitMarker();
                CloseSay(32000);
                DisplayMessage("",0);
                for (y=0;y<20;y++) WalkArea[y].ty=-1;
                for (y=0;y<24;y++) { memset(Phase[0].Data+12+44*(16+y),0,32);
                                     memset(Phase[0].Data+12+44*(16+y)+1760,0,32); }
              }
              MakeMegaTable(-1);  Adresse++;                             break;
      case 30:AddOntimeJump(Parameter[0],Parameter[1],Parameter[2]);
              Adresse+=7;                                                break;
      case 31:if (KeyAbortState!=2)
                OpenWindow(Parameter[0]/1024,(Parameter[0]/32)&31,Parameter[0]&31);
              Adresse+=3;                                                break;
      case 32:SetVar(Parameter[0],(rand() % Parameter[1]) + 1);
              Adresse+=5;                                                break;
      case 33:SetVar(Parameter[0],(rand() % Var[Parameter[1]]) + 1);
              Adresse+=5;                                                break;
      case 34:Person[GetRealPersonNum(Parameter[0])].z=Parameter[1];
              SortPersons();
              RedrawAllPersons();
              Adresse+=5;                                                break;
      case 35:if (Parameter[0]!=32000)
                RemovePerson(GetRealPersonNum(Parameter[0]));
              else RemoveAllPersons();
              Adresse+=3;                                                break;
      case 38:SetVar(Parameter[0],Parameter[1]);
              Adresse+=5;                                                break;
      case 39:SetVar(Parameter[0],Var[Parameter[1]]);
              Adresse+=5;                                                break;
      case 40:SetObject(Parameter[0],Parameter[1]);
              Adresse+=5;                                                break;
      case 41:DayLightR=Parameter[0];
              DayLightG=Parameter[1];
              DayLightB=Parameter[2];
              if (Eingeblendet) Blend(64,1);
              Adresse+=7;                                                break;
      case 42:if (KeyAbortState!=2)
              {
                LadeInfoBild(Parameter[1],InfoTyp1);
                LadeInfoBild(Parameter[2],InfoTyp2);
                LadeBild(Parameter[0]);
              } Adresse+=7;                                              break;
      case 43:StopAllWalking();
              Adresse++;                                                 break;
      case 44:PrintSentence(UnPack((char *)Parameter));             /*Text*/
              Adresse+=2+strlen((char *)Parameter);                      break;
      case 84:PrintSentence(UnPack((char *)Parameter));             /*Text+*/
              Adresse+=2+strlen((char *)Parameter);
              GetClickOrEnter(32000);
              CloseWindow();                                             break;
      case 90:PrintSentence(UnPack((char *)Parameter));             /*Text++*/
              GetClickOrEnter(32000);
              CloseWindow();
              PlayerHasControl=LastControl;
              return(1);
      case 45:UnMarkArea(Label,Parameter[0]);   Adresse+=3; break;
      case 46:UnMarkArea(Jumper,Parameter[0]);  Adresse+=3; break;
      case 47:Wait(Parameter[0]);
              Adresse+=3;                                                break;
      case 48:PlayerHasControl=LastControl;
              return(0);          /*NotFound*/
      case 50:PlayerHasControl=LastControl;
              return(1);          /*Found*/
      case 51:Restart();
              PlayerHasControl=LastControl;
              return(1);
      case 52:if (KeyAbortState!=2) Blend(Parameter[0],1);
              Adresse+=3;                                                break;
      case 53:if (KeyAbortState!=2) Blend(Parameter[0],0); else Blend(64,0);
              Adresse+=3;                                                break;
      case 54:if (KeyAbortState!=2) Adress(Parameter[0],Parameter[1]);
              Adresse+=5;                                                break;
      case 55:WaitWhileSomeBodyIsWalking(-1);
              Adresse++;                                                 break;
      case 56:Adresse=ParseIf(Adresse,Parameter[5],Parameter[0],Parameter[1],Parameter[2],Parameter[3],Parameter[4]);
              break;
      case 57:SetPerson(Parameter[0],Prg[15+Adresse],Parameter[1],Parameter[2],
                        Parameter[3],Parameter[4],Parameter[5],Parameter[6]);
              Adresse+=16;                                                break;
      case 58:Shake(Parameter[0],Parameter[1]);
              Adresse+=5;                                                break;
      case 59:InitOntimes();
              Adresse++;                                                 break;
      case 60:if (Var[Parameter[0]]==Parameter[1])
              {
                PrintSentence(UnPack((char *)&Parameter[2]));
                GetClickOrEnter(32000);
                CloseWindow();
                PlayerHasControl=LastControl;
                return(1);
              } Adresse+=6+strlen((char *)&Parameter[2]);                break;
      case 61:Execute(Parameter[0],Parameter[1]);
              Adresse+=5;                                                break;
      case 62:AddPowerParse(Parameter[0],UnPack((char*)&Parameter[1]));
              Adresse+=4+strlen((char *)&Parameter[1]);                  break;
      case 63:CheckFile(Parameter[0],Parameter[1],Parameter[2],Parameter[3]);
              Adresse+=9;                                                break;
      case 64:DisplayMessage(UnPack((char *)Parameter),42);
              Adresse+=2+strlen((char *)Parameter);                      break;
      case 65:AddDescription(Parameter[0],UnPack((char *)&(Parameter[1])));
              Adresse+=4+strlen((char *)&(Parameter[1]));                break;
      case 66:AddSayWindow(Parameter[0],Parameter[1],Parameter[2],UnPack((char*)&Parameter[3]));
              Adresse+=8+strlen((char *)&Parameter[3]);
              SayWait(Parameter[0]);                                     break;
      case 67:AddOption(Parameter[0],Parameter[1],Parameter[2],Parameter[3],ProcNum,Adresse+9);
              Adresse+=10+strlen((char *)&Parameter[4]);                 break;
      case 68:AddDialog(Parameter[0]);
              Adresse+=3;                                                break;
      case 69:InitDialogs();
              Adresse++;                                                 break;
      case 70:RemoveDialog(Parameter[0]);
              Adresse+=3;                                                break;
      case 71:ResetDialog(Parameter[0]);
              Adresse+=3;                                                break;
      case 72:
      case 78:if (KeyAbortState!=2)
                SelectFromDialog(Parameter[0],Parameter[1],Prg[Adresse]==78);
              else SetVar(Parameter[0],1);
              Adresse+=5;                                                break;
      case 73:ToggleDialogPoint(Parameter[0],Parameter[1],Parameter[2]);
              Adresse+=7;                                                break;
      case 74:if (MoasIf3(Parameter[1],Parameter[3],Parameter[0],Parameter[2],
                          Parameter[4],Parameter[5],Parameter[7],Parameter[6],
                          Parameter[8],Parameter[9],Parameter[11],Parameter[10]))
                Adresse+=27;
              else
                Adresse=Parameter[12];
              break;
      case 75:ToggleScreen(Parameter[0]);
              Adresse+=3;                                                break;
      case 76:InternalError(Parameter[0]);
      case 77:Packing=0;
              Adresse++;                                                 break;
      case 80:if (MoasIf2(Parameter[1],Parameter[3],Parameter[0],Parameter[2],
                          Parameter[4],Parameter[5],Parameter[7],Parameter[6]))
                Adresse+=19;
              else
                Adresse=Parameter[8];
              break;
      case 81:AddSayWindow(Parameter[0],Parameter[1],Parameter[2],UnPack((char*)&Parameter[3]));
              Adresse+=8+strlen((char *)&Parameter[3]);                  break;
      case 82:AddWalkArea(Parameter[0],Parameter[1],Parameter[2],Parameter[3],Parameter[4],Parameter[5]);
              Adresse+=13;                                                break;
      case 83:RemoveStatists();
              Adresse++;                                                 break;
      case 85:SayProperties[Parameter[0]].PrsNum=Parameter[1];
              SayProperties[Parameter[0]].TxtColor=Parameter[2];
              SayProperties[Parameter[0]].StdFont=Parameter[3];
              Adresse+=9;                                                 break;
      case 86:CloseSay(Parameter[0]);
              Adresse+=3;                                                 break;
      case 87:Blend2(Parameter[0],Parameter[1],Parameter[2],Parameter[3]);
              Adresse+=9;                                                break;
      case 88:WalkPerson(Parameter[0],Parameter[1],Parameter[2]);
              Adresse+=7;                                                break;
      case 79:GetClickOrEnter(32000);
              CloseWindow();
              PlayerHasControl=LastControl;
              return(1);
      case 89:CodeText(Parameter[0],UnPack((char *)&(Parameter[1])));
              Adresse+=4+strlen((char *)&(Parameter[1]));                break;
      case 91:strcpy(RoomName,UnPack((char *)&(Parameter[0])));
              Adresse+=2+strlen((char *)&(Parameter[0]));                break;
      case 92:                                       /*SayIf*/
      case 97:                                       /*SayIf+*/
      case 98:                                       /*SayIf++*/
      case 99:if (Var[Parameter[0]]==Parameter[1])   /*SayIf+Found*/
              {
                AddSayWindow(Parameter[2],-1000,-1000,UnPack((char*)(&Parameter[3])));
                if (Prg[Adresse]!=92 && Prg[Adresse]!=99) SayWait(Parameter[2]);
                if (Prg[Adresse]==98 || Prg[Adresse]==99) { PlayerHasControl=LastControl; return(1); }
              }
              Adresse+=8+strlen((char *)&Parameter[3]);
              break;
      case 93:PaletteCycleFirst=Parameter[0];
              PaletteCycleLast=Parameter[1];
              CycleWait=Parameter[2];
              Adresse+=7;                                                break;
      case 94:LookKeys();
              Adresse++;                                                 break;
      //case 95:LoadMusic(Parameter[0]);
      //        Adresse+=3;                                                break;
      case 96:iWalk(Parameter[0],Parameter[1]-Person[GetRealPersonNum(Parameter[0])].xs/2,Parameter[2]-Person[GetRealPersonNum(Parameter[0])].ys);
              WaitWhileSomeBodyIsWalking(Person[GetRealPersonNum(Parameter[0])].Num);
              if (KeyAbortState!=2 && (Person[GetRealPersonNum(Parameter[0])].x!=Person[GetRealPersonNum(Parameter[0])].TargetX || Person[GetRealPersonNum(Parameter[0])].y!=Person[GetRealPersonNum(Parameter[0])].TargetY))
              {
                PlayerHasControl=LastControl;
                return(1);
              }
              Adresse+=7;                                                break;
      default:printf("x=%i",Prg[Adresse]); getch();
              InternalError(1017);
    }
  }
}
void WalkPerson(short PrsNum,short tx,short ty)
{
  Person[GetRealPersonNum(PrsNum)].TargetX=tx-(Person[GetRealPersonNum(PrsNum)].xs>>1);
  Person[GetRealPersonNum(PrsNum)].TargetY=ty-Person[GetRealPersonNum(PrsNum)].ys;
  WaitWhileSomeBodyIsWalking(Person[GetRealPersonNum(PrsNum)].Num);
}
void SetVar(short VarNum,short Value)
{
  if (VarNum>AnzVars) InternalError(1018);
  if (VarNum==8 && Value>Var[VarNum]) AutoSaveGame=1;
  Var[VarNum]=Value;
  if (VarNum==13) QuickSetPalette(Palette);
  if (VarNum==14 || VarNum==17) TurnInventar(V_InventarState);
  if (VarNum==19)
  {
    short c;

    for (c=0;c<MAX_PRS;c++)
      if (Person[c].Num!=-1 && Person[c].Bits&128 && Person[c].Bits&1024)
        LoescheKopie(c);
  }
}
void ExecuteTheProgram(void)
{
  if (Execute(1,0)==0)
    if (Execute(CurrentProc,0)==0) Execute(2,0);
}
void AddOntimeJump(unsigned short Seconds,unsigned short Proc,unsigned short Adresse)
{
  short c;

  if (Seconds>0)
  {
    for (c=0;c<10;c++) if (Ontimes[c].TimeLeft==0) break;
    if (c==10) InternalError(1030);
    Ontimes[c].TimeLeft= Seconds;
    Ontimes[c].Proc    = Proc;
    Ontimes[c].Adresse = Adresse;
  }
  else
    for (c=0;c<10;c++)
      if (Ontimes[c].TimeLeft>0 && Ontimes[c].Proc==Proc && Ontimes[c].Adresse==Adresse)
      {
        Ontimes[c].TimeLeft=0;
        return;
      }
}
void SetObject(short ObjNum,short Pos)
{
  if (ObjNum>=1000) ObjNum-=1000;
  if (ObjNum>AnzInventar) InternalError(1031);
  Inventar[ObjNum].x=Pos;
  if (Pos==-50) Inventar[ObjNum].x=-3;          /* Have    */
  if (Pos==-51) Inventar[ObjNum].x=CurrentProc; /* Present */
  if (Pos==-54) Inventar[ObjNum].x=-2;          /* Used    */
  if (Pos==-55) Inventar[ObjNum].x=-1;          /* Not created */
  if (V_InventarState) TurnInventar(1);
}
void AddPerson(short Prs,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask)
{
  short ifil,c,MaxPerson;

  for (c=0;c<MAX_PRS;c++) if (Person[c].Num==-1) break;
  if (c==MAX_PRS) InternalError(1026);

  ifil=DataOpen(1);       ChkOpen("?",READ);
  read(ifil,(char*)&MaxPerson,2);
  if (Prs>=MaxPerson) InternalError(1027);
  if (lseek(ifil,(sizeof(struct PERSON)+4l)*Prs+2,SEEK_SET)==-1l) InternalError(1027);
  ChkRead(ifil,(char*)&Person[c],sizeof(struct PERSON));
  Person[c].xs=Phase[Person[c].PhaseNum].x;
  Person[c].ys=Phase[Person[c].PhaseNum].y;
  Person[c].Anz=Phase[Person[c].PhaseNum].Anz;
  ChkClose(ifil);

  LadePhase(Person[c].PhaseNum);
  Phase[Person[c].PhaseNum].InUse=1;
  Person[c].iWalk=0;
  Person[c].Phase=Phase[Person[c].PhaseNum].Data;
  Person[c].Counter=Person[c].z=Person[c].GhostLevel=0;
  SetPersonProperties(c,Inf,x,y,tx,ty,Bits,Mask);
}
void SetPerson(short Prs,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask)
{
  short p,OldX,OldY;

  p=GetRealPersonNum(Prs);
  OldX=Person[p].x;      OldY=Person[p].y;
  SetPersonProperties(p,Inf,x,y,tx,ty,Bits,Mask);
  p=GetRealPersonNum(Prs);
  PowerRedrawArea(OldX,OldY,Person[p].xs+OldX,Person[p].ys+OldY);
}
void SetPersonProperties(short c,char Inf,short x,short y,short tx,short ty,USHORT Bits,USHORT Mask)
{
  Person[c].x+=Person[c].xs>>1;
  Person[c].TargetX+=Person[c].xs>>1;
  Person[c].y+=Person[c].ys;
  Person[c].TargetY+=Person[c].ys;

  if ((Inf & 32) != 0) Person[c].x+=x; else Person[c].x=x;
  if ((Inf & 16) != 0) Person[c].y+=y; else Person[c].y=y;
  if ((Inf & 12) == 0) Person[c].TargetX=tx;
    else if ((Inf & 12) == 4) Person[c].TargetX+=tx;
      else Person[c].TargetX=Person[c].x+tx;
  if ((Inf &  3) == 0) Person[c].TargetY=ty;
    else if ((Inf & 3) == 1) Person[c].TargetY+=ty;
      else Person[c].TargetY=Person[c].y+ty;

  Person[c].Bits=(Person[c].Bits & Mask) | Bits;
  if (Person[c].Bits & 33536) MakeMegaTable(1);

  if ((Person[c].Bits & (1024+768))==0 && Person[c].Phase!=Phase[Person[c].PhaseNum].Data)
  {
    FREE(Person[c].Phase);
    Person[c].Phase=Phase[Person[c].PhaseNum].Data;
  }
  if ((Person[c].Bits & (1024+768)) && Person[c].Phase==Phase[Person[c].PhaseNum].Data)
    Person[c].Phase=MALLOC(Person[c].xs*Person[c].ys*Person[c].Anz); /*Sign overflow ?*/

  Person[c].x-=Person[c].xs>>1;
  Person[c].TargetX-=Person[c].xs>>1;
  Person[c].y-=Person[c].ys;
  Person[c].TargetY-=Person[c].ys;
  Person[c].Boden=Boden(c);
  LoescheKopie(c);
  CheckKopie(c);

  SortPersons();
  RedrawAllPersons();
}
void Restart(void)
{
  short c;

  for (c=0;c<5;c++)
    if (SayWindow[c].RefPrs!=-1)
    {
      if (SayWindow[c].Data) FREE(SayWindow[c].Data);
      if (SayWindow[c].Text) FREE(SayWindow[c].Text);
    }
  RemoveAllPersons();
  RemoveDescriptions();
  InitSystemVars(); InitPersons();
  InitOntimes();    InitPictureData();
  InitDialogs();
  Clearscreen();
  Execute(0,0);
}
void RedrawAllPersons(void)
{
  short c;

  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1)
      RedrawPerson(c);
}
void RedrawPerson(short c)
{
  PowerRedrawArea(Person[c].x,Person[c].y,Person[c].xs+Person[c].x,Person[c].ys+Person[c].y);
}
void RemoveDescriptions(void)
{
  short c;

  for (c=0;c<AnzDescription;c++) FREE(Description[c].Name);
  for (c=0;c<MAX_PARSE;c++)
    if (PowerParse[c].Nummer!=-1) FREE(PowerParse[c].String);
  AnzDescription=0;
}
void AddDescription(short Num,char *Text)
{
  short c;

  for (c=0;c<AnzDescription;c++)
    if (Description[c].Number==Num) InternalError(1012);
  Description[c].Name=MALLOC(strlen(Text)+1);
  strcpy(Description[c].Name,Text);

  Description[c].Number=Num;
  if (++AnzDescription>=500) InternalError(1036);
}
void RemoveStatists(void)
{
  register short c;

  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1 && (Person[c].Bits & 32)==0)
    {
      RemovePerson(c);
      c=-1;
    }
}
void RemovePerson(short Num)
{
  short x1,y1,x2,y2;

  x1=Person[Num].x; x2=Person[Num].x+Person[Num].xs;
  y1=Person[Num].y; y2=Person[Num].y+Person[Num].ys;
  if (Person[Num].Phase && (Person[Num].Bits & (1024+768)) && Person[Num].Num!=-1)
    FREE(Person[Num].Phase);
  Person[Num].Num=-1;
  SortPersons(); PowerRedrawArea(x1,y1,x2,y2);
}
void CloseSay(short Window)
{
  short c;

  if (Window==-1) Window=Person[GetRealPersonNum(Window)].Num;

  for (c=0;c<5;c++)
    if ((SayWindow[c].RefPrs==Window && Window!=32000) || (SayWindow[c].RefPrs!=-1 && Window==32000))
    {
      SayWindow[c].RefPrs=-1;
      if (SayWindow[c].Data) FREE(SayWindow[c].Data);
      if (SayWindow[c].Text) FREE(SayWindow[c].Text);
      SayWindow[c].Text=NULL;
      PowerRedrawArea(SayWindow[c].x,SayWindow[c].y,SayWindow[c].x+SayWindow[c].xs,SayWindow[c].y+SayWindow[c].ys);
    }
}
void Blend2(short r,short g,short b,short Speed)
{
  register short c,r_,g_,b_;

  r_=DayLightR; g_=DayLightG; b_=DayLightB;
  for (c=0;c<=100;c+=Speed,kbhit())
  {
    DayLightR=(r*c+(100-c)*r_)/101;
    DayLightG=(g*c+(100-c)*g_)/101;
    DayLightB=(b*c+(100-c)*b_)/101;
    QuickSetPalette(Palette);
  }
}
void RemoveAllPersons(void)
{
  short c;

  for (c=0;c<MAX_PRS;c++)
  {
    if (Person[c].Phase && Person[c].Phase!=Phase[Person[c].PhaseNum].Data && Person[c].Num!=-1)
      FREE(Person[c].Phase);
    Person[c].Num=-1;
  }
}
void TeakEffekt(void)
{
  short c,x,y,r,w;
  long Time;
  long *Tab320;

  struct STAR
  {
    short x,y;
    short tx,ty;
    short vx,vy;
    char *Vga;
    char Color;
    uChar SpeedDummy[15];   /* Summe ergibt 32 */
  } *Star;

  if (KeyAbortState==2) return;
  Star=(struct STAR*)MALLOC(sizeof(struct STAR)*2500); Tab320=(long*)MALLOC(4*200);

  c=0;
  Tab320+=100;
  for (y=-100;y<100;y++)
    Tab320[y]=y*320;
  for (y=69;y<143;y++)
    for (x=118;x<235;x++)
      if (Screen[x+y*320l]!=16)
      {
        Star[c].tx=x;
        Star[c].ty=y;
        r=rand()%39;
        w=rand()%20000;
        Star[c].x=Star[c].tx+sin(w)*r; /*+160;*/
        Star[c].y=Star[c].ty+cos(w)*r; /*+96;*/
        Star[c].vx=-(Star[c].y-Star[c].ty)/4;
        Star[c].vy=(Star[c].x-Star[c].tx)/4;
        Star[c].Vga=(VGA_ADRESSE)+Star[c].x+Star[c].y*320l;
        Star[c].Color=Screen[x+y*320l];
        c++; if (c>=2500) InternalError(1050);
      }
  for (Time=0;Time<64000;Time++) Screen[Time]=Info1[Time]|(Screen[Time]-16);

  for (x=0;x<256;x++)
  {
    outp(0x3c8,x);
    outp(0x3c9,Palette[3*((x&15)+16)]);
    outp(0x3c9,Palette[3*((x&15)+16)+1]);
    outp(0x3c9,Palette[3*((x&15)+16)+2]);
  }
  memcpy(VGA_ADRESSE,Info1,64000);
  w=0;
  Time=GetTicker();
  while (!kbhit() && w<58)
  {
    w++;
    for (x=0;x<c;x++)
    {
      Star[x].Vga[0]=Info1[((unsigned long)Star[x].Vga)&65535];
      if (Star[x].x<Star[x].tx) Star[x].vx++;
      else if (Star[x].x>Star[x].tx) Star[x].vx--;
      if (Star[x].y<Star[x].ty) Star[x].vy++;
      else if (Star[x].y>Star[x].ty) Star[x].vy--;
      if (w>20 && w<=25)
      {
        if (Star[x].y<Star[x].ty && Star[x].vy<0) Star[x].vy++;
        else if (Star[x].y>Star[x].ty && Star[x].vy>0) Star[x].vy--;
        else if (Star[x].y==Star[x].ty) Star[x].vy>>=1;
        if (Star[x].x<Star[x].tx && Star[x].vx<0) Star[x].vx++;
        else if (Star[x].x>Star[x].tx && Star[x].vx>0) Star[x].vx--;
        else if (Star[x].x==Star[x].tx) Star[x].vx>>=1;
      } else
      if (w>25 && w<=32)
      {
        if (Star[x].y<Star[x].ty && Star[x].vy<0) Star[x].vy+=2;
        else if (Star[x].y>Star[x].ty && Star[x].vy>0) Star[x].vy-=2;
        else if (Star[x].y==Star[x].ty) Star[x].vy>>=1;
        if (Star[x].x<Star[x].tx && Star[x].vx<0) Star[x].vx+=2;
        else if (Star[x].x>Star[x].tx && Star[x].vx>0) Star[x].vx-=2;
        else if (Star[x].x==Star[x].tx) Star[x].vx>>=1;
      } else
      if (w>32)
      {
        if (Star[x].x<Star[x].tx && Star[x].vx<0) Star[x].vx=0;
        else if (Star[x].x>Star[x].tx && Star[x].vx>0) Star[x].vx=0;
        else if (Star[x].x==Star[x].tx) Star[x].vx=0;
        if (Star[x].y<Star[x].ty && Star[x].vy<0) Star[x].vy=0;
        else if (Star[x].y>Star[x].ty && Star[x].vy>0) Star[x].vy=0;
        else if (Star[x].y==Star[x].ty) Star[x].vy=0;
      }
      Star[x].x+=Star[x].vx;
      Star[x].y+=Star[x].vy;
      if (w>45)
      {
        if (Star[x].x<Star[x].tx) { Star[x].x++; Star[x].Vga++; }
        else if (Star[x].x>Star[x].tx) { Star[x].x--; Star[x].Vga--; }
        if (Star[x].y<Star[x].ty) { Star[x].y++; Star[x].Vga+=320; }
        else if (Star[x].y>Star[x].ty) { Star[x].y--; Star[x].Vga-=320; }
      }
      Star[x].Vga+=Star[x].vx+Tab320[Star[x].vy];
      if (x<((w+2)<<6)) Star[x].Vga[0]=Star[x].Color;
    }
    while (GetTicker()==Time); Time=GetTicker();
    while (GetTicker()==Time); Time=GetTicker();
    while (GetTicker()==Time); Time=GetTicker();
    while (GetTicker()==Time); Time=GetTicker();
  }
  for (c=1;c<30;c++)
  {
    for (y=0;y<10;y++)
      if (c>y && c-y<=20)
        for (x=0;x<16;x++)
        {
          outp(0x3c8,x+((y+1)<<4));
          outp(0x3c9,((short)Palette[3*x+48]*(20-(c-y))+63*(c-y))/20);
          outp(0x3c9,((short)Palette[3*x+1+48]*(20-(c-y))+63*(c-y))/20);
          outp(0x3c9,((short)Palette[3*x+2+48]*(20-(c-y))+63*(c-y))/20);
        }
 // if (c&3==0)
 // {
 //   while (inp(0x3da)&8); while ((inp(0x3da)&8)==0);
 // }
    Time=GetTicker(); while (Time==GetTicker()) x++;
    Time=GetTicker(); while (Time==GetTicker()) x++;
  }
  memcpy(VGA_ADRESSE,Screen,64000);
  for (c=29;c>=0;c--)
  {
    for (y=0;y<10;y++)
      if (c>=y && c-y<=20)
        for (x=0;x<16;x++)
        {
          outp(0x3c8,x+(((9-y)+1)<<4));
          outp(0x3c9,((short)Palette[3*x+48]*(20-(c-y))+63*(c-y))/20);
          outp(0x3c9,((short)Palette[3*x+1+48]*(20-(c-y))+63*(c-y))/20);
          outp(0x3c9,((short)Palette[3*x+2+48]*(20-(c-y))+63*(c-y))/20);
        }
//  if (c&3==0)
//  {
//    while (inp(0x3da)&8); while ((inp(0x3da)&8)==0);
//  }
    Time=GetTicker(); while (Time==GetTicker()) x++;
    Time=GetTicker(); while (Time==GetTicker()) x++;
    Time=GetTicker(); while (Time==GetTicker()) x++;
  }
  while (kbhit()) getch();
  Time=GetTicker();
  while (GetTicker()-Time<17*64 && !kbhit());
  while (kbhit()) getch();
  for (c=1;c<15;c++)
  {
    for (x=0;x<256;x++)
    {
      outp(0x3c8,x);
      outp(0x3c9,((short)Palette[3*((x&15)+16)]*(15-c))/15);
      outp(0x3c9,((short)Palette[3*((x&15)+16)+1]*(15-c))/15);
      outp(0x3c9,((short)Palette[3*((x&15)+16)+2]*(15-c))/15);
    }
//  while (inp(0x3da)&8);
//  while (inp(0x3da)&8==0);
    Time=GetTicker(); while (Time==GetTicker()) x++;
    Time=GetTicker(); while (Time==GetTicker()) x++;
  }
  Clearscreen();
  FREE((char*)(Tab320-100));
  FREE((char*)Star);
}

void AddWalkArea(short x1,short y1,short x2,short y2,short tx,short ty)
{
  short c;

  for (c=0;c<20;c++)
    if (WalkArea[c].ty==-1) break;
  if (c>=20) InternalError(1048);
  WalkArea[c].x1=x1;
  WalkArea[c].y1=y1;
  WalkArea[c].x2=x2;
  WalkArea[c].y2=y2;
  WalkArea[c].tx=tx;
  WalkArea[c].ty=ty;
}
