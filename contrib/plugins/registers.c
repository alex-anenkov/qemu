/*
 * TODO: header
 */
#include <glib.h>
#include <inttypes.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

/* Print log to file every N instructions */
#define LOG_BUF_N_INSN 1000000

const char * const REGS[] = { "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
                              "rbp", "rsp", "eflags" };

typedef struct reg_data {
    const char *name;
    size_t idx;
} reg_data;

/*
 * Each vcpu has its own independent data set, which is only initialized once
 */
typedef struct vcpu_cache {
    GList *reglist;
    GString *log;
    size_t print_counter;
    unsigned int vcpu_index;
} vcpu_cache;

vcpu_cache *caches = NULL;

/*
 * Convert registers names into indices of these registers
 * and return them as a list
 */
static GList *init_reg_idx_list(void)
{
    GList *list;
    size_t arr_size, reg_idx, i;

    arr_size = sizeof(REGS) / sizeof(REGS[0]);
    list = g_list_alloc();
    for (i = 0; i < arr_size; i++) {
        if (qemu_plugin_find_reg(REGS[i], &reg_idx)) {
            reg_data *data = g_new0(reg_data, 1);
            data->name = REGS[i];
            data->idx = reg_idx;
            list = g_list_append(list, data);
        }
    }

    return list;
}

static void print_register_to_log(gpointer data, gpointer user_data)
{
    if (data == NULL)
        return;

    vcpu_cache *cache = (vcpu_cache *)user_data;
    reg_data *reg = (reg_data *)data;

    size_t size;
    uint64_t *value = qemu_plugin_read_reg(reg->idx, &size);
    g_string_append_printf(cache->log, "vcpu=%u, %s=", cache->vcpu_index, reg->name);

    if (size == 4) {
        g_string_append_printf(cache->log, "%08x", (uint32_t)*value);
    }
    else if (size == 8) {
        g_string_append_printf(cache->log, "%016" PRIx64, *value);
    }
    else {
        // unknown register size
        g_assert_not_reached();
    }

    g_string_append_printf(cache->log, ", size=%ld\n", size);

    g_free(value);
}

static void init_vcpu_cache(vcpu_cache *cache, unsigned int vcpu_index)
{
    if (cache->reglist == NULL) {
        cache->reglist = init_reg_idx_list();
    }

    if (cache->log == NULL) {
        cache->log = g_string_new("");
    }
    cache->vcpu_index = vcpu_index;
}

static void free_vcpu_cache(vcpu_cache *cache)
{
    g_list_free_full(cache->reglist, g_free);
    g_string_free(cache->log, true);
    cache->print_counter = 0;
}

/**
 * Log registers on instruction execution
 */
static void vcpu_insn_exec(unsigned int vcpu_index, void *udata)
{
    init_vcpu_cache(&caches[vcpu_index], vcpu_index);

    g_list_foreach(caches[vcpu_index].reglist,
                   (GFunc)print_register_to_log, &caches[vcpu_index]);

    caches[vcpu_index].print_counter++;
    if (caches[vcpu_index].print_counter >= LOG_BUF_N_INSN) {
        qemu_plugin_outs(caches[vcpu_index].log->str);
        g_string_erase(caches[vcpu_index].log, 0, caches[vcpu_index].log->len);
        caches[vcpu_index].print_counter = 0;
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
            qemu_plugin_outs(caches[i].log->str);
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
