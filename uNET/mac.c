/**********************************************************************************
@file   mac.c
@brief  UNET MAC-layer functions
@authors: Gustavo Weber Denardin
          Carlos Henrique Barriquello

Copyright (c) <2009-2013> <Universidade Federal de Santa Maria>

  * Software License Agreement
  *
  * The Software is owned by the authors, and is protected under 
  * applicable copyright laws. All rights are reserved.
  *
  * The above copyright notice shall be included in
  * all copies or substantial portions of the Software.
  *  
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE. 
*********************************************************************************/

#include "hardware.h"
#include "BRTOS.h"
#include "MRF24J40.h"
#include "unet_api.h"
#include "NetConfig.h"
#include <stdlib.h>

extern BRTOS_Sem   *RF_TX_Event;
extern BRTOS_Sem   *MAC_Event;
extern BRTOS_Queue *RF;

#if (!defined MAC16_MEM_ADDRESS) || (!defined PANID_MEM_ADDRESS)
#error "Please define 'MAC16_MEM_ADDRESS' and 'PANID_MEM_ADDRESS'"
#endif

#ifdef __GNUC__
#if (DEVICE_TYPE != PAN_COORDINATOR)
#define macAddress         			(*((INT32U *) MAC16_MEM_ADDRESS))
#define macPANIdentificator         (*((INT32U *) PANID_MEM_ADDRESS))
#else
const INT32U macAddress            = 0xFFFFFFFF;
const INT32U macPANIdentificator   = 0xFFFFFFFF;
#endif
#else
#if (DEVICE_TYPE != PAN_COORDINATOR)
  const INT32U macAddress           @MAC16_MEM_ADDRESS = 0xFFFFFFFF;
  const INT32U macPANIdentificator  @PANID_MEM_ADDRESS = 0xFFFFFFFF;
#else 
  const INT32U macAddress            = 0xFFFFFFFF;
  const INT32U macPANIdentificator   = 0xFFFFFFFF; 
#endif
#endif

#if (!defined PANID_INIT_VALUE) || (!defined MAC16_INIT_VALUE) || (!defined ROUTC_INIT_VALUE)
#error "Please define 'PANID_INIT_VALUE', 'MAC16_INIT_VALUE' and 'ROUTC_INIT_VALUE'"
#endif

volatile INT16U       macPANId       = PANID_INIT_VALUE;
volatile INT16U       macAddr        = MAC16_INIT_VALUE;
volatile INT8U        RouterCapacity = ROUTC_INIT_VALUE;
volatile INT8U        macACK         = FALSE;

// Informações do nó solicitando associação
volatile INT8U          AssociateAddress[8];

// Sempre que enviar uma mensagem, incrementar este número
volatile INT8U          SequenceNumber = 0x80; 

/* Um pacote é composto por um mac frame control 
em conjunto com o unet packet */
volatile MAC_FRAME_CONTROL mac_frame_control;
volatile UNET_PACKET       unet_packet;

/* O vetor unet_beacon guarda uma quantidade "BeaconLimit"
de beacons durante o active scan de um canal  */
volatile UNET_BEACON       unet_beacon[BeaconLimit];
volatile INT8U             BeaconCnt = 0; 

// Flags de indicação de tarefas pendentes pela camada MAC
volatile MAC_TASKS_PENDING mac_tasks_pending;

/* Frame CRC check */
static INT16U FrameCRC = 0;    

const INT16U ccitt_crc16_table[256] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};

#define BY_LUT 0
#define BY_FUN 1
  
#define  REVERSE_BYTE   BY_FUN

#ifndef REVERSE_BYTE
  #error "REVERSE_BYTE method must be defined!"
#endif

#if (REVERSE_BYTE == BY_LUT)
const INT8U rev_bitorder_table[256] = 
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};
#endif

#if (REVERSE_BYTE == BY_FUN)

/* Reverse order byte */
INT8U ReverseByte(INT8U);

INT8U ReverseByte(INT8U b){

    return   b = (INT8U)(((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16); 

}
#endif


// Analiza se o beacon é valido na rede UNET
// se não for, descarta pacote
INT8U MAC_BeaconVerify(void)
{
    // Se for de tamanho diferente, descarta beacon por estar fora de padrão
    if (mac_packet.Payload_Size != macBeaconPayloadLength)
      return FALSE;
    
    if ((mac_packet.MAC_Payload[0] == 0xFF) & (mac_packet.MAC_Payload[1] == 0xCF))
    {
      if (mac_packet.MAC_Payload[4] == 0xAA)
      {
        if (mac_packet.MAC_Payload[7] > 0)
          return TRUE;
        else
          return FALSE;
      }else
      {
        return FALSE;
      }
    }else
    {
      return FALSE;
    }
}

// Monta pacote de comando MAC
void MAC_Command(INT8U command, INT8U Parameters, INT16U PANId, INT16U ShortAddr){
  INT8U i = 0;
  INT8U j = 0;
  INT8U FrameIndex = 0;
  INT8U HeaderSize = 0;
  INT8U PayloadSize = 0;
                            
  // Inicia montagem do pacote MAC Command
  
  // Indicação de MAC Command no Frame Control
  PHYSetLongRAMAddr(2, (INT8U)(0x03+Parameters));
  HeaderSize++;
  
  if(mac_tasks_pending.bits.isAssociated == 1)
  {
      if (mac_tasks_pending.bits.AssociationInProgress == 1)
      {
          // Association Response
          if (command == ASSOCIATION_RESPONSE)
          {
            PHYSetLongRAMAddr(3, 0xCC);
            HeaderSize++;          
          }
      } 
  }else
  {
      // Indicação de Dest Address de 16b, sem Source Address, no Frame Control
      if (mac_tasks_pending.bits.ScanInProgress == 1)
      {      
          PHYSetLongRAMAddr(3, 0x08);
          HeaderSize++;
      }
      
      if (mac_tasks_pending.bits.AssociationPending == 1)
      {
          PHYSetLongRAMAddr(3, 0xC8);
          HeaderSize++;      
      }      
  }
  
  // Sequence Number
  PHYSetLongRAMAddr(4, SequenceNumber);
  if (++SequenceNumber == 0) SequenceNumber = 1;
  HeaderSize++;
  
  if(mac_tasks_pending.bits.isAssociated == 1)
  {
      if (mac_tasks_pending.bits.AssociationInProgress == 1)
      {
          // Association Response
          if (command == ASSOCIATION_RESPONSE)
          {
            j = (INT8U)(macPANId & 0xFF);
            PHYSetLongRAMAddr(5, j);
            j = (INT8U)(macPANId >> 8);
            PHYSetLongRAMAddr(6, j);
            
            // Copia endereço 64b do nó pedindo associação
            j = 7;
            for(i=8;i>0;i--)
            {
              PHYSetLongRAMAddr(j, AssociateAddress[i-1]);
              j++;
            }
            
            // Envia endereço 64b do nó fonte do pacote
            j = 15;
            for(i=8;i>0;i--)
            {
              PHYSetLongRAMAddr(j, mac64Address[i-1]);
              j++;
            }            
            HeaderSize += 18;
            FrameIndex = 23;            
          }
      }      
  }else
  {
      if (mac_tasks_pending.bits.ScanInProgress == 1)
      {
          /* Indicação de Dest Address de 16b, 
          sem Source Address, no Frame Control */
          PHYSetLongRAMAddr(5, 0xFF);
          PHYSetLongRAMAddr(6, 0xFF);
          PHYSetLongRAMAddr(7, 0xFF);
          PHYSetLongRAMAddr(8, 0xFF);
          HeaderSize += 4;
          FrameIndex = 9;
      }
      
      if (mac_tasks_pending.bits.AssociationPending == 1)
      {
          /* Indicação de Dest Address de 16b, 
          com Source Address em 64b, no Frame Control */
          j = (INT8U)(PANId & 0xFF);
          PHYSetLongRAMAddr(5, j);
          j = (INT8U)(PANId >> 8);
          PHYSetLongRAMAddr(6, j);
          
          j = (INT8U)(ShortAddr & 0xFF);
          PHYSetLongRAMAddr(7, j);
          j = (INT8U)(ShortAddr >> 8);
          PHYSetLongRAMAddr(8, j);
                              
          // Não precisa PANId da fonte pq
          // a associação é intrapan
          
          // Adiciona endereço 64b da fonte
          j = 9;
          for(i=8;i>0;i--)
          {
             PHYSetLongRAMAddr(j, mac64Address[i-1]);
             j++;
          }
          HeaderSize += 12;
          FrameIndex = 17;
      }
      
  }
  
  // Adiciona o comando
  PHYSetLongRAMAddr(FrameIndex, command);
  FrameIndex++;
  PayloadSize++;
  
  switch(command)
  {
    case ASSOCIATION_REQUEST:
      // Monta o Capability information field
      // Não pode ser PAN Coordinator
      // Dispositivo FFD, alimentado pela rede
      // não desliga o receptor para conservar energia
      // sem segurança
      // Solicitando endereço de 16 bits
      PHYSetLongRAMAddr(FrameIndex, 0x8E);
      FrameIndex++;
      PayloadSize++;
      break;
    case ASSOCIATION_RESPONSE:
      // Associa o nó sem enviar endereço de 16 bits
      // sem segurança
      PHYSetLongRAMAddr(FrameIndex, 0xFE);
      FrameIndex++;
      PHYSetLongRAMAddr(FrameIndex, 0xFF);
      FrameIndex++;
      PHYSetLongRAMAddr(FrameIndex, 0x00);
      FrameIndex++;
      PayloadSize += 3;
      break;
    default:
      break;
  }

  // Informação do tamanho do MAC header em bytes
  // No modo não seguro é ignorado
  PHYSetLongRAMAddr(0x000,HeaderSize);
  // Informação do tamanho em bytes do MAC header + Payload
  PHYSetLongRAMAddr(0x001,(INT8U)(HeaderSize+PayloadSize));

  // transmit packet without ACK requested
  // Para solicitar ACK, bit2 = 1
  if ((Parameters&0x20) == 0x20)
  {
    UserEnterCritical();
  	mac_tasks_pending.bits.PacketPendingAck = 1;
  	UserExitCritical();
    PHYSetShortRAMAddr(WRITE_TXNMTRIG,0b00000101);
  }
  else
  {
    UserEnterCritical();
  	mac_tasks_pending.bits.PacketPendingAck = 0;
  	UserExitCritical();
    PHYSetShortRAMAddr(WRITE_TXNMTRIG,0b00000001);
  }
}


// Monta pacote de comando MAC
void MAC_Beacon(void){
  INT8U HeaderSize = 0;
  INT8U PayloadSize = 0;
                            
  // Inicia montagem do pacote Beacon
  
  // Indicação de Beacon no Frame Control
  PHYSetLongRAMAddr(2, 0x00);
  HeaderSize++;
  
  /* Indicação de Dest Address de 16b, 
  sem Source Address, no Frame Control */    
  PHYSetLongRAMAddr(3, 0x80);
  HeaderSize++;
 
  
  // Sequence Number
  PHYSetLongRAMAddr(4, SequenceNumber);
  if (++SequenceNumber == 0) SequenceNumber = 1;
  HeaderSize++;
  
  // PanId do coordenador que gera o Beacon
  PHYSetLongRAMAddr(5, (INT8U)(macPANId & 0xFF));
  PHYSetLongRAMAddr(6, (INT8U)(macPANId >> 8));
  
  // Endereço do coordenador que gera o Beacon
  PHYSetLongRAMAddr(7, (INT8U)(macAddr & 0xFF));
  PHYSetLongRAMAddr(8, (INT8U)(macAddr >> 8));
  HeaderSize += 4;
  
  // Padrão IEEE 802.15.4 p/ Beaconless networks
  PHYSetLongRAMAddr(9, 0xFF);
  PHYSetLongRAMAddr(10, 0xCF);
  PHYSetLongRAMAddr(11, 0x00);
  PHYSetLongRAMAddr(12, 0x00);
  PayloadSize += 4;
  
  // Protocol Id
  PHYSetLongRAMAddr(13, 0xAA);
  PayloadSize++;
  
  // Profile
  PHYSetLongRAMAddr(14, 0x00);
  PayloadSize++;
  
  // Distancia do PAN coordinator
  PHYSetLongRAMAddr(15, thisNodeDepth);
  PayloadSize++;
  
  // Capacidade de endereçar Roteadores
  PHYSetLongRAMAddr(16, RouterCapacity);
  PayloadSize++;    

  // Informação do tamanho do MAC header em bytes
  // No modo não seguro é ignorado
  PHYSetLongRAMAddr(0x000,HeaderSize);
  // Informação do tamanho em bytes do MAC header + Payload
  PHYSetLongRAMAddr(0x001,(INT8U)(HeaderSize+PayloadSize));

  //transmit packet without ACK requested
  UserEnterCritical();
  mac_tasks_pending.bits.PacketPendingAck = 1;
  UserExitCritical();
  PHYSetShortRAMAddr(WRITE_TXNMTRIG,0b00000001);
}



  // Analisa a requisição de uma mensagem MAC e responde
 void MAC_Response(void)
 {
    INT8U k = 0;
    INT8U AssociateCapabilityInfo;
    
    // Responde comandos MAC somente depois da associação do nó
    switch(mac_packet.MAC_Payload[0])
    {
      case ASSOCIATION_REQUEST:
        // Não associa outros nós enquanto não terminar
        // um processo de associação pendente
        if(mac_tasks_pending.bits.AssociationInProgress != 1)
        {
            AssociateCapabilityInfo = mac_packet.MAC_Payload[1];
            if (AssociateCapabilityInfo == 0x8E)
            {
              // Copia endereço 64b do requisitante
              for(k=0;k<8;k++)
              {
                 AssociateAddress[k] = mac_packet.SrcAddr_64b[k];
              }
              UserEnterCritical();
              mac_tasks_pending.bits.AssociationInProgress = 1;
              UserExitCritical();
            }
        }
        break;
      case ASSOCIATION_RESPONSE:
        break;
      case DISASSOCIATION_NOTIFICATION:
        break;
      case DATA_REQUEST:
        if(mac_tasks_pending.bits.AssociationInProgress == 1)
        {
          // Enviar Association Response
          //acquireRadio();
          macACK = FALSE;
          MAC_Command(ASSOCIATION_RESPONSE,MAC_ACK_INTRA_PAN,0xFFFF,0xFFFF);
          
          // Espera Ack
          // Colocar para dormir com semaforo com timeout
          // e acordar na rotina de interrupção pelo mesmo semaforo
          OSSemPend(RF_TX_Event,TX_TIMEOUT);
          if (macACK == TRUE)
          {
            // Associação completada com sucesso
            UserEnterCritical();
            mac_tasks_pending.bits.AssociationInProgress = 0;
            UserExitCritical();
          }else
          {
            // Erro no processo de Associação
        	UserEnterCritical();
            mac_tasks_pending.bits.AssociationInProgress = 0;
            UserExitCritical();
          }
        }
        break;
      case PAN_ID_CONFLICT_NOTIFICATION:
        break;
      case ORPHAN_NOTIFICATION:
        break;
      case BEACON_REQUEST:
        // Envia Beacon
        DelayTask(RadioRand());
        MAC_Beacon();
        // Espera resultado do envio do Beacon
        OSSemPend(RF_TX_Event,TX_TIMEOUT);
        break;
      default:
        break;
    }
  
 }
 
 
// Algoritmo que determina um valor randomico de espera para responder
// um MAC Command  - valor entre 0 e 34
INT16U RadioRand(void)
{
  INT16U j = 0;
  OS_SR_SAVE_VAR;
  
  OSEnterCritical();
  
  j = OSGetCount(); 
  j = (INT16U)(j ^ macAddr);
  j = (INT16U)(j ^ mac_packet.Frame_RSSI);
  
  OSExitCritical();
  
  while(j > 17)
  {
    j = (INT16U)(j >> 2);
  }
  
  j = (INT16U)(j * 2);
  
  return j;
  
}
                                       

// Função que controla a associação do nó
INT8U UNET_Associate(void)
{
   //INT8U AssociateTimeout = 0;
   INT8U i   = 0;
   INT8U j   = 0;
   INT8U z  = 0;
   INT8U aux = 0;
   
   // máquina de estados que irá coordenar entrada na rede
   // O processo deve demorar no máximo 2 segundos = 4 * 500ms
   
   if (mac_tasks_pending.bits.isAssociated == 1){    
       while(0){}
   }
   // Começa o processo de Active Scan
   UserEnterCritical();
     mac_tasks_pending.bits.AssociationPending = 0;
     mac_tasks_pending.bits.ScanInProgress = 1;
   UserExitCritical();
   
   DelayTask((INT16U)(RadioRand()*7));
   
   BeaconCnt = 0;
   
   while (BeaconCnt == 0) 
   {
      for(i=0;i<4;i++)
      {
        // Envia pedido de beacon
        // Sem ACK e com broadcast de PANId       
        MAC_Command(BEACON_REQUEST,MAC_NACK,0xFFFF,0xFFFF);
        DelayTask((INT16U)(53+RadioRand()));
        
        
        MAC_Command(BEACON_REQUEST,MAC_NACK,0xFFFF,0xFFFF);
        DelayTask((INT16U)(53+RadioRand()));
        
                        
        // Delay máximo = 32 * aSuperFrameDuration (15,36ms)
        DelayTask(381);
      
        if (BeaconCnt == BeaconLimit){           
          break;
        }
      }
      
      if (BeaconCnt == 0)
      {
                        
        // Reset de Radio se ficou mais de 50 segundos sem receber pacotes
        if (nwk_tasks_pending.bits.RadioReset == 1)
        {
            
            //  Disable receiving packets off air
            PHYSetShortRAMAddr(WRITE_BBREG1,0x04);
            
            // Limpa o buffer de recepção
            (void)OSCleanQueue(RF);
            
            MRF24J40Reset();
            
            //  Enable receiving packets off air
            PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
                      
            nwk_tasks_pending.bits.RadioReset = 0;
        }
                
        // Tempo para nova tentativa
        DelayTask((INT16U)(3333+(RadioRand()*13)));
      }
      
      
   }
   
   // Termina o processo de Active Scan
   UserEnterCritical();
   mac_tasks_pending.bits.ScanInProgress = 0;
   
   // Máquina de estados para solicitar associação
   // Começa o processo de associação
   mac_tasks_pending.bits.AssociationPending = 1;
   UserExitCritical();
   
   
   // Escolhe o coordenador ao qual irá solicitar associação
   // pela menor profundidade
   
  AssociateLoop:
   aux = 0;
   j = 0;
   
   for(i=0;i<BeaconCnt;i++)
   {
      if (unet_beacon[i].Beacon_RSSI > aux)
      {
        // Verifica se já não houve tentativa de associação negada
        if(unet_beacon[i].AssociationStatus == 0)
        {
            aux = unet_beacon[i].Beacon_RSSI;
            j = i;
        }
      }
   }
   
   // Se não houve seleção de beacon
   // reinicia o processo de beacon request
   if (aux == 255)
   {
      UserEnterCritical();
	      mac_tasks_pending.bits.AssociationPending = 1;
      UserExitCritical();
      for(i=0;i<BeaconLimit;i++)
      {
          unet_beacon[i].Addr_16b = 0xFFFE;
          unet_beacon[i].DeviceDepth = 255;
          BeaconCnt = 0;
      }      
      return FALSE;
   }

  
  z = 0;
  
  while(z<3) 
  {
   // Associate Request com Acknowledgement
   macACK = FALSE;
   MAC_Command(ASSOCIATION_REQUEST,MAC_ACK_INTRA_PAN,unet_beacon[j].PAN_Ident,unet_beacon[j].Addr_16b);
   
   // Colocar para dormir com semaforo com timeout
   // e acordar na rotina de interrupção pelo mesmo semaforo
   OSSemPend(RF_TX_Event,TX_TIMEOUT);
     
   if (macACK == TRUE)
   {
      // Espera o tempo aResponseWaitTime
      // = 32 * BaseSuperframeDuration
      DelayTask(aResponseWaitTime);
      
      // antes do Data Request
      // escreve o PANId no radio
      PHYSetShortRAMAddr(WRITE_PANIDL,(INT8U)(unet_beacon[j].PAN_Ident&0xFF));
      PHYSetShortRAMAddr(WRITE_PANIDH,(INT8U)(unet_beacon[j].PAN_Ident>>8));
      
      macPANId = unet_beacon[j].PAN_Ident;
      
      // Data Request
      macACK = FALSE;
      MAC_Command(DATA_REQUEST,MAC_ACK_INTRA_PAN,unet_beacon[j].PAN_Ident,unet_beacon[j].Addr_16b);
   
      // Colocar para dormir com semaforo com timeout
      // e acordar na rotina de interrupção pelo mesmo semaforo
      OSSemPend(RF_TX_Event,TX_TIMEOUT);
      
      if (macACK == TRUE)
      {
         mac_packet.MAC_Payload[0] = 0;
         OSSemPend(MAC_Event,50);
   
         // Verifica se é Association Response
         if (mac_packet.MAC_Payload[0] == 0x02)
         {
            // Analisar "Association status field"
            // Se diferente de 0, alocar motivo em unet_beacon[j].AssociationStatus
            if (mac_packet.MAC_Payload[3] == 0)
            {
                // Dispositivo Associado
                // Ao terminar associação, deixa o estado de associação pendente
                // para o estado de associado
                macPANId   = unet_beacon[j].PAN_Ident;
                // Copia o endereço recebido pelo nó ao qual foi associado
                macAddr = (INT16U)((mac_packet.MAC_Payload[2]<<8) | mac_packet.MAC_Payload[1]);
                
                // Gera endereço estocástico
                if (macAddress == 0xFFFFFFFF)
                {
                  if(macAddr == 0xFFFE)
                  {
                    macAddr = (INT16U)(mac_packet.Frame_CRC ^ TIMER_ADDR);
                  }
                }
                else
                {
                  macAddr = (INT16U)(macAddress & 0xFFFF);
                }
                
                PHYSetShortRAMAddr(WRITE_SADRL,(INT8U)(macAddr&0xFF));
                PHYSetShortRAMAddr(WRITE_SADRH,(INT8U)(macAddr>>8));
                
                VerifyNewAddress();
                
                UserEnterCritical();
                  RouterCapacity = 1;
                  mac_tasks_pending.bits.AssociationPending = 0;                
                  mac_tasks_pending.bits.isAssociated = 1;
                UserExitCritical();
                return TRUE;
            }else
            {
                // Falha na associação
                // O motivo estará disponível no Association Status do beacon solicitado
                unet_beacon[j].AssociationStatus = mac_packet.MAC_Payload[3];
                goto AssociationFail;
            }
         }else
         {
            // Não veio Association Response
            goto AssociationFail;
         }          
      } else
      {
          // Não veio Ack do Data Request
          goto AssociationFail;
      }       
   } else
   {
     AssociationFail:
      // Reinicia processo
      // Volta endereços p/ default          
      PHYSetShortRAMAddr(WRITE_PANIDL,0xFF);
      PHYSetShortRAMAddr(WRITE_PANIDH,0xFF);
      macPANId = 0xFFFF;           
      // Não veio Ack do Associate Request
      // Tenta associação em outro coordenador
      z++;
      DelayTask(RadioRand());
      
   }
  }
  unet_beacon[j].DeviceDepth = 0xFF;
  goto AssociateLoop;
   
   
}

/* Functions for CRC calculation and checking */
void CRC_Update(INT8U data)
{
    INT8U tmp = 0;
    INT8U short_data = 0;
    INT16U fcs_tmp = 0;

#if (REVERSE_BYTE == BY_LUT)    
    short_data = (INT8U)(rev_bitorder_table[data] & 0xff);
#else // (REVERSE_BYTE == BY_FUN) 
    short_data = (INT8U)ReverseByte(data);
#endif
 
    tmp = (INT8U)((INT8U)(FrameCRC >> 8) ^ short_data);
    fcs_tmp = (INT16U)ccitt_crc16_table[tmp];
    FrameCRC = (INT16U)((INT16U)(FrameCRC << 8) ^ fcs_tmp);
}


INT16U CRC_Get(void)
{
    INT8U tmp = 0;
    INT8U tmp2 = 0;
#if (REVERSE_BYTE == BY_LUT)     
    tmp = rev_bitorder_table[(INT8U)(FrameCRC >> 8)];
    tmp2 = rev_bitorder_table[(INT8U)(FrameCRC & 0xFF)];
#else // (REVERSE_BYTE == BY_FUN) 
   tmp = ReverseByte((INT8U)(FrameCRC >> 8));
   tmp2 = ReverseByte((INT8U)(FrameCRC & 0xFF));
#endif     
    
    FrameCRC = 0; /* reset frame CRC for next packet */
    
    return ((INT16U)((tmp2 << 8) | tmp));
}



