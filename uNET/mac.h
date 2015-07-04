/**********************************************************************************
@file   mac.h
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


#ifndef UNET_MAC_H
#define UNET_MAC_H

#include "BRTOS.h"
#include "NetConfig.h"
#include <stddef.h>

#define   mac_packet          unet_packet.mac_packet_u
#define   nwk_packet          unet_packet.nwk_packet_u
#define   app_packet          unet_packet.app_packet_u

                                                          
// Function Prototypes
INT8U MAC_BeaconVerify(void);
void MAC_Beacon(void);
void MAC_Response(void);
void MAC_Command(INT8U command, INT8U Parameters,INT16U PANId, INT16U ShortAddr);

INT8U UNET_Associate(void);
INT16U RadioRand(void);
INT16U CRC_Get(void); 
void CRC_Update(INT8U data);
      
    
#define aMaxFrameResponseTime  1220
#define aMaxFrameRetries       3
#define macBeaconPayloadLength (INT8U)8   
#define BeaconLimit            (INT8U)2
#define aResponseWaitTime      (INT16U)492   /* 32 * aSuperFrameDuration (15,36ms) */

#define BeaconFrame          0b000
#define DataFrame            0b001
#define AckFrame             0b010
#define MACFrame             0b011


#define MAC_NACK             (INT8U)0x00
#define MAC_ACK              (INT8U)0x20
#define MAC_INTRA_PAN        (INT8U)0x40
#define MAC_ACK_INTRA_PAN    (INT8U)0x60          

/* 
Payload do NWK = 28 bytes + 
4 bytes da camada de app (32 bytes)		
foi usado para implementar o "for" 
que retira os dados extras da camada app (payload da app)	
*/
#define MAX_PHY_PACKETSIZE  (INT8U)127
#define MIN_MAC_HEADER_SIZE (INT8U)9  /* (MFC(2b) + SN(1b) + ADDR (6b))*/
#define NWK_HEADER_SIZE     (INT8U)28
#define APP_HEADER_SIZE     (INT8U)4
#define NWK_APP_HEADER_SIZE (NWK_HEADER_SIZE + APP_HEADER_SIZE)

// Max. app payload size
#define MAX_APP_PAYLOAD_SIZE    (INT8U)(MAX_PHY_PACKETSIZE-MIN_MAC_HEADER_SIZE-NWK_APP_HEADER_SIZE - 3)

// Radio Watchdog Timeout in msec
#define RadioWatchdogTimeout (INT16U)15000  

// Definição padrão para todos os roteadores do sistema
// #define nwkMaxChildren        (INT8U)3
// #define nwkMaxRouters         (INT8U)3
#define nwkMaxDepth           (INT8U)200

/* removed from here and included in NetConfig.h */
// #define RFBufferSize                (INT16U)768

/* mac frame control struct */
typedef union _MAC_FRAME_CONTROL
{
    INT16U 		Val;
    INT8U       bytes[2];
    struct _MAC_FRAME_CONTROL_bits
    {
#if PROCESSOR == COLDFIRE_V1
    	INT8U   FrameType       :3;
        INT8U   SecurityEnabled :1;
        INT8U   FramePending    :1;
        INT8U   ACKRequest      :1;
        INT8U   IntraPAN        :1;
        INT8U                   :1;
        INT8U                   :1;
        INT8U                   :1;
        INT8U   DstAddrMode     :2;
        INT8U                   :1;
        INT8U                   :1;
        INT8U   SrcAddrMode     :2;
#endif
#if PROCESSOR == ARM_Cortex_M0
        INT8U                   :1;
        INT8U                   :1;
        INT8U   DstAddrMode     :2;
        INT8U                   :1;
        INT8U                   :1;
        INT8U   SrcAddrMode     :2;
        INT8U   FrameType       :3;
        INT8U   SecurityEnabled :1;
        INT8U   FramePending    :1;
        INT8U   ACKRequest      :1;
        INT8U   IntraPAN        :1;
        INT8U                   :1;
#endif
    } bits;
} MAC_FRAME_CONTROL;

/* mac packet struct */
typedef struct _MAC_PACKET
{
    INT8U         Sequence_Number;
    INT8U         Unused_ByteAlign;
    INT16U        Dst_PAN_Ident;
    INT16U        DstAddr_16b;
    INT8U         DstAddr_64b[8];
    INT16U        Src_PAN_Ident;
    INT16U        SrcAddr_16b;
    INT8U         SrcAddr_64b[8];
    INT8U         MAC_Payload[MAX_APP_PAYLOAD_SIZE+NWK_APP_HEADER_SIZE]; // app header (4bytes) + nwk header (28 bytes)
    INT8U         Payload_Size;
    INT16U        Frame_CRC;
    INT8U         Frame_RSSI;
    INT8U         Frame_LQI;      
} MAC_PACKET;

/* nwk packet struct */
typedef struct _NWK_PACKET 
{
    // MAC Header    
    INT8U         MAC_Header[offsetof(MAC_PACKET,MAC_Payload)];     
    // NWK Packet
    INT8U         NWK_Packet_Type;
    INT8U         NWK_Parameter;
    INT16U		  NWK_Destiny;
    INT16U		  NWK_Source;
    INT8U         NWK_Packet_Life;    
    INT8U         NWK_Payload[MAX_APP_PAYLOAD_SIZE+APP_HEADER_SIZE];     // app header (4bytes)
    INT8U         NWK_Payload_Size;
    INT8U         Frame_CRC_RSSI_LQI[4];
} NWK_PACKET;

/* app packet struct */      
typedef struct _APP_PACKET    
{    
    // MAC Header    
    INT8U         MAC_Header[offsetof(MAC_PACKET,MAC_Payload)];    
    // NWK Header    
    INT8U         NWK_Header[offsetof(NWK_PACKET,NWK_Payload) - offsetof(MAC_PACKET,MAC_Payload)];
    
    // APP Packet
    INT8U         APP_Identify;      /* Id da tarefa a ser acordada */
    INT8U         APP_Profile;       /*  Perfil de Aplicação
                                     ex: perfil p/ iluminação 
                                     = LIGHTING_PROFILE */
    INT8U         APP_Command;       /* Comando do perfil a ser executado */
    INT8U         APP_Command_Attribute;      
                                     /* Atributo do comando do perfil a ser executado
                                      ex: comando = ON/OFF, atributo = ON */
    INT8U         APP_Payload[MAX_APP_PAYLOAD_SIZE];
    INT8U         APP_Command_Size;  /* Atributos da mensagem em bytes */
    INT8U         Frame_CRC_RSSI_LQI[4];
} APP_PACKET;

// APP Packet Header
typedef union {
    
    INT8U         APP_Identify;      /* Id da tarefa a ser acordada */
    INT8U         APP_Profile;       /*  Perfil de Aplicação
                                     ex: perfil p/ iluminação 
                                     = LIGHTING_PROFILE */
    INT8U         APP_Command;       /* Comando do perfil a ser executado */
    INT8U         APP_Command_Attribute;
}APP_PACKET_HEADER;


typedef union _UNET_PACKET
{
  MAC_PACKET  mac_packet_u;
  NWK_PACKET  nwk_packet_u;
  APP_PACKET  app_packet_u;  
} UNET_PACKET;


typedef struct _UNET_BEACON
{
    INT16U        PAN_Ident;
    INT16U        Addr_16b;
    INT8U         Beacon_RSSI;
    INT8U         DeviceDepth;
    INT8U         AssociationStatus;
} UNET_BEACON;



enum packet_states
{
    start_packet,
    dest_00,
    dest_01,
    dest_10,
    dest_11,
    intra_pan,
    source_000,
    source_001,
    source_010,
    source_011,    
    source_100,
    source_101,
    source_110,
    source_111,
    payload,
    CRC,
    end_packet
};

typedef union _MAC_TASKS_PENDING
{
    INT8U Val;
    struct _bits
    {
        INT8U ScanInProgress        :1;
        INT8U AssociationPending    :1;        
        INT8U PacketPendingAck      :1;
        INT8U AssociationInProgress :1;
        INT8U isAssociated          :1;
        INT8U isDataFrameRxed       :1;
    } bits;
} MAC_TASKS_PENDING;


/* enumerations for MLME-SYNC-LOSS.indication */
#define PAN_ID_CONFLICT 0xee
#define REALIGNMENT 0xef
#define BEACON_LOST 0xe0  

#define ASSOCIATION_REQUEST 0x01
#define ASSOCIATION_RESPONSE 0x02
#define DISASSOCIATION_NOTIFICATION 0x03
#define DATA_REQUEST 0x04
#define PAN_ID_CONFLICT_NOTIFICATION 0x05
#define ORPHAN_NOTIFICATION 0x06
#define BEACON_REQUEST 0x07
#define COORDINATOR_REALIGNMENT 0x08
#define GTS_REQUEST 0x09

/* MCPS_DATA_confirm.status/MLME_COMM_STATUS.indication values */
#define SUCCESS 0x00
#define TRANSACTION_OVERFLOW  0xf1
#define TRANSACTION_EXPIRED   0xf0
#define CHANNEL_ACCESS_FAILURE 0xe1
#define DENIED 0xe2
#define DISABLE_TRX_FAILURE 0xe3
#define INVALID_GTS 0xe6
#define NO_ACK 0xe9
#define UNAVAILABLE_KEY 0xf3
#define FRAME_TOO_LONG 0xe5
#define FAILED_SECURITY_CHECK 0xe4
#define INVALID_PARAMETER 0xe8
#define MAC_INVALID_HANDLE 0xe7
#define NO_DATA 0xeb
#define NO_SHORT_ADDRESS 0x3c
#define MAC_NO_BEACON 0xea

// MLME_SCAN_request.ScanType values
#define MAC_SCAN_ENERGY_DETECT      0x00
#define MAC_SCAN_ACTIVE_SCAN        0x01
#define MAC_SCAN_PASSIVE_SCAN       0x02
#define MAC_SCAN_ORPHAN_SCAN        0x03 


#ifdef __GNUC__
#if (DEVICE_TYPE != PAN_COORDINATOR)
#define macAddress         			(*((INT32U *) MAC16_MEM_ADDRESS))
#define macPANIdentificator         (*((INT32U *) PANID_MEM_ADDRESS))
#else
extern const INT32U macAddress;
extern const INT32U macPANIdentificator;
#endif
#else
#if (DEVICE_TYPE != PAN_COORDINATOR)
extern  const    INT32U             macAddress            @MAC16_MEM_ADDRESS;
extern  const    INT32U             macPANIdentificator   @PANID_MEM_ADDRESS;
#else
extern  const    INT32U             macAddress;
extern  const    INT32U             macPANIdentificator; 
#endif
#endif

extern  volatile INT16U             macPANId;
extern  volatile INT16U             macAddr;
extern  volatile INT8U              macACK;
extern  volatile INT8U              RouterCapacity;
extern  volatile INT8U              SequenceNumber;
extern  volatile INT8U              AssociateAddress[8];
extern  volatile MAC_FRAME_CONTROL  mac_frame_control;
extern  volatile MAC_TASKS_PENDING  mac_tasks_pending;
extern  volatile UNET_PACKET      unet_packet;
extern  volatile UNET_BEACON      unet_beacon[BeaconLimit];
extern  volatile INT8U              BeaconCnt;


#endif
