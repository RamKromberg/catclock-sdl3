#!/bin/sh

INTERVAL=1.5
HIDE_IDLE=false
SORT_BY_CPU=false

# Parse CLI arguments manually
while [ $# -gt 0 ]; do
  case "$1" in
    --hide)
      HIDE_IDLE=true
      shift
      ;;
    --sort)
      SORT_BY_CPU=true
      shift
      ;;
    -d)
      if [ -n "$2" ]; then
        INTERVAL="$2"
        shift 2
      else
        echo "Error: -d requires a numeric value"
        exit 1
      fi
      ;;
    *)
      shift
      ;;
  esac
done

# ==========================================
# COLOR & STYLE CONFIGURATION (htop Themes)
# ==========================================
RESET="\033[0m"
BOLD="\033[1m"

HEADER_TEXT_COLOR="\033[30m"      # Black text
HEADER_BG_COLOR="\033[42m"        # Green background
HEADER_COLOR="${HEADER_TEXT_COLOR}${HEADER_BG_COLOR}"

PID_COLOR="\033[1;36m"            # Bright Cyan
METRIC_COLOR="\033[0m"            # Neutral White
TIME_COLOR="\033[33m"             # Soft Yellow

TREE_COLOR="\033[32m"             # Dim Green
THREAD_COLOR="\033[1;30m"         # Dark Gray

CORES=$(nproc)

if [ -d "/run/user/$(id -u)" ]; then
  RAM_DIR="/run/user/$(id -u)"
else
  RAM_DIR="/tmp"
fi

CPU_LAST=$(mktemp "$RAM_DIR/track_cpu_last.XXXXXX")
CPU_CURR=$(mktemp "$RAM_DIR/track_cpu_curr.XXXXXX")
SYS_LAST=$(mktemp "$RAM_DIR/track_sys_last.XXXXXX")
RAW_STORE=$(mktemp "$RAM_DIR/track_raw.XXXXXX")
CALC_STORE=$(mktemp "$RAM_DIR/track_calc.XXXXXX")
FRAME_TMP=$(mktemp "$RAM_DIR/track_frame.XXXXXX")

trap 'rm -f "$CPU_LAST" "$CPU_CURR" "$SYS_LAST" "$RAW_STORE" "$CALC_STORE" "$FRAME_TMP"; exit' INT TERM EXIT

> "$CPU_LAST"
echo "0" > "$SYS_LAST"

while true; do
  ps -AT -o pid= -o lwp= -o user= -o vsz= -o rss= -o %mem= -o time= -o comm= -o args= | \
	grep "catclock-sdl3" | \
	grep -vE "(foot|bash|sh|grep|track.sh)" | \
	sort -k1,1n -k2,2n > "$RAW_STORE"

  SYS_TICKS2=$(awk '/^cpu / {print $2+$3+$4+$5+$6+$7+$8+$9+$10}' /proc/stat)

  while read -r pid tid user vsz rss pmem time thread_comm args; do
	[ -z "$pid" ] && continue
	stat_file="/proc/$pid/task/$tid/stat"
	if [ -f "$stat_file" ]; then
	  ticks=$(awk '{print $14+$15}' "$stat_file")
	  echo "$tid $ticks"
	fi
  done < "$RAW_STORE" > "$CPU_CURR"

  SYS_TICKS1=$(cat "$SYS_LAST")
  SYS_DELTA=$(awk -v s1="$SYS_TICKS1" -v s2="$SYS_TICKS2" 'BEGIN {print s2-s1}')

  w_pid=3; w_user=4; w_virt=4; w_res=3; w_shr=3; w_cpu=4; w_mem=4; w_time=5

  > "$CALC_STORE"
  while read -r pid tid user vsz rss pmem time thread_comm args; do
	[ -z "$pid" ] && continue

	pcpu="0.0"
	if [ "$SYS_DELTA" -gt 0 ] && [ -s "$CPU_LAST" ]; then
	  t1=$(grep "^$tid " "$CPU_LAST" | awk '{print $2}')
	  t2=$(grep "^$tid " "$CPU_CURR" | awk '{print $2}')

	  if [ -n "$t1" ] && [ -n "$t2" ]; then
		pcpu=$(awk -v t1="$t1" -v t2="$t2" -v sd="$SYS_DELTA" -v nc="$CORES" 'BEGIN {
		  diff = t2 - t1;
		  if (diff < 0) diff = 0;
		  printf "%.1f", (diff / sd) * 100 * nc
		}')
	  fi
	fi

	if [ "$HIDE_IDLE" = true ] && [ "$pcpu" = "0.0" ]; then
	  continue
	fi

	virt_m=$(awk -v k="$vsz" 'BEGIN {printf "%.0fM", k/1024}')
	res_m=$(awk -v k="$rss" 'BEGIN {printf "%.0fM", k/1024}')

	if [ -f "/proc/$pid/statm" ]; then
	  shr_m=$(awk '{printf "%.0fM", ($3 * 4) / 1024}' "/proc/$pid/statm")
	else
	  shr_m="0M"
	fi

	if [ "$pid" != "$tid" ]; then
	  shr_m="0"
	  rank=2
	else
	  rank=1
	fi

	# FIXED: Adaptive Command Column Graphics
	if [ "$pid" = "$tid" ]                    ; then cmd_display="${TREE_COLOR}├─ ${RESET}$args"
	elif [ "$SORT_BY_CPU" = true ]            ; then cmd_display="${TREE_COLOR}  → ${RESET}${THREAD_COLOR}(Thread) $thread_comm${RESET}"
	else                                           cmd_display="${TREE_COLOR}│  ├─ ${RESET}${THREAD_COLOR}$thread_comm${RESET}"
	fi

	[ ${#tid} -gt $w_pid ] && w_pid=${#tid}
	[ ${#user} -gt $w_user ] && w_user=${#user}
	[ ${#virt_m} -gt $w_virt ] && w_virt=${#virt_m}
	[ ${#res_m} -gt $w_res ] && w_res=${#res_m}
	[ ${#shr_m} -gt $w_shr ] && w_shr=${#shr_m}
	[ ${#pcpu} -gt $w_cpu ] && w_cpu=${#pcpu}
	[ ${#pmem} -gt $w_mem ] && w_mem=${#pmem}
	[ ${#time} -gt $w_time ] && w_time=${#time}

	echo "$rank|$pid|$tid|$user|$virt_m|$res_m|$shr_m|$pcpu|$pmem|$time|$cmd_display" >> "$CALC_STORE"
  done < "$RAW_STORE"

  HEADER_TEXT=$(printf "%-${w_pid}s %-${w_user}s %-${w_virt}s %-${w_res}s %-${w_shr}s %-${w_cpu}s %-${w_mem}s %-${w_time}s %s" \
	"PID" "USER" "VIRT" "RES" "SHR" "CPU%" "MEM%" "TIME+" "Command")
  FRAME="${HEADER_COLOR}${HEADER_TEXT}${RESET}"

  total_width=$((w_pid + w_user + w_virt + w_res + w_shr + w_cpu + w_mem + w_time + 9))
  DIVIDER=$(printf "%${total_width}s" | tr ' ' '-')
  FRAME="${FRAME}${IFS}${DIVIDER}"

  if [ "$SORT_BY_CPU" = true ]; then
	sorted_data=$(sort -t'|' -k1,1n -k2,2n -k8,8nr "$CALC_STORE")
  else
	sorted_data=$(cat "$CALC_STORE")
  fi

  > "$FRAME_TMP"
  echo "$sorted_data" | while IFS='|' read -r r_rank r_pid r_tid r_user r_virt r_res r_shr r_cpu r_mem r_time r_cmd; do
	[ -z "$r_tid" ] && continue
	printf -- "${PID_COLOR}%-${w_pid}s${RESET} %-${w_user}s ${METRIC_COLOR}%-${w_virt}s %-${w_res}s %-${w_shr}s %-${w_cpu}s %-${w_mem}s${RESET} ${TIME_COLOR}%-${w_time}s${RESET} %b\n" \
	  "$r_tid" "$r_user" "$r_virt" "$r_res" "$r_shr" "$r_cpu" "$r_mem" "$r_time" "$r_cmd"
  done >> "$FRAME_TMP"

  clear
  printf "%b\n" "$FRAME"
  cat "$FRAME_TMP"

  cat "$CPU_CURR" > "$CPU_LAST"
  echo "$SYS_TICKS2" > "$SYS_LAST"

  sleep "$INTERVAL"
done
