#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>   
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#define BUFSIZE  6000
#define BUFF_SIZE 6000  
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/cpumask.h>
#include <linux/sched/topology.h>
#include <linux/types.h>
#include <linux/slab.h>  // Added for kmalloc and kfree

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Liran B.H");

static struct proc_dir_entry *get_info_ent;
static struct proc_dir_entry *capacity_ent;
static struct proc_dir_entry *av_capacity_ent;

static struct proc_dir_entry *topo_ent;
static struct proc_dir_entry *latency_ent;

extern void get_fine_stl_preempts(int cpunum, u64* preempt, u64* steal_time);
extern void reset_max_latency(u64 max_latency);
extern void get_max_latency(int cpunum, u64* max_latency);
extern void set_custom_capacity(unsigned long capacity, int cpu);
extern void set_avg_latency(unsigned long latency, int cpu);
extern void set_live_topology(struct sched_domain_topology_level *topology);
extern void set_average_capacity_all(int test);
extern int get_average_capacity_all;



extern cpumask_var_t cpu_l2c_shared_map;
extern void set_l2c_shared_mask(int cpu, struct cpumask new_mask);
extern void set_llc_shared_mask(int cpu, struct cpumask new_mask);
struct cpumask cpuset_array[NR_CPUS];
static int test_integer = 3;
EXPORT_SYMBOL(cpuset_array);

// Function declarations
static ssize_t blank_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t blank_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t get_info_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
ssize_t capacity_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset);
ssize_t latency_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset);
ssize_t topology_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset);
ssize_t vav_capacity_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset);



// Function implementations
static ssize_t blank_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos) 
{
    printk(KERN_DEBUG "write handler\n");
    return -1;
}

static ssize_t blank_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    printk(KERN_DEBUG "read handler\n");
    return -1;
}

void set_capacities(char *data) 
{
    char *token, *cur;
    char *data_copy;
    int index = 0;
    long number;

    data_copy = kstrdup(data, GFP_KERNEL);
    if (!data_copy)
        return;
            
    cur = data_copy;
    while ((token = strsep(&cur, ";")) != NULL) {
        if (*token == '\0')
            continue;
        if (kstrtol(token, 10, &number) == 0) {
            printk("Capacity:%lu Cpu%d:", (unsigned long)number, index);
            set_custom_capacity((unsigned long)number, index);
//            set_avg_latency(1, 1);
            index++;
        }
    }
    kfree(data_copy);
}

ssize_t capacity_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset) {
    char *procfs_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
    if (!procfs_buffer)
        return -ENOMEM;

    if (count > BUFF_SIZE) {
        count = BUFF_SIZE;
    }

    if (copy_from_user(procfs_buffer, buffer, count)) {
        kfree(procfs_buffer);
        return -EFAULT;
    }

    set_capacities(procfs_buffer);
    kfree(procfs_buffer);
    return count;
}


void set_av_capacity(char *data) {
    long capacity;
    if (kstrtol(data, 10, &capacity) == 0) {
        printk(KERN_INFO "Setting average capacity to: %ld\n", capacity);
        // Assuming set_avg_capacity is a function that sets the average capacity for all CPUs
        // If you need to set it for a specific CPU, you might need to modify this
            set_average_capacity_all((unsigned int)capacity);
    } else {
        printk(KERN_ERR "Invalid input for average capacity\n");
    }
}

ssize_t av_capacity_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset) {
    char *procfs_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
    if (!procfs_buffer)
        return -ENOMEM;

    if (count > BUFF_SIZE) {
        count = BUFF_SIZE;
    }

    if (copy_from_user(procfs_buffer, buffer, count)) {
        kfree(procfs_buffer);
        return -EFAULT;
    }
    procfs_buffer[count] = '\0';
    set_av_capacity(procfs_buffer);
    kfree(procfs_buffer);
    return count;
}

void set_latencies(char *data) {
    char *token, *cur;
    char *data_copy;
    int index = 0;
    long number;
    u64 i = 0;  // Initialize i to 0

    data_copy = kstrdup(data, GFP_KERNEL);
    if (!data_copy)
        return;

    reset_max_latency(0);
    cur = data_copy;
    while ((token = strsep(&cur, ";")) != NULL) {
        if (*token == '\0')
            continue;
        kstrtol(token, 10, &number);  // Changed base from 100 to 10
	printk("cpu at %d has latency: %llu",index,(unsigned long long) number);
        set_avg_latency(index, (unsigned long long)number);
        index++;
    }
    kfree(data_copy);
}

ssize_t latency_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset) {
    char *procfs_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
    if (!procfs_buffer)
        return -ENOMEM;

    if (count > BUFF_SIZE) {
        count = BUFF_SIZE;
    }

    if (copy_from_user(procfs_buffer, buffer, count)) {
        kfree(procfs_buffer);
        return -EFAULT;
    }
    set_latencies(procfs_buffer);
    kfree(procfs_buffer);
    return count;
}

void set_topology(char *data) {
    struct sched_domain_topology_level *topology = get_sched_topology();
    if (topology == NULL) {
        printk(KERN_WARNING "Failed to retrieve Scheduling Domain Topology.\n");
        return;
    }

    cpumask_t use_cpumask;
    cpumask_clear(&use_cpumask);
    int sched_domain = 0;
    int comp_cpu = 0;
    int cpu = 0;
    char currentChar = *data;


    int corr_index = 0;
    while (*data != '\0') {
        currentChar = *data;
        if (currentChar == ';') {
            if (sched_domain == 0) {
                cpumask_copy(&cpuset_array[cpu], &use_cpumask);
            }
            cpumask_copy(topology[sched_domain].mask(cpu), &use_cpumask);
            cpumask_clear(&use_cpumask);
            cpu++;
            comp_cpu = 0;
        } else if (currentChar == ':') {
            sched_domain++;
            comp_cpu = 0;
            cpu = 0;
        } else {
            if (currentChar == '1') {
                cpumask_set_cpu(comp_cpu, &use_cpumask);
            }
            comp_cpu++;
        }
        data++;
	corr_index++;
    }
    int num_cpus;
    set_live_topology(topology);
}

#define MAX_TOPOLOGY_LEVEL 3  // As per your userspace code (levels 3, 4, 5)

void set_topology_new(const char *data, size_t count) {
    struct sched_domain_topology_level *topology = get_sched_topology();
    if (topology == NULL) {
        printk(KERN_WARNING "Failed to retrieve Scheduling Domain Topology.\n");
        return;
    }

    int num_cpus = num_present_cpus();  // Get the actual number of CPUs in the system
    int sched_domain = 0;
    int cpu = 0;
    size_t bit_index = 0;
    cpumask_t use_cpumask;

    while (bit_index < count * 8 && sched_domain < MAX_TOPOLOGY_LEVEL) {
        cpumask_clear(&use_cpumask);

        // Read bits for current CPU
        for (int comp_cpu = 0; comp_cpu < num_cpus && bit_index < count * 8; comp_cpu++, bit_index++) {
            if (test_bit(bit_index, (unsigned long *)data)) {
                cpumask_set_cpu(comp_cpu, &use_cpumask);
            }
        }

        if (sched_domain == 0) {
            cpumask_copy(&cpuset_array[cpu], &use_cpumask);
        }
        cpumask_copy(topology[sched_domain].mask(cpu), &use_cpumask);

        cpu++;

        // If we've processed all CPUs for this level or reached end of data
        if (cpu == num_cpus || bit_index >= count * 8) {
            sched_domain++;
            cpu = 0;
        }
    }

    set_live_topology(topology);
}

ssize_t topology_write(struct file *file, const char __user *buffer, size_t count, loff_t *offset) {
//    char *procfs_buffer = kmalloc(BUFF_SIZE, GFP_KERNEL);
    char *procfs_buffer = kmalloc(count, GFP_KERNEL);
    if (!procfs_buffer)
        return -ENOMEM;

    //if (count > BUFF_SIZE) {
    //    count = BUFF_SIZE;
    //}

    if (copy_from_user(procfs_buffer, buffer, count)) {
        kfree(procfs_buffer);
        return -EFAULT;
    }

    set_topology_new(procfs_buffer,count);
    kfree(procfs_buffer);
    return count;
}




static ssize_t get_info_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos) 
{
    char buf[BUFSIZE];
    int len = 0;
    int cpu;
    u64 preempt, steal_time, max_latency;

    for_each_online_cpu(cpu) {
        get_fine_stl_preempts(cpu, &preempt, &steal_time);   
        get_max_latency(cpu, &max_latency);
        len += snprintf(buf + len, sizeof(buf) - len, "CPU %d:\n%llu\n%llu\n%llu\n", cpu, preempt, steal_time, max_latency);
        if (len >= sizeof(buf)) {
            len = sizeof(buf);
            break;
        }
    }

    if (*ppos > 0 || count < len)
        return 0;  

    if (copy_to_user(ubuf, buf, len))
        return -EFAULT;

    *ppos = len;

    return len;
}

static const struct proc_ops get_information_ops = {
    .proc_read = get_info_read,
    .proc_write = blank_write,
};

static const struct proc_ops cust_capacity_ops = {
    .proc_read = blank_read,
    .proc_write = capacity_write,
};


static const struct proc_ops cust_av_capacity_ops = {
    .proc_read = blank_read,
    .proc_write = av_capacity_write,
};


static const struct proc_ops cust_latency_ops = {
    .proc_read = blank_read,
    .proc_write = latency_write,
};

static const struct proc_ops cust_topo_ops = {
    .proc_read = blank_read,
    .proc_write = topology_write,  // Changed from blank_write to topology_write
};

static int vsched_init(void)
{
    get_info_ent = proc_create("vcap_info", 0666, NULL, &get_information_ops);
    capacity_ent = proc_create("vcapacity_write", 0660, NULL, &cust_capacity_ops);
    latency_ent = proc_create("vlatency_write", 0660, NULL, &cust_latency_ops);
    topo_ent = proc_create("vtopology_write", 0660, NULL, &cust_topo_ops);
    av_capacity_ent = proc_create("vav_capacity_write", 0660, NULL, &cust_av_capacity_ops);

    if (get_info_ent == NULL || capacity_ent == NULL || topo_ent == NULL || latency_ent == NULL|| av_capacity_ent == NULL) {
        proc_remove(get_info_ent);
        proc_remove(capacity_ent);
        proc_remove(latency_ent);
        proc_remove(topo_ent);

        printk(KERN_ALERT "Error: Could not successfully initialize vSched's kernel modules - check your kernel version\n");
        return -ENOMEM;
    }
    printk(KERN_ALERT "Successfully initialized vSched's Kernel modules\n");
    return 0;
}

static void vsched_cleanup(void)
{
    proc_remove(av_capacity_ent);
    proc_remove(get_info_ent);
    proc_remove(capacity_ent);
    proc_remove(latency_ent);
    proc_remove(topo_ent);
}

module_init(vsched_init);
module_exit(vsched_cleanup);
