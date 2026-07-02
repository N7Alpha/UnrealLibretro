# Netplay Beta Integration Task List

**Status (2026-07-01):** All P0 tasks done (Codex, reviewed/fixed). P1 tasks 7–11 and P2 tasks 12–18 done.
P3: 21 and 22 done; 19 deferred (needs a careful trace, low payoff); 20 is a non-goal by definition.
The automatable part of the verification pass is now `ulnet_disconnect_test.sh` (in CI). The plugin
compiles against UE 5.6.1 on macOS (arm64) and the editor boots headless with it; a PIE
netplay session between two editor instances is the remaining manual check.
Note: task 8 reuses `OnNetplayError` (code = `ulnet_session_event_t` value) instead of adding a new delegate.

Goal: ship a beta of ulnet.h netplay in the UnrealLibretro plugin, **client-server only** (authority + spectators). P2P mesh stays in the protocol but gets no plugin/Blueprint surface. The hard requirement is that disconnects in the client-server model never hang a session forever. Loose ends are acceptable (MVP), silent hangs are not.

## Codebase orientation (read this first)

- [ulnet.h](Source/UnrealLibretro/Private/ulnet.h) — single-header netcode. Public API/structs at the top; the core protocol implementation lives inside `#ifndef ULNET_C` (line ~2572). The ICE-lite NAT transport is in `#ifndef ULNET_NAT_C` above it. ImGui diagnostics at the bottom under `ULNET_IMGUI`.
- [sam2.h](Source/UnrealLibretro/Private/sam2.h) — signaling server/matchmaker protocol + client. Client impl under `#ifndef SAM2_CLIENT_C`, server under `#ifndef SAM2_SERVER_C`.
- [LibretroContext.cpp](Source/UnrealLibretro/Private/LibretroContext.cpp) — the plugin's core thread loop; owns the `ulnet_session_t` (`l->netplay_session`), pumps `ulnet_poll_session` / `ulnet_service_network` / `sam2_client_poll` (~lines 1546–1830). Defines `SAM2_IMPLEMENTATION`/`ULNET_IMPLEMENTATION`.
- [LibretroCoreInstance.cpp](Source/UnrealLibretro/Private/LibretroCoreInstance.cpp) / [LibretroCoreInstance.h](Source/UnrealLibretro/Public/LibretroCoreInstance.h) — Blueprint API: `NetplayHost`, `NetplaySync`, `OnNetplayRoomModified`, `OnNetplayDesync` (currently never fired), `OnNetplayError`.
- `Source/ThirdParty/netarch/` — standalone client (`netarch.cpp`) and test suite (`ulnet_test.c`) that share the SAME ulnet.h/sam2.h. Any change to ulnet.h must keep these building. `ulnet_test.c` builds two ways: single-TU (e.g. `tcc -DULNET_TEST_MAIN -run ulnet_test.c` style) and separate-TU via cmake with `ULNET_TEST_IMPLEMENTATION`; tests that call `static` helpers only work single-TU, so new externally-visible test hooks need `ULNET_LINKAGE`.
- Session model recap: `ulnet_session_t` has `peer[SAM2_TOTAL_PEERS]` and parallel opaque `transport[]`. Port 0 (`SAM2_AUTHORITY_INDEX`) is the authority. In client-server mode spectators connect ONLY to the authority; the authority relays player state packets to everyone and folds spectator "suggested input" into its own input stream. `room_we_are_in.peer_topology` bit p set = p2p player; for this beta the only topology we create is bit 0 set (authority is the sole player).
- Verification for every ulnet.h task: run the existing tests in `Source/ThirdParty/netarch/ulnet_test.c` (see build modes above) and keep `netarch.cpp` compiling. There is no automated Unreal-side test; keep the plugin building.

Tasks are ordered by priority. P0 = the disconnect work the beta is blocked on. Do them in order; several later tasks depend on the fields/paths added in P0-1.

---

## P0 — Disconnect handling (beta blockers)

### 1. Add peer liveness timeout to ulnet
**Problem:** After a transport reaches `ULNET_TRANSPORT_READY` nothing ever detects a dead peer. `ulnet__nat_poll_agent_timers` (ulnet.h ~1343) only times out DURING connection (`connect_deadline_usec`, and `state < ULNET_TRANSPORT_READY` guards). A peer whose process dies just goes silent: a spectator blocks forever in the tick gate, and the authority retransmits reliable packets to it forever.

**Change (in ulnet.h core, transport-agnostic — do NOT put this in the NAT layer):**
- Add `int64_t last_packet_receive_unix_usec;` to `ulnet_peer_t`. Set it to `ulnet__get_unix_time_microseconds()` at the top of `ulnet_receive_packet` (~4631).
- Add `int64_t peer_disconnect_timeout_usec;` to `ulnet_session_t`; default it in `ulnet_session_init_defaulted` (~4537) to 10 seconds. `0` disables the check (keep tests deterministic — verify no existing test trips it; if any does, disable via this field in the test setup).
- In `ulnet_service_network` (~3895), in the existing teardown loop over ports: for each port with a READY transport and a peer, if `last_packet_receive_unix_usec != 0` and `now - last_packet_receive_unix_usec > timeout`, set `session->peer_pending_disconnect_bitfield |= (1ULL << p)` (the existing code path right there already tears down pending-disconnect ports). If `last_packet_receive_unix_usec == 0` (nothing received yet since READY), initialize it to `now` the first time the transport is observed READY, so the clock starts at readiness rather than never.

### 2. Add keepalive so idle-but-alive links don't false-timeout
**Problem:** Traffic only flows from inside `ulnet_poll_session`. When the Unreal game thread pauses (PIE pause, synchronous-tick mode with no tick requests, load hitch) the loop still calls `ulnet_service_network(l->netplay_session, 0)` (LibretroContext.cpp:1758) but nothing is sent, so with task 1 both sides would time each other out during a long pause.

**Change:** Add `int64_t last_packet_send_local_unix_usec;` to `ulnet_peer_t` (note: the existing `last_packet_send_unix_usec` holds the REMOTE peer's echoed send time for ping estimation — do not reuse it). Stamp it in `ulnet_udp_send`. In `ulnet_service_network`, for each READY transport, if `now - last_packet_send_local_unix_usec > peer_disconnect_timeout_usec / 3`, send an empty ACK-only packet: `ulnet_reliable_send_with_acks_only(session, p, NULL, 0)`. This is already a valid wire message (the RX path treats an empty ACK-only payload as just an ACK, ~4724).

### 3. Non-authority: losing the authority must end the session, not hang it
**Problem:** When the authority's transport fails or times out, `ulnet_service_network` logs "Coordinator left" and calls `ulnet_disconnect_peer(session, 0)` — and then the spectator sits forever: `room_we_are_in` still says `SAM2_FLAG_ROOM_IS_NETWORK_HOSTED`, `ulnet_session_can_tick` (or the `ULNET_WAITING_FOR_SAVE_STATE_SENTINEL` check) blocks eternally, and the frontend is never told.

**Change:**
- Add a session event callback so the frontend hears about lifecycle failures:
  ```c
  typedef enum ulnet_session_event {
      ULNET_SESSION_EVENT_AUTHORITY_LOST = 1,   // authority link failed/timed out; session reset to solo
      ULNET_SESSION_EVENT_SAVESTATE_TIMEOUT = 2 // joined but never received the initial savestate
  } ulnet_session_event_t;
  ```
  Add `void (*session_event_callback)(void *user_ptr, int event);` to `ulnet_session_t` (NULL-checked at call sites).
- In the `ulnet_service_network` teardown loop: when the failing/timed-out port is `SAM2_AUTHORITY_INDEX` and `!ulnet_is_authority(session)`, after `ulnet_disconnect_peer`, reset to a working solo session — call `ulnet_session_tear_down(session)` then `ulnet_session_init_defaulted(session)` (this is exactly what the room-abandoned path in `ulnet_poll_session` ~4215 does), then fire `ULNET_SESSION_EVENT_AUTHORITY_LOST`. Important: `ulnet_session_init_defaulted` must not clobber frontend-configured fields — audit it; today it preserves `user_ptr`/callbacks/`nat_stun_host` implicitly because it only resets specific fields, keep it that way for the new fields too (`peer_disconnect_timeout_usec`, `session_event_callback` must survive; simplest is to set defaults only in `ulnet_session_init_defaulted` when the value is 0... no — just have init_defaulted always set the timeout default and have the frontend set the callback once after each init call; document this in the header comment).

### 4. Timeout for "waiting for savestate" after joining
**Problem:** `NetplaySync` sets `frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL` and connects to the authority. If the authority accepts the connection but the savestate transfer never completes (drop-heavy link, authority wedged), the spectator waits forever even though the transport looks READY (task 1 won't fire — the authority may keep sending state packets we can't use).

**Change:** Add `int64_t waiting_for_save_state_since_unix_usec;` to `ulnet_session_t`. In `ulnet_poll_session`, when `frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL` and the field is 0, stamp it; when a savestate is applied (`session->frame_counter = savestate_transfer_payload->frame_counter` in the RX path ~5132) zero it. If waiting longer than ~30s (constant `ULNET_WAITING_FOR_SAVE_STATE_TIMEOUT_USEC`), reset to solo exactly as in task 3 and fire `ULNET_SESSION_EVENT_SAVESTATE_TIMEOUT`.

### 5. Authority: free the room slot when a spectator dies
**Problem:** The graceful path (EXIT message, ~4682) calls `ulnet__authority_remove_peer`, which frees `peer_ids[p]` in `next_room`/`room_we_are_in` and advertises the change. The ungraceful path (transport FAILED, or timeout from task 1) only calls `ulnet_disconnect_peer` — the slot stays occupied forever: the roster shown to everyone still lists the dead peer, and the slot can fill up (`SAM2_RESPONSE_ROOM_FULL` at ~5378).

**Change:** In the `ulnet_service_network` teardown loop, when we are the authority and a non-authority port is being torn down for failure/timeout (i.e. not merely `peer_pending_disconnect` from a processed EXIT — EXIT already did the bookkeeping), call `ulnet__authority_remove_peer(session, &session->next_room, p, ulnet_port_is_p2p(&session->room_we_are_in, p))` before the disconnect. Note `ulnet__authority_remove_peer` for a spectator also sets `peer_pending_disconnect_bitfield`, which the same loop then consumes — make sure you don't double-release (guard on `session->peer[p]`/`session->transport[p]` still being non-NULL; `ulnet_disconnect_peer` asserts both).

### 6. Treat a full reliable send queue as a dead link
**Problem:** For a silent peer the authority's reliable window stops advancing; after `ULNET_RELIABLE_ACK_BUFFER_SIZE` (128) queued state packets (~2s at 60fps), `ulnet_reliable_send` fails and logs `SAM2_LOG_ERROR("Reliable send queue is full...")` every frame, forever.

**Change:** In `ulnet_reliable_send` (~3542), when the queue is full, set `session->peer_pending_disconnect_bitfield |= (1ULL << port)` and downgrade the log to WARN (it will now fire a bounded number of times because the peer gets culled next `ulnet_service_network`). With tasks 1/5 this is belt-and-braces, but it converts the failure mode from "spam + wedge" to "prompt disconnect" even with long timeouts configured.

---

## P1 — Unreal plugin surface

### 7. Add `NetplayLeave()` Blueprint function
There is currently no way to leave a room. In [LibretroCoreInstance.cpp](Source/UnrealLibretro/Private/LibretroCoreInstance.cpp) add, following the pattern of `NetplaySync` (enqueue on `NetplayTasks`):
```cpp
UFUNCTION(BlueprintCallable, Category = "Libretro|Netplay")
void NetplayLeave();
```
Implementation: if `room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED`, call `ulnet_session_tear_down(netplay_session)` (best-effort EXIT to authority is built in) then `ulnet_session_init_defaulted(netplay_session)` and re-apply frontend fields (delay_frames, callbacks — see task 3 note). The existing room-changed broadcast in LibretroContext.cpp (~1765) will notify Blueprint automatically.

### 8. Surface disconnect events to Blueprint
Wire the `session_event_callback` from tasks 3/4. In LibretroContext.cpp where the session is configured (~1661, next to `sam2_send_callback`), set the callback; it runs on the core thread, so marshal to the game thread the same way the room-modified broadcast does (`FFunctionGraphTask::CreateAndDispatchWhenReady`). Deliver through the existing `OnNetplayError` delegate with a distinct message/code per event (e.g. code = the `ulnet_session_event_t` value, negative-offset to avoid colliding with `SAM2_RESPONSE_*`), or add a dedicated `OnNetplayDisconnected` delegate — dedicated delegate preferred, it's one line in the header.

### 9. Fire `OnNetplayDesync` (it is declared but never broadcast)
`ulnet__check_for_desync` maintains `session->peer[our_port]->desynced_frame` (nonzero while desynced). In the LibretroContext core loop, next to the room-modified check (~1765), track the previous value; on a 0 → nonzero transition, broadcast `OnNetplayDesync(desynced_frame)` on the game thread. (In pure client-server topology this can't fire — desync checks require both sides to be p2p players — but wiring it is cheap and it removes a dead public API.)

### 10. Make `delay_frames` configurable
Hardcoded `l->netplay_session->delay_frames = 2; // @todo Make configurable` at LibretroContext.cpp:1549. Add `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Libretro|Netplay", meta=(ClampMin="0", ClampMax="3"))` `int32 NetplayDelayFrames = 2;` on `ULibretroCoreInstance` and pass it through at session setup. Clamp to `[0, ULNET_DELAY_FRAMES_MAX]` in C++ too, not just metadata.

### 11. Fail fast when not connected to sam2
`NetplaySync`/`NetplayHost` enqueue work that silently does nothing useful when `l->connected_to_sam2` is false (`NetplaySync`'s ICE handshake needs sam2 signaling; it burns the 5s NAT timeout then fails). At the top of the enqueued task, if `!CoreInstance->connected_to_sam2 || CoreInstance->sam_socket == SAM2_SOCKET_INVALID`, broadcast `OnNetplayError` ("Not connected to signaling server") on the game thread and return.

---

## P2 — Duplicate-code cleanup

### 12. Unify the tick-readiness logic
`ulnet_session_can_tick` (ulnet.h ~3816) and the ImGui-annotated block inside `ulnet_poll_session` (~4086–4111) compute the exact same four gates independently (savestate sentinel, per-port frame availability, local buffered frames, authority snapshot freshness). Refactor: make the single source of truth a function returning a reason bitmask, e.g. `int ulnet__tick_gates(ulnet_session_t*, int64_t *out_missing_port)` with bits like `GATE_WAITING_SAVESTATE / GATE_MISSING_INPUT / GATE_BUFFERING / GATE_AWAITING_AUTHORITY_SNAPSHOT`; `ulnet_session_can_tick` becomes `gates == 0`; the poll-session ImGui block prints from the mask. Behavior must be identical — the asserts at ~4097 stay.

### 13. Extract spectator-input aggregation
The "OR every connected peer's `spectator_suggested_input_state` into an input block" loop exists twice: in `ulnet_poll_session` (~3983–4011, folding into the local player's input + port mask) and in the `ULNET_CHANNEL_SPECTATOR_INPUT` relay handler (~4886–4908, aggregating for forwarding). Extract a helper like `static uint8_t ulnet__aggregate_spectator_input(ulnet_session_t*, bool players_only_excluded, ulnet_input_state_t out[ULNET_PORT_COUNT])` returning the nonzero port mask; parameterize the peer filter (poll path: every connected peer; relay path: spectators only).

### 14. Extract the reliable TX-window walk
The `for (sequence = tx_head; sequence_less_than(sequence, tx_next) && (sequence - tx_head) < WINDOW; sequence++)` iteration over in-flight slots appears three times: retransmit pass in `ulnet_service_network` (~3925), deadline computation in `ulnet__cap_timeout_for_reliability` (~3874), and `ulnet__reliable_pump_window` (~3526). At minimum share the deadline expression (`last_send == 0 ? now : last_send + retransmit_delay`) between the first two so retransmit timing and sleep capping can't drift apart; a tiny macro/inline iterator for the loop header is fine.

### 15. Merge the two savestate packet structs
`ulnet_save_state_packet_header_t` (~230) and `ulnet_save_state_packet_fragment2_t` (~243) duplicate the same 3-byte header field-for-field; fragment2 just adds the max-size payload array. Give fragment2 (rename to `ulnet_save_state_packet_t`) the payload and delete the duplicated field block by embedding or by defining the header once. Keep the existing `SAM2_STATIC_ASSERT(sizeof == ULNET_PACKET_SIZE_BYTES_MAX)` and re-check every `sizeof(ulnet_save_state_packet_header_t)` call site is unchanged in value.

### 16. Delete the duplicated "empty ROM path" check in LibretroContext.cpp
Lines ~1606–1611 and ~1625–1631 are the same `!supports_no_game && game.IsEmpty()` guard; the first copy doesn't set `l->ErrorMessage`. Keep the second (complete) one, delete the first.

### 17. Use the LE16 helpers consistently in the reliable channel
The reliable sequence/ack fields are declared `uint8_t [2]` little-endian, but parsing is ad hoc and one path is host-endian:
- `ulnet_receive_packet` ~4648: `((uint16_t)packet[2] << 8) | packet[1]` (manual LE).
- ACK parse ~4707: manual LE from the array.
- `ulnet__wrap_packet` ~3471 and the RX in-order delivery ~4737/~4763: `memcpy` of a host-endian `uint16_t` — wrong on big-endian, and `ulnet__reliable_transmit_slot` ~3498 `memcmp`s a host-endian value against the wire field.
Replace all of these with `ulnet__read_le16`/`ulnet__write_le16` (already defined ~613). Pure consistency/portability; no wire change on little-endian hosts. The reliable-channel tests in `ulnet_test.c` must pass unchanged.

### 18. Namespace the stray globals
`core_wants_tick_in_seconds` (~3091) and `ulnet__rdtsc` (~2734) are non-static and un-prefixed / half-prefixed. Both are used by `Source/ThirdParty/netarch/netarch.cpp`, so they must stay externally linkable: give `core_wants_tick_in_seconds` a `ulnet_` prefix and `ULNET_LINKAGE`, declare both in the header section, and update netarch.cpp call sites (~3456, ~3708 and the `ulnet__rdtsc` uses).

---

## P3 — Protocol/misc loose ends (optional, safe to defer)

### 19. Investigate the `ulnet_is_authority` fallback
`|| room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == 0` (~443) is flagged `@todo I don't think this extra check should be necessary`. `ulnet__reset_to_local_solo` always seats `our_peer_id` at port 0, so the fallback should only matter before the sam2 CONN message assigns a peer id (both sides are 0 then, and the check is a tautology with the first clause). Trace the pre-CONN window; if truly redundant, delete it and run the full test suite. If not, replace the @todo with a comment explaining exactly when it fires. Do NOT just delete without the trace.

### 20. Explicit non-goal: no P2P surface
Do not add Blueprint/UI affordances for `peer_topology` changes (player promotion, port claiming, JOIN topology toggles). The wire protocol keeps the topology bitfield — it's shared with the standalone netarch client — but the plugin only ever creates rooms with topology bit 0 set (`NetplayHost`, LibretroCoreInstance.cpp:114) and joins as spectator (`NetplaySync`). The "@todo ... should force a resync" p2p-disconnect branch in `ulnet_service_network` (~3910) stays a logged error; unreachable through the plugin surface.

### 21. Logging hygiene in the savestate RX path
- ~4945: raw `printf("Received savestate transfer packet from non-authority agent\n")` → `SAM2_LOG_WARN`.

### 22. Delete the dead `ULNET_SESSION_FLAG_READY_TO_TICK_SET`
The flag (~96) is checked in `ulnet_receive_packet` (~4657, logs an ERROR) but never set anywhere in the codebase. Delete the flag definition and the check.

---

## Verification pass (after P0)

**Automated:** `Source/ThirdParty/netarch/ulnet_disconnect_test.sh` (also wired into CI in netarch.yml)
runs real sam2 + ICE loopback peers built from `ulnet_test.c` (`--disconnect-authority` /
`--disconnect-spectator` modes) and covers, end to end:
1. **spectator-killed** — SIGKILL the spectator → host's liveness timeout reclaims the room slot.
2. **spectator-leaves** — graceful EXIT → host reclaims the slot.
3. **authority-killed** — SIGKILL the host → spectator resets to solo, `AUTHORITY_LOST` event fires.
4. **savestate-timeout** — host admits but never syncs → spectator's savestate-wait deadline fires.
5. **pause-keepalive** — host stops ticking 3.5× the liveness timeout → keepalives hold the link and
   the spectator resumes ticking afterwards.

**Still manual (Unreal-specific):** two PIE instances with `NetplayHost`/`NetplaySync`/`NetplayLeave`
to confirm the Blueprint events (`OnNetplayError` code = `ulnet_session_event_t`) fire on the game
thread, and that a dedicated-server / `-nullrhi` client runs the core headless (video paths are
gated on `FApp::CanEverRender()`).
