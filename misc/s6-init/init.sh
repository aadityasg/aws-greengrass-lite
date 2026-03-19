#!/bin/sh
# s6-init: Start all GGL core daemons under s6 without systemd.
set -e

SCAN_DIR=/run/s6-services
SOCK_DIR=/run/greengrass

mkdir -p "$SCAN_DIR" "$SOCK_DIR" /var/lib/greengrass/packages/recipes \
    /var/lib/greengrass/packages/artifacts /var/lib/greengrass/work

# Create an s6 service directory for a daemon
make_service() {
    name=$1
    bin=$2
    dir="$SCAN_DIR/$name"
    mkdir -p "$dir"
    cat > "$dir/run" <<EOF
#!/bin/sh
exec $bin
EOF
    chmod +x "$dir/run"
}

# Wait for a coreBus socket to appear (max 10s)
wait_socket() {
    sock="$SOCK_DIR/$1"
    i=0
    while [ ! -S "$sock" ] && [ $i -lt 100 ]; do
        sleep 0.1
        i=$((i + 1))
    done
    [ -S "$sock" ] && echo "Socket $1 ready" || echo "WARN: $1 not ready after 10s"
}

# Start s6-svscan in background
s6-svscan "$SCAN_DIR" &
SVSCAN_PID=$!
sleep 0.2

echo "=== Layer 0: ggconfigd ==="
make_service ggl.core.ggconfigd ggconfigd
s6-svscanctl -a "$SCAN_DIR"
wait_socket gg_config

echo "=== Layer 1: iotcored, ggipcd, ggpubsubd, tesd, gg-servicemgrd ==="
make_service ggl.core.iotcored iotcored
make_service ggl.core.ggipcd ggipcd
make_service ggl.core.ggpubsubd ggpubsubd
make_service ggl.core.tesd tesd
make_service ggl.core.gg-servicemgrd gg-servicemgrd
s6-svscanctl -a "$SCAN_DIR"
wait_socket aws_iot_mqtt

echo "=== Layer 2: tes-serverd, gghealthd, ggdeploymentd, gg-fleet-statusd ==="
make_service ggl.core.tes-serverd tes-serverd
make_service ggl.core.gghealthd gghealthd
make_service ggl.core.ggdeploymentd ggdeploymentd
make_service ggl.core.gg-fleet-statusd gg-fleet-statusd
s6-svscanctl -a "$SCAN_DIR"

echo "=== All daemons launched under s6 ==="

# Stay alive — wait for s6-svscan
wait $SVSCAN_PID
