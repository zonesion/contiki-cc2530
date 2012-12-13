/*
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
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"
#include "dev/watchdog.h"
#include "dev/leds.h"
#include "net/rpl/rpl.h"
#include "dev/button-sensor.h"
#include "debug.h"
#include "dev/leds.h"
#include "sys/rtimer.h"
//#include "httpd.h"
/*---------------------------------------------------------------------------*/

PROCESS(tcp_server_process, "TCP server process"); 
PROCESS(mytimer_process, "Timer process");

AUTOSTART_PROCESSES(&tcp_server_process, &mytimer_process);

/*---------------------------------------------------------------------------*/
#ifndef WEBSERVER_CONF_CGI_CONNS
#define CONNS UIP_CONNS
#else /* WEBSERVER_CONF_CGI_CONNS */
#define CONNS WEBSERVER_CONF_CGI_CONNS
#endif /* WEBSERVER_CONF_CGI_CONNS */

typedef struct {
	struct psock sk;
	unsigned char timer;
	char inputbuf[64];	
}conn_t;
MEMB(conns, conn_t, CONNS);
 

#define STATE_WAITING 0
#define STATE_OUTPUT  1

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

static void
print_local_addresses(void);

PT_THREAD(handle_conn(conn_t *c))
{
	char *parm = NULL;
	PSOCK_BEGIN(&c->sk);
	PSOCK_READTO(&c->sk, ISO_space);
	if (memcmp(c->inputbuf, "GET", 3)) {
		PSOCK_CLOSE_EXIT(&c->sk);
	}
	PSOCK_READTO(&c->sk, ISO_space);
        c->inputbuf[PSOCK_DATALEN(&c->sk)-1] = 0;
	parm = strchr(c->inputbuf, '?');
        if (parm != NULL) {
	  char * p;
	  char st;
	    p = strstr(parm, 'LED=ON');
	   st = leds_get();
	   if (p != NULL) {
		// tun on led
		st |= 0x01;
	   }
	    p = strstr(parm, "LED=OFF");
	   if (p != NULL) {
		// tun off led
		st &= 0xFE;
	    } 
	    leds_off(0xff);
	    leds_on(st);
	}
	// send led ctrl page
	PSOCK_SEND_STR(&c->sk, "HTTP/1.1 200 OK\r\n");
	PSOCK_SEND_STR(&c->sk, "Content-Type: text/html\r\n\r\n");
	PSOCK_SEND_STR(&c->sk, "<html><head><title>LED Control -- Contiki Webserver</title></head>");
	PSOCK_SEND_STR(&c->sk, "<body>");
	PSOCK_SEND_STR(&c->sk, "<h1>LED Control Page</h1>");
	PSOCK_SEND_STR(&c->sk, "<p>LED State : ");
        PSOCK_SEND_STR(&c->sk, (leds_get()&0x01)?"ON":"OFF");
	PSOCK_SEND_STR(&c->sk, "</p>");
	PSOCK_SEND_STR(&c->sk, "<form method=\"GET\" name=\"led\">"
	"<input type=\"radio\" name=\"LED\" value=\"ON\">ON"
	"<input type=\"radio\" name=\"LED\" value=\"OFF\">OFF<br>"
	"<input type=\"submit\" value=\"submit\">");	
	PSOCK_SEND_STR(&c->sk, "</body>");
	PSOCK_CLOSE(&c->sk);
	PSOCK_END(&c->sk);
}

/*---------------------------------------------------------------------------*/
static void handle_connections(conn_t *c)
{
        handle_conn(c);	
}

void httpd_appcall(void *data)
{
	conn_t *s = (conn_t *)data;

	if (uip_aborted() || uip_closed() || uip_timedout()) {
		if (s != NULL) {
		 	memb_free(&conns, s);	
		} 
	} else if (uip_connected()) {
		s = (conn_t *)memb_alloc(&conns);
		if (s == NULL) {
			uip_abort();
			return;
		}
		((uip_tcp_appstate_t *)(uip_conn->appstate))->state = s;

	//	((uip_tcp_appstate_t *)(uip_conn->appstate))->p = PROCESS_CURRENT();

		PSOCK_INIT(&s->sk, (uint8_t *)s->inputbuf, sizeof(s->inputbuf)-1);
		s->timer = 0;
		handle_connections(s);
	} else if (s != NULL) {
		if (uip_poll()) {
			++s->timer;
			if (s->timer >= 20) {
				uip_abort();
				memb_free(&conns, s);
			}
		} else {
			s->timer = 0;
		}
		handle_connections(s);
	} else {
		uip_abort();
	}
}


PROCESS_THREAD(mytimer_process, ev, data)
{
	static struct etimer et;
	PROCESS_BEGIN();
        while (1) {
        etimer_set(&et, 5 * CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        etimer_reset(&et);
        print_local_addresses();
	}
	PROCESS_END();
}


PROCESS_THREAD(tcp_server_process, ev, data)
{
  
  PROCESS_BEGIN();
  putstring("Webserver start ...\n");

  tcp_listen(UIP_HTONS(80));
  while (1) {
	PROCESS_WAIT_EVENT();
	if (ev == tcpip_event) {
		httpd_appcall(data);
	}
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server's IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused && (state == ADDR_TENTATIVE || state
        == ADDR_PREFERRED)) {
      PRINTF("  ");
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}

