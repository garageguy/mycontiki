/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file
 *         Example of how the collect primitive works.
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "dev/serial-line.h"
#include "dev/leds.h"
#include "collect-common.h"
#include "collect-view.h"
#include "net/mac/csma.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static unsigned long time_offset;
static int send_active = 1;

#ifndef GGPERIOD
#define GGPERIOD 100
#endif
#define DAG_CONSTRUCTION_DURATION 50
#define CLEANUP_DURATION 10

#define CBR			3
#define ROUNDS_NUM	2

/*---------------------------------------------------------------------------*/
PROCESS(collect_common_process, "collect common process");
AUTOSTART_PROCESSES(&collect_common_process);
/*---------------------------------------------------------------------------*/
static unsigned long
get_time(void)
{
  return clock_seconds() + time_offset;
}
/*---------------------------------------------------------------------------*/
static unsigned long
strtolong(const char *data) {
  unsigned long value = 0;
  int i;
  for(i = 0; i < 10 && isdigit(data[i]); i++) {
    value = value * 10 + data[i] - '0';
  }
  return value;
}
/*---------------------------------------------------------------------------*/
void
collect_common_set_send_active(int active)
{
  send_active = active;
}
/*---------------------------------------------------------------------------*/
void
collect_common_recv(const linkaddr_t *originator, uint8_t seqno, uint8_t hops,
                    uint8_t *payload, uint16_t payload_len)
{
  unsigned long time;
  uint16_t data;
  int i;

  printf("%u", 8 + payload_len / 2);
  /* Timestamp. Ignore time synch for now. */
  time = get_time();
  printf(" %lu %lu 0", ((time >> 16) & 0xffff), time & 0xffff);
  /* Ignore latency for now */
  printf(" %u %u %u %u",
         originator->u8[0] + (originator->u8[1] << 8), seqno, hops, 0);
  for(i = 0; i < payload_len / 2; i++) {
    memcpy(&data, payload, sizeof(data));
    payload += sizeof(data);
    printf(" %u", data);
  }
  printf("\n");
  leds_blink();

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(collect_common_process, ev, data)
{
  static struct etimer period_timer, wait_timer, duration_timer, cleanup_timer;
  static uint16_t round, cleanup_round=1; 
  static uint32_t gg_sending_interval=0, gg_saved_num_total_sent=0;
  uint32_t gg_cpu_e, gg_lpm_e, gg_tx_e, gg_rx_e, gg_total_e;  

  PROCESS_BEGIN();

  collect_common_net_init();
  /* Send a packet every 60-62 seconds. */
  etimer_set(&period_timer, DAG_CONSTRUCTION_DURATION * CLOCK_SECOND );
  etimer_set(&duration_timer, CLOCK_SECOND * GGPERIOD);
  while(round < ROUNDS_NUM) {
  	gg_sending_interval = (CLOCK_SECOND/8)<<(CBR);
    PROCESS_WAIT_EVENT();
    if(ev == serial_line_event_message) {
      char *line;
      line = (char *)data;
      if(strncmp(line, "collect", 7) == 0 ||
         strncmp(line, "gw", 2) == 0) {
        collect_common_set_sink();
      } else if(strncmp(line, "net", 3) == 0) {
        collect_common_net_print();
      } else if(strncmp(line, "time ", 5) == 0) {
        unsigned long tmp;
        line += 6;
        while(*line == ' ') {
          line++;
        }
        tmp = strtolong(line);
        time_offset = clock_seconds() - tmp;
        printf("Time offset set to %lu\n", time_offset);
      } else if(strncmp(line, "mac ", 4) == 0) {
        line +=4;
        while(*line == ' ') {
          line++;
        }
        if(*line == '0') {
          NETSTACK_RDC.off(1);
          printf("mac: turned MAC off (keeping radio on): %s\n",
                 NETSTACK_RDC.name);
        } else {
          NETSTACK_RDC.on();
          printf("mac: turned MAC on: %s\n", NETSTACK_RDC.name);
        }

      } else if(strncmp(line, "~K", 2) == 0 ||
                strncmp(line, "killall", 7) == 0) {
        /* Ignore stop commands */
      } else {
        printf("unhandled command: %s\n", line);
      }
    }
    if(ev == PROCESS_EVENT_TIMER) {
      if(data == &period_timer) {
        //etimer_reset(&period_timer);
        etimer_set(&period_timer, gg_sending_interval);
        etimer_set(&wait_timer, 
			random_rand() % gg_sending_interval);//* RANDWAIT));

      } else if(data == &wait_timer) {
        if(send_active) {
          /* Time to send the data */
          collect_common_send();
        }
      } else if (data == &duration_timer){
	  
      		gg_cpu_e = gg_total_cpu / 32768* 1800L ;
			gg_lpm_e = gg_total_lpm / 32768* 5L ;
			gg_tx_e = gg_total_transmit / 32768* 19500L  ;
			gg_rx_e = gg_total_listen / 32768* 21800L  ;
			gg_total_e = gg_cpu_e + gg_lpm_e + gg_tx_e + gg_rx_e;
            printf("GUOGE--stats: %u %lu %lu %lu %lu\n", 
				round + 1,
		  		//gg_num_total_sent,
		  		gg_num_udp_sent,
		  		//gg_num_successfully_transmitted,
		  		gg_num_dropped_buffer_overflow, 
				gg_num_dropped_channel_loss,
				//gg_max_num_neighbour_queue,
				//gg_sending_interval
				//gg_total_cpu, gg_total_lpm, gg_total_transmit, gg_total_listen,
				//gg_cpu_e, gg_lpm_e, gg_tx_e, gg_rx_e, 
				gg_total_e
				);  
			//printf("GUOGE--energy consumption, CPU:%lu, LPM:%lu, TX:%lu, LI:%lu\n",				gg_total_cpu, gg_total_lpm, gg_total_transmit, gg_total_listen);
			print_average_delay_of_nodes();

			gg_saved_num_total_sent = gg_num_total_sent;
			clear_queues();
			etimer_set(&cleanup_timer, CLOCK_SECOND * CLEANUP_DURATION);
      } else if (data == &cleanup_timer){
//      	printf("GUOGE--cleanup round:%u, num variance:%lu\n", 
//			cleanup_round, gg_num_total_sent - gg_saved_num_total_sent);
/*		if (gg_num_total_sent - gg_saved_num_total_sent > 10){
			cleanup_round++;
			clear_queues();
			etimer_set(&cleanup_timer, CLOCK_SECOND * CLEANUP_DURATION);
			gg_saved_num_total_sent = gg_num_total_sent;
			continue;
		}
*/
		gg_num_total_sent=0;
	  	gg_num_udp_sent=0;
	  	gg_num_successfully_transmitted=0;
	  	gg_num_dropped_buffer_overflow=0;
		gg_num_dropped_channel_loss=0;
		gg_max_num_neighbour_queue=0;
		gg_total_cpu = gg_total_lpm = gg_total_transmit = gg_total_listen = 0;
		gg_total_delay = gg_received_packets_num = 0;
		round++;
		cleanup_round = 1;
		etimer_set(&period_timer, (CLOCK_SECOND/8)<<(CBR));
		etimer_set(&duration_timer, (CLOCK_SECOND * GGPERIOD)<<round);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
