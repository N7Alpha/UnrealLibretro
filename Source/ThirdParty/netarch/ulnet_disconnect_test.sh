#!/usr/bin/env bash
# End-to-end disconnect handling test for the ulnet client-server (authority + spectator) model.
# Runs entirely on loopback (macOS or Linux, no root): a real sam2 signaling server and real
# ICE-lite peer processes built from ulnet_test.c. Covers the NETPLAY_BETA_TASKS.md verification
# pass that can be automated:
#   spectator-killed    - SIGKILL the spectator; the host's liveness timeout reclaims the room slot
#   spectator-leaves    - graceful EXIT; the host reclaims the room slot
#   authority-killed    - SIGKILL the host; the spectator resets to solo and reports AUTHORITY_LOST
#   savestate-timeout   - host admits but never produces a savestate; spectator's wait deadline fires
#   pause-keepalive     - host stops ticking for 3.5x the liveness timeout; keepalives carry the
#                         link and the spectator resumes ticking afterwards
set -uo pipefail

PORT="${ULNET_DISCONNECT_TEST_PORT:-9224}"
TIMEOUT="${ULNET_DISCONNECT_TEST_TIMEOUT:-30}"       # Per-process deadline, seconds
PEER_TIMEOUT="${ULNET_DISCONNECT_TEST_PEER_TIMEOUT:-2}" # Liveness/savestate-wait timeout, seconds

usage() {
    cat <<EOF
usage: $0 [--port port] [--timeout seconds] [--peer-timeout seconds]

Environment equivalents:
  ULNET_DISCONNECT_TEST_PORT=9224
  ULNET_DISCONNECT_TEST_TIMEOUT=30
  ULNET_DISCONNECT_TEST_PEER_TIMEOUT=2
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --port) PORT="$2"; shift 2 ;;
        --timeout) TIMEOUT="$2"; shift 2 ;;
        --peer-timeout) PEER_TIMEOUT="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1"; usage; exit 2 ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
WORK_DIR="${TMPDIR:-/tmp}/ulnet_disconnect_test.$$"
BIN="$WORK_DIR/ulnet_disconnect_peer"
mkdir -p "$WORK_DIR"

SERVER_PID=""
CASE_PIDS=()

cleanup() {
    set +e
    for pid in "${CASE_PIDS[@]}" $SERVER_PID; do
        kill -9 "$pid" 2>/dev/null
    done
    wait 2>/dev/null
    if [ "${ULNET_DISCONNECT_TEST_KEEP_WORKDIR:-0}" = "1" ]; then
        echo "Keeping disconnect test logs in $WORK_DIR"
    else
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

cc -DSAM2_IMPLEMENTATION \
    -DULNET_IMPLEMENTATION \
    -DULNET_TEST_MAIN \
    -I"$ROOT_DIR/Source/UnrealLibretro/Private" \
    -I"$ROOT_DIR/Source/UnrealLibretroEditor/miniz" \
    "$ROOT_DIR/Source/UnrealLibretroEditor/miniz/miniz.c" \
    "$ROOT_DIR/Source/ThirdParty/netarch/ulnet_test.c" \
    -o "$BIN" || exit 1

# One signaling server outlives every case; each case connects with fresh peer ids
"$BIN" --nat-matrix-server "$PORT" "$((TIMEOUT * 8))" >"$WORK_DIR/server.log" 2>&1 &
SERVER_PID=$!

# Peers fail fast on a refused TCP connect, so wait until the server is actually listening
server_ready_deadline=$((SECONDS + TIMEOUT))
until nc -z 127.0.0.1 "$PORT" 2>/dev/null; do
    if [ "$SECONDS" -ge "$server_ready_deadline" ]; then
        echo "FAIL sam2 server never started listening on port $PORT (log: $WORK_DIR/server.log)"
        exit 1
    fi
    sleep 0.1
done

# wait_for_line <file> <pattern> -> 0 once the pattern appears, 1 on deadline
wait_for_line() {
    local file="$1" pattern="$2" deadline=$((SECONDS + TIMEOUT))
    until grep -q "$pattern" "$file" 2>/dev/null; do
        if [ "$SECONDS" -ge "$deadline" ]; then
            return 1
        fi
        sleep 0.1
    done
}

# wait_for_ready <ready-file> -> echoes the authority peer id, or fails
wait_for_ready() {
    local ready_file="$1" deadline=$((SECONDS + TIMEOUT))
    until [ -s "$ready_file" ]; do
        if [ "$SECONDS" -ge "$deadline" ]; then
            return 1
        fi
        sleep 0.1
    done
    cat "$ready_file"
}

FAILURES=0

report() {
    local name="$1" ok="$2"
    shift 2
    if [ "$ok" = "0" ]; then
        printf "PASS %-20s\n" "$name"
    else
        printf "FAIL %-20s logs: %s\n" "$name" "$*"
        FAILURES=$((FAILURES + 1))
    fi
}

# run_case <name> <authority-mode> <spectator-mode> <kill-target>
#   kill-target: none | spectator-after-sync | authority-after-sync
# Success criteria per kill-target:
#   none / spectator-after-sync -> authority must exit 0 (spectator exit checked only for graceful modes)
#   authority-after-sync        -> spectator must exit 0
run_case() {
    local name="$1" authority_mode="$2" spectator_mode="$3" kill_target="$4"
    local ready_file="$WORK_DIR/$name.ready"
    local authority_log="$WORK_DIR/$name.authority.log"
    local spectator_log="$WORK_DIR/$name.spectator.log"
    rm -f "$ready_file"

    "$BIN" --disconnect-authority 127.0.0.1 "$PORT" "$ready_file" "$TIMEOUT" "$PEER_TIMEOUT" "$authority_mode" \
        >"$authority_log" 2>&1 &
    local authority_pid=$!
    CASE_PIDS+=("$authority_pid")

    local authority_peer_id
    if ! authority_peer_id="$(wait_for_ready "$ready_file")"; then
        report "$name" 1 "$authority_log (authority never became ready)"
        kill -9 "$authority_pid" 2>/dev/null
        wait "$authority_pid" 2>/dev/null
        return
    fi

    "$BIN" --disconnect-spectator 127.0.0.1 "$PORT" "$authority_peer_id" "$TIMEOUT" "$PEER_TIMEOUT" "$spectator_mode" \
        >"$spectator_log" 2>&1 &
    local spectator_pid=$!
    CASE_PIDS+=("$spectator_pid")

    local ok=0
    case "$kill_target" in
        spectator-after-sync)
            if wait_for_line "$spectator_log" "HARNESS spectator synced"; then
                kill -9 "$spectator_pid" 2>/dev/null
            else
                ok=1
            fi
            ;;
        authority-after-sync)
            if wait_for_line "$spectator_log" "HARNESS spectator synced"; then
                kill -9 "$authority_pid" 2>/dev/null
            else
                ok=1
            fi
            ;;
    esac

    local authority_status=0 spectator_status=0
    if [ "$kill_target" = "authority-after-sync" ]; then
        wait "$spectator_pid" 2>/dev/null; spectator_status=$?
        kill -9 "$authority_pid" 2>/dev/null
        wait "$authority_pid" 2>/dev/null
        [ "$spectator_status" -eq 0 ] || ok=1
    elif [ "$authority_mode" = "never-sync" ]; then
        # The spectator carries the assertion; the authority is just scenery
        wait "$spectator_pid" 2>/dev/null; spectator_status=$?
        kill -9 "$authority_pid" 2>/dev/null
        wait "$authority_pid" 2>/dev/null
        [ "$spectator_status" -eq 0 ] || ok=1
    else
        wait "$authority_pid" 2>/dev/null; authority_status=$?
        [ "$authority_status" -eq 0 ] || ok=1
        if [ "$kill_target" = "spectator-after-sync" ]; then
            wait "$spectator_pid" 2>/dev/null # Killed; exit code is meaningless
        else
            wait "$spectator_pid" 2>/dev/null; spectator_status=$?
            [ "$spectator_status" -eq 0 ] || ok=1
        fi
    fi

    report "$name" "$ok" "$authority_log $spectator_log"
}

run_case spectator-killed  expect-leave sync-then-hang           spectator-after-sync
run_case spectator-leaves  expect-leave sync-then-leave          none
run_case authority-killed  expect-leave expect-authority-lost    authority-after-sync
run_case savestate-timeout never-sync   expect-savestate-timeout none
run_case pause-keepalive   pause        stay-connected           none

if [ "$FAILURES" -eq 0 ]; then
    echo "All disconnect cases passed"
    exit 0
fi
echo "$FAILURES disconnect case(s) failed"
exit 1
