/**********************************************************************************
@file   network.c
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

#include "hardware.h"
#include "BRTOS.h"
#include "drivers.h"
#include "MRF24J40.h"
 
#include "unet_api.h"

#if (PROCESSOR == ARM_Cortex_M0)
#include "FLASH.h"
#endif

#if(DEVICE_TYPE == ROUTER)
#include "app.h" 
#define CFG_PARENT_THRESHOLD      Config_PARENT_THRESHOLD
#define CFG_PARENT_THRESHOLD_MIN  Config_PARENT_THRESHOLD_MIN
#else
#define CFG_PARENT_THRESHOLD      0
#define CFG_PARENT_THRESHOLD_MIN  0
#endif

 
// Tabela de nós vizinhos
volatile UNET_NEIGHBOURHOOD              unet_neighbourhood[NEIGHBOURHOOD_SIZE];

#if(DEVICE_TYPE == PAN_COORDINATOR)
volatile INT8U							 thisNodeDepth = 0;
#else
volatile INT8U							 thisNodeDepth = NO_ROUTE_TO_BASESTATION;
#endif

volatile INT16U							 ParentNeighborID = 0xFFFE;
volatile INT16U							 ParentRSSI = 0;

// 
volatile UNET_SYMMETRIC_NEIGHBOURHOOD    unet_neighbor_ping;
volatile NEIGHBOR_TABLE_T                NeighborTable = 0;

#if (USE_REACTIVE_UP_ROUTE == 1)
volatile UNET_ROUTING_UP_TABLE           unet_routing_up_table[ROUTING_UP_TABLE_SIZE];
#endif


// Payload das mensagens roteadas
volatile INT8U                 			NWKPayload[MAX_APP_PAYLOAD_SIZE];

#if (NWK_MUTEX_TYPE == BRTOS_MUTEX)
/* Mutex for Radio IEEE 802.15.4 */
BRTOS_Mutex                  *Radio_IEEE802;
#endif

static   INT16U              DepthWatchdog        = 0;
volatile INT16U              RadioWatchdog        = 5000;
volatile INT8U               NeighborPingTimeCnt  = 1;

#if (USE_REACTIVE_UP_ROUTE == 1)
volatile INT8U				 ReactiveUpTimeCnt    = 1;
volatile INT16U 			 ReactiveUpCnt        = 0;
#endif

volatile NWK_TASKS_PENDING   nwk_tasks_pending;


//Função para adquirir direito exclusivo ao radio
void acquireRadio(void)
{  
    OSMutexAcquire(Radio_IEEE802);
}

//Função para liberar o radio
void releaseRadio(void)
{
    OSMutexRelease(Radio_IEEE802);
}


/* 
   Cria um mutex informando que o recurso está disponível
   após a inicialização da prioridade máxima a acessar o recurso = priority
*/
void init_radio_resource(INT8U priority)
{
  if (OSMutexCreate(&Radio_IEEE802,priority) != ALLOC_EVENT_OK)
  {
    while(1){};
  };
}

// Monta pacote de comando Neighbor Ping
// HeaderSize = 9 bytes 
// MAX_BASE_STATION = 4
// Payload Max. = 2 + 9*4 + 8 + 16*3  = 94 bytes
// Sobram 22 bytes dos 125 disponíveis
// 4 bits por canal dos vizinhos * 16 = 8 bytes com suporte multicanal
// 1 byte para canal e contagem de vizinhos 
void NeighborPing(void)
{
  INT8U i           = 0;
  INT8U address     = 0;
  INT8U HeaderSize  = 0;
  INT8U PayloadSize = 0;
                            
  // Inicia montagem do pacote Data
  
  // Indicação de Beacon no Frame Control
  PHYSetLongRAMAddr(2, 0x41);
  HeaderSize++;
  
  // Indicação de Dest e Source Address de 16b, no Frame Control    
  PHYSetLongRAMAddr(3, 0x88);
  HeaderSize++;  
  
  // Sequence Number
  PHYSetLongRAMAddr(4, SequenceNumber);
  if (++SequenceNumber == 0) SequenceNumber = 1;
  HeaderSize++;
  
  // PanId do coordenador que gera o data packet
  PHYSetLongRAMAddr(5, (INT8U)(macPANId & 0xFF));
  PHYSetLongRAMAddr(6, (INT8U)(macPANId >> 8));
  
  // Endereço de destino broadcast do data packet
  PHYSetLongRAMAddr(7, (INT8U)0xFF);
  PHYSetLongRAMAddr(8, (INT8U)0xFF);
  
  // Endereço fonte do data packet
  PHYSetLongRAMAddr(9, (INT8U)(macAddr & 0xFF));
  PHYSetLongRAMAddr(10, (INT8U)(macAddr >> 8));
  HeaderSize += 6;  
  
  
  // Enviar dados do pacote de vizinhança padrão da rede
  // Tipo de pacote de dados
  PHYSetLongRAMAddr(11, DATA_PING);
  PayloadSize++;

  PHYSetLongRAMAddr(12, thisNodeDepth);
  PayloadSize++;

  address = 12;
  
  // Coloca os vizinhos no pacote
  for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
  {
    if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
    {
      PHYSetLongRAMAddr((INT16U)(++address), (INT8U)(unet_neighbourhood[i].Addr_16b >> 8));
      PHYSetLongRAMAddr((INT16U)(++address), (INT8U)(unet_neighbourhood[i].Addr_16b & 0xFF));
      PHYSetLongRAMAddr((INT16U)(++address), (INT8U)(unet_neighbourhood[i].NeighborRSSI & 0xFF));
      PayloadSize += 3;
    }
  }

  // Informação do tamanho do MAC header em bytes
  // No modo não seguro é ignorado
  PHYSetLongRAMAddr(0x000,HeaderSize);
  // Informação do tamanho em bytes do MAC header + Payload
  PHYSetLongRAMAddr(0x001,(INT8U)(HeaderSize+PayloadSize));

  //transmit packet without ACK requested
  mac_tasks_pending.bits.PacketPendingAck = 1;
  PHYSetShortRAMAddr(WRITE_TXNMTRIG,0b00000001);
}



// Verifica se o endereço será reprogramado
void VerifyNewAddress(void)
{
  INT32U OldAddress = 0; 
  
#if DEVICE_TYPE != PAN_COORDINATOR
#if (PROCESSOR == COLDFIRE_V1)
  OldAddress = (INT32U)(*(INT32U*)macAddress);
#endif

#if (PROCESSOR == ARM_Cortex_M0)
  OldAddress = (INT32U)(*(INT32U*)MAC16_MEM_ADDRESS);
#endif
#endif

  if (OldAddress == 0xFFFFFFFF){
    // Grava na flash endereco de rede    
#if FLASH_SUPPORTED == 1
#if (PROCESSOR == COLDFIRE_V1)
    UserEnterCritical();
    // Grava na flash endereco mac
    OldAddress = (INT32U)(macAddr & 0xFFFF);
    Flash_Prog((INT32U)&macAddress, (INT32U)&OldAddress, 1);
    
    // Grava na flash mac pan id
    OldAddress = (INT32U)(macPANId & 0xFFFF);
    Flash_Prog((INT32U)&macPANIdentificator, (INT32U)&OldAddress, 1);    
    UserExitCritical();
#endif

#if (PROCESSOR == ARM_Cortex_M0)
#if (DEVICE_TYPE == ROUTER)
    // Grava na flash endereco mac
    UserEnterCritical();
    OldAddress = (INT32U)(macAddr & 0xFFFF);
    WriteToFlash((INT8U*)&OldAddress, MAC16_MEM_ADDRESS, 4);

    // Grava na flash mac pan id
    OldAddress = (INT32U)(macPANId & 0xFFFF);
    WriteToFlash((INT8U*)&OldAddress, PANID_MEM_ADDRESS, 4);
    UserExitCritical();
#endif
#endif
#endif
  }
}



INT8U VerifyPacketReplicated(void)
{
    INT8U i = 0;
    
    for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
    {
        if (unet_neighbourhood[i].Addr_16b == mac_packet.SrcAddr_16b)
        {
          if (unet_neighbourhood[i].NeighborLastID == mac_packet.Sequence_Number)
          {
            return TRUE;
          }else
          {
            unet_neighbourhood[i].NeighborLastID = mac_packet.Sequence_Number;
            unet_neighbourhood[i].IDTimeout      = LAST_ID_SYSTEM_TIMER_TIMEOUT;
            return OK;
          }
        }
    }
    return OK;
}



void VerifyNeighbourhoodLastIDTimeout(void)
{
    INT8U i = 0;
    
    for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
    {
        if (unet_neighbourhood[i].IDTimeout)
        {
          unet_neighbourhood[i].IDTimeout--;
          
          if (unet_neighbourhood[i].IDTimeout == 0)
          {
              unet_neighbourhood[i].NeighborLastID = 0;
          }
        }
    }
}

void VerifyNeighbourhood(void)
{
      INT8U i = 0;
      
      // Verifica se existem vizinhos inativos
      for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
      {        
        // Se o nó está inativo por um determinado tempo
        if ((NeighborTable & (0x01 << i)) == 0)
        {
          // Retira da tabela de vizinhos
          if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
          {
            // Update Base Station Tables
        	if (ParentNeighborID == unet_neighbourhood[i].Addr_16b)
        	{
        		thisNodeDepth = ROUTE_TO_BASESTATION_LOST;
        		ParentNeighborID = 0xFFFE;
        		ParentRSSI = 0;
                
                // Inicia contador de estabilização de profundidade
                ClearDepthWatchdog();
        	}

            // Delete Neighbor information
            unet_neighbourhood[i].Addr_16b      = 0xFFFE;
            unet_neighbourhood[i].NeighborRSSI  = 0;

            // Aumenta a quantidade de pings qdo um nó sai da rede
            // Acelera o ping para propagar esta informação
            UserEnterCritical();
            NeighborPingTimeCnt = 1;
            UserExitCritical();
          }
        }
      }

      if (thisNodeDepth == ROUTE_TO_BASESTATION_LOST)
      {
    	  // Update this node depth
    	  UpdateDepth();
      }
            
      // Clear table to refresh neighbors activity
      NeighborTable = 0;
}

#if (USE_REACTIVE_UP_ROUTE == 1)
void VerifyUpRouteTable(void)
{
      INT8U i = 0;

      // Verifica se existem nós inativos
      for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
      {
        // Se o nó está inativo por um determinado tempo
        if (unet_routing_up_table[i].activity == FALSE)
        {
          // Retira da tabela de rotas up
          if (unet_routing_up_table[i].DestinyAddr != 0xFFFE)
          {
            // Delete node information
        	unet_routing_up_table[i].DestinyAddr   = 0xFFFE;
        	unet_routing_up_table[i].Addr_16b      = 0xFFFE;
        	unet_routing_up_table[i].Destination   = FALSE;
        	unet_routing_up_table[i].hops		   = 0;
          }
        }
      }

      // Clear table to refresh nodes activity
      for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
      {
    	  unet_routing_up_table[i].activity = FALSE;
      }
}
#endif


void IncDepthWatchdog(void) 
{
    DepthWatchdog++;
}

INT16U GetDepthWatchdog(void) 
{
  return DepthWatchdog;
}

void ClearDepthWatchdog(void) 
{
  DepthWatchdog = 0;
}


//ParentNeighborID
//ParentRSSI
void UpdateDepth(void)
{
    int i = 0;
	// Varre a lista de vizinhos
    for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
    {
      // Verifica se existe vizinhos com profundidade menor
      if (unet_neighbourhood[i].NeighborDepth < thisNodeDepth)
      {
    	  // Verifica se o nó com menor profundidade é simétrico
    	  if(unet_neighbourhood[i].NeighborStatus.bits.Symmetric == TRUE)
    	  {
    		  if (thisNodeDepth == (unet_neighbourhood[i].NeighborDepth + 1))
    		  {
    			  // Mesma profundidade, verificando o RSSI do nó pai
    			  if (ParentNeighborID == unet_neighbourhood[i].Addr_16b){
    				  // Atualiza o RSSI do nó pai
    				  ParentRSSI = unet_neighbourhood[i].NeighborRSSI;
    			  }else{
    				  if (unet_neighbourhood[i].NeighborRSSI > ParentRSSI){
    					  // Troca nó pai para um nó de melhor enlace
    					  ParentNeighborID = unet_neighbourhood[i].Addr_16b;
    					  ParentRSSI = unet_neighbourhood[i].NeighborRSSI;
    				  }
    			  }
    		  }else{
    			  // Reduziu a profundidade através do vizinho escolhido
    			  thisNodeDepth = unet_neighbourhood[i].NeighborDepth + 1;
    			  ParentNeighborID = unet_neighbourhood[i].Addr_16b;
    			  ParentRSSI = unet_neighbourhood[i].NeighborRSSI;
    		  }
    	  }
      }
    }
}


 
void HandleNewNeighborPing(void)
{
      INT8U i = 0;
      INT8U j = 0;
      INT8U k = 0;
#if (defined CHECK_DUPLICATE_MAC) && (CHECK_DUPLICATE_MAC == 1)
      INT8U foundme = 0;
#endif
      
      i = NEIGHBOURHOOD_SIZE;
      
      // Verifica se o vizinho já está na tabela
      for(j=0;j<NEIGHBOURHOOD_SIZE;j++)
      {
        if (unet_neighbourhood[j].Addr_16b == unet_neighbor_ping.Addr_16b)
        {
          i = j;
          break;
        }
      }
      
      // Se não está na tabela
      if (i == NEIGHBOURHOOD_SIZE) 
      {
          // Procura posição vazia
          for(j=0;j<NEIGHBOURHOOD_SIZE;j++)
          {
            if (unet_neighbourhood[j].Addr_16b == 0xFFFE)
            { 
              i = j;
              k = 1;
              // Aumenta a quantidade de pings qdo um novo nó entrar na rede
              UserEnterCritical();
              NeighborPingTimeCnt = 1;
              UserExitCritical();
              break;
            }
          }      
        
      }
      
      // Só grava vizinho se houver posição livre
      if (i<NEIGHBOURHOOD_SIZE)
      {
          unet_neighbourhood[i].Addr_16b      = unet_neighbor_ping.Addr_16b;
          
          // Verifica se o vizinho é novo para calcular média de RSSI
          if (k)
          {
            unet_neighbourhood[i].NeighborRSSI = unet_neighbor_ping.NeighborRSSI;
          }else
          {
            unet_neighbourhood[i].NeighborRSSI = (INT8U)(((unet_neighbourhood[i].NeighborRSSI * 7) + unet_neighbor_ping.NeighborRSSI)>> 3);
          }
          
          unet_neighbourhood[i].NeighborLQI  = unet_neighbor_ping.NeighborLQI;
          unet_neighbourhood[i].NeighborDepth = unet_neighbor_ping.NeighborDepth;
          unet_neighbourhood[i].NeighborStatus.bits.Symmetric = FALSE;
          
          // Varre os endereços de vizinhos do ping
          for(j=0;j<unet_neighbor_ping.NeighborsNumber;j++)
          {
            // Se o nó encontra seu endereço nesta lista
            if (unet_neighbor_ping.Neighbors[j] == macAddr)
            {
#if (defined CHECK_DUPLICATE_MAC) && (CHECK_DUPLICATE_MAC == 1)              
              if(++foundme == 2){ // MAC duplicado ?
                 macAddr = (INT16U)(macAddr + RadioRand()); //pequena mudança no MAC address
                 foundme = 0; 
              }
#endif              
              // Verifica se o RSSI de seu sinal e do vizinho estão acima do threshold minimo
              if ((unet_neighbor_ping.NeighborsRSSI[j] >= RSSI_THRESHOLD) && (unet_neighbourhood[i].NeighborRSSI >= RSSI_THRESHOLD))
              {              
                // Se sim, o nó é simetrico
                unet_neighbourhood[i].NeighborStatus.bits.Symmetric = TRUE;
              }
                          
              // não precisa continuar a varredura se encontrou o id na tabela
              // break; // removido para poder verificar MAC address duplicado e MW
            }
          }          
          
          // Substitui update basestation
          UpdateDepth();
          
          // Informa atividade do nó
          NeighborTable = (NEIGHBOR_TABLE_T)(NeighborTable | (NEIGHBOR_TABLE_T)(0x01 << i));          
      }
}




// Monta pacote de comando NWK
// o payload do pacote será copiado do vetor NWKPayload
void NWK_Command(INT16U Address, INT8U r_parameter, INT8U payload_size, INT8U packet_life, INT16U destiny)
{
  INT8U i = 0;
  INT8U FrameIndex = 0;
  INT8U HeaderSize = 0;
  INT8U PayloadSize = 0;
  INT8U tmp = 0;
                            
  // Inicia montagem do pacote NWK Command
  // Inicia montagem do pacote Data p/ roteamento
  
  // Indicação de Beacon no Frame Control
  PHYSetLongRAMAddr(2, 0x61);
  HeaderSize++;
  
  // Indicação de Dest e Source Address de 16b, no Frame Control
  PHYSetLongRAMAddr(3, 0x88);
  HeaderSize++;
 
  
  // Sequence Number
  UserEnterCritical();
  tmp = SequenceNumber;
  UserExitCritical();
  
  PHYSetLongRAMAddr(4, tmp);  
  
  HeaderSize++;
  
  // PanId do coordenador que gera o data packet
  PHYSetLongRAMAddr(5, (INT8U)(macPANId & 0xFF));
  PHYSetLongRAMAddr(6, (INT8U)(macPANId >> 8));
  
  // Endereço de destino do data packet
  PHYSetLongRAMAddr(7, (INT8U)(Address & 0xFF));
  PHYSetLongRAMAddr(8, (INT8U)(Address >> 8));
  
  // Não precisa PANId da fonte pq
  // o pacote é intrapan
      
  // Endereço fonte do data packet
  PHYSetLongRAMAddr(9, (INT8U)(macAddr & 0xFF));
  PHYSetLongRAMAddr(10, (INT8U)(macAddr >> 8));
  HeaderSize += 6;
  FrameIndex = 11;
  
  // Tipo de pacote de dados
  PHYSetLongRAMAddr(FrameIndex, ROUTE_PACKET);
  FrameIndex++;
  PayloadSize++;
  
  // Adiciona os parametros de roteamento
  // Sentido de transmissão
  // Verificação se é o destino do pacote
  // outros
  PHYSetLongRAMAddr(FrameIndex, r_parameter);
  FrameIndex++;
  PayloadSize++;  
                        
  // Adiciona endereço de rede (localização GPS)  
  if (packet_life == 0)
  {
    // Endereço mac do destino final
	PHYSetLongRAMAddr(FrameIndex++, (INT8U)(destiny & 0xFF));
	PHYSetLongRAMAddr(FrameIndex++, (INT8U)(destiny >> 8));
    
    // Endereço mac do nó fonte
	PHYSetLongRAMAddr(FrameIndex++, (INT8U)(macAddr & 0xFF));
	PHYSetLongRAMAddr(FrameIndex++, (INT8U)(macAddr >> 8));
  }else
  {
    // Copia os endereços de rede de destino e fonte
        
    // Copia os endereços de rede (endereço do nó de destino final)
    tmp = (INT8U)(nwk_packet.NWK_Destiny & 0xFF);
    PHYSetLongRAMAddr(FrameIndex++, (INT8U)(tmp));
    tmp = (INT8U)(nwk_packet.NWK_Destiny >> 8);
    PHYSetLongRAMAddr(FrameIndex++, (INT8U)(tmp));
    
    // Copia os endereços de rede (endereço do nó fonte)
    tmp = (INT8U)(nwk_packet.NWK_Source & 0xFF);
    PHYSetLongRAMAddr(FrameIndex++, (INT8U)(tmp));
    tmp = (INT8U)(nwk_packet.NWK_Source >> 8);
    PHYSetLongRAMAddr(FrameIndex++, (INT8U)(tmp));
  }
  
  PayloadSize +=4;
  
  // Adiciona o tempo de vida do pacote
  // ou seja, numero de saltos
  PHYSetLongRAMAddr(FrameIndex++, packet_life);
  PayloadSize++;
  
  if (packet_life == 0)
  {
    for(i=0;i<payload_size;i++)
    {
      PHYSetLongRAMAddr(FrameIndex++, NWKPayload[i]);
      PayloadSize++;
    }  
    
      /* keep stats */
      IncUNET_NodeStat_apptxed();
        
  }else
  {
    // Copia o payload do pacote anterior
    for(i=0;i<payload_size;i++)
    {
      // Copia somente o payload da mensagem
      // Cabeçalho MAC + Rede           = 11 bytes
      // Endereços de Rede  			= 4 bytes
      // Tempo de vida do pacote        =  1 byte
      // Total                          = 16 bytes
      tmp = nwk_packet.NWK_Payload[i];
      PHYSetLongRAMAddr(FrameIndex++, (INT8U)(tmp));
      PayloadSize++;
    }    
  }

  // Informação do tamanho do MAC header em bytes
  // No modo não seguro é ignorado
  PHYSetLongRAMAddr(0x000,HeaderSize);
  
  // Informação do tamanho em bytes do MAC header + Payload
  PHYSetLongRAMAddr(0x001,(INT8U)(HeaderSize+PayloadSize));

  // transmit packet with ACK requested
  // Para solicitar ACK, bit2 = 1
  mac_tasks_pending.bits.PacketPendingAck = 1;
  PHYSetShortRAMAddr(WRITE_TXNMTRIG,0b00000101);
}


INT8U HandleRoutePacket(void)
{
  INT8U i = 0;
  INT8U match_count = 0;
  INT8U semaphore_return = 0;
  INT8U attempts = 0;
  INT8U state = 0;  
  INT8U nwk_state ; /* Variável utilizada na máquina de estados de roteamento */
  
  // Verifica se o destino existe na tabela de vizinhos   
  // Inicia máquina de estados para decodificar pacote e realizar roteamento
  nwk_state = start_route;
  
  while(nwk_state != end_route)
  {  
    switch(nwk_state)
    {
      case start_route:
        // Verifica o tempo de vida do pacote
        // Se maior que nwkMaxDepth saltos, descarta o pacote
        if ((INT8U)(nwk_packet.NWK_Packet_Life+1) > nwkMaxDepth)
        {
          nwk_state = end_route;
          state = PACKET_LIFE_ERROR;          
        }else
        {

          #if (USE_REACTIVE_UP_ROUTE == 1)
        	if ((nwk_packet.NWK_Parameter&NWK_DIRECTION) == NOT_DEST_DOWN){
				// ********************************************************************************
				// Guarda a informação de rota do nó que passou por este roteador no sentido para o roteador (down)
				// Verifica se ja existe este endereço na tabela
				for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
				{
				  match_count = 0;

				  // match count = 8 para manter compatibilidade
				  // Procura se o nó fonte do pacote já está na up routing table
				  if (nwk_packet.NWK_Source == unet_routing_up_table[i].DestinyAddr) match_count = 8;

				  if (match_count == 8)
				  {
					// Para o laço "for" se encontrar o endereço na lista de vizinhos
					// E faz com que a maquina de estados repasse o pacote
					// ao seu destino
					break;
				  }
				}

				// Se não está na tabela
				if (match_count != 8)
				{
					// Procura posição vazia
					for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
					{
					  if (unet_routing_up_table[i].Addr_16b == 0xFFFE)
					  {
						match_count = 8;
						break;
					  }
					}

				}

				// Se existe posição na tabela ou estiver atualizando a posição
				if (match_count == 8)
				{
				  unet_routing_up_table[i].Addr_16b = mac_packet.SrcAddr_16b;
				  if (nwk_packet.NWK_Packet_Life == 0)
				  {
					unet_routing_up_table[i].Destination = TRUE;
				  }else
				  {
					unet_routing_up_table[i].Destination = FALSE;
				  }
				  // Copia o endereço do nó de origem do pacote para a lista de rotas up disponíveis
				  unet_routing_up_table[i].DestinyAddr = nwk_packet.NWK_Source;
				  unet_routing_up_table[i].hops 	   = nwk_packet.NWK_Packet_Life + 1;
				  unet_routing_up_table[i].activity	   = TRUE;

				}
        	}
          #endif
          
          // ********************************************************************************          
          
          // Informa atividade do nó
          for(i=0;i<NEIGHBOURHOOD_SIZE;i++) 
          {            
            if (mac_packet.SrcAddr_16b == unet_neighbourhood[i].Addr_16b)
            {
                NeighborTable = (NEIGHBOR_TABLE_T)(NeighborTable | (NEIGHBOR_TABLE_T)(0x01 << i));
                unet_neighbourhood[i].NeighborStatus.bits.Symmetric = TRUE;
                break;
            }
          }
          
          // Verifica se o nó é o destino do pacote
          if(nwk_packet.NWK_Packet_Type == BROADCAST_PACKET || nwk_packet.NWK_Parameter&NWK_BROADCAST){
            nwk_state = broadcast;
          }else{
            if ((nwk_packet.NWK_Parameter&NWK_DEST) == NWK_DEST)
            {
              nwk_state = call_app_layer;
            } else
            {
              nwk_state = neighbor_table_search;
            }
          }
        }
                
        break;
      case broadcast:
          // repassa pacote
          UpBroadcastRoute((INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD));
          // depois passa uma cópia para a camada de aplicação
          nwk_state = call_app_layer;
      break;
      
      case neighbor_table_search:
        // Verifica se o endereço de destino pertence a um vizinho
        for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
        {
          match_count = 0;

          if (unet_neighbourhood[i].NeighborStatus.bits.Symmetric == TRUE)
          {          
              if (nwk_packet.NWK_Destiny == unet_neighbourhood[i].Addr_16b){
            	  match_count = 8;
            	  break;
              }
          }
        }
        
        if (match_count == 8)
        {
          // Encontrou o destino na lista de vizinhos
          match_count = i;
          attempts = 0;
          nwk_state = send_dest_packet;
        }else
        {
          // Continua o processo de roteamento
          if ((nwk_packet.NWK_Parameter&NWK_DIRECTION) == NOT_DEST_UP)
          {
            nwk_state = route_up;
          }else
          {
            nwk_state = route_down;
          }
        }
        break;

      case route_up:
        // Realiza o roteamento no sentido contrário ao dos coordenadores
        #if (USE_REACTIVE_UP_ROUTE == 1)
        state = ReactiveUpRoute(IN_PROGRESS_ROUTE,0,0);
        #else
        //state = UpRoute(IN_PROGRESS_ROUTE,0);
        state = 0;
        #endif
        nwk_state = end_route;
        break;

      case route_down:
        // Realiza o roteamento no sentido do pan coordinator
        // Roteamento por Node Depth
        #if (DEVICE_TYPE != PAN_COORDINATOR)        
          state = DownRoute(IN_PROGRESS_ROUTE,0);
        #endif
        
        nwk_state = end_route;
        break;
                
      case send_dest_packet:
        // Envia o pacote para o seu destino
        attempts = 0;
        while (attempts < NWK_TX_RETRIES)
        {
          if (attempts < (NWK_TX_RETRIES-1))
          {
        	if ((nwk_packet.NWK_Parameter&NWK_DIRECTION) == NWK_DIRECTION){
        		NWK_Command(unet_neighbourhood[match_count].Addr_16b, DEST_DOWN, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1),0);
        	}else{
        		NWK_Command(unet_neighbourhood[match_count].Addr_16b, DEST_UP, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1),0);
        	}
            semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));
            
            if (semaphore_return == OK)
            {
              if (macACK == TRUE) 
              {
                unet_neighbourhood[match_count].NeighborStatus.bits.Symmetric = TRUE;
                
                nwk_state = end_route;
                state = OK;
                // Sai do laço while
                break;
              }else 
              {
                attempts++;
                // Espera tempo de bursting error
                DelayTask((INT16U)(RadioRand()*30));
              }
            } else
            {              
              // Radio provavelmente travou, efetuar o reset
              attempts++;

              //  Disable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
              
              MRF24J40Reset();

              //  Enable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x00);    
            }
          }else
          {
            nwk_state = end_route;
            state = ROUTE_ATTEMPTS_ERROR;
            break;
          }
        }
        
        // Increments Packet Sequence ID
        // Used to identify replicated packets
        UserEnterCritical();
        if (++SequenceNumber == 0) SequenceNumber = 1;
        UserExitCritical();        
        
        break;
        
      case call_app_layer:
        // Acorda a tarefa de aplicação e termina o processo de roteamento
        UNET_APP();
        nwk_state = end_route;
        state = OK;
        break;

      default:
        // Sai sem fazer nada
        // Erro de formato do pacote
        nwk_state = end_route;
        state = ROUTE_FRAME_ERROR;
        break;                    
    }
  }
  
  return state;
  
}


// Realiza o roteamento no sentido do PAN Coordinator "NearBase"
INT8U DownRoute(INT8U RouteInit, INT8U NWKPayloadSize)
{

  INT8U   i = 0;
  INT8U   selected_node = 0;
  INT8U   attempts = 0;
  INT8U   semaphore_return = 0;
  INT8U   MinorDepth = 255;
  INT8U   MaxRSSI = 0;
  INT16U  BlackList = 0;
  
  
  if (NWKPayloadSize > MAX_APP_PAYLOAD_SIZE){   
       return PAYLOAD_OVERFLOW;
  }
  
  if (thisNodeDepth >= ROUTE_TO_BASESTATION_LOST){
      return NO_ROUTE_AVAILABLE;
  }

  // Se está enviando uma mensagem no sentido do coordenador
  // não precisa de mensagem de manutenção de rota up
  UserEnterCritical();
  ReactiveUpCnt = 0;
  UserExitCritical();

  // Encontra a menor profundidade na tabela de vizinhos
  TryAnotherNodeDown:
   
    MinorDepth = 255;
    MaxRSSI = 0;
    selected_node = 0;
    
    // Varrendo a tabela de vizinhos em busca do nó simétrico de menor profundidade
    for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
    {
      // Sempre que a profundidade do nó for menor que a atualmente selecionada,
      // escolhe o nó como próximo valor de menor profundidade
      // <= devido a um dos nós de mesma profundidade estar na black list
      if ((unet_neighbourhood[i].NeighborDepth <= MinorDepth) && (unet_neighbourhood[i].NeighborStatus.bits.Symmetric == TRUE))
      {
        // O nó escolhido não deve ter profundidade superior a do nó que está roteando
        if (unet_neighbourhood[i].NeighborDepth <= thisNodeDepth)
        {
          // Verifica se o nó selecionado não está na Black List
          // Se não está na black list, é o próximo nó de menor profundidade
          if ((BlackList & (1 << i)) == 0) 
          {  
            if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
            {
              // Verifica se a profundidade diminuiu
              // para procurar pelo melhor RSSI nesta profundidade
              if (unet_neighbourhood[i].NeighborDepth < MinorDepth)
              {
                MaxRSSI = 0;
              }              
              
              if (unet_neighbourhood[i].NeighborRSSI >= MaxRSSI)
              {
                selected_node = i;
                MinorDepth = unet_neighbourhood[i].NeighborDepth;
                MaxRSSI = unet_neighbourhood[i].NeighborRSSI;
              }
            }
          }
        }
      }
    }
    
    // Se não selecionou um nó simétrico para tentativa de roteamento
    if (MinorDepth == 255)
    {
      MaxRSSI = 0;
      selected_node = 0;      
      // Se ainda existe conexão com uma estação base
      if (thisNodeDepth < ROUTE_TO_BASESTATION_LOST)
      {
        // Varrendo a tabela de vizinhos em busca do nó de menor profundidade
        for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
        {
          // Sempre que a profundidade do nó for menor que a atualmente selecionada,
          // escolhe o nó como próximo valor de menor profundidade
          // <= devido a um dos nós de mesma profundidade estar na black list
          if (unet_neighbourhood[i].NeighborDepth <= MinorDepth)
          {
            // O nó escolhido não deve ter profundidade superior a do nó que está roteando
            if (unet_neighbourhood[i].NeighborDepth <= thisNodeDepth)
            {
              // Verifica se o nó selecionado não está na Black List
              // Se não está na black list, é o próximo nó de menor profundidade
              if ((BlackList & (1 << i)) == 0) 
              {  
                if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
                {
                  // Verifica se a profundidade diminuiu
                  // para procurar pelo melhor RSSI nesta profundidade
                  if (unet_neighbourhood[i].NeighborDepth < MinorDepth)
                  {
                    MaxRSSI = 0;
                  }              
                  
                  if (unet_neighbourhood[i].NeighborRSSI >= MaxRSSI)
                  {
                    selected_node = i;
                    MinorDepth = unet_neighbourhood[i].NeighborDepth;
                    MaxRSSI = unet_neighbourhood[i].NeighborRSSI;
                  }
                }
              }
            }
          }
        }      
      }
    }
    
    // Se selecionou um nó para tentativa de roteamento
    if (MinorDepth != 255)
    {      
      attempts = 0;      
      // Tenta rotear o pacote 3 vezes
      while (attempts < NWK_TX_RETRIES)
      {
        if (attempts < (NWK_TX_RETRIES-1))
        {
          // Envia pacote a ser roteado
          if (RouteInit != START_ROUTE)
          {
            if (MinorDepth == 0)              
              NWK_Command(unet_neighbourhood[selected_node].Addr_16b, DEST_DOWN, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1),0);
            else
              NWK_Command(unet_neighbourhood[selected_node].Addr_16b, NOT_DEST_DOWN, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1),0);
          }else
          {
            if (MinorDepth == 0)
              NWK_Command(unet_neighbourhood[selected_node].Addr_16b, DEST_DOWN, NWKPayloadSize,0,0);
            else
              NWK_Command(unet_neighbourhood[selected_node].Addr_16b, NOT_DEST_DOWN, NWKPayloadSize,0,0);
          }
          // Espera confirmação de recepção
          semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));
          
          // Se foi recebido, ok
          if (semaphore_return == OK)
          {
            if (macACK == TRUE) 
            {
              i = OK;
              
              // Informa atividade do nó
              NeighborTable = (NEIGHBOR_TABLE_T)(NeighborTable | (NEIGHBOR_TABLE_T)(0x01 << selected_node));
              unet_neighbourhood[selected_node].NeighborStatus.bits.Symmetric = TRUE;
              
              // Sai do laço while
              break;
            }else 
            {
              attempts++;
              // Espera tempo de bursting error
              DelayTask((INT16U)(RadioRand()*30));
            }
          } else
          {
              // Radio provavelmente travou, efetuar o reset
              attempts++;

              //  Disable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
              
              MRF24J40Reset();

              //  Enable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
          }
        }else
        {
          // Se estorou o número de tentativas, desiste de rotear por este nó
          i = ROUTE_NODE_ERROR;
          BlackList = (INT16U)(BlackList | (1 << selected_node));
          goto TryAnotherNodeDown;
        }
      }            
    }else
    {
      i = ROUTE_ATTEMPTS_ERROR;
    }

    // Increments Packet Sequence ID
    // Used to identify replicated packets
    UserEnterCritical();
    if (++SequenceNumber == 0) SequenceNumber = 1;
    UserExitCritical();
    
    return i;
}

// Envia mensagens para todos os nós com maior profundidade a 1 salto de distância
INT8U UpSimpleRoute(INT8U NWKPayloadSize)
{
  INT8U i = 0;
  INT8U attempts = 0;
  INT8U semaphore_return = 0;
    
  // Varrendo a tabela de vizinhos em busca de nós com maior profundidade
  for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
  {
    if(unet_neighbourhood[i].NeighborDepth == (thisNodeDepth + 1))
    {
      attempts = 0;
      
      // Tenta rotear o pacote NWK_TX_RETRIES vezes
      while (attempts <= NWK_TX_RETRIES)
      {
          // Envia pacote a ser roteado
          NWK_Command(unet_neighbourhood[i].Addr_16b, DEST_UP, NWKPayloadSize,0,unet_neighbourhood[i].Addr_16b);
          // Espera confirmação de recepção
          semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));
          
          // Se foi recebido, ok
          if (semaphore_return == OK)
          {
            if (macACK == TRUE) 
            {
              i = OK;
              unet_neighbourhood[i].NeighborStatus.bits.Symmetric = TRUE;
              // Sai do laço while
              break;
            }else 
            {
              attempts++;
              // Espera tempo de bursting error
              DelayTask((INT16U)(RadioRand()*30));              
            }
          } else
          {
              // Radio provavelmente travou, efetuar o reset
              attempts++;

              //  Disable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
              
              MRF24J40Reset();

              //  Enable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
          }
      }
      // Increments Packet Sequence ID
      // Used to identify replicated packets
      UserEnterCritical();
      if (++SequenceNumber == 0) SequenceNumber = 1;
      UserExitCritical();
    }
    
    return i;
  }

  return i;

}


// Envia mensagens para todos os nós com maior profundidade a 1 salto de distância
INT8U UpBroadcastRoute(INT8U NWKPayloadSize)
{
  INT8U i = 0;
  INT8U attempts = 0;
  INT8U semaphore_return = 0;
  INT8U ret = ROUTE_ATTEMPTS_ERROR;
  
    
  // Varrendo a tabela de vizinhos em busca de nós com maior profundidade
  for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
  {
  
    if(unet_neighbourhood[i].NeighborDepth == (thisNodeDepth + 1))
    {
      attempts = 0;
      
      // Tenta rotear o pacote NWK_TX_RETRIES vezes
      while (attempts <= NWK_TX_RETRIES)
      {
          // Envia pacote a ser roteado
          NWK_Command(unet_neighbourhood[i].Addr_16b, NWK_BROADCAST, NWKPayloadSize,0,0xFFFF);
          
          // Espera confirmação de recepção
          semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));
          
          // Se foi recebido, ok
          if (semaphore_return == OK)
          {
            if (macACK == TRUE) 
            {
              ret = OK; 
              unet_neighbourhood[i].NeighborStatus.bits.Symmetric = TRUE;
              break;  // Sai do laço while
            }else 
            {
              attempts++;
              // Espera tempo de bursting error
              DelayTask((INT16U)(RadioRand()*30));              
            }
          } else
          {
              // Radio provavelmente travou, efetuar o reset
              attempts++;

              //  Disable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
              
              MRF24J40Reset();

              //  Enable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
          }
      }
      // Increments Packet Sequence ID
      // Used to identify replicated packets
      UserEnterCritical();
      if (++SequenceNumber == 0) SequenceNumber = 1;
      UserExitCritical();
    }
    
    return ret;
  }

  return ret;
}

#if (USE_REACTIVE_UP_ROUTE == 1)
  // Realiza o roteamento no sentido reverso ao PAN Coordinator
  INT8U ReactiveUpRoute(INT8U RouteInit, INT8U NWKPayloadSize, INT16U destiny)
  {
    INT8U i = 0;
    INT8U j = 0;
    INT8U attempts = 0;
    INT8U match_count = 0;
    INT8U semaphore_return = 0;


    if (RouteInit == IN_PROGRESS_ROUTE) 
    {
      for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
      {
        match_count = 0;

        // Se achar o destino na tabela de rotas up
        if (nwk_packet.NWK_Destiny == unet_routing_up_table[i].DestinyAddr) match_count = 8;
        
        if (match_count == 8) 
        {
          // Para o laço "for" se encontrar o endereço na lista de vizinhos
          // E faz com que a maquina de estados repasse o pacote
          // ao seu destino
          break;
        }
      }  
    }else
    {
      for(i=0;i<ROUTING_UP_TABLE_SIZE;i++)
      {
        match_count = 0;

        if (unet_routing_up_table[i].DestinyAddr == destiny) match_count = 8;
        
        if (match_count == 8) 
        {
          // Para o laço "for" se encontrar o endereço na lista de vizinhos
          // E faz com que a maquina de estados repasse o pacote
          // ao seu destino
          break;
        }
      }
    }
      
    
    if (match_count == 8) 
    {
        attempts = 0;
        
        // Tenta rotear o pacote NWK_TX_RETRIES vezes
        while (attempts < NWK_TX_RETRIES)
        {
          if (attempts < (NWK_TX_RETRIES-1))
          {
            // Envia pacote a ser roteado
            
            // Analisa se é o destino do pacote
            // Envia pacote a ser roteado
            if (RouteInit == IN_PROGRESS_ROUTE)
            {
              if (unet_routing_up_table[i].Destination == TRUE)
                NWK_Command(unet_routing_up_table[i].Addr_16b, DEST_UP, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1), 0);
              else
                NWK_Command(unet_routing_up_table[i].Addr_16b, NOT_DEST_UP, (INT8U)(mac_packet.Payload_Size - NWK_OVERHEAD),(INT8U)(nwk_packet.NWK_Packet_Life+1), 0);
            }else
            {
              if (unet_routing_up_table[i].Destination == TRUE)
                NWK_Command(unet_routing_up_table[i].Addr_16b, DEST_UP, NWKPayloadSize,0, destiny);
              else
                NWK_Command(unet_routing_up_table[i].Addr_16b, NOT_DEST_UP, NWKPayloadSize,0, destiny);
            }
            // Espera confirmação de recepção
            semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));

            // Se foi recebido, ok
            if (semaphore_return == OK)
            {
              if (macACK == TRUE) 
              {
                i = OK;
                // Sai do laço while
                break;
              }else 
              {
                attempts++;
                // Espera tempo de bursting error
                DelayTask((INT16U)(RadioRand()*30));                              
              }
            } else
            {
              // Radio provavelmente travou, efetuar o reset
              attempts++;

              //  Disable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
              
              MRF24J40Reset();

              //  Enable receiving packets off air
              PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
            }
          }else
          {
            // Se estourou o número de tentativas, desiste de rotear por este nó
            j = ROUTE_NODE_ERROR;
            break;
          }
        }
        
        // Increments Packet Sequence ID
        // Used to identify replicated packets
        UserEnterCritical();
        if (++SequenceNumber == 0) SequenceNumber = 1;
        UserExitCritical();      
    }else
    {
      j = NO_ROUTE_AVAILABLE;
    }
      
    return j;
  }
#endif

INT8U OneHopRoute(INT8U NWKPayloadSize, INT16U destiny){

    INT8U       i = 0;
    INT8U       attempts = 0;  
    INT8U       semaphore_return = 0;
    INT8U       selected_node = 0;
    INT8U       match_count = 0;       
        
    ////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////
    ///// Procura por destino na tabela de vizinhos         ////
    ////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////// 
      for(i=0;i<NEIGHBOURHOOD_SIZE;i++)
      {
        match_count = 0;

        if (destiny == unet_neighbourhood[i].Addr_16b)
          match_count++;

        if (match_count)
        {
          // Para o laço "for" se encontrar o endereço na lista de vizinhos
          // E faz com que a maquina de estados repasse o pacote
          // ao seu destino
          if (unet_neighbourhood[i].NeighborStatus.bits.Symmetric == TRUE)
          {
            selected_node = i;
            break;
          }
        }
      }        
    
      if (!match_count)   return NO_ROUTE_AVAILABLE;
    
      // Tenta transmitir o pacote NWK_TX_RETRIES vezes
      while (attempts < NWK_TX_RETRIES)
      {
        if (attempts < (NWK_TX_RETRIES-1))
        {
          // Envia pacote  
          NWK_Command(unet_neighbourhood[selected_node].Addr_16b, DEST_UP, NWKPayloadSize,0, destiny);

          // Espera confirmação de recepção
          semaphore_return = OSSemPend(RF_TX_Event,(INT16U)(TX_TIMEOUT+RadioRand()));
          
          // Se foi recebido, ok
          if (semaphore_return == OK)
          {
            if (macACK == TRUE) 
            {
              i = OK;
              
              // Informa atividade do nó
              NeighborTable = (NEIGHBOR_TABLE_T)(NeighborTable | (NEIGHBOR_TABLE_T)(0x01 << selected_node));                
              unet_neighbourhood[i].NeighborStatus.bits.Symmetric = TRUE;
              // Sai do laço while
              break;
            }else 
            {
              attempts++;
              // Espera tempo de bursting error
              DelayTask((INT16U)(RadioRand()*30));                
            }
          } else
          {
            // Radio provavelmente travou, efetuar o reset
            attempts++;

            //  Disable receiving packets off air
            PHYSetShortRAMAddr(WRITE_BBREG1,0x04);              
            
            MRF24J40Reset();

            //  Enable receiving packets off air
            PHYSetShortRAMAddr(WRITE_BBREG1,0x00);
          }
        }else
        {
          i = NO_ROUTE_AVAILABLE;
          break;
        }
      }                

      
    // Increments Packet Sequence ID
    // Used to identify replicated packets
    UserEnterCritical();
    if (++SequenceNumber == 0) SequenceNumber = 1;
    UserExitCritical();      
    
    return i;
  
}

#if (USE_REACTIVE_UP_ROUTE == 1)
INT8U ReactiveUpMessage(void)
{
  INT8U j = 0;
  INT8U status = 0;

  // Não executa nada na camada de aplicação
  NWKPayload[j++] = 0xFF;

  status = DownRoute(START_ROUTE,(INT8U)(j));

  return status;
}
#endif



