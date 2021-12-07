# -*- coding: UTF-8 -*-

import ctypes
import errno
import logging
import _ulog
import os
import struct
import sys
import threading
import time

_prio_logging_map = {
    0: logging.CRITICAL,
    1: logging.CRITICAL,
    2: logging.CRITICAL,
    3: logging.ERROR,
    4: logging.WARNING,
    5: logging.WARNING,
    6: logging.INFO,
    7: logging.DEBUG,
}


_logging_prio_map = {
    logging.CRITICAL: 0,
    logging.ERROR: 3,
    logging.WARNING: 4,
    logging.INFO: 6,
    logging.DEBUG: 7,
}


_ulog_loggers = []


def _ulog_bridge(prio, ulog_cookie, buf, size):
    # 1. forward the logs to ulogger kernel module
    global ulog_kernel_write_func
    if ulog_kernel_write_func is not None:
        ulog_kernel_write_func(prio, ulog_cookie, buf, size)

    # 2. handle the python logging side of things
    message = _ulog.string_cast(buf, errors="surrogateescape")
    message = message.strip("\r\n")
    level = _prio_logging_map[int(prio)]
    name = "ulog"
    func = ""
    if ulog_cookie:
        func = _ulog.string_cast(ulog_cookie.contents.name)

    record = None
    for ulog_logger in _ulog_loggers:
        if not ulog_logger.isEnabledFor(level):
            continue
        if record is None:
            record = ulog_logger.makeRecord(
                name, level, None, 0, message, [], None, func=func
            )
        ulog_logger.handle(record)


ulog_kernel_write_func = None
ulog_write_func = _ulog.ulog_write_func_t(_ulog_bridge)


def enable_bridge(logger, forward=True):
    global _ulog_loggers
    if logger not in _ulog_loggers:
        _ulog_loggers.append(logger)

    global ulog_kernel_write_func
    if not forward:
        ulog_kernel_write_func = None
    elif ulog_kernel_write_func is None:
        # backup the ulog kernel write function ...
        ulog_kernel_write_func = _ulog.ulog_get_write_func()
    # override ulog write function
    res = _ulog.ulog_set_write_func(ulog_write_func)
    if res != 0:
        ulog_kernel_write_func = None
        raise RuntimeError("Failed to init ulog write func")


def disable_bridge(logger):
    global _ulog_loggers
    if logger in _ulog_loggers:
        _ulog_loggers.remove(logger)


RawEntry = _ulog.struct_ulog_raw_entry


class ULogDeviceHandler(logging.Handler):

    def __init__(self, device_name=b"main", level=logging.NOTSET, raw_mode=True):
        super().__init__(level=level)

        self._pid = os.getpid()
        self._process_name = None
        self._thread_name = None
        self._thread_id = None
        self._clock_func = None
        self._raw_mode = raw_mode

        if isinstance(device_name, str):
            device_name = device_name.encode("utf-8")
        if not device_name.startswith(b"/dev/ulog_"):
            self._device_name = device_name
            self._device_path = b"/dev/ulog_%s" % device_name
        else:
            self._device_name = device_name[10:]
            self._device_path = device_name

        self._ensure_ulog_device()

        if self._raw_mode:
            # Try to open ulog device in raw mode and fallback to standard mode
            try:
                self._open_ulog_raw(device_name)
            except Exception:
                self._raw_mode = False
                self._open_ulog()
        else:
            self._open_ulog()

    def _ensure_ulog_device(self):
        if not os.path.exists(self._device_path):
            with open("/sys/devices/virtual/misc/ulog_main/logs", "wb") as f:
                f.write(b"ulog_%s 17" % self._device_name)
            time.sleep(1)
            if not os.path.exists(self._device_path):
                raise OSError(
                    errno.ENOENT,
                    "ulog device '{}' cannot be created".format(
                        self._device_name.decode()
                    ),
                )

    def _open_ulog_raw(self, device_name):
        # The process name is only used in raw mode (i.e. Linux only)
        # so we simply get the process name from Linux procfs.
        with open("/proc/self/comm", "rb") as f:
            self._process_name = f.read().strip()

        self.fd = _ulog.ulog_raw_open(_ulog.char_pointer_cast(self._device_path))
        if self.fd < 0:
            raise OSError(
                -self.fd,
                "Cannot open '{}' ulog device: {}".format(
                    self._device_path.decode(), errno.errorcode[-self.fd]
                ),
            )

    def _open_ulog(self):
        self.fd = os.open(self._device_path, os.O_WRONLY | os.O_CLOEXEC)
        if self.fd < 0:
            raise OSError(
                ctypes.get_errno(),
                "Cannot open '{}' ulog device: {}".format(
                    self._device_path.decode(), errno.errorcode[-self.fd]
                ),
            )

    def _get_clock(self):
        if self._clock_func is not None:
            return self._clock_func()
        else:
            return time.clock_gettime(time.CLOCK_REALTIME)

    def set_clock_func(self, clock_func):
        if not self._raw_mode:
            raise ValueError("Ulog clock function can only be changed when in raw mode")
        # Check that we can safely call this function
        if clock_func is not None:
            clock_func()
        self._clock_func = clock_func

    @property
    def raw_mode(self):
        return self._raw_mode

    @property
    def process_name(self):
        return self._process_name

    @process_name.setter
    def process_name(self, value):
        if isinstance(value, str):
            value = value.encode("utf-8")
        elif not isinstance(value, bytes):
            raise ValueError("Ulog process name should be of type 'str' or 'bytes'")
        self._process_name = value

    @property
    def thread_name(self):
        if self._thread_name:
            return self._thread_name
        local = threading.local()
        thread_name = getattr(local, "ulog_thread_name", None)
        if thread_name is None:
            local.ulog_thread_name = threading.current_thread().getName()
        return local.ulog_thread_name

    @thread_name.setter
    def thread_name(self, value):
        if isinstance(value, str):
            value = value.encode("utf-8")
        elif not isinstance(value, bytes):
            raise TypeError(
                "Ulog thread name should be of type 'str' or 'bytes' not '{}'".format(
                    type(value)
                )
            )
        self._thread_name = value

    @property
    def thread_id(self):
        if self._thread_id:
            return self._thread_id
        local = threading.local()
        thread_id = getattr(local, "ulog_thread_id", None)
        if thread_id is None:
            local.ulog_thread_id = ctypes.CDLL("libc.so.6").syscall(186)
        return local.ulog_thread_id

    @thread_id.setter
    def thread_id(self, value):
        if not isinstance(value, int):
            raise TypeError(
                "Ulog thread_id should be of type 'int' not '{}'".format(type(value))
            )
        self._thread_id = value

    @property
    def pid(self):
        return self._pid

    @pid.setter
    def pid(self, value):
        if not isinstance(value, int):
            raise TypeError(
                "Ulog pid should be of type 'int' not '{}'".format(type(value))
            )
        self._pid = value

    def emit(self, record):
        prio = _logging_prio_map[record.levelno]
        tag = record.name.encode("utf-8")
        message = self.format(record).encode("utf-8")
        lines = message.splitlines()
        for line in lines:
            if self._raw_mode:
                raw = RawEntry()
                self.format_raw_entry(raw, prio, tag, line)
                self.write_raw_entry(raw)
            else:
                self.write(prio, tag, line)

    def format_raw_entry(self, raw, prio, tag, message):
        raw.entry.pid = self.pid
        raw.entry.tid = self.thread_id
        ts = self._get_clock()
        raw.entry.sec = int(ts)
        raw.entry.nsec = int((ts - raw.entry.sec) * 1e9)
        raw.prio = prio
        raw.pname = _ulog.char_pointer_cast(self.process_name)
        raw.tname = _ulog.char_pointer_cast(self.thread_name)
        raw.tag = _ulog.char_pointer_cast(tag)
        raw.message = _ulog.char_pointer_cast(message)
        raw.pname_len = len(self.process_name) + 1
        raw.tname_len = len(self.thread_name) + 1
        raw.tag_len = len(tag) + 1
        raw.message_len = len(message) + 1

    def write_raw_entry(self, raw):
        return _ulog.ulog_raw_log(self.fd, ctypes.pointer(raw))

    def write(self, prio, tag, message):
        return os.writev(
            self.fd, [struct.pack("I", prio), tag + b"\0", message + b"\0"]
        )

    def close(self):
        super().close()
        if self._raw_mode:
            _ulog.ulog_raw_close(self.fd)
        else:
            os.close(self.fd)


class ULogHandler(logging.Handler):

    def __init__(self, level=logging.NOTSET):
        super().__init__(level=level)

        # Setup master cookie (not used for actual logging)
        name = b"ulog_py"
        self.master_cookie = _ulog.struct_ulog_cookie()
        self.master_cookie.name = ctypes.create_string_buffer(name)
        self.master_cookie.namesize = len(name) + 1
        self.master_cookie.level = -1
        self.master_cookie.userdata = None
        self.master_cookie.next = None
        _ulog.ulog_init_cookie(self.master_cookie)

    def emit(self, record):
        prio = _logging_prio_map[record.levelno]
        tag = record.name.encode("utf-8")

        # Master cookie level, this should have been initialized
        masterlevel = self.master_cookie.level

        # Use temporary cookie
        cookie = _ulog.struct_ulog_cookie()
        cookie.name = ctypes.create_string_buffer(tag)
        cookie.namesize = len(tag) + 1
        # This is safe only because level is non-negative
        cookie.level = masterlevel
        cookie.next = None

        message = self.format(record).encode("utf-8")

        # Force logging as Notice(level 5 in ulog) if message is an event
        if message.startswith(b"EVT:"):
            prio = 5

        lines = message.splitlines()
        for line in lines:
            _ulog.ulog_log_str(prio, cookie, line)


def setup_logging(process_name):
    """
    Setup python logging redirection to ulog and update process name so that it
    is not just 'python3'. process_name can be either a string or bytes and
    shall be less than or equal to 15 characters.
    It also attach a hook to sys.excepthook to intercept uncaught exceptions
    to log them in ulog as well (instead of just sdterr)

    It returns the root logger.
    """
    root_logger = logging.getLogger()
    root_logger.addHandler(ULogHandler())
    if os.getenv("ULOG_LEVEL") == "D":
        root_logger.setLevel(logging.DEBUG)
    else:
        root_logger.setLevel(logging.INFO)

    # Attach hook to intercept uncaught exceptions (for example during a
    # callback from native code). Otherwise they are only printed on stderr
    # that is not visible for background services.
    def handle_exception(exc_type, exc_value, exc_traceback):
        if issubclass(exc_type, KeyboardInterrupt):
            sys.__excepthook__(exc_type, exc_value, exc_traceback)
        else:
            root_logger.error(
                    "Uncaught exception",
                    exc_info=(exc_type, exc_value, exc_traceback))
            sys.exit(1)
    sys.excepthook = handle_exception

    if process_name and sys.platform == "linux":
        if isinstance(process_name, str):
            process_name = process_name.encode("utf-8")
        libc = ctypes.cdll.LoadLibrary("libc.so.6")
        if libc:
            # PR_SET_NAME=15
            libc.prctl(15, process_name, 0, 0, 0)

            # The warning log is after the change because in the simulator,
            # the process name is cached at first log...
            if len(process_name) > 15:
                root_logger.warning("Truncating process name %s -> %s",
                        process_name, process_name[:15])

    return root_logger
