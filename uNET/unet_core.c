
/**********************************************************************************
@file   unet_core.c
@brief  UNET tasks code
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
#include "spi.h"
#include "MRF24J40.h" 
#include "app.h"
#include "unet_api.h"

#if (PROCESSOR == ARM_Cortex_M0)
#include "FLASH.h"
#endif

/**************************************************************//*!
*
* Global variables
*
******************************************************************/  

OS_QUEUE      RFBuffer;
BRTOS_Queue  *RF;
BRTOS_Sem    *RF_Event;
BRTOS_Sem    *RF_RX_Event;
BRTOS_Sem    *RF_TX_Event;
BRTOS_Sem    *MAC_Event;

BRTOS_TH	TH_RADIO;
BRTOS_TH	TH_MAC;
BRTOS_TH	TH_NETWORK;

#ifdef SIGNAL_APP1
BRTOS_Sem    *(SIGNAL_APP1);
#endif
#ifdef SIGNAL_APP2
BRTOS_Sem    *(SIGNAL_APP2);
#endif
#ifdef SIGNAL_APP3
BRTOS_Sem    *(SIGNAL_APP3);
#endif
#ifdef SIGNAL_APP4
BRTOS_Sem    *(SIGNAL_APP4);
#endif

#ifdef SIGNAL_APP255
BRTOS_Sem    *(SIGNAL_APP255);   // reservada para bootloader
#endif 

#if PROCESSOR == COLDFIRE_V1
#pragma warn_implicitconv off
#endif

const CHAR8 *unet_version=        ///< Informs network version
{
   UNET_VERSION
};

/* UNET network statistics */
static struct{
  INT16U rxed;       // received packets
  INT16U txed;       // successfully transmited
  INT16U txfailed;   // transmission failures
  INT16U routed;     // routed packets
  INT16U apptxed;    // apptxed packets
  INT16U dropped;    // packets dropped by hops limit, route not available
  INT16U overbuf;    // packets dropped by RX buffer overflow
  INT16U routdrop;   // packets dropped by routing buffer overflow
  INT16U rxedbytes;  // rxed bytes
  INT16U txedbytes;  // txed bytes
  INT16U rxbps;       // rx throughput
  INT16U txbps;       // tx throughput
  INT16U radioresets;  // radio reset
  INT16U hellos;       // hellos rxed
}UNET_NodeStat = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // 28 bytes


static void ClearUNET_NodeStat(void)
{
  UNET_NodeStat.rxed = 0;
  UNET_NodeStat.txed = 0;
  UNET_NodeStat.txfailed = 0;
  UNET_NodeStat.routed = 0;
  UNET_NodeStat.apptxed = 0;
  UNET_NodeStat.overbuf = 0;
  UNET_NodeStat.routdrop = 0;
  UNET_NodeStat.radioresets = 0;
  UNET_NodeStat.hellos = 0;
}

#if (DEVICE_TYPE == ROUTER)
#define CHECK_NODESTAT(x)   if((x) == 0xFFFF) ClearUNET_NodeStat();
#else
#define CHECK_NODESTAT(x)
#endif

void IncUNET_NodeStat_apptxed(void){
  UNET_NodeStat.apptxed++;
  CHECK_NODESTAT(UNET_NodeStat.apptxed)
}


/* Function to start all UNET Tasks */
void UNET_Init(void)
{  
  ////////////////////////////////////////////////////
  //     Initialize IEEE 802.15.4 radio mutex     ////
  ////////////////////////////////////////////////////  
  init_radio_resource(UNET_Mutex_Priority);
  
  
  ////////////////////////////////////////////////
  //     Initialize OS Network Services     //////
  ////////////////////////////////////////////////  
  if ((INT8U)OSQueueCreate(&RFBuffer,RFBufferSize,&RF) != ALLOC_EVENT_OK)
  {
    while(1){};              
  }

  /* UNET signals */
  if ((INT8U)OSSemCreate(0,&RF_RX_Event) != ALLOC_EVENT_OK)
  {
    while(1){};
  }
  
  if ((INT8U)OSSemCreate(0,&RF_TX_Event) != ALLOC_EVENT_OK)
  {
    while(1){};
  }  
  
  if ((INT8U)OSSemCreate(0,&MAC_Event) != ALLOC_EVENT_OK)
  {
    while(1){};
  } 
   
  
  #ifdef SIGNAL_APP1
    if ((INT8U)OSSemCreate(0,&(SIGNAL_APP1)) != ALLOC_EVENT_OK)
    {
      while(1){};
    } 
  #endif
  #ifdef SIGNAL_APP2
    if ((INT8U)OSSemCreate(0,&(SIGNAL_APP2)) != ALLOC_EVENT_OK)
    {
      while(1){};
    } 
  #endif 
  #ifdef SIGNAL_APP3
    if ((INT8U)OSSemCreate(0,&(SIGNAL_APP3)) != ALLOC_EVENT_OK)
    {
      while(1){};
    } 
  #endif 
  #ifdef SIGNAL_APP4
    if ((INT8U)OSSemCreate(0,&(SIGNAL_APP4)) != ALLOC_EVENT_OK)
    {
      while(1){};
    } 
  #endif 
  #ifdef SIGNAL_APP255
    if ((INT8U)OSSemCreate(0,&(SIGNAL_APP255)) != ALLOC_EVENT_OK)
    {
      while(1){};
    } 
  #endif    

     
  
    
  ////////////////////////////////////////////////
  //     Initialize Network Tasks           //////
  ////////////////////////////////////////////////
  if(InstallTask(&UNET_RF_Event,"RF Event Handler",UNET_RF_Event_StackSize,RF_EventHandlerPriority, NULL, &TH_RADIO) != OK)
  {
     while(1){};
  }
  if(InstallTask(&UNET_MAC,"UNET MAC Handler",UNET_MAC_StackSize,MAC_HandlerPriority, NULL, &TH_MAC) != OK)
  {
     while(1){};
  }
  
  if(InstallTask(&UNET_NWK,"UNET NWK Handler",UNET_NWK_StackSize,NWK_HandlerPriority, NULL, &TH_NETWORK) != OK)
  {
     while(1){};
  }

}

static INT16U StatTimer             = 0;
static INT16U AssociateTimeout      = 0;
static INT16U NeighborCnt           = 0;
static INT32U NeighbourhoodCnt      = 0;
volatile INT16U ping_retries = 0;
volatile INT16U debug_tx_count1 = 0;
volatile INT16U debug_tx_count2 = 0;
volatile INT16U debug_tx_count3 = 0;
volatile INT16U debug_tx_count4 = 0;

/** UNET Network Timeout Function */
void BRTOS_TimerHook(void)
{   
    
#if NETWORK_ENABLE == 1    
    
    // Realiza esta operação se está em processo de associação
    if (mac_tasks_pending.bits.AssociationInProgress)
    {
        AssociateTimeout++;
        if (AssociateTimeout >= (aResponseWaitTime+49)) 
        {
            mac_tasks_pending.bits.AssociationInProgress = 0;
            AssociateTimeout = 0;
        }
    }
    
    if (mac_tasks_pending.bits.isAssociated == 1)
    {
    	// Contador que define o momento para transmitir o ping da vizinhança
        NeighborCnt++;
        if (NeighborCnt >= NeighborPingTimeV)
        {
            NeighborCnt = 0;
            if (NeighborPingTimeCnt < MAX_PING_TIME)
            {
                NeighborPingTimeCnt++;
            }
            NeighborPingTimeV = (NEIGHBOR_PING_TIME * NeighborPingTimeCnt) + RadioRand();

            if (mac_tasks_pending.bits.AssociationInProgress != 1)
            {
				// Avisa que há um ping pendente
            	debug_tx_count1 = 0;
            	debug_tx_count2 = 0;
            	debug_tx_count3 = 0;
            	debug_tx_count4 = 0;
				nwk_tasks_pending.bits.DataPingPending = 1;

				// Transmite PING_RETRIES pings
				ping_retries = 0;
				nwk_tasks_pending.bits.RetryBroadcast = 1;

				// Acorda a tarefa de rede
				OSSemPost(MAC_Event);
            }
        }
        
        // Timeout para analisar a tabela de vizinhança
        NeighbourhoodCnt++; 
        if (NeighbourhoodCnt >= NeighbourhoodTimeout)
        {
            NeighbourhoodCnt = 0;

            // Avisa que deve verificar a tabela de vizinhança
            nwk_tasks_pending.bits.VerifyNeighbourhoodTable = 1;
            
            // Acorda a tarefa de rede
            OSSemPost(MAC_Event);
        }        
        
    }

    // Verifica se o radio está muito tempo sem receber mensagens
    RadioWatchdog++;
    if (RadioWatchdog > RadioWatchdogTimeout)
    {
      RadioWatchdog = 0;
      
      // Avisa que o radio pode estar travado
      nwk_tasks_pending.bits.RadioReset = 1;      
        
      // Acorda a tarefa de rede
      if (mac_tasks_pending.bits.isAssociated == 1)
      {
        OSSemPost(MAC_Event);
      }
    }    
    
    IncDepthWatchdog();    
    
    // update throughput stats every one sec
    // keep average of last 8 sec.
    if(++StatTimer >= 1000){
       StatTimer = 0;
       UNET_NodeStat.rxbps = (UNET_NodeStat.rxbps*7 + (UNET_NodeStat.rxedbytes*8))>>3;
       UNET_NodeStat.txbps = (UNET_NodeStat.txbps*7 + (UNET_NodeStat.txedbytes*8))>>3;
       UNET_NodeStat.rxedbytes = 0;
       UNET_NodeStat.txedbytes = 0;
    } 
    
#endif
         
}

/* UNET Application Handler */
void UNET_APP(void)
{   
    /* Retira todos os cabeçalhos e se houver algo a mais, 
    são outros atributos */
    if (mac_packet.Payload_Size < NWK_APP_HEADER_SIZE){ 
        app_packet.APP_Command_Size = 0;
    }else{
        app_packet.APP_Command_Size = mac_packet.Payload_Size - (NWK_APP_HEADER_SIZE);
    }
    
    // Acorda a tarefa que esta executando a aplicação
    switch(app_packet.APP_Identify)
    {    
      case APP_01:
        #ifdef SIGNAL_APP1
          OSSemPost(SIGNAL_APP1);
        #endif
        break; 
      case APP_02:
        #ifdef SIGNAL_APP2      
          OSSemPost(SIGNAL_APP2);
        #endif
        break;
      case APP_03:
        #ifdef SIGNAL_APP3      
          OSSemPost(SIGNAL_APP3);
        #endif
        break; 
      case APP_04:
        #ifdef SIGNAL_APP4      
          OSSemPost(SIGNAL_APP4);
        #endif
        break; 
      case APP_255:
        #ifdef SIGNAL_APP255
          OSSemPost(SIGNAL_APP255);
        #endif
        break;                     
      
      default:
        break;
    }
    
}



/* UNET Network Handler */
void UNET_NWK(void *param)
{
   // task setup
   INT8U i        = 0;
   
   (void)param;
   
   // Liga LED indicando que não está associado
   #ifdef ACTIVITY_LED
   ACTIVITY_LED_LOW;
   #endif

   // Liga LED indicando que não está associado
   #if (defined DEBUG_EXATRON && DEBUG_EXATRON == 1)
     #if (BDM_ENABLE == 0)
         BDM_DEBUG_OUT = 1;  /* turn off DEBUG OUT */
         DelayTask(500);
         BDM_DEBUG_OUT = 0;  /* turn on DEBUG OUT */
     #endif 
   #endif
   
   // Inicializa flags de estado da rede
   UserEnterCritical();
   mac_tasks_pending.Val = 0; 
   nwk_tasks_pending.Val = 0;
   UserExitCritical();
   
   NeighborPingTimeV = NEIGHBOR_PING_TIME + RadioRand() * 75;
   
   for(i=0;i<BeaconLimit;i++)
   {
       unet_beacon[i].Addr_16b = 0xFFFE;
   }     
   
   #if (DEVICE_TYPE == PAN_COORDINATOR)
   UserEnterCritical();
    mac_tasks_pending.bits.isAssociated = 1;
   UserExitCritical();
   #else
      if (macAddress == 0xFFFFFFFF) 
      {
		  #if (ROUTER_AUTO_ASSOCIATION == TRUE)
                  
          // Gera endereço estocástico
    	  // Temos que pegar os dados de algum define e depois gravar na flash
	  	  macAddr = (INT16U)1;
	  	  macPANId = 0x4742;

	  	  // Grava os endereços mac e panid no radio
          PHYSetDeviceAddress(macPANId,macAddr);

          UserEnterCritical();
            RouterCapacity = 1;
            mac_tasks_pending.bits.AssociationPending = 0;
            mac_tasks_pending.bits.isAssociated = 1;
          UserExitCritical();

          #else
              do
              {
                i = UNET_Associate();
              }while (i == FALSE);
          #endif        
      }else
      {
        macAddr = (INT16U)(macAddress & 0xFFFF);
        macPANId = (INT16U)(macPANIdentificator & 0xFFFF); 
        
        PHYSetDeviceAddress(macPANId,macAddr);
               
        UserEnterCritical();
          RouterCapacity = 1;     /* bug corrigido: 01-09-2014 */
          mac_tasks_pending.bits.AssociationPending = 0; 
          mac_tasks_pending.bits.isAssociated = 1;
        UserExitCritical();
      }         
   #endif
   
   
   
   // Desliga LED indicando que está associado
   #ifdef ACTIVITY_LED
    ACTIVITY_LED_HIGH;
   #endif
   
   // Liga LED indicando que está associado
   #if (defined DEBUG_EXATRON && DEBUG_EXATRON == 1)
     #if (BDM_ENABLE == 0)
         BDM_DEBUG_OUT = 1;  /* turn off DEBUG OUT */
         DelayTask(500);
         BDM_DEBUG_OUT = 0;  /* turn on DEBUG OUT */
     #endif 
   #endif
   
   // Limpa fila da vizinhança
   for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
   {
      unet_neighbourhood[i].Addr_16b            			= 0xFFFE;
      unet_neighbourhood[i].NeighborRSSI        			= 0;
      unet_neighbourhood[i].NeighborLastID      			= 0;
      unet_neighbourhood[i].IDTimeout           			= 0;
      unet_neighbourhood[i].NeighborDepth       			= NO_ROUTE_TO_BASESTATION;
      unet_neighbourhood[i].NeighborStatus.bits.Symmetric 	= FALSE;

      #if (USE_REACTIVE_UP_ROUTE == 1)      
      unet_routing_up_table[i].Addr_16b            = 0xFFFE;
      unet_routing_up_table[i].DestinyAddr		   = 0xFFFE;
      unet_routing_up_table[i].Destination         = FALSE;
      #endif
   }    
   
   // Limpa coordinator depth
   UserEnterCritical();
#if(DEVICE_TYPE == PAN_COORDINATOR)
   thisNodeDepth = 0;
#else
   thisNodeDepth = NO_ROUTE_TO_BASESTATION;
#endif
   UserExitCritical();
   
   // task main loop
   for (;;)
   {
      // Espera por um evento da camada MAC sem timeout
      OSSemPend(MAC_Event,0);
      
      acquireRadio();
      
      UserEnterCritical();
      // Verifica solicitação de roteamento de pacote
      if (nwk_tasks_pending.bits.RoutePending == 1)          // set in UNET_MAC
      {
    	  UserExitCritical();
    	  /* route and keep stats */
          if(HandleRoutePacket() == OK){      
            UNET_NodeStat.routed++;
            CHECK_NODESTAT(UNET_NodeStat.routed);
          }else{
            UNET_NodeStat.routdrop++;
            CHECK_NODESTAT(UNET_NodeStat.routdrop);
          }
          UserEnterCritical();
            nwk_tasks_pending.bits.RoutePending = 0;
          UserExitCritical();
      }else
      {
    	  UserExitCritical();
      }
      
      // Analisa novo ping de vizinho
      UserEnterCritical();
      if (nwk_tasks_pending.bits.NewNeighborPing == 1)      // set in UNET_MAC
      {
          UserExitCritical();
          
      	  HandleNewNeighborPing();
      	  
      	  UserEnterCritical();
          nwk_tasks_pending.bits.NewNeighborPing = 0;
          UserExitCritical();
      }else
  		{
  			  UserExitCritical();
  		}
           
      
      #if(DEVICE_TYPE != PAN_COORDINATOR)
  		  // Verifica novo endereço de posição
  		  UserEnterCritical();
  		  if (nwk_tasks_pending.bits.NewAddressArrived == 1)       // set in UNET_MAC
  		  {
  			  UserExitCritical();
  			  
  			  VerifyNewAddress();
  			  
  			  UserEnterCritical();
  			  nwk_tasks_pending.bits.NewAddressArrived = 0;
  			  UserExitCritical();
  		  }else
  		  {
  			  UserExitCritical();
  		  }
      #endif 

                     
      
      // Monta e transmite pacote com ping para vizinhança
	    UserEnterCritical();
      if (nwk_tasks_pending.bits.DataPingPending == 1)  // set in BRTOS_TimerHook
      {
          UserExitCritical();
           
          debug_tx_count4++;
          NeighborPing();
		  ping_retries++;

          if (OSSemPend(RF_TX_Event,PING_TIME) == TIMEOUT)
          {
        	  UserEnterCritical();
        	  //nwk_tasks_pending.bits.RadioReset = 1;
        	  nwk_tasks_pending.bits.RadioReset = 0;
        	  UserExitCritical();
          }else
          {
        	  nwk_tasks_pending.bits.RadioReset = 0;
          }
          
          UserEnterCritical();
          nwk_tasks_pending.bits.DataPingPending = 0;
          UserExitCritical();

    	if(nwk_tasks_pending.bits.RetryBroadcast == 1)
        {
    		if (ping_retries < PING_RETRIES)
			{
				// Avisa que há um ping pendente
    			UserEnterCritical();
				nwk_tasks_pending.bits.DataPingPending = 1;
				UserExitCritical();

				// Acorda a tarefa de rede
				OSSemPost(MAC_Event);
			}else
			{
				UserEnterCritical();
				nwk_tasks_pending.bits.RetryBroadcast = 0;
				UserExitCritical();
			}
        }

          #if (defined MULTICHANNEL_SUPPORT) && (MULTICHANNEL_SUPPORT==1)
            SetChannel(rx_channel); /* back to saved RX channel */
          #endif
      }else
      {
    	  UserExitCritical();
      }
      
      // Verifica a tabela de vizinhos
      UserEnterCritical();
      if (nwk_tasks_pending.bits.VerifyNeighbourhoodTable == 1)   // set in BRTOS_TimerHook
      {
          UserExitCritical();
          VerifyNeighbourhood();
          UserEnterCritical();
      	    nwk_tasks_pending.bits.VerifyNeighbourhoodTable = 0;
      	  UserExitCritical();
      }else
      {
        UserExitCritical();     
      }
      
      // Reset de Radio
      UserEnterCritical();
      if (nwk_tasks_pending.bits.RadioReset == 1)   // set in BRTOS_TimerHook & UNET_NWK
      {
          nwk_tasks_pending.bits.RadioReset = 0;
          UserExitCritical();          
          
          //  Disable receiving packets off air
          PHYSetShortRAMAddr(WRITE_BBREG1,0x04);
          
          // Limpa o buffer de recepção
          (void)OSCleanQueue(RF);
          
          MRF24J40Reset();

          //  Enable receiving packets off air
          PHYSetShortRAMAddr(WRITE_BBREG1,0x00);         
          
          // Statistics
          UNET_NodeStat.radioresets++;
          CHECK_NODESTAT(UNET_NodeStat.radioresets);
      }           

      releaseRadio();      
   }
}


static void RFBufferClean(void)
{
    (void)OSCleanQueue(RF);
    mac_packet.Dst_PAN_Ident = 0xFFFE;
    mac_packet.DstAddr_16b   = 0xFFFE;            
}

#if ((INCLUDE_PRINT == 1) && (DEVICE_TYPE == PAN_COORDINATOR))
#include "UART.h"
#endif

// UNET MAC Handler
void UNET_MAC(void *param)
{
   /* task setup */
   INT8U packet_size = 0;
   INT8U data1 = 0;
   INT8U data2 = 0;
   INT8U i = 0;
   INT8U index = 0;
   INT8U beacon = 0;
   INT8U packet_state = 0;
   INT8U packet_error = 0;
   INT16U CRCValue = 0;
  
   (void)param;
   packet_state = start_packet;
   BeaconCnt = 0; 
   
   for (;;) 
   {
      OSSemPend (RF_RX_Event,0);
            
      acquireRadio();
      
      // Retira da fila o tamanho do pacote
      (void)OSRQueue(&RFBuffer,&packet_size);
      
      packet_state = start_packet;
      packet_error = 0;
      
      if (packet_size > 128)
      {
        RFBufferClean();
        packet_error++;
        packet_state = end_packet;
      }
      
      // Retira do pacote o frame control
      (void)OSRQueue(&RFBuffer,&data1);
      (void)OSRQueue(&RFBuffer,&data2);
      mac_frame_control.Val = (INT16U)((data1 << 8)  | data2);

      // Retira do pacote o sequence number
      (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.Sequence_Number);
      packet_size -= 3;
      
      if (packet_size > 128)
      {
        RFBufferClean();
        packet_error++;
        packet_state = end_packet;
      }            
      
      // Inicia máquina de estados para decodificar pacote
      while(packet_state != end_packet)
      {  
        switch(packet_state)
        {
          case start_packet:
            switch(mac_frame_control.bits.DstAddrMode)
            {
              case 0:
                packet_state = dest_00;
                break;
              case 1:
                packet_state = dest_01;
                break;
              case 2:
                packet_state = dest_10;
                packet_size -= 4;
                if (packet_size > 128)
                {
                  RFBufferClean();
                  packet_error++;
                  packet_state = end_packet;
                }                
                break;
              case 3:
                packet_state = dest_11;
                packet_size -= 10;
                if (packet_size > 128)
                {
                  RFBufferClean();
                  packet_error++;
                  packet_state = end_packet;
                }                
                break;
              default:
                RFBufferClean();
                packet_error++;
                packet_state = end_packet;              
                break;
            }
          break;
          
          case dest_00:
             mac_packet.DstAddr_16b   = 0xFFFE;
             mac_packet.Dst_PAN_Ident = 0xFFFE;
             for(i=8;i>0;i--)
             {
                mac_packet.DstAddr_64b[i-1] = 0;
             }           
             packet_state = intra_pan; 
             break;
          case dest_01:
             for(i=0;i<packet_size;i++)
             {
                (void)OSRQueue(&RFBuffer,&data1);    
             }
             packet_state = end_packet;
             break;
          case dest_10:
             (void)OSRQueue(&RFBuffer,&data1);
             (void)OSRQueue(&RFBuffer,&data2);
             mac_packet.Dst_PAN_Ident = (INT16U)((data2 << 8) | data1);
             (void)OSRQueue(&RFBuffer,&data1);
             (void)OSRQueue(&RFBuffer,&data2);
             mac_packet.DstAddr_16b = (INT16U)((data2 << 8) | data1);
             packet_state = intra_pan;
             break;
          case dest_11:
             (void)OSRQueue(&RFBuffer,&data1);
             (void)OSRQueue(&RFBuffer,&data2);
             mac_packet.Dst_PAN_Ident = (INT16U)((data2 << 8) | data1);
             for(i=8;i>0;i--)
             {
                (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.DstAddr_64b[i-1]);
             }
             packet_state = intra_pan;        
             break;                                 
          case intra_pan:
             if(mac_frame_control.bits.IntraPAN)
             {
                switch(mac_frame_control.bits.SrcAddrMode)
                {
                  case 0:
                    packet_state = source_100;
                    break;
                  case 1:
                    packet_state = source_101;
                    break;
                  case 2:
                    packet_state = source_110;                
                    break;
                  case 3:
                    packet_state = source_111;
                    break;
                  default:
                    RFBufferClean();
                    packet_error++;
                    packet_state = end_packet;              
                    break;                    
                }
                
             }else
             {
                switch(mac_frame_control.bits.SrcAddrMode)
                {
                  case 0:
                    packet_state = source_000;
                    break;
                  case 1:
                    packet_state = source_001;
                    break;
                  case 2:
                    packet_state = source_010;                
                    break;
                  case 3:
                    packet_state = source_011;
                    break;
                  default:
                    RFBufferClean();
                    packet_error++;
                    packet_state = end_packet;              
                    break;                                                                          
                }           
             }
            break;
          case source_000:
            mac_packet.SrcAddr_16b   = 0xFFFE;
            mac_packet.Src_PAN_Ident = 0xFFFE;
            for(i=8;i>0;i--)
            {
               mac_packet.SrcAddr_64b[i-1] = 0;
            }
            packet_state = payload;
            break;
          case source_001:
            for(i=0;i<packet_size;i++)
            {
               (void)OSRQueue(&RFBuffer,&data1);
            }
            packet_state = end_packet;
            break;
          case source_010:
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            mac_packet.Src_PAN_Ident = (INT16U)((data2 << 8) | data1);
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            mac_packet.SrcAddr_16b = (INT16U)((data2 << 8) | data1);
            packet_size -= 4;
            if (packet_size > 128)
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }
            packet_state = payload;
            break;
          case source_011:
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            mac_packet.Src_PAN_Ident = (INT16U)((data2 << 8) | data1);
            for(i=8;i>0;i--)
            {
               (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.SrcAddr_64b[i-1]);
            }        
            packet_size -= 10;
            if (packet_size > 128)
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }            
            packet_state = payload;
            break;
          case source_100:
            mac_packet.SrcAddr_16b   = 0xFFFE;
            mac_packet.Src_PAN_Ident = 0xFFFE;
            for(i=8;i>0;i--)
            {
               mac_packet.SrcAddr_64b[i-1] = 0;
            }
            packet_state = payload;         
            break;
          case source_101:
            for(i=0;i<packet_size;i++)
            {
               (void)OSRQueue(&RFBuffer,&data1);
            }
            packet_state = end_packet;        
            break;
          case source_110:
            mac_packet.Src_PAN_Ident = 0xFFFE;
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            mac_packet.SrcAddr_16b = (INT16U)((data2 << 8) | data1);
            packet_size -= 2;
            if (packet_size > 128)
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }            
            packet_state = payload;
            break;
          case source_111:
            mac_packet.Src_PAN_Ident = 0xFFFE;
            for(i=8;i>0;i--)
            {
               (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.SrcAddr_64b[i-1]);
            }
            packet_size -= 8;
            if (packet_size > 128)
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }            
            packet_state = payload;
            break;
          case payload:
            mac_packet.Payload_Size = 0;
            if (packet_size > 123) 
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }
            for(i=0;i<(packet_size-2);i++)
            {
               (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.MAC_Payload[i]);
               mac_packet.Payload_Size++;
            }
            packet_size = (INT8U)(packet_size - mac_packet.Payload_Size);
            if (packet_size > 128)
            {
              RFBufferClean();
              packet_error++;
              packet_state = end_packet;
            }             
            packet_state = CRC;         
            break;
          case CRC:
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            mac_packet.Frame_CRC = (INT16U)((data2 << 8) | data1);
            packet_size -= 2;
            
            // Salva CRC Computado
            (void)OSRQueue(&RFBuffer,&data1);
            (void)OSRQueue(&RFBuffer,&data2);
            CRCValue = (INT16U)((data1 << 8) | data2);
            
            // Salva o RSSI (Receiver Signal Strength Indicator)
            (void)OSRQueue(&RFBuffer,(INT8U *)&mac_packet.Frame_RSSI);
             // Salva o LQI (Link Quality Indicator)
            (void)OSRQueue(&RFBuffer,(INT8U*)(&mac_packet.Frame_LQI));
            
            packet_state = end_packet;
            break;
          default:
            RFBufferClean();
            packet_error++;
            packet_state = end_packet;
            break;                    
        }
      }
      
      
      packet_state = start_packet;
      
            
      /* Tem espaço para o próximo pacote ? */
      if(RFBuffer.OSQEntries + 133  <= RFBufferSize){
        PHYSetAutoACK(1);        
      }
      
      
      // Reset do contador de Watchdog do Radio
      RadioWatchdog = 0;
      
      if ((mac_packet.Frame_CRC != CRCValue) || (packet_error != 0)){         
          UNET_NodeStat.dropped++;
          CHECK_NODESTAT(UNET_NodeStat.dropped);
      }else 
      {
      
          if (mac_tasks_pending.bits.isAssociated == 1)
          {
            // Testa o PANId
            data1 = 0;
            if((mac_packet.Dst_PAN_Ident == macPANId) || (mac_packet.Dst_PAN_Ident == 0xFFFF))
            {  
                // Testa o endereço
                switch(mac_frame_control.bits.DstAddrMode)
                {
                  case 0b10:
                    // Testa endereço de 16 bits
                    if(mac_packet.DstAddr_16b != macAddr)
                      data1 = 0xFF;
                    
                    // Verifica se é broadcast
                    if(mac_packet.DstAddr_16b == 0xFFFF)
                      data1 = 0;            
                    break;
                  case 0b11:
                    // Testa endereço de 64 bits
                    for(i=0;i<8;i++)
                    {
                      if(mac_packet.DstAddr_64b[i] != mac64Address[i])
                      {
                        data1++;
                      }
                    }
                    break;
                  default:
                    UNET_NodeStat.dropped++;
                    CHECK_NODESTAT(UNET_NodeStat.dropped);
                    (void)OSCleanQueue(RF);
                    break;
                }
            }else
            {
              // PANId diferente da esperada
              data1 = 0xFF;
            }
            
            
            if(data1 != 0) {
                UNET_NodeStat.dropped++;
                CHECK_NODESTAT(UNET_NodeStat.dropped);
            }else   // Caso o endereço esteja correto
            {  
                // Trata somente pacotes Data e MAC
                // caso seja outro tipo de pacote, descarta
                if(mac_frame_control.bits.FrameType == DataFrame)
                {
                    
                    // Trafego da rede
                    #ifdef ACTIVITY_LED
                	     ACTIVITY_LED_TOGGLE;
                    #endif
                    
                    // Trafego da rede
                    #if (defined DEBUG_EXATRON && DEBUG_EXATRON == 1)
                    	UserEnterCritical();
                        mac_tasks_pending.bits.isDataFrameRxed = 1;
                        UserExitCritical();
                    #endif                      
                    
                    // Analisa tipo de data frame
                    switch(mac_packet.MAC_Payload[0])
                    {
                      case DATA_PING:
                        // pacote de ping de vizinhança
						#if ((INCLUDE_PRINT == 1) && (DEVICE_TYPE == PAN_COORDINATOR))
                    	PRINT_PING_INFO();
						#endif
                        unet_neighbor_ping.Addr_16b            = mac_packet.SrcAddr_16b;
                        unet_neighbor_ping.NeighborRSSI        = mac_packet.Frame_RSSI;
                        unet_neighbor_ping.NeighborLQI         = mac_packet.Frame_LQI;
                        unet_neighbor_ping.NeighborDepth       = mac_packet.MAC_Payload[1];
                        index = 2;
                        
                        // Copy neighbourhood of this neighbor
                        unet_neighbor_ping.NeighborsNumber = 0;
                        for(i=0;i<((mac_packet.Payload_Size - index)/3);i++)
                        {
                          if(i>=NEIGHBOURHOOD_SIZE) break; 
                          unet_neighbor_ping.Neighbors[i] = (INT16U)((mac_packet.MAC_Payload[(index+(i*3))] << 8) | mac_packet.MAC_Payload[(index+1+(i*3))]);
                          unet_neighbor_ping.NeighborsRSSI[i] = mac_packet.MAC_Payload[(index+2+(i*3))];
                          unet_neighbor_ping.NeighborsNumber++;
                        }
                        
                        // Acorda tarefa de rede
                        UserEnterCritical();
                          nwk_tasks_pending.bits.NewNeighborPing = 1;
                        UserExitCritical();
                        
                        UNET_NodeStat.hellos++;
                        CHECK_NODESTAT(UNET_NodeStat.hellos);
                        
                        OSSemPost(MAC_Event);
                        break;
                      case BROADCAST_PACKET:
                        if (VerifyPacketReplicated() == OK)
                        {
                          // Acorda tarefa de rede
                          UserEnterCritical();
                          nwk_tasks_pending.bits.BroadcastPending = 1;
                          UserExitCritical();
                          
                          OSSemPost(MAC_Event);                        
                        }
                        break;
                      case ROUTE_PACKET:
                        if (VerifyPacketReplicated() == OK)
                        {
                          // Armazena no buffer de roteamento e acorda tarefa de rede
                          UserEnterCritical();
                            nwk_tasks_pending.bits.RoutePending = 1;
                          UserExitCritical();
                          
                          OSSemPost(MAC_Event);
                        }
                        break;
                      
                      case ADDRESS_PACKET:
                        if (VerifyPacketReplicated() == OK)
                        {
                          // pacote de endereço novo
                          UserEnterCritical();
                            nwk_tasks_pending.bits.NewAddressArrived = 1;
                          UserExitCritical();                            
                          
                          OSSemPost(MAC_Event);
                        }
                        break;                        
                      default:                        
                        break;
                    }
                
                
                }
            
                if(mac_frame_control.bits.FrameType == MACFrame)
                {
                    // Chama função que trata MAC Frame Command
                    // Por enquanto só trata "Association request", "Data request" e "Beacon request"
                    MAC_Response();
                }
            }
          }else
          {
            if (mac_tasks_pending.bits.ScanInProgress == 1)
            {
              // Verifica se o pacote é beacon
              // se não for, descarta o pacote
              if(mac_frame_control.bits.FrameType == BeaconFrame)
              {
                  beacon = MAC_BeaconVerify();
                  if (beacon == TRUE)
                  {
                    // Trata beacon
                    if (BeaconCnt < BeaconLimit)
                    {
                        // Verifica se este beacon já não está alocado
                        for(i=0;i<BeaconLimit;i++)
                        {
                          if (unet_beacon[i].Addr_16b == mac_packet.SrcAddr_16b)
                          {
                            // utiliza a mesma variável para informar
                            // que o beacon deste PANId já está alocado
                            beacon = FALSE;
                            break;
                          }
                        }
                        
                        // Se o beacon da rede UNET ainda não foi alocado, Aloca
                        if (beacon == TRUE)
                        { 
                          unet_beacon[BeaconCnt].PAN_Ident         = mac_packet.Src_PAN_Ident;
                          unet_beacon[BeaconCnt].Addr_16b          = mac_packet.SrcAddr_16b;
                          unet_beacon[BeaconCnt].Beacon_RSSI       = mac_packet.Frame_RSSI;
                          // Verificar formato do Beacon Frame
                          unet_beacon[BeaconCnt].DeviceDepth       = mac_packet.MAC_Payload[6];
                          unet_beacon[BeaconCnt].AssociationStatus = 0;
                          BeaconCnt++;
                        }
                    }
                    
                  }
              }          
            } else
            {
              // Analisa se o endereço de destino do pacote
              // caso não seja o endereço do nó, descarta pacote
              data1 = 0;
              for(i=0;i<8;i++)
              {
                  if(mac_packet.DstAddr_64b[i] != mac64Address[i])
                  {
                    data1 = 0xFF;
                    break;
                  }
                  
              }
              
              if (data1 == 0)
              {
                  // Se o endereço estiver correto
                  // Verifica se é um MAC Frame
                  if(mac_frame_control.bits.FrameType != MACFrame)
                  {
                      // Tratar MAC frames da associação
                      OSSemPost(MAC_Event);
                  }
              }
            }
          }
      }
      
      releaseRadio();
   }
}

/* Task to handle radio Rx and Tx events */
void UNET_RF_Event(void *param)
{
  /* task setup */
  volatile MRF24J40_IFREG flags;
  INT16U         size = 0;
  INT8U          i;
  INT8U          RSSI_VAL;
  INT8U          LQI_VAL;
  INT8U          ReceivedByte=0;  
  INT16U         j=0;
  INT16U         CRCValue = 0;

  (void)param;
  //////////////////////////////////////////////
  //     Initialize IEEE 802.15.4 radio     ////
  //////////////////////////////////////////////

  // Initialize SPI Port
  SPI_Init();

  // Initialize interrupt and I/O port for the radio
  Radio_port_Init();

  // Initialize radio registers
  MRF24J40Init();
  
  for (;;)
  {
      // Stop Task to wait RF Event
      OSSemPend (RF_Event,0);
        
      // read the interrupt status register to see what caused the interrupt        
      flags.Val=PHYGetShortRAMAddr(READ_ISRSTS);
           
      if(flags.bits.RF_TXIF)
      {
        //if the TX interrupt was triggered
        BYTE results;                       
                
        debug_tx_count1++;
        //read out the results of the transmission
        results.Val = PHYGetShortRAMAddr(READ_TXSR);
                
        if(results.bits.b0 == 1)
        {
          //the transmission wasn't successful and the number
          //of retries is located in bits 7-6 of TXSR
          //failed to Transmit
          UNET_NodeStat.txfailed++;
          CHECK_NODESTAT(UNET_NodeStat.txfailed);
          macACK = FALSE;
        }
        else
        {
          //transmission successful
          //MAC ACK received if requested else packet passed CCA
          debug_tx_count2++;
          UNET_NodeStat.txed++;
          CHECK_NODESTAT(UNET_NodeStat.txed);
          
          i=PHYGetLongRAMAddr(0x001);
          
          if(UNET_NodeStat.txedbytes<0xFFFF){
             UNET_NodeStat.txedbytes +=i;
          }
          macACK = TRUE;
        }
        
        if (mac_tasks_pending.bits.PacketPendingAck == 1)
        {
          UserEnterCritical();
          debug_tx_count3++;
          mac_tasks_pending.bits.PacketPendingAck = 0;
          UserExitCritical();
          OSSemPost(RF_TX_Event);
        }
      }
            
      if(flags.bits.RF_RXIF)
      {                      
         // packet received
         // Need to read it out          
         // Disable receiving packets off air
         PHYSetShortRAMAddr(WRITE_BBREG1,0x04);
         
         //first byte of the receive buffer is the packet length                
         i=PHYGetLongRAMAddr(0x300);          
         
         // Verifica se aceitar o pacote irá gerar buffer underrun
         size = RFBuffer.OSQEntries + i + 4;
         
         if(RFBufferSize > size)
         {            
             // Incrementa o numero de pacotes recebidos
             UNET_NodeStat.rxed++;
             CHECK_NODESTAT(UNET_NodeStat.rxed);
             
             if(UNET_NodeStat.rxedbytes<0xFFFF){
                UNET_NodeStat.rxedbytes +=i;
             }
                
             for(j=0;j<=i;j++)
             {
                //read out the rest of the buffer
                ReceivedByte = PHYGetLongRAMAddr((INT16U)0x300 + j);    
                OSWQueue(&RFBuffer,ReceivedByte);
                if ((j > 0) && (j <= i-2))
                {
                  CRC_Update(ReceivedByte);
                }
             }
             
             CRCValue = CRC_Get();       
             
             // Guarda o CRC computado
             OSWQueue(&RFBuffer,(INT8U)(CRCValue >> 8));
             OSWQueue(&RFBuffer,(INT8U)(CRCValue & 0xFF));

             // LQI  --> 0x300+i+(INT16U)1
             // RSSI --> 0x300+i+(INT16U)2
             LQI_VAL  = PHYGetLongRAMAddr(0x300+i+(INT16U)1);
             RSSI_VAL = PHYGetLongRAMAddr(0x300+i+(INT16U)2);
             
             // Guarda no final da fila o valor do RSSI
             OSWQueue(&RFBuffer,RSSI_VAL);
             OSWQueue(&RFBuffer,LQI_VAL);
             
             OSSemPost(RF_RX_Event);
             
             /* Não tem espaço para o próximo pacote ? */
             if(RFBuffer.OSQEntries + 133  > RFBufferSize){
                /* Desabilita ACK automatico */
                PHYSetAutoACK(0);                
             }              
             
         }else{
            // Incrementa o numero de pacotes descartados por overflow de buffer de RX
            UNET_NodeStat.overbuf++;
            CHECK_NODESTAT(UNET_NodeStat.overbuf);
         }
         
        // Reinicia o ponteiro do buffer RX
        PHYSetShortRAMAddr(WRITE_RXFLUSH, 0x01);
        // bypass MRF24J40 errata 5
        PHYSetShortRAMAddr(WRITE_FFOEN, 0x98);        
        //  Enable receiving packets off air
        PHYSetShortRAMAddr(WRITE_BBREG1,0x00);                                          
      
      }
      
      (void)RFFLAG;           // Lê o registrador de ISR
      RFIF;                   // Limpa a flag de ISR
      RFINT_ENABLE;           // Habilita interrupção de hardware ligada ao pino INT
      
  }
}

/* Return a pointer to "UNET_NodeStat" struct */
INT8U* GetUNET_Statistics(INT8U* tamanho){
    if(tamanho == NULL) return NULL;
    
    *tamanho = sizeof(UNET_NodeStat);
    return (INT8U*)&UNET_NodeStat;
}







