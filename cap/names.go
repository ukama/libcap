package cap

/* ** DO NOT EDIT THIS FILE. IT WAS AUTO-GENERATED BY LIBCAP'S GO BUILDER (mknames.go) ** */

// NamedCount holds the number of capabilities with official names.
const NamedCount = 40

// CHOWN etc., are the named capability bits on this system. The
// canonical source for each name is the "uapi/linux/capabilities.h"
// file, which is hard-coded into this package.
const (
	CHOWN Value = iota
	DAC_OVERRIDE
	DAC_READ_SEARCH
	FOWNER
	FSETID
	KILL
	SETGID
	SETUID
	SETPCAP
	LINUX_IMMUTABLE
	NET_BIND_SERVICE
	NET_BROADCAST
	NET_ADMIN
	NET_RAW
	IPC_LOCK
	IPC_OWNER
	SYS_MODULE
	SYS_RAWIO
	SYS_CHROOT
	SYS_PTRACE
	SYS_PACCT
	SYS_ADMIN
	SYS_BOOT
	SYS_NICE
	SYS_RESOURCE
	SYS_TIME
	SYS_TTY_CONFIG
	MKNOD
	LEASE
	AUDIT_WRITE
	AUDIT_CONTROL
	SETFCAP
	MAC_OVERRIDE
	MAC_ADMIN
	SYSLOG
	WAKE_ALARM
	BLOCK_SUSPEND
	AUDIT_READ
	PERFMON
	BPF
)

var names = map[Value]string{
	CHOWN:            "cap_chown",
	DAC_OVERRIDE:     "cap_dac_override",
	DAC_READ_SEARCH:  "cap_dac_read_search",
	FOWNER:           "cap_fowner",
	FSETID:           "cap_fsetid",
	KILL:             "cap_kill",
	SETGID:           "cap_setgid",
	SETUID:           "cap_setuid",
	SETPCAP:          "cap_setpcap",
	LINUX_IMMUTABLE:  "cap_linux_immutable",
	NET_BIND_SERVICE: "cap_net_bind_service",
	NET_BROADCAST:    "cap_net_broadcast",
	NET_ADMIN:        "cap_net_admin",
	NET_RAW:          "cap_net_raw",
	IPC_LOCK:         "cap_ipc_lock",
	IPC_OWNER:        "cap_ipc_owner",
	SYS_MODULE:       "cap_sys_module",
	SYS_RAWIO:        "cap_sys_rawio",
	SYS_CHROOT:       "cap_sys_chroot",
	SYS_PTRACE:       "cap_sys_ptrace",
	SYS_PACCT:        "cap_sys_pacct",
	SYS_ADMIN:        "cap_sys_admin",
	SYS_BOOT:         "cap_sys_boot",
	SYS_NICE:         "cap_sys_nice",
	SYS_RESOURCE:     "cap_sys_resource",
	SYS_TIME:         "cap_sys_time",
	SYS_TTY_CONFIG:   "cap_sys_tty_config",
	MKNOD:            "cap_mknod",
	LEASE:            "cap_lease",
	AUDIT_WRITE:      "cap_audit_write",
	AUDIT_CONTROL:    "cap_audit_control",
	SETFCAP:          "cap_setfcap",
	MAC_OVERRIDE:     "cap_mac_override",
	MAC_ADMIN:        "cap_mac_admin",
	SYSLOG:           "cap_syslog",
	WAKE_ALARM:       "cap_wake_alarm",
	BLOCK_SUSPEND:    "cap_block_suspend",
	AUDIT_READ:       "cap_audit_read",
	PERFMON:          "cap_perfmon",
	BPF:              "cap_bpf",
}

var bits = map[string]Value{
	"cap_chown":            CHOWN,
	"cap_dac_override":     DAC_OVERRIDE,
	"cap_dac_read_search":  DAC_READ_SEARCH,
	"cap_fowner":           FOWNER,
	"cap_fsetid":           FSETID,
	"cap_kill":             KILL,
	"cap_setgid":           SETGID,
	"cap_setuid":           SETUID,
	"cap_setpcap":          SETPCAP,
	"cap_linux_immutable":  LINUX_IMMUTABLE,
	"cap_net_bind_service": NET_BIND_SERVICE,
	"cap_net_broadcast":    NET_BROADCAST,
	"cap_net_admin":        NET_ADMIN,
	"cap_net_raw":          NET_RAW,
	"cap_ipc_lock":         IPC_LOCK,
	"cap_ipc_owner":        IPC_OWNER,
	"cap_sys_module":       SYS_MODULE,
	"cap_sys_rawio":        SYS_RAWIO,
	"cap_sys_chroot":       SYS_CHROOT,
	"cap_sys_ptrace":       SYS_PTRACE,
	"cap_sys_pacct":        SYS_PACCT,
	"cap_sys_admin":        SYS_ADMIN,
	"cap_sys_boot":         SYS_BOOT,
	"cap_sys_nice":         SYS_NICE,
	"cap_sys_resource":     SYS_RESOURCE,
	"cap_sys_time":         SYS_TIME,
	"cap_sys_tty_config":   SYS_TTY_CONFIG,
	"cap_mknod":            MKNOD,
	"cap_lease":            LEASE,
	"cap_audit_write":      AUDIT_WRITE,
	"cap_audit_control":    AUDIT_CONTROL,
	"cap_setfcap":          SETFCAP,
	"cap_mac_override":     MAC_OVERRIDE,
	"cap_mac_admin":        MAC_ADMIN,
	"cap_syslog":           SYSLOG,
	"cap_wake_alarm":       WAKE_ALARM,
	"cap_block_suspend":    BLOCK_SUSPEND,
	"cap_audit_read":       AUDIT_READ,
	"cap_perfmon":          PERFMON,
	"cap_bpf":              BPF,
}
