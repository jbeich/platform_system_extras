#include <uapi/linux/bpf.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/if_packet.h>
#include <uapi/linux/ip.h>
#include "bpf_helpers.h"

#define egressMap 0x12345678ffffffff
#define ingressMap 0x87654321ffffffff

SEC("test")
int bpf_prog1(struct __sk_buff *skb)
{
	int index;
        int ret = bpf_skb_load_bytes(skb, ETH_HLEN + offsetof(struct iphdr, protocol), &index, sizeof(int));
	u64 *value;

	if (skb->pkt_type != PACKET_OUTGOING) {
            value = bpf_map_lookup_elem(egressMap, &index);
            if (value)
                    __sync_fetch_and_add(value, skb->len);
        } else {
            value = bpf_map_lookup_elem(ingressMap, &index);
            if (value)
                    __sync_fetch_and_add(value, skb->len);
        }
	return 0;
}
char _license[] SEC("license") = "GPL";
