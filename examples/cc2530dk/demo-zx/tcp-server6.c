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



/* Should we act as RPL root? */
#define SERVER_RPL_ROOT       0 

#if SERVER_RPL_ROOT
static uip_ipaddr_t ipaddr;
#endif
/*---------------------------------------------------------------------------*/

PROCESS(tcp_server_process, "TCP server process"); 
AUTOSTART_PROCESSES(&tcp_server_process);

/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses:\n");
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
/*---------------------------------------------------------------------------*/
#if SERVER_RPL_ROOT
void
create_dag()
{
  rpl_dag_t *dag;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  print_local_addresses();

  dag = rpl_set_root(RPL_DEFAULT_INSTANCE, &uip_ds6_get_global(ADDR_PREFERRED)->ipaddr);
  if(dag != NULL) {
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("Created a new RPL dag with ID: ");
    PRINT6ADDR(&dag->dag_id);
    PRINTF("\n");
  }
}
#endif /* SERVER_RPL_ROOT */
/*---------------------------------------------------------------------------*/

static struct psock ps;
static char buffer[20];
static PT_THREAD(handle_con(struct psock *p))
{

	PSOCK_BEGIN(p);
	PSOCK_SEND_STR(p, "Welcome to contiki!\n"
			"Please put something and press enter. \n"
			"press 'exit' to quit!\n");
 	do {
	    PSOCK_READTO(p, '\n');
            PRINTF(" psock read data len %d\n", PSOCK_DATALEN(p));

            PSOCK_SEND_STR(p, "GOT:");
	    PSOCK_SEND(p, buffer, PSOCK_DATALEN(p));
        }while(!(PSOCK_DATALEN(p)==5 && 0==memcmp(buffer, "exit", 4)));

	PSOCK_SEND_STR(p, "Good bye!\r\n");
	PSOCK_CLOSE(p);
	
	PSOCK_END(p);
}

PROCESS_THREAD(tcp_server_process, ev, data)
{

  PROCESS_BEGIN();
  putstring("Starting web server\n");

  tcp_listen(UIP_HTONS(80));
  while (1) {
	PROCESS_WAIT_EVENT_UNTIL(ev==tcpip_event);
 	if (uip_connected()) {
		PSOCK_INIT(&ps, buffer, sizeof(buffer));
		while ( !(uip_aborted()||uip_closed()||uip_timedout())) {
               	 	handle_con(&ps);
			PROCESS_WAIT_EVENT_UNTIL(ev==tcpip_event);
 			PRINTF(" UIP_NEW DATA() %d\n", uip_datalen());
		}
	}
  }

  PROCESS_END();
}

void lirend(char *fmt, ...)
{
/*        int x;
         va_list arg_ptr;
        char buf[32];

        va_start(arg_ptr, fmt);
        x = vsprintf(buf, fmt, arg_ptr);
        va_end(arg_ptr);

*/
        putstring(fmt);

}
/*---------------------------------------------------------------------------*/
