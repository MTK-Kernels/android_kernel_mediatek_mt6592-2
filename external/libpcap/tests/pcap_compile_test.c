   pcap = pcap_open_dead(link, snaplen); 
    
   prog = argv[0]; 
   if (pcap_compile(pcap, &p, prog, optimize, 0) < 0) { 
      fprintf(stderr, pcap_geterr(pcap)); 
      exit(1); 
   } 
   bpf_dump(&p, option); 
   pcap_freecode(&p); 
   pcap_close(pcap); 
 
