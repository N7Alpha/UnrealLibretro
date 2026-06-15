#!/usr/bin/env bash
set -euo pipefail

PORT="${ULNET_IPV6_MATRIX_PORT:-9220}"
TIMEOUT="${ULNET_IPV6_MATRIX_TIMEOUT:-10}"
SKIP_EXPECTED_FAIL="${ULNET_IPV6_MATRIX_SKIP_EXPECTED_FAIL:-0}"

usage() {
    cat <<EOF
usage: $0 [--port port] [--timeout seconds] [--skip-expected-fail]

Environment equivalents:
  ULNET_IPV6_MATRIX_PORT=9220
  ULNET_IPV6_MATRIX_TIMEOUT=10
  ULNET_IPV6_MATRIX_SKIP_EXPECTED_FAIL=1
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
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
    echo "SKIP: Linux network namespaces are required for the IPv6 matrix test."
    exit 0
fi

if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "SKIP: IPv6 matrix test needs root for ip netns and ip6tables."
    exit 0
fi

for tool in ip ip6tables cc; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "SKIP: missing required tool '$tool'."
        exit 0
    fi
done

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
WORK_DIR="${TMPDIR:-/tmp}/ulnet_ipv6_matrix.$$"
BIN="$WORK_DIR/ulnet_ipv6_matrix_peer"
PREFIX="ul6$$"

PUB="${PREFIX}pub"
RTA="${PREFIX}ra"
RTB="${PREFIX}rb"
PEERA="${PREFIX}pa"
PEERB="${PREFIX}pb"

SERVER_ADDR="2001:db8:100::1"

mkdir -p "$WORK_DIR"

cleanup() {
    set +e
    jobs -pr | xargs -r kill 2>/dev/null
    jobs -pr | xargs -r wait 2>/dev/null
    for ns in "$PEERA" "$PEERB" "$RTA" "$RTB" "$PUB"; do
        ip netns del "$ns" 2>/dev/null
    done
    if [ "${ULNET_IPV6_MATRIX_KEEP_WORKDIR:-0}" = "1" ]; then
        echo "Keeping IPv6 matrix logs in $WORK_DIR"
    else
        rm -rf "$WORK_DIR"
    fi
}
trap cleanup EXIT

cc -DSAM2_IMPLEMENTATION \
    -DULNET_IMPLEMENTATION \
    -DULNET_TEST_MAIN \
    ${ULNET_IPV6_MATRIX_DEBUG:+-DULNET_NAT_DEBUG} \
    -I"$ROOT_DIR/Source/UnrealLibretro/Private" \
    -I"$ROOT_DIR/Source/UnrealLibretroEditor/miniz" \
    "$ROOT_DIR/Source/UnrealLibretroEditor/miniz/miniz.c" \
    "$ROOT_DIR/Source/ThirdParty/netarch/ulnet_test.c" \
    -o "$BIN"

for ns in "$PUB" "$RTA" "$RTB" "$PEERA" "$PEERB"; do
    ip netns add "$ns"
    ip -n "$ns" link set lo up
done

ip -n "$PUB" link add br0 type bridge
ip -n "$PUB" addr add "$SERVER_ADDR/64" dev br0
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

make_veth "$PEERA" eth0 "$RTA" lan0 va$$ ra$$
make_veth "$PEERB" eth0 "$RTB" lan0 vb$$ rb$$
make_veth "$RTA" pub0 "$PUB" pa0 rac$$ pac$$
make_veth "$RTB" pub0 "$PUB" pb0 rbc$$ pbc$$

ip -n "$PUB" link set pa0 master br0
ip -n "$PUB" link set pb0 master br0

ip -n "$PEERA" addr add 2001:db8:101::2/64 dev eth0
ip -n "$PEERA" route add default via 2001:db8:101::1
ip -n "$RTA" addr add 2001:db8:101::1/64 dev lan0
ip -n "$RTA" addr add 2001:db8:100::2/64 dev pub0
ip -n "$RTA" route add 2001:db8:102::/64 via 2001:db8:100::3

ip -n "$PEERB" addr add 2001:db8:102::2/64 dev eth0
ip -n "$PEERB" route add default via 2001:db8:102::1
ip -n "$RTB" addr add 2001:db8:102::1/64 dev lan0
ip -n "$RTB" addr add 2001:db8:100::3/64 dev pub0
ip -n "$RTB" route add 2001:db8:101::/64 via 2001:db8:100::2

ip netns exec "$RTA" sysctl -qw net.ipv6.conf.all.forwarding=1
ip netns exec "$RTB" sysctl -qw net.ipv6.conf.all.forwarding=1

reset_firewall_rules() {
    local ns="$1"
    local mode="$2"

    ip netns exec "$ns" ip6tables -F
    ip netns exec "$ns" ip6tables -P FORWARD DROP

    if [ "$mode" = "udp-blocked" ]; then
        ip netns exec "$ns" ip6tables -A FORWARD -p udp -j DROP
        ip netns exec "$ns" ip6tables -A FORWARD -j ACCEPT
        return
    fi

    if [ "$mode" = "open" ]; then
        ip netns exec "$ns" ip6tables -A FORWARD -j ACCEPT
        return
    fi

    ip netns exec "$ns" ip6tables -A FORWARD -i lan0 -o pub0 -j ACCEPT
    ip netns exec "$ns" ip6tables -A FORWARD -i pub0 -o lan0 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
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
        printf "SKIP %-34s expected=fail\n" "$name"
        return 0
    fi

    reset_firewall_rules "$RTA" "$mode_a"
    reset_firewall_rules "$RTB" "$mode_b"
    rm -rf "$signal_dir"
    mkdir -p "$signal_dir"

    ip netns exec "$PUB" "$BIN" --nat-matrix-server "$PORT" "$TIMEOUT" >"$server_log" 2>&1 &
    local server_pid=$!
    local tcpdump_pid=""
    if [ "${ULNET_IPV6_MATRIX_DEBUG:-}" != "" ] && command -v tcpdump >/dev/null 2>&1; then
        ip netns exec "$PUB" tcpdump -l -nn -i br0 "ip6 and udp and not port $PORT" >"$capture_log" 2>&1 &
        tcpdump_pid=$!
    fi
    sleep 0.2

    ip netns exec "$PEERA" "$BIN" --nat-matrix-peer-file "$SERVER_ADDR" "$PORT" "$signal_dir" authority spectator "$TIMEOUT" >"$authority_log" 2>&1 &
    local authority_pid=$!
    ip netns exec "$PEERB" "$BIN" --nat-matrix-peer-file "$SERVER_ADDR" "$PORT" "$signal_dir" spectator authority "$TIMEOUT" >"$spectator_log" 2>&1 &
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

    if [ "$expected" = "$actual" ]; then
        printf "PASS %-34s expected=%s actual=%s\n" "$name" "$expected" "$actual"
        return 0
    fi

    printf "FAIL %-34s expected=%s actual=%s\n" "$name" "$expected" "$actual"
    echo "  logs: $server_log $authority_log $spectator_log"
    return 1
}

case_name() {
    printf "%s_%s" "${1//-/_}" "${2//-/_}"
}

expected_for_case() {
    if [ "$1" = "udp-blocked" ] || [ "$2" = "udp-blocked" ]; then
        echo fail
    else
        echo pass
    fi
}

failures=0
firewall_modes=(open stateful-firewall udp-blocked)
for mode_a in "${firewall_modes[@]}"; do
    for mode_b in "${firewall_modes[@]}"; do
        run_case_file "$(case_name "$mode_a" "$mode_b")" "$mode_a" "$mode_b" "$(expected_for_case "$mode_a" "$mode_b")" || failures=$((failures + 1))
    done
done

exit "$failures"
