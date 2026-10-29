# CAN Architecture

How CAN is layered across `BM`/`FBL`/`APP`, how the two buses are wired, and how the diagnostic gateway fits in.

## Layers

The stack follows AUTOSAR's order and its idea: **CAN IDs exist only in CANIF** (and below). Everything above it speaks PDU IDs. CANIF maps a received CAN ID to a PDU ID and a user (CANTP or PDUR), and a PDU ID to a CAN ID to send. ISO-TP goes CANIF → CANTP → PDUR, plain frames CANIF → PDUR; in AUTOSAR terms CANTP's *N-PDUs* face CANIF and its *N-SDUs* face PDUR.

```
        UDS / UDS_CLIENT / signal modules           know only PDUR route ids
                 |
               PDUR                                 routing table: route id -> upper-layer RX, lower-layer TX, confirmation
              /     \
         CANTP       |                              ISO-TP: pairs of N-PDU ids (to CANIF) and N-SDU ids (= PDUR routes)
              \     /
               CANIF                                receive table (CAN id -> PDU id + user), transmit table (PDU id -> channel + CAN id)
              /     \
     CAN driver      transceiver driver
  (HAL_CAN, MCP2515)  (TJA1051, ...)
```

- **RX**: a driver calls `CANIF_rx_indication()`. The channel's receive table maps every ID it takes (or a range of them, up to `last_id`) to a PDU ID and a user — `CANIF_TO_CANTP` (`CANTP_rx_frame_received( channel's cantp_p, n-pdu )`, ISR-safe, queued for CANTP's tick) or `CANIF_TO_PDUR` (plain frame, queued, `PDUR_rx_indication_id( route )` from `CANIF_tick()`). Anything else is dropped. CANTP reassembles and passes the message up on the pair's N-SDU, which is a PDUR route (`PDUR_rx_indication`).
- **TX**: a route's `lower_layer_tx_func` is a per-instance CANTP adapter such as `app_cantp_transmit` (ISO-TP: `CANTP_tx_request()` finds the pair by N-SDU and sends frames down as the pair's N-PDU through `CANIF_transmit`) or `CANIF_transmit` directly (plain). Both end in `CANIF_transmit`, which sends at once or, if the driver refuses and the channel has a queue, queues the frame; queued frames go out from `CANIF_tick()`, in order. A transmit entry that covers a range of CAN IDs takes the one last given to `CANIF_set_dynamic_tx_id()`. Both look the PDU ID up in CANIF's transmit table for the channel and CAN ID. An entry can cover a range of CAN IDs, picked per send (`CANIF_transmit_id`, `CANIF_pdur_if_tx_id` — AUTOSAR's dynamic ID). PDUR hands the route's own PDU ID down, so a lower layer's tables are keyed by the PDU IDs of the routes that use it.
- **Tick**: CANIF delivers and sends, then `CANTP_tick()` services each CANTP instance, one schedule row per instance (CANIF never ticks CANTP).

`xCOMMON_MODULES` is a **git submodule** shared with other projects, each pinned at its own commit. `Common_Src` is HUB-local (`APP`/`FBL`/`BM`) and holds the board drivers, including `HAL_CAN`.

### HAL_CAN — `Common_Src/HAL_CAN`

The STM32F103's one bxCAN peripheral, 500 kbit/s on PA12/PA11. `HAL_CAN_send_frame`/`HAL_CAN_set_mode`/`HAL_CAN_get_status` match `CANIF_channel_cfg_st`'s slots, so a CANIF row names them directly. The `channel` given to `HAL_CAN_set_rx_callback()` is only echoed back to `CANIF_rx_indication` — there is one physical bus.

### CANIF — `xCOMMON_MODULES/Src/CANIF`

One `CANIF_channel_cfg_st` row per bus: the driver's send function (`send_frame`, or `send_frame_inst` + `send_inst_p` for a multi-instance driver such as `MCP2515_send_frame`), optional mode/status/transceiver functions, the receive table (`rx_pdus_p`), and `state_p` — caller-owned RAM for the plain-frame queues (`NULL_P` for an ISO-TP-only channel, as in FBL). A receive entry maps a CAN ID (or a range) to a `pdu_id` and a user (CANTP: its N-PDU; PDUR: the route), and only matches a frame of its own `id_type` (standard by default, so an extended frame with the same number is a different one). One transmit table, passed to `CANIF_init()`, maps each PDU ID to its channel, CAN ID and ID type. `HAL_CAN` sets `CAN_TXFP`, so bxCAN sends in the order frames were loaded and CANIF can queue frames of one ID without them overtaking each other.

### CANTP — `xCOMMON_MODULES/Src/CANTP`

ISO 15765-2 segmentation and reassembly, classic CAN and CAN FD, one `CANTP_instance_st` per bus. It knows no CAN ID, ID type or bus: each `tp_ids[]` pair is `{ req_id, resp_id, rx_sdu_id, tx_sdu_id }` — N-PDUs towards CANIF (received on `req_id`, sent as `resp_id`) and N-SDUs towards PDUR (a message received on `req_id` is passed up as `rx_sdu_id`; one requested on `tx_sdu_id` goes out on `resp_id` and is confirmed on `tx_sdu_id`). There is no instance registry: a CANIF channel row names its instance (`cantp_p`), and the integration binds each PDUR route to one with a small transmit adapter around `CANTP_tx_request()`. Plain frames never pass through it. CAN FD framing is a runtime `fd_enable` per instance. FBL leaves `fd_enable` off, so it runs classic CAN.

### PDUR — `xCOMMON_MODULES/Src/PDUR`

The routing table, indexed by PDU ID: each route has an upper-layer RX function (`upperLayerRxIndication`, or `upperLayerRxIndicationId` for an endpoint that is also told the CAN ID — AUTOSAR's meta data), a lower-layer TX function (a per-instance CANTP adapter such as `app_cantp_transmit`, or `CANIF_transmit`, given the route's own PDU ID) and an optional `txConfirmation`. It holds no channel, CAN ID or medium. `PDUR_tx()` and the route callbacks are all its users ever see.

## How APP wires it

Files: `APP/Src/INT_STUBS/APP_PDU_IDS.h`, `APP/Src/INT_STUBS/INTEGRATION_STUBS.c`, `APP/Src/MAIN/main.c`, `APP/Src/SCHED_CFG/SCHED_config.c`.

| Channel | Driver | Receive table | CANTP |
|---|---|---|---|
| `APP_CAN_CH_MAIN` (CAN1) | `HAL_CAN` + TJA1051 | UDS 0x700 / 0x7E0 → CANTP; shifter 0x197 / 0x55E → PDUR; each node's request ID → PDUR (gateway route) | `app_cantp_instance_s` (HUB's own UDS server) |
| `APP_CAN_CH_CAN2` | `MCP2515` on SPI1 | each node's response ID → PDUR (gateway route) | `app_cantp2_instance_s` (HUB's UDS_CLIENT) |

- CAN1 frames arrive by interrupt (`HAL_CAN_set_rx_callback(..., CANIF_rx_indication)`), and `CANIF_tick` drains CAN1's transmit queue.
- The MCP2515 shares SPI1 with the NRF24, so it is never touched from an interrupt: `MCP2515_tick()` reads it from the scheduler and passes each frame to `CANIF_rx_indication` directly (the function table's `rx_indication_func_p`, with the channel from the config's `rx_channel`).
- Schedule order: the scheduler's base tick is 5 ms (`APP_TIMER_TICK_RATE_MS`). The gateway path - `MCP2515_tick` → `DIAG_GW_tick` → `CANIF_tick` - runs every 5 ms, so a frame crosses the gateway in the tick it is received (at most 5 ms of waiting). The HUB's own client and server side - `UDS_CLIENT_tick` (between `DIAG_GW_tick` and `CANIF_tick`), `CANTP_tick` (after `CANIF_tick`) and `UDS_tick` - stays on a 10 ms period. `HMI_SH1106_tick` runs at offset 5 ms so its blocking OLED writes stay out of the passes the gateway shares with the other 10 ms work.

## Diagnostic gateway (CAN1 tester ↔ CAN2 node)

Nodes on CAN2 each have their own diagnostic ID pair: any request ID from 0x718–0x799, answered on the request ID − 0x100 (0x618–0x699). Which ones are fitted is not known at build time, so nothing lists them — the two ranges are configured once (`APP_NODE_*_ID`, `INTEGRATION_STUBS.h`). The ranges live only in the CANIF receive tables (an entry with a `last_id`), which map each to one PDUR route (`APP_PDU_GW_TESTER`, `APP_PDU_GW_NODE`); they end at `DIAG_GW_tester_rx` and `DIAG_GW_node_rx`, ID-aware endpoints (`upperLayerRxIndicationId`, called through `PDUR_rx_indication_id`) so DIAG_GW knows the node. The HUB's own CANTP on CAN2 talks to one virtual node (one pair, `APP_CANIF_PDU_HUB_RX/TX`): `app_diag_gw_to_hub` presents the reply to it, and `app_cantp2_can_tx` sends everything CANTP sends to the selected node's request ID. Node number n is request ID − 0x718. A node is shared by a tester on CAN1 (forwarded frame by frame, so flashing passes through untouched) and the HUB's own `UDS_CLIENT`, which talks to one node at a time (`UDS_CLIENT_set_node( n )`; `APP_NODE_FROM_REQUEST_ID()` converts a request ID). `DIAG_GW` (`xCOMMON_MODULES/Src/DIAG_GW`) decides who each response frame belongs to, per node (up to `DIAG_GW_MAX_NODES` nodes tracked at once, a slot claimed when the tester or HUB first addresses a node) — replies go to whoever made that node's last request. It never reads a frame; it keeps the two clients apart by time. The tester owns a node while it is active: the HUB is only granted it (`DIAG_GW_local_reserve`) after the tester has been silent on it for `tester_idle_ms` (5.5 s: past the node's S3 and the longest response-pending wait; the node's replies to the tester count as activity). The HUB then keeps the node for `local_ms` (1 s), started again by each reply it gets, so its reply must be a quick read with no response pending. A tester frame that arrives in that time is held and sent on when the HUB is done. Its config table (`app_diag_gw_cfg_s`) in `APP/Src/INT_STUBS/INTEGRATION_STUBS.c` wires it:

- **tester → node**: CANIF (CAN1) → PDUR `APP_PDU_GW_TESTER` → `DIAG_GW_tester_rx` → DIAG_GW → `CANIF_set_dynamic_tx_id` + `CANIF_transmit` on transmit PDU `APP_CANIF_PDU_GW_NODE_TX` (CAN2, every node request ID) → MCP2515, with the ID it came in with.
- **node → tester**: MCP2515 → CANIF (CAN2) → PDUR `APP_PDU_GW_NODE` → `DIAG_GW_node_rx` → DIAG_GW → `CANIF_set_dynamic_tx_id` + `CANIF_transmit` on `APP_CANIF_PDU_GW_TESTER_TX` (CAN1, every node response ID) → HAL_CAN.
- **HUB ↔ node**: `UDS_CLIENT` → PDUR `APP_PDU_NODE_DIAG` → `app_cantp2_transmit` → CANTP → `app_cantp2_can_tx` (puts the selected node on the CAN ID: `CANIF_set_dynamic_tx_id` + `CANIF_transmit` on `APP_CANIF_PDU_HUB_TX`) → CANIF → MCP2515, after `DIAG_GW_local_reserve( node )`; the reply comes back through DIAG_GW → `CANTP_rx_frame_received( &app_cantp2_instance_s, APP_CANIF_PDU_HUB_RX )` → CANTP → PDUR → `UDS_CLIENT`. Nothing the HUB asked for ever reaches CAN1.

Sequence diagrams: `xCOMMON_MODULES/Src/DIAG_GW/DIAG_GW_sequences.drawio`.

## How FBL wires it

Files: `FBL/Src/INT_STUBS/INTEGRATION_STUBS.c`, `FBL/Makefile`.

One channel on `HAL_CAN`, ISO-TP only: the CANIF row's receive table maps the two UDS request ids (0x700, 0x7E0) to CANTP N-PDUs, a two-entry transmit table maps the N-PDUs it answers with back to 0x600 / 0x7E8, and `state_p` is `NULL_P`, so FBL carries no plain-frame queues. Both PDUR routes use `fbl_cantp_transmit`, and the CANIF row names FBL's one CANTP instance. `FBL/Makefile` lists its shared sources explicitly to keep tight control of flash — FBL is ~97% full.

`BM` has no CAN code — it only selects `APP` vs `FBL` and jumps.

## Adding a channel

1. A new enumerator in `app_can_channel_et` (`APP_PDU_IDS.h`), before `APP_CAN_NUM_CHANNELS`.
2. The driver instance (and transceiver, if any) in `INTEGRATION_STUBS.c`.
3. A `CANTP_instance_st` with its own RX buffer and `tp_ids[]` pairs (N-PDUs from `app_canif_pdu_id_et`, N-SDUs = PDUR routes), if the bus carries ISO-TP.
4. A `canif_channels_s[]` row: driver, receive table, and a `CANIF_channel_state_st` if it carries plain frames.
5. PDU ids in `app_pdu_id_et`, routes in `pdur_routing_table_s[]`, and a `canif_tx_pdus_s[]` entry (PDU id -> channel + CAN id) for each route or N-PDU that sends.
6. A receive path into `CANIF_rx_indication` — an interrupt callback, or a scheduled poll for an SPI controller.

## See also

- `xCOMMON_MODULES/Src/README.md` — full module list for the shared submodule.
