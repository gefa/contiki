/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
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
 *
 * -----------------------------------------------------------------
 *
 * Author  : Adam Dunkels, Joakim Eriksson, Niclas Finne
 * Created : 2006-03-07
 * Updated : $Date: 2010/01/15 10:32:36 $
 *           $Revision: 1.6 $
 *
 * Simple application to indicate connectivity between two nodes:
 *
 * - Red led indicates a packet sent via radio (one packet sent each second)
 * - Green led indicates that this node can hear the other node but not
 *   necessary vice versa (unidirectional communication).
 * - Blue led indicates that both nodes can communicate with each
 *   other (bidirectional communication)
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "cc2420.h"
#include <stdio.h>
#include <string.h>

PROCESS(radio_test_process, "Radio test");
AUTOSTART_PROCESSES(&radio_test_process);

#define ON  1
#define OFF 0

#define HEADER "RTST"
#define PACKET_SIZE 6
#define PORT 9345
#define CLOCK 8
#define PACKETS_PER_SECOND (CLOCK_SECOND/CLOCK)

struct indicator {
  int onoff;
  int led;
  clock_time_t interval;
  struct etimer timer;
};
static int recv_count = 0;
static int other_count = 0;
static int packet_slots = 0;
static struct etimer send_timer;
static struct indicator recv, other, flash;

/*---------------------------------------------------------------------*/
static void
set(struct indicator *indicator, int onoff) {
  if(indicator->onoff ^ onoff) {
    indicator->onoff = onoff;
    if(onoff) {
      leds_on(indicator->led);
    } else {
      leds_off(indicator->led);
    }
  }
  if(onoff) {
    etimer_set(&indicator->timer, indicator->interval);
  }
}
/*---------------------------------------------------------------------------*/
static void
abc_recv(struct abc_conn *c)
{
  /* packet received */
  if(packetbuf_datalen() < PACKET_SIZE
     || strncmp((char *)packetbuf_dataptr(), HEADER, sizeof(HEADER))) {
    /* invalid message */

  } else {
    PROCESS_CONTEXT_BEGIN(&radio_test_process);
    set(&recv, ON);
    recv_count++;
    if (((char *)packetbuf_dataptr())[sizeof(HEADER)] ? ON : OFF) {
      set(&other, ON);
      other_count++;
    } else {
      set(&other, OFF);
    }
    /* synchronize the sending to keep the nodes from sending
       simultaneously */

    etimer_set(&send_timer, CLOCK);
    etimer_adjust(&send_timer, - (int) (CLOCK >> 1));
    PROCESS_CONTEXT_END(&radio_test_process);
  }
}
static const struct abc_callbacks abc_call = {};//{abc_recv};
static struct abc_conn abc;
/*---------------------------------------------------------------------*/
PROCESS_THREAD(radio_test_process, ev, data)
{
  static uint8_t txpower;
  PROCESS_BEGIN();

  txpower = CC2420_TXPOWER_MAX;

  /* Initialize the indicators */
  recv.onoff = other.onoff = flash.onoff = OFF;
  recv.interval = other.interval = CLOCK;
  flash.interval = 1;
  flash.led = LEDS_RED;
  recv.led = LEDS_GREEN;
  other.led = LEDS_BLUE;

  abc_open(&abc, PORT, &abc_call);
  etimer_set(&send_timer, CLOCK);
  SENSORS_ACTIVATE(button_sensor);

  while(1) {
    PROCESS_WAIT_EVENT();
    if (ev == PROCESS_EVENT_TIMER) {
      if(data == &send_timer) {
	etimer_reset(&send_timer);

	/* send packet */
	packetbuf_copyfrom(HEADER, sizeof(HEADER));
	((char *)packetbuf_dataptr())[sizeof(HEADER)] = recv.onoff;
	/* send arbitrary data to fill the packet size */
	packetbuf_set_datalen(PACKET_SIZE);
	set(&flash, ON);
	abc_send(&abc);
  packetbuf_copyfrom(HEADER, sizeof(HEADER));
  ((char *)packetbuf_dataptr())[sizeof(HEADER)] = recv.onoff;
  /* send arbitrary data to fill the packet size */
  packetbuf_set_datalen(PACKET_SIZE);
  set(&flash, ON);
  abc_send(&abc);
  //if(recv.onoff) printf("G\n");
  //if(other.onoff) printf("B\n");

      } /*else if(data == &other.timer) {
	set(&other, OFF);

      } else if(data == &recv.timer) {
	set(&recv, OFF);

      } else if(data == &flash.timer) {
	set(&flash, OFF);
      }*/
    } else if(ev == sensors_event && data == &button_sensor) {
      if(txpower > 5) {
	txpower -= 5;
      } else {
	txpower = CC2420_TXPOWER_MAX;
	leds_blink();
      }
      cc2420_set_txpower(txpower);
      printf("txpower set to %u\n", txpower);
    }
    //printf("data len %d\n", sizeof(data));
    //printf("%x\n", data);
    //printf("CLOCK %d\n", CLOCK);
    //if(flash.onoff) printf("R\n");

    // packet_slots++;
    // if (packet_slots >= PACKETS_PER_SECOND*4) {
    //   printf("r=%d o=%d t=%d\n", \
    //     recv_count, other_count, PACKETS_PER_SECOND);
    //   int i;
    //   for(i=0;i<recv_count;++i){
    //     if(i==PACKETS_PER_SECOND){
    //       printf("###|");
    //     }else{
    //       printf("### ");
    //     }
    //   }
    //   printf("\n");
    //   packet_slots = recv_count = other_count = 0;
    // }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------*/
