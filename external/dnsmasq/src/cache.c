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

static struct crec *cache_head = NULL, *cache_tail = NULL, **hash_table = NULL;
#ifdef HAVE_DHCP
static struct crec *dhcp_spare = NULL;
#endif
static struct crec *new_chain = NULL;
static int cache_inserted = 0, cache_live_freed = 0, insert_error;
static union bigname *big_free = NULL;
static int bignames_left, hash_size;
static int uid = 0;
static char *addrbuff = NULL;

static const struct {
  unsigned int type;
  const char * const name;
} typestr[] = {
  { 1,   "A" },
  { 2,   "NS" },
  { 5,   "CNAME" },
  { 6,   "SOA" },
  { 10,  "NULL" },
  { 11,  "WKS" },
  { 12,  "PTR" },
  { 13,  "HINFO" },	
  { 15,  "MX" },
  { 16,  "TXT" },
  { 22,  "NSAP" },
  { 23,  "NSAP_PTR" },
  { 24,  "SIG" },
  { 25,  "KEY" },
  { 28,  "AAAA" },
  { 33,  "SRV" },
  { 35,  "NAPTR" },
  { 36,  "KX" },
  { 37,  "CERT" },
  { 38,  "A6" },
  { 39,  "DNAME" },
  { 41,  "OPT" },
  { 48,  "DNSKEY" },
  { 249, "TKEY" },
  { 250, "TSIG" },
  { 251, "IXFR" },
  { 252, "AXFR" },
  { 253, "MAILB" },
  { 254, "MAILA" },
  { 255, "ANY" }
};

static void cache_free(struct crec *crecp);
static void cache_unlink(struct crec *crecp);
static void cache_link(struct crec *crecp);
static void rehash(int size);
static void cache_hash(struct crec *crecp);

void cache_init(void)
{
  struct crec *crecp;
  int i;

  if (daemon->options & OPT_LOG)
    addrbuff = safe_malloc(ADDRSTRLEN);
  
  bignames_left = daemon->cachesize/10;
  
  if (daemon->cachesize > 0)
    {
      crecp = safe_malloc(daemon->cachesize*sizeof(struct crec));
      
      for (i=0; i < daemon->cachesize; i++, crecp++)
	{
	  cache_link(crecp);
	  crecp->flags = 0;
	  crecp->uid = uid++;
	}
    }
  
  
  rehash(daemon->cachesize);
}

static void rehash(int size)
{
  struct crec **new, **old, *p, *tmp;
  int i, new_size, old_size;

  
  for (new_size = 64; new_size < size/10; new_size = new_size << 1);
  
  
  if (!hash_table)
    new = safe_malloc(new_size * sizeof(struct crec *));
  else if (new_size <= hash_size || !(new = whine_malloc(new_size * sizeof(struct crec *))))
    return;

  for(i = 0; i < new_size; i++)
    new[i] = NULL;

  old = hash_table;
  old_size = hash_size;
  hash_table = new;
  hash_size = new_size;
  
  if (old)
    {
      for (i = 0; i < old_size; i++)
	for (p = old[i]; p ; p = tmp)
	  {
	    tmp = p->hash_next;
	    cache_hash(p);
	  }
      free(old);
    }
}
  
static struct crec **hash_bucket(char *name)
{
  unsigned int c, val = 017465; 
  const unsigned char *mix_tab = (const unsigned char*)typestr; 

  while((c = (unsigned char) *name++))
    {
      
      if (c >= 'A' && c <= 'Z')
	c += 'a' - 'A';
      val = ((val << 7) | (val >> (32 - 7))) + (mix_tab[(val + c) & 0x3F] ^ c);
    } 
  
  
  return hash_table + ((val ^ (val >> 16)) & (hash_size - 1));
}

static void cache_hash(struct crec *crecp)
{

  struct crec **up = hash_bucket(cache_get_name(crecp));

  if (!(crecp->flags & F_REVERSE))
    {
      while (*up && ((*up)->flags & F_REVERSE))
	up = &((*up)->hash_next); 
      
      if (crecp->flags & F_IMMORTAL)
	while (*up && !((*up)->flags & F_IMMORTAL))
	  up = &((*up)->hash_next);
    }
  crecp->hash_next = *up;
  *up = crecp;
}
 
static void cache_free(struct crec *crecp)
{
  crecp->flags &= ~F_FORWARD;
  crecp->flags &= ~F_REVERSE;
  crecp->uid = uid++; 
  
  if (cache_tail)
    cache_tail->next = crecp;
  else
    cache_head = crecp;
  crecp->prev = cache_tail;
  crecp->next = NULL;
  cache_tail = crecp;
  
  
  if (crecp->flags & F_BIGNAME)
    {
      crecp->name.bname->next = big_free;
      big_free = crecp->name.bname;
      crecp->flags &= ~F_BIGNAME;
    }
}    

static void cache_link(struct crec *crecp)
{
  if (cache_head) 
    cache_head->prev = crecp;
  crecp->next = cache_head;
  crecp->prev = NULL;
  cache_head = crecp;
  if (!cache_tail)
    cache_tail = crecp;
}

 
static void cache_unlink (struct crec *crecp)
{
  if (crecp->prev)
    crecp->prev->next = crecp->next;
  else
    cache_head = crecp->next;

  if (crecp->next)
    crecp->next->prev = crecp->prev;
  else
    cache_tail = crecp->prev;
}

char *cache_get_name(struct crec *crecp)
{
  if (crecp->flags & F_BIGNAME)
    return crecp->name.bname->name;
  else if (crecp->flags & (F_DHCP | F_CONFIG)) 
    return crecp->name.namep;
  
  return crecp->name.sname;
}

static int is_outdated_cname_pointer(struct crec *crecp)
{
  if (!(crecp->flags & F_CNAME))
    return 0;
  
  if (crecp->addr.cname.cache && crecp->addr.cname.uid == crecp->addr.cname.cache->uid)
    return 0;
  
  return 1;
}

static int is_expired(time_t now, struct crec *crecp)
{
  if (crecp->flags & F_IMMORTAL)
    return 0;

  if (difftime(now, crecp->ttd) < 0)
    return 0;
  
  return 1;
}

static int cache_scan_free(char *name, struct all_addr *addr, time_t now, unsigned short flags)
{
 
  struct crec *crecp, **up;
  
  if (flags & F_FORWARD)
    {
      for (up = hash_bucket(name), crecp = *up; crecp; crecp = crecp->hash_next)
	if (is_expired(now, crecp) || is_outdated_cname_pointer(crecp))
	  { 
	    *up = crecp->hash_next;
	    if (!(crecp->flags & (F_HOSTS | F_DHCP)))
	      {
		cache_unlink(crecp);
		cache_free(crecp);
	      }
	  } 
	else if ((crecp->flags & F_FORWARD) && 
		 ((flags & crecp->flags & (F_IPV4 | F_IPV6)) || ((crecp->flags | flags) & F_CNAME)) &&
		 hostname_isequal(cache_get_name(crecp), name))
	  {
	    if (crecp->flags & (F_HOSTS | F_DHCP))
	      return 0;
	    *up = crecp->hash_next;
	    cache_unlink(crecp);
	    cache_free(crecp);
	  }
	else
	  up = &crecp->hash_next;
    }
  else
    {
      int i;
#ifdef HAVE_IPV6
      int addrlen = (flags & F_IPV6) ? IN6ADDRSZ : INADDRSZ;
#else
      int addrlen = INADDRSZ;
#endif 
      for (i = 0; i < hash_size; i++)
	for (crecp = hash_table[i], up = &hash_table[i]; 
	     crecp && ((crecp->flags & F_REVERSE) || !(crecp->flags & F_IMMORTAL));
	     crecp = crecp->hash_next)
	  if (is_expired(now, crecp))
	    {
	      *up = crecp->hash_next;
	      if (!(crecp->flags & (F_HOSTS | F_DHCP)))
		{ 
		  cache_unlink(crecp);
		  cache_free(crecp);
		}
	    }
	  else if (!(crecp->flags & (F_HOSTS | F_DHCP)) &&
		   (flags & crecp->flags & F_REVERSE) && 
		   (flags & crecp->flags & (F_IPV4 | F_IPV6)) &&
		   memcmp(&crecp->addr.addr, addr, addrlen) == 0)
	    {
	      *up = crecp->hash_next;
	      cache_unlink(crecp);
	      cache_free(crecp);
	    }
	  else
	    up = &crecp->hash_next;
    }
  
  return 1;
}


void cache_start_insert(void)
{
  while (new_chain)
    {
      struct crec *tmp = new_chain->next;
      cache_free(new_chain);
      new_chain = tmp;
    }
  new_chain = NULL;
  insert_error = 0;
}
 
struct crec *cache_insert(char *name, struct all_addr *addr, 
			  time_t now,  unsigned long ttl, unsigned short flags)
{
  struct crec *new;
  union bigname *big_name = NULL;
  int freed_all = flags & F_REVERSE;
  int free_avail = 0;

  log_query(flags | F_UPSTREAM, name, addr, NULL);

  
  flags &= ~F_CONFIG;

  
  if (insert_error)
    return NULL;

  if (!cache_scan_free(name, addr, now, flags))
    {
      insert_error = 1;
      return NULL;
    }
  
  
  while (1) {
    if (!(new = cache_tail)) 
      {
	insert_error = 1;
	return NULL;
      }
    
    
    if (new->flags & (F_FORWARD | F_REVERSE))
      { 
	if (free_avail)
	  {
	    insert_error = 1;
	    return NULL;
	  }
		
	if (freed_all)
	  {
	    free_avail = 1; 
	    cache_scan_free(cache_get_name(new), &new->addr.addr, now, new->flags);
	    cache_live_freed++;
	  }
	else
	  {
	    cache_scan_free(NULL, NULL, now, 0);
	    freed_all = 1;
	  }
	continue;
      }
 
    if (name && (strlen(name) > SMALLDNAME-1))
      {
	if (big_free)
	  { 
	    big_name = big_free;
	    big_free = big_free->next;
	  }
	else if (!bignames_left ||
		 !(big_name = (union bigname *)whine_malloc(sizeof(union bigname))))
	  {
	    insert_error = 1;
	    return NULL;
	  }
	else
	  bignames_left--;
	
      }

    
    cache_unlink(new);
    break;
  }
  
  new->flags = flags;
  if (big_name)
    {
      new->name.bname = big_name;
      new->flags |= F_BIGNAME;
    }

  if (name)
    strcpy(cache_get_name(new), name);
  else
    *cache_get_name(new) = 0;

  if (addr)
    new->addr.addr = *addr;
  else
    new->addr.cname.cache = NULL;
  
  new->ttd = now + (time_t)ttl;
  new->next = new_chain;
  new_chain = new;

  return new;
}

void cache_end_insert(void)
{
  if (insert_error)
    return;
  
  while (new_chain)
    { 
      struct crec *tmp = new_chain->next;
      
      if (is_outdated_cname_pointer(new_chain))
	cache_free(new_chain);
      else
	{
	  cache_hash(new_chain);
	  cache_link(new_chain);
	  cache_inserted++;
	}
      new_chain = tmp;
    }
  new_chain = NULL;
}

struct crec *cache_find_by_name(struct crec *crecp, char *name, time_t now, unsigned short prot)
{
  struct crec *ans;

  if (crecp) 
    ans = crecp->next;
  else
    {
      struct crec *next, **up, **insert = NULL, **chainp = &ans;
      int ins_flags = 0;
      
      for (up = hash_bucket(name), crecp = *up; crecp; crecp = next)
	{
	  next = crecp->hash_next;
	  
	  if (!is_expired(now, crecp) && !is_outdated_cname_pointer(crecp))
	    {
	      if ((crecp->flags & F_FORWARD) && 
		  (crecp->flags & prot) &&
		  hostname_isequal(cache_get_name(crecp), name))
		{
		  if (crecp->flags & (F_HOSTS | F_DHCP))
		    {
		      *chainp = crecp;
		      chainp = &crecp->next;
		    }
		  else
		    {
		      cache_unlink(crecp);
		      cache_link(crecp);
		    }
	      	      
		  if (insert && (crecp->flags & (F_REVERSE | F_IMMORTAL)) == ins_flags)
		    {
		      *up = crecp->hash_next;
		      crecp->hash_next = *insert;
		      *insert = crecp;
		      insert = &crecp->hash_next;
		    }
		  else
		    {
		      if (!insert)
			{
			  insert = up;
			  ins_flags = crecp->flags & (F_REVERSE | F_IMMORTAL);
			}
		      up = &crecp->hash_next; 
		    }
		}
	      else
		
		up = &crecp->hash_next; 
	    }
	  else
	    {
	      
	      *up = crecp->hash_next;
	      if (!(crecp->flags & (F_HOSTS | F_DHCP)))
		{ 
		  cache_unlink(crecp);
		  cache_free(crecp);
		}
	    }
	}
	  
      *chainp = cache_head;
    }

  if (ans && 
      (ans->flags & F_FORWARD) &&
      (ans->flags & prot) &&
      hostname_isequal(cache_get_name(ans), name))
    return ans;
  
  return NULL;
}

struct crec *cache_find_by_addr(struct crec *crecp, struct all_addr *addr, 
				time_t now, unsigned short prot)
{
  struct crec *ans;
#ifdef HAVE_IPV6
  int addrlen = (prot == F_IPV6) ? IN6ADDRSZ : INADDRSZ;
#else
  int addrlen = INADDRSZ;
#endif
  
  if (crecp) 
    ans = crecp->next;
  else
    {  
       int i;
       struct crec **up, **chainp = &ans;
       
       for (i=0; i<hash_size; i++)
	 for (crecp = hash_table[i], up = &hash_table[i]; 
	      crecp && (crecp->flags & F_REVERSE);
	      crecp = crecp->hash_next)
	   if (!is_expired(now, crecp))
	     {      
	       if ((crecp->flags & prot) &&
		   memcmp(&crecp->addr.addr, addr, addrlen) == 0)
		 {	    
		   if (crecp->flags & (F_HOSTS | F_DHCP))
		     {
		       *chainp = crecp;
		       chainp = &crecp->next;
		     }
		   else
		     {
		       cache_unlink(crecp);
		       cache_link(crecp);
		     }
		 }
	       up = &crecp->hash_next;
	     }
	   else
	     {
	       *up = crecp->hash_next;
	       if (!(crecp->flags & (F_HOSTS | F_DHCP)))
		 {
		   cache_unlink(crecp);
		   cache_free(crecp);
		 }
	     }
       
       *chainp = cache_head;
    }
  
  if (ans && 
      (ans->flags & F_REVERSE) &&
      (ans->flags & prot) &&
      memcmp(&ans->addr.addr, addr, addrlen) == 0)
    return ans;
  
  return NULL;
}

static void add_hosts_entry(struct crec *cache, struct all_addr *addr, int addrlen, 
			    unsigned short flags, int index, int addr_dup)
{
  struct crec *lookup = cache_find_by_name(NULL, cache->name.sname, 0, flags & (F_IPV4 | F_IPV6));
  int i, nameexists = 0;
  struct cname *a;

  
  if (lookup && (lookup->flags & F_HOSTS))
    {
      nameexists = 1;
      if (memcmp(&lookup->addr.addr, addr, addrlen) == 0)
	{
	  free(cache);
	  return;
	}
    }
  
  
  if (addr_dup)
    flags &= ~F_REVERSE;
  else
    for (i=0; i<hash_size; i++)
      {
	for (lookup = hash_table[i]; lookup; lookup = lookup->hash_next)
	  if ((lookup->flags & F_HOSTS) && 
	      (lookup->flags & flags & (F_IPV4 | F_IPV6)) &&
	      memcmp(&lookup->addr.addr, addr, addrlen) == 0)
	    {
	      flags &= ~F_REVERSE;
	      break;
	    }
	if (lookup)
	  break;
      }
  
  cache->flags = flags;
  cache->uid = index;
  memcpy(&cache->addr.addr, addr, addrlen);
  cache_hash(cache);
  
  
  if (!nameexists)
    for (a = daemon->cnames; a; a = a->next)
      if (hostname_isequal(cache->name.sname, a->target) &&
	  (lookup = whine_malloc(sizeof(struct crec))))
	{
	  lookup->flags = F_FORWARD | F_IMMORTAL | F_CONFIG | F_HOSTS | F_CNAME;
	  lookup->name.namep = a->alias;
	  lookup->addr.cname.cache = cache;
	  lookup->addr.cname.uid = index;
	  cache_hash(lookup);
	}
}

static int eatspace(FILE *f)
{
  int c, nl = 0;

  while (1)
    {
      if ((c = getc(f)) == '#')
	while (c != '\n' && c != EOF)
	  c = getc(f);
      
      if (c == EOF)
	return 1;

      if (!isspace(c))
	{
	  ungetc(c, f);
	  return nl;
	}

      if (c == '\n')
	nl = 1;
    }
}
	 
static int gettok(FILE *f, char *token)
{
  int c, count = 0;
 
  while (1)
    {
      if ((c = getc(f)) == EOF)
	return (count == 0) ? EOF : 1;

      if (isspace(c) || c == '#')
	{
	  ungetc(c, f);
	  return eatspace(f);
	}
      
      if (count < (MAXDNAME - 1))
	{
	  token[count++] = c;
	  token[count] = 0;
	}
    }
}

static int read_hostsfile(char *filename, int index, int cache_size)
{  
  FILE *f = fopen(filename, "r");
  char *token = daemon->namebuff, *domain_suffix = NULL;
  int addr_count = 0, name_count = cache_size, lineno = 0;
  unsigned short flags = 0, saved_flags = 0;
  struct all_addr addr, saved_addr;
  int atnl, addrlen = 0, addr_dup;

  if (!f)
    {
      my_syslog(LOG_ERR, _("failed to load names from %s: %s"), filename, strerror(errno));
      return 0;
    }
  
  eatspace(f);
  
  while ((atnl = gettok(f, token)) != EOF)
    {
      addr_dup = 0;
      lineno++;
      
#ifdef HAVE_IPV6      
      if (inet_pton(AF_INET, token, &addr) > 0)
	{
	  flags = F_HOSTS | F_IMMORTAL | F_FORWARD | F_REVERSE | F_IPV4;
	  addrlen = INADDRSZ;
	  domain_suffix = get_domain(addr.addr.addr4);
	}
      else if (inet_pton(AF_INET6, token, &addr) > 0)
	{
	  flags = F_HOSTS | F_IMMORTAL | F_FORWARD | F_REVERSE | F_IPV6;
	  addrlen = IN6ADDRSZ;
	  domain_suffix = daemon->domain_suffix;
	}
#else 
      if ((addr.addr.addr4.s_addr = inet_addr(token)) != (in_addr_t) -1)
	{
	  flags = F_HOSTS | F_IMMORTAL | F_FORWARD | F_REVERSE | F_IPV4;
	  addrlen = INADDRSZ;
	  domain_suffix = get_domain(addr.addr.addr4);
	}
#endif
      else
	{
	  my_syslog(LOG_ERR, _("bad address at %s line %d"), filename, lineno); 
	  while (atnl == 0)
	    atnl = gettok(f, token);
	  continue;
	}
      
      if (saved_flags == flags && memcmp(&addr, &saved_addr, addrlen) == 0)
	addr_dup = 1;
      else
	{
	  saved_flags = flags;
	  saved_addr = addr;
	}
      
      addr_count++;
      
      
      if ((name_count - cache_size) > 1000)
	{
	  rehash(name_count);
	  cache_size = name_count;
	} 
      
      while (atnl == 0)
	{
	  struct crec *cache;
	  int fqdn, nomem;
	  char *canon;
	  
	  if ((atnl = gettok(f, token)) == EOF)
	    break;

	  fqdn = !!strchr(token, '.');

	  if ((canon = canonicalise(token, &nomem)))
	    {
	      
	      if ((daemon->options & OPT_EXPAND) && domain_suffix && !fqdn && 
		  (cache = whine_malloc(sizeof(struct crec) + 
					strlen(canon)+2+strlen(domain_suffix)-SMALLDNAME)))
		{
		  strcpy(cache->name.sname, canon);
		  strcat(cache->name.sname, ".");
		  strcat(cache->name.sname, domain_suffix);
		  add_hosts_entry(cache, &addr, addrlen, flags, index, addr_dup);
		  addr_dup = 1;
		  name_count++;
		}
	      if ((cache = whine_malloc(sizeof(struct crec) + strlen(canon)+1-SMALLDNAME)))
		{
		  strcpy(cache->name.sname, canon);
		  add_hosts_entry(cache, &addr, addrlen, flags, index, addr_dup);
		  name_count++;
		}
	      free(canon);
	      
	    }
	  else if (!nomem)
	    my_syslog(LOG_ERR, _("bad name at %s line %d"), filename, lineno); 
	}
    } 

  fclose(f);
  rehash(name_count);
  
  my_syslog(LOG_INFO, _("read %s - %d addresses"), filename, addr_count);
  
  return name_count;
}
	    
void cache_reload(void)
{
  struct crec *cache, **up, *tmp;
  int i, total_size = daemon->cachesize;
  struct hostsfile *ah;

  cache_inserted = cache_live_freed = 0;
  
  for (i=0; i<hash_size; i++)
    for (cache = hash_table[i], up = &hash_table[i]; cache; cache = tmp)
      {
	tmp = cache->hash_next;
	if (cache->flags & F_HOSTS)
	  {
	    *up = cache->hash_next;
	    free(cache);
	  }
	else if (!(cache->flags & F_DHCP))
	  {
	    *up = cache->hash_next;
	    if (cache->flags & F_BIGNAME)
	      {
		cache->name.bname->next = big_free;
		big_free = cache->name.bname;
	      }
	    cache->flags = 0;
	  }
	else
	  up = &cache->hash_next;
      }
  
  if ((daemon->options & OPT_NO_HOSTS) && !daemon->addn_hosts)
    {
      if (daemon->cachesize > 0)
	my_syslog(LOG_INFO, _("cleared cache"));
      return;
    }

  if (!(daemon->options & OPT_NO_HOSTS))
    total_size = read_hostsfile(HOSTSFILE, 0, total_size);
  
  for (i = 0, ah = daemon->addn_hosts; ah; ah = ah->next)
    {
      if (i <= ah->index)
	i = ah->index + 1;

      if (ah->flags & AH_DIR)
	ah->flags |= AH_INACTIVE;
      else
	ah->flags &= ~AH_INACTIVE;
    }

  for (ah = daemon->addn_hosts; ah; ah = ah->next)
    if (!(ah->flags & AH_INACTIVE))
      {
	struct stat buf;
	if (stat(ah->fname, &buf) != -1 && S_ISDIR(buf.st_mode))
	  {
	    DIR *dir_stream;
	    struct dirent *ent;
	    
	    
	    ah->flags |= AH_INACTIVE;
	    
	    if (!(dir_stream = opendir(ah->fname)))
	      my_syslog(LOG_ERR, _("cannot access directory %s: %s"), 
			ah->fname, strerror(errno));
	    else
	      {
		while ((ent = readdir(dir_stream)))
		  {
		    size_t lendir = strlen(ah->fname);
		    size_t lenfile = strlen(ent->d_name);
		    struct hostsfile *ah1;
		    char *path;
		    
		    
		    if (lenfile == 0 || 
			ent->d_name[lenfile - 1] == '~' ||
			(ent->d_name[0] == '#' && ent->d_name[lenfile - 1] == '#') ||
			ent->d_name[0] == '.')
		      continue;
		    

		    for (ah1 = daemon->addn_hosts; ah1; ah1 = ah1->next)
		      {
			if (lendir < strlen(ah1->fname) &&
			    strstr(ah1->fname, ah->fname) == ah1->fname &&
			    ah1->fname[lendir] == '/' &&
			    strcmp(ah1->fname + lendir + 1, ent->d_name) == 0)
			  {
			    ah1->flags &= ~AH_INACTIVE;
			    break;
			  }
		      }
		    
		    
		    if (!ah1)
		      {
			if (!(ah1 = whine_malloc(sizeof(struct hostsfile))))
			  continue;
			
			if (!(path = whine_malloc(lendir + lenfile + 2)))
			  {
			    free(ah1);
			    continue;
			  }
		      	
			strcpy(path, ah->fname);
			strcat(path, "/");
			strcat(path, ent->d_name);
			ah1->fname = path;
			ah1->index = i++;
			ah1->flags = AH_DIR;
			ah1->next = daemon->addn_hosts;
			daemon->addn_hosts = ah1;
		      }
		    
		    
		    if ((ah1->flags & AH_DIR) && stat(ah1->fname, &buf) != -1 && !S_ISREG(buf.st_mode))
		      ah1->flags |= AH_INACTIVE; 

		  }
		closedir(dir_stream);
	      }
	  }
      }
	    
  for (ah = daemon->addn_hosts; ah; ah = ah->next)
    if (!(ah->flags & AH_INACTIVE))
      total_size = read_hostsfile(ah->fname, ah->index, total_size);
} 

char *get_domain(struct in_addr addr)
{
  struct cond_domain *c;

  for (c = daemon->cond_domain; c; c = c->next)
    if (ntohl(addr.s_addr) >= ntohl(c->start.s_addr) &&
        ntohl(addr.s_addr) <= ntohl(c->end.s_addr))
      return c->domain;

  return daemon->domain_suffix;
}

#ifdef HAVE_DHCP
void cache_unhash_dhcp(void)
{
  struct crec *cache, **up;
  int i;

  for (i=0; i<hash_size; i++)
    for (cache = hash_table[i], up = &hash_table[i]; cache; cache = cache->hash_next)
      if (cache->flags & F_DHCP)
	{
	  *up = cache->hash_next;
	  cache->next = dhcp_spare;
	  dhcp_spare = cache;
	}
      else
	up = &cache->hash_next;
}

void cache_add_dhcp_entry(char *host_name, 
			  struct in_addr *host_address, time_t ttd) 
{
  struct crec *crec = NULL, *aliasc;
  unsigned short flags =  F_DHCP | F_FORWARD | F_IPV4 | F_REVERSE;
  int in_hosts = 0;
  struct cname *a;
  
  while ((crec = cache_find_by_name(crec, host_name, 0, F_IPV4 | F_CNAME)))
    {
      
      if (crec->flags & F_HOSTS)
	{
	  if (crec->addr.addr.addr.addr4.s_addr != host_address->s_addr)
	    {
	      strcpy(daemon->namebuff, inet_ntoa(crec->addr.addr.addr.addr4));
	      my_syslog(LOG_WARNING, 
			_("not giving name %s to the DHCP lease of %s because "
			  "the name exists in %s with address %s"), 
			host_name, inet_ntoa(*host_address),
			record_source(crec->uid), daemon->namebuff);
	      return;
	    }
	  else
	    
	    in_hosts = 1;
	}
      else if (!(crec->flags & F_DHCP))
	{
	  cache_scan_free(host_name, NULL, 0, crec->flags & (F_IPV4 | F_CNAME | F_FORWARD));
	  
	  break;
	}
    }
  
   if (in_hosts)
    return;

   if ((crec = cache_find_by_addr(NULL, (struct all_addr *)host_address, 0, F_IPV4)))
     {
       if (crec->flags & F_NEG)
	 cache_scan_free(NULL, (struct all_addr *)host_address, 0, F_IPV4 | F_REVERSE);
       else
	 
	 flags &= ~F_REVERSE;
     }
   
   if ((crec = dhcp_spare))
    dhcp_spare = dhcp_spare->next;
  else 
    crec = whine_malloc(sizeof(struct crec));
  
  if (crec) 
    {
      crec->flags = flags;
      if (ttd == 0)
	crec->flags |= F_IMMORTAL;
      else
	crec->ttd = ttd;
      crec->addr.addr.addr.addr4 = *host_address;
      crec->name.namep = host_name;
      crec->uid = uid++;
      cache_hash(crec);

      for (a = daemon->cnames; a; a = a->next)
	if (hostname_isequal(host_name, a->target))
	  {
	    if ((aliasc = dhcp_spare))
	      dhcp_spare = dhcp_spare->next;
	    else 
	      aliasc = whine_malloc(sizeof(struct crec));
	    
	    if (aliasc)
	      {
		aliasc->flags = F_FORWARD | F_CONFIG | F_DHCP | F_CNAME;
		if (ttd == 0)
		  aliasc->flags |= F_IMMORTAL;
		else
		  aliasc->ttd = ttd;
		aliasc->name.namep = a->alias;
		aliasc->addr.cname.cache = crec;
		aliasc->addr.cname.uid = crec->uid;
		cache_hash(aliasc);
	      }
	  }
    }
}
#endif


void dump_cache(time_t now)
{
  struct server *serv, *serv1;

  my_syslog(LOG_INFO, _("time %lu"), (unsigned long)now);
  my_syslog(LOG_INFO, _("cache size %d, %d/%d cache insertions re-used unexpired cache entries."), 
	    daemon->cachesize, cache_live_freed, cache_inserted);
  my_syslog(LOG_INFO, _("queries forwarded %u, queries answered locally %u"), 
	    daemon->queries_forwarded, daemon->local_answer);

  if (!addrbuff && !(addrbuff = whine_malloc(ADDRSTRLEN)))
    return;

  
  for (serv = daemon->servers; serv; serv = serv->next)
    serv->flags &= ~SERV_COUNTED;
  
  for (serv = daemon->servers; serv; serv = serv->next)
    if (!(serv->flags & (SERV_NO_ADDR | SERV_LITERAL_ADDRESS | SERV_COUNTED)))
      {
	int port;
	unsigned int queries = 0, failed_queries = 0;
	for (serv1 = serv; serv1; serv1 = serv1->next)
	  if (!(serv1->flags & (SERV_NO_ADDR | SERV_LITERAL_ADDRESS | SERV_COUNTED)) && sockaddr_isequal(&serv->addr, &serv1->addr))
	    {
	      serv1->flags |= SERV_COUNTED;
	      queries += serv1->queries;
	      failed_queries += serv1->failed_queries;
	    }
	port = prettyprint_addr(&serv->addr, addrbuff);
	my_syslog(LOG_INFO, _("server %s#%d: queries sent %u, retried or failed %u"), addrbuff, port, queries, failed_queries);
      }
  
  if ((daemon->options & (OPT_DEBUG | OPT_LOG)))
    {
      struct crec *cache ;
      int i;
      my_syslog(LOG_DEBUG, "Host                                     Address                        Flags     Expires");
    
      for (i=0; i<hash_size; i++)
	for (cache = hash_table[i]; cache; cache = cache->hash_next)
	  {
	    char *a, *p = daemon->namebuff;
	    p += sprintf(p, "%-40.40s ", cache_get_name(cache));
	    if ((cache->flags & F_NEG) && (cache->flags & F_FORWARD))
	      a = ""; 
	    else if (cache->flags & F_CNAME) 
	      {
		a = "";
		if (!is_outdated_cname_pointer(cache))
		  a = cache_get_name(cache->addr.cname.cache);
	      }
#ifdef HAVE_IPV6
	    else 
	      { 
		a = addrbuff;
		if (cache->flags & F_IPV4)
		  inet_ntop(AF_INET, &cache->addr.addr, addrbuff, ADDRSTRLEN);
		else if (cache->flags & F_IPV6)
		  inet_ntop(AF_INET6, &cache->addr.addr, addrbuff, ADDRSTRLEN);
	      }
#else
            else 
	      a = inet_ntoa(cache->addr.addr.addr.addr4);
#endif
	    p += sprintf(p, "%-30.30s %s%s%s%s%s%s%s%s%s%s  ", a, 
			 cache->flags & F_IPV4 ? "4" : "",
			 cache->flags & F_IPV6 ? "6" : "",
			 cache->flags & F_CNAME ? "C" : "",
			 cache->flags & F_FORWARD ? "F" : " ",
			 cache->flags & F_REVERSE ? "R" : " ",
			 cache->flags & F_IMMORTAL ? "I" : " ",
			 cache->flags & F_DHCP ? "D" : " ",
			 cache->flags & F_NEG ? "N" : " ",
			 cache->flags & F_NXDOMAIN ? "X" : " ",
			 cache->flags & F_HOSTS ? "H" : " ");
#ifdef HAVE_BROKEN_RTC
	    p += sprintf(p, "%lu", cache->flags & F_IMMORTAL ? 0: (unsigned long)(cache->ttd - now));
#else
	    p += sprintf(p, "%s", cache->flags & F_IMMORTAL ? "\n" : ctime(&(cache->ttd)));
	    
	    *(p-1) = 0;
#endif
	    my_syslog(LOG_DEBUG, daemon->namebuff);
	  }
    }
}

char *record_source(int index)
{
  struct hostsfile *ah;

  if (index == 0)
    return HOSTSFILE;

  for (ah = daemon->addn_hosts; ah; ah = ah->next)
    if (ah->index == index)
      return ah->fname;
  
  return "<unknown>";
}

void querystr(char *str, unsigned short type)
{
  unsigned int i;
  
  sprintf(str, "query[type=%d]", type); 
  for (i = 0; i < (sizeof(typestr)/sizeof(typestr[0])); i++)
    if (typestr[i].type == type)
      sprintf(str,"query[%s]", typestr[i].name);
}

void log_query(unsigned short flags, char *name, struct all_addr *addr, char *arg)
{
  char *source, *dest = addrbuff;
  char *verb = "is";
  
  if (!(daemon->options & OPT_LOG))
    return;

  if (addr)
    {
#ifdef HAVE_IPV6
      inet_ntop(flags & F_IPV4 ? AF_INET : AF_INET6,
		addr, addrbuff, ADDRSTRLEN);
#else
      strncpy(addrbuff, inet_ntoa(addr->addr.addr4), ADDRSTRLEN);  
#endif
    }

  if (flags & F_REVERSE)
    {
      dest = name;
      name = addrbuff;
    }
  
  if (flags & F_NEG)
    {
      if (flags & F_NXDOMAIN)
	{
	  if (flags & F_IPV4)
	    dest = "NXDOMAIN-IPv4";
	  else if (flags & F_IPV6)
	    dest = "NXDOMAIN-IPv6";
	  else
	    dest = "NXDOMAIN";
	}
      else
	{      
	  if (flags & F_IPV4)
	    dest = "NODATA-IPv4";
	  else if (flags & F_IPV6)
	    dest = "NODATA-IPv6";
	  else
	    dest = "NODATA";
	}
    }
  else if (flags & F_CNAME)
    {
      
      if (flags & F_NXDOMAIN)
	dest = arg;
      else
	dest = "<CNAME>";
    }
    
  if (flags & F_CONFIG)
    source = "config";
  else if (flags & F_DHCP)
    source = "DHCP";
  else if (flags & F_HOSTS)
    source = arg;
  else if (flags & F_UPSTREAM)
    source = "reply";
  else if (flags & F_SERVER)
    {
      source = "forwarded";
      verb = "to";
    }
  else if (flags & F_QUERY)
    {
      source = arg;
      verb = "from";
    }
  else
    source = "cached";
  
  if (strlen(name) == 0)
    name = ".";

  my_syslog(LOG_DEBUG, "%s %s %s %s", source, name, verb, dest);
}

