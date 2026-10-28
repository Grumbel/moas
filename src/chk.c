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
#include "datapath.h"

unsigned char __b1,__b2,__b3,__b4;
extern char *_FRAGEZEICHEN;

CHKSUM MkChk(unsigned char *Array,short Length)
{
  register short c;

  ClrChk();

  for (c=0;c<Length;c++)
    SingleChk(Array[c]);

  return(GetChk());
}
void ClrChk(void)
{
  #ifdef debug
    printf("\nClrChk");
  #endif
  __b1=__b2=__b3=__b4=0;
}
void SingleChk(unsigned char c)
{
  #ifdef debug
    printf("\n:%i",(int)c);
  #endif

  __b1+=c;
  __b2^=c;
  __b3++;
  __b4^=(c+__b3);
}
CHKSUM GetChk(void)
{
  #ifdef debug
    printf("\nGetChk");
  #endif
  return(__b1+((CHKSUM)__b2*256)+((CHKSUM)__b3*65536)+((CHKSUM)__b4*256*65536));
}
void AreaChk(unsigned char *Array,long Length)
{
  register long c;

  for (c=0;c<Length;c++)
    SingleChk(Array[c]);
}
short ChkOpen(char *FileName,char Mode)
{
  short xfil = -1;
  static short LastMode=-1;

  if (Mode==MODE)
    return(LastMode);
  else
    LastMode=Mode;
  ClrChk();
  if (_FRAGEZEICHEN && FileName && strcmp(FileName,_FRAGEZEICHEN)==0) return(-1);
  if (Mode==READ)
#ifdef DOS
    xfil=open(FileName,O_RDONLY|O_BINARY);
#else
    xfil=MoasOpenData(FileName);
#endif
  if (Mode==WRITE)
#if defined(DOS) || defined(_WIN32)
    xfil=open(FileName,O_TRUNC|O_CREAT|O_WRONLY|O_BINARY,S_IWRITE);
#else
    xfil=open(FileName,O_TRUNC|O_CREAT|O_WRONLY,S_IWRITE);
#endif
  return(xfil);
}
void ChkRead(short xfil,char *Target,long Length)
{
  if (Length>0)
  {
    if (read(xfil,Target,Length)!=Length)
      InternalError(1005);
    AreaChk(Target,Length);
  }
}
void ChkWrite(short xfil,char *Target,long Length)
{
  if (Length>0)
  {
     if (write(xfil,Target,Length)!=Length)
      InternalError(1006);
    AreaChk(Target,Length);
  }
}
void ChkClose(short xfil)
{
  CHKSUM Check;

  if (ChkOpen(NULL,MODE)==READ)
  {
    if (read(xfil,(char*)&Check,sizeof( CHKSUM ))!=sizeof( CHKSUM ))
      InternalError(1005);
    if (Check!=GetChk())
      InternalError(1007);
  }
  else
  {
    Check=GetChk();
    if (write(xfil,(char*)&Check,sizeof( CHKSUM ))!=sizeof( CHKSUM ))
      InternalError(1006);
  }
  close(xfil);
}

