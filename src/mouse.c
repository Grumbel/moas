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
#define HIGHLIGHT 40

#ifndef DOS
/* Suppress MouseIntelligence verb after LOOK-mode SIEHE on this press. */
static int suppress_left_verb = 0;
#endif

char _sString[]=">%s<";
char *__Benutze;
char *__BenutzeMit;
char *__Siehe;
char *__Sprich;
char *__Bewege;
char *__BewegeMit;

short IsMousePresent(void);
void MouseCheck(void);
void MouseIntelligence(void);
void MouseRead(void);

void MouseCheck(void)
{
#ifndef DOS
  /*
   * Capture previous button state, then poll SDL.
   * Last* must still differ from Left/Right when callers (GetClickOrEnter)
   * inspect the edge AFTER MouseCheck returns.
   */
  short prevL = Mouse.Left, prevR = Mouse.Right, prevM = Mouse.Middle;
  MoasMouseCheckDepth++;
  PlatformPumpEvents();
  Mouse.LastLeft = prevL;
  Mouse.LastRight = prevR;
  Mouse.LastMiddle = prevM;
  if (PlatformShouldQuit()) {
    MoasMouseCheckDepth--;
    return;
  }
  /* Dialogs turn CursorState off; restore when no window is open. */
  if (Window.Status == None && Mouse.CursorState == 0)
    Mouse.CursorState = 1;
#endif
  LookKeys();
  MouseRead();
  if (Mouse.Left==0 && Mouse.LastLeft==0) Mouse.ClickObject=-1;
  if (Mouse.Right==0 && Mouse.LastRight==1 && PlayerHasControl && Mouse.Left==0 && Mouse.LastLeft==0)
    if (WhatsAt(Mouse.x,Mouse.y)!=-1)
    {
      V_Verb=SIEHE;
      DisplayMouseText(HIGHLIGHT);
      V_Par1=WhatsAt(Mouse.x,Mouse.y);
      V_Par2=-1;
      ExecuteTheProgram();
      Mouse.LastRight=0;
    }
  if (Mouse.Right && Mouse.LastRight==0 && PlayerHasControl && V_NoWalk==0)
    if (WhatsAt(Mouse.x,Mouse.y)==-1)
      WalkClick(Mouse.x,Mouse.y);

  if (Mouse.Right==0 && Mouse.LastRight==1)
  {
    short PN=GetPlayerPersonNumber(); /*Person Number*/
    if (((PN==MAX_PRS) || (Person[PN].StepSpeed==0)) && PlayerHasControl==1 && Window.Status==None)
    {
      short T0=Var[0],T1=Var[1],T2=Var[2];
      Mouse.LastRight=0;
      V_Verb=C_WALK;
      V_Par1=-1;
      V_Par2=-1;
      ExecuteTheProgram();
      Var[0]=T0; Var[1]=T1; Var[2]=T2;
    }
  }
  if (Mouse.Left==1 && Mouse.LastLeft==0)
  {
    Mouse.ClickX=Mouse.x;
    Mouse.ClickY=Mouse.y;
    Mouse.ClickObject=WhatsAt(Mouse.x,Mouse.y);
    MoasDbg(MOAS_DBG_INPUT, "L-down at %d,%d obj=%d CursorState=%d mode=%d",
            (int)Mouse.x, (int)Mouse.y, (int)Mouse.ClickObject, (int)Mouse.CursorState,
            PlatformGetTouchMode());
#ifndef DOS
    suppress_left_verb = 0;
    /*
     * Empty ground → walk in USE and LOOK (no right button on touch).
     * Object hits: USE → MouseIntelligence verbs; LOOK → SIEHE on release.
     */
    if (PlayerHasControl && V_NoWalk == 0 && Window.Status == None
        && Mouse.ClickObject == -1)
      WalkClick(Mouse.x, Mouse.y);
#endif
  }
  if (Mouse.Left==0 && Mouse.LastLeft==1)
  {
    Mouse.ReleaseX=Mouse.x;
    Mouse.ReleaseY=Mouse.y;
    MoasDbg(MOAS_DBG_INPUT, "L-up at %d,%d", (int)Mouse.x, (int)Mouse.y);
#ifndef DOS
    /*
     * LOOK mode: object tap → SIEHE only (DOS right-click look). Never run
     * Benutze/Bewege on the same press. USE mode keeps MouseIntelligence.
     */
    if (PlatformGetTouchMode() == MOAS_TOUCH_LOOK
        && PlayerHasControl && Window.Status == None
        && Mouse.ClickObject != -1)
    {
      /* Object tap in LOOK = DOS right-click look. Suppress USE verbs on release. */
      V_Verb = SIEHE;
      V_Par1 = Mouse.ClickObject;
      V_Par2 = -1;
      ExecuteTheProgram();
      suppress_left_verb = 1;
      Mouse.ClickObject = -1;
      Mouse.BewegeMode = -1;
      MoasDbg(MOAS_DBG_INPUT, "LOOK-mode SIEHE obj=%d", (int)V_Par1);
    } else if (PlayerHasControl == 1 && Window.Status == None
               && Mouse.ClickObject == -1) {
      /* Idle C_WALK after empty-ground walk (both modes). */
      short PN = GetPlayerPersonNumber();
      if ((PN == MAX_PRS) || (Person[PN].StepSpeed == 0))
      {
        short T0 = Var[0], T1 = Var[1], T2 = Var[2];
        V_Verb = C_WALK;
        V_Par1 = -1;
        V_Par2 = -1;
        ExecuteTheProgram();
        Var[0] = T0;
        Var[1] = T1;
        Var[2] = T2;
      }
    }
#endif
  }
  if (PlayerHasControl)
    MouseIntelligence();
#ifndef DOS
  if (suppress_left_verb && Mouse.Left == 0)
    suppress_left_verb = 0;
#endif

  IveGotTooMuchTime();
#ifndef DOS
  PlatformPresent();
  PlatformDelay(16);
  MoasMouseCheckDepth--;
#endif
}
short IsMousePresent(void)
{
#ifdef DOS
  static short MousePres=-1;
  union REGS Regs;

  if (MousePres!=-1) return(MousePres);
  if (((long*)0)[51]==0l) return(MousePres=0);

  Regs.w.ax=0;
  int386(51,&Regs,&Regs);
  if (Regs.w.ax==65535)
  {
    SetMousePos(100,100);
    return (MousePres=1);
  } else return (MousePres=0);
#else
  return 1; /* SDL always has a pointer device */
#endif
}

void MouseRead(void)
{
#ifdef DOS
  /*
   * Original relative-mouse trick: pin hardware cursor at (100,100), read
   * delta, divide by adaptive MouseDiv.  On real mice MouseDiv settles to a
   * few mickeys.  DOSBox reports large pixel-like deltas and collapses Div
   * to 1 → hyperspeed.  Floor Div so emulators stay usable; still matches
   * the original algorithm otherwise.
   */
  static short MouseDivX=30000,MouseDivY=30000,LastX=-1000,LastY=-1000;
  short x=0,y=0,ox,oy;
  union REGS Regs;

  Regs.w.bx=0;
  if (IsMousePresent())
  {
    Regs.w.ax=3;
    int386(51,&Regs,&Regs);
    x=((short)(Regs.w.cx))-100;
    y=((short)(Regs.w.dx))-100;
    SetMousePos(100,100);
  }

  ox=Mouse.x-15;
  oy=Mouse.y-15;
  {
    short dx = 0, dy = 0;
    if (MouseDivX > 0) dx = (short)(x / MouseDivX);
    if (MouseDivY > 0) dy = (short)(y / MouseDivY);
    /* Cap step so one huge emulator poll cannot jump across the screen */
    if (dx > 12) dx = 12;
    if (dx < -12) dx = -12;
    if (dy > 12) dy = 12;
    if (dy < -12) dy = -12;
    Mouse.x += dx;
    Mouse.y += dy;
  }
  if (Mouse.x<0) Mouse.x=0;    if (Mouse.x>319) Mouse.x=319;
  if (Mouse.y<0) Mouse.y=0;    if (Mouse.y>199) Mouse.y=199;
  if (Window.Status==None) PowerRedrawArea(min(Mouse.x-15,ox),min(Mouse.y-15,oy),max(Mouse.x-15,ox)+59,max(Mouse.y-15,oy)+55);
                      else VMouse(2);

  if (LastX!=x && abs(LastX-x)<MouseDivX && abs(LastX-x)>0) MouseDivX=abs(LastX-x);
  if (LastY!=y && abs(LastY-y)<MouseDivY && abs(LastY-y)>0) MouseDivY=abs(LastY-y);
  /* Tiny floor only — main speed control is the per-frame cap above */
  if (MouseDivX < 2) MouseDivX = 2;
  if (MouseDivY < 2) MouseDivY = 2;

  LastX=x;  Mouse.LastLeft=Mouse.Left;
  LastY=y;  Mouse.LastRight=Mouse.Right;
            Mouse.LastMiddle=Mouse.Middle;

  Mouse.Left=(Regs.w.bx & 1) | SpaceState;
  Mouse.Right=(Regs.w.bx & 2)/2;
  Mouse.Middle=(Regs.w.bx & 4)/4;
#else
  /* SDL already updated Mouse.x/y/buttons in PlatformPumpEvents. */
  if (SpaceState)
    Mouse.Left = 1;
  /* Keep hotspot flag current so cursor shape can change while moving. */
  if (PlayerHasControl && Window.Status == None)
    ThingAtCursor = (WhatsAt(Mouse.x, Mouse.y) != -1);
  /*
   * VMouse save/restore — do not bake cursor into Screen (avoids trails).
   * As on DOS, windows (dialogs, menus) clear CursorState but still track
   * the cursor via VMouse; VMouse itself honours V_CursorState.
   */
  if (Window.Status!=None || (Mouse.CursorState==1 && V_CursorState==1))
    VMouse(2);
#endif
}

void MouseIntelligence(void)
{
  register short y;
  char prev_bewege = Mouse.BewegeMode;
  short inventar_embed = 0;

  DisplayMouseText(42);
#ifndef DOS
  /* LOOK mode: never enter Bewege (pull) while the mode is active. */
  if (PlatformGetTouchMode() == MOAS_TOUCH_LOOK)
    Mouse.BewegeMode = -1;
#endif
  if (Mouse.Left && Mouse.LastLeft==0 && Mouse.ClickObject<2000 && Mouse.ClickObject>999)
  {
    inventar_embed = 1;
    for (y=0;y<24;y++)
    {
      memcpy(Phase[0].Data+12+44*(16+y),Inventar[Mouse.ClickObject-1000].Data+y*32,32);
      memcpy(Phase[0].Data+12+44*(16+y)+1760,Inventar[Mouse.ClickObject-1000].Data+y*32,32);
    }
  }
  if (Window.Status==None && ((Mouse.LastLeft==1 && Mouse.Left==0) || (Mouse.Left==0 && Phase[0].Data[767])))
  {
    inventar_embed = 1;
    for (y=0;y<24;y++) { memset(Phase[0].Data+12+44*(16+y),0,32);
                         memset(Phase[0].Data+12+44*(16+y)+1760,0,32); }
  }
  if (Mouse.LastLeft==1 && Mouse.Left==0)
  {
    if (Window.Status==None)
    {
#ifndef DOS
      if (suppress_left_verb) {
        /* LOOK-mode SIEHE already ran — do not Benutze/Bewege on release. */
        Mouse.BewegeMode = -1;
      } else
#endif
      {
        DisplayMouseText(HIGHLIGHT);
        if (Mouse.BewegeMode!=-1)
        {
          Mouse.BewegeMode=-1;
          PowerRedrawArea(Mouse.x-15,Mouse.y-15,Mouse.x+44,Mouse.y+40);
        }
        if (V_Verb!=0 && (V_Verb!=BENUTZE || V_Par2!=-1)) ExecuteTheProgram();
      }
    }
    Mouse.ClickObject=-1;
  }
#ifndef DOS
  /*
   * MouseRead already ran VMouse(2) before DisplayMouseText. BewegeMode and
   * inventar embeds into Phase[0] change after that:
   * - BewegeMode change is seen by Type==2 (last_bewege).
   * - Inventar embed mutates Phase[0] texels without changing frame_off, so
   *   Type==2 would skip — force remove+create.
   */
  if (Window.Status!=None || (Mouse.CursorState==1 && V_CursorState==1))
  {
    if (inventar_embed)
    {
      VMouse(3);
      VMouse(1);
    }
    else if (Mouse.BewegeMode != prev_bewege)
      VMouse(2);
  }
#endif
}
short WhatsAt(short x,short y)
{
  register short c,d,BestPerson,BestPersonY;
  enum {Prs , Back} Mode=Back;

  if (V_InventarState)
    for (c=0;c<AnzInventar;c++)
      if (Inventar[c].x==-3)
        if (x>=(Inventar[c].y%10)*32 && x<=(Inventar[c].y%10)*32+31 && y>=(Inventar[c].y/10)*24 && y<=(Inventar[c].y/10)*24+23)
        {
          if (c>=31 && c<=33) /* Sonderfall: Megawumme */
            if (Inventar[c=34].Data==NULL) LadeInventar(c);
          return(1000+c);
        }

  BestPersonY=BestPerson=-1;
  for (c=0;c<MAX_PRS;c++)
    if (Person[c].x<=x && Person[c].y<=y && Person[c].Num!=-1)
      if (Person[c].x+Person[c].xs>x &&
          Person[c].y+Person[c].ys>y)
      for (d=0;d<AnzDescription;d++)
        if (Description[d].Number==Person[c].Num)
          if ((Person[c].Phase+Person[c].Sequence[Person[c].Counter][Person[c].Dir]*Person[c].xs*Person[c].ys)[x-Person[c].x+(y-Person[c].y)*Person[c].xs])
          {
            BestPerson=c;
            BestPersonY=Person[c].y+Person[c].ys+Person[c].z;
            Mode=Prs;
            break;
          }
  if (BestPersonY<=Info1[Tab320[y]+x]) Mode=Back;
  if (Mode==Back)
  {
    for (c=0;c<50;c++)
      if (Marker[c].MarkTyp==Label)
      {
        if (Marker[c].x1<=x && Marker[c].y1<=y && Marker[c].x2>=x && Marker[c].y2>=y)
          return(Marker[c].Ref);
      }
  }
  else return(Person[BestPerson].Num);
  return(-1);
}
#ifndef DOS
static short ClipRect(short *r,short x1,short y1,short x2,short y2,short Top,short Kind)
{
  short t;

  if (x1>x2) { t=x1; x1=x2; x2=t; }
  if (y1>y2) { t=y1; y1=y2; y2=t; }
  if (x1<0) x1=0;
  if (y1<Top) y1=Top;
  if (x2>319) x2=319;
  if (y2>199) y2=199;
  if (x1>x2 || y1>y2) return 0;
  r[0]=x1; r[1]=y1; r[2]=x2; r[3]=y2; r[4]=Kind;
  return 1;
}
/*
 * Hotspots for the port's highlight (H key, HINT button). Kind 0: what
 * WhatsAt can return, i.e. Label areas and the persons that have a
 * description, minus the player. Kind 1, only in master mode: Jumper
 * areas, which TestIfPersonIsInTrap fires when the player's feet walk into
 * them. Those are exits but also any other walk-in trigger, too noisy for
 * normal play.
 * Writes x1,y1,x2,y2 (inclusive, screen pixels),kind per hotspot, exits
 * first; persons get the bounds of their current frame, and everything is
 * cut off below the open inventory bar, which WhatsAt checks first.
 * Nothing while the room isn't playable (cutscene, window).
 */
int MoasGetHotspots(short *rects,int max)
{
  short c,d,x,y,pn,x1,y1,x2,y2,Top=0;
  int n=0;
  unsigned char *Frame;

  if (!Screen || Window.Status!=None || PlayerHasControl!=1)
    return 0;
  if (V_InventarState)
    for (c=0;c<AnzInventar;c++)
      if (Inventar[c].x==-3 && (Inventar[c].y/10+1)*24>Top)
        Top=(Inventar[c].y/10+1)*24;
  for (c=0;c<50 && n<max && V_Master;c++)
    if (Marker[c].MarkTyp==Jumper)
      n+=ClipRect(rects+5*n,Marker[c].x1,Marker[c].y1,Marker[c].x2,Marker[c].y2,Top,1);
  for (c=0;c<50 && n<max;c++)
    if (Marker[c].MarkTyp==Label)
      n+=ClipRect(rects+5*n,Marker[c].x1,Marker[c].y1,Marker[c].x2,Marker[c].y2,Top,0);

  pn=GetPlayerPersonNumber();
  for (c=0;c<MAX_PRS && n<max;c++)
  {
    if (c==pn || Person[c].Num==-1 || !Person[c].Phase) continue;
    for (d=0;d<AnzDescription;d++)
      if (Description[d].Number==Person[c].Num) break;
    if (d==AnzDescription) continue;
    Frame=Person[c].Phase+Person[c].Sequence[Person[c].Counter][Person[c].Dir]*Person[c].xs*Person[c].ys;
    x1=y1=32767; x2=y2=-1;
    for (y=0;y<Person[c].ys;y++)
      for (x=0;x<Person[c].xs;x++)
        if (Frame[x+y*Person[c].xs])
        {
          if (x<x1) x1=x;
          if (x>x2) x2=x;
          if (y<y1) y1=y;
          if (y>y2) y2=y;
        }
    if (x2>=0)
      n+=ClipRect(rects+5*n,Person[c].x+x1,Person[c].y+y1,Person[c].x+x2,Person[c].y+y2,Top,0);
  }
  return n;
}
#endif
void DisplayMouseText(short Color)
{
  char String[100];
  char *s1,*s2;
  short dx,dy;

  if (Window.Status!=None) return;
  if ((Mouse.Left|Mouse.LastLeft)==1) V_Par1=Mouse.ClickObject;
  else V_Par1=-1;
  if (Mouse.LastLeft==1 && Mouse.Left==0) V_Par2=WhatsAt(Mouse.ReleaseX,Mouse.ReleaseY);
  else ThingAtCursor=((V_Par2=WhatsAt(Mouse.x,Mouse.y))!=-1);
  s1=DescriptionText(V_Par1);
  s2=DescriptionText(V_Par2);
  sprintf(String,_sString,s2);
  V_Verb=0;

  Mouse.BewegeMode=-1;
  if (V_Par1!=-1 || V_Par2!=-1)
  {
    if (Mouse.Left+Mouse.LastLeft==0 && (Mouse.Right+Mouse.LastRight!=0) && V_Par2!=-1)
    {
      V_Verb=SIEHE;
      sprintf(String,__Siehe,s2);
    }
    else
    if ((V_Par1>=2000 || (V_Par1>=0 && V_Par1<1000)) && V_Par2!=V_Par1)
    {
      V_Verb=BEWEGE;
      dx=Mouse.ClickX-Mouse.x;
      dy=Mouse.ClickY-Mouse.y;
      if (abs(dy)>=2*abs(dx))
        if (dy>0) Mouse.BewegeMode=0;
             else Mouse.BewegeMode=4;
      else if (abs(dy)*2>=abs(dx))
        if (dy>0) if (dx<0) Mouse.BewegeMode=1;
                       else Mouse.BewegeMode=7;
             else if (dx<0) Mouse.BewegeMode=3;
                       else Mouse.BewegeMode=5;
      else if (dx<0) Mouse.BewegeMode=2;
                else Mouse.BewegeMode=6;
      if (V_Par2==-1 || (V_Par2>=1000 && V_Par2<2000)) sprintf(String,__Bewege,s1);
      else { sprintf(String,__BewegeMit,s1,s2); Mouse.BewegeMode+=8; }
    }
    if (*String=='>')
    {
      if (V_Par1<1000 && V_Par1==V_Par2 && Person[GetRealPersonNum(V_Par1)].Bits & 4)
      {
        sprintf(String,__Sprich,s1);
        V_Verb=SPRICH;
      }
      else
        if (V_Par1!=-1)
        {
          short c;

          V_Verb=BENUTZE;
          if (V_Par1==V_Par2)
          {
            for (c=0;c<MAX_PARSE;c++)
              if (PowerParse[c].Nummer==V_Par1) break;
            if (c==MAX_PARSE) sprintf(String,__Benutze,s1);
                         else snprintf(String, sizeof(String), "%s", (const char *)PowerParse[c].String);
          }
          else
          {
            if (V_Par2==-1) s2="???";
            for (c=0;c<MAX_PARSE;c++)
              if (PowerParse[c].Nummer==V_Par1+10000) break;
            if (c==MAX_PARSE) sprintf(String,__BenutzeMit,s1,s2);
                         else {
              /* Format string comes from game data; must not trip -Wformat-security. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
              snprintf(String, sizeof(String), (const char *)PowerParse[c].String, s2);
#pragma GCC diagnostic pop
            }
          }
        }
    }
  }
  DisplayMessage(String,Color);
}
char *DescriptionText(short Num)
{
  short c;

  for (c=0;c<AnzDescription;c++)
    if (Description[c].Number==Num)
      return(Description[c].Name);
  return("???");
}
void VMouse(short Type) /* 1=Create, 2=Update, 3=Remove */
{
#ifdef DOS
  /* Exact original soft-cursor overlay (VGA Mode 13h). */
  register unsigned short x,y;
  unsigned static ox,oy,State=0;

  if (Type==1 && State==0)
  {
    for (y=0;y<40 && y+Mouse.y<200;y++)
      memcpy(VMouseBuf+y*44,(char *)0xA0000+Mouse.x+(Mouse.y+y)*320,44);
    if (V_CursorState==1)
      for (y=0;y<40 && y+Mouse.y<200;y++)
        for (x=0;x<44 && x+Mouse.x<320;x++)
          if (RefTable[0].Num==-1)
          {
            if (Phase[0].Data[x+y*44+3520])
              ((unsigned char*)0xA0000)[x+Mouse.x+(y+Mouse.y)*320]=Phase[0].Data[x+y*44+3520];
          }
          else
          {
            if (Phase[0].Data[x+y*44+1760])
              ((unsigned char*)0xA0000)[x+Mouse.x+(y+Mouse.y)*320]=Phase[0].Data[x+y*44+1760];
          }
    State=1;
  }
  if (Type==2)
  {
    if (Mouse.x!=ox || Mouse.y!=oy)
    {
      VMouse(3);
      VMouse(1);
    }
  }
  if (Type==3 && State==1)
  {
    for (y=0;y<40 && y+oy<200;y++)
      memcpy((char *)0xA0000+ox+(oy+y)*320,VMouseBuf+y*44,44);
    State=0;
  }
  ox=Mouse.x;
  oy=Mouse.y;
#else
  register short x,y;
  static short ox,oy,State=0;
  static short last_frame = -1;
  static char last_bewege = -2;
  static short save_x,save_y,saved_w,saved_h;
  short frame_off;
  short cx,cy,cw,ch,stride;
  unsigned char *src;
  char use_bewege;

  if (!VMouseBuf || !Phase || !Phase[0].Data)
    return;

  use_bewege = (Mouse.BewegeMode != -1 && Phase[1].Data != NULL);
  if (use_bewege)
  {
    cx = (short)(Mouse.x - 15);
    cy = (short)(Mouse.y - 15);
    cw = 30;
    ch = 30;
    frame_off = (short)(900 * (short)Mouse.BewegeMode);
    src = Phase[1].Data;
    stride = 30;
  }
  else
  {
    cx = Mouse.x;
    cy = Mouse.y;
    cw = 44;
    ch = 40;
    if (!PlayerHasControl)
      frame_off = 3520;
    else
      frame_off = (short)(1760 * (ThingAtCursor ? 1 : 0));
    src = Phase[0].Data;
    stride = 44;
  }

  if (Type==1 && State==0)
  {
    save_x = cx;
    save_y = cy;
    saved_w = cw;
    saved_h = ch;
    for (y=0; y<ch; y++)
    {
      short sy = (short)(cy + y);
      if (sy < 0 || sy >= 200)
        continue;
      for (x=0; x<cw; x++)
      {
        short sx = (short)(cx + x);
        if (sx < 0 || sx >= 320)
          continue;
        VMouseBuf[y*44 + x] = (unsigned char)(VGA_ADRESSE)[sx + sy*320];
      }
    }
    if (V_CursorState==1)
      for (y=0; y<ch; y++)
      {
        short sy = (short)(cy + y);
        if (sy < 0 || sy >= 200)
          continue;
        for (x=0; x<cw; x++)
        {
          short sx = (short)(cx + x);
          unsigned char pix;
          if (sx < 0 || sx >= 320)
            continue;
          pix = src[x + y*stride + frame_off];
          if (pix)
            (VGA_ADRESSE)[sx + sy*320] = pix;
        }
      }
    last_frame = frame_off;
    last_bewege = Mouse.BewegeMode;
    State=1;
  }
  if (Type==2)
  {
    if (Mouse.x!=ox || Mouse.y!=oy || last_frame != frame_off
        || last_bewege != Mouse.BewegeMode)
    {
      VMouse(3);
      VMouse(1);
    }
  }
  if (Type==3 && State==1)
  {
    for (y=0; y<saved_h; y++)
    {
      short sy = (short)(save_y + y);
      if (sy < 0 || sy >= 200)
        continue;
      for (x=0; x<saved_w; x++)
      {
        short sx = (short)(save_x + x);
        if (sx < 0 || sx >= 320)
          continue;
        (VGA_ADRESSE)[sx + sy*320] = VMouseBuf[y*44 + x];
      }
    }
    State=0;
  }
  ox=Mouse.x;
  oy=Mouse.y;
#endif
}
void AddPowerParse(short Nummer,char *String)
{
  short c;

  if (Nummer==-1)
  {
    for (c=0;c<MAX_PARSE;c++)
      if (PowerParse[c].Nummer!=-1)
      {
        PowerParse[c].Nummer=-1;
        FREE(PowerParse[c].String);
      }
    return;
  }

  for (c=0;c<MAX_PARSE;c++)
    if (PowerParse[c].Nummer==Nummer)
    {
      FREE(PowerParse[c].String);
      if (*String==0) PowerParse[c].Nummer=-1;
      else
      {
        PowerParse[c].String=MALLOC(strlen(String)+1);
        strcpy(PowerParse[c].String,String);
      }
      return;
    }

  for (c=0;c<MAX_PARSE;c++)
    if (PowerParse[c].Nummer==-1)
    {
      if (*String)
      {
        PowerParse[c].String=MALLOC(strlen(String)+1);
        strcpy(PowerParse[c].String,String);
        PowerParse[c].Nummer=Nummer;
      }
      return;
    }
  InternalError(1038);
}
void WalkClick(short x,short y) /* Click an legale Position umleiten */
{
  short c,Real=GetPlayerPersonNumber();

  for (c=0;c<200;c++)
  {
    if (IsPositionOk(Real,x-Person[Real].xs/2,y-Person[Real].ys-c) && y-c>=0 && y-c<200)
    {
      iWalk(-1,x-Person[Real].xs/2,y-Person[Real].ys-c);
      break;
    }
    if (IsPositionOk(Real,x-Person[Real].xs/2,y-Person[Real].ys+c) && y+c>=0 && y+c<200)
    {
      iWalk(-1,x-Person[Real].xs/2,y-Person[Real].ys+c);
      break;
    }
  }
}

#ifndef DOS
/*
 * Gamepad left stick / D-pad: walk toward a point offset from the player.
 * ax,ay in roughly -1000..1000 (platform scales stick axes).
 */
void MoasPadWalk(int ax, int ay)
{
  short pn, px, py, tx, ty;
  if (ax > -180 && ax < 180 && ay > -180 && ay < 180)
    return;
  if (!PlayerHasControl || V_NoWalk || Window.Status != None)
    return;
  pn = GetPlayerPersonNumber();
  if (pn < 0 || pn > MAX_PRS)
    return;
  px = (short)(Person[pn].x + Person[pn].xs / 2);
  py = (short)(Person[pn].y + Person[pn].ys);
  tx = (short)(px + (ax * 55) / 1000);
  ty = (short)(py + (ay * 40) / 1000);
  if (tx < 0) tx = 0;
  if (ty < 0) ty = 0;
  if (tx > 319) tx = 319;
  if (ty > 199) ty = 199;
  WalkClick(tx, ty);
}
#endif
void SetMousePos(short x,short y)
{
#ifdef DOS
  union REGS Regs;

  Regs.w.ax=4;
  Regs.w.cx=x;
  Regs.w.dx=y;
  int386(51,&Regs,&Regs);
#else
  PlatformSetMousePos(x, y);
#endif
}

