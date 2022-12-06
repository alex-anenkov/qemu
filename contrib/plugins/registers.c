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

static void print_register(GString *log, const char *reg_name, unsigned int cpu_index)
{
    qemu_plugin_reg_handle_t reg;
    if (qemu_plugin_find_reg(reg_name, &reg)) {
        g_autoptr(GByteArray) regdata;
        size_t size = qemu_plugin_read_reg(&reg, &regdata);
        uint64_t v = g_array_index(regdata, uint64_t, 0);
        g_string_append_printf(log, "cpu=%u, %s=%016" PRIx64 ", size=%ld\n", cpu_index, reg_name, v, size);
    }
    else {
        g_string_append_printf(log, "register %s not found\n", reg_name);
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
