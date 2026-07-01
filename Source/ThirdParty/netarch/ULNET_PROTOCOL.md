# ULNET Protocol

Status: draft, unversioned. This document distills the protocol implemented by `ulnet.h` and
`sam2.h` (which will eventually merge into one file). It specifies peer identity, room membership,
message formats, and the rules that keep deterministic lockstep emulation in sync. Transport
establishment (NAT traversal, ICE, sockets) is deliberately out of scope: the protocol assumes an
unreliable, unordered datagram link of at most `ULNET_PACKET_SIZE_BYTES_MAX` bytes between peers
that the room says should be connected, plus a TCP connection from each peer to a SAM2 signaling
server.

## 1. Conventions

The key words MUST, MUST NOT, SHOULD, and MAY are used in the RFC 2119 sense.

All multi-byte integers on the wire are little-endian. SAM2 messages and the room record are
serialized as packed little-endian C structs (packing is asserted at compile time; big-endian
hosts are unsupported).

```text
ULNET_PACKET_SIZE_BYTES_MAX    = 1408   max datagram payload
SAM2_TOTAL_PEERS               = 64     ports per room
SAM2_AUTHORITY_INDEX           = 0      the authority's port
ULNET_PORT_COUNT               = 8      emulated controller ports
ULNET_DELAY_FRAMES_MAX         = 3      max local input delay
ULNET_ROOM_CHANGE_LEAD_FRAMES  = 4      room-change advertisement lead
ULNET_STATE_PACKET_HISTORY_SIZE = 64    per-port retained state frames
```

## 2. Architecture

Three kinds of participant:

- **SAM2 server**: a stateless-ish rendezvous service reached over TCP. It assigns peer IDs,
  stores one advertised room per hosting peer, and relays opaque connection signals between
  peers. It never sees game traffic.
- **Authority**: the peer occupying port 0 of a room. It owns room membership consensus, relays
  player state, and originates savestate synchronization.
- **Peers**: everyone in the room, occupying ports 1..63 (or 0 for the authority). A peer is a
  *player* (contributes deterministic input) or a *spectator* (receives state, may suggest input).

Connectivity policy (who holds a datagram link to whom):

- The authority holds a link to every occupied port.
- Every peer holds a link to the authority.
- Players hold direct links to each other (full mesh among players).
- The peer with the lesser peer ID initiates each connection.

The authority relays every player's state to all other peers, so the mesh links are a latency
optimization: duplicate frame-stamped state is harmless because stale frames are dropped.

## 3. Peers, Ports, and Rooms

A **peer ID** is a nonzero `uint16` assigned by the SAM2 server. Values 0 and 1 are reserved
sentinels (`PORT_AVAILABLE`, `PORT_UNAVAILABLE`); a port is *occupied* when its entry is greater
than the sentinels.

A **room** is a 64-slot port table plus metadata:

```text
sam2_room (232 bytes)
+0    char[64]   name
+64   char[32]   core_and_version
+96   u32        rom_hash
+100  u32        flags          bit 6: ROOM_IS_NETWORK_HOSTED
+104  u16[64]    peer_ids       port 0 is the authority
+232  u64        peer_topology  bit p set => port p is a player
```

Room identity is the authority's peer ID (`peer_ids[0]`). Two room snapshots describe the same
room iff that field matches.

Roles derived from the room:

- **Player**: an occupied port whose topology bit is set. Players publish deterministic input.
- **Spectator**: an occupied non-authority port whose topology bit is clear.
- **Coordinator-only authority**: the authority with topology bit 0 clear. It contributes no
  input but still publishes the authority state stream (the room-consensus heartbeat).

Ports are stable identities. A peer MUST NOT move between ports; a role change happens in place
by flipping the port's topology bit. When a port's occupant changes, all protocol state for that
port (sequence numbers, state history, suggestions) is reset — a new occupant is a new identity.

A local (non-network-hosted) session is a degenerate room: the local peer is authority and sole
player, and unknown-peer signals MUST NOT be admitted.

## 4. SAM2 Messages

SAM2 messages are fixed-size structs over TCP, framed by an 8-byte ASCII header:

```text
bytes 0-3   tag     ("MAKE", "LIST", "JOIN", "CONN", "SIGN", "FAIL", "EXIT")
byte  4     major version digit
byte  5     '.'
byte  6     minor version digit
byte  7     'r'     (encode marker: raw struct)
```

Receivers match on tag + major version only; a major-version mismatch rejects the message.

| Tag  | Body | Direction | Semantics |
|------|------|-----------|-----------|
| CONN | `u16 peer_id`, `u16 flags[3]` | server→client, client→server | On connect the server assigns the client a peer ID. A client MAY request a different unused ID (refused while hosting a room). |
| MAKE | `sam2_room` | client↔server | Register or update the room hosted by the sender. The server overwrites `peer_ids[0]` with the sender's ID and echoes the room back. |
| LIST | `sam2_room` | client↔server | Room iteration: the request's `peer_ids[0]` is a minimum authority ID; the response is the first hosted room whose authority ID is >= it (a zeroed room means end of list). |
| SIGN | `u16 peer_id`, `char[246] signal` | client↔server↔client | Relay an opaque transport signal to `peer_id`. The server rewrites `peer_id` to the sender's ID before forwarding, so the recipient learns who is signaling. |
| FAIL | `u16 peer_id`, `char[238] description`, `i64 code` | any | Error report with a negative code (invalid args, room full, version mismatch, ...). |
| JOIN | `sam2_room` | peer→authority | A room-change request (section 9). Carried peer-to-peer over the ULNET ASCII channel, not through the server; the sender is implicit in the connection it arrives on. |
| EXIT | header only | peer→authority | The sender intends to leave the room (section 9). ULNET ASCII channel only. |

Joining a room is therefore: `LIST` to discover it, `SIGN` the authority to establish a datagram
link (the authority admits unknown signalers as spectators, assigning the lowest free port), then
optionally `JOIN` to request promotion to player.

## 5. ULNET Datagrams

Every ULNET datagram begins with one channel byte; the top three bits select the channel:

```text
  7 6 5 4 3 2 1 0
 +-+-+-+-+-+-+-+-+
 | ch  |  flags  |
 +-+-+-+-+-+-+-+-+

0x00  reserved / invalid
0x40  ASCII control message (JOIN, EXIT)
0x60  spectator input
0x80  savestate transfer
0xa0  reliable wrapper
0xc0  state, sender ports 0..31   (low 6 bits = original sender port)
0xe0  state, sender ports 32..63
```

The two state channels form one 6-bit port field: receivers accept either top-bit pattern and
take `byte & 0x3f` as the original sender's room port.

## 6. Reliable Delivery

The reliable wrapper provides ordered, exactly-once delivery per directed peer pair:

```text
+0   u8    0xa0 | flags        flag 0x10: ACK-only
+1   u16   sequence
+3   u16   ack_sequence
+5   ...   inner payload (any ULNET datagram)
```

Two profiles share the wrapper:

- **Reliable data** consumes a sequence number and is retransmitted until acknowledged.
- **ACK-only** (flag 0x10) carries a cumulative ACK plus an optional latest-only payload; it
  consumes no sequence number and is never retransmitted. `sequence` is ignored.

Rules:

- Each directed peer pair has an independent 16-bit sequence space. A sender MUST number reliable
  data monotonically and MAY keep a bounded window of unacknowledged packets in flight.
- `ack_sequence` is cumulative: it names the receiver's *next expected* sequence. Receiving it
  releases every lower in-flight packet. An ACK outside the sender's outstanding range MUST be
  ignored.
- A receiver MUST deliver reliable data in sequence order, buffering out-of-order arrivals. A
  sequence below the expected one is a duplicate and MUST be answered with an empty ACK-only
  packet (so the sender stops retransmitting). A sequence too far ahead to buffer is dropped;
  retransmission redelivers it.
- After delivering reliable data, a receiver SHOULD emit an empty ACK-only packet rather than
  wait for application traffic to piggyback the ACK — a peer blocked at its tick gate may have
  nothing else to send, and withholding the ACK would deadlock both windows.

Channel usage: state packets and ASCII control messages travel as reliable data; spectator input
travels as an ACK-only payload.

## 7. State Packets

A state packet carries exactly one simulation frame of one port's input, plus session metadata.
It has no magic bytes: the channel byte, structural validation, and the SAM2 major version gate
it (intentional — do not add magic).

```text
State packet
+0    u8    0xc0|port or 0xe0|port
+1    i64   ping_send_time                  }  four unix-microsecond timestamps
+9    i64   ping_echo_send_time             }  implementing an RFC 5905-style
+17   i64   ping_echo_receive_time          }  ping/clock-offset exchange;
+25   i64   ping_echo_kernel_receive_time   }  auxiliary, restamped per link
+33   i64   frame                    the simulation frame this packet describes
+41   i64   room_effective_frame     boundary for the advertised room (authority only)
+49   i64   save_state_frame         latest frame the sender hashed its savestate at
+57   i64   input_poll_time          unix usec the input was polled (auxiliary)
+65   u32   save_state_hash          xxh32 of that savestate (desync check)
+69   u32   input_state_hash         reserved (production incomplete)
+73   u8    flags                    bit 0: ROOM_PRESENT   bit 1: CORE_OPTION_PRESENT
+74   ...   input block
+...  room                           if ROOM_PRESENT (authority only)
+...  u8 key_len, u8 value_len,      if CORE_OPTION_PRESENT: one synchronized
      key, value                     core-option change taking effect at `frame`
```

**Input block** (shared with spectator input):

```text
+0   u8       port_mask       bit N: emulated controller port N is present
+1   14B × popcount(mask)     packed frames, ascending port order

Packed input frame (14 bytes)
+0   u16      button bits, one per joypad id 0..15 (digital: nonzero => pressed)
+2   s16[6]   analog axes/triggers (libretro input ids 33..38)
```

Semantics:

- Every player publishes one state packet per simulation frame it buffers, queued as reliable
  data on every link. Each frame's packet is immutable once produced; a receiver whose link came
  up late is replayed the missing frames in order.
- Only the authority sets ROOM_PRESENT; `room`/`room_effective_frame` are meaningful only from
  it (section 8).
- At most one core option changes per frame, taken from the authority's stream. The reserved key
  `netplay_delay_frames` retunes the input delay; any other key updates the synchronized
  core-option table. Both take effect at that exact frame on every peer.

Acceptance rules:

- If the immediate link is not the original sender's port, the immediate sender MUST be the
  authority (relay). Spectators MUST NOT send state packets, and the original port MUST be the
  authority or a current player.
- A packet whose `frame` is older than the newest accepted frame for that port is dropped
  (normal UDP reordering).
- Every accepted packet is retained in a per-port history of the last
  `ULNET_STATE_PACKET_HISTORY_SIZE` frames. Simulation reads input for frame F from this history
  by *exact* frame — never "latest".
- The authority relays every accepted player packet, unmodified except for per-link ping
  restamping, to every other connected peer as reliable data.
- If a peer needs a frame older than the sender's retained history, replay is impossible; the
  sender flags that peer for savestate resync instead (section 10).

## 8. Deterministic Simulation

Deterministic input for frame F is the bitwise OR, over every player port, of that port's input
block for exactly frame F. Any player may drive any subset of the eight controller ports; each
publishes a configurable local port mask (default: port 0 only, so all peers OR onto controller
0; a mask of 0xff publishes all ports).

**Input delay.** A player buffers its own input `delay_frames` (0..3) ahead of the frame it is
simulating, publishing state for frame `F + delay` while ticking F. This hides one round trip of
latency.

**Tick gate.** A peer may run simulation frame F only when all of the following hold:

1. It is not waiting for a savestate (section 10).
2. Every player port has exact-frame-F input in history.
3. If the peer is itself a player: it has buffered at least `delay_frames` of its own input
   beyond F.
4. If the peer is in a network-hosted room and is not the authority: the authority's published
   frame is at least `F + 1 - ULNET_ROOM_CHANGE_LEAD_FRAMES`. This holds regardless of whether
   the authority is a player; it is what stops peers from running past a room-change boundary
   they have not seen advertised.

After ticking F, the peer evaluates room adoption for the F → F+1 boundary (section 8.1), then
increments its frame counter.

### 8.1 Room Consensus

The authority is the only writer of room membership. It keeps a *committed* room (what everyone
is simulating with) and a *desired* room (edits not yet in effect), and advertises the desired
room in its state stream. Everyone — including the authority — switches at the same frame
boundary.

Room changes fall into two classes:

- **Immediate** (no effect on the deterministic input set): admitting a new spectator, a
  spectator leaving, authority flag changes. These may apply without scheduling.
- **Scheduled** (active-set changes): promoting a spectator to player, demoting a player,
  a player leaving, the authority toggling its own topology bit. Every peer must start or stop
  waiting for that port's input on the same frame, so these are advertised ahead of time.

Scheduling (authority):

```text
advertise_frame       = max(authority_published_frame,
                            last_room_snapshot_frame_sent + 1,
                            1)
room_effective_frame  = advertise_frame + ULNET_ROOM_CHANGE_LEAD_FRAMES
```

At most one scheduled change may be pending; further active-set requests arriving meanwhile are
ignored. Until its published frame reaches `advertise_frame`, the authority publishes the
committed room with `room_effective_frame = 0` (no change advertised); from then on it publishes
the desired room and the boundary.

Adoption (every peer, after ticking frame F): take the most recent advertised room from the
authority's state history with a nonzero effective frame. If its authority matches the committed
room's authority, it differs from the committed room, and `F + 1 >= room_effective_frame`, commit
it. Newly present player ports are treated as starting no earlier than the adopter's own frame,
and links are opened/closed to match the new room. If the adopted room is no longer
network-hosted, the room was abandoned and each peer returns to a solo session.

## 9. Peer Control Messages

ASCII control messages travel as reliable data on the ASCII channel and reuse the SAM2 header
format:

- **EXIT** (header only): the sender is leaving. The authority removes it from the desired room —
  immediately for a spectator, via a scheduled change for a player.
- **JOIN** (`sam2_room` body): a room-change request; only the authority handles it. The request
  expresses the sender's desired view of its *own* port: absent means leave, topology bit
  toggled means promote/demote (scheduled). Port moves are unsupported and rejected. The
  authority may also send itself a JOIN to change room flags (e.g. clearing
  ROOM_IS_NETWORK_HOSTED abandons the room).

## 10. Savestate Synchronization

Savestate transfer brings a peer to an exact (frame, room, core options, machine state) so it can
join the deterministic stream. A joining peer holds its frame counter at a WAITING_FOR_SAVE_STATE
sentinel until a savestate installs. Peers that fall behind the state-packet history are resynced
the same way.

Only the authority sends savestate data. It maintains at most one active transfer, identified by
a 2-bit transfer ID (incremented per transfer) and a target-peer bitfield.

**Payload** (checksummed byte stream, reassembled from packets):

```text
+0    i64   total_size_bytes
+8    i64   frame_counter        savestate is the state immediately before this frame
+16   room  room                 committed room at that frame
+248  u32   checksum             xxh32 over the payload with this field zeroed
+252  i32   compressed_options_size
+256  i32   compressed_savestate_size
+260  i32   decompressed_savestate_size
+264  u8[]  zstd(savestate) then zstd(core option table)
```

**Packets.** The payload (plus Reed-Solomon parity) is split into up to 16 *packet groups*, each
an RS code of n = k + 16 blocks (k <= 239, 16 parity blocks fixed by protocol). Any k of a
group's n packets reconstruct the group.

```text
Savestate data packet
+0   u8   0x80 | flags        bit0 k_is_239, bit1 sequence_hi_is_0,
                              bit2 ack, bits3-4 transfer_id
+1   u8   packet_groups       when k=239 and sequence_hi=0
          sequence_hi         otherwise when k=239
          k                   otherwise (implies a single packet group)
+2   u8   sequence_lo         block index within the group
+3   ...  block payload

Savestate ACK
+0   u8   0x80 | ack-flag | transfer_id
+1   u8   0
+2   u8   0
```

Sender behavior: send whole packet groups, paced so the aggregate rate across all targets is
8 Mibit/s (halved on each retry). After the last group, wait up to 2 seconds for ACKs. Peers that
ACK are complete; when a deadline passes with targets remaining, start a *new* transfer (fresh
snapshot, next transfer ID, half rate) targeting the failures plus any newly pending peers. After
2 retries, give up: the authority abandons the network session and continues solo.

Receiver behavior: collect blocks per group until k arrive, RS-decode, reassemble, verify the
checksum and size fields (reject on any mismatch), decompress, install the savestate, then adopt
`frame_counter`, `room`, and the core-option table from the payload. Send one ACK for that
transfer ID only after successful installation, and ignore further data packets for an installed
ID. Data packets are accepted only from the authority link.

After a peer installs a savestate for frame N, senders replay state packets to it starting at
exactly frame N.

## 11. Desync Detection

Each player periodically hashes its serialized machine state and publishes
(`save_state_frame`, `save_state_hash`) in its state packets. Two players whose hashes differ for
the same frame have diverged; this is currently detection/telemetry only, with no automated
recovery. `input_state_hash` is reserved for the analogous check on aggregated input and is not
yet fully produced.

## 12. Security Considerations

Identity is trusted: nothing cryptographically binds a datagram link, a claimed sender port, or a
peer ID. The only structural mitigations are that non-authority links may not speak for other
ports, spectators may not send state, and savestates are accepted only from the authority link.
Signatures on state packets are a known future need.
