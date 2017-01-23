
pcap_t *septel_create(const char *device, char *ebuf, int *is_ours);
int septel_findalldevs(pcap_if_t **devlistp, char *errbuf);
