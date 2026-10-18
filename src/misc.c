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

char TabNewDir[16]=
 { 0,2,6,0,4,3,5,0,0,1,7,0,0,0,0,0 } ;

void InternalError(short num)
{
  long c;

  
  if (num==1007) return; /* ==>+<== Ignore CHKSUM due to FastLink */
#ifdef DOS
  _setvideomode(_DEFAULTMODE);
#endif

  switch(num)
  {
    case 1000:printf("\nNicht genug erweiterter Speicherplatz !"); break;
    case 1001:printf("\nDatenfile korrupt. Bitte neu installieren !"); break;
    case 1002:printf("\nCannot open data file (see stderr for path details)!\n"); break;
    case 1003:printf("\nVGA-Karte ist nicht vorhanden !");         break;
    case 1047:printf("\nDOS-Handling Problem !");                  break;
    case 1048:printf("\nKann Soundkarte nicht ansprechen !");      break;
    case 1049:printf("\nNicht genug standart Speicherplatz !");    break;
    default:if (num) printf("\nInterner Fehler %i",num);
  }
#ifdef DOS
  outp(0x40,0xff); outp(0x40,0xff);
  _dos_setvect(0x9,RealIrq9);
  _dos_setvect(0x8,RealIrq1c);
#endif
  //ExitCommunicator=1;
  //for (c=0;c<1000000;c++)
  //  if (ExitCommunicator!=1) break;
  //_dos_setvect(SoftIRQ[Config.IrqNum],RealSoundIrq);
  exit(-1);
}
/*********
char *MALLOC(long Size)
{
  char *p;

  if (Size<0) InternalError(3000);
  if (Size>256000) InternalError(3001);
  p=malloc(Size+3);
  if (p==NULL)
  {
    FreeSomeMemory();
    p=malloc(Size+3);
  }
  if (p==NULL) InternalError(1000);

  return(p);
}
void FREE(char *p)
{
  free(p);
}********/
void LookKeys(void)
{
  short PN=GetPlayerPersonNumber(); /*Person Number*/
  short PersonX,PersonY;
  short Strg;
  long Time;
  static short CursorDir=-1,CursorSpeed=1,LastStrg;
  static long LastTime;

  PersonX=Person[PN].x; PersonY=Person[PN].y;
  if (PlayerHasControl==0) PN=MAX_PRS;

  #ifdef DOS
  Time=*((long*)0x0046C); Strg=((((char *)0x00417)[0]&4)/4) | SpaceState | (Window.Status!=None) | (PN==MAX_PRS) | (Person[PN].StepSpeed==0);
#else
  Time=GetTicker(); Strg=SpaceState | (Window.Status!=None) | (PN==MAX_PRS) | (Person[PN].StepSpeed==0);
#endif

  switch(KeyScanCode)
  {
    case 72:CursorDir=0;  break;
    case 73:CursorDir=1;  break;
    case 77:CursorDir=2;  break;
    case 81:CursorDir=3;  break;
    case 80:CursorDir=4;  break;
    case 79:CursorDir=5;  break;
    case 75:CursorDir=6;  break;
    case 71:CursorDir=7;  break;
    case 76:CursorDir=8;  break;
    default:CursorDir=-1;
  }
  if (CursorSpeed>15) CursorSpeed=15;
  if (CursorDir==-1 || CursorDir==8) CursorSpeed=1;
  if (V_NoWalk) CursorDir=8;

  if (Window.Status==None && PlayerHasControl==1 && AutoSaveGame==1)
  {
    SaveGame(1);
    AutoSaveGame=0;
  }

  if (KeyScanCode!=LastKeyScanCode)
  {
    if (((PN==MAX_PRS) | (Person[PN].StepSpeed==0)) && PlayerHasControl==1 && Window.Status==None
#ifdef DOS
        && (((char *)0x00417)[0]&4)==0
#endif
       )
      if (KeyScanCode==72 || KeyScanCode==73 || KeyScanCode==77 || KeyScanCode==80 || KeyScanCode==81 || KeyScanCode==79 || KeyScanCode==75 || KeyScanCode==71)
      {
        short T0=Var[0],T1=Var[1],T2=Var[2];
        V_Verb=C_WALK;
        ExecuteTheProgram();
        Var[0]=T0; Var[1]=T1; Var[2]=T2;
      }

    LastKeyScanCode=KeyScanCode;
    switch(KeyScanCode)
    {
      case 57:SpaceState|=1;         break;
      case 57+128:SpaceState&=2;     break;
      case 28:SpaceState|=2;         break;
      case 28+128:SpaceState&=1;     break;
    }
    if (Window.Status==None && PlayerHasControl==1)
      switch(KeyScanCode)
      {
        case 15:TurnInventar(V_InventarState^1);  break;
        case 63:SaveGame(0);          break;
        case 65:LoadGame();           break;
      }
  }
  if (!Strg && Window.Status==None && PlayerHasControl==1 && Mouse.Right==0)
  {
    switch(CursorDir)
    {
      case 0:Person[PN].TargetX=PersonX;     Person[PN].TargetY=PersonY-999;        break;
      case 1:Person[PN].TargetX=PersonX+999; Person[PN].TargetY=PersonY-999;        break;
      case 2:Person[PN].TargetX=PersonX+999; Person[PN].TargetY=PersonY;            break;
      case 3:Person[PN].TargetX=PersonX+999; Person[PN].TargetY=PersonY+999;        break;
      case 4:Person[PN].TargetX=PersonX;     Person[PN].TargetY=PersonY+999;        break;
      case 5:Person[PN].TargetX=PersonX-999; Person[PN].TargetY=PersonY+999;        break;
      case 6:Person[PN].TargetX=PersonX-999; Person[PN].TargetY=PersonY;            break;
      case 7:Person[PN].TargetX=PersonX-999; Person[PN].TargetY=PersonY-999;        break;
      case 8:Person[PN].TargetX=PersonX;     Person[PN].TargetY=PersonY;            break;
    }
    if (CursorDir>=0 && CursorDir<=7) Person[PN].iWalk=0;
  }
  if (LastTime!=Time && Strg && CursorDir!=-1)
  {
    short ox,oy;

    ox=Mouse.x;
    oy=Mouse.y;
    switch(CursorDir)
    {
      case 0:Mouse.y-=CursorSpeed; break;
      case 1:Mouse.y-=CursorSpeed;
      case 2:Mouse.x+=CursorSpeed; break;
      case 3:Mouse.x+=CursorSpeed;
      case 4:Mouse.y+=CursorSpeed; break;
      case 5:Mouse.y+=CursorSpeed;
      case 6:Mouse.x-=CursorSpeed; break;
      case 7:Mouse.x-=CursorSpeed; Mouse.y-=CursorSpeed; break;
    }
    if (Mouse.x<0) Mouse.x=0;    if (Mouse.x>319) Mouse.x=319;
    if (Mouse.y<0) Mouse.y=0;    if (Mouse.y>199) Mouse.y=199;
    if (Window.Status==None) PowerRedrawArea(min(Mouse.x,ox),min(Mouse.y,oy),max(Mouse.x,ox)+44,max(Mouse.y,oy)+40);
                        else VMouse(2);

    CursorSpeed++;
  }

  if (kbhit())
  {
    V_Par1=WhatsAt(Mouse.x,Mouse.y);
    switch(V_Keyboard=getch())
    {
      case 'p':if (V_Master)
               {
                 char String[80];
                 sprintf(String,"mouse pos: x:%i  y:%i",Mouse.x,Mouse.y);
                 DisplayMessage(String,13);
               } break;
      case 's':
      case 'S':if (Window.Status==None && PlayerHasControl)
                 Mouse.Right=1;  break;
      case 0:switch((V_Keyboard=getch()+1000)-1000)
             {
               case 16 :InternalError(0);
               //case '>':V_Play=1; StartSoundProcessing(); break;  /*F4*/
               case 'D':if (Window.Status==None || Window.x1>115 || Window.y2<142) FileMenu();
                        break;
               case 23 :if (V_Master) memcpy((char *)VGA_ADRESSE,Info1,64000);
                        break;
               case 36 :if (V_Master) memcpy((char *)VGA_ADRESSE,Info2,64000);
                        break;
               case '1':if (V_Master) RedrawCompleteScreen();
                        break;
               case '2':if (V_Master) ShowMarker();
                        break;
               case 'B':if (V_Master) FreeSomeMemory();
                        break;
               case 25 :if (V_Master) RedrawAllPersons();
                        break;
               case '#':if (V_Master) ShowHercBak();
                        break;
               case '-':if (V_Master)
                        {
                          static char Buffer[40],oBuf[80];
                          short ofil;

#ifdef DOS
                          _clearscreen(_GCLEARSCREEN);
#endif
                          printf("Exportiere Label! Bitte Namen eingeben.\n");
                          printf("Danach per Drag&Drop ein Rechteck markieren.\nName:");

                          if (fgets(Buffer, sizeof(Buffer), stdin)) { size_t n=strlen(Buffer); if (n && Buffer[n-1]=='\n') Buffer[n-1]=0; }

                          PowerRedrawArea(0,0,320,200);
                          if (Buffer)
                          {
                            char *Tmp;
                            short l;

                            ofil=open("Export.Dpp",O_RDWR
#ifdef DOS
                                      +O_BINARY
#endif
                                      ,S_IWRITE);
                            if (ofil>0)
                            {
                              Tmp=MALLOC(l=filelength(ofil));
                              read(ofil,Tmp,l);
                              close(ofil);
                              ofil=open("Export.Dpp",O_RDWR
#ifdef DOS
                                        +O_BINARY
#endif
                                        +O_TRUNC,S_IWRITE);
                              if (Tmp[l-1]==26) l--;
                              write(ofil,Tmp,l);
                              close(ofil);
                              FREE(Tmp);
                            }

                            MouseCheck();
                            while (Mouse.Left | Mouse.Right) MouseCheck();
                            GetClickOrEnter(32000);
                            sprintf(oBuf,"MarkArea %3i %3i %3i %3i %s\x0d\x0a",Mouse.ClickX,Mouse.ClickY,Mouse.ReleaseX,Mouse.ReleaseY,Buffer);
                            ofil=open("Export.Dpp",O_CREAT+O_RDWR
#ifdef DOS
                                      +O_BINARY
#endif
                                      +O_APPEND,S_IWRITE);
                            write(ofil,oBuf,strlen(oBuf));
                            close(ofil);
                          }
                        }
                        break;
               case ';':if (Window.Status==None && PlayerHasControl)
                        {
                          V_Verb=BENUTZE;
                          V_Par1=V_Par2=1000;
                          ExecuteTheProgram();
                        }
               case 20:if (getenv("THPRIV")) /*Is ThPrivilidge defined ?*/
                         V_Master^=1;   break;
             }
    }
  }
  LastTime=Time; LastStrg=Strg;
}
void IveGotTooMuchTime(void)
{
  static short LastTime,Walker=0,LastCall=0;
  short ThisTime;

  ThisTime=GetTicker();
  if (ThisTime!=LastTime)
  {
    if (ThisTime-LastCall>146 && WalkingAllowed==1)
    {
      LastCall=ThisTime;
      UpDateOntimes();
      WillBeCalledEverySecond();
    }
    else if (LastCall>ThisTime) LastCall=ThisTime;

    LastTime=ThisTime;          /*TotalSpeed:1..8*/
    Walker++; if (WalkingAllowed==1 && Walker>=10-TotalSpeed) { UpDateWalking(); Walker=0; }
    Walker++; if (WalkingAllowed==1 && Walker>=10-TotalSpeed) { UpDateWalking(); Walker=0; }
    if (WalkingAllowed==1 && (ThisTime&3)==0) UpDateSayWindows();
    if (WalkingAllowed==1) LookKeys();
  }
}
void WillBeCalledEverySecond(void)
{
  V_Seconds++;
  V_SecondsElapsed++;
  V_TimeSinceLastSave++;
  if (V_Seconds>=60) {V_Seconds=0; V_Minutes++; V_MinutesSinceLastPoint++; }
  if (V_Minutes>=60) {V_Minutes=0; V_Hours++; }
  if (V_Hours>=24) V_Hours=0;
}
void Wait(short Milliseconds)
{
  WinEnhance();
#ifdef DOS
  {
    static unsigned char LastTime;
    union REGS Regs;

    while (Milliseconds>0 && KeyAbortState!=2)
    {
      Regs.h.ah=44;
      int386(0x21,&Regs,&Regs);
      if ((Regs.h.dl/10)!=LastTime)
        Milliseconds--;
      LastTime=(Regs.h.dl/10);
      MouseCheck();
    }
  }
#else
  /* ~1 unit ≈ 10 ms on DOS clock; sleep in 10 ms steps and pump events */
  while (Milliseconds > 0 && KeyAbortState != 2
#ifndef DOS
         && !PlatformShouldQuit()
#endif
        )
  {
    MouseCheck();
    Milliseconds--;
  }
#endif
}
void GetClickOrEnter(short Time)
{
  WinEnhance();
  WalkingAllowed=0;
#ifdef DOS
  {
    unsigned char LastTime = 0;
    union REGS Regs;

    while (KeyAbortState!=2 && Time>0)
    {
      if (Time!=32000)
      {
        Regs.h.ah=44;
        int386(0x21,&Regs,&Regs);
        if ((Regs.h.dh)!=LastTime)
          Time--;
        LastTime=(Regs.h.dh);
      }
      MouseCheck();

      if ( (!(Mouse.Left | Mouse.Right)) && (Mouse.LastLeft | Mouse.LastRight)) Time=0;
    }
  }
#else
  /*
   * Time==32000 means "wait forever for a click".
   * Otherwise Time is roughly centisecond-ish ticks; we map to frames.
   */
  {
    int forever = (Time == 32000);
    short saw_down = 0;
    while (KeyAbortState != 2 && !PlatformShouldQuit() && (forever || Time > 0))
    {
      MouseCheck(); /* pumps SDL, presents, latches buttons, delays */
      if (Mouse.Left || Mouse.Right || SpaceState)
        saw_down = 1;
      /* Release after a press (DOS behaviour) */
      if (saw_down && !(Mouse.Left | Mouse.Right) && !SpaceState)
        break;
      /* Also accept pure edge in case we entered already held */
      if ((!(Mouse.Left | Mouse.Right)) && (Mouse.LastLeft | Mouse.LastRight))
        break;
      /* conio queue: Enter / Space */
      if (kbhit()) {
        int ch = getch();
        if (ch == 13 || ch == 10 || ch == 32 || ch == 27)
          break;
        if (ch == 0 && kbhit())
          (void)getch();
      }
      if (!forever)
        Time--;
    }
  }
#endif
  Mouse.LastRight = Mouse.LastLeft = 0;
  SpaceState = 0;
  WalkingAllowed = 1;
  if (Window.Status == None)
    Mouse.CursorState = 1;
}
void LoadPack(void)
{
  short ifil,Length,value=0,c;
  char *buffer;
  short NEW=1;
  CHKSUM check;

  if ((ifil=ChkOpen("PIZZZA.PCK",READ))==-1)
    ifil=ChkOpen("GAME.PCK",READ);

  Length=(short)filelength(ifil)-4;

  buffer=(char*)MALLOC(Length);

  ChkRead(ifil,buffer,(unsigned short)Length);
  read(ifil,(char*)&check,4);

  ClrChk();

  SingleChk(buffer[0]);

  for (c=1;c<Length;c++,value++)
  {
    buffer[c]^=value;
    SingleChk(buffer[c]);
    if (NEW==1)
    {
      codes[anz_codes++]=buffer+c;
      NEW=0;
    }
    if (buffer[c]==0) NEW=1;
  }
  close (ifil);

  if (check!=GetChk()) InternalError(2007);
}
char *UnPack(unsigned char *String)
{
  static unsigned char Buffer[300];
  short c,l;

  if (!String) {
    Buffer[0] = 0;
    return (char *)Buffer;
  }
  if (Packing==0) {
    strncpy((char *)Buffer, (const char *)String, sizeof(Buffer)-1);
    Buffer[sizeof(Buffer)-1] = 0;
  } else {
    size_t used = 0;
    Buffer[0] = 0;
    l = (short)strlen((const char *)String);
    for (c = 0; c < l; c++) {
      const char *piece = (const char *)codes[(unsigned char)String[c]];
      size_t plen;
      if (!piece) continue;
      plen = strlen(piece);
      if (used + plen >= sizeof(Buffer) - 1)
        break;
      memcpy(Buffer + used, piece, plen);
      used += plen;
      Buffer[used] = 0;
    }
  }
  return (char *)Buffer;
}
void RedrawCompleteScreen(void)
{
  PowerRedrawArea(0,0,319,199);
}
void StopPerson(short PrsNum)
{
  Person[PrsNum].iWalk=0;
  Person[PrsNum].TargetX=Person[PrsNum].x;
  Person[PrsNum].TargetY=Person[PrsNum].y;
}
void StopAllWalking(void)
{
  short c;

  for (c=0;c<MAX_PRS;c++) StopPerson(c);
}
void UpDateWalking(void) /*==>+<== vllig neuschreiben !!! Total Chaos !!! */
{
  register short ox,oy,c,d,NDir,WasSomething,Ok,Speed,WasAnythingAtAll=0;
  static long Animation=0;
  char PersonHasTalked;

  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1)
    {
      if (Person[c].BaseSpeed>1 && (Animation % Person[c].BaseSpeed)) continue;
      if (Person[c].Dir>7 && (Person[c].Bits & 8192)==0)
      {
        Person[c].Dir=(Person[c].Dir-8)*2;
        Person[c].Counter=0;
        PersonHasTalked=1;
      } else PersonHasTalked=0;
      for (d=0;d<5;d++) if (SayWindow[d].RefPrs==Person[c].Num) break;
      if (PersonHasTalked==0 && Person[c].x==Person[c].TargetX && Person[c].y==Person[c].TargetY && d==5 && (Person[c].Bits & (2048+8+1))==0) continue;
      Ok=NDir=WasSomething=0;
      ox=Person[c].x; oy=Person[c].y;
      Speed=Person[c].StepSpeed;

      if (Person[c].x==Person[c].TargetX) Ok++;
      else
      {
        if (abs(Person[c].x-Person[c].TargetX)<Speed) Speed=abs(Person[c].x-Person[c].TargetX);
        if (Person[c].x<Person[c].TargetX) Person[c].x+=Speed,WasSomething=1,NDir+=1;
                                      else Person[c].x-=Speed,WasSomething=1,NDir+=2;
        Speed=Person[c].StepSpeed;
        if (Speed>100) Speed=1;
      }

 /*   if (Speed>0 && !IsPersonPositionOk(c)) */
      if (!IsPersonPositionOk(c))
        { Person[c].x=ox; Ok++; }
      if (Person[c].y==Person[c].TargetY) Ok++;
      else
      {
        if (abs(Person[c].y-Person[c].TargetY)<Speed) Speed=abs(Person[c].y-Person[c].TargetY);
        if (Person[c].y<Person[c].TargetY) Person[c].y+=Speed,WasSomething=1,NDir+=4;
                                      else Person[c].y-=Speed,WasSomething=1,NDir+=8;
        Speed=Person[c].StepSpeed;
        if (Speed>100) Speed=1;
      }
 /*   if (Speed>0 && !IsPersonPositionOk(c)) */
      if (!IsPersonPositionOk(c))
        { Person[c].y=oy; Ok++; }

      if (Person[c].iWalk && (Ok==2 || (Person[c].x==Person[c].TargetX && Person[c].y==Person[c].TargetY)))
      {
        if (Person[c].x==Person[c].TargetX && Person[c].y==Person[c].TargetY)
        {
          if (Person[c].x!=Person[c].gtx || Person[c].y!=Person[c].gty)
          {
            iWalk(Person[c].Num,Person[c].gtx,Person[c].gty);
            Ok=1;
          }
          else Person[c].iWalk=0;
        }
        else
        {
          Person[c].iWalk=0;
          V_Verb=M_STUPID;
          V_Par1=Person[c].Num;
          ExecuteTheProgram();
        }
      }
      if (Ok==2 && ((Person[c].Bits&1)==0))
      {
        if ((Person[c].Dir==0 || Person[c].Dir==4) && Person[c].Counter!=0)
        {
          Person[c].Counter=0;  /* Person gerade hinstellen */
          CheckKopie(c);
          RedrawPerson(c);
        }

        for (d=0;d<5;d++) if (SayWindow[d].RefPrs==Person[c].Num) break;
        if (d<5 && Person[c].Sequence[0][Person[c].Dir/2+8]!=-1)
        {
          for (d=0;d<7;d++) if (Person[c].Sequence[d][Person[c].Dir/2+8]==-1) break;
          Person[c].Counter=(Animation / Person[c].PhaseSpeed / 4) % d;
          Person[c].Dir=Person[c].Dir/2+8;                      // Talk Speed
          CheckKopie(c);
          RedrawPerson(c);
          WasAnythingAtAll=1;
        }
        else if (PersonHasTalked)
        {
          CheckKopie(c);
          RedrawPerson(c);
        }

        if ((Person[c].Bits & 2)==2 && IsPersonPositionOk(c+1000) && (Person[c].x!=Person[c].TargetX || Person[c].y!=Person[c].TargetY)) continue; /* Hlt andere Prs sie auf ?*/

        if ((Person[c].x==Person[c].TargetX && Person[c].y==Person[c].TargetY) || Person[c].StepSpeed>0 || Person[c].Bits&16384==0)
          StopPerson(c);
        if (Person[c].Bits & 8)
        {
          short T0=Var[0],T1=Var[1],T2=Var[2];

          V_Par1=Person[c].Num;
          V_Verb=M_WALK;
          if (Person[c].Bits & 2048) RemovePerson(c);
          ExecuteTheProgram();

          Var[0]=T0; Var[1]=T1; Var[2]=T2;
        } else if (Person[c].Bits & 2048) RemovePerson(c);
        continue;
      }
      if ((Person[c].Bits & 8192)==0 && Ok!=2) Person[c].Dir=TabNewDir[NDir];
      if (Person[c].Sequence[Person[c].Counter][Person[c].Dir]==-1)
      {
        Person[c].Counter=0;
        WasSomething=1;
      }
      if (WasSomething==1 || (Person[c].Bits&16) || (Person[c].Bits&1))
      {
        if (Person[c].Boden != Boden(c))
        {
          LoescheKopie(c);
          Person[c].Boden=Boden(c);
        }
        if ((Animation % Person[c].PhaseSpeed)==0)
          NextPersonPhase(c);
        CheckKopie(c);
        if (Person[c].x!=Person[c].TargetX || Person[c].y!=Person[c].TargetY || (Person[c].Bits & 2048)==0)
          PowerRedrawArea(Person[c].x-Speed,Person[c].y-Speed,Person[c].xs+Person[c].x+Speed,Person[c].ys+Person[c].y+Speed);
        WasAnythingAtAll=1;
      }
    }
  SortPersons(); TestIfPersonIsInTrap();
  Animation++;
}
short Boden(short c)
{
  unsigned short x,y;

  y=Person[c].y+Person[c].ys;
  x=Person[c].x+Person[c].xs/2;
  y=min(y,199); y=max(y,0);
  x=min(x,319); x=max(x,0);
  return(Info2[x+Tab320[y]]&127);  /* 0..127 */
}
void NextPersonPhase(short PrsNum)
{
  do
  {
    Person[PrsNum].Counter++;
    if (Person[PrsNum].Counter>7)
      if (Person[PrsNum].Bits & 8192)    /* Consekutive ? */
      {
        Person[PrsNum].Counter=0;
        Person[PrsNum].Dir++;
        if (Person[PrsNum].Sequence[Person[PrsNum].Counter][Person[PrsNum].Dir]==-1 || Person[PrsNum].Dir>=12)
        {
          Person[PrsNum].Dir=0;
          if (Person[PrsNum].Bits & 4096)
            StopPerson(PrsNum);
        }
      }
      else
      {
        Person[PrsNum].Counter=0;
        if (Person[PrsNum].Bits & 4096)
          StopPerson(PrsNum);
      }
  } while (Person[PrsNum].Sequence[Person[PrsNum].Counter][Person[PrsNum].Dir]==-1);
}
short IsPositionOk(short PrsNum,short x,short y)
{
  register unsigned short d;
  register short c;
  static char Simple=0;
  static uShort yFoot,Anfang,Ende;

  if (PrsNum>=1000) { Simple=1; PrsNum-=1000; }

  if (Person[PrsNum].Bits & 16384) return(1);
  yFoot=y+Person[PrsNum].ys;

  if (yFoot<200)
  {
    Anfang=max(0,x)+Tab320[yFoot];
    Ende  =min(320,x+Person[PrsNum].xs)-1+Tab320[yFoot];
    for (d=Anfang;d<Ende;d++)
      if ((Info2[d]>>7)==0) return(0);
  }

  if (Simple) return(1);

  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1 && c!=PrsNum && (Person[c].Bits & 16384)==0)
      if (abs(yFoot+Person[PrsNum].z-Person[c].y-Person[c].z-Person[c].ys)<3)
      {
        if (x<Person[c].x)
        {
          if (x+Person[PrsNum].xs>Person[c].x) return(0);
        } else
          if (Person[c].x+Person[c].xs>x) return(0);
      }
  return(1);
}
short IsPersonPositionOk(short PrsNum)
{
  return(IsPositionOk(PrsNum,Person[PrsNum].x,Person[PrsNum].y));
}
short GetPlayerPersonNumber(void)
{
  register short c;

  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1 && (Person[c].Bits & 32)) return(c);
  return(MAX_PRS);
}
void SortPersons(void)                                                          /*<QW>*/
{
  register short c;

  for (c=0;c<14;c++)
  {
    if (Person[c].Num==-1)
      if (Person[c+1].Num==-1) continue;
      else goto ChangePlaces;
    if (Person[c+1].Num==-1) continue;
    if (Person[c].ys+Person[c].z+Person[c].y<=Person[c+1].z+Person[c+1].y+Person[c+1].ys) continue;

    ChangePlaces:
    Person[MAX_PRS]=Person[c];
    Person[c]=Person[c+1];
    Person[c+1]=Person[MAX_PRS];
    c-=2; if (c<-1) c=-1;
  }
}
short GetRealPersonNum(short Number)
{
  short c;

  if (Number==-1) return(GetPlayerPersonNumber());
  for (c=0;c<MAX_PRS;c++) if (Person[c].Num==Number) return(c);
#ifdef DOS
  _setvideomode(_DEFAULTMODE);  /* ==>+<== DEBUG Version */
#endif
  printf("\n'short GetRealPersonNum(short Number)' findet Person nicht.");
  printf("\ngesucht war Person Nummer:%i",Number);
  printf("\nDie internen Personennummern sind folgendermaáen belegt:");
  for (c=0;c<MAX_PRS;c++)
    printf("\ninterne Nummer %i: Person Nummer %i",c,Person[c].Num);
  getch();
#ifdef DOS
  _setvideomode(_MRES256COLOR);
#endif
  InternalError(1028);
  return(-1);
}
void AddArea(short x1,short y1,short x2,short y2,unsigned short Ref,enum MARKTYP MarkTyp)
{
  short c;

  for (c=0;c<50;c++) if (Marker[c].MarkTyp==Unused) break;
  if (c==50) InternalError(1029);

  Marker[c].x1=x1;   Marker[c].y1=y1;
  Marker[c].x2=x2;   Marker[c].y2=y2;
  Marker[c].Ref=Ref; Marker[c].MarkTyp=MarkTyp;
}
void UnMarkArea(enum MARKTYP MarkTyp,short Ref)
{
  short c;

  for (c=0;c<50;c++)
    if (Marker[c].MarkTyp==MarkTyp && Marker[c].Ref==Ref)
      Marker[c].MarkTyp=Unused;
}
void ShowMarker(void)
{
  short c;

  for (c=0;c<50;c++)
    if (Marker[c].MarkTyp!=Unused)
    {
      if (Marker[c].MarkTyp==Label) _setcolor(2); else _setcolor(0);
#ifdef DOS
      _rectangle(_GBORDER,Marker[c].x1,Marker[c].y1,Marker[c].x2,Marker[c].y2);
#endif
    }
}
void TestIfPersonIsInTrap(void)
{
  short c,x,y;

  c=GetPlayerPersonNumber();
  if (c!=MAX_PRS)
  {
    x=Person[c].x+Person[c].xs/2;
    y=Person[c].y+Person[c].ys;
    for (c=0;c<50;c++)
      if (Marker[c].MarkTyp==Jumper)
        if (Marker[c].x1<=x && Marker[c].y1<=y && Marker[c].x2>=x && Marker[c].y2>=y)
          {
            Execute(CurrentProc,Marker[c].Ref);
            return;
          }
  }
}
void UpDateOntimes(void)
{
  short c;

  for (c=0;c<10;c++)
    if (Ontimes[c].TimeLeft>0)
    {
      Ontimes[c].TimeLeft--;
      if (Ontimes[c].TimeLeft==0)
        Execute(Ontimes[c].Proc,Ontimes[c].Adresse);
    }
}
void Adress(short Prs,short Ref)
{
  short c;

  Prs=GetRealPersonNum(Prs);
  if (Ref>=10000) Person[Prs].Dir=Ref-10000;
  else
  {
    short dx,dy;

    c=GetRealPersonNum(Ref);
    dx=(Person[c].x+Person[c].xs/2)-(Person[Prs].x+Person[Prs].xs/2);
    dy=(Person[c].y+Person[c].ys/2)-(Person[Prs].y+Person[Prs].ys/2);
    if (abs(dy)>=2*abs(dx))
      if (dy>0) Person[Prs].Dir=0;
           else Person[Prs].Dir=4;
    else if (abs(dy)*2>=abs(dx))
      if (dy>0) if (dx>0) Person[Prs].Dir=1;
                     else Person[Prs].Dir=7;
           else if (dx>0) Person[Prs].Dir=3;
                     else Person[Prs].Dir=5;
    else if (dx>0) Person[Prs].Dir=2;
              else Person[Prs].Dir=6;
  }
  if (Person[Prs].Sequence[Person[Prs].Counter][Person[Prs].Dir]==-1)
    Person[Prs].Counter=0;
  CheckKopie(Prs);
  RedrawPerson(Prs);
}
void WaitWhileSomeBodyIsWalking(short PersonNum)
{
  register short c,Ok=0;

  WinEnhance();
  while (Ok==0 && KeyAbortState!=2)
  {
    MouseCheck();
    for (Ok=1,c=0;c<MAX_PRS;c++)
      if ((Person[c].x!=Person[c].TargetX || Person[c].y!=Person[c].TargetY) && (Person[c].StepSpeed>0 || (Person[c].Bits&4096)==4096))
      if (PersonNum==-1)
      {
        if (Person[c].Num!=-1) Ok=0;
      } else if (Person[c].Num==PersonNum) Ok=0;
  }
}
void TurnInventar(short State)
{
  short c,RealHave,AnzHave,AnzHave2;

  V_InventarState=State;
  for (RealHave=c=0;c<AnzInventar;c++)
    if (Inventar[c].x==-3) RealHave++;

  AnzHave=AnzHave2=0;
  for (c=0;c<AnzInventar;c++)
  {
    if (Inventar[c].x==-3)
    {
      LadeInventar(c);
      Inventar[c].y=AnzHave2;
      AnzHave++;
      AnzHave2++;
    }
    if (V_IsoInv)
    {
      if ((AnzHave+1)*(AnzHave+1)>=RealHave*4)
      {
        RealHave-=AnzHave;
        AnzHave=0;
        AnzHave2=(AnzHave2/10+1)*10;
      }
    }
  }
  PowerRedrawArea(0,0,319,(AnzHave2/10+1)*24);
}
void AddPictureToTab(short Num,enum PICTYP Typ)
{
  short c;

  for (c=0;c<10;c++) if (PicturesOnScreen[c].Typ==-1)
    {
      PicturesOnScreen[c].Num=Num;
      PicturesOnScreen[c].Typ=Typ;    return;
    }
  InternalError(1032);
}
void FreeSomeMemory(void)
{
  short c;

  UpDatePersonMemory();
  for (c=0;c<AnzInventar;c++)
    if (Inventar[c].x!=-3 && Inventar[c].Data)
    {
      FREE(Inventar[c].Data);
      Inventar[c].Data=NULL;
    }
  for (c=2;c<AnzPhasen;c++)
    if (Phase[c].InUse==0 && Phase[c].Data)
    {
      FREE(Phase[c].Data);
      Phase[c].Data=NULL;
    }
  for (c=0;c<AnzProgramme;c++)
    if (c!=CurrentProc && Programm[c].Data)
    {
      FREE(Programm[c].Data);
      Programm[c].Data=NULL;
    }
  for (c=0;c<MAX_PRS;c++)
    if (Person[c].Num!=-1 && (Person[c].Bits & 33536)) return;
  MakeMegaTable(-1);
}
char *MrMalloc(long Size,char *File,short Line)
{
  char *p;
  static char Buffer[200];

  p=malloc(Size+3);
  if (p==NULL)
  {
    FreeSomeMemory();
    if ((p=malloc(Size+3))==NULL)
    {
      sprintf(Buffer,"Failed allocating %li bytes in %s, Line %i at %p\x0d\x0a",Size,File,Line,p);
      write(MallocFile,Buffer,strlen(Buffer));
      InternalError(1000);
    }
  }
//sprintf(Buffer,"Allocated %li bytes in %s, Line %i at %p\x0d\x0a",Size,File,Line,p);
//write(MallocFile,Buffer,strlen(Buffer));
  return(p);
}
void MrFree(char *p,char *File,short Line)
{
  static char Buffer[200];
//sprintf(Buffer,"Deallocated in %s, Line %i at %p\x0d\x0a",File,Line,p);
//write(MallocFile,Buffer,strlen(Buffer));
  free(p);
}

