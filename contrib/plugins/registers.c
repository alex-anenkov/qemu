/*
 * Copyright (C) 2021, Alexandre Iooss <erdnaxe@crans.org>
 *
 * Log instruction execution with memory access.
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define DF_MASK                 0x00000400
/* eflags masks */
#define CC_C    0x0001
#define CC_P    0x0004
#define CC_A    0x0010
#define CC_Z    0x0040
#define CC_S    0x0080
#define CC_O    0x0800

static void print_eflags(GString *log,
                         const char *regname,
                         uint64_t *regdata,
                         size_t regdata_size,
                         unsigned int cpu_index)
{
    uint32_t v = (uint32_t)*regdata;
    g_string_append_printf(log, "cpu=%u, %s=%08x [%c%c%c%c%c%c%c], size=%ld\n", cpu_index, regname,
                v,
                v & DF_MASK ? 'D' : '-',
                v & CC_O ? 'O' : '-',
                v & CC_S ? 'S' : '-',
                v & CC_Z ? 'Z' : '-',
                v & CC_A ? 'A' : '-',
                v & CC_P ? 'P' : '-',
                v & CC_C ? 'C' : '-',
                regdata_size);
}

static void print_register(GString *log, const char *regname, unsigned int cpu_index)
{
    size_t reg;
    if (qemu_plugin_find_reg(regname, &reg)) {
        size_t regdata_size;
        uint64_t *regdata = qemu_plugin_read_reg(reg, &regdata_size);
        if (strcmp(regname, "eflags") == 0) {
            print_eflags(log, regname, regdata, regdata_size, cpu_index);
        }
        else {
            g_string_append_printf(log, "cpu=%u, %s=%08x, size=%ld\n", cpu_index, regname, (uint32_t)*regdata, regdata_size);
        }
        g_free(regdata);
    }
    else {
        g_string_append_printf(log, "register %s not found\n", regname);
    }
}

/**
 * Log registers on instruction execution
 */
static void vcpu_insn_exec(unsigned int cpu_index, void *udata)
{
    g_autoptr(GString) log = g_string_new("");
    print_register(log, "rax", cpu_index);
    print_register(log, "rbx", cpu_index);
    print_register(log, "rcx", cpu_index);
    print_register(log, "rdx", cpu_index);
    print_register(log, "rsi", cpu_index);
    print_register(log, "rdi", cpu_index);
    print_register(log, "rbp", cpu_index);
    print_register(log, "rsp", cpu_index);
    print_register(log, "r8", cpu_index);
    print_register(log, "r9", cpu_index);
    print_register(log, "r10", cpu_index);
    print_register(log, "r11", cpu_index);
    print_register(log, "r12", cpu_index);
    print_register(log, "r13", cpu_index);
    print_register(log, "r14", cpu_index);
    print_register(log, "r15", cpu_index);
    print_register(log, "rip", cpu_index);
    print_register(log, "cs", cpu_index);
    print_register(log, "ss", cpu_index);
    print_register(log, "ds", cpu_index);
    print_register(log, "es", cpu_index);
    print_register(log, "fs", cpu_index);
    print_register(log, "gs", cpu_index);
    print_register(log, "fs_base", cpu_index);
    print_register(log, "gs_base", cpu_index);
    print_register(log, "eflags", cpu_index);
    qemu_plugin_outs(log->str);
}

/**
 * On translation block new translation
 *
 * QEMU convert code by translation block (TB). By hooking here we can then hook
 * a callback on each instruction.
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;

    size_t n = qemu_plugin_tb_n_insns(tb);
    for (size_t i = 0; i < n; i++) {
        insn = qemu_plugin_tb_get_insn(tb, i);
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, NULL);

    }
}

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);

    return 0;
}
