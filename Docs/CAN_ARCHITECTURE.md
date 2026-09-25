# CAN Architecture

How CAN is layered across `BM`/`FBL`/`APP`, and how to add a second CAN bus.

## Layers

```
        UDS                (xCOMMON_MODULES/Src/UDS)          knows only route ids, no CAN
         |
        PDUR                (xCOMMON_MODULES/Src/PDUR)          logical routing table, .channel is a plain u8_t
         |
        CANTP               (xCOMMON_MODULES/Src/CANTP)         ISO 15765-2 (ISO-TP), one instance per channel
         |
        CANIF               (xCOMMON_MODULES/Src/CANIF)         channel table: dispatches to a CAN driver + transceiver per row
        /    \
   CAN driver         transceiver driver
  (HAL_CAN, MCP2515)  (CAN_TRCV/TJA1051, ...)
```

`xCOMMON_MODULES` is a **git submodule** (`common_modules`), shared across multiple independent projects, each pinned at its own commit. `CANIF`/`CANTP`/`PDUR`/`UDS` are already generic and multi-channel/multi-instance — none of them need to change to add a channel. `Common_Src` is HUB-local (shared only by `APP`/`FBL`/`BM`, not published) and holds the one thing that *is* single-instance: `HAL_CAN`.

### HAL_CAN — `Common_Src/HAL_CAN/HAL_CAN.c` / `.h`

Wraps the STM32F103's **one physical bxCAN1 peripheral** directly — `CAN_Init(CAN1, ...)`, `CAN_Transmit(CAN1, ...)`, ISR is `USB_LP_CAN1_RX0_IRQHandler`. Fixed 500 kbit/s (APB1=36MHz, prescaler=4, BS1=14tq/BS2=3tq). Pins are PA12 (TX) / PA11 (RX), from `Common_Src/PROJ_CONFIG/HAL_config.h`.

```c
void         HAL_CAN_init( void );
void         HAL_CAN_set_rx_callback( u8_t channel, HAL_CAN_rx_callback_ft rx_callback_p );
pass_fail_et HAL_CAN_send_frame( u32_t id, u8_t id_type, const u8_t* data_p, u8_t dlc );
pass_fail_et HAL_CAN_set_mode( CANIF_ctrl_mode_et mode );
pass_fail_et HAL_CAN_get_status( CANIF_ctrl_status_st* status_p );
```

None of these take a hardware instance parameter — there's only one bxCAN to talk to. The `channel` byte in `HAL_CAN_set_rx_callback()` is **not** a hardware selector: it's a label the board hands in, stored and echoed straight back out through the RX callback so the layer above (`CANIF_rx_indication`) knows which logical bus a frame arrived on. `HAL_CAN.c` itself only ever has one registered callback and one physical bus.

`HAL_CAN_send_frame`/`HAL_CAN_set_mode`/`HAL_CAN_get_status` are shaped to match `CANIF_channel_cfg_st`'s `send_frame`/`ctrl_set_mode`/`ctrl_get_status` function-pointer slots exactly, so a `CANIF` channel row can point at them with no adapter.

### CANTP — `xCOMMON_MODULES/Src/CANTP`

ISO-TP (ISO 15765-2) segmentation/reassembly, CAN-FD capable. `CANTP_instance_st` is fully multi-instance: each instance carries its own `.channel`, its own request/response ID table (`tp_ids[]`), its own RX/TX FIFOs and session slots. One `CANTP_instance_st` exists per CAN bus. Key calls: `CANTP_init()`, `CANTP_tick()`, `CANTP_tx_request()`, `CANTP_rx_frame_received()`.

### CANIF — `xCOMMON_MODULES/Src/CANIF`

The channel table. `CANIF_init(channels_p, num_channels)` takes an array of `CANIF_channel_cfg_st`, one row per bus:

```c
typedef struct
{
    /* send_frame / ctrl_set_mode / ctrl_get_status — the CAN driver for this bus            */
    /* trcv_set_mode / trcv_inst_p                   — the transceiver for this bus           */
    /* cantp_p                                        — this channel's CANTP_instance_st*      */
} CANIF_channel_cfg_st;
```

`CANIF_tick()` ticks every channel's CANTP instance. `CANIF_rx_indication(channel, id, id_type, data, dlc)` and `CANIF_transmit(...)` dispatch by channel index. `CANIF_pdur_tp_tx()` / `CANIF_pdur_normal_tx()` are ready-made adapters that a `PDUR_route_st` points at directly.

### PDUR — `xCOMMON_MODULES/Src/PDUR`

Logical routing table above CANTP. `PDUR_route_st` holds `.channel`, `.rx_id`/`.rx_id_mask`/`.rx_medium`, `.tx_id`/`.tx_medium`, and the RX/TX callback pointers. `.channel` is a plain `u8_t`, numbered per-project (0 is the default a route lands on if it doesn't set one). `PDUR_tx()` / `PDUR_rx_indication_physical()` are the entry points UDS binds to directly.

### UDS — `xCOMMON_MODULES/Src/UDS`

Never sees a CAN id or a channel — only `UDS_route_id_t` (a PDUR route index). Fully CAN-agnostic.

## How APP wires it (multi-channel-ready)

Files: `APP/Src/INT_STUBS/APP_PDU_IDS.h`, `APP/Src/INT_STUBS/INTEGRATION_STUBS.c`, `APP/Src/MAIN/main.c`, `APP/Src/MODE_MGR/MODE_MGR.c`.

- **Channels**: `app_can_channel_et` in `APP_PDU_IDS.h` — currently just `APP_CAN_CH_MAIN = 0u`.
- **PDU ids**: `app_pdu_id_et` in the same file — logical route indices (sensor slots, UDS functional/physical, GKT/TCU/MQB_CLUSTER tx), not raw CAN ids. These index `pdur_routing_table_s[]`.
- **`INTEGRATION_STUBS.c`** builds, for `APP_CAN_CH_MAIN`:
  - `app_cantp_instance_s` (a `CANTP_instance_st`) with `.channel = APP_CAN_CH_MAIN`, `can_tx_func_p = CANIF_transmit`, and its UDS functional/physical `tp_ids[]` pairs (0x700/0x600, 0x7E0/0x7E8).
  - `tja1051_main_instance_s` — the transceiver instance for this bus.
  - `canif_channels_s[APP_CAN_NUM_CHANNELS]` — row `[APP_CAN_CH_MAIN]` wires `HAL_CAN_send_frame`/`HAL_CAN_set_mode`/`HAL_CAN_get_status` + the TJA1051 transceiver + `&app_cantp_instance_s`.
  - `pdur_routing_table_s[]` — every route is on `APP_CAN_CH_MAIN` by leaving `.channel` at its zero default.
- **Boot order** (`APP/Src/MAIN/main.c`, `app_main()`): `HAL_CAN_init()` → `PDUR_init(...)` → `app_cantp_instance_init(); CANTP_init(&app_cantp_instance_s)` → `CANIF_init(canif_channels_s, APP_CAN_NUM_CHANNELS)` → **then** `HAL_CAN_set_rx_callback(APP_CAN_CH_MAIN, CANIF_rx_indication)` (deliberately last, so a frame can't arrive mid-init) → `UDS_init(...)`.
- **Tick**: `MODE_MGR_tick()` calls `CANIF_tick()` every cycle, which ticks every channel's CANTP instance — a new bus needs no change here.

## How FBL wires it (deliberately smaller — no CANIF)

Files: `FBL/Src/INT_STUBS/INTEGRATION_STUBS.c`, `FBL/Src/UDS_CFG/UDS_config.c`, `FBL/Makefile`.

FBL has one bus and one job (answer UDS flashing requests), so it skips `CANIF` and wires `CANTP` straight to `HAL_CAN`:

- `fbl_cantp_instance_s` — a single `CANTP_instance_st`, channel hardcoded to 0.
- `fbl_can_tx()` wraps `HAL_CAN_send_frame` directly and is set as the instance's `can_tx_func_p`.
- `fbl_can_rx()` calls `CANTP_rx_frame_received(&fbl_cantp_instance_s, ...)` directly and is registered via `HAL_CAN_set_rx_callback(0u, fbl_can_rx)`.
- `fbl_pdur_routing_table_s[]` has two rows (UDS functional, UDS physical), both implicitly channel 0.
- Same CAN ids as APP (0x700/0x600 functional, 0x7E0/0x7E8 physical) — safe to reuse since `BM` only ever runs one of `APP`/`FBL` at a time.
- Ticked from `FBL_run()`'s main loop, not a SysTick mode-manager: `CANTP_tick(&fbl_cantp_instance_s); UDS_tick();`.

`FBL/Makefile` lists its CAN-related shared-module sources explicitly (`CANTP.c`, `CAN_TRCV/TJA1051/TJA1051.c`, plus `Common_Src/HAL_CAN/HAL_CAN.c`) rather than globbing a directory, to keep tight control over flash budget — FBL is already ~96.8% full.

`BM` has no CAN code at all — it only selects `APP` vs `FBL` and jumps.

## Adding a second CAN channel

**Hardware constraint**: the STM32F103C8 on this board is a medium-density part with exactly one bxCAN peripheral — there is no on-chip CAN2 to duplicate `HAL_CAN.c` for. A second channel means a **second, different driver** on an external controller (e.g. SPI), not a second copy of `HAL_CAN.c`.

`xCOMMON_MODULES/Src/CAN_CTRL/MCP2515` already has a multi-instance SPI CAN controller driver (`MCP2515_init(inst_p, func_table_p, cfg_p)`, `MCP2515_transmit()`, `MCP2515_receive()`, etc.) that fits this. Everything above `HAL_CAN` (`CANIF`, `CANTP`, `PDUR`) is already channel-generic, so this is an **APP-only change** — no `xCOMMON_MODULES` API changes required.

Steps, in `APP/Src/INT_STUBS/`:

1. **`APP_PDU_IDS.h`** — add a new enumerator to `app_can_channel_et`, e.g. `APP_CAN_CH_SECONDARY`, before `APP_CAN_NUM_CHANNELS`.
2. **Driver + transceiver instance** — add an `MCP2515_instance_st` (with its SPI transfer / chip-select function table) and a second transceiver instance (another `CAN_TRCV` driver instance for its own enable/status pins). `MCP2515`'s send/receive calls use their own frame struct, not `CANIF_channel_cfg_st`'s `(id, id_type, data_p, dlc)` shape, so write a small adapter function pair (`app_mcp2515_send_frame()`, etc.) — this is the one place the "point CANIF straight at the driver" pattern needs a thin shim.
3. **New `CANTP_instance_st`** — a second instance (e.g. `app_cantp_instance_2_s`) with its own RX buffer and `tp_ids[]`, `.channel = APP_CAN_CH_SECONDARY`. Call `CANTP_init()` on it from `main.c` alongside the existing one.
4. **`canif_channels_s[]`** — add a row at index `APP_CAN_CH_SECONDARY` (bump the array size via `APP_CAN_NUM_CHANNELS`) pointing at the new driver's adapters, the new transceiver instance, and the new CANTP instance.
5. **PDU ids + routes** — add new entries to `app_pdu_id_et` for whatever lives on the new bus, and add rows to `pdur_routing_table_s[]` with `.channel = APP_CAN_CH_SECONDARY` (existing rows are unaffected — they stay on `APP_CAN_CH_MAIN` by leaving `.channel` at its default 0).
6. **RX delivery** — bxCAN's RX arrives via a real NVIC IRQ into `HAL_CAN_set_rx_callback()`. MCP2515 has no direct core interrupt wired through `HAL_CAN`; its `/INT` pin needs its own GPIO-EXTI or polled glue in `INTEGRATION_STUBS.c` that calls `CANIF_rx_indication(APP_CAN_CH_SECONDARY, id, id_type, data, dlc)` directly.
7. `CANIF_init(canif_channels_s, APP_CAN_NUM_CHANNELS)` in `main.c` needs no change — the count is already dynamic.
8. `MODE_MGR_tick()`'s `CANIF_tick()` call needs no change — it already ticks every channel.

Adding a channel to `FBL` is a separate, larger change: `FBL` bypasses `CANIF` entirely and hardcodes channel 0 straight through `fbl_can_tx`/`fbl_can_rx` to `HAL_CAN`, so it would need its own second hardcoded path rather than a new row in a table. There is currently no reason to do this — all of FBL's UDS/flash traffic runs on channel 0.

## See also

- `xCOMMON_MODULES/Src/README.md` — full module list for the shared submodule.
