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

void ShowHercBak(void)
{
  /*
   * Master debug (key '#'): person / say-window dump.
   * DOS used VGA text mode at 0xB8000; non-DOS prints to stderr only
   * (never write absolute text-mode addresses).
   */
  static unsigned char Buffer[80],B2[20];
  short c,y;

#ifdef DOS
  _setvideomode(_DEFAULTMODE);

  for (c=0;c<35;c++) ((unsigned short*)(0xB8000))[c]=0x0f00+"Prs ### x    y    z    tx   ty   Bits"[c];
  for (y=0;y<20;y++)
  {
    ltoa(Person[y].Bits,B2,2);
    while (strlen(B2)<16)
    {
      memmove(B2+1,B2,16);
      B2[0]='0';
    }
    sprintf(Buffer,"%-3i %-3i %-4i %-4i %-4i %-4i %-4i %s",y,Person[y].Num,Person[y].x,Person[y].y,Person[y].z,Person[y].TargetX,Person[y].TargetY,B2);
    if (Person[y].Bits&32 && Person[y].Num!=-1)
      for (c=0;c<46;c++) if (Buffer[c]==32) Buffer[c]=250;
    for (c=0;c<46;c++) ((unsigned short*)(0xB80A0+160*y))[c]=0x0f00+Buffer[c];
  }
  for (y=0;y<5;y++)
  {
    memcpy(B2,SayWindow[y].Text,15);
    for (c=0;c<15;c++) if (!B2[c]) break;
    for (;c<15;c++) B2[c]=32;
    sprintf(Buffer,"  %i     %-2i %-4i %s",c,SayWindow[y].RefPrs,SayWindow[y].Time,B2);
    for (c=0;c<32;c++) ((unsigned short*)(0xB8240+160*y))[c]=0x0f00+Buffer[c];
  }
  for (y=0;y<24;y++) ((unsigned short*)(0xB805e+160*y))[0]=0x0fB3;
  sprintf(Buffer,"Mouse:[%i|%i] %c%c",Mouse.x,Mouse.y,Mouse.Left ? 'L' : 32,Mouse.Right ? 'R' : 32);
  for (c=0;c<18;c++) ((unsigned short*)(0xB8060))[c]=0x0f00+Buffer[c];
  strcpy(Buffer,"SayWin Prs Time Text");
  for (c=0;c<20;c++) ((unsigned short*)(0xB81a0))[c]=0x0f00+Buffer[c];
  for (c=0;c<33;c++) ((unsigned short*)(0xB8A5e))[c]=((unsigned short*)(0xB855e))[c]=((unsigned short*)(0xB80Fc))[c]=0x0f00+((unsigned char*)"ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ")[c];

  getch();
  _setvideomode(_MRES256COLOR);
#else
  fprintf(stderr, "\n=== ShowHercBak (Master debug) ===\n");
  fprintf(stderr, "Prs ### x    y    z    tx   ty   Bits\n");
  for (y=0;y<20;y++)
  {
    ltoa(Person[y].Bits,(char *)B2,2);
    while (strlen((char *)B2)<16)
    {
      memmove(B2+1,B2,16);
      B2[0]='0';
    }
    sprintf((char *)Buffer,"%-3i %-3i %-4i %-4i %-4i %-4i %-4i %s",
            y,Person[y].Num,Person[y].x,Person[y].y,Person[y].z,
            Person[y].TargetX,Person[y].TargetY,B2);
    fprintf(stderr, "%s\n", Buffer);
  }
  fprintf(stderr, "SayWin Prs Time Text\n");
  for (y=0;y<5;y++)
  {
    if (SayWindow[y].Text)
      memcpy(B2,SayWindow[y].Text,15);
    else
      memset(B2,0,15);
    for (c=0;c<15;c++) if (!B2[c]) break;
    for (;c<15;c++) B2[c]=32;
    B2[15]=0;
    sprintf((char *)Buffer,"  %i     %-2i %-4i %s",c,SayWindow[y].RefPrs,SayWindow[y].Time,B2);
    fprintf(stderr, "%s\n", Buffer);
  }
  fprintf(stderr, "Mouse:[%i|%i] %c%c\n",
          Mouse.x,Mouse.y,Mouse.Left ? 'L' : ' ',Mouse.Right ? 'R' : ' ');
  fprintf(stderr, "(press any key to continue)\n");
  getch();
#endif
  Blend(64,1);
  PowerRedrawArea(0,0,320,200);
}

