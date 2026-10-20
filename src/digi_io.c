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
#include <stdint.h>
#include <string.h>
#ifndef DOS
#include <SDL.h>
#include "datapath.h"
#include "platform.h"
#endif

#ifdef DOS
void InitDSP(void);
#endif
void InitVolumeTab(void);
void InitStepTable(void);
void GetEnvirenment(void);

#ifndef DOS
static SDL_AudioDeviceID digi_audio_dev = 0;

/* One mixed frame into SampleBuffer2 (was MySoundIrq body without DMA). */
static void DigiMixFrame(void);
static void digi_audio_callback(void *userdata, Uint8 *stream, int len);

void DigiStartSdlAudio(void)
{
  SDL_AudioSpec want, have;
  if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      fprintf(stderr, "moas: SDL audio init failed: %s\n", SDL_GetError());
      return;
    }
  }
  SDL_zero(want);
  want.freq = 14000;
  want.format = AUDIO_U8;
  want.channels = 1;
  want.samples = 1024;
  want.callback = digi_audio_callback;
  digi_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (!digi_audio_dev) {
    fprintf(stderr, "moas: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    return;
  }
  SDL_PauseAudioDevice(digi_audio_dev, 0);
}

void DigiStopSdlAudio(void)
{
  if (digi_audio_dev) {
    SDL_CloseAudioDevice(digi_audio_dev);
    digi_audio_dev = 0;
  }
}
#endif

void InitDigi(void)
{
  short c;

  SampleSize=1000;
  SpeechVolume=0;                                          // 100% Sound
  GlobalVolume=255;                                        // voll stoff
  DefInsPos[8]=0xffffffff;
  DefInsPos[9]=0xffffffff;
  ExitCommunicator=0;

  SoftIRQ[ 0]=8;
  SoftIRQ[ 1]=9;
  SoftIRQ[ 2]=10;
  SoftIRQ[ 3]=11;
  SoftIRQ[ 4]=12;
  SoftIRQ[ 5]=13;
  SoftIRQ[ 6]=14;
  SoftIRQ[ 7]=15;
  SoftIRQ[ 8]=0x70;
  SoftIRQ[ 9]=0x71;
  SoftIRQ[10]=0x72;
  SoftIRQ[11]=0x73;
  SoftIRQ[12]=0x74;
  SoftIRQ[13]=0x75;
  SoftIRQ[14]=0x76;
  SoftIRQ[15]=0x77;

  SampleBuffer0=(uChar*)D32DosMemAlloc(15000);  //notwendig im DOS
  SampleBuffer1=(uChar*)D32DosMemAlloc(15000);  //notwendig im DOS
  SampleBuffer2=(uChar*)D32DosMemAlloc(15000);  //notwendig im DOS
  OverdriveBuffer=(short*)D32DosMemAlloc(40000);
  memset(SampleBuffer0,0,15000);
  memset(SampleBuffer1,128,15000);
  memset(SampleBuffer2,128,15000);
  for (c=0;c<64;c++) Sample[c].Data=NULL;

#ifdef DOS
  GetEnvirenment();
  InitDSP();
#else
  /* No SB hardware — tables + SDL stream only. */
#endif
  InitVolumeTab();
  InitStepTable();
  /* SDL audio started later from InitAll after PlatformInit (Android). */
}

#ifdef DOS
void InitDSP(void)
{
  short c;

  /*InitDsp*/
  OutpWait(Config.BaseNum+6,1);
  OutpWait(Config.BaseNum+6,0);
  for (c=0;c<10000;c++)
    if (inp(Config.BaseNum+0xa)==0xAA) break;
  if (c==10000) InternalError(1048);

  OutpWait(Config.BaseNum+0xC,0xD1);  /*Speaker on*/

  OutpWait(Config.BaseNum+0xC,0x40);
  OutpWait(Config.BaseNum+0xC,184);   /*14000 Hz*/
}
#endif

void InitVolumeTab(void)
{
  short c,d;

  for (c=0;c<32;c++)
  {
    for (d=0;d<128;d++)   VolumeTab[c*256+d]=d*c/32;
    for (d=128;d<256;d++) VolumeTab[c*256+d]=-((256-d)*c/32);
  }
  for (c=0;c<5120;c++)
  {
    OverdriveRemapper[c]=c-0x0A0A+128;
    if (c-0x0A0A+128>255) OverdriveRemapper[c]=255;
    if (c-0x0A0A+128<0) OverdriveRemapper[c]=0;
  }
}

void InitStepTable(void)
{
  StepTable[ 0]=(long)(256*261.7/261.7/2);
  StepTable[ 1]=(long)(256*277.2/261.7/2);
  StepTable[ 2]=(long)(256*293.7/261.7/2);
  StepTable[ 3]=(long)(256*311.1/261.7/2);
  StepTable[ 4]=(long)(256*329.6/261.7/2);
  StepTable[ 5]=(long)(256*349.2/261.7/2);
  StepTable[ 6]=(long)(256*370.0/261.7/2);
  StepTable[ 7]=(long)(256*392.0/261.7/2);
  StepTable[ 8]=(long)(256*415.3/261.7/2);
  StepTable[ 9]=(long)(256*440.0/261.7/2);
  StepTable[10]=(long)(256*466.2/261.7/2);
  StepTable[11]=(long)(256*493.9/261.7/2);

  StepTable[16]=(long)(256*261.7/261.7);         /* C   */
  StepTable[17]=(long)(256*277.2/261.7);         /* Cis */
  StepTable[18]=(long)(256*293.7/261.7);         /* D   */
  StepTable[19]=(long)(256*311.1/261.7);         /* Dis */
  StepTable[20]=(long)(256*329.6/261.7);         /* E   */
  StepTable[21]=(long)(256*349.2/261.7);         /* F   */
  StepTable[22]=(long)(256*370.0/261.7);         /* Fis */
  StepTable[23]=(long)(256*392.0/261.7);         /* G   */
  StepTable[24]=(long)(256*415.3/261.7);         /* Gis */
  StepTable[25]=(long)(256*440.0/261.7);         /* A   */
  StepTable[26]=(long)(256*466.2/261.7);         /* Ais */
  StepTable[27]=(long)(256*493.9/261.7);         /* H   */

  StepTable[32]=(long)(256*261.7/261.7*2);
  StepTable[33]=(long)(256*277.2/261.7*2);
  StepTable[34]=(long)(256*293.7/261.7*2);
  StepTable[35]=(long)(256*311.1/261.7*2);
  StepTable[36]=(long)(256*329.6/261.7*2);
  StepTable[37]=(long)(256*349.2/261.7*2);
  StepTable[38]=(long)(256*370.0/261.7*2);
  StepTable[39]=(long)(256*392.0/261.7*2);
  StepTable[40]=(long)(256*415.3/261.7*2);
  StepTable[41]=(long)(256*440.0/261.7*2);
  StepTable[42]=(long)(256*466.2/261.7*2);
  StepTable[43]=(long)(256*493.9/261.7*2);

  StepTable[48]=(long)(256*261.7/261.7*4);
  StepTable[49]=(long)(256*277.2/261.7*4);
  StepTable[50]=(long)(256*293.7/261.7*4);
  StepTable[51]=(long)(256*311.1/261.7*4);
  StepTable[52]=(long)(256*329.6/261.7*4);
  StepTable[53]=(long)(256*349.2/261.7*4);
  StepTable[54]=(long)(256*370.0/261.7*4);
  StepTable[55]=(long)(256*392.0/261.7*4);
  StepTable[56]=(long)(256*415.3/261.7*4);
  StepTable[57]=(long)(256*440.0/261.7*4);
  StepTable[58]=(long)(256*466.2/261.7*4);
  StepTable[59]=(long)(256*493.9/261.7*4);
}

void GetEnvirenment(void)
{
  char *p=getenv("BLASTER");     /* BLASTER=A220 I7 D1 T3 */

  while (*p!='A') p++;
  Config.BaseNum=(short)strtol(p+1,NULL,16);

  while (*p!='I') p++;
  Config.IrqNum=atoi(p+1);

  while (*p!='D') p++;
  Config.DmaNum=atoi(p+1);

  if (Config.BaseNum!=0x210 && Config.BaseNum!=0x220 && Config.BaseNum!=0x230 && Config.BaseNum!=0x240 && Config.BaseNum!=0x250 && Config.BaseNum!=0x260)
    Config.BaseNum=0x220;
  if (Config.IrqNum!=2 && Config.IrqNum!=3 && Config.IrqNum!=5 && Config.IrqNum!=7)
    Config.IrqNum=7;
  if (Config.DmaNum!=1 && Config.DmaNum!=3)
    Config.DmaNum=1;

  if (Config.DmaNum==1)
  {
    DMA_ADRESS_Maske=0x0a; DMA_ADRESS_LoescheFF=0x0c; DMA_ADRESS_Modus=0x0b;
    DMA_ADRESS_Adresse=0x02; DMA_ADRESS_Seite=0x83; DMA_ADRESS_Zaehler=0x03;
  }

  if (Config.DmaNum==3)
  {
    DMA_ADRESS_Maske=0xD4; DMA_ADRESS_LoescheFF=0xD8; DMA_ADRESS_Modus=0xD6;
    DMA_ADRESS_Adresse=0xC4; DMA_ADRESS_Seite=0x88; DMA_ADRESS_Zaehler=0xC6;
  }
}

void *D32DosMemAlloc(long size)
{
#ifdef DOS
  union REGS Regs;

  Regs.x.eax = 0x0100;           /* DPMI allocate DOS memory */
  Regs.x.ebx = (size + 15) >> 4; /* Number of paragraphs requested */
  int386(0x31,&Regs, &Regs);

  if (Regs.x.cflag) InternalError(1049);
  return((void *)((Regs.x.eax & 0xFFFF) << 4));
#else
  void *p = malloc((size_t)size);
  if (!p) InternalError(1049);
  return p;
#endif
}

void LoadMusic(short Num)
{
  short c,Anz,ifil;
  char Name[80];
#ifndef DOS
#pragma pack(push, 1)
  struct SampleDisk {
    unsigned char Name[40];
    int32_t Rate;
    int32_t Size;
    uint32_t DataFlag; /* non-zero => sample payload follows */
  } disk;
#pragma pack(pop)
#endif

  V_Play=CurrentLine=0;
  /* Legacy music container name; no PIZZZA*.TSS in the Mänick data set. */
  /* sprintf(Name,"PIZZZA%i.TSS",Num); */
  sprintf(Name,"GAME%i.TSS",Num); /* still absent in manick/; load is optional */
#ifndef DOS
  ifil=MoasOpenData(Name);
#else
  ifil=open(Name,O_RDONLY|O_BINARY);
#endif

  if (ifil<0) {
#ifndef DOS
    /* Music is optional; MoasOpenData already logs on failure when verbose. */
#endif
    return;
  }
  lseek(ifil,80,SEEK_CUR);         /* Ignore SongTitle */
  read(ifil,Song,sizeof(Song));
  read(ifil,&SongBegin,2);
  read(ifil,&SongEnd,2);
  lseek(ifil,64,SEEK_CUR);         /* Ignore ColTitle*/

  read(ifil,(char*)&Anz,2);
  lseek(ifil,Anz*10,SEEK_CUR);  /* Ignore RowTitles*/

  for (c=0;c<64;c++)
    if (Sample[c].Data!=NULL) { FREE((char*)Sample[c].Data); Sample[c].Data=NULL; }

#ifdef DOS
  read(ifil,(char*)Sample,sizeof(Sample));
  for (c=0;c<64;c++)
    if (Sample[c].Data!=NULL)
    {
      Sample[c].Data=(uChar*)MALLOC(Sample[c].Size);
      read(ifil,Sample[c].Data,Sample[c].Size);
    }
#else
  for (c=0;c<64;c++) {
    if (read(ifil, &disk, sizeof(disk)) != (ssize_t)sizeof(disk)) {
      memset(&Sample[c], 0, sizeof(Sample[c]));
      continue;
    }
    memcpy(Sample[c].Name, disk.Name, 40);
    Sample[c].Rate = disk.Rate;
    Sample[c].Size = disk.Size;
    Sample[c].Data = NULL;
    if (disk.DataFlag != 0 && disk.Size > 0) {
      Sample[c].Data = (uChar*)MALLOC(disk.Size);
      if (Sample[c].Data)
        read(ifil, Sample[c].Data, disk.Size);
    }
  }
#endif

  close(ifil);
  V_Play=1;
#ifndef DOS
  fprintf(stderr, "moas: LoadMusic(%d) SongBegin=%d SongEnd=%d\n",
          (int)Num, (int)SongBegin, (int)SongEnd);
#endif
}

#ifndef DOS
/* Shared tracker step + mix (DOS: inside MySoundIrq). */
static void DigiMixFrame(void)
{
  short c;
  uShort GoToLine = 0xFFFF;

  if (!V_Play || !SampleBuffer2 || !OverdriveBuffer)
    return;

  if (SampleSize > 14000)
    SampleSize = 1000;

  memset(OverdriveBuffer, 0x0a0a, SampleSize * 2 + 2);

  for (c = 0; c < 8; c++) {
    if ((Song[CurrentLine].Col[c] >> 11) != 0x1f &&
        (Song[CurrentLine].Col[c] & 15) != 13 &&
        (Song[CurrentLine].Col[c] & 15) != 14) {
      DefIns[c] = (Song[CurrentLine].Col[c] >> 11);
      DefInsPos[c] = 0;
    }
    if (((Song[CurrentLine].Col[c] >> 6) & 31) != 0 &&
        (Song[CurrentLine].Col[c] & 15) != 13 &&
        (Song[CurrentLine].Col[c] & 15) != 14)
      DefVol[c] = (Song[CurrentLine].Col[c] >> 6) & 31;
    if ((Song[CurrentLine].Col[c] & 15) != 15) {
      if ((Song[CurrentLine].Col[c] & 15) == 13)
        SampleSize = 4000 - ((Song[CurrentLine].Col[c] >> 6) & 31) * 3000 / 31;
      else if ((Song[CurrentLine].Col[c] & 15) == 14) {
        if (((Song[CurrentLine].Col[c] >> 6) & 31) == 31 && ForNextValue > 0) {
          ForNextValue--;
          GoToLine = ForNextPosition;
        }
        if (((Song[CurrentLine].Col[c] >> 6) & 31) < 31 && ForNextPosition != CurrentLine) {
          ForNextValue = ((Song[CurrentLine].Col[c] >> 6) & 31) - 1;
          ForNextPosition = CurrentLine;
        }
      } else
        DefNote[c] = Song[CurrentLine].Col[c] & 63;
    }
    if ((DefInsPos[c] >> 8) > (uLong)Sample[DefIns[c]].Size)
      DefNote[c] |= 15;
    SoundMixer(c);
  }
  SoundMixer(8);
  SoundMixer(9);
  ApplyOverdrive();

  if (CurrentLine > SongEnd) {
    CurrentLine = SongBegin;
  } else if (GoToLine == 0xFFFF)
    CurrentLine++;
  else
    CurrentLine = GoToLine;
}

static void digi_audio_callback(void *userdata, Uint8 *stream, int len)
{
  int filled = 0;
  (void)userdata;
  if (!stream || len <= 0)
    return;
  while (filled < len) {
    if (!V_Play || !SampleBuffer2) {
      memset(stream + filled, 128, (size_t)(len - filled));
      break;
    }
    DigiMixFrame();
    {
      int n = (int)SampleSize;
      if (n > len - filled)
        n = len - filled;
      if (n > 0) {
        memcpy(stream + filled, SampleBuffer2, (size_t)n);
        filled += n;
      } else {
        stream[filled++] = 128;
      }
    }
  }
}
#endif

#ifdef DOS
void __interrupt /*__far*/ MySoundIrq(void)
{
  static uShort c,GoToLine;
  static uChar *h;

  h=SampleBuffer1; SampleBuffer1=SampleBuffer2; SampleBuffer2=h;
  inp(Config.BaseNum+0xe);  /* confirm Irq */

  /* Die Musik */
  if (ExitCommunicator==0)
  {
    if (V_Play)
    {
      /*DisableIRQs*/
//    _disable();
//    OutpWait(0x21,inp(0x21)&128);

      /*DMA-Controller*/
      outp(DMA_ADRESS_Maske,0x05);
      outp(DMA_ADRESS_LoescheFF,0x00);
 //   outp(DMA_ADRESS_Modus,0x49);
      outp(DMA_ADRESS_Adresse,((uLong)SampleBuffer1)&255);
      outp(DMA_ADRESS_Adresse,(((uLong)SampleBuffer1)&0xff00)>>8);
      outp(DMA_ADRESS_Seite,((uLong)SampleBuffer1)>>16);
      outp(DMA_ADRESS_Zaehler,(SampleSize+1)&255);
      outp(DMA_ADRESS_Zaehler,(SampleSize+1)>>8);
      outp(DMA_ADRESS_Maske,0x01);

      /*StartChannel*/
      OutpWait(Config.BaseNum+0xC,0x14);
      OutpWait(Config.BaseNum+0xC,SampleSize&255);
      OutpWait(Config.BaseNum+0xC,SampleSize>>8);

      /*EnableIRQs*/
//    _enable();
//    outp(0x21,inp(0x21)&127);
      outp(0x20,0x20);

      GoToLine=0xFFFF;
      for (c=0;c<8;c++)
      {
        /* Change Instrument */
        if ((Song[CurrentLine].Col[c]>>11)!=0x1f && (Song[CurrentLine].Col[c]&15)!=13 && (Song[CurrentLine].Col[c]&15)!=14)
        {
          DefIns[c]=(Song[CurrentLine].Col[c]>>11);
          DefInsPos[c]=0;
        }

        /* Change Volume */
        if (((Song[CurrentLine].Col[c]>>6)&31)!=0 && (Song[CurrentLine].Col[c]&15)!=13 && (Song[CurrentLine].Col[c]&15)!=14)
          DefVol[c]=(Song[CurrentLine].Col[c]>>6)&31;

        /* Play Note */
        if ((Song[CurrentLine].Col[c]&15)!=15)
        {
          if ((Song[CurrentLine].Col[c]&15)==13)           // Set SPEED
            SampleSize=4000-((Song[CurrentLine].Col[c]>>6)&31)*3000/31;
          else if ((Song[CurrentLine].Col[c]&15)==14)      // FOR-NEXT
          {
            if (((Song[CurrentLine].Col[c]>>6)&31)==31 && ForNextValue>0)
            {
              ForNextValue--;
              GoToLine=ForNextPosition;
            }
            if (((Song[CurrentLine].Col[c]>>6)&31)<31 && ForNextPosition!=CurrentLine)
            {
              ForNextValue=((Song[CurrentLine].Col[c]>>6)&31)-1;
              ForNextPosition=CurrentLine;
            }
          }
          else DefNote[c]=Song[CurrentLine].Col[c]&63;
        }

        if (DefVol[c]>31) DefVol[c]=31;
      }
      memset(OverdriveBuffer,0x0a0a,SampleSize*2+2);

      for (c=0;c<10;c++)
      {
        if ( (DefInsPos[c]>>8) > Sample[DefIns[c]].Size ) DefNote[c]|=15;

        SoundMixer(c);
      }
      ApplyOverdrive();

      if (CurrentLine>SongEnd)
      {
        ForNextValue=0;
        ForNextPosition=32000;
        CurrentLine=SongBegin;
      }
      else if (GoToLine==0xFFFF) CurrentLine++; else CurrentLine=GoToLine;
    }
    else
    {
      /*DisableIRQs*/
//    _disable();
//    OutpWait(0x21,inp(0x21)&128);

      /*DMA-Controller*/
      outp(DMA_ADRESS_Maske,0x05);
      outp(DMA_ADRESS_LoescheFF,0x00);
//    outp(DMA_ADRESS_Modus,0x49);
      outp(DMA_ADRESS_Adresse,((uLong)SampleBuffer0)&255);
      outp(DMA_ADRESS_Adresse,(((uLong)SampleBuffer0)&0xff00)>>8);
      outp(DMA_ADRESS_Seite,((uLong)SampleBuffer0)>>16);
      outp(DMA_ADRESS_Zaehler,(SampleSize+1)&255);
      outp(DMA_ADRESS_Zaehler,(SampleSize+1)>>8);
      outp(DMA_ADRESS_Maske,0x01);

      /*StartChannel*/
      OutpWait(Config.BaseNum+0xC,0x14);
      OutpWait(Config.BaseNum+0xC,SampleSize&255);
      OutpWait(Config.BaseNum+0xC,SampleSize>>8);

      /*EnableIRQs*/
//    _enable();
//    outp(0x21,inp(0x21)&127);
      outp(0x20,0x20);
    }
  } else outp(0x20,0x20);
  if (ExitCommunicator!=0) ExitCommunicator=2;
}
#endif


void StartSoundProcessing(void)
{
  memset(SampleBuffer1,128,5000);
  memset(SampleBuffer2,128,5000);
  memset(DefInsPos,0,10*4);
  memset(DefNote,15,10);
  ForNextValue=0;
  ForNextPosition=32000;
  CurrentLine=0;

#ifdef DOS
  {
  uShort Seite,Block;
  /*Stop old sound*/
  OutpWait(Config.BaseNum+0xC,0xD0);

  /*Start Sample*/
  Seite=((uLong)SampleBuffer1)/65536;
  Block=((uLong)SampleBuffer1)&65535;

  /*DisableIRQs*/
  _disable();
  OutpWait(0x21,inp(0x21)&128);

  /*DMA-Controller*/
  outp(DMA_ADRESS_Maske,0x05);
  outp(DMA_ADRESS_LoescheFF,0x00);
  outp(DMA_ADRESS_Modus,0x59);
//outp(DMA_ADRESS_Modus,0x49);
  outp(DMA_ADRESS_Adresse,Block&255);
  outp(DMA_ADRESS_Adresse,Block/256);
  outp(DMA_ADRESS_Seite,Seite);
  outp(DMA_ADRESS_Zaehler,100&255);
  outp(DMA_ADRESS_Zaehler,100/256);
  outp(DMA_ADRESS_Maske,0x01);

  OutpWait(Config.BaseNum+0xC,0x40);
  OutpWait(Config.BaseNum+0xC,184);   /*14000 Hz*/

  /*StartChannel*/
  OutpWait(Config.BaseNum+0xC,0x14);
  OutpWait(Config.BaseNum+0xC,100&255);
  OutpWait(Config.BaseNum+0xC,100/256);

  /*EnableIRQs*/
  _enable();
  outp(0x21,inp(0x21)&127);

  ((uChar*)(VGA_ADRESSE))[1]++;
  }
#else
  if (digi_audio_dev)
    SDL_PauseAudioDevice(digi_audio_dev, 0);
#endif
}

