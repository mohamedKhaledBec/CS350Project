#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/sched/signal>

#define PROC_NAME "sysmon"

static int sysmon_show(struct seq_file *m, void *v) {
    struct sysinfo i;
    si_meminfo(&i);
    unsigned long totalram = (i.totalram * i.mem_unit) / (1024 * 1024);
    unsigned long freeram = (i.freeram * i.mem_unit) / (1024 * 1024);

    seq_printf(m, "System Monitor Kernel Report:\n");
    seq_printf(m, "Total RAM: %lu MB\n", totalram);
    seq_printf(m, "Free RAM:  %lu MB\n", freeram);
    seq_printf(m, "Active Processes: %d\n", nr_threads);
    return 0;
}

static int sysmon_open(struct inode *inode, struct file *file) {
    return single_open(file, sysmon_show, NULL);
}

static ssize_t sysmon_write(struct file *file, const char __user *buffer,
                            size_t count, loff_t *f_pos) {
    char msg[128];
    if (count > sizeof(msg) - 1)
        count = sizeof(msg) - 1;

    if (copy_from_user(msg, buffer, count))
        return -EFAULT;

    msg[count] = '\0';
    pr_info("sysmon alert: %s\n", msg);
    return count;
}

static const struct proc_ops sysmon_ops = {
    .proc_open = sysmon_open,
    .proc_read = seq_read,
    .proc_write = sysmon_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init sysmon_init(void) {
    proc_create(PROC_NAME, 0666, NULL, &sysmon_ops);
    pr_info("sysmon: Module loaded.\n");
    return 0;
}

static void __exit sysmon_exit(void) {
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("sysmon: Module unloaded.\n");
}

module_init(sysmon_init);
module_exit(sysmon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohamed Khaled Becetti");
MODULE_DESCRIPTION("Kernel module for system monitoring alerts");
