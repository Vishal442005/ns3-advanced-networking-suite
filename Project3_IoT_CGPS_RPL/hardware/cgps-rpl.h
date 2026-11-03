/**
 * @file cgps-rpl.h
 * @brief Confidence-Gated Parent Switching RPL (CGPS-RPL) Embedded Header
 * @author Research Team & Antigravity Autonomous Agent
 * @date 2026
 *
 * Designed for deployment on ultra-low-power IEEE 802.15.4 motes (ARM Cortex-M3/M4,
 * STM32F103, CC2538, nRF52840) running Contiki-NG or RIOT OS.
 * Operates purely using fixed-point Q8.8 arithmetic without hardware FPU dependency.
 */

#ifndef CGPS_RPL_H_
#define CGPS_RPL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Configuration & Architectural Constants                                    */
/* -------------------------------------------------------------------------- */

#define CGPS_MAX_NEIGHBORS          8      /**< Maximum candidate parents tracked */
#define CGPS_QUEUE_CAPACITY         64     /**< Maximum MAC packet buffer depth */
#define CGPS_Q8_SHIFT               8      /**< Fixed-point shift for Q8.8 */
#define CGPS_Q8_ONE                 (1 << CGPS_Q8_SHIFT) /**< 1.0 in Q8.8 (256) */

/* Default Parametric Values (Scaled in Q8.8) */
#define CGPS_DEFAULT_ALPHA          128    /**< Weight alpha = 0.50 (128 / 256) */
#define CGPS_DEFAULT_GAMMA_STAR     166    /**< Optimal stopping gamma* = 0.65 (166 / 256) */
#define CGPS_DEFAULT_HYST_TICKS     50     /**< Switching hysteresis holdoff (50 ms / 50 ticks) */
#define CGPS_MAX_MAC_RETRIES        7      /**< IEEE 802.15.4 standard retry limit */

/* -------------------------------------------------------------------------- */
/* Fixed-Point Types                                                          */
/* -------------------------------------------------------------------------- */

typedef int16_t q8_8_t; /**< Signed 16-bit fixed-point: 8 integer bits, 8 fraction bits */

/**
 * @brief Fast fixed-point multiplication: (a * b) >> 8
 */
static inline q8_8_t q8_mul(q8_8_t a, q8_8_t b) {
    return (q8_8_t)(((int32_t)a * (int32_t)b) >> CGPS_Q8_SHIFT);
}

/**
 * @brief Fast fixed-point division: ((int32_t)a << 8) / b
 */
static inline q8_8_t q8_div(q8_8_t a, q8_8_t b) {
    if (b == 0) return 0x7FFF;
    return (q8_8_t)((((int32_t)a) << CGPS_Q8_SHIFT) / b);
}

/* -------------------------------------------------------------------------- */
/* Data Structures                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Compact neighbor table entry (16 bytes packed per candidate parent)
 */
typedef struct __attribute__((packed)) {
    uint16_t node_id;            /**< 16-bit 6LoWPAN short MAC address */
    uint16_t rank;               /**< RPL DAG Rank (RFC 6550) */
    uint16_t etx;                /**< Scaled RFC 6719 ETX metric (Q8.8) */
    uint8_t  consecutive_fails;  /**< Number of consecutive unacknowledged frame attempts */
    uint8_t  last_rssi;          /**< Inverted link RSSI (dBm offset) */
    q8_8_t   confidence;         /**< Computed link confidence C_p(t) in Q8.8 */
    uint32_t last_active_tick;   /**< System tick timestamp of last transmission */
} cgps_neighbor_t;

/**
 * @brief Core CGPS-RPL router control block
 */
typedef struct {
    uint16_t my_node_id;                     /**< Self node address */
    uint16_t my_rank;                        /**< Self node DODAG rank */
    int8_t   primary_parent_idx;             /**< Index of current preferred parent (-1 if none) */
    uint8_t  neighbor_count;                 /**< Number of active entries in neighbor_table */
    cgps_neighbor_t neighbors[CGPS_MAX_NEIGHBORS]; /**< Fixed memory neighbor table */
    
    /* Cross-layer state variables */
    uint16_t current_queue_occupancy;        /**< Current instantaneous buffer packets */
    uint16_t queue_capacity;                 /**< Maximum queue buffer size */
    uint32_t last_switch_tick;               /**< System timestamp of last parent switch */
    
    /* Configurable hyperparameters (Q8.8) */
    q8_8_t alpha;                            /**< Weight balance: alpha * (1 - Q/Qmax) */
    q8_8_t gamma_star;                       /**< Optimal stopping confidence threshold */
    uint32_t hysteresis_ticks;               /**< Minimum ticks required between parent flaps */
    
    /* Diagnostic Telemetry */
    uint32_t total_parent_switches;          /**< Cumulative parent switch count */
    uint32_t total_deflections_activated;    /**< Emergency deflection count */
    uint32_t total_flaps_suppressed;         /**< Flaps suppressed by hysteresis guard */
    uint32_t dropped_packets;                /**< Packets dropped at buffer */
} cgps_rpl_cb_t;

/* -------------------------------------------------------------------------- */
/* Public API Functions                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize the CGPS-RPL control block and neighbor table.
 */
void cgps_rpl_init(cgps_rpl_cb_t *cb, uint16_t node_id, uint16_t initial_rank);

/**
 * @brief Register or update a candidate parent in the neighbor table.
 */
int8_t cgps_rpl_upsert_neighbor(cgps_rpl_cb_t *cb, uint16_t node_id, uint16_t rank, uint16_t etx);

/**
 * @brief Inform CGPS-RPL of MAC transmission feedback (unicast ACK success or failure).
 */
void cgps_rpl_on_mac_feedback(cgps_rpl_cb_t *cb, uint16_t target_id, bool ack_received, uint8_t retries);

/**
 * @brief Inform CGPS-RPL of current local MAC packet buffer occupancy.
 */
void cgps_rpl_on_queue_update(cgps_rpl_cb_t *cb, uint16_t queue_depth);

/**
 * @brief Compute the cross-layer transient confidence C_p(t) for a given candidate parent.
 */
q8_8_t cgps_rpl_compute_confidence(const cgps_rpl_cb_t *cb, int8_t neighbor_idx);

/**
 * @brief Evaluate parent switching conditions and trigger emergency deflection if warranted.
 * 
 * Enforces Theorem 1 (Acyclic DAG Invariant): Deflection candidate MUST satisfy Rank(p) < Rank(self).
 * 
 * @param cb Pointer to control block.
 * @param current_tick Monotonic system tick.
 * @return int8_t Index of selected forwarding parent, or -1 if no valid loop-free candidate exists.
 */
int8_t cgps_rpl_evaluate_parent_switch(cgps_rpl_cb_t *cb, uint32_t current_tick);

/**
 * @brief Fast O(1) emergency deflection query for an outgoing alarm packet.
 */
uint16_t cgps_rpl_get_next_hop(cgps_rpl_cb_t *cb, uint32_t current_tick);

#ifdef __cplusplus
}
#endif

#endif /* CGPS_RPL_H_ */
