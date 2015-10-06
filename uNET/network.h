/**********************************************************************************
@file   network.h
@brief  UNET NWK-layer functions
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

#ifndef UNET_NWK_H
#define UNET_NWK_H

#include "NetConfig.h"

// coordinator depth defines
#define ROUTE_TO_BASESTATION_LOST (INT8U)0xFE
#define NO_ROUTE_TO_BASESTATION   (INT8U)0xFF

// This timeout is based on the system timer 
// and is used to control the packets last ID timeout
#define LAST_ID_SYSTEM_TIMER_TIMEOUT  (INT8U)8

// Data packet types
#define DATA_PING            (INT8U)0x01
#define BROADCAST_PACKET     (INT8U)0x02
#define ROUTE_PACKET         (INT8U)0x03
#define ADDRESS_PACKET       (INT8U)0x04

// Routing Errors
#define PACKET_LIFE_ERROR    (INT8U)0x05
#define ROUTE_ATTEMPTS_ERROR (INT8U)0x06
#define ROUTE_FRAME_ERROR    (INT8U)0x07
#define ROUTE_NODE_ERROR     (INT8U)0x08
#define NO_ROUTE_AVAILABLE   (INT8U)0x09
#define PAYLOAD_OVERFLOW     (INT8U)0x10


// Routing algorithms
#define NWK_DEST             (INT8U)0b0001
#define NWK_DIRECTION        (INT8U)0b0010
#define NWK_BROADCAST        (INT8U)0b1000


#define DEST_UP              (INT8U)0b0001
#define DEST_DOWN            (INT8U)0b0011
#define NOT_DEST_UP          (INT8U)0b0000
#define NOT_DEST_DOWN        (INT8U)0b0010

                                               
// Routes
#define DOWN_ROUTE           (INT8U)0x10      
#define UP_ROUTE             (INT8U)0x20

#define START_ROUTE          (INT8U)0x00
#define IN_PROGRESS_ROUTE    (INT8U)0x01

// Maximum size of the neighbourhood table
#define NEIGHBOURHOOD_SIZE      (INT8U)8
typedef INT16U                  NEIGHBOR_TABLE_T;
#define ROUTING_UP_TABLE_SIZE   (INT8U)8

/* Link reliability parameters */
#define RSSI_THRESHOLD          (INT8U)10

/* Nwk Tx retries */
#if (CONTIKI_MAC_ENABLE == 1)
#define NWK_TX_RETRIES          (INT8U)50	  
#else
#define NWK_TX_RETRIES          (INT8U)3
#endif

/* Nwk Tx retries */
#define NWK_TX_RETRIES_UP       (INT8U)(NWK_TX_RETRIES)

// Set RX buffer control
#define AUTO_ACK_CONTROL        0

// Neighbor Ping Time in msec
#define NEIGHBOR_PING_TIME   (INT16U)(1000)

// Neighbourhood Timeout in msec
#define NEIGHBOURHOOD_TIMEOUT (INT32U)(NEIGHBOR_PING_TIME*MAX_PING_TIME*3)

// Reactive maintenance packet Time in msec
#define REACTIVE_UP_MESSAGE_TIME   (INT16U)(5000)

// Reactive table maintenance timeout in msec
#define REACTIVE_UP_TIMEOUT (INT32U)(REACTIVE_UP_MESSAGE_TIME*MAX_UPROUTE_MAINTENANCE_TIME*2)

// Depth Watchdog Timeout in msec
#define DEPTH_TIMEOUT           (INT16U)20000

/* Overhead of NWK layer in bytes */
#define NWK_OVERHEAD            (INT8U)7

typedef union _NWK_TASKS_PENDING
{
    INT8U Val;
    struct _nwk_bits
    {
        INT8U DataPingPending             :1;
        INT8U VerifyNeighbourhoodTable    :1;
        INT8U NewNeighborPing             :1;
        INT8U RadioReset                  :1;
        INT8U RoutePending                :1;
        INT8U ReactiveUpMessagePending    :1;
        INT8U VerifyReactiveUpTable       :1;
        INT8U RetryBroadcast			  :1;
    } bits;
} NWK_TASKS_PENDING;


typedef union _dword
{
  unsigned char   int8u[4];
  unsigned short  int16u[2];
  unsigned long   int32u;  
} UINT_DWORD; 


typedef union _NEIGHBOR_STATUS
{
    INT8U Val;
    struct 
    {
        INT8U Symmetric             :1;
        INT8U RxAllowed             :1;        
        INT8U TxPending             :1;
        INT8U Active                :1;
        INT8U RxChannel             :4;
    } bits;
} NB_STATUS;

typedef struct _UNET_NEIGHBOURHOOD
{
    INT16U        Addr_16b;                   // 16 bit address from neighbor
    INT8U         NeighborRSSI;               // Average of the Neighbor signal quality     
    NB_STATUS     NeighborStatus;             // Neighbor status
    INT8U         NeighborLQI;                // Neighbor link quality indicator
    INT8U         NeighborLastID;             // Last message ID used for this neighbor
    INT8U         IDTimeout;                  // Timeout to delete info of the Last message
    INT8U		  NeighborDepth;			  // Depth to the coordinator
} UNET_NEIGHBOURHOOD;


typedef struct _UNET_SYMMETRIC_NEIGHBOURHOOD
{
    INT16U          Addr_16b;                           // 16 bit address from neighbor
    INT8U           NeighborRSSI;                       // Neighbor signal quality
    INT8U           NeighborDepth;                      // Neighbor depth to the coordinator
    INT16U          Neighbors[NEIGHBOURHOOD_SIZE];      // Vizinhos do nó que enviou o ping
    INT8U           NeighborsRSSI[NEIGHBOURHOOD_SIZE];  // Numero de vizinhos no nó que enviou o ping
    INT8U           NeighborsNumber;                    // Numero de vizinhos no nó que enviou o ping
    INT8U           NeighborLQI;
} UNET_SYMMETRIC_NEIGHBOURHOOD;


typedef struct _UNET_ROUTING_UP_TABLE
{
    INT16U        Addr_16b;                   // 16 bit address from intermediate neighbor
    INT16U		  DestinyAddr;				  // 16 bit address of the destination node
    INT8U         Destination;				  // informs one hop distance to the destination node
    INT8U		  hops;						  // Distance in hops to the destination node
    INT8U		  activity;					  // node activity in the last minutes
} UNET_ROUTING_UP_TABLE;



/* Function Prototypes */

void NeighborPing(void);
void HandleNewNeighborPing(void);
INT8U HandleRoutePacket(void);
void VerifyNeighbourhood(void);
void VerifyNeighbourhoodLastIDTimeout(void);
INT8U VerifyPacketReplicated(void);
void UpdateDepth(void);
void NWK_Command(INT16U Address, INT8U r_parameter, INT8U payload_size, INT8U packet_life, INT16U destiny);

void VerifyNewAddress(void);

void IncDepthWatchdog(void);
INT16U GetDepthWatchdog(void);
void ClearDepthWatchdog(void);

void acquireRadio(void);
void releaseRadio(void);
void init_radio_resource(INT8U priority);

INT8U DownRoute(INT8U RouteInit, INT8U AppPayloadSize);
INT8U UpRoute(INT8U RouteInit, INT8U AppPayloadSize);
INT8U UpSimpleRoute(INT8U NWKPayloadSize);
INT8U UpBroadcastRoute(INT8U NWKPayloadSize);
INT8U OneHopRoute(INT8U NWKPayloadSize, INT16U destiny);

#if (USE_REACTIVE_UP_ROUTE == 1)
INT8U ReactiveUpMessage(void);
INT8U ReactiveUpRoute(INT8U RouteInit, INT8U NWKPayloadSize, INT16U destiny);
void VerifyUpRouteTable(void);
#endif

/* UNET API */
#if 0
INT8U UNET_TX_toBase(INT8U *ptr_data, INT8U nbr_bytes);
INT8U UNET_TX_toAll(INT8U *ptr_data, INT8U nbr_bytes);
INT8U UNET_TX_toChildren(INT8U *ptr_data, INT8U nbr_bytes);
INT8U UNET_TX_toAddress(INT8U *ptr_data, INT8U nbr_bytes, POSITION* address);
#endif





// Routing states
enum nwk_packet_state
{
    start_route,              
    neighbor_table_search,
    broadcast,
    route_up,
    route_down,
    send_dest_packet,
    call_app_layer,
    end_packet_life,
    end_route
};



#if (USE_REACTIVE_UP_ROUTE == 1)
extern  volatile UNET_ROUTING_UP_TABLE        unet_routing_up_table[ROUTING_UP_TABLE_SIZE];
#endif


extern  volatile UNET_NEIGHBOURHOOD             unet_neighbourhood[NEIGHBOURHOOD_SIZE];
extern  volatile UNET_SYMMETRIC_NEIGHBOURHOOD   unet_neighbor_ping;
extern  volatile NEIGHBOR_TABLE_T                 NeighborTable;

extern  volatile INT8U               thisNodeDepth;
extern  volatile INT16U				 ParentNeighborID;
extern  volatile INT16U				 ParentRSSI;

extern  volatile INT16U              RadioWatchdog;
extern  volatile INT8U               NeighborPingTimeCnt;

#if (USE_REACTIVE_UP_ROUTE == 1)
extern  volatile INT16U 			 ReactiveUpCnt;
extern  volatile INT8U				 ReactiveUpTimeCnt;
#endif

extern  volatile NWK_TASKS_PENDING   nwk_tasks_pending;


#endif
