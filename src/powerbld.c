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
#define MOUSE_DEL    1
unsigned char *WalkNLayers;
unsigned char *Screen;
void ShowMouse(short Flag);
char KeyAbortState;

void PowerBlend(short Mode)
{
  register unsigned short c;
  register unsigned char d,Largest=0;
  long Time;

  for (c=0;c<64000;c++)
    if (WalkNLayers[c]>Largest) Largest=WalkNLayers[c];

  ShowMouse(MOUSE_DEL);
  if (Mode)
  {
    for (d=0;d<=Largest && KeyAbortState!=2;d++)
    {
      while (Time==*((long*)0x0000046C));
      Time=*((long*)0x0000046C);
      for (c=0;c<64000;c++)
        if (WalkNLayers[c]==d)
          ((unsigned char *)(0xA0000000))[c]=Screen[c];
    }
  }
  else
  {
    for (d=Largest;d!=255 && KeyAbortState!=2;d--)
    {
      while (Time==*((long*)0x0000046C));
      Time=*((long*)0x0000046C);
      for (c=0;c<64000;c++)
        if (WalkNLayers[c]==d)
          ((unsigned char *)(0xA0000000))[c]=0;
    }
    if (KeyAbortState==2) memset(Screen,0,64000);
  }
}

