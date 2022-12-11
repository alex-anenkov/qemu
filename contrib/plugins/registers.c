/*
 * TODO: header
 */
#include <glib.h>
#include <inttypes.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/* Print report to file every N instructions */
#define REPORT_BUF_N_INSN 1000000

const char * const REGS[] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
                              "rbp", "rsp", "rip", "eflags" };

/*
 * Each vcpu has its own independent data set, which is only initialized once
 */
typedef struct vcpu_cache {
    struct qemu_plugin_reg_ctx *reg_ctx;
    GString *report;
    size_t report_counter;
} vcpu_cache;

vcpu_cache *caches = NULL;

static void print_register_value(GString *report, const void *data, size_t size)
{
    if (size == 4) {
        g_string_append_printf(report, "%08x", *(uint32_t *)data);
    }
    else if (size == 8) {
        g_string_append_printf(report, "%016" PRIx64, *(uint64_t *)data);
    }
    else {
        // unknown register size
        g_assert_not_reached();
    }
}

static void init_vcpu_cache(vcpu_cache *cache)
{
    if (cache->reg_ctx == NULL) {
        size_t size = sizeof(REGS) / sizeof(REGS[0]);
        cache->reg_ctx = qemu_plugin_reg_create_context(REGS, size);
        if (cache->reg_ctx == NULL) {
            qemu_plugin_outs("Failed to create context\n");
            exit(1);
        }
        cache->report = g_string_new("");
        cache->report_counter = 0;
    }
}

static void free_vcpu_cache(vcpu_cache *cache)
{
    if (cache == NULL)
        return;

    g_string_free(cache->report, true);
    qemu_plugin_reg_free_context(cache->reg_ctx);
}

/**
 * Log registers on instruction execution
 */
static void vcpu_insn_exec(unsigned int vcpu_index, void *udata)
{
    vcpu_cache *cache = &caches[vcpu_index];
    init_vcpu_cache(cache);

    qemu_plugin_regs_read_all(cache->reg_ctx);

    size_t i, n_regs = qemu_plugin_n_regs(cache->reg_ctx);
    for (i = 0; i < n_regs; i++) {
        const void *data = qemu_plugin_reg_get_ptr(cache->reg_ctx, i);
        size_t size = qemu_plugin_reg_get_size(cache->reg_ctx, i);
        const char *name = qemu_plugin_reg_get_name(cache->reg_ctx, i);
        g_string_append_printf(cache->report, "vcpu=%u, %s=", vcpu_index, name);
        print_register_value(cache->report, data, size);
        g_string_append_printf(cache->report, ", size=%ld\n", size);
    }

    cache->report_counter++;
    if (cache->report_counter >= REPORT_BUF_N_INSN) {
        qemu_plugin_outs(cache->report->str);
        g_string_erase(cache->report, 0, cache->report->len);
        cache->report_counter = 0;
    }
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
 * On plugin exit, print log in cache and free memory
 */
static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    if (caches != NULL) {
        int n_cpus = qemu_plugin_n_max_vcpus();
        int i;
        for (i = 0; i < n_cpus; i++) {
            qemu_plugin_outs(caches[i].report->str);
            free_vcpu_cache(&caches[i]);
        }
        g_free(caches);
    }
}

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    if (caches == NULL)
        caches = g_new0(vcpu_cache, qemu_plugin_n_max_vcpus());

    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
