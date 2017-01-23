/* dnsmasq is Copyright (c) 2000-2009 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef HAVE_DHCP

#define BOOTREQUEST              1
#define BOOTREPLY                2
#define DHCP_COOKIE              0x63825363

#define MIN_PACKETSZ             300

#define OPTION_PAD               0
#define OPTION_NETMASK           1
#define OPTION_ROUTER            3
#define OPTION_DNSSERVER         6
#define OPTION_HOSTNAME          12
#define OPTION_DOMAINNAME        15
#define OPTION_BROADCAST         28
#define OPTION_VENDOR_CLASS_OPT  43
#define OPTION_REQUESTED_IP      50 
#define OPTION_LEASE_TIME        51
#define OPTION_OVERLOAD          52
#define OPTION_MESSAGE_TYPE      53
#define OPTION_SERVER_IDENTIFIER 54
#define OPTION_REQUESTED_OPTIONS 55
#define OPTION_MESSAGE           56
#define OPTION_MAXMESSAGE        57
#define OPTION_T1                58
#define OPTION_T2                59
#define OPTION_VENDOR_ID         60
#define OPTION_CLIENT_ID         61
#define OPTION_SNAME             66
#define OPTION_FILENAME          67
#define OPTION_USER_CLASS        77
#define OPTION_CLIENT_FQDN       81
#define OPTION_AGENT_ID          82
#define OPTION_ARCH              93
#define OPTION_PXE_UUID          97
#define OPTION_SUBNET_SELECT     118
#define OPTION_END               255

#define SUBOPT_CIRCUIT_ID        1
#define SUBOPT_REMOTE_ID         2
#define SUBOPT_SUBNET_SELECT     5     
#define SUBOPT_SUBSCR_ID         6     
#define SUBOPT_SERVER_OR         11    

#define SUBOPT_PXE_BOOT_ITEM     71    
#define SUBOPT_PXE_DISCOVERY     6
#define SUBOPT_PXE_SERVERS       8
#define SUBOPT_PXE_MENU          9
#define SUBOPT_PXE_MENU_PROMPT   10

#define DHCPDISCOVER             1
#define DHCPOFFER                2
#define DHCPREQUEST              3
#define DHCPDECLINE              4
#define DHCPACK                  5
#define DHCPNAK                  6
#define DHCPRELEASE              7
#define DHCPINFORM               8

#define have_config(config, mask) ((config) && ((config)->flags & (mask))) 
#define option_len(opt) ((int)(((unsigned char *)(opt))[1]))
#define option_ptr(opt, i) ((void *)&(((unsigned char *)(opt))[2u+(unsigned int)(i)]))

static int sanitise(unsigned char *opt, char *buf);
static struct in_addr server_id(struct dhcp_context *context, struct in_addr override, struct in_addr fallback);
static unsigned int calc_time(struct dhcp_context *context, struct dhcp_config *config, unsigned char *opt);
static void option_put(struct dhcp_packet *mess, unsigned char *end, int opt, int len, unsigned int val);
static void option_put_string(struct dhcp_packet *mess, unsigned char *end, 
			      int opt, char *string, int null_term);
static struct in_addr option_addr(unsigned char *opt);
static struct in_addr option_addr_arr(unsigned char *opt, int offset);
static unsigned int option_uint(unsigned char *opt, int i, int size);
static void log_packet(char *type, void *addr, unsigned char *ext_mac, 
		       int mac_len, char *interface, char *string, u32 xid);
static unsigned char *option_find(struct dhcp_packet *mess, size_t size, int opt_type, int minsize);
static unsigned char *option_find1(unsigned char *p, unsigned char *end, int opt, int minsize);
static size_t dhcp_packet_size(struct dhcp_packet *mess, struct dhcp_netid *netid,
			       unsigned char *agent_id, unsigned char *real_end);
static void clear_packet(struct dhcp_packet *mess, unsigned char *end);
static void do_options(struct dhcp_context *context,
		       struct dhcp_packet *mess,
		       unsigned char *real_end, 
		       unsigned char *req_options,
		       char *hostname, 
		       char *domain, char *config_domain,
		       struct dhcp_netid *netid,
		       struct in_addr subnet_addr,
		       unsigned char fqdn_flags,
		       int null_term, int pxearch,
		       unsigned char *uuid);


static void match_vendor_opts(unsigned char *opt, struct dhcp_opt *dopt); 
static void do_encap_opts(struct dhcp_opt *opts, int encap, int flag, struct dhcp_packet *mess, unsigned char *end, int null_term);
static void pxe_misc(struct dhcp_packet *mess, unsigned char *end, unsigned char *uuid);
static int prune_vendor_opts(struct dhcp_netid *netid);
static struct dhcp_opt *pxe_opts(int pxe_arch, struct dhcp_netid *netid);
struct dhcp_boot *find_boot(struct dhcp_netid *netid);

  
size_t dhcp_reply(struct dhcp_context *context, char *iface_name, int int_index,
		  size_t sz, time_t now, int unicast_dest, int *is_inform)
{
  unsigned char *opt, *clid = NULL;
  struct dhcp_lease *ltmp, *lease = NULL;
  struct dhcp_vendor *vendor;
  struct dhcp_mac *mac;
  struct dhcp_netid_list *id_list;
  int clid_len = 0, ignore = 0, do_classes = 0, selecting = 0, pxearch = -1;
  struct dhcp_packet *mess = (struct dhcp_packet *)daemon->dhcp_packet.iov_base;
  unsigned char *end = (unsigned char *)(mess + 1); 
  unsigned char *real_end = (unsigned char *)(mess + 1); 
  char *hostname = NULL, *offer_hostname = NULL, *client_hostname = NULL, *domain = NULL;
  int hostname_auth = 0, borken_opt = 0;
  unsigned char *req_options = NULL;
  char *message = NULL;
  unsigned int time;
  struct dhcp_config *config;
  struct dhcp_netid *netid;
  struct in_addr subnet_addr, fallback, override;
  unsigned short fuzz = 0;
  unsigned int mess_type = 0;
  unsigned char fqdn_flags = 0;
  unsigned char *agent_id = NULL, *uuid = NULL;
  unsigned char *emac = NULL;
  int emac_len = 0;
  struct dhcp_netid known_id, iface_id;
  struct dhcp_opt *o;
  unsigned char pxe_uuid[17];

  subnet_addr.s_addr = override.s_addr = 0;

  
  iface_id.net = iface_name;
  iface_id.next = NULL;
  netid = &iface_id; 
  
  if (mess->op != BOOTREQUEST || mess->hlen > DHCP_CHADDR_MAX)
    return 0;
   
  if (mess->htype == 0 && mess->hlen != 0)
    return 0;

  
  if ((opt = option_find(mess, sz, OPTION_MESSAGE_TYPE, 1)))
    {
      mess_type = option_uint(opt, 0, 1);

      
      if (*((u32 *)&mess->options) != htonl(DHCP_COOKIE))
	return 0;

      if ((opt = option_find(mess, sz, OPTION_MAXMESSAGE, 2)))
	{
	  size_t size = (size_t)option_uint(opt, 0, 2) - 28;
	  
	  if (size > DHCP_PACKET_MAX)
	    size = DHCP_PACKET_MAX;
	  else if (size < sizeof(struct dhcp_packet))
	    size = sizeof(struct dhcp_packet);
	  
	  if (expand_buf(&daemon->dhcp_packet, size))
	    {
	      mess = (struct dhcp_packet *)daemon->dhcp_packet.iov_base;
	      real_end = end = ((unsigned char *)mess) + size;
	    }
	}

      if ((option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ) || mess_type == DHCPDISCOVER))
	mess->ciaddr.s_addr = 0;

      if ((opt = option_find(mess, sz, OPTION_AGENT_ID, 1)))
	{
	  /* Any agent-id needs to be copied back out, verbatim, as the last option
	     in the packet. Here, we shift it to the very end of the buffer, if it doesn't
	     get overwritten, then it will be shuffled back at the end of processing.
	     Note that the incoming options must not be overwritten here, so there has to 
	     be enough free space at the end of the packet to copy the option. */
	  unsigned char *sopt;
	  unsigned int total = option_len(opt) + 2;
	  unsigned char *last_opt = option_find(mess, sz, OPTION_END, 0);
	  if (last_opt && last_opt < end - total)
	    {
	      end -= total;
	      agent_id = end;
	      memcpy(agent_id, opt, total);
	    }

	  
	  if ((sopt = option_find1(option_ptr(opt, 0), option_ptr(opt, option_len(opt)), SUBOPT_SUBNET_SELECT, INADDRSZ)))
	    subnet_addr = option_addr(sopt);

	  
	  if ((sopt = option_find1(option_ptr(opt, 0), option_ptr(opt, option_len(opt)), SUBOPT_SERVER_OR, INADDRSZ)))
	    override = option_addr(sopt);
	  
	   
	  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
	    {
	      int search;
	      
	      if (vendor->match_type == MATCH_CIRCUIT)
		search = SUBOPT_CIRCUIT_ID;
	      else if (vendor->match_type == MATCH_REMOTE)
		search = SUBOPT_REMOTE_ID;
	      else if (vendor->match_type == MATCH_SUBSCRIBER)
		search = SUBOPT_SUBSCR_ID;
	      else 
		continue;

	      if ((sopt = option_find1(option_ptr(opt, 0), option_ptr(opt, option_len(opt)), search, 1)) &&
		  vendor->len == option_len(sopt) &&
		  memcmp(option_ptr(sopt, 0), vendor->data, vendor->len) == 0)
		{
		  vendor->netid.next = netid;
		  netid = &vendor->netid;
		  break;
		} 
	    }
	}

      
      if (subnet_addr.s_addr == 0 && (opt = option_find(mess, sz, OPTION_SUBNET_SELECT, INADDRSZ)))
	subnet_addr = option_addr(opt);
      
      
      if ((opt = option_find(mess, sz, OPTION_CLIENT_ID, 1)))
	{
	  clid_len = option_len(opt);
	  clid = option_ptr(opt, 0);
	}

      
      lease = lease_find_by_client(mess->chaddr, mess->hlen, mess->htype, clid, clid_len);

      if (lease && !clid && lease->clid)
	{
	  clid_len = lease->clid_len;
	  clid = lease->clid;
	}

      
      emac = extended_hwaddr(mess->htype, mess->hlen, mess->chaddr, clid_len, clid, &emac_len);
    }
  
  for (mac = daemon->dhcp_macs; mac; mac = mac->next)
    if (mac->hwaddr_len == mess->hlen &&
	(mac->hwaddr_type == mess->htype || mac->hwaddr_type == 0) &&
	memcmp_masked(mac->hwaddr, mess->chaddr, mess->hlen, mac->mask))
      {
	mac->netid.next = netid;
	netid = &mac->netid;
      }
  
  if (mess->giaddr.s_addr || subnet_addr.s_addr || mess->ciaddr.s_addr)
    {
      struct dhcp_context *context_tmp, *context_new = NULL;
      struct in_addr addr;
      int force = 0;
      
      if (subnet_addr.s_addr)
	{
	  addr = subnet_addr;
	  force = 1;
	}
      else if (mess->giaddr.s_addr)
	{
	  addr = mess->giaddr;
	  force = 1;
	}
      else
	{
	  
	  addr = mess->ciaddr;
	  for (context_tmp = context; context_tmp; context_tmp = context_tmp->current)
	    if (context_tmp->netmask.s_addr && 
		is_same_net(addr, context_tmp->start, context_tmp->netmask) &&
		is_same_net(addr, context_tmp->end, context_tmp->netmask))
	      {
		context_new = context;
		break;
	      }
	} 
		
      if (!context_new)
	for (context_tmp = daemon->dhcp; context_tmp; context_tmp = context_tmp->next)
	  if (context_tmp->netmask.s_addr  && 
	      is_same_net(addr, context_tmp->start, context_tmp->netmask) &&
	      is_same_net(addr, context_tmp->end, context_tmp->netmask))
	    {
	      context_tmp->current = context_new;
	      context_new = context_tmp;
	    }
      
      if (context_new || force)
	context = context_new;
      
    }
  
  if (!context)
    {
      return 0;
    }

  
  fallback = context->local;
  
  if (daemon->options & OPT_LOG_OPTS)
    {
      struct dhcp_context *context_tmp;
      for (context_tmp = context; context_tmp; context_tmp = context_tmp->current)
	{
	  strcpy(daemon->namebuff, inet_ntoa(context_tmp->start));
	  if (context_tmp->flags & (CONTEXT_STATIC | CONTEXT_PROXY))
	    my_syslog(MS_DHCP | LOG_INFO, _("%u Available DHCP subnet: %s/%s"),
		      ntohl(mess->xid), daemon->namebuff, inet_ntoa(context_tmp->netmask));
	  else
	    my_syslog(MS_DHCP | LOG_INFO, _("%u Available DHCP range: %s -- %s"), 
		      ntohl(mess->xid), daemon->namebuff, inet_ntoa(context_tmp->end));
	}
    }

  mess->op = BOOTREPLY;
  
  config = find_config(daemon->dhcp_conf, context, clid, clid_len, 
		       mess->chaddr, mess->hlen, mess->htype, NULL);

  
  if (config)
    {
      known_id.net = "known";
      known_id.next = netid;
      netid = &known_id;
    }
  
  if (mess_type == 0)
    {
      
      struct dhcp_netid id, bootp_id;
      struct in_addr *logaddr = NULL;

      
      if (mess->htype == 0 || mess->hlen == 0 || (context->flags & CONTEXT_PROXY))
	return 0;
      
      if (have_config(config, CONFIG_DISABLE))
	message = _("disabled");

      end = mess->options + 64; 
            
      if (have_config(config, CONFIG_NAME))
	{
	  hostname = config->hostname;
	  domain = config->domain;
	}

      if (have_config(config, CONFIG_NETID))
	{
	  config->netid.next = netid;
	  netid = &config->netid;
	}

      
      if (mess->file[0])
	{
	  memcpy(daemon->dhcp_buff2, mess->file, sizeof(mess->file));
	  daemon->dhcp_buff2[sizeof(mess->file) + 1] = 0; 
	  id.net = (char *)daemon->dhcp_buff2;
	  id.next = netid;
	  netid = &id;
	}

      bootp_id.net = "bootp";
      bootp_id.next = netid;
      netid = &bootp_id;
      
      for (id_list = daemon->dhcp_ignore; id_list; id_list = id_list->next)
	if (match_netid(id_list->list, netid, 0))
	  message = _("ignored");
      
      if (!message)
	{
	  int nailed = 0;

	  if (have_config(config, CONFIG_ADDR))
	    {
	      nailed = 1;
	      logaddr = &config->addr;
	      mess->yiaddr = config->addr;
	      if ((lease = lease_find_by_addr(config->addr)) &&
		  (lease->hwaddr_len != mess->hlen ||
		   lease->hwaddr_type != mess->htype ||
		   memcmp(lease->hwaddr, mess->chaddr, lease->hwaddr_len) != 0))
		message = _("address in use");
	    }
	  else
	    {
	      if (!(lease = lease_find_by_client(mess->chaddr, mess->hlen, mess->htype, NULL, 0)) ||
		  !address_available(context, lease->addr, netid))
		{
		   if (lease)
		     {
		       
		       lease_prune(lease, now);
		       lease = NULL;
		     }
		   if (!address_allocate(context, &mess->yiaddr, mess->chaddr, mess->hlen, netid, now))
		     message = _("no address available");
		}
	      else
		mess->yiaddr = lease->addr;
	    }
	  
	  if (!message && !(context = narrow_context(context, mess->yiaddr, netid)))
	    message = _("wrong network");
	  else if (context->netid.net)
	    {
	      context->netid.next = netid;
	      netid = &context->netid;
	    }	 
	  
	  if (!message && !nailed)
	    {
	      for (id_list = daemon->bootp_dynamic; id_list; id_list = id_list->next)
		if ((!id_list->list) || match_netid(id_list->list, netid, 0))
		  break;
	      if (!id_list)
		message = _("no address configured");
	    }

	  if (!message && 
	      !lease && 
	      (!(lease = lease_allocate(mess->yiaddr))))
	    message = _("no leases left");
	  
	  if (!message)
	    {
	      logaddr = &mess->yiaddr;
		
	      lease_set_hwaddr(lease, mess->chaddr, NULL, mess->hlen, mess->htype, 0);
	      if (hostname)
		lease_set_hostname(lease, hostname, 1); 
	      
	      lease_set_expires(lease,  
				have_config(config, CONFIG_TIME) ? config->lease_time : 0xffffffff, 
				now); 
	      lease_set_interface(lease, int_index);
	      
	      clear_packet(mess, end);
	      do_options(context, mess, end, NULL, hostname, get_domain(mess->yiaddr), 
			 domain, netid, subnet_addr, 0, 0, 0, NULL);
	    }
	}
      
      log_packet("BOOTP", logaddr, mess->chaddr, mess->hlen, iface_name, message, mess->xid);
      
      return message ? 0 : dhcp_packet_size(mess, netid, agent_id, real_end);
    }
      
  if ((opt = option_find(mess, sz, OPTION_CLIENT_FQDN, 4)))
    {
      
      int len = option_len(opt);
      char *pq = daemon->dhcp_buff;
      unsigned char *pp, *op = option_ptr(opt, 0);
      
      fqdn_flags = *op;
      len -= 3;
      op += 3;
      pp = op;
      
      
      if (!(fqdn_flags & 0x01))
	fqdn_flags |= 0x02;
      
      fqdn_flags &= ~0x08;
      fqdn_flags |= 0x01;
      
      if (fqdn_flags & 0x04)
	while (*op != 0 && ((op + (*op) + 1) - pp) < len)
	  {
	    memcpy(pq, op+1, *op);
	    pq += *op;
	    op += (*op)+1;
	    *(pq++) = '.';
	  }
      else
	{
	  memcpy(pq, op, len);
	  if (len > 0 && op[len-1] == 0)
	    borken_opt = 1;
	  pq += len + 1;
	}
      
      if (pq != daemon->dhcp_buff)
	pq--;
      
      *pq = 0;
      
      if (legal_hostname(daemon->dhcp_buff))
	offer_hostname = client_hostname = daemon->dhcp_buff;
    }
  else if ((opt = option_find(mess, sz, OPTION_HOSTNAME, 1)))
    {
      int len = option_len(opt);
      memcpy(daemon->dhcp_buff, option_ptr(opt, 0), len);
      if (len > 0 && daemon->dhcp_buff[len-1] == 0)
	borken_opt = 1;
      else
	daemon->dhcp_buff[len] = 0;
      if (legal_hostname(daemon->dhcp_buff))
	client_hostname = daemon->dhcp_buff;
    }

  if (client_hostname && daemon->options & OPT_LOG_OPTS)
    my_syslog(MS_DHCP | LOG_INFO, _("%u client provides name: %s"), ntohl(mess->xid), client_hostname);
  
  if (have_config(config, CONFIG_NAME))
    {
      hostname = config->hostname;
      domain = config->domain;
      hostname_auth = 1;
      
      if (fqdn_flags != 0 || !client_hostname || hostname_isequal(hostname, client_hostname))
        offer_hostname = hostname;
    }
  else if (client_hostname)
    {
      domain = strip_hostname(client_hostname);
      
      if (strlen(client_hostname) != 0)
	{
	  hostname = client_hostname;
	  if (!config)
	    {
	      struct dhcp_config *new = find_config(daemon->dhcp_conf, context, NULL, 0,
						    mess->chaddr, mess->hlen, 
						    mess->htype, hostname);
	      if (new && !have_config(new, CONFIG_CLID) && !new->hwaddr)
		{
		  config = new;
		  
		  known_id.net = "known";
		  known_id.next = netid;
		  netid = &known_id;
		}
	    }
	}
    }
  
  if (have_config(config, CONFIG_NETID))
    {
      config->netid.next = netid;
      netid = &config->netid;
    }
  
  for (o = daemon->dhcp_match; o; o = o->next)
    {
      int i, matched = 0;
      
      if (!(opt = option_find(mess, sz, o->opt, 1)) ||
	  o->len > option_len(opt))
	continue;
      
      if (o->len == 0)
	matched = 1;
      else if (o->flags & DHOPT_HEX)
	{ 
	  if (memcmp_masked(o->val, option_ptr(opt, 0), o->len, o->u.wildcard_mask))
	    matched = 1;
	}
      else 
	for (i = 0; i <= (option_len(opt) - o->len); ) 
	  {
	    if (memcmp(o->val, option_ptr(opt, i), o->len) == 0)
	      {
		matched = 1;
		break;
	      }

	    if (o->flags & DHOPT_STRING)
	      i++;
	    else
	      i += o->len;
	  }
      
      if (matched)
	{
	  o->netid->next = netid;
	  netid = o->netid;
	}
    }
	

  if ((opt = option_find(mess, sz, OPTION_USER_CLASS, 1)))
    {
      unsigned char *ucp = option_ptr(opt, 0);
      int tmp, j;
      for (j = 0; j < option_len(opt); j += ucp[j] + 1);
      if (j == option_len(opt))
	for (j = 0; j < option_len(opt); j = tmp)
	  {
	    tmp = j + ucp[j] + 1;
	    ucp[j] = 0;
	  }
    }
    
  for (vendor = daemon->dhcp_vendors; vendor; vendor = vendor->next)
    {
      int mopt;
      
      if (vendor->match_type == MATCH_VENDOR)
	mopt = OPTION_VENDOR_ID;
      else if (vendor->match_type == MATCH_USER)
	mopt = OPTION_USER_CLASS; 
      else
	continue;

      if ((opt = option_find(mess, sz, mopt, 1)))
	{
	  int i;
	  for (i = 0; i <= (option_len(opt) - vendor->len); i++)
	    if (memcmp(vendor->data, option_ptr(opt, i), vendor->len) == 0)
	      {
		vendor->netid.next = netid;
		netid = &vendor->netid;
		break;
	      }
	}
    }

  
  match_vendor_opts(option_find(mess, sz, OPTION_VENDOR_ID, 1), daemon->dhcp_opts);
    
  if (daemon->options & OPT_LOG_OPTS)
    {
      if (sanitise(option_find(mess, sz, OPTION_VENDOR_ID, 1), daemon->namebuff))
	my_syslog(MS_DHCP | LOG_INFO, _("%u Vendor class: %s"), ntohl(mess->xid), daemon->namebuff);
      if (sanitise(option_find(mess, sz, OPTION_USER_CLASS, 1), daemon->namebuff))
	my_syslog(MS_DHCP | LOG_INFO, _("%u User class: %s"), ntohl(mess->xid), daemon->namebuff);
    }

  
  for (id_list = daemon->dhcp_ignore; id_list; id_list = id_list->next)
    if (match_netid(id_list->list, netid, 0))
      ignore = 1;
   
  
  if (have_config(config, CONFIG_NOCLID))
    clid = NULL;
          
  
  if (daemon->enable_pxe && 
      (opt = option_find(mess, sz, OPTION_VENDOR_ID, 9)) && 
      strncmp(option_ptr(opt, 0), "PXEClient", 9) == 0)
    {
      if ((opt = option_find(mess, sz, OPTION_PXE_UUID, 17)))
	{
	  memcpy(pxe_uuid, option_ptr(opt, 0), 17);
	  uuid = pxe_uuid;
	}

      
      if ((mess_type == DHCPREQUEST || mess_type == DHCPINFORM) &&
	  (opt = option_find(mess, sz, OPTION_VENDOR_CLASS_OPT, 1)) &&
	  (opt = option_find1(option_ptr(opt, 0), option_ptr(opt, option_len(opt)), SUBOPT_PXE_BOOT_ITEM, 4)))
	{
	  struct pxe_service *service;
	  int type = option_uint(opt, 0, 2);
	  int layer = option_uint(opt, 2, 2);
	  unsigned char save71[4];
	  struct dhcp_opt opt71;

	  if (ignore)
	    return 0;

	  if (layer & 0x8000)
	    {
	      my_syslog(MS_DHCP | LOG_ERR, _("PXE BIS not supported"));
	      return 0;
	    }

	  memcpy(save71, option_ptr(opt, 0), 4);
	  
	  for (service = daemon->pxe_services; service; service = service->next)
	    if (service->type == type)
	      break;
	  
	  if (!service || !service->basename)
	    return 0;
	  
	  clear_packet(mess, end);
	  
	  mess->yiaddr = mess->ciaddr;
	  mess->ciaddr.s_addr = 0;
	  if (service->server.s_addr != 0)
	    mess->siaddr = service->server; 
	  else
	    mess->siaddr = context->local; 
	  
	  snprintf((char *)mess->file, sizeof(mess->file), "%s.%d", service->basename, layer);
	  option_put(mess, end, OPTION_MESSAGE_TYPE, 1, DHCPACK);
	  option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, htonl(context->local.s_addr));
	  pxe_misc(mess, end, uuid);
	  
	  prune_vendor_opts(netid);
	  opt71.val = save71;
	  opt71.opt = SUBOPT_PXE_BOOT_ITEM;
	  opt71.len = 4;
	  opt71.flags = DHOPT_VENDOR_MATCH;
	  opt71.netid = NULL;
	  opt71.next = daemon->dhcp_opts;
	  do_encap_opts(&opt71, OPTION_VENDOR_CLASS_OPT, DHOPT_VENDOR_MATCH, mess, end, 0);
	  
	  log_packet("PXE", &mess->yiaddr, emac, emac_len, iface_name, (char *)mess->file, mess->xid);
	  return dhcp_packet_size(mess, netid, agent_id, real_end);	  
	}
      
      if ((opt = option_find(mess, sz, OPTION_ARCH, 2)))
	{
	  pxearch = option_uint(opt, 0, 2);

	  
	  if ((mess_type == DHCPDISCOVER || mess_type == DHCPREQUEST) && 
	      (context->flags & CONTEXT_PROXY))
	    {
	      struct dhcp_boot *boot = find_boot(netid);

	      mess->yiaddr.s_addr = 0;
	      if  (mess_type == DHCPDISCOVER || mess->ciaddr.s_addr == 0)
		{
		  mess->ciaddr.s_addr = 0;
		  mess->flags |= htons(0x8000); 
		}

	      clear_packet(mess, end);
	      
	      if (boot)
		{
		  if (boot->next_server.s_addr)
		    mess->siaddr = boot->next_server;
		  
		  if (boot->file)
		    strncpy((char *)mess->file, boot->file, sizeof(mess->file)-1);
		}

	      option_put(mess, end, OPTION_MESSAGE_TYPE, 1, 
			 mess_type == DHCPDISCOVER ? DHCPOFFER : DHCPACK);
	      option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, htonl(context->local.s_addr));
	      pxe_misc(mess, end, uuid);
	      prune_vendor_opts(netid);
	      do_encap_opts(pxe_opts(pxearch, netid), OPTION_VENDOR_CLASS_OPT, DHOPT_VENDOR_MATCH, mess, end, 0);
	      
	      log_packet("PXE", NULL, emac, emac_len, iface_name, ignore ? "proxy" : "proxy-ignored", mess->xid);
	      return ignore ? 0 : dhcp_packet_size(mess, netid, agent_id, real_end);	  
	    }
	}
    }

  
  if (context->flags & CONTEXT_PROXY)
    return 0;
  
  if ((opt = option_find(mess, sz, OPTION_REQUESTED_OPTIONS, 0)))
    {
      req_options = (unsigned char *)daemon->dhcp_buff2;
      memcpy(req_options, option_ptr(opt, 0), option_len(opt));
      req_options[option_len(opt)] = OPTION_END;
    }
  
  switch (mess_type)
    {
    case DHCPDECLINE:
      if (!(opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)) ||
	  option_addr(opt).s_addr != server_id(context, override, fallback).s_addr)
	return 0;
      
      
      sanitise(option_find(mess, sz, OPTION_MESSAGE, 1), daemon->dhcp_buff);
      
      if (!(opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))
	return 0;
      
      log_packet("DHCPDECLINE", option_ptr(opt, 0), emac, emac_len, iface_name, daemon->dhcp_buff, mess->xid);
      
      if (lease && lease->addr.s_addr == option_addr(opt).s_addr)
	lease_prune(lease, now);
      
      if (have_config(config, CONFIG_ADDR) && 
	  config->addr.s_addr == option_addr(opt).s_addr)
	{
	  prettyprint_time(daemon->dhcp_buff, DECLINE_BACKOFF);
	  my_syslog(MS_DHCP | LOG_WARNING, _("disabling DHCP static address %s for %s"), 
		    inet_ntoa(config->addr), daemon->dhcp_buff);
	  config->flags |= CONFIG_DECLINED;
	  config->decline_time = now;
	}
      else
	
	for (; context; context = context->current)
	  context->addr_epoch++;
      
      return 0;

    case DHCPRELEASE:
      if (!(context = narrow_context(context, mess->ciaddr, netid)) ||
	  !(opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)) ||
	  option_addr(opt).s_addr != server_id(context, override, fallback).s_addr)
	return 0;
      
      if (lease && lease->addr.s_addr == mess->ciaddr.s_addr)
	lease_prune(lease, now);
      else
	message = _("unknown lease");

      log_packet("DHCPRELEASE", &mess->ciaddr, emac, emac_len, iface_name, message, mess->xid);
	
      return 0;
      
    case DHCPDISCOVER:
      if (ignore || have_config(config, CONFIG_DISABLE))
	{
	  message = _("ignored");
	  opt = NULL;
	}
      else 
	{
	  struct in_addr addr, conf;
	  
	  addr.s_addr = conf.s_addr = 0;

	  if ((opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))	 
	    addr = option_addr(opt);
	  
	  if (have_config(config, CONFIG_ADDR))
	    {
	      char *addrs = inet_ntoa(config->addr);
	      
	      if ((ltmp = lease_find_by_addr(config->addr)) && 
		  ltmp != lease &&
		  !config_has_mac(config, ltmp->hwaddr, ltmp->hwaddr_len, ltmp->hwaddr_type))
		{
		  int len;
		  unsigned char *mac = extended_hwaddr(ltmp->hwaddr_type, ltmp->hwaddr_len,
						       ltmp->hwaddr, ltmp->clid_len, ltmp->clid, &len);
		  my_syslog(MS_DHCP | LOG_WARNING, _("not using configured address %s because it is leased to %s"),
			    addrs, print_mac(daemon->namebuff, mac, len));
		}
	      else
		{
		  struct dhcp_context *tmp;
		  for (tmp = context; tmp; tmp = tmp->current)
		    if (context->router.s_addr == config->addr.s_addr)
		      break;
		  if (tmp)
		    my_syslog(MS_DHCP | LOG_WARNING, _("not using configured address %s because it is in use by the server or relay"), addrs);
		  else if (have_config(config, CONFIG_DECLINED) &&
			   difftime(now, config->decline_time) < (float)DECLINE_BACKOFF)
		    my_syslog(MS_DHCP | LOG_WARNING, _("not using configured address %s because it was previously declined"), addrs);
		  else
		    conf = config->addr;
		}
	    }
	  
	  if (conf.s_addr)
	    mess->yiaddr = conf;
	  else if (lease && 
		   address_available(context, lease->addr, netid) && 
		   !config_find_by_address(daemon->dhcp_conf, lease->addr))
	    mess->yiaddr = lease->addr;
	  else if (opt && address_available(context, addr, netid) && !lease_find_by_addr(addr) && 
		   !config_find_by_address(daemon->dhcp_conf, addr))
	    mess->yiaddr = addr;
	  else if (emac_len == 0)
	    message = _("no unique-id");
	  else if (!address_allocate(context, &mess->yiaddr, emac, emac_len, netid, now))
	    message = _("no address available");      
	}
      
      log_packet("DHCPDISCOVER", opt ? option_ptr(opt, 0) : NULL, emac, emac_len, iface_name, message, mess->xid); 

      if (message || !(context = narrow_context(context, mess->yiaddr, netid)))
	return 0;

      log_packet("DHCPOFFER" , &mess->yiaddr, emac, emac_len, iface_name, NULL, mess->xid);

      if (context->netid.net)
	{
	  context->netid.next = netid;
	  netid = &context->netid;
	}
       
      time = calc_time(context, config, option_find(mess, sz, OPTION_LEASE_TIME, 4));
      clear_packet(mess, end);
      option_put(mess, end, OPTION_MESSAGE_TYPE, 1, DHCPOFFER);
      option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(server_id(context, override, fallback).s_addr));
      option_put(mess, end, OPTION_LEASE_TIME, 4, time);
      
      if (time != 0xffffffff)
	{
	  option_put(mess, end, OPTION_T1, 4, (time/2));
	  option_put(mess, end, OPTION_T2, 4, (time*7)/8);
	}
      do_options(context, mess, end, req_options, offer_hostname, get_domain(mess->yiaddr), 
		 domain, netid, subnet_addr, fqdn_flags, borken_opt, pxearch, uuid);
      
      return dhcp_packet_size(mess, netid, agent_id, real_end);
      
    case DHCPREQUEST:
      if (ignore || have_config(config, CONFIG_DISABLE))
	return 0;
      if ((opt = option_find(mess, sz, OPTION_REQUESTED_IP, INADDRSZ)))
	{
	  
	  mess->yiaddr = option_addr(opt);
	  
	  
	  do_classes = 1;
	  
	  if ((opt = option_find(mess, sz, OPTION_SERVER_IDENTIFIER, INADDRSZ)))
	    {
	      
	      selecting = 1;
	      
	      if (override.s_addr != 0)
		{
		  if (option_addr(opt).s_addr != override.s_addr)
		    return 0;
		}
	      else 
		{
		  for (; context; context = context->current)
		    if (context->local.s_addr == option_addr(opt).s_addr)
		      break;
		  
		  if (!context)
		    {
		      if (!(daemon->options & OPT_AUTHORITATIVE))
			return 0;
		      message = _("wrong server-ID");
		    }
		}

	      
	      if (lease && lease->addr.s_addr != mess->yiaddr.s_addr)
		{
		  lease_prune(lease, now);
		  lease = NULL;
		}
	    }
	  else
	    {
	      
	      if (!lease && !(daemon->options & OPT_AUTHORITATIVE))
		return 0;
	      
	      if (lease && lease->addr.s_addr != mess->yiaddr.s_addr)
		{
		  message = _("wrong address");
		  
		  lease_prune(lease, now);
		  lease = NULL;
		}
	    }
	}
      else
	{
	   
	  if ((lease && mess->ciaddr.s_addr != lease->addr.s_addr) ||
	      (!lease && !(daemon->options & OPT_AUTHORITATIVE)))
	    {
	      message = _("lease not found");
	      
	      unicast_dest = 0;
	    }
	  
	  fuzz = rand16();
	  mess->yiaddr = mess->ciaddr;
	}
      
      log_packet("DHCPREQUEST", &mess->yiaddr, emac, emac_len, iface_name, NULL, mess->xid);
 
      if (!message)
	{
	  struct dhcp_config *addr_config;
	  struct dhcp_context *tmp = NULL;
	  
	  if (have_config(config, CONFIG_ADDR))
	    for (tmp = context; tmp; tmp = tmp->current)
	      if (context->router.s_addr == config->addr.s_addr)
		break;
	  
	  if (!(context = narrow_context(context, mess->yiaddr, netid)))
	    {
	      
	      message = _("wrong network");
	      
	      unicast_dest = 0;
	    }
	  
	  
	  else if (!address_available(context, mess->yiaddr, netid) &&
		   (!have_config(config, CONFIG_ADDR) || config->addr.s_addr != mess->yiaddr.s_addr))
	    message = _("address not available");
	  
	  else if (!tmp && !selecting &&
		   have_config(config, CONFIG_ADDR) && 
		   (!have_config(config, CONFIG_DECLINED) ||
		    difftime(now, config->decline_time) > (float)DECLINE_BACKOFF) &&
		   config->addr.s_addr != mess->yiaddr.s_addr &&
		   (!(ltmp = lease_find_by_addr(config->addr)) || ltmp == lease))
	    message = _("static lease available");

	  
	  else if ((addr_config = config_find_by_address(daemon->dhcp_conf, mess->yiaddr)) && addr_config != config)
	    message = _("address reserved");

	  else if (!lease && (ltmp = lease_find_by_addr(mess->yiaddr)))
	    {
	      if (config && config_has_mac(config, ltmp->hwaddr, ltmp->hwaddr_len, ltmp->hwaddr_type))
		{
		  my_syslog(MS_DHCP | LOG_INFO, _("abandoning lease to %s of %s"),
			    print_mac(daemon->namebuff, ltmp->hwaddr, ltmp->hwaddr_len), 
			    inet_ntoa(ltmp->addr));
		  lease = ltmp;
		}
	      else
		message = _("address in use");
	    }

	  if (!message)
	    {
	      if (emac_len == 0)
		message = _("no unique-id");
	      
	      else if (!lease)
		{	     
		  if ((lease = lease_allocate(mess->yiaddr)))
		    do_classes = 1;
		  else
		    message = _("no leases left");
		}
	    }
	}

      if (message)
	{
	  log_packet("DHCPNAK", &mess->yiaddr, emac, emac_len, iface_name, message, mess->xid);
	  
	  mess->yiaddr.s_addr = 0;
	  clear_packet(mess, end);
	  option_put(mess, end, OPTION_MESSAGE_TYPE, 1, DHCPNAK);
	  option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(server_id(context, override, fallback).s_addr));
	  option_put_string(mess, end, OPTION_MESSAGE, message, borken_opt);
	  if (!unicast_dest || mess->giaddr.s_addr != 0 || 
	      mess->ciaddr.s_addr == 0 || is_same_net(context->local, mess->ciaddr, context->netmask))
	    {
	      mess->flags |= htons(0x8000); 
	      mess->ciaddr.s_addr = 0;
	    }
	}
      else
	{
	   if (do_classes)
	     {
	       if (mess->giaddr.s_addr)
		 lease->giaddr = mess->giaddr;
	       
	       lease->changed = 1;
	       
	       if ((opt = option_find(mess, sz, OPTION_USER_CLASS, 1)))
		 {
		   int len = option_len(opt);
		   unsigned char *ucp = option_ptr(opt, 0);
		   
		   if (len != 0 && ucp[0] == 0)
		     ucp++, len--;
		   free(lease->userclass);
		   if ((lease->userclass = whine_malloc(len+1)))
		     {
		       memcpy(lease->userclass, ucp, len);
		       lease->userclass[len] = 0;
		       lease->userclass_len = len+1;
		     }
		 }
	       if ((opt = option_find(mess, sz, OPTION_VENDOR_ID, 1)))
		 {
		   int len = option_len(opt);
		   unsigned char *ucp = option_ptr(opt, 0);
		   free(lease->vendorclass);
		   if ((lease->vendorclass = whine_malloc(len+1)))
		     {
		       memcpy(lease->vendorclass, ucp, len);
		       lease->vendorclass[len] = 0;
		       lease->vendorclass_len = len+1;
		     }
		 }
	       if ((opt = option_find(mess, sz, OPTION_HOSTNAME, 1)))
		 {
		   int len = option_len(opt);
		   unsigned char *ucp = option_ptr(opt, 0);
		   free(lease->supplied_hostname);
		   if ((lease->supplied_hostname = whine_malloc(len+1)))
		     {
		       memcpy(lease->supplied_hostname, ucp, len);
		       lease->supplied_hostname[len] = 0;
		       lease->supplied_hostname_len = len+1;
		     }
		 }
	     }
	   
	   if (!hostname_auth && (client_hostname = host_from_dns(mess->yiaddr)))
	     {
	      hostname = client_hostname;
	      hostname_auth = 1;
	    }
      
	  if (context->netid.net)
	    {
	      context->netid.next = netid;
	      netid = &context->netid;
	    }
	
	  time = calc_time(context, config, option_find(mess, sz, OPTION_LEASE_TIME, 4));
	  lease_set_hwaddr(lease, mess->chaddr, clid, mess->hlen, mess->htype, clid_len);
	   
	  
	  if (!hostname_auth)
	    {
	      for (id_list = daemon->dhcp_ignore_names; id_list; id_list = id_list->next)
		if ((!id_list->list) || match_netid(id_list->list, netid, 0))
		  break;
	      if (id_list)
		hostname = NULL;
	    }
	  if (hostname)
	    lease_set_hostname(lease, hostname, hostname_auth);
	  
	  lease_set_expires(lease, time, now);
	  lease_set_interface(lease, int_index);

	  if (override.s_addr != 0)
	    lease->override = override;
	  else
	    override = lease->override;

	  log_packet("DHCPACK", &mess->yiaddr, emac, emac_len, iface_name, hostname, mess->xid);  
	  
	  clear_packet(mess, end);
	  option_put(mess, end, OPTION_MESSAGE_TYPE, 1, DHCPACK);
	  option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(server_id(context, override, fallback).s_addr));
	  option_put(mess, end, OPTION_LEASE_TIME, 4, time);
	  if (time != 0xffffffff)
	    {
	      while (fuzz > (time/16))
		fuzz = fuzz/2; 
	      option_put(mess, end, OPTION_T1, 4, (time/2) - fuzz);
	      option_put(mess, end, OPTION_T2, 4, ((time/8)*7) - fuzz);
	    }
	  do_options(context, mess, end, req_options, hostname, get_domain(mess->yiaddr), 
		     domain, netid, subnet_addr, fqdn_flags, borken_opt, pxearch, uuid);
	}

      return dhcp_packet_size(mess, netid, agent_id, real_end); 
      
    case DHCPINFORM:
      if (ignore || have_config(config, CONFIG_DISABLE))
	message = _("ignored");
      
      log_packet("DHCPINFORM", &mess->ciaddr, emac, emac_len, iface_name, message, mess->xid);
     
      if (message || mess->ciaddr.s_addr == 0)
	return 0;

      
      context = narrow_context(context, mess->ciaddr, netid);
      
      if (!lease &&
	  (lease = lease_find_by_addr(mess->ciaddr)) && 
	  lease->hostname)
	hostname = lease->hostname;
      
      if (!hostname)
	hostname = host_from_dns(mess->ciaddr);

      log_packet("DHCPACK", &mess->ciaddr, emac, emac_len, iface_name, hostname, mess->xid);
      
      if (context && context->netid.net)
	{
	  context->netid.next = netid;
	  netid = &context->netid;
	}
      
      if (lease)
	{
	  if (override.s_addr != 0)
	    lease->override = override;
	  else
	    override = lease->override;
	}

      clear_packet(mess, end);
      option_put(mess, end, OPTION_MESSAGE_TYPE, 1, DHCPACK);
      option_put(mess, end, OPTION_SERVER_IDENTIFIER, INADDRSZ, ntohl(server_id(context, override, fallback).s_addr));
      
      if (lease)
	{
	  if (lease->expires == 0)
	    time = 0xffffffff;
	  else
	    time = (unsigned int)difftime(lease->expires, now);
	  option_put(mess, end, OPTION_LEASE_TIME, 4, time);
	  lease_set_interface(lease, int_index);
	}

      do_options(context, mess, end, req_options, hostname, get_domain(mess->ciaddr),
		 domain, netid, subnet_addr, fqdn_flags, borken_opt, pxearch, uuid);
      
      *is_inform = 1; 
      return dhcp_packet_size(mess, netid, agent_id, real_end); 
    }
  
  return 0;
}

unsigned char *extended_hwaddr(int hwtype, int hwlen, unsigned char *hwaddr, 
				      int clid_len, unsigned char *clid, int *len_out)
{
  if (hwlen == 0 && clid && clid_len > 3)
    {
      if (clid[0]  == hwtype)
	{
	  *len_out = clid_len - 1 ;
	  return clid + 1;
	}

#if defined(ARPHRD_EUI64) && defined(ARPHRD_IEEE1394)
      if (clid[0] ==  ARPHRD_EUI64 && hwtype == ARPHRD_IEEE1394)
	{
	  *len_out = clid_len - 1 ;
	  return clid + 1;
	}
#endif
      
      *len_out = clid_len;
      return clid;
    }
  
  *len_out = hwlen;
  return hwaddr;
}

static unsigned int calc_time(struct dhcp_context *context, struct dhcp_config *config, unsigned char *opt)
{
  unsigned int time = have_config(config, CONFIG_TIME) ? config->lease_time : context->lease_time;
  
  if (opt)
    { 
      unsigned int req_time = option_uint(opt, 0, 4);
      if (req_time < 120 )
	req_time = 120; 
      if (time == 0xffffffff || (req_time != 0xffffffff && req_time < time))
	time = req_time;
    }

  return time;
}

static struct in_addr server_id(struct dhcp_context *context, struct in_addr override, struct in_addr fallback)
{
  if (override.s_addr != 0)
    return override;
  else if (context)
    return context->local;
  else
    return fallback;
}

static int sanitise(unsigned char *opt, char *buf)
{
  char *p;
  int i;
  
  *buf = 0;
  
  if (!opt)
    return 0;

  p = option_ptr(opt, 0);

  for (i = option_len(opt); i > 0; i--)
    {
      char c = *p++;
      if (isprint((int)c))
	*buf++ = c;
    }
  *buf = 0; 
  
  return 1;
}

static void log_packet(char *type, void *addr, unsigned char *ext_mac, 
		       int mac_len, char *interface, char *string, u32 xid)
{
  struct in_addr a;
 
  
  if (addr)
    memcpy(&a, addr, sizeof(a));
  
  print_mac(daemon->namebuff, ext_mac, mac_len);
  
  if(daemon->options & OPT_LOG_OPTS)
     my_syslog(MS_DHCP | LOG_INFO, "%u %s(%s) %s%s%s %s",
	       ntohl(xid), 
	       type,
	       interface, 
	       addr ? inet_ntoa(a) : "",
	       addr ? " " : "",
	       daemon->namebuff,
	       string ? string : "");
  else
    my_syslog(MS_DHCP | LOG_INFO, "%s(%s) %s%s%s %s",
	      type,
	      interface, 
	      addr ? inet_ntoa(a) : "",
	      addr ? " " : "",
	      daemon->namebuff,
	      string ? string : "");
}

static void log_options(unsigned char *start, u32 xid)
{
  while (*start != OPTION_END)
    {
      int is_ip, is_name, i;
      char *text = option_string(start[0], &is_ip, &is_name);
      unsigned char trunc = option_len(start);
      
      if (is_ip)
	for (daemon->namebuff[0]= 0, i = 0; i <= trunc - INADDRSZ; i += INADDRSZ) 
	  {
	    if (i != 0)
	      strncat(daemon->namebuff, ", ", 256 - strlen(daemon->namebuff));
	    strncat(daemon->namebuff, inet_ntoa(option_addr_arr(start, i)), 256 - strlen(daemon->namebuff));
	  }
      else if (!is_name || !sanitise(start, daemon->namebuff))
	{
	  if (trunc > 13)
	    trunc = 13;
	  print_mac(daemon->namebuff, option_ptr(start, 0), trunc);
	}
      
      my_syslog(MS_DHCP | LOG_INFO, "%u sent size:%3d option:%3d%s%s%s%s%s", 
		ntohl(xid), option_len(start), start[0],
		text ? ":" : "", text ? text : "",
		trunc == 0 ? "" : "  ",
		trunc == 0 ? "" : daemon->namebuff,
		trunc == option_len(start) ? "" : "...");
      start += start[1] + 2;
    }
}

static unsigned char *option_find1(unsigned char *p, unsigned char *end, int opt, int minsize)
{
  while (1) 
    {
      if (p > end)
	return NULL;
      else if (*p == OPTION_END)
	return opt == OPTION_END ? p : NULL;
      else if (*p == OPTION_PAD)
	p++;
      else 
	{ 
	  int opt_len;
	  if (p > end - 2)
	    return NULL; 
	  opt_len = option_len(p);
	  if (p > end - (2 + opt_len))
	    return NULL; 
	  if (*p == opt && opt_len >= minsize)
	    return p;
	  p += opt_len + 2;
	}
    }
}
 
static unsigned char *option_find(struct dhcp_packet *mess, size_t size, int opt_type, int minsize)
{
  unsigned char *ret, *overload;
  
  
  if ((ret = option_find1(&mess->options[0] + sizeof(u32), ((unsigned char *)mess) + size, opt_type, minsize)))
    return ret;

  
  if (!(overload = option_find1(&mess->options[0] + sizeof(u32), ((unsigned char *)mess) + size, OPTION_OVERLOAD, 1)))
    return NULL;
  
  
  if ((overload[2] & 1) &&
      (ret = option_find1(&mess->file[0], &mess->file[128], opt_type, minsize)))
    return ret;

  
  if ((overload[2] & 2) &&
      (ret = option_find1(&mess->sname[0], &mess->sname[64], opt_type, minsize)))
    return ret;

  return NULL;
}

static struct in_addr option_addr_arr(unsigned char *opt, int offset)
{
  
  
  struct in_addr ret;

  memcpy(&ret, option_ptr(opt, offset), INADDRSZ);

  return ret;
}

static struct in_addr option_addr(unsigned char *opt)
{
  return option_addr_arr(opt, 0);
}

static unsigned int option_uint(unsigned char *opt, int offset, int size)
{
  
  unsigned int ret = 0;
  int i;
  unsigned char *p = option_ptr(opt, offset);
  
  for (i = 0; i < size; i++)
    ret = (ret << 8) | *p++;

  return ret;
}

static unsigned char *dhcp_skip_opts(unsigned char *start)
{
  while (*start != 0)
    start += start[1] + 2;
  return start;
}

 
static unsigned char *find_overload(struct dhcp_packet *mess)
{
  unsigned char *p = &mess->options[0] + sizeof(u32);
  
  while (*p != 0)
    {
      if (*p == OPTION_OVERLOAD)
	return p;
      p += p[1] + 2;
    }
  return NULL;
}

static size_t dhcp_packet_size(struct dhcp_packet *mess, struct dhcp_netid *netid,
			       unsigned char *agent_id, unsigned char *real_end)
{
  unsigned char *p = dhcp_skip_opts(&mess->options[0] + sizeof(u32));
  unsigned char *overload;
  size_t ret;
  struct dhcp_netid_list *id_list;
  struct dhcp_netid *n;

  
  if (agent_id)
    {
      memmove(p, agent_id, real_end - agent_id);
      p += real_end - agent_id;
      memset(p, 0, real_end - p); 
    }
  
  
  if (netid && (daemon->options & OPT_LOG_OPTS))
    {
      char *s = daemon->namebuff;
      for (*s = 0; netid; netid = netid->next)
	{
	  
	  for (n = netid->next; n; n = n->next)
	    if (strcmp(netid->net, n->net) == 0)
	      break;
	  
	  if (!n)
	    {
	      strncat (s, netid->net, (MAXDNAME-1) - strlen(s));
	      if (netid->next)
		strncat (s, ", ", (MAXDNAME-1) - strlen(s));
	    }
	}
      my_syslog(MS_DHCP | LOG_INFO, _("%u tags: %s"), ntohl(mess->xid), s);
    } 
   
  
  overload = find_overload(mess);
  
  if (overload && (option_uint(overload, 0, 1) & 1))
    {
      *dhcp_skip_opts(mess->file) = OPTION_END;
      if (daemon->options & OPT_LOG_OPTS)
	log_options(mess->file, mess->xid);
    }
  else if ((daemon->options & OPT_LOG_OPTS) && strlen((char *)mess->file) != 0)
    my_syslog(MS_DHCP | LOG_INFO, _("%u bootfile name: %s"), ntohl(mess->xid), (char *)mess->file);
  
  if (overload && (option_uint(overload, 0, 1) & 2))
    {
      *dhcp_skip_opts(mess->sname) = OPTION_END;
      if (daemon->options & OPT_LOG_OPTS)
	log_options(mess->sname, mess->xid);
    }
  else if ((daemon->options & OPT_LOG_OPTS) && strlen((char *)mess->sname) != 0)
    my_syslog(MS_DHCP | LOG_INFO, _("%u server name: %s"), ntohl(mess->xid), (char *)mess->sname);


  *p++ = OPTION_END;
  
  if (daemon->options & OPT_LOG_OPTS)
    {
      if (mess->siaddr.s_addr != 0)
	my_syslog(MS_DHCP | LOG_INFO, _("%u next server: %s"), ntohl(mess->xid), inet_ntoa(mess->siaddr));
      
      log_options(&mess->options[0] + sizeof(u32), mess->xid);
    } 
  
  for (id_list = daemon->force_broadcast; id_list; id_list = id_list->next)
    if (match_netid(id_list->list, netid, 0))
      mess->flags |= htons(0x8000); 
  
  ret = (size_t)(p - (unsigned char *)mess);
  
  if (ret < MIN_PACKETSZ)
    ret = MIN_PACKETSZ;

  return ret;
}

static unsigned char *free_space(struct dhcp_packet *mess, unsigned char *end, int opt, int len)
{
  unsigned char *p = dhcp_skip_opts(&mess->options[0] + sizeof(u32));
  
  if (p + len + 3 >= end)
    
    {
      unsigned char *overload;
      
      if (!(overload = find_overload(mess)) &&
	  (mess->file[0] == 0 || mess->sname[0] == 0))
	{
	  overload = p;
	  *(p++) = OPTION_OVERLOAD;
	  *(p++) = 1;
	}
      
      p = NULL;
      
      
      if (overload)
	{
	  if (mess->file[0] == 0)
	    overload[2] |= 1;
	  
	  if (overload[2] & 1)
	    {
	      p = dhcp_skip_opts(mess->file);
	      if (p + len + 3 >= mess->file + sizeof(mess->file))
		p = NULL;
	    }
	  
	  if (!p)
	    {
	      
	      if (mess->sname[0] == 0)
		overload[2] |= 2;
	      
	      if (overload[2] & 2)
		{
		  p = dhcp_skip_opts(mess->sname);
		  if (p + len + 3 >= mess->sname + sizeof(mess->file))
		    p = NULL;
		}
	    }
	}
      
      if (!p)
	my_syslog(MS_DHCP | LOG_WARNING, _("cannot send DHCP/BOOTP option %d: no space left in packet"), opt);
    }
 
  if (p)
    {
      *(p++) = opt;
      *(p++) = len;
    }

  return p;
}
	      
static void option_put(struct dhcp_packet *mess, unsigned char *end, int opt, int len, unsigned int val)
{
  int i;
  unsigned char *p = free_space(mess, end, opt, len);
  
  if (p) 
    for (i = 0; i < len; i++)
      *(p++) = val >> (8 * (len - (i + 1)));
}

static void option_put_string(struct dhcp_packet *mess, unsigned char *end, int opt, 
			      char *string, int null_term)
{
  unsigned char *p;
  size_t len = strlen(string);

  if (null_term && len != 255)
    len++;

  if ((p = free_space(mess, end, opt, len)))
    memcpy(p, string, len);
}

static int do_opt(struct dhcp_opt *opt, unsigned char *p, struct dhcp_context *context, int null_term)
{
  int len = opt->len;
  
  if ((opt->flags & DHOPT_STRING) && null_term && len != 255)
    len++;

  if (p && len != 0)
    {
      if (context && (opt->flags & DHOPT_ADDR))
	{
	  int j;
	  struct in_addr *a = (struct in_addr *)opt->val;
	  for (j = 0; j < opt->len; j+=INADDRSZ, a++)
	    {
	      
	      if (a->s_addr == 0)
		memcpy(p, &context->local, INADDRSZ);
	      else
		memcpy(p, a, INADDRSZ);
	      p += INADDRSZ;
	    }
	}
      else
	memcpy(p, opt->val, len);
    }  
  return len;
}

static int in_list(unsigned char *list, int opt)
{
  int i;

   
  if (!list)
    return 1;
  
  for (i = 0; list[i] != OPTION_END; i++)
    if (opt == list[i])
      return 1;

  return 0;
}

static struct dhcp_opt *option_find2(struct dhcp_netid *netid, struct dhcp_opt *opts, int opt)
{
  struct dhcp_opt *tmp;  
  for (tmp = opts; tmp; tmp = tmp->next)
    if (tmp->opt == opt && !(tmp->flags & (DHOPT_ENCAPSULATE | DHOPT_VENDOR)))
      if (match_netid(tmp->netid, netid, netid ? 0 : 1))
	return tmp;
	      
  return netid ? option_find2(NULL, opts, opt) : NULL;
}

static void match_vendor_opts(unsigned char *opt, struct dhcp_opt *dopt)
{
  for (; dopt; dopt = dopt->next)
    {
      dopt->flags &= ~DHOPT_VENDOR_MATCH;
      if (opt && (dopt->flags & DHOPT_VENDOR))
	{
	  int i, len = 0;
	  if (dopt->u.vendor_class)
	    len = strlen((char *)dopt->u.vendor_class);
	  for (i = 0; i <= (option_len(opt) - len); i++)
	    if (len == 0 || memcmp(dopt->u.vendor_class, option_ptr(opt, i), len) == 0)
	      {
		dopt->flags |= DHOPT_VENDOR_MATCH;
		break;
	      }
	}
    }
}

static void do_encap_opts(struct dhcp_opt *opt, int encap, int flag,  
			  struct dhcp_packet *mess, unsigned char *end, int null_term)
{
  int len, enc_len;
  struct dhcp_opt *start;
  unsigned char *p;
    
  
  for (enc_len = 0, start = opt; opt; opt = opt->next)
    if (opt->flags & flag)
      {
	int new = do_opt(opt, NULL, NULL, null_term) + 2;
	if (enc_len + new <= 255)
	  enc_len += new;
	else
	  {
	    p = free_space(mess, end, encap, enc_len);
	    for (; start && start != opt; start = start->next)
	      if (p && (start->flags & flag))
		{
		  len = do_opt(start, p + 2, NULL, null_term);
		  *(p++) = start->opt;
		  *(p++) = len;
		  p += len;
		}
	    enc_len = new;
	    start = opt;
	  }
      }
  
  if (enc_len != 0 &&
      (p = free_space(mess, end, encap, enc_len + 1)))
    {
      for (; start; start = start->next)
	if (start->flags & flag)
	  {
	    len = do_opt(start, p + 2, NULL, null_term);
	    *(p++) = start->opt;
	    *(p++) = len;
	    p += len;
	  }
      *p = OPTION_END;
    }
}

static void pxe_misc(struct dhcp_packet *mess, unsigned char *end, unsigned char *uuid)
{
  unsigned char *p;

  option_put_string(mess, end, OPTION_VENDOR_ID, "PXEClient", 0);
  if (uuid && (p = free_space(mess, end, OPTION_PXE_UUID, 17)))
    memcpy(p, uuid, 17);
}

static int prune_vendor_opts(struct dhcp_netid *netid)
{
  int force = 0;
  struct dhcp_opt *opt;

  
  for (opt = daemon->dhcp_opts; opt; opt = opt->next)
    if (opt->flags & DHOPT_VENDOR_MATCH)
      {
	if (!match_netid(opt->netid, netid, 1))
	  opt->flags &= ~DHOPT_VENDOR_MATCH;
	else if (opt->flags & DHOPT_FORCE)
	  force = 1;
      }
  return force;
}

static struct dhcp_opt *pxe_opts(int pxe_arch, struct dhcp_netid *netid)
{
#define NUM_OPTS 4  

  unsigned  char *p, *q;
  struct pxe_service *service;
  static struct dhcp_opt *o, *ret;
  int i, j = NUM_OPTS - 1;
  
  
  static unsigned char discovery_control;
  static unsigned char fake_prompt[] = { 0, 'P', 'X', 'E' }; 
  static struct dhcp_opt *fake_opts = NULL;
  
  discovery_control = 2;
  
  ret = daemon->dhcp_opts;
  
  if (!fake_opts && !(fake_opts = whine_malloc(NUM_OPTS * sizeof(struct dhcp_opt))))
    return ret;

  for (i = 0; i < NUM_OPTS; i++)
    {
      fake_opts[i].flags = DHOPT_VENDOR_MATCH;
      fake_opts[i].netid = NULL;
      fake_opts[i].next = i == (NUM_OPTS - 1) ? ret : &fake_opts[i+1];
    }
  
  
  p = (unsigned char *)daemon->dhcp_buff;
  q = (unsigned char *)daemon->dhcp_buff2;

  for (i = 0, service = daemon->pxe_services; service; service = service->next)
    if (pxe_arch == service->CSA && match_netid(service->netid, netid, 1))
      {
	size_t len = strlen(service->menu);
	if (p - (unsigned char *)daemon->dhcp_buff + len + 3 < 253)
	  {
	    *(p++) = service->type >> 8;
	    *(p++) = service->type;
	    *(p++) = len;
	    memcpy(p, service->menu, len);
	    p += len;
	    i++;
	  }
	else
	  {
	  toobig:
	    my_syslog(MS_DHCP | LOG_ERR, _("PXE menu too large"));
	    return daemon->dhcp_opts;
	  }
	
	if (!service->basename)
	  {
	    if (service->server.s_addr != 0)
	      {
		if (q - (unsigned char *)daemon->dhcp_buff2 + 3 + INADDRSZ >= 253)
		  goto toobig;
		
		
		*(q++) = service->type >> 8;
		*(q++) = service->type;
		*(q++) = 1;
		
		memcpy(q, &service->server.s_addr, INADDRSZ);
		q += INADDRSZ;
	      }
	    else if (service->type != 0)
	      discovery_control = 0;
	  }	  
      }

  
  fake_prompt[0] = (i > 1) ? 255 : 0;
  
  if (i == 0)
    discovery_control = 8; 
  else
    {
      ret = &fake_opts[j--];
      ret->len = p - (unsigned char *)daemon->dhcp_buff;
      ret->val = (unsigned char *)daemon->dhcp_buff;
      ret->opt = SUBOPT_PXE_MENU;

      if (q - (unsigned char *)daemon->dhcp_buff2 != 0)
	{
	  ret = &fake_opts[j--]; 
	  ret->len = q - (unsigned char *)daemon->dhcp_buff2;
	  ret->val = (unsigned char *)daemon->dhcp_buff2;
	  ret->opt = SUBOPT_PXE_SERVERS;
	}
    }

  for (o = daemon->dhcp_opts; o; o = o->next)
    if ((o->flags & DHOPT_VENDOR_MATCH) && o->opt == SUBOPT_PXE_MENU_PROMPT)
      break;
  
  if (!o)
    {
      ret = &fake_opts[j--]; 
      ret->len = sizeof(fake_prompt);
      ret->val = fake_prompt;
      ret->opt = SUBOPT_PXE_MENU_PROMPT;
    }
  
  if (discovery_control != 0)
    {
      ret = &fake_opts[j--]; 
      ret->len = 1;
      ret->opt = SUBOPT_PXE_DISCOVERY;
      ret->val= &discovery_control;
    }

  return ret;
}
  
static void clear_packet(struct dhcp_packet *mess, unsigned char *end)
{
  memset(mess->sname, 0, sizeof(mess->sname));
  memset(mess->file, 0, sizeof(mess->file));
  memset(&mess->options[0] + sizeof(u32), 0, end - (&mess->options[0] + sizeof(u32)));
  mess->siaddr.s_addr = 0;
}

struct dhcp_boot *find_boot(struct dhcp_netid *netid)
{
  struct dhcp_boot *boot;

  
  for (boot = daemon->boot_config; boot; boot = boot->next)
    if (match_netid(boot->netid, netid, 0))
      break;
  if (!boot)
    
    for (boot = daemon->boot_config; boot; boot = boot->next)
      if (match_netid(boot->netid, netid, 1))
	break;

  return boot;
}

static void do_options(struct dhcp_context *context,
		       struct dhcp_packet *mess,
		       unsigned char *end, 
		       unsigned char *req_options,
		       char *hostname, 
		       char *domain, char *config_domain,
		       struct dhcp_netid *netid,
		       struct in_addr subnet_addr,
		       unsigned char fqdn_flags,
		       int null_term, int pxe_arch,
		       unsigned char *uuid)
{
  struct dhcp_opt *opt, *config_opts = daemon->dhcp_opts;
  struct dhcp_boot *boot;
  unsigned char *p;
  int i, len, force_encap = 0;
  unsigned char f0 = 0, s0 = 0;
  int done_file = 0, done_server = 0;

  if (config_domain && (!domain || !hostname_isequal(domain, config_domain)))
    my_syslog(MS_DHCP | LOG_WARNING, _("Ignoring domain %s for DHCP host name %s"), config_domain, hostname);
  
  
  if ((daemon->options & OPT_LOG_OPTS) && req_options)
    {
      char *q = daemon->namebuff;
      for (i = 0; req_options[i] != OPTION_END; i++)
	{
	  char *s = option_string(req_options[i], NULL, NULL);
	  q += snprintf(q, MAXDNAME - (q - daemon->namebuff),
			"%d%s%s%s", 
			req_options[i],
			s ? ":" : "",
			s ? s : "", 
			req_options[i+1] == OPTION_END ? "" : ", ");
	  if (req_options[i+1] == OPTION_END || (q - daemon->namebuff) > 40)
	    {
	      q = daemon->namebuff;
	      my_syslog(MS_DHCP | LOG_INFO, _("%u requested options: %s"), ntohl(mess->xid), daemon->namebuff);
	    }
	}
    }
      
  if (context)
    mess->siaddr = context->local;
  
  if ((boot = find_boot(netid)))
    {
      if (boot->sname)
	{	  
	  if (!(daemon->options & OPT_NO_OVERRIDE) &&
	      req_options && 
	      in_list(req_options, OPTION_SNAME))
	    option_put_string(mess, end, OPTION_SNAME, boot->sname, 1);
	  else
	    strncpy((char *)mess->sname, boot->sname, sizeof(mess->sname)-1);
	}
      
      if (boot->file)
	{
	  if (!(daemon->options & OPT_NO_OVERRIDE) &&
	      req_options && 
	      in_list(req_options, OPTION_FILENAME))
	    option_put_string(mess, end, OPTION_FILENAME, boot->file, 1);
	  else
	    strncpy((char *)mess->file, boot->file, sizeof(mess->file)-1);
	}
      
      if (boot->next_server.s_addr)
	mess->siaddr = boot->next_server;
    }
  else
    {
      if ((!req_options || !in_list(req_options, OPTION_FILENAME)) && mess->file[0] == 0 &&
	  (opt = option_find2(netid, config_opts, OPTION_FILENAME)) && !(opt->flags & DHOPT_FORCE))
	{
	  strncpy((char *)mess->file, (char *)opt->val, sizeof(mess->file)-1);
	  done_file = 1;
	}
      
      if ((!req_options || !in_list(req_options, OPTION_SNAME)) &&
	  (opt = option_find2(netid, config_opts, OPTION_SNAME)) && !(opt->flags & DHOPT_FORCE))
	{
	  strncpy((char *)mess->sname, (char *)opt->val, sizeof(mess->sname)-1);
	  done_server = 1;
	}
      
      if ((opt = option_find2(netid, config_opts, OPTION_END)))
	mess->siaddr.s_addr = ((struct in_addr *)opt->val)->s_addr;	
    }
        

  if (!req_options || (daemon->options & OPT_NO_OVERRIDE))
    {
      f0 = mess->file[0];
      mess->file[0] = 1;
      s0 = mess->sname[0];
      mess->sname[0] = 1;
    }
      
  if (mess->file[0] == 0 || mess->sname[0] == 0)
    end -= 3;

  
  if (subnet_addr.s_addr)
    option_put(mess, end, OPTION_SUBNET_SELECT, INADDRSZ, ntohl(subnet_addr.s_addr));
  
  
  if (context)
    {
      if (!option_find2(netid, config_opts, OPTION_NETMASK))
	option_put(mess, end, OPTION_NETMASK, INADDRSZ, ntohl(context->netmask.s_addr));
  
      if (context->broadcast.s_addr &&
	  !option_find2(netid, config_opts, OPTION_BROADCAST))
	option_put(mess, end, OPTION_BROADCAST, INADDRSZ, ntohl(context->broadcast.s_addr));
      
      if (context->router.s_addr &&
	  in_list(req_options, OPTION_ROUTER) &&
	  !option_find2(netid, config_opts, OPTION_ROUTER))
	option_put(mess, end, OPTION_ROUTER, INADDRSZ, ntohl(context->router.s_addr));
      
      if (in_list(req_options, OPTION_DNSSERVER) &&
	  !option_find2(netid, config_opts, OPTION_DNSSERVER))
	option_put(mess, end, OPTION_DNSSERVER, INADDRSZ, ntohl(context->local.s_addr));
    }

  if (domain && in_list(req_options, OPTION_DOMAINNAME) && 
      !option_find2(netid, config_opts, OPTION_DOMAINNAME))
    option_put_string(mess, end, OPTION_DOMAINNAME, domain, null_term);
 
  
  if (hostname)
    {
      if (in_list(req_options, OPTION_HOSTNAME) &&
	  !option_find2(netid, config_opts, OPTION_HOSTNAME))
	option_put_string(mess, end, OPTION_HOSTNAME, hostname, null_term);
      
      if (fqdn_flags != 0)
	{
	  len = strlen(hostname) + 3;
	  
	  if (fqdn_flags & 0x04)
	    len += 2;
	  else if (null_term)
	    len++;

	  if (domain)
	    len += strlen(domain) + 1;
	  
	  if ((p = free_space(mess, end, OPTION_CLIENT_FQDN, len)))
	    {
	      *(p++) = fqdn_flags;
	      *(p++) = 255;
	      *(p++) = 255;

	      if (fqdn_flags & 0x04)
		{
		  p = do_rfc1035_name(p, hostname);
		  if (domain)
		    p = do_rfc1035_name(p, domain);
		  *p++ = 0;
		}
	      else
		{
		  memcpy(p, hostname, strlen(hostname));
		  p += strlen(hostname);
		  if (domain)
		    {
		      *(p++) = '.';
		      memcpy(p, domain, strlen(domain));
		      p += strlen(domain);
		    }
		  if (null_term)
		    *(p++) = 0;
		}
	    }
	}
    }      

  for (opt = config_opts; opt; opt = opt->next)
    {
      int optno = opt->opt;

      
      if (!(opt->flags & DHOPT_FORCE) && !in_list(req_options, optno))
	continue;
      
      
      if (optno == OPTION_CLIENT_FQDN ||
	  optno == OPTION_MAXMESSAGE ||
	  optno == OPTION_OVERLOAD ||
	  optno == OPTION_PAD ||
	  optno == OPTION_END)
	continue;

      if (optno == OPTION_SNAME && done_server)
	continue;

      if (optno == OPTION_FILENAME && done_file)
	continue;
      
      
      if (opt != option_find2(netid, config_opts, optno))
	continue;
      
      if (opt->len == 0 && 
	  (optno == OPTION_NETMASK ||
	   optno == OPTION_BROADCAST ||
	   optno == OPTION_ROUTER ||
	   optno == OPTION_DNSSERVER || 
	   optno == OPTION_DOMAINNAME ||
	   optno == OPTION_HOSTNAME))
	continue;

      
      if (pxe_arch != -1 && optno == OPTION_VENDOR_ID)
	continue;
      
      
      len = do_opt(opt, NULL, context, 
		   (optno == OPTION_SNAME || optno == OPTION_FILENAME) ? 1 : null_term);

      if ((p = free_space(mess, end, optno, len)))
	{
	  do_opt(opt, p, context, 
		 (optno == OPTION_SNAME || optno == OPTION_FILENAME) ? 1 : null_term);
	  
	  if (optno == OPTION_VENDOR_ID)
	    match_vendor_opts(p - 2, config_opts);
	}  
    }

  for (opt = config_opts; opt; opt = opt->next)
    opt->flags &= ~DHOPT_ENCAP_DONE;
  
  for (opt = config_opts; opt; opt = opt->next)
    if ((opt->flags & (DHOPT_ENCAPSULATE | DHOPT_ENCAP_DONE)) ==  DHOPT_ENCAPSULATE)
      {
	struct dhcp_opt *o;
	int found = 0;
	
	for (o = config_opts; o; o = o->next)
	  {
	    o->flags &= ~DHOPT_ENCAP_MATCH;
	    if ((o->flags & DHOPT_ENCAPSULATE) && opt->u.encap == o->u.encap)
	      {
		o->flags |= DHOPT_ENCAP_DONE;
		if (match_netid(o->netid, netid, 1) &&
		    (o->flags & DHOPT_FORCE || in_list(req_options, o->u.encap)))
		  {
		    o->flags |= DHOPT_ENCAP_MATCH;
		    found = 1;
		  }
	      }
	  }
	
	if (found)
	  do_encap_opts(config_opts, opt->u.encap, DHOPT_ENCAP_MATCH, mess, end, null_term);
      }

  
  force_encap = prune_vendor_opts(netid);
  if (in_list(req_options, OPTION_VENDOR_CLASS_OPT))
    force_encap = 1;

  if (pxe_arch != -1)
    {
      pxe_misc(mess, end, uuid);
      config_opts = pxe_opts(pxe_arch, netid);
    }

  if (force_encap)
    do_encap_opts(config_opts, OPTION_VENDOR_CLASS_OPT, DHOPT_VENDOR_MATCH, mess, end, null_term);
  
   
  if (!req_options || (daemon->options & OPT_NO_OVERRIDE))
    {
      mess->file[0] = f0;
      mess->sname[0] = s0;
    }
}

#endif
