#!/usr/bin/env bash
set -euo pipefail

MODE="${ULNET_NAT_MATRIX_MODE:-file}"
PORT="${ULNET_NAT_MATRIX_PORT:-9219}"
TIMEOUT="${ULNET_NAT_MATRIX_TIMEOUT:-20}"
SKIP_EXPECTED_FAIL="${ULNET_NAT_MATRIX_SKIP_EXPECTED_FAIL:-0}"

usage() {
    cat <<EOF
usage: $0 [--mode file|sam2] [--port port] [--timeout seconds] [--skip-expected-fail]

Environment equivalents:
  ULNET_NAT_MATRIX_MODE=file|sam2
  ULNET_NAT_MATRIX_PORT=9219
  ULNET_NAT_MATRIX_TIMEOUT=20
  ULNET_NAT_MATRIX_SKIP_EXPECTED_FAIL=1
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --mode)
            MODE="$2"
            shift 2
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        --skip-expected-fail)
            SKIP_EXPECTED_FAIL=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            exit 2
            ;;
    esac
done

if [ "$(uname -s)" != "Linux" ]; then
    echo "SKIP: Linux network namespaces are required for the NAT matrix test."
    exit 0
fi

if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "SKIP: NAT matrix test needs root for ip netns and iptables."
    exit 0
fi

for tool in ip iptables cc; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "SKIP: missing required tool '$tool'."
        exit 0
    fi
done

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
WORK_DIR="${TMPDIR:-/tmp}/ulnet_nat_matrix.$$"
BIN="$WORK_DIR/ulnet_nat_matrix_peer"
PREFIX="uln$$"

PUB="${PREFIX}pub"
NATA="${PREFIX}na"
NATB="${PREFIX}nb"
PEERA="${PREFIX}pa"
PEERB="${PREFIX}pb"

mkdir -p "$WORK_DIR"

cleanup() {
    set +e
    jobs -pr | xargs -r kill 2>/dev/null
    jobs -pr | xargs -r wait 2>/dev/null
    for ns in "$PEERA" "$PEERB" "$NATA" "$NATB" "$PUB"; do
        ip netns del "$ns" 2>/dev/null
    done
    if [ "${ULNET_NAT_MATRIX_KEEP_WORKDIR:-0}" = "1" ]; then
        echo "Keeping NAT matrix logs in $WORK_DIR"
    else
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

cc -DSAM2_IMPLEMENTATION \
    -DULNET_IMPLEMENTATION \
    -DULNET_TEST_MAIN \
    -DULNET_THIRDPARTY_NO_ZSTD \
    ${ULNET_NAT_MATRIX_DEBUG:+-DULNET_NAT_DEBUG} \
    -I"$ROOT_DIR/Source/UnrealLibretro/Private" \
    -I"$ROOT_DIR/Source/UnrealLibretroEditor/miniz" \
    "$ROOT_DIR/Source/UnrealLibretroEditor/miniz/miniz.c" \
    "$ROOT_DIR/Source/ThirdParty/netarch/ulnet_test.c" \
    -o "$BIN"

for ns in "$PUB" "$NATA" "$NATB" "$PEERA" "$PEERB"; do
    ip netns add "$ns"
    ip -n "$ns" link set lo up
done

ip -n "$PUB" link add br0 type bridge
ip -n "$PUB" addr add 203.0.113.1/24 dev br0
ip -n "$PUB" link set br0 up

make_veth() {
    local left_ns="$1"
    local left_if="$2"
    local right_ns="$3"
    local right_if="$4"
    local temp_a="$5"
    local temp_b="$6"

    ip link add "$temp_a" type veth peer name "$temp_b"
    ip link set "$temp_a" netns "$left_ns"
    ip link set "$temp_b" netns "$right_ns"
    ip -n "$left_ns" link set "$temp_a" name "$left_if"
    ip -n "$right_ns" link set "$temp_b" name "$right_if"
    ip -n "$left_ns" link set "$left_if" up
    ip -n "$right_ns" link set "$right_if" up
}

make_veth "$PEERA" eth0 "$NATA" lan0 va$$ na$$
make_veth "$PEERB" eth0 "$NATB" lan0 vb$$ nb$$
make_veth "$NATA" pub0 "$PUB" pa0 nac$$ pac$$
make_veth "$NATB" pub0 "$PUB" pb0 nbc$$ pbc$$

ip -n "$PUB" link set pa0 master br0
ip -n "$PUB" link set pb0 master br0

ip -n "$PEERA" addr add 10.10.1.2/24 dev eth0
ip -n "$PEERA" route add default via 10.10.1.1
ip -n "$NATA" addr add 10.10.1.1/24 dev lan0
ip -n "$NATA" addr add 203.0.113.2/24 dev pub0

ip -n "$PEERB" addr add 10.10.2.2/24 dev eth0
ip -n "$PEERB" route add default via 10.10.2.1
ip -n "$NATB" addr add 10.10.2.1/24 dev lan0
ip -n "$NATB" addr add 203.0.113.3/24 dev pub0

ip netns exec "$NATA" sysctl -qw net.ipv4.ip_forward=1
ip netns exec "$NATB" sysctl -qw net.ipv4.ip_forward=1

reset_nat_rules() {
    local ns="$1"
    local lan_cidr="$2"
    local lan_ip="$3"
    local mode="$4"

    ip netns exec "$ns" iptables -F
    ip netns exec "$ns" iptables -t nat -F
    ip netns exec "$ns" iptables -P FORWARD DROP

    if [ "$mode" = "udp-blocked" ]; then
        ip netns exec "$ns" iptables -A FORWARD -p udp -j DROP
    fi

    ip netns exec "$ns" iptables -A FORWARD -i lan0 -o pub0 -j ACCEPT
    ip netns exec "$ns" iptables -A FORWARD -i pub0 -o lan0 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
    if [ "$mode" = "full-cone" ]; then
        ip netns exec "$ns" iptables -A FORWARD -i pub0 -o lan0 -p udp -d "$lan_ip" -j ACCEPT
        ip netns exec "$ns" iptables -t nat -A PREROUTING -i pub0 -p udp -j DNAT --to-destination "$lan_ip"
    fi

    if [ "$mode" = "symmetric" ]; then
        ip netns exec "$ns" iptables -t nat -A POSTROUTING -s "$lan_cidr" -o pub0 -j MASQUERADE --random-fully
    else
        ip netns exec "$ns" iptables -t nat -A POSTROUTING -s "$lan_cidr" -o pub0 -j MASQUERADE
    fi
}

run_case() {
    local name="$1"
    local mode_a="$2"
    local mode_b="$3"
    local expected="$4"
    local ready_file="$WORK_DIR/$name.authority"
    local server_log="$WORK_DIR/$name.server.log"
    local authority_log="$WORK_DIR/$name.authority.log"
    local spectator_log="$WORK_DIR/$name.spectator.log"
    local capture_log="$WORK_DIR/$name.public-udp.log"

    if [ "$SKIP_EXPECTED_FAIL" = "1" ] && [ "$expected" = "fail" ]; then
        printf "SKIP %-28s expected=fail\n" "$name"
        return 0
    fi

    reset_nat_rules "$NATA" 10.10.1.0/24 10.10.1.2 "$mode_a"
    reset_nat_rules "$NATB" 10.10.2.0/24 10.10.2.2 "$mode_b"
    rm -f "$ready_file"

    ip netns exec "$PUB" "$BIN" --nat-matrix-server "$PORT" "$TIMEOUT" >"$server_log" 2>&1 &
    local server_pid=$!
    local tcpdump_pid=""
    if [ "${ULNET_NAT_MATRIX_DEBUG:-}" != "" ] && command -v tcpdump >/dev/null 2>&1; then
        ip netns exec "$PUB" tcpdump -l -nn -i br0 "udp and not port $PORT" >"$capture_log" 2>&1 &
        tcpdump_pid=$!
    fi
    sleep 0.2

    ip netns exec "$PEERA" "$BIN" --nat-matrix-authority 203.0.113.1 "$PORT" "$ready_file" "$TIMEOUT" >"$authority_log" 2>&1 &
    local authority_pid=$!

    local authority_peer_id=""
    for _ in $(seq 1 100); do
        if [ -s "$ready_file" ]; then
            authority_peer_id="$(cat "$ready_file")"
            break
        fi
        sleep 0.05
    done

    local spectator_status=1
    if [ -n "$authority_peer_id" ]; then
        if ip netns exec "$PEERB" "$BIN" --nat-matrix-spectator 203.0.113.1 "$PORT" "$authority_peer_id" "$TIMEOUT" >"$spectator_log" 2>&1; then
            spectator_status=0
        fi
    fi

    kill "$authority_pid" "$server_pid" 2>/dev/null || true
    if [ -n "$tcpdump_pid" ]; then
        kill "$tcpdump_pid" 2>/dev/null || true
    fi
    wait "$authority_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    if [ -n "$tcpdump_pid" ]; then
        wait "$tcpdump_pid" 2>/dev/null || true
    fi

    if { [ "$expected" = "pass" ] && [ "$spectator_status" -eq 0 ]; } ||
       { [ "$expected" = "fail" ] && [ "$spectator_status" -ne 0 ]; }; then
        printf "PASS %-28s expected=%s actual=%s\n" "$name" "$expected" "$([ "$spectator_status" -eq 0 ] && echo pass || echo fail)"
        return 0
    fi

    printf "FAIL %-28s expected=%s actual=%s\n" "$name" "$expected" "$([ "$spectator_status" -eq 0 ] && echo pass || echo fail)"
    echo "  logs: $server_log $authority_log $spectator_log"
    return 1
}

run_case_file() {
    local name="$1"
    local mode_a="$2"
    local mode_b="$3"
    local expected="$4"
    local signal_dir="$WORK_DIR/$name.signal"
    local server_log="$WORK_DIR/$name.server.log"
    local authority_log="$WORK_DIR/$name.authority.log"
    local spectator_log="$WORK_DIR/$name.spectator.log"
    local capture_log="$WORK_DIR/$name.public-udp.log"

    if [ "$SKIP_EXPECTED_FAIL" = "1" ] && [ "$expected" = "fail" ]; then
        printf "SKIP %-28s expected=fail\n" "$name"
        return 0
    fi

    reset_nat_rules "$NATA" 10.10.1.0/24 10.10.1.2 "$mode_a"
    reset_nat_rules "$NATB" 10.10.2.0/24 10.10.2.2 "$mode_b"
    rm -rf "$signal_dir"
    mkdir -p "$signal_dir"

    ip netns exec "$PUB" "$BIN" --nat-matrix-server "$PORT" "$TIMEOUT" >"$server_log" 2>&1 &
    local server_pid=$!
    local tcpdump_pid=""
    if [ "${ULNET_NAT_MATRIX_DEBUG:-}" != "" ] && command -v tcpdump >/dev/null 2>&1; then
        ip netns exec "$PUB" tcpdump -l -nn -i br0 "udp and not port $PORT" >"$capture_log" 2>&1 &
        tcpdump_pid=$!
    fi
    sleep 0.2

    ip netns exec "$PEERA" "$BIN" --nat-matrix-peer-file 203.0.113.1 "$PORT" "$signal_dir" authority spectator "$TIMEOUT" >"$authority_log" 2>&1 &
    local authority_pid=$!
    ip netns exec "$PEERB" "$BIN" --nat-matrix-peer-file 203.0.113.1 "$PORT" "$signal_dir" spectator authority "$TIMEOUT" >"$spectator_log" 2>&1 &
    local spectator_pid=$!

    local authority_status=0
    local spectator_status=0
    wait "$authority_pid" || authority_status=$?
    wait "$spectator_pid" || spectator_status=$?
    kill "$server_pid" 2>/dev/null || true
    if [ -n "$tcpdump_pid" ]; then
        kill "$tcpdump_pid" 2>/dev/null || true
    fi
    wait "$server_pid" 2>/dev/null || true
    if [ -n "$tcpdump_pid" ]; then
        wait "$tcpdump_pid" 2>/dev/null || true
    fi

    local actual="fail"
    if [ "$authority_status" -eq 0 ] && [ "$spectator_status" -eq 0 ]; then
        actual="pass"
    fi

    if [ "$expected" = "observe" ]; then
        printf "OBS  %-28s actual=%s\n" "$name" "$actual"
        return 0
    fi

    if [ "$expected" = "$actual" ]; then
        printf "PASS %-28s expected=%s actual=%s\n" "$name" "$expected" "$actual"
        return 0
    fi

    printf "FAIL %-28s expected=%s actual=%s\n" "$name" "$expected" "$actual"
    echo "  logs: $server_log $authority_log $spectator_log"
    return 1
}

case_name() {
    printf "%s_%s" "${1//-/_}" "${2//-/_}"
}

expected_for_case() {
    local mode_a="$1"
    local mode_b="$2"

    if [ "$mode_a" = "udp-blocked" ] || [ "$mode_b" = "udp-blocked" ]; then
        echo fail
    elif [ "$mode_a" = "full-cone" ] || [ "$mode_b" = "full-cone" ]; then
        echo pass
    elif [ "$mode_a" = "symmetric" ] || [ "$mode_b" = "symmetric" ]; then
        echo fail
    elif [ "$MODE" = "file" ] && [ "$mode_a" = "masquerade" ] && [ "$mode_b" = "masquerade" ]; then
        echo observe
    else
        echo fail
    fi
}

failures=0
nat_modes=(full-cone masquerade symmetric udp-blocked)
if [ "$MODE" = "sam2" ]; then
    for mode_a in "${nat_modes[@]}"; do
        for mode_b in "${nat_modes[@]}"; do
            run_case "$(case_name "$mode_a" "$mode_b")" "$mode_a" "$mode_b" "$(expected_for_case "$mode_a" "$mode_b")" || failures=$((failures + 1))
        done
    done
else
    for mode_a in "${nat_modes[@]}"; do
        for mode_b in "${nat_modes[@]}"; do
            run_case_file "$(case_name "$mode_a" "$mode_b")" "$mode_a" "$mode_b" "$(expected_for_case "$mode_a" "$mode_b")" || failures=$((failures + 1))
        done
    done
fi

exit "$failures"
