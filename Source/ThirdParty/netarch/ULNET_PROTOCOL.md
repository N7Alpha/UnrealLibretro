# ULNET Protocol Draft

Status: reverse-engineered from the current implementation. This document specifies the current
ad hoc ULNET protocol at the level needed to reason about netplay correctness. It is not a code
walkthrough.

Signaling, NAT traversal, savestate compression, FEC, and zstd are auxiliary to this protocol. They
are mentioned only where they affect core ULNET state.

## Conventions

The words MUST, MUST NOT, SHOULD, and MAY are used in the RFC sense.

Unless stated otherwise, integers on the wire are little-endian. The current implementation also
serializes some packed C structs after RLE compression; a portable future protocol should replace
those native-layout payloads with explicit field encodings.

Current constants:

```text
ULNET_PACKET_SIZE_BYTES_MAX   = 1408
SAM2_AUTHORITY_INDEX          = 0
SAM2_TOTAL_PEERS              = 64
ULNET_PORT_COUNT              = 8
ULNET_DELAY_BUFFER_SIZE       = 8
ULNET_DELAY_FRAMES_MAX        = 3
ULNET_ROOM_CHANGE_LEAD_FRAMES = 4
SAVESTATE_DEFAULT_RATE        = 8 Mibit/s
SAVESTATE_MAX_RETRIES         = 2
```

## Room Model

A room is a 64-slot peer table. Slot 0 is always the authority slot.

```text
sam2_room
+0    char[64]   name
+64   char[32]   core_and_version
+96   u32        rom_hash
+100  u32        flags
+104  u16[64]    peer_ids
+232  u64        peer_topology
```

A port is occupied when `peer_ids[p]` is greater than the reserved sentinel peer IDs. A port is a
p2p player when it is occupied and bit `p` is set in `peer_topology`.

Roles:

- Authority: the peer in port 0. It owns room consensus and publishes authoritative room snapshots.
- Player: an occupied p2p port. Players publish deterministic input state.
- Active player: a player not marked inactive by room flags. Only active players gate frame advance.
- Spectator: an occupied non-authority port whose topology bit is clear. Spectators suggest input but
  do not directly contribute deterministic input state.
- Coordinator-only authority: authority with topology bit 0 clear. It is not an active player, but it
  still publishes the authority state stream used as a room snapshot heartbeat.

Ports are stable identities. A peer MUST NOT move from one port to another as part of a room-change
request. A role change at the same port preserves that port identity. A changed occupant at a port is
a new identity for that port and resets per-port protocol state.

## Connectivity Model

ULNET assumes this logical connection policy:

- Authority has a link to every occupied port.
- Every occupied peer has a link to the authority.
- P2P players have direct links to other p2p players.
- Spectators do not require direct links to non-authority players.

The authority relays player state packets to other peers. Spectators depend on this relay. P2P
players MAY also receive the same state directly from the original player; duplicate frame-stamped
state is harmless because stale frames are dropped.

Transport establishment is out of scope for this document.

## Packet Header

Every ULNET UDP payload starts with one channel byte. The top three bits select a channel.

```text
Generic header byte

  7 6 5 4 3 2 1 0
 +-+-+-+-+-+-+-+-+
 | ch  |   flags |
 +-+-+-+-+-+-+-+-+

channel = byte & 0xe0
flags   = byte & 0x1f
```

Defined channels:

```text
0x00  reserved / invalid
0x40  ASCII control message
0x60  spectator input
0x80  savestate transfer
0xa0  reliable wrapper
0xc0  input/state, low port range
0xe0  input/state, high port range
```

For input/state packets, the low six bits encode the original sender port:

```text
Input/state header byte

  7 6 5 4 3 2 1 0
 +-+-+-+-+-+-+-+-+
 |1|1|p p p p p p|  ports 0..63 across 0xc0 and 0xe0 channel ranges
 +-+-+-+-+-+-+-+-+

port = byte & 0x3f
```

The current encoder emits `0xc0 | port`; ports 32..63 therefore use the `0xe0` channel range.

## Reliable Wrapper

Most ULNET protocol payloads are carried inside the reliable wrapper. There are two reliability
profiles:

- Reliable data: consumes sequence numbers and is retransmitted until acknowledged.
- ACK-only payload: carries an ACK and a payload, but does not consume sequence numbers and is not
  retransmitted by the reliable queue.

```text
Reliable wrapper

+0   u8    channel_and_flags = 0xa0 | flags
+1   u16   sequence_le
+3   u16   ack_sequence_le
+5   ...   inner_payload
```

`flags & 0x10` marks ACK-only. For ACK-only wrappers, `sequence_le` is ignored.

Each directed peer pair has an independent 16-bit sequence space. `ack_sequence_le` is the receiver's
next expected reliable data sequence from the peer, not the last received sequence.

Reliable data delivery rules:

- A sender MUST assign monotonically increasing 16-bit sequence numbers to reliable data packets.
- A sender MUST send or retransmit only the current transmit head until it is acknowledged.
- A receiver MUST process reliable data only when `sequence == expected_rx_sequence`.
- A receiver MUST advance `expected_rx_sequence` after processing in-order reliable data.
- A receiver MUST ignore old reliable data.
- A receiver MUST NOT process future reliable data before missing earlier reliable data.
- An ACK outside the sender's current transmit window MUST be ignored.
- When an ACK advances the transmit head, the sender SHOULD immediately try to send the next queued
  reliable data packet.

Current state traffic usage:

- Every 8th state packet is reliable data.
- Other per-frame state packets are ACK-only payloads.
- Spectator input packets are ACK-only payloads.
- ASCII control messages use reliable data.

## State Packet

State packets carry a complete compressed logical state for one port.

```text
State packet inner payload

+0    u8    input_state_header = 0xc0|port or 0xe0|port
+1    i64   ping_send_unix_usec_le
+9    i64   ping_echo_send_unix_usec_le
+17   i64   ping_echo_callsite_receive_unix_usec_le
+25   i64   ping_echo_kernel_receive_unix_usec_le
+33   ...   rle8(ulnet_state)
```

The ping fields are auxiliary timing metadata. They do not affect deterministic simulation.

Logical decoded state:

```text
ulnet_state

int64   frame
int64   room_effective_frame
i16     input_state[8][8][64]
room    room
option  core_option[8]
int64   save_state_frame
u32     save_state_hash[8]
u32     input_state_hash[8]
int64   input_poll_unix_usec[8]
```

`frame` is the newest frame for which this port has buffered state. Input for simulation frame `F`
is read from `input_state[F % ULNET_DELAY_BUFFER_SIZE]`.

Only the authority port's `room` and `room_effective_frame` are authoritative for room consensus.
Non-authority state packets may carry local room fields, but peers MUST NOT use them to adopt room
changes.

State packet acceptance rules:

- If the immediate transport sender is not the original sender port, the immediate sender MUST be the
  authority.
- A spectator MUST NOT send state packets.
- The original sender port MUST be the authority or a current p2p player.
- A receiver MUST drop a state packet whose decoded `frame` is older than the receiver's stored frame
  for that original sender port.
- A receiver MAY accept an equal or newer state packet and replace stored state for that port.
- The authority SHOULD relay accepted player state to other linked peers while preserving the original
  sender port in the state packet header.

Every 8th state frame, where `(frame + 1) % 8 == 0`, is stored as state history and sent as reliable
data. This history is used by spectators to reconstruct state and by the reliable path to recover from
loss. Other state packets are sent frequently as ACK-only payloads.

## Spectator Input Packet

Spectator input is advisory. It is not deterministic state by itself. A player or authority may fold
spectator suggestions into that player's next published input state, at which point the player state
becomes authoritative for deterministic simulation.

Current spectator input payload:

```text
Spectator input inner payload

+0    u8    channel = 0x60
+1    u8[32] reserved_zero_padding
+33   ...   rle8(suggested_input_state[8][64])
```

On receipt, the payload is stored under the immediate transport sender's port. Players OR-merge
suggestions from connected peers into the next local input frame before publishing state.

The reserved padding exists because the current sender and receiver use the state-packet header size
as the encoded-input offset. The protocol SHOULD treat bytes 1..32 as reserved and set them to zero.

## Input Semantics

For each simulation frame, deterministic input is the bitwise OR of all active players' buffered
input for that frame. Any active player may drive any of the eight controller ports.

```text
for each active player P:
  for controller C in 0..7:
    input[C] |= state[P].input_state[frame % 8][C]
```

Each local p2p player buffers future input until its state reaches:

```text
state[our_port].frame >= local_frame_counter + delay_frames
```

The input ring has eight slots. A peer MUST NOT advance using active-player state that is older than
the target frame or at least eight frames ahead of the target frame.

## Frame Advancement

A session may tick simulation frame `F` only when all of these conditions hold:

```text
frame_counter != WAITING_FOR_SAVE_STATE

for each active player P:
  state[P].frame >= F
  state[P].frame <  F + ULNET_DELAY_BUFFER_SIZE

if this peer is a p2p player:
  state[our_port].frame - F + 1 >= delay_frames

if this peer is network-hosted and not authority:
  required_authority_frame = F + 1 - ULNET_ROOM_CHANGE_LEAD_FRAMES
  required_authority_frame <= 0
    or state[authority].frame >= required_authority_frame
```

The final authority freshness rule is independent of whether the authority is an active player. It is
the rule that prevents non-authority peers from running past an unseen room-change boundary.

After a successful tick, the peer evaluates room adoption for the boundary between frame `F` and
`F + 1`, then increments `frame_counter`.

## Room Changes

The authority owns room changes. A peer requests a room change, but only the authority decides and
publishes the resulting room snapshot.

There are two classes of room change.

Immediate changes:

- new peers admitted as spectators;
- spectator leaves;
- some room-level flag changes by the authority.

Scheduled active-set changes:

- spectator promoted to player;
- player demoted to spectator;
- player leaves;
- authority enters or leaves the active player set.

Active-set changes are scheduled because every peer must stop waiting for old active-player input, or
start waiting for new active-player input, on the same simulation boundary.

The authority maintains a committed room and a pending room. At most one scheduled active-set change
may be pending. If another active-set request arrives while one is pending, it is ignored rather than
queued.

Scheduling:

```text
advertise_frame = max(
  authority_state_frame,
  last_sent_authority_room_snapshot_frame + 1,
  1
)

room_effective_frame = advertise_frame + ULNET_ROOM_CHANGE_LEAD_FRAMES
```

Publication:

```text
if authority_state_frame < advertise_frame:
  publish committed_room, room_effective_frame = 0
else:
  publish pending_room, room_effective_frame
```

Adoption:

```text
if incoming_room.authority_peer_id == committed_room.authority_peer_id
and incoming_room != committed_room
and frame_counter + 1 >= incoming_room_effective_frame:
  committed_room = incoming_room
```

For the current constants, active-set changes are advertised at least four frames before they take
effect. The authority MUST NOT first advertise a scheduled room change at frame 0.

Coordinator-only authority behavior is a consequence of the same rules. When authority topology bit 0
is clear, the authority does not provide deterministic input, but it still advances and publishes its
authority state stream. Non-authority peers may run ahead of coordinator input because it is not
active-player input, but they MUST NOT run beyond the authority room-snapshot freshness boundary.

## Control Messages

ASCII control messages are carried as reliable data. The core ULNET room-control messages are:

```text
EXIT...   peer intends to leave
JOIN...   peer requests a room/topology change
```

Only the authority handles room-change requests. Requests to move a peer to a different port are
unsupported. Transport signaling and room-list/make/connect messages are auxiliary and are not
specified here.

## Savestate Sync

Savestate sync is an auxiliary mechanism for bringing a peer to a known frame and room. A peer may be
held at `WAITING_FOR_SAVE_STATE` until it installs a savestate. The savestate payload includes a frame
counter and room snapshot, after which normal frame-advance and room-snapshot rules resume.

Savestate transfer is authority-originated. A non-authority peer MUST NOT send savestate data packets.
The authority MUST have at most one active savestate transfer at a time. The active transfer has a
transfer ID and a target peer bitfield.

Savestate data packets use the savestate channel. The channel flags identify normal data packets or
transfer ACK packets.

Savestate channel flags:

```text
bit 0     k_is_239
bit 1     sequence_hi_is_0
bit 2     transfer_ack
bits 3-4  transfer_id modulo 4
```

```text
Savestate data packet

+0   u8   channel_and_flags = 0x80 | flags
+1   u8   transfer parameter:
         packet_groups when k=239 and sequence_hi=0
         sequence_hi otherwise when k=239
         reed_solomon_k otherwise
+2   u8   sequence_lo
+3   ...  fragment payload
```

```text
Savestate transfer ACK

+0   u8   channel_and_flags = 0x80 | transfer_ack | transfer_id
+1   u8   reserved_zero
+2   u8   reserved_zero
```

The transfer is divided into packet groups. A packet group is the smallest transmission-control unit:
all packets in a group are sent together. The sender SHOULD pace packet groups against the assumed
total outbound savestate rate. The default rate is 8 Mibit/s.

A receiver MUST send a transfer ACK only after the whole savestate has been accepted and installed.
A receiver MUST ignore remaining data packets from a transfer ID that it has already installed.

The authority considers a target peer complete when it has received that peer's transfer ACK. If the
authority has sent all packet groups and a target peer has not completed before the ACK deadline,
that peer is failed for this attempt.

On attempt failure, the authority starts a new savestate transfer rather than reusing the previous
payload. The retry target set is:

```text
failed_peers | peers_already_awaiting_savestate
```

Each retry uses a new transfer ID and halves the previous savestate rate for the whole retry target
set. After two retries, remaining failures are terminal for the network session; the peer handling
policy is to leave network play and continue solo.

The compression format, FEC layout, fragmentation payload contents, checksums, and zstd details are
out of scope.

## Blindspots And Open Protocol Questions

- Identity is trusted. There is no signature or cryptographic binding between a transport peer,
  claimed sender port, and peer ID.
- ACK-only state packets are not retransmitted. Correctness relies on frequent state plus every-8th
  reliable state history.
- Reliable delivery is head-of-line with a small fixed history. Queue saturation or overwritten
  reliable history is fatal or lossy depending on path.
- If a state packet cannot fit after compression, it is skipped. Repeated poor compression could
  starve room snapshots or reliable history.
- Overlapping active-set room changes are ignored rather than queued or rejected with a protocol
  response visible to the requester.
- Spectator input uses state-header-sized padding. This should be either specified intentionally or
  replaced by a compact packet format.
- The RLE8 stream and native-layout state structs are de facto wire format, but the protocol does not
  yet define a standalone portable encoding.
- Out-of-order reliable data is treated as a protocol error but does not currently mandate disconnect
  or resync.
- Desync hash fields exist, but input hash production appears incomplete.
- Savestate installation directly sets frame and room state; the exact reconciliation required after
  install is not fully specified here.
