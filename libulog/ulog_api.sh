# file to be sourced, in order to use ulog without using an external process
# such as ulogger for each logged line
#
# to use it, on top of your shell script, do :
#    ulog_tag="my super tag" # optional, defaulting to "(shell script)"
#    source /usr/share/ulog/ulog_api.sh

# redirect stdout and stderr to ulog, to catch logs not issued with ulogX funcs
exec 1> /dev/ulog_main 2>&1

ULOG_CRIT=2
ULOG_ERR=3
ULOG_WARN=4
ULOG_NOTICE=5
ULOG_INFO=6
ULOG_DEBUG=7

ulog() {
	level="$1"
	message="$2"
	if [ -z "${ulog_tag+x}" ]
	then
		ulog_tag="(shell script)"
	fi

	printf "%d\0\0\0%s\0%s\n" "${level}" "${ulog_tag}" "${message}"
}

ulogc() {
	ulog ${ULOG_CRIT} "$1"
}

uloge() {
	ulog ${ULOG_ERR} "$1"
}

ulogw() {
	ulog ${ULOG_WARN} "$1"
}

ulogn() {
	ulog ${ULOG_NOTICE} "$1"
}

ulogi() {
	ulog ${ULOG_INFO} "$1"
}

ulogd() {
	ulog ${ULOG_DEBUG} "$1"
}

