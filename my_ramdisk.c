#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>

#define MY_DISK_NAME "my_ramdisk"
#define MY_DISK_SIZE (16 * 1024 * 1024)

static int major_num;
static struct gendisk *my_gd = NULL;
static char *disk_memory = NULL;

// Обработчик I/O
static void my_submit_bio(struct bio *bio) {
    struct bio_vec bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;

    bio_for_each_segment(bvec, bio, iter) {
        char *buffer = kmap_local_page(bvec.bv_page) + bvec.bv_offset;
        size_t len = bvec.bv_len;
        size_t offset = sector << SECTOR_SHIFT;

        if (bio_data_dir(bio) == WRITE)
            memcpy(disk_memory + offset, buffer, len);
        else
            memcpy(buffer, disk_memory + offset, len);

        kunmap_local(buffer);
        sector += (len >> SECTOR_SHIFT);
    }
    bio_endio(bio);
}

static const struct block_device_operations my_fops = {
    .owner = THIS_MODULE,
    .submit_bio = my_submit_bio,
};

// Функция инициализации
static int __init my_init(void) {
    struct queue_limits lim;
    int ret;

    printk(KERN_INFO "MyRAMDisk: Step 1 - Allocating memory\n");
    disk_memory = vmalloc(MY_DISK_SIZE);
    if (!disk_memory) return -ENOMEM;

    printk(KERN_INFO "MyRAMDisk: Step 2 - Registering blkdev\n");
    major_num = register_blkdev(0, MY_DISK_NAME);
    if (major_num <= 0) {
        vfree(disk_memory);
        return -EBUSY;
    }

    printk(KERN_INFO "MyRAMDisk: Step 3 - Setting limits\n");
    blk_set_stacking_limits(&lim);
    lim.logical_block_size = SECTOR_SIZE;
    lim.physical_block_size = SECTOR_SIZE;
    lim.io_min = SECTOR_SIZE;
    lim.max_hw_sectors = 1024;

    printk(KERN_INFO "MyRAMDisk: Step 4 - Allocating disk\n");
    my_gd = blk_alloc_disk(&lim, NUMA_NO_NODE);
    if (IS_ERR(my_gd)) {
        printk(KERN_ERR "MyRAMDisk: blk_alloc_disk failed\n");
        unregister_blkdev(major_num, MY_DISK_NAME);
        vfree(disk_memory);
        return PTR_ERR(my_gd);
    }

    my_gd->major = major_num;
    my_gd->first_minor = 0;
    my_gd->fops = &my_fops;
    my_gd->minors = 1;

    snprintf(my_gd->disk_name, 32, MY_DISK_NAME);
    set_capacity(my_gd, MY_DISK_SIZE >> SECTOR_SHIFT);

    printk(KERN_INFO "MyRAMDisk: Step 5 - Adding disk\n");
    ret = add_disk(my_gd);
    if (ret) {
        printk(KERN_ERR "MyRAMDisk: add_disk failed with %d\n", ret);
        put_disk(my_gd);
        unregister_blkdev(major_num, MY_DISK_NAME);
        vfree(disk_memory);
        return ret;
    }

    printk(KERN_INFO "MyRAMDisk: SUCCESS! Device live.\n");
    return 0;
}

// Функция выгрузки
static void __exit my_exit(void) {
    if (my_gd) {
        del_gendisk(my_gd);
        put_disk(my_gd);
    }
    unregister_blkdev(major_num, MY_DISK_NAME);
    vfree(disk_memory);
    printk(KERN_INFO "MyRAMDisk: Unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cleaned RAMDisk for 6.17");