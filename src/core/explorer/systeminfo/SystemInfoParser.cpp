// SPDX-FileCopyrightText: Copyright (C) 2026 Nguyen Khac Thanh <ask@nkthanh.dev>
//
// SPDX-License-Identifier: GPL-3.0-only

#include "SystemInfoParser.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

namespace SystemInfoParser
{
namespace
{
QString trParse(const char *text)
{
    return QCoreApplication::translate("SystemInfoParser", text);
}

bool looksLikeUnavailable(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("no /proc")) ||
           hay.contains(QLatin1String("command not found")) ||
           hay.contains(QLatin1String("not found")) || hay.contains(QLatin1String("no such file"));
}

bool looksLikePermissionDenied(const QString &stderrText, const QString &errorMessage)
{
    const QString hay = (stderrText + QLatin1Char('\n') + errorMessage).toLower();
    return hay.contains(QLatin1String("permission denied")) ||
           hay.contains(QLatin1String("operation not permitted")) ||
           hay.contains(QLatin1String("access denied"));
}

CpuCoreTicks parseTicksArray(const QJsonArray &arr, int index)
{
    CpuCoreTicks t;
    t.index = index;
    auto at = [&arr](int i) -> quint64 {
        if (i < 0 || i >= arr.size()) {
            return 0;
        }
        return static_cast<quint64>(arr.at(i).toVariant().toULongLong());
    };
    t.user = at(0);
    t.nice = at(1);
    t.system = at(2);
    t.idle = at(3);
    t.iowait = at(4);
    t.irq = at(5);
    t.softirq = at(6);
    t.steal = at(7);
    return t;
}

quint64 jsonUInt64(const QJsonObject &obj, const char *key)
{
    return static_cast<quint64>(obj.value(QLatin1String(key)).toVariant().toULongLong());
}

double jsonDouble(const QJsonObject &obj, const char *key)
{
    return obj.value(QLatin1String(key)).toDouble();
}

QString jsonString(const QJsonObject &obj, const char *key)
{
    return obj.value(QLatin1String(key)).toString();
}

/// Optional numeric field: missing / non-number / negative → -1.
double jsonOptionalDouble(const QJsonObject &obj, const char *key)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    if (v.isUndefined() || v.isNull()) {
        return -1.0;
    }
    bool ok = false;
    const double d = v.toVariant().toDouble(&ok);
    if (!ok || d < 0.0) {
        return -1.0;
    }
    return d;
}

qint64 jsonOptionalInt64(const QJsonObject &obj, const char *key)
{
    const QJsonValue v = obj.value(QLatin1String(key));
    if (v.isUndefined() || v.isNull()) {
        return -1;
    }
    bool ok = false;
    const qint64 n = v.toVariant().toLongLong(&ok);
    if (!ok || n < 0) {
        return -1;
    }
    return n;
}

int jsonOptionalInt(const QJsonObject &obj, const char *key)
{
    const qint64 n = jsonOptionalInt64(obj, key);
    if (n < 0 || n > 2147483647LL) {
        return -1;
    }
    return static_cast<int>(n);
}
} // namespace

QString fetchCommand()
{
    // POSIX sh + awk: emit one JSON object from /proc, /sys, and df.
    // No sleep — CPU%/NIC rates are computed client-side between polls.
    return QStringLiteral(
        R"SH(set +e
if [ ! -r /proc/stat ] || [ ! -r /proc/meminfo ]; then
  echo 'no /proc' >&2
  exit 127
fi

json_esc() {
  printf '%s' "$1" | awk 'BEGIN{ORS=""} {
    gsub(/\\/,"\\\\"); gsub(/"/,"\\\""); gsub(/\t/,"\\t"); gsub(/\r/,"\\r"); gsub(/\n/,"\\n");
    print
  }'
}

PRETTY=""
if [ -r /etc/os-release ]; then
  PRETTY=$(awk -F= '/^PRETTY_NAME=/{
    v=$0; sub(/^[^=]*=/,"",v); gsub(/^"/,"",v); gsub(/"$/,"",v); print v; exit
  }' /etc/os-release 2>/dev/null)
fi
KERNEL=$(uname -srm 2>/dev/null)
ARCH=$(uname -m 2>/dev/null)
HOSTNAME=$(hostname 2>/dev/null || cat /proc/sys/kernel/hostname 2>/dev/null)
UPTIME=$(awk '{printf "%d", $1+0}' /proc/uptime 2>/dev/null)

LOAD=$(awk '{
  printf "{\"1\":%s,\"5\":%s,\"15\":%s}", $1+0, $2+0, $3+0
}' /proc/loadavg 2>/dev/null)
[ -n "$LOAD" ] || LOAD='{"1":0,"5":0,"15":0}'

CPU_MODEL=$(awk -F: '
  /^model name/ { gsub(/^ +/,"",$2); print $2; exit }
  /^Hardware/   { gsub(/^ +/,"",$2); print $2; exit }
  /^Model/      { gsub(/^ +/,"",$2); print $2; exit }
' /proc/cpuinfo 2>/dev/null)
LOGICAL=$(grep -c '^processor' /proc/cpuinfo 2>/dev/null)
[ -n "$LOGICAL" ] || LOGICAL=0

CPU_AGG=$(awk '/^cpu / {
  printf "["
  for (i=2; i<=NF && i<=9; i++) { if (i>2) printf ","; printf "%s", $i+0 }
  printf "]"
  exit
}' /proc/stat 2>/dev/null)
[ -n "$CPU_AGG" ] || CPU_AGG='[0,0,0,0,0,0,0,0]'

CPU_CORES=$(awk '
  BEGIN { first=1 }
  /^cpu[0-9]/ {
    if (!first) printf ","
    first=0
    printf "["
    for (i=2; i<=NF && i<=9; i++) { if (i>2) printf ","; printf "%s", $i+0 }
    printf "]"
  }
' /proc/stat 2>/dev/null)

MEM=$(awk '
  /^MemTotal:/     { t=$2 }
  /^MemAvailable:/ { a=$2 }
  /^MemFree:/      { f=$2 }
  /^Buffers:/      { b=$2 }
  /^Cached:/       { c=$2 }
  /^Shmem:/        { sh=$2 }
  /^SReclaimable:/ { sr=$2 }
  /^SwapTotal:/    { st=$2 }
  /^SwapFree:/     { sf=$2 }
  END {
    printf "{\"MemTotal\":%d,\"MemAvailable\":%d,\"MemFree\":%d,\"Buffers\":%d,\"Cached\":%d,\"Shmem\":%d,\"SReclaimable\":%d,\"SwapTotal\":%d,\"SwapFree\":%d}",
      t+0, a+0, f+0, b+0, c+0, sh+0, sr+0, st+0, sf+0
  }
' /proc/meminfo 2>/dev/null)
[ -n "$MEM" ] || MEM='{"MemTotal":0,"MemAvailable":0,"MemFree":0,"Buffers":0,"Cached":0,"Shmem":0,"SReclaimable":0,"SwapTotal":0,"SwapFree":0}'

DISKS=$(df -PBK 2>/dev/null | awk '
  BEGIN { first=1 }
  NR==1 { next }
  {
    fs=$1; size=$2+0; used=$3+0; avail=$4+0; pct=$5; mount=$6
    for (i=7; i<=NF; i++) mount = mount " " $i
    gsub(/%/,"",pct)
    # skip pseudo filesystems
    if (fs ~ /^(tmpfs|devtmpfs|devfs|overlay|squashfs|proc|sysfs|cgroup|cgroup2|pstore|bpf|tracefs|debugfs|securityfs|hugetlbfs|mqueue|fusectl|configfs|binfmt_misc|autofs|rpc_pipefs|nfsd|none)$/) next
    if (mount ~ /^\/(proc|sys|dev|run)(\/|$)/ && fs !~ /^\//) next
    # escape strings
    gsub(/\\/,"\\\\",fs); gsub(/"/,"\\\"",fs)
    gsub(/\\/,"\\\\",mount); gsub(/"/,"\\\"",mount)
    if (!first) printf ","
    first=0
    printf "{\"fs\":\"%s\",\"mount\":\"%s\",\"size_kb\":%d,\"used_kb\":%d,\"avail_kb\":%d,\"use_pct\":%d}",
      fs, mount, size, used, avail, pct+0
  }
')

NICS=$(awk '
  NR<=2 { next }
  {
    name=$1
    gsub(/:/,"",name)
    if (name == "lo") next
    rx=$2+0; rxp=$3+0; rxerr=$4+0
    tx=$10+0; txp=$11+0; txerr=$12+0
    printf "%s %s %s %s %s %s %s\n", name, rx, rxp, rxerr, tx, txp, txerr
  }
' /proc/net/dev 2>/dev/null | while read -r name rx rxp rxerr tx txp txerr; do
  [ -n "$name" ] || continue
  mac=""
  state=""
  ipv4=""
  ipv6=""
  speed=-1
  mtu=0
  [ -r "/sys/class/net/$name/address" ] && mac=$(cat "/sys/class/net/$name/address" 2>/dev/null)
  [ -r "/sys/class/net/$name/operstate" ] && state=$(cat "/sys/class/net/$name/operstate" 2>/dev/null)
  [ -r "/sys/class/net/$name/mtu" ] && mtu=$(cat "/sys/class/net/$name/mtu" 2>/dev/null)
  if [ -r "/sys/class/net/$name/speed" ]; then
    sp=$(cat "/sys/class/net/$name/speed" 2>/dev/null) || sp=""
    case "$sp" in
      ''|*[!0-9]*) speed=-1 ;;
      *) speed=$sp ;;
    esac
  fi
  if command -v ip >/dev/null 2>&1; then
    ipv4=$(ip -o -4 addr show dev "$name" 2>/dev/null | awk '{
      a=$4; sub(/\/.*/,"",a);
      if (n++) printf ",";
      printf "%s", a
    }')
    ipv6=$(ip -o -6 addr show dev "$name" scope global 2>/dev/null | awk '{
      a=$4; sub(/\/.*/,"",a);
      if (n++) printf ",";
      printf "%s", a
    }')
  fi
  mac_e=$(json_esc "$mac")
  state_e=$(json_esc "$state")
  name_e=$(json_esc "$name")
  ipv4_e=$(json_esc "$ipv4")
  ipv6_e=$(json_esc "$ipv6")
  printf '{"name":"%s","mac":"%s","state":"%s","ipv4":"%s","ipv6":"%s","speed_mbps":%s,"mtu":%s,"rx_bytes":%s,"rx_packets":%s,"rx_errors":%s,"tx_bytes":%s,"tx_packets":%s,"tx_errors":%s}\n' \
    "$name_e" "$mac_e" "$state_e" "$ipv4_e" "$ipv6_e" "${speed:--1}" "${mtu:-0}" \
    "$rx" "$rxp" "$rxerr" "$tx" "$txp" "$txerr"
done | awk '
  BEGIN { first=1 }
  {
    if (!first) printf ","
    first=0
    printf "%s", $0
  }
')

CPU_GOV=""
CPU_FMIN=0
CPU_FMAX=0
CPU_FREQS=""
if [ -d /sys/devices/system/cpu ]; then
  [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ] && \
    CPU_GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)
  [ -r /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq ] && \
    CPU_FMIN=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq 2>/dev/null)
  [ -r /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq ] && \
    CPU_FMAX=$(cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 2>/dev/null)
  # Prefer scaling_cur_freq; fall back to cpuinfo_cur_freq.
  CPU_FREQS=$(for d in /sys/devices/system/cpu/cpu[0-9]*; do
    [ -d "$d" ] || continue
    f=0
    if [ -r "$d/cpufreq/scaling_cur_freq" ]; then
      f=$(cat "$d/cpufreq/scaling_cur_freq" 2>/dev/null)
    elif [ -r "$d/cpufreq/cpuinfo_cur_freq" ]; then
      f=$(cat "$d/cpufreq/cpuinfo_cur_freq" 2>/dev/null)
    fi
    printf '%s\n' "${f:-0}"
  done | awk 'BEGIN{first=1}{if(!first)printf ",";first=0;printf "%d",$1+0}')
fi

DISK_IO=$(for d in /sys/block/*; do
  [ -d "$d" ] || continue
  name=$(basename "$d")
  case "$name" in
    loop*|ram*|fd*|md*|dm-*) continue ;;
  esac
  awk -v n="$name" '$3==n {
    printf "{\"name\":\"%s\",\"reads\":%d,\"writes\":%d,\"sectors_read\":%d,\"sectors_written\":%d,\"io_ticks\":%d}\n",
      n, $4+0, $8+0, $6+0, $10+0, $13+0
    exit
  }' /proc/diskstats 2>/dev/null
done | awk 'BEGIN{first=1}{if(!first)printf ",";first=0;printf "%s",$0}')

# Prefer hwmon labelled sensors; fall back to thermal_zone* when hwmon is empty.
TEMPS=$( {
  found=0
  for h in /sys/class/hwmon/hwmon*; do
    [ -d "$h" ] || continue
    hname=$(tr -d '\n' <"$h/name" 2>/dev/null)
    [ -n "$hname" ] || hname=$(basename "$h")
    for input in "$h"/temp*_input; do
      [ -r "$input" ] || continue
      base=${input##*/}
      base=${base%_input}
      label=""
      [ -r "$h/${base}_label" ] && label=$(tr -d '\n' <"$h/${base}_label" 2>/dev/null)
      raw=$(tr -d '\n' <"$input" 2>/dev/null)
      case "$raw" in
        ''|*[!0-9-]*) continue ;;
      esac
      celsius=$(awk -v t="$raw" 'BEGIN{printf "%.1f", t/1000.0}')
      if [ -n "$label" ]; then
        sname="$hname: $label"
      else
        sname="$hname/$base"
      fi
      sname_e=$(json_esc "$sname")
      printf '{"name":"%s","celsius":%s}\n' "$sname_e" "$celsius"
      found=1
    done
  done
  if [ "$found" -eq 0 ]; then
    for z in /sys/class/thermal/thermal_zone*; do
      [ -d "$z" ] || continue
      [ -r "$z/temp" ] || continue
      ztype=$(tr -d '\n' <"$z/type" 2>/dev/null)
      [ -n "$ztype" ] || ztype=$(basename "$z")
      raw=$(tr -d '\n' <"$z/temp" 2>/dev/null)
      case "$raw" in
        ''|*[!0-9-]*) continue ;;
      esac
      celsius=$(awk -v t="$raw" 'BEGIN{printf "%.1f", t/1000.0}')
      ztype_e=$(json_esc "$ztype")
      printf '{"name":"%s","celsius":%s}\n' "$ztype_e" "$celsius"
    done
  fi
} | awk 'BEGIN{first=1}{if(!first)printf ",";first=0;printf "%s",$0}' )

GPUS=""
GPU_PROCS=""
if command -v nvidia-smi >/dev/null 2>&1; then
  GPUS=$(nvidia-smi --query-gpu=index,name,uuid,pci.bus_id,driver_version,pstate,utilization.gpu,utilization.memory,memory.total,memory.used,memory.free,temperature.gpu,power.draw,power.limit,clocks.current.sm,clocks.current.memory --format=csv,noheader,nounits 2>/dev/null | awk -F', *' '
  function num(v) {
    gsub(/^ +| +$/, "", v)
    if (v == "" || v == "N/A" || v == "[N/A]" || v == "[Not Supported]") return "-1"
    return v
  }
  function esc(s,   t) {
    t = s
    gsub(/^ +| +$/, "", t)
    gsub(/\\/, "\\\\", t)
    gsub(/"/, "\\\"", t)
    gsub(/\t/, "\\t", t)
    return t
  }
  BEGIN { first=1 }
  NF >= 16 {
    if (!first) printf ","
    first=0
    printf "{\"index\":%s,\"name\":\"%s\",\"uuid\":\"%s\",\"pci\":\"%s\",\"driver\":\"%s\",\"pstate\":\"%s\",\"util_gpu\":%s,\"util_mem\":%s,\"mem_total\":%s,\"mem_used\":%s,\"mem_free\":%s,\"temp\":%s,\"power_draw\":%s,\"power_limit\":%s,\"clock_sm\":%s,\"clock_mem\":%s}",
      num($1), esc($2), esc($3), esc($4), esc($5), esc($6),
      num($7), num($8), num($9), num($10), num($11), num($12), num($13), num($14), num($15), num($16)
  }
  ')
  GPU_PROCS=$(nvidia-smi --query-compute-apps=pid,process_name,gpu_uuid,used_gpu_memory --format=csv,noheader,nounits 2>/dev/null | awk -F', *' '
  function num(v) {
    gsub(/^ +| +$/, "", v)
    if (v == "" || v == "N/A" || v == "[N/A]" || v == "[Not Supported]") return "-1"
    return v
  }
  function esc(s,   t) {
    t = s
    gsub(/^ +| +$/, "", t)
    gsub(/\\/, "\\\\", t)
    gsub(/"/, "\\\"", t)
    gsub(/\t/, "\\t", t)
    return t
  }
  BEGIN { first=1 }
  NF >= 4 {
    if (!first) printf ","
    first=0
    printf "{\"pid\":%s,\"name\":\"%s\",\"gpu_uuid\":\"%s\",\"used_mem\":%s}",
      num($1), esc($2), esc($3), num($4)
  }
  ')
fi

read_dmi() {
  f="/sys/class/dmi/id/$1"
  if [ -r "$f" ]; then
    tr -d '\n' <"$f" 2>/dev/null
  fi
}

VIRT_ALL="none"
VIRT_VM="none"
VIRT_CT="none"
if command -v systemd-detect-virt >/dev/null 2>&1; then
  VIRT_ALL=$(systemd-detect-virt 2>/dev/null || true)
  VIRT_VM=$(systemd-detect-virt -v 2>/dev/null || true)
  VIRT_CT=$(systemd-detect-virt -c 2>/dev/null || true)
fi
[ -n "$VIRT_ALL" ] || VIRT_ALL="none"
[ -n "$VIRT_VM" ] || VIRT_VM="none"
[ -n "$VIRT_CT" ] || VIRT_CT="none"
# systemd-detect-virt prints "none" and exits 1 when not virtualized.
case "$VIRT_ALL" in none|"") VIRT_ALL="none" ;; esac
case "$VIRT_VM" in none|"") VIRT_VM="none" ;; esac
case "$VIRT_CT" in none|"") VIRT_CT="none" ;; esac

CPU_VENDOR=$(awk -F: '/^vendor_id/{gsub(/^ +/,"",$2); print $2; exit}' /proc/cpuinfo 2>/dev/null)
CPU_HV=0
grep -E '^flags[[:space:]]*:.*[[:space:]]hypervisor([[:space:]]|$)' /proc/cpuinfo >/dev/null 2>&1 && CPU_HV=1

DMI_SYS=$(read_dmi sys_vendor)
DMI_PRODUCT=$(read_dmi product_name)
DMI_PRODUCT_VER=$(read_dmi product_version)
DMI_BOARD_V=$(read_dmi board_vendor)
DMI_BOARD=$(read_dmi board_name)
DMI_CHASSIS_V=$(read_dmi chassis_vendor)
DMI_CHASSIS_T=$(read_dmi chassis_type)
DMI_BIOS_V=$(read_dmi bios_vendor)
DMI_BIOS_VER=$(read_dmi bios_version)
DMI_BIOS_DATE=$(read_dmi bios_date)

DOCKER=0
[ -f /.dockerenv ] && DOCKER=1
PODMAN=0
[ -f /run/.containerenv ] && PODMAN=1
WSL=0
uname -r 2>/dev/null | grep -qi microsoft && WSL=1
[ -d /proc/sys/fs/binfmt_misc/WSLInterop ] 2>/dev/null && WSL=1

CGROUP=$(awk 'BEGIN{ORS=";"} {print}' /proc/1/cgroup 2>/dev/null | sed 's/;$//')
# Keep cgroup summary short for JSON size.
CGROUP=$(printf '%s' "$CGROUP" | awk '{ if (length($0)>240) print substr($0,1,240)"..."; else print }')

PRETTY_E=$(json_esc "$PRETTY")
KERNEL_E=$(json_esc "$KERNEL")
ARCH_E=$(json_esc "$ARCH")
HOSTNAME_E=$(json_esc "$HOSTNAME")
MODEL_E=$(json_esc "$CPU_MODEL")
GOV_E=$(json_esc "$CPU_GOV")
VIRT_ALL_E=$(json_esc "$VIRT_ALL")
VIRT_VM_E=$(json_esc "$VIRT_VM")
VIRT_CT_E=$(json_esc "$VIRT_CT")
CPU_VENDOR_E=$(json_esc "$CPU_VENDOR")
DMI_SYS_E=$(json_esc "$DMI_SYS")
DMI_PRODUCT_E=$(json_esc "$DMI_PRODUCT")
DMI_PRODUCT_VER_E=$(json_esc "$DMI_PRODUCT_VER")
DMI_BOARD_V_E=$(json_esc "$DMI_BOARD_V")
DMI_BOARD_E=$(json_esc "$DMI_BOARD")
DMI_CHASSIS_V_E=$(json_esc "$DMI_CHASSIS_V")
DMI_CHASSIS_T_E=$(json_esc "$DMI_CHASSIS_T")
DMI_BIOS_V_E=$(json_esc "$DMI_BIOS_V")
DMI_BIOS_VER_E=$(json_esc "$DMI_BIOS_VER")
DMI_BIOS_DATE_E=$(json_esc "$DMI_BIOS_DATE")
CGROUP_E=$(json_esc "$CGROUP")

IS_VM=false
IS_CT=false
CPU_HV_B=false
DOCKER_B=false
PODMAN_B=false
WSL_B=false
[ "$VIRT_VM" != "none" ] && IS_VM=true
[ "$VIRT_CT" != "none" ] && IS_CT=true
[ "$CPU_HV" -eq 1 ] && { CPU_HV_B=true; IS_VM=true; }
if [ "$DOCKER" -eq 1 ] || [ "$PODMAN" -eq 1 ]; then
  IS_CT=true
fi
[ "$DOCKER" -eq 1 ] && DOCKER_B=true
[ "$PODMAN" -eq 1 ] && PODMAN_B=true
[ "$WSL" -eq 1 ] && WSL_B=true

printf '{'
printf '"os":{"pretty":"%s","kernel":"%s","arch":"%s","hostname":"%s","uptime_sec":%s},' \
  "$PRETTY_E" "$KERNEL_E" "$ARCH_E" "$HOSTNAME_E" "${UPTIME:-0}"
printf '"load":%s,' "$LOAD"
printf '"cpu":{"model":"%s","logical":%s,"governor":"%s","freq_min_khz":%s,"freq_max_khz":%s,"freq_khz":[%s],"stat":{"agg":%s,"cores":[%s]}},' \
  "$MODEL_E" "${LOGICAL:-0}" "$GOV_E" "${CPU_FMIN:-0}" "${CPU_FMAX:-0}" "$CPU_FREQS" "$CPU_AGG" "$CPU_CORES"
printf '"mem":%s,' "$MEM"
printf '"disks":[%s],' "$DISKS"
printf '"disk_io":[%s],' "$DISK_IO"
printf '"temps":[%s],' "$TEMPS"
printf '"gpus":[%s],' "$GPUS"
printf '"gpu_procs":[%s],' "$GPU_PROCS"
printf '"virt":{"detect":"%s","vm":"%s","container":"%s","is_vm":%s,"is_container":%s,"cpu_hypervisor":%s,"cpu_vendor":"%s","dmi_sys_vendor":"%s","dmi_product_name":"%s","dmi_product_version":"%s","dmi_board_vendor":"%s","dmi_board_name":"%s","dmi_chassis_vendor":"%s","dmi_chassis_type":"%s","dmi_bios_vendor":"%s","dmi_bios_version":"%s","dmi_bios_date":"%s","docker_env":%s,"podman_env":%s,"wsl":%s,"cgroup_init":"%s"},' \
  "$VIRT_ALL_E" "$VIRT_VM_E" "$VIRT_CT_E" "$IS_VM" "$IS_CT" "$CPU_HV_B" "$CPU_VENDOR_E" \
  "$DMI_SYS_E" "$DMI_PRODUCT_E" "$DMI_PRODUCT_VER_E" "$DMI_BOARD_V_E" "$DMI_BOARD_E" \
  "$DMI_CHASSIS_V_E" "$DMI_CHASSIS_T_E" "$DMI_BIOS_V_E" "$DMI_BIOS_VER_E" "$DMI_BIOS_DATE_E" \
  "$DOCKER_B" "$PODMAN_B" "$WSL_B" "$CGROUP_E"
printf '"nics":[%s]' "$NICS"
printf '}\n'
)SH");
}

bool parseSnapshot(const QByteArray &stdoutBytes, SystemInfo *out, QString *error)
{
    if (!out) {
        if (error) {
            *error = trParse("Output buffer is null");
        }
        return false;
    }
    *out = SystemInfo{};

    const QByteArray trimmed = stdoutBytes.trimmed();
    if (trimmed.isEmpty()) {
        if (error) {
            *error = trParse("Empty system info response");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = trParse("Invalid system info JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();

    const QJsonObject os = root.value(QLatin1String("os")).toObject();
    out->os.prettyName = jsonString(os, "pretty");
    out->os.kernel = jsonString(os, "kernel");
    out->os.arch = jsonString(os, "arch");
    out->os.hostname = jsonString(os, "hostname");
    out->os.uptimeSec = static_cast<qint64>(jsonUInt64(os, "uptime_sec"));

    const QJsonObject load = root.value(QLatin1String("load")).toObject();
    out->load.load1 = jsonDouble(load, "1");
    out->load.load5 = jsonDouble(load, "5");
    out->load.load15 = jsonDouble(load, "15");

    const QJsonObject cpu = root.value(QLatin1String("cpu")).toObject();
    out->cpu.model = jsonString(cpu, "model");
    out->cpu.logicalCpus = cpu.value(QLatin1String("logical")).toInt();
    out->cpu.governor = jsonString(cpu, "governor");
    out->cpu.freqMinKHz = static_cast<qint64>(jsonUInt64(cpu, "freq_min_khz"));
    out->cpu.freqMaxKHz = static_cast<qint64>(jsonUInt64(cpu, "freq_max_khz"));
    const QJsonArray freqArr = cpu.value(QLatin1String("freq_khz")).toArray();
    out->cpu.coreFreqKHz.reserve(freqArr.size());
    for (const QJsonValue &v : freqArr) {
        out->cpu.coreFreqKHz.push_back(static_cast<qint64>(v.toVariant().toLongLong()));
    }
    const QJsonObject stat = cpu.value(QLatin1String("stat")).toObject();
    out->cpu.aggregate = parseTicksArray(stat.value(QLatin1String("agg")).toArray(), -1);
    const QJsonArray cores = stat.value(QLatin1String("cores")).toArray();
    out->cpu.cores.reserve(cores.size());
    for (int i = 0; i < cores.size(); ++i) {
        out->cpu.cores.push_back(parseTicksArray(cores.at(i).toArray(), i));
    }
    if (out->cpu.logicalCpus <= 0) {
        out->cpu.logicalCpus = out->cpu.cores.size();
    }

    const QJsonObject mem = root.value(QLatin1String("mem")).toObject();
    out->mem.totalKb = jsonUInt64(mem, "MemTotal");
    out->mem.availableKb = jsonUInt64(mem, "MemAvailable");
    out->mem.freeKb = jsonUInt64(mem, "MemFree");
    out->mem.buffersKb = jsonUInt64(mem, "Buffers");
    out->mem.cachedKb = jsonUInt64(mem, "Cached");
    out->mem.shmemKb = jsonUInt64(mem, "Shmem");
    out->mem.sReclaimableKb = jsonUInt64(mem, "SReclaimable");
    out->mem.swapTotalKb = jsonUInt64(mem, "SwapTotal");
    out->mem.swapFreeKb = jsonUInt64(mem, "SwapFree");

    const QJsonArray disks = root.value(QLatin1String("disks")).toArray();
    out->disks.reserve(disks.size());
    for (const QJsonValue &v : disks) {
        const QJsonObject d = v.toObject();
        DiskInfo disk;
        disk.filesystem = jsonString(d, "fs");
        disk.mountpoint = jsonString(d, "mount");
        disk.sizeKb = jsonUInt64(d, "size_kb");
        disk.usedKb = jsonUInt64(d, "used_kb");
        disk.availKb = jsonUInt64(d, "avail_kb");
        disk.usePercent = d.value(QLatin1String("use_pct")).toInt();
        out->disks.push_back(disk);
    }

    const QJsonArray diskIo = root.value(QLatin1String("disk_io")).toArray();
    out->diskIo.reserve(diskIo.size());
    for (const QJsonValue &v : diskIo) {
        const QJsonObject d = v.toObject();
        DiskIoInfo io;
        io.name = jsonString(d, "name");
        io.readsCompleted = jsonUInt64(d, "reads");
        io.writesCompleted = jsonUInt64(d, "writes");
        io.sectorsRead = jsonUInt64(d, "sectors_read");
        io.sectorsWritten = jsonUInt64(d, "sectors_written");
        io.ioTicksMs = jsonUInt64(d, "io_ticks");
        out->diskIo.push_back(io);
    }

    const QJsonArray temps = root.value(QLatin1String("temps")).toArray();
    out->temps.reserve(temps.size());
    for (const QJsonValue &v : temps) {
        const QJsonObject t = v.toObject();
        TempSensorInfo sensor;
        sensor.name = jsonString(t, "name");
        sensor.celsius = jsonDouble(t, "celsius");
        if (sensor.celsius < -273.0) {
            sensor.celsius = -1.0;
        }
        out->temps.push_back(sensor);
    }

    const QJsonArray gpus = root.value(QLatin1String("gpus")).toArray();
    out->gpus.reserve(gpus.size());
    for (const QJsonValue &v : gpus) {
        const QJsonObject g = v.toObject();
        GpuInfo gpu;
        gpu.index = jsonOptionalInt(g, "index");
        gpu.name = jsonString(g, "name");
        gpu.uuid = jsonString(g, "uuid");
        gpu.pciBusId = jsonString(g, "pci");
        gpu.driverVersion = jsonString(g, "driver");
        gpu.pstate = jsonString(g, "pstate");
        gpu.utilGpuPercent = jsonOptionalDouble(g, "util_gpu");
        gpu.utilMemPercent = jsonOptionalDouble(g, "util_mem");
        gpu.memTotalMiB = jsonOptionalInt64(g, "mem_total");
        gpu.memUsedMiB = jsonOptionalInt64(g, "mem_used");
        gpu.memFreeMiB = jsonOptionalInt64(g, "mem_free");
        gpu.tempCelsius = jsonOptionalDouble(g, "temp");
        gpu.powerDrawW = jsonOptionalDouble(g, "power_draw");
        gpu.powerLimitW = jsonOptionalDouble(g, "power_limit");
        gpu.clockSmMHz = jsonOptionalInt64(g, "clock_sm");
        gpu.clockMemMHz = jsonOptionalInt64(g, "clock_mem");
        out->gpus.push_back(gpu);
    }

    const QJsonArray gpuProcs = root.value(QLatin1String("gpu_procs")).toArray();
    out->gpuProcesses.reserve(gpuProcs.size());
    for (const QJsonValue &v : gpuProcs) {
        const QJsonObject p = v.toObject();
        GpuProcessInfo proc;
        proc.pid = jsonOptionalInt64(p, "pid");
        proc.name = jsonString(p, "name");
        proc.gpuUuid = jsonString(p, "gpu_uuid");
        proc.usedMemoryMiB = jsonOptionalInt64(p, "used_mem");
        out->gpuProcesses.push_back(proc);
    }

    const QJsonArray nics = root.value(QLatin1String("nics")).toArray();
    out->nics.reserve(nics.size());
    for (const QJsonValue &v : nics) {
        const QJsonObject n = v.toObject();
        NicInfo nic;
        nic.name = jsonString(n, "name");
        nic.mac = jsonString(n, "mac");
        nic.operState = jsonString(n, "state");
        nic.ipv4 = jsonString(n, "ipv4");
        nic.ipv6 = jsonString(n, "ipv6");
        nic.speedMbps =
            static_cast<qint64>(n.value(QLatin1String("speed_mbps")).toVariant().toLongLong());
        if (nic.speedMbps < 0) {
            nic.speedMbps = -1;
        }
        nic.mtu = n.value(QLatin1String("mtu")).toInt();
        nic.rxBytes = jsonUInt64(n, "rx_bytes");
        nic.txBytes = jsonUInt64(n, "tx_bytes");
        nic.rxPackets = jsonUInt64(n, "rx_packets");
        nic.txPackets = jsonUInt64(n, "tx_packets");
        nic.rxErrors = jsonUInt64(n, "rx_errors");
        nic.txErrors = jsonUInt64(n, "tx_errors");
        out->nics.push_back(nic);
    }

    const QJsonObject virt = root.value(QLatin1String("virt")).toObject();
    out->virt.detectVirt = jsonString(virt, "detect");
    out->virt.vm = jsonString(virt, "vm");
    out->virt.container = jsonString(virt, "container");
    out->virt.isVm = virt.value(QLatin1String("is_vm")).toBool();
    out->virt.isContainer = virt.value(QLatin1String("is_container")).toBool();
    out->virt.cpuHypervisorFlag = virt.value(QLatin1String("cpu_hypervisor")).toBool();
    out->virt.cpuVendor = jsonString(virt, "cpu_vendor");
    out->virt.dmiSysVendor = jsonString(virt, "dmi_sys_vendor");
    out->virt.dmiProductName = jsonString(virt, "dmi_product_name");
    out->virt.dmiProductVersion = jsonString(virt, "dmi_product_version");
    out->virt.dmiBoardVendor = jsonString(virt, "dmi_board_vendor");
    out->virt.dmiBoardName = jsonString(virt, "dmi_board_name");
    out->virt.dmiChassisVendor = jsonString(virt, "dmi_chassis_vendor");
    out->virt.dmiChassisType = jsonString(virt, "dmi_chassis_type");
    out->virt.dmiBiosVendor = jsonString(virt, "dmi_bios_vendor");
    out->virt.dmiBiosVersion = jsonString(virt, "dmi_bios_version");
    out->virt.dmiBiosDate = jsonString(virt, "dmi_bios_date");
    out->virt.dockerEnv = virt.value(QLatin1String("docker_env")).toBool();
    out->virt.podmanEnv = virt.value(QLatin1String("podman_env")).toBool();
    out->virt.wsl = virt.value(QLatin1String("wsl")).toBool();
    out->virt.cgroupInit = jsonString(virt, "cgroup_init");

    return true;
}

ExplorerCapability classifyFailure(int exitStatus,
                                   const QByteArray &stderrBytes,
                                   const QString &errorMessage,
                                   QString *messageOut)
{
    const QString stderrText = QString::fromUtf8(stderrBytes).trimmed();
    const QString detail =
        !errorMessage.isEmpty()
            ? errorMessage
            : (!stderrText.isEmpty() ? stderrText
                                     : trParse("Remote command failed (exit %1)").arg(exitStatus));

    if (looksLikeUnavailable(stderrText, errorMessage) || exitStatus == 127) {
        if (messageOut) {
            *messageOut =
                trParse("System info is not available on this host (requires Linux /proc).");
        }
        return ExplorerCapability::Unavailable;
    }
    if (looksLikePermissionDenied(stderrText, errorMessage) || exitStatus == 126) {
        if (messageOut) {
            *messageOut = trParse("Permission denied while reading system info.");
        }
        return ExplorerCapability::PermissionDenied;
    }
    if (messageOut) {
        *messageOut = detail;
    }
    return ExplorerCapability::Error;
}

QString formatBytes(quint64 bytes)
{
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;
    constexpr double kTiB = kGiB * 1024.0;
    const double b = static_cast<double>(bytes);

    if (b >= kTiB) {
        return trParse("%1 TiB").arg(b / kTiB, 0, 'f', 1);
    }
    if (b >= kGiB) {
        return trParse("%1 GiB").arg(b / kGiB, 0, 'f', 1);
    }
    if (b >= kMiB) {
        return trParse("%1 MiB").arg(b / kMiB, 0, 'f', 1);
    }
    if (b >= kKiB) {
        return trParse("%1 KiB").arg(b / kKiB, 0, 'f', 0);
    }
    return trParse("%1 B").arg(bytes);
}

QString formatBytesFromKiB(quint64 kib)
{
    return formatBytes(kib * 1024ULL);
}

QString formatUptime(qint64 seconds)
{
    if (seconds < 0) {
        return QStringLiteral("—");
    }
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 mins = (seconds % 3600) / 60;
    if (days > 0) {
        return trParse("%1d %2h %3m").arg(days).arg(hours).arg(mins);
    }
    if (hours > 0) {
        return trParse("%1h %2m").arg(hours).arg(mins);
    }
    return trParse("%1m").arg(mins);
}

QString formatPercent(double percent)
{
    if (percent < 0.0) {
        return QStringLiteral("—");
    }
    return trParse("%1%").arg(percent, 0, 'f', 1);
}

QString formatRateBps(double bps)
{
    if (bps < 0.0) {
        return QStringLiteral("—");
    }
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = kKiB * 1024.0;
    constexpr double kGiB = kMiB * 1024.0;
    if (bps >= kGiB) {
        return trParse("%1 GiB/s").arg(bps / kGiB, 0, 'f', 2);
    }
    if (bps >= kMiB) {
        return trParse("%1 MiB/s").arg(bps / kMiB, 0, 'f', 2);
    }
    if (bps >= kKiB) {
        return trParse("%1 KiB/s").arg(bps / kKiB, 0, 'f', 1);
    }
    return trParse("%1 B/s").arg(bps, 0, 'f', 0);
}

QString formatFreqKHz(qint64 kHz)
{
    if (kHz <= 0) {
        return QStringLiteral("—");
    }
    if (kHz >= 1000000) {
        return trParse("%1 GHz").arg(static_cast<double>(kHz) / 1000000.0, 0, 'f', 2);
    }
    return trParse("%1 MHz").arg(static_cast<double>(kHz) / 1000.0, 0, 'f', 0);
}

QString formatIops(double iops)
{
    if (iops < 0.0) {
        return QStringLiteral("—");
    }
    return trParse("%1").arg(iops, 0, 'f', 1);
}

QString formatCelsius(double celsius)
{
    // Convention: -1.0 means unknown / unavailable.
    if (celsius == -1.0) {
        return QStringLiteral("—");
    }
    return trParse("%1 °C").arg(celsius, 0, 'f', 1);
}

QString formatLinkSpeed(qint64 speedMbps)
{
    if (speedMbps < 0) {
        return QStringLiteral("—");
    }
    if (speedMbps >= 1000 && speedMbps % 1000 == 0) {
        return trParse("%1 Gbps").arg(speedMbps / 1000);
    }
    if (speedMbps >= 1000) {
        return trParse("%1 Gbps").arg(static_cast<double>(speedMbps) / 1000.0, 0, 'f', 1);
    }
    return trParse("%1 Mbps").arg(speedMbps);
}

QString formatSnapshotText(const SystemInfo &info)
{
    QStringList lines;
    lines << trParse("=== System Info ===");
    lines << trParse("OS: %1").arg(info.os.prettyName.isEmpty() ? QStringLiteral("—")
                                                                : info.os.prettyName);
    lines << trParse("Kernel: %1")
                 .arg(info.os.kernel.isEmpty() ? QStringLiteral("—") : info.os.kernel);
    lines << trParse("Arch: %1").arg(info.os.arch.isEmpty() ? QStringLiteral("—") : info.os.arch);
    lines << trParse("Hostname: %1")
                 .arg(info.os.hostname.isEmpty() ? QStringLiteral("—") : info.os.hostname);
    lines << trParse("Uptime: %1").arg(formatUptime(info.os.uptimeSec));
    lines << trParse("Load: %1 / %2 / %3")
                 .arg(info.load.load1, 0, 'f', 2)
                 .arg(info.load.load5, 0, 'f', 2)
                 .arg(info.load.load15, 0, 'f', 2);
    lines << QString();
    lines << trParse("--- CPU ---");
    lines << trParse("Model: %1 (%2 logical)")
                 .arg(info.cpu.model.isEmpty() ? QStringLiteral("—") : info.cpu.model)
                 .arg(info.cpu.logicalCpus);
    lines << trParse("Usage: %1").arg(formatPercent(info.cpu.usagePercent));
    if (!info.cpu.governor.isEmpty() || info.cpu.freqMinKHz > 0 || info.cpu.freqMaxKHz > 0) {
        lines << trParse("Governor: %1 · Range: %2 – %3")
                     .arg(info.cpu.governor.isEmpty() ? QStringLiteral("—") : info.cpu.governor,
                          formatFreqKHz(info.cpu.freqMinKHz),
                          formatFreqKHz(info.cpu.freqMaxKHz));
    }
    lines << QString();
    lines << trParse("--- Memory ---");
    lines << trParse("Total: %1 · Available: %2 · Free: %3")
                 .arg(formatBytesFromKiB(info.mem.totalKb),
                      formatBytesFromKiB(info.mem.availableKb),
                      formatBytesFromKiB(info.mem.freeKb));
    lines << trParse("Buffers: %1 · Cached: %2 · Shmem: %3 · SReclaimable: %4")
                 .arg(formatBytesFromKiB(info.mem.buffersKb),
                      formatBytesFromKiB(info.mem.cachedKb),
                      formatBytesFromKiB(info.mem.shmemKb),
                      formatBytesFromKiB(info.mem.sReclaimableKb));
    if (info.mem.swapTotalKb > 0) {
        const quint64 swapUsed = info.mem.swapTotalKb > info.mem.swapFreeKb
                                     ? info.mem.swapTotalKb - info.mem.swapFreeKb
                                     : 0;
        lines << trParse("Swap: %1 / %2")
                     .arg(formatBytesFromKiB(swapUsed), formatBytesFromKiB(info.mem.swapTotalKb));
    } else {
        lines << trParse("Swap: none");
    }
    if (!info.temps.isEmpty()) {
        lines << QString();
        lines << trParse("--- Temperature ---");
        for (const TempSensorInfo &t : info.temps) {
            lines << trParse("%1: %2").arg(t.name, formatCelsius(t.celsius));
        }
    }
    if (!info.gpus.isEmpty()) {
        lines << QString();
        lines << trParse("--- GPU ---");
        for (const GpuInfo &g : info.gpus) {
            const QString mem = (g.memUsedMiB >= 0 && g.memTotalMiB >= 0)
                                    ? trParse("%1 / %2 MiB").arg(g.memUsedMiB).arg(g.memTotalMiB)
                                    : QStringLiteral("—");
            const QString power =
                (g.powerDrawW >= 0.0)
                    ? ((g.powerLimitW >= 0.0) ? trParse("%1 / %2 W")
                                                    .arg(g.powerDrawW, 0, 'f', 1)
                                                    .arg(g.powerLimitW, 0, 'f', 0)
                                              : trParse("%1 W").arg(g.powerDrawW, 0, 'f', 1))
                    : QStringLiteral("—");
            lines << trParse("[%1] %2  util %3  mem %4  temp %5  power %6  %7")
                         .arg(g.index >= 0 ? QString::number(g.index) : QStringLiteral("—"),
                              g.name.isEmpty() ? QStringLiteral("—") : g.name,
                              formatPercent(g.utilGpuPercent),
                              mem,
                              formatCelsius(g.tempCelsius),
                              power,
                              g.pstate.isEmpty() ? QStringLiteral("—") : g.pstate);
            if (!g.uuid.isEmpty() || !g.driverVersion.isEmpty()) {
                lines << trParse("  UUID %1  driver %2")
                             .arg(g.uuid.isEmpty() ? QStringLiteral("—") : g.uuid,
                                  g.driverVersion.isEmpty() ? QStringLiteral("—")
                                                            : g.driverVersion);
            }
        }
        if (!info.gpuProcesses.isEmpty()) {
            lines << trParse("Processes:");
            for (const GpuProcessInfo &p : info.gpuProcesses) {
                lines << trParse("  pid %1  %2  mem %3 MiB  gpu %4")
                             .arg(p.pid >= 0 ? QString::number(p.pid) : QStringLiteral("—"),
                                  p.name.isEmpty() ? QStringLiteral("—") : p.name,
                                  p.usedMemoryMiB >= 0 ? QString::number(p.usedMemoryMiB)
                                                       : QStringLiteral("—"),
                                  p.gpuUuid.isEmpty() ? QStringLiteral("—") : p.gpuUuid);
            }
        }
    }
    if (!info.disks.isEmpty()) {
        lines << QString();
        lines << trParse("--- Disks ---");
        for (const DiskInfo &d : info.disks) {
            lines << trParse("%1 on %2 — %3 used / %4 (%5%)")
                         .arg(d.filesystem,
                              d.mountpoint,
                              formatBytesFromKiB(d.usedKb),
                              formatBytesFromKiB(d.sizeKb))
                         .arg(d.usePercent);
        }
    }
    if (!info.diskIo.isEmpty()) {
        lines << QString();
        lines << trParse("--- Disk I/O ---");
        for (const DiskIoInfo &d : info.diskIo) {
            lines << trParse("%1  R %2  W %3  util %4")
                         .arg(d.name,
                              formatRateBps(d.readBps),
                              formatRateBps(d.writeBps),
                              formatPercent(d.utilPercent));
        }
    }
    if (!info.nics.isEmpty()) {
        lines << QString();
        lines << trParse("--- Network ---");
        for (const NicInfo &n : info.nics) {
            QStringList addr;
            if (!n.ipv4.isEmpty()) {
                addr << n.ipv4;
            }
            if (!n.ipv6.isEmpty()) {
                addr << n.ipv6;
            }
            lines << trParse("%1 [%2] %3  MTU %4  %5")
                         .arg(n.name,
                              n.operState.isEmpty() ? QStringLiteral("—") : n.operState,
                              formatLinkSpeed(n.speedMbps))
                         .arg(n.mtu > 0 ? QString::number(n.mtu) : QStringLiteral("—"),
                              addr.isEmpty() ? QStringLiteral("—") : addr.join(QLatin1Char(' ')));
            lines << trParse("  RX %1  TX %2  err %3/%4")
                         .arg(n.rxBps >= 0.0 ? formatRateBps(n.rxBps) : formatBytes(n.rxBytes),
                              n.txBps >= 0.0 ? formatRateBps(n.txBps) : formatBytes(n.txBytes))
                         .arg(n.rxErrors)
                         .arg(n.txErrors);
        }
    }
    lines << QString();
    lines << trParse("--- Virtualization ---");
    lines << trParse("Detected: %1 (VM=%2, container=%3)")
                 .arg(info.virt.detectVirt.isEmpty() ? QStringLiteral("none")
                                                     : info.virt.detectVirt,
                      info.virt.vm.isEmpty() ? QStringLiteral("none") : info.virt.vm,
                      info.virt.container.isEmpty() ? QStringLiteral("none") : info.virt.container);
    if (!info.virt.dmiSysVendor.isEmpty() || !info.virt.dmiProductName.isEmpty()) {
        lines << trParse("DMI: %1 %2")
                     .arg(info.virt.dmiSysVendor, info.virt.dmiProductName)
                     .trimmed();
    }
    return lines.join(QLatin1Char('\n'));
}

QString formatSnapshotJson(const SystemInfo &info)
{
    QJsonObject root;

    QJsonObject os;
    os.insert(QStringLiteral("pretty"), info.os.prettyName);
    os.insert(QStringLiteral("kernel"), info.os.kernel);
    os.insert(QStringLiteral("arch"), info.os.arch);
    os.insert(QStringLiteral("hostname"), info.os.hostname);
    os.insert(QStringLiteral("uptime_sec"), static_cast<qint64>(info.os.uptimeSec));
    root.insert(QStringLiteral("os"), os);

    QJsonObject load;
    load.insert(QStringLiteral("1"), info.load.load1);
    load.insert(QStringLiteral("5"), info.load.load5);
    load.insert(QStringLiteral("15"), info.load.load15);
    root.insert(QStringLiteral("load"), load);

    QJsonObject cpu;
    cpu.insert(QStringLiteral("model"), info.cpu.model);
    cpu.insert(QStringLiteral("logical"), info.cpu.logicalCpus);
    cpu.insert(QStringLiteral("governor"), info.cpu.governor);
    cpu.insert(QStringLiteral("usage_percent"), info.cpu.usagePercent);
    cpu.insert(QStringLiteral("freq_min_khz"), info.cpu.freqMinKHz);
    cpu.insert(QStringLiteral("freq_max_khz"), info.cpu.freqMaxKHz);
    root.insert(QStringLiteral("cpu"), cpu);

    QJsonObject mem;
    mem.insert(QStringLiteral("MemTotal"), static_cast<qint64>(info.mem.totalKb));
    mem.insert(QStringLiteral("MemAvailable"), static_cast<qint64>(info.mem.availableKb));
    mem.insert(QStringLiteral("MemFree"), static_cast<qint64>(info.mem.freeKb));
    mem.insert(QStringLiteral("Buffers"), static_cast<qint64>(info.mem.buffersKb));
    mem.insert(QStringLiteral("Cached"), static_cast<qint64>(info.mem.cachedKb));
    mem.insert(QStringLiteral("Shmem"), static_cast<qint64>(info.mem.shmemKb));
    mem.insert(QStringLiteral("SReclaimable"), static_cast<qint64>(info.mem.sReclaimableKb));
    mem.insert(QStringLiteral("SwapTotal"), static_cast<qint64>(info.mem.swapTotalKb));
    mem.insert(QStringLiteral("SwapFree"), static_cast<qint64>(info.mem.swapFreeKb));
    root.insert(QStringLiteral("mem"), mem);

    QJsonArray temps;
    for (const TempSensorInfo &t : info.temps) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), t.name);
        o.insert(QStringLiteral("celsius"), t.celsius);
        temps.append(o);
    }
    root.insert(QStringLiteral("temps"), temps);

    QJsonArray gpus;
    for (const GpuInfo &g : info.gpus) {
        QJsonObject o;
        o.insert(QStringLiteral("index"), g.index);
        o.insert(QStringLiteral("name"), g.name);
        o.insert(QStringLiteral("uuid"), g.uuid);
        o.insert(QStringLiteral("pci"), g.pciBusId);
        o.insert(QStringLiteral("driver"), g.driverVersion);
        o.insert(QStringLiteral("pstate"), g.pstate);
        o.insert(QStringLiteral("util_gpu"), g.utilGpuPercent);
        o.insert(QStringLiteral("util_mem"), g.utilMemPercent);
        o.insert(QStringLiteral("mem_total"), g.memTotalMiB);
        o.insert(QStringLiteral("mem_used"), g.memUsedMiB);
        o.insert(QStringLiteral("mem_free"), g.memFreeMiB);
        o.insert(QStringLiteral("temp"), g.tempCelsius);
        o.insert(QStringLiteral("power_draw"), g.powerDrawW);
        o.insert(QStringLiteral("power_limit"), g.powerLimitW);
        o.insert(QStringLiteral("clock_sm"), g.clockSmMHz);
        o.insert(QStringLiteral("clock_mem"), g.clockMemMHz);
        gpus.append(o);
    }
    root.insert(QStringLiteral("gpus"), gpus);

    QJsonArray gpuProcs;
    for (const GpuProcessInfo &p : info.gpuProcesses) {
        QJsonObject o;
        o.insert(QStringLiteral("pid"), p.pid);
        o.insert(QStringLiteral("name"), p.name);
        o.insert(QStringLiteral("gpu_uuid"), p.gpuUuid);
        o.insert(QStringLiteral("used_mem"), p.usedMemoryMiB);
        gpuProcs.append(o);
    }
    root.insert(QStringLiteral("gpu_procs"), gpuProcs);

    QJsonArray disks;
    for (const DiskInfo &d : info.disks) {
        QJsonObject o;
        o.insert(QStringLiteral("fs"), d.filesystem);
        o.insert(QStringLiteral("mount"), d.mountpoint);
        o.insert(QStringLiteral("size_kb"), static_cast<qint64>(d.sizeKb));
        o.insert(QStringLiteral("used_kb"), static_cast<qint64>(d.usedKb));
        o.insert(QStringLiteral("avail_kb"), static_cast<qint64>(d.availKb));
        o.insert(QStringLiteral("use_pct"), d.usePercent);
        disks.append(o);
    }
    root.insert(QStringLiteral("disks"), disks);

    QJsonArray diskIo;
    for (const DiskIoInfo &d : info.diskIo) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), d.name);
        o.insert(QStringLiteral("read_bps"), d.readBps);
        o.insert(QStringLiteral("write_bps"), d.writeBps);
        o.insert(QStringLiteral("read_iops"), d.readIops);
        o.insert(QStringLiteral("write_iops"), d.writeIops);
        o.insert(QStringLiteral("util_percent"), d.utilPercent);
        diskIo.append(o);
    }
    root.insert(QStringLiteral("disk_io"), diskIo);

    QJsonArray nics;
    for (const NicInfo &n : info.nics) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), n.name);
        o.insert(QStringLiteral("state"), n.operState);
        o.insert(QStringLiteral("mac"), n.mac);
        o.insert(QStringLiteral("ipv4"), n.ipv4);
        o.insert(QStringLiteral("ipv6"), n.ipv6);
        o.insert(QStringLiteral("speed_mbps"), n.speedMbps);
        o.insert(QStringLiteral("mtu"), n.mtu);
        o.insert(QStringLiteral("rx_bps"), n.rxBps);
        o.insert(QStringLiteral("tx_bps"), n.txBps);
        o.insert(QStringLiteral("rx_errors"), static_cast<qint64>(n.rxErrors));
        o.insert(QStringLiteral("tx_errors"), static_cast<qint64>(n.txErrors));
        nics.append(o);
    }
    root.insert(QStringLiteral("nics"), nics);

    QJsonObject virt;
    virt.insert(QStringLiteral("detect"), info.virt.detectVirt);
    virt.insert(QStringLiteral("vm"), info.virt.vm);
    virt.insert(QStringLiteral("container"), info.virt.container);
    virt.insert(QStringLiteral("is_vm"), info.virt.isVm);
    virt.insert(QStringLiteral("is_container"), info.virt.isContainer);
    virt.insert(QStringLiteral("dmi_sys_vendor"), info.virt.dmiSysVendor);
    virt.insert(QStringLiteral("dmi_product_name"), info.virt.dmiProductName);
    root.insert(QStringLiteral("virt"), virt);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace SystemInfoParser
