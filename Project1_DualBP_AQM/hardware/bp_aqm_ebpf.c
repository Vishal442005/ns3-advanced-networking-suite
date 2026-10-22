// SPDX-License-Identifier: GPL-2.0
/*
 * bp_aqm_ebpf.c - Linux Kernel eBPF (tc-bpf) In-Network Implementation of BP-AQM
 * Attaches to Linux Traffic Control (tc) egress qdisc for line-rate edge routing.
 */

#include <linux/bpf.h>
#include <linux/pkt_cls.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define TARGET_DELAY_US   10000   // 10 ms
#define SAFETY_DELAY_US   30000   // 30 ms
#define MAX_PERSISTENCE   3       // Overload tolerance limit

struct bp_state {
    __u64 last_update_ns;
    __u32 smoothed_delay_us;
    __u32 base_probability;       // Scaled [0, 65535]
    __u32 eff_probability;
    __u32 arrival_bytes;
    __u16 overload_intervals;
};

// BPF Array Map for per-interface queue state
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 16);
    __type(key, __u32);
    __type(value, struct bp_state);
} aqm_state_map SEC(".maps");

SEC("classifier")
int bp_aqm_egress(struct __sk_buff *skb)
{
    void *data = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;
    __u32 key = 0;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return TC_ACT_OK;

    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return TC_ACT_OK;

    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return TC_ACT_OK;

    struct bp_state *state = bpf_map_lookup_elem(&aqm_state_map, &key);
    if (!state)
        return TC_ACT_OK;

    __u64 now_ns = bpf_ktime_get_ns();
    __u32 head_delay_us = (skb->tstamp > 0 && now_ns > skb->tstamp)
                          ? (__u32)((now_ns - skb->tstamp) / 1000)
                          : 0;

    // 1. EWMA smoothing: d_bar = 0.8 * d_bar + 0.2 * d_curr
    state->smoothed_delay_us = (state->smoothed_delay_us * 4 + head_delay_us) / 5;

    // 2. Dual-Envelope Safety & Tolerance Modulation
    if (head_delay_us > SAFETY_DELAY_US) {
        // Safety guard active: accelerate marking
        state->eff_probability = state->base_probability + ((head_delay_us - SAFETY_DELAY_US) >> 3);
    } else if (state->overload_intervals > 0 && state->overload_intervals <= MAX_PERSISTENCE) {
        // Microburst tolerance: damp marking by 4x
        state->eff_probability = state->base_probability >> 2;
    } else {
        state->eff_probability = state->base_probability;
    }

    // 3. Probabilistic Marking / Drop
    __u32 rand_val = bpf_get_prandom_u32() & 0xFFFF;
    if (rand_val < state->eff_probability) {
        // Check ECT bit in IP diffserv
        __u8 ecn = iph->tos & 0x03;
        if (ecn == 0x01 || ecn == 0x02) {
            // Mark Congestion Experienced (CE = 0x03)
            __u8 old_tos = iph->tos;
            iph->tos = (iph->tos & 0xFC) | 0x03;
            // Update incremental IP checksum
            bpf_l3_csum_replace(skb, offsetof(struct iphdr, check), old_tos, iph->tos, 2);
            return TC_ACT_OK;
        } else {
            // Non-ECT: drop packet before queue
            return TC_ACT_SHOT;
        }
    }

    return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
