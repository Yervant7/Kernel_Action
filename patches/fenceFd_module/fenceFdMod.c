#include <linux/dma-fence.h>
#include <linux/sync_file.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/mutex.h>  // Para proteger a lista

#define DEVICE_NAME "fence_comm"
#define CREATE_FENCE 0x1001  // Defina um número único para o ioctl CREATE_FENCE
#define SIGNAL_FENCE 0x1002  // Defina um número único para o ioctl SIGNAL_FENCE

static dev_t dev_num;
static struct cdev fence_cdev;
static struct class *fence_class;
static DEFINE_MUTEX(fence_list_lock);  // Proteção da lista

// Estrutura personalizada para fence
struct fence_mod {
    struct dma_fence fence;
    int fence_fd;
    spinlock_t lock;  // Adicionar spinlock
};

// Funções que retornam driver_name e timeline_name
static const char *get_driver_name(struct dma_fence *fence) {
    return "fence_mod_driver";
}

static const char *get_timeline_name(struct dma_fence *fence) {
    return "buffer_timeline";
}

static const struct dma_fence_ops fence_mod_ops = {
    .get_driver_name = get_driver_name,
    .get_timeline_name = get_timeline_name,
};

// Função para criar a fence
struct dma_fence *create_fence_mod(void) {
    struct fence_mod *fence = kzalloc(sizeof(*fence), GFP_KERNEL);
    if (!fence)
        return NULL;
    
    spin_lock_init(&fence->lock);  // Inicializar o spinlock
    dma_fence_init(&fence->fence, &fence_mod_ops, &fence->lock, 0, 0);
    return &fence->fence;
}

// Sinalizar fence como concluída
void signal_fence(struct dma_fence *fence) {
    dma_fence_signal(fence);
}

// Criar um arquivo de sincronização (fenceFd)
int create_sync_file(struct dma_fence *fence) {
    struct sync_file *sync_file;
    int fence_fd;

    sync_file = sync_file_create(fence);
    if (!sync_file)
        return -ENOMEM;

    fence_fd = get_unused_fd_flags(O_CLOEXEC);
    if (fence_fd < 0) {
        if (sync_file && sync_file->file)
            fput(sync_file->file);
        return fence_fd;
    }

    fd_install(fence_fd, sync_file->file);  // Instalar o arquivo no FD
    return fence_fd;
}

// Associar fence_fd a uma estrutura e gerenciar via uma tabela simples
struct fence_mod_list {
    struct list_head list;
    struct dma_fence *fence;
    int fence_fd;
};

static LIST_HEAD(fence_list);

// Função para encontrar uma fence_mod pela fence_fd
struct dma_fence *find_fence_by_fd(int fence_fd) {
    struct fence_mod_list *entry;

    mutex_lock(&fence_list_lock);  // Proteger a lista com mutex
    list_for_each_entry(entry, &fence_list, list) {
        if (entry->fence_fd == fence_fd) {
            mutex_unlock(&fence_list_lock);
            return entry->fence;
        }
    }
    mutex_unlock(&fence_list_lock);
    return NULL;
}

// Adicionar uma fence à lista
void add_fence_to_list(struct dma_fence *fence, int fence_fd) {
    struct fence_mod_list *entry = kmalloc(sizeof(*entry), GFP_KERNEL);
    if (entry) {
        entry->fence = fence;
        entry->fence_fd = fence_fd;

        mutex_lock(&fence_list_lock);  // Proteger a lista com mutex
        list_add(&entry->list, &fence_list);
        mutex_unlock(&fence_list_lock);
    }
}

// Remover uma fence da lista (opcional para limpeza)
void remove_fence_from_list(int fence_fd) {
    struct fence_mod_list *entry, *tmp;

    mutex_lock(&fence_list_lock);  // Proteger a lista com mutex
    list_for_each_entry_safe(entry, tmp, &fence_list, list) {
        if (entry->fence_fd == fence_fd) {
            list_del(&entry->list);
            kfree(entry);
            break;
        }
    }
    mutex_unlock(&fence_list_lock);
}

// Implementação da função ioctl
static long fence_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct dma_fence *fence;
    int fence_fd;

    switch (cmd) {
    case CREATE_FENCE:
        // Criar fence e retornar o FD
        fence = create_fence_mod();
        if (!fence)
            return -ENOMEM;

        fence_fd = create_sync_file(fence);
        if (fence_fd < 0)
            return fence_fd;

        // Adicionar a fence à lista de controle
        add_fence_to_list(fence, fence_fd);

        // Retornar o fence_fd para o userspace
        if (copy_to_user((int __user *)arg, &fence_fd, sizeof(fence_fd)))
            return -EFAULT;

        pr_info("Fence FD created: %d\n", fence_fd);
        break;

    case SIGNAL_FENCE:
        // Recuperar fence_fd do userspace
        if (copy_from_user(&fence_fd, (int __user *)arg, sizeof(fence_fd)))
            return -EFAULT;

        // Buscar a fence associada ao fence_fd
        fence = find_fence_by_fd(fence_fd);
        if (!fence) {
            pr_err("Fence FD %d not found\n", fence_fd);
            return -EINVAL;
        }

        // Sinalizar a fence como concluída
        signal_fence(fence);

        // Opcional: Remover da lista após sinalizar
        remove_fence_from_list(fence_fd);

        pr_info("Fence FD %d signaled\n", fence_fd);
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

// Operações de arquivo para o dispositivo
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fence_ioctl,
};

// Inicializar o dispositivo
static int __init fence_init(void) {
    int ret;

    // Alocar número de dispositivo
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }

    // Inicializar o cdev
    cdev_init(&fence_cdev, &fops);
    ret = cdev_add(&fence_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    // Criar classe e dispositivo no /dev
    fence_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(fence_class)) {
        pr_err("Failed to create class\n");
        cdev_del(&fence_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(fence_class);
    }

    device_create(fence_class, NULL, dev_num, NULL, DEVICE_NAME);
    pr_info("Fence communication device initialized\n");

    return 0;
}

static void __exit fence_exit(void) {
    device_destroy(fence_class, dev_num);
    class_destroy(fence_class);
    cdev_del(&fence_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("Fence communication device removed\n");
}

module_init(fence_init);
module_exit(fence_exit);

MODULE_AUTHOR("Linux");
MODULE_DESCRIPTION("Linux default module");
MODULE_LICENSE("GPL");
