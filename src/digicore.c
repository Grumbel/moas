//============================================================================================
// MOAS - Mouse Orientiertes Adventure System
//============================================================================================
// Der Sourcecode ist derzeit urheberrechtlich geschützt:
// Copyright (C) 2000 bei Thomas Holz
//
// Sobald ich den entsprechenden Text habe, wird das in Public Domain geändert, evtl mit 
// kleinen Einschränkungen.
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
#define uLong unsigned long
#define uShort unsigned short
#define uChar unsigned char

long StepTable[4*16];                     // Kanal 0-7 : Musik
short VolumeTab[32*256];                  // Kanal 8&9 : Sprache und Effekt
uChar DefVol[10],DefIns[10],DefNote[10];
uLong DefInsPos[10];
uShort SampleSize;
short *OverdriveBuffer;
uChar *SampleBuffer2,OverdriveRemapper[5130];

uLong SpeechSize;
uChar *SpeechData;
uShort SpeechVolume,GlobalVolume;                          // [0..256]

struct SAMPLE
{
  uChar Name[40];
  long Rate;
  long Size;
  uChar *Data;
} Sample[64];

void SoundMixer(const short c)
{
  register uLong e;
  register long d,Ende;
  register short *NewVolumeTab;
  register uChar *Data;

  /* Im normalen RAM sichern */
  long Size;
  uLong Step;

  if (c<8)
  {
    if ((DefNote[c]&15)!=15)                               // Teil fr Sound
    {
      Step=StepTable[DefNote[c]];
      e=DefInsPos[c]; Size=Sample[DefIns[c]].Size; Data=Sample[DefIns[c]].Data;
      NewVolumeTab=VolumeTab+((DefVol[c]*(((256-SpeechVolume)*GlobalVolume)>>8))&0xff00);

      if ( ((e+Step*SampleSize)>>8)>=Size ) { Ende=((Size<<8)-e)/Step; DefNote[c]|=15; }
//    if ( ((e+Step*SampleSize)>>8)>Size ) Ende=((Size<<8)-e)/Step;
      else Ende=SampleSize;

      for (d=0;d<Ende;d++)
      {
        OverdriveBuffer[d]+=NewVolumeTab[Data[e>>8]];      // Kern fr Sound
        e+=Step;
      }
//    if ((e>>8)>=Size) DefNote[c]|=15;
      DefInsPos[c]=e;
    }
  }
  else
  {
    if (DefInsPos[c]!=0xFFFFFFFF)
    {
      e=DefInsPos[c]; Data=SpeechData+e;
      NewVolumeTab=VolumeTab+((DefVol[c]*((SpeechVolume*GlobalVolume)>>8))&0xff00);

      if (e+SampleSize>SpeechSize) Ende=SpeechSize-e;
      else Ende=SampleSize;

      for (d=0;d<Ende;d++)
        OverdriveBuffer[d]+=NewVolumeTab[Data[d]];        // Kern fr Sprache

      if (d+e>=Size) DefInsPos[9]=0xFFFFFFFF;
        else DefInsPos[9]=d+e;
    }
  }
}
void ApplyOverdrive(void)
{
  register short c;

  for (c=0;c<SampleSize;c++)
    SampleBuffer2[c]=OverdriveRemapper[OverdriveBuffer[c]];
}
