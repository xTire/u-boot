#include <common.h>
#include <malloc.h>
#include <asm/dma-mapping.h>
#include <asm/arch/io.h>
#include <asm/arch/sdio.h>
#include <mmc.h>
#include <linux/err.h>
#include <emmc_partitions.h>

//extern struct partitions *part_table;
struct partitions * part_table = NULL;

struct mmc_config * mmc_config_of =NULL;
bool is_partition_checked = false;  //part_table

#define PARTITION_ELEMENT(na, sz, flags) {.name = na, .size = sz, .mask_flags = flags,}
struct partitions emmc_partition_table[]={
    PARTITION_ELEMENT(MMC_BOOT_NAME, MMC_BOOT_DEVICE_SIZE, STORE_CODE),
    PARTITION_ELEMENT(MMC_RESERVED_NAME, MMC_RESERVED_SIZE, 0),
    PARTITION_ELEMENT(MMC_CACHE_NAME, 0, 0),                    // the size and flag should be get from spl
    // PARTITION_ELEMENT(MMC_KEY_NAME, MMC_KEY_SIZE, 0),
    // PARTITION_ELEMENT(MMC_SECURE_NAME, MMC_SECURE_SIZE, 0),
    PARTITION_ELEMENT(MMC_ENV_NAME, MMC_ENV_SIZE, 0),
};


struct partitions partitions_emmc[]={
        {
            .name = "uboot",
            .offset = 0,
            .size = 2*1024*1024,
            .mask_flags = 1,
        },
        {
            .name = "ubootenv",
            .offset = 2*1024*1024,
            .size = 128*1024,
        },
        {
            .name = "dtvenv",
            .offset = 5*1024*1024,
            .size = 256*1024,
        },
        {
            .name = "logo",//4
            .offset = 8*1024*1024,
            .size = 8*1024*1024, // 14M
        },
        {
            .name = "recovery",
            .offset = 16*1024*1024,
            .size = 8*1024*1024,
        },
        {
            .name = "boot",
            .offset = 24*1024*1024,
            .size = 8*1024*1024,
        },
        {
            .name = "reserved",
            .offset = 36*1024*1024,
            .size = 2*1024*1024,
        },
        {
            .name = "system",
            .offset = 40*1024*1024,
            .size = 640*1024*1024,
        },
        {
            .name = "cache",
            .offset = 680*1024*1024,
            .size = 64*1024*1024,
        },
        {
            .name = "param",
            .offset = 744*1024*1024,
            .size = 16*1024*1024,
        },
        {
            .name = "dtv",
            .offset = 760*1024*1024,
            .size = 256*1024*1024,
        },
        {
            .name = "atv",
            .offset = 1016*1024*1024,
            .size = 192*1024*1024,
        },
        {
            .name = "dtvbackup",
            .offset = 1208*1024*1024,
            .size = 64*1024*1024,
        },
        {
            .name = "data",
            .offset = 1280*1024*1024,
            .size = (uint64_t)2400*1024*1024,
        },
};


void show_mmc_patition (struct partitions *part, int part_num)
{
    int i, cnt_stuff;

    printf("        name                        offset              size\n");
    printf("=================================================================\n");
	for (i=0; i < part_num ; i++) {
        printf("%4d: %s", i, part[i].name);
        cnt_stuff = sizeof(part[i].name) - strlen(part[i].name);
        if (cnt_stuff < 0) // something is wrong
            cnt_stuff = 0;
        cnt_stuff += 2;
        while (cnt_stuff--) {
            printf(" ");
        }
		printf("%18llx%18llx\n", part[i].offset, part[i].size);
		// printf("mmc_device->offset : %llx",mmc_config_of->partitions[i].offset);
		// printf("mmc_device->size : %llx",mmc_config_of->partitions[i].size);
	}
}

static struct partitions* find_partition_by_name (struct partitions *part_tbl, int part_num, char *name)
{
    int i;

	for (i=0; i < part_num; i++) {
        if (!part_tbl[i].name || (part_tbl[i].name[0] == '\0'))
            break;

	    if (!strncmp(part_tbl[i].name, name, MAX_MMC_PART_NAME_LEN)) {
            return &(part_tbl[i]);
        }
	}

    return NULL;
}

struct partitions* find_mmc_partition_by_name (char *name)
{
    struct partitions *p=NULL;

    p = find_partition_by_name(mmc_config_of->partitions, mmc_config_of->part_num, name);
    if (!p)
        printf("Can not find partition name \"%s\"\n", name);

    return p;
}

// set partition info according to what get from the spl
int set_partition_info (struct partitions *src_tbl, int src_part_num,
        struct partitions *dst_tbl, int dst_part_num, char *name)
{
    struct partitions *src=NULL;
    struct partitions *dst=NULL;

    src = find_partition_by_name(src_tbl, src_part_num, name);
    if (!src)
        return -1; // error

    dst = find_partition_by_name(dst_tbl, dst_part_num, name);
    if (!src)
        return -1; // error

    dst->size = src->size;
    dst->mask_flags = src->mask_flags;

    return 0; // OK
}

int mmc_get_partition_table (struct mmc *mmc)
{
	int i, part_num_left, resv_size, ret=0, part_num=0;
	struct partitions *part_ptr;

	mmc_config_of = kmalloc((sizeof(struct mmc_config)), 0);
	if(mmc_config_of == NULL){
		aml_mmc_dbg("malloc failed for mmc config!");
		ret = -1;
		goto exit_err;
	}

	memset(mmc_config_of,0x0,(sizeof(struct mmc_config)));
	part_ptr = mmc_config_of->partitions;

    set_partition_info(part_table, MAX_MMC_PART_NUM,
            emmc_partition_table, ARRAY_SIZE(emmc_partition_table), MMC_ENV_NAME);
    set_partition_info(part_table, MAX_MMC_PART_NUM,
            emmc_partition_table, ARRAY_SIZE(emmc_partition_table), MMC_CACHE_NAME);

	for (i=0; i < ARRAY_SIZE(emmc_partition_table); i++) {
        if((!strncmp(emmc_partition_table[i].name, MMC_BOOT_NAME, MAX_MMC_PART_NAME_LEN)) // eMMC boot partition
                && (!POR_EMMC_BOOT())) { // not eMMC boot, skip
            printf("Not emmc boot, POR_BOOT_VALUE=%d\n", POR_BOOT_VALUE);
            continue;
        }

		strncpy(part_ptr[part_num].name, emmc_partition_table[i].name, MAX_MMC_PART_NAME_LEN);
		part_ptr[part_num].size = emmc_partition_table[i].size;
        if (part_num == 0) { // first partition
            part_ptr[part_num].offset = 0;
        } else {
            if (!strncmp(part_ptr[part_num-1].name, MMC_BOOT_NAME, MAX_MMC_PART_NAME_LEN)) { // eMMC boot partition
                resv_size = MMC_BOOT_PARTITION_RESERVED;
            } else {
                resv_size = PARTITION_RESERVED;
            }
            part_ptr[part_num].offset = part_ptr[part_num-1].offset
                + part_ptr[part_num-1].size + resv_size;
        }
		part_num++;
    }

	// if(POR_EMMC_BOOT()){ // mmc boot
		// strncpy(part_ptr[part_num].name, MMC_BOOT_NAME, MAX_MMC_PART_NAME_LEN);
		// part_ptr[part_num].size = MMC_BOOT_DEVICE_SIZE;
                // part_ptr[part_num].offset = 0;
		// part_num++;
	// }

// #if 1
    // strncpy(part_ptr[part_num].name, MMC_RESERVED_NAME, MAX_MMC_PART_NAME_LEN);
    // part_ptr[part_num].size = MMC_RESERVED_SIZE;
	// if(part_num == 0){
		// part_ptr[part_num].offset = 0;
	// }else{
		// part_ptr[part_num].offset = part_ptr[part_num-1].offset
		 // + part_ptr[part_num-1].size + PARTITION_RESERVED;
	// }
    // part_num++;
// #endif

// #if 1 //CONFIG_MMC_KEY
    // strncpy(part_ptr[part_num].name, MMC_KEY_NAME, MAX_MMC_PART_NAME_LEN);
    // part_ptr[part_num].size = MMC_KEY_SIZE;
    // part_ptr[part_num].offset = part_ptr[part_num-1].offset
        // + part_ptr[part_num-1].size + PARTITION_RESERVED;
    // part_num++;
// #endif

// #if 1 //CONFIG_MMC_SECURE
    // strncpy(part_ptr[part_num].name, MMC_SECURE_NAME, MAX_MMC_PART_NAME_LEN);
    // part_ptr[part_num].size = MMC_SECURE_SIZE;
    // part_ptr[part_num].offset = part_ptr[part_num-1].offset
        // + part_ptr[part_num-1].size + PARTITION_RESERVED;
    // part_num++;
// #endif

    part_num_left = MAX_MMC_PART_NUM - part_num;
	for(i=0; i < part_num_left ; i++){
        if (!strncmp(part_table[i].name, MMC_ENV_NAME, MAX_MMC_PART_NAME_LEN)) { // skip env partition
            printf("[%s] skip %s partition.\n", __FUNCTION__, MMC_ENV_NAME);
            continue;
        }

        if (!strncmp(part_table[i].name, MMC_CACHE_NAME, MAX_MMC_PART_NAME_LEN)) { // get the info of cache partition
            printf("[%s] skip %s partition.\n", __FUNCTION__, MMC_CACHE_NAME);
            continue;
        }

		strncpy(part_ptr[part_num].name, part_table[i].name, MAX_MMC_PART_NAME_LEN);
		part_ptr[part_num].size = part_table[i].size;
        part_ptr[part_num].offset = part_ptr[part_num-1].offset
            + part_ptr[part_num-1].size + PARTITION_RESERVED;

        // printf("*****************%d********************************\n", part_num);
        // printf("mmc_device->name : %s\n", part_ptr[part_num].name);
        // printf("mmc_device->offset : %llx\n", part_ptr[part_num].offset);
        // printf("mmc_device->size : %llx\n", part_ptr[part_num].size);

		if ((part_table[i].size == -1) || ((part_ptr[part_num].offset + part_ptr[part_num].size) > mmc->capacity)) {
            part_ptr[part_num].size = mmc->capacity - part_ptr[part_num].offset;
            // printf("mmc->capacity=%llx, mmc_device->size=%llx\n", mmc->capacity, part_ptr[part_num].size);
			break;
        }
		part_num++;
	}
	strncpy(mmc_config_of->version, MMC_UBOOT_VERSION, MAX_MMC_PART_NAME_LEN);
	mmc_config_of->part_num = part_num + 1;
	mmc_config_of->private_data = mmc;

//    aml_mmc_msg("mmc_config_of->part_num %d",mmc_config_of->part_num);

	return ret;

exit_err:
	if(mmc_config_of){
		kfree(mmc_config_of);
		mmc_config_of = NULL;
        ret = -ENOMEM;
	}

	return ret;
}

int mmc_partition_tbl_checksum_calc (struct partitions *part, int part_num)
{
    int i, j;
	u32 checksum = 0, *p;

	for (i = 0; i < part_num; i++) {
		p = (u32*)part;
		for (j = sizeof(struct partitions)/sizeof(checksum); j > 0; j--) {
			checksum += *p;
			p++;
	    }
    }

	return checksum;
}

static struct partitions * find_emmc_partitions_info (char *name)
{
        int i;

        struct partitions *ptr = partitions_emmc;

        for(i=0; i < sizeof(partitions_emmc)/sizeof(partitions_emmc[0]); i++){
            if (!ptr[i].name || (ptr[i].name[0]=='\0') )
                break;

            if (!strncmp(ptr[i].name, name, MAX_MMC_PART_NAME_LEN)){
                return &(ptr[i]);
            }
        }

        //return &partitions_emmc[2];
        return NULL;
}

#if 0
static struct partitions* find_partition_by_name (struct partitions *part_tbl, int part_num, char *name)
{
    int i;
    struct partitions *src=NULL;
    struct partitions *dst=NULL;

	for (i=0; i < part_num; i++) {
        if (!part_tbl[i].name || (part_tbl[i].name[0] == '\0'))
            break;

	    if (!strncmp(part_tbl[i].name, name, MAX_MMC_PART_NAME_LEN)) {
            return &(part_tbl[i]);
        }
	}

    return NULL;
}

#endif

static void dump_partition(struct mmc_partitions_fmt *pt_fmt)
{
        int i = 0;
        struct partitions *pt = pt_fmt->partitions;

        printk("------------------------\n");
        printk("magic %s \n",pt_fmt->magic);
        printk("part_num %d \n",pt_fmt->part_num);
        printk("checksum %d \n",pt_fmt->checksum);
        printk("version %s \n",pt_fmt->version);

        for(i = 0;i < pt_fmt->part_num;i++){
            printk("name %s \n",pt->name);
            printk("offset %llu \n",pt->offset);
            printk("size %llu \n",pt->size);
            printk("mask_flags %d \n",pt->mask_flags);
            pt++;
        }
        printk("------------------------\n");
}


int mmc_write_partition_tbl (struct mmc *mmc, struct mmc_config *mmc_cfg, struct mmc_partitions_fmt *pt_fmt)
{
    int ret=0, start_blk, size, blk_cnt, i;
    char *buf, *src;
    struct partitions *pp;

    buf = kmalloc(mmc->read_bl_len, 0); // size of a block
    if(buf == NULL){
        aml_mmc_dbg("malloc failed for buffer!");
        ret = -ENOMEM;
        goto exit_err;
    }
    memset(pt_fmt, 0, sizeof(struct mmc_partitions_fmt));
    memset(buf, 0, mmc->read_bl_len);

    memcpy(pt_fmt->version, mmc_cfg->version, sizeof(pt_fmt->version));
    pt_fmt->part_num = mmc_cfg->part_num;
    memcpy(pt_fmt->partitions, mmc_cfg->partitions, MAX_MMC_PART_NUM*sizeof(mmc_cfg->partitions[0]));

    pt_fmt->checksum = mmc_partition_tbl_checksum_calc(pt_fmt->partitions, pt_fmt->part_num);
    strncpy(pt_fmt->magic, MMC_PARTITIONS_MAGIC, sizeof(pt_fmt->magic));

    pp = find_emmc_partitions_info(MMC_RESERVED_NAME);

    if (!pp) {
        ret = -1;
        goto exit_err;
    }
#ifdef TV_DUMP_MMC_INFO
    dump_partition(pt_fmt);
    //printf(" %d ret = %d \n",__LINE__,ret);
#endif
    start_blk = pp->offset/mmc->read_bl_len;
    size = sizeof(struct mmc_partitions_fmt);
    src = (char *)pt_fmt;
    if (size >= mmc->read_bl_len) {
        blk_cnt = size / mmc->read_bl_len;
        printf(" line %d mmc write lba=%#x, blocks=%#x\n",__LINE__, start_blk, blk_cnt);
        ret = mmc->block_dev.block_write(mmc->block_dev.dev, start_blk, blk_cnt, src);
        if (ret == 0) { // error
            ret = -1;
            goto exit_err;
        }

        start_blk += blk_cnt;
        src += blk_cnt * mmc->read_bl_len;
        size -= blk_cnt * mmc->read_bl_len;
    }

    //printf(" %d ret = %d \n",__LINE__,ret);

    if (size > 0) { // the last block
        memcpy(buf, src, size);
        // buf[mmc->read_bl_len - 2] = 0x55;
        // buf[mmc->read_bl_len - 1] = 0xaa;

        printf(" line %d mmc write lba=%#x, blocks=%#x\n",__LINE__, start_blk, blk_cnt);
        ret = mmc->block_dev.block_write(mmc->block_dev.dev, start_blk, 1, buf);
        if (ret == 0) { // error
            ret = -1;
            goto exit_err;
        }
    }

    //printf(" %d ret = %d \n",__LINE__,ret);
    ret = 0; // everything is OK now

exit_err:
    if(buf){
        kfree(buf);
    }

    printf("%s: mmc write partition %s!\n", __FUNCTION__, (ret==0)? "OK": "ERROR");

    return ret;
}



int mmc_read_partition_tbl (struct mmc *mmc, struct mmc_partitions_fmt *pt_fmt)
{
    int ret=0, start_blk, size, blk_cnt;
    char *buf, *dst;
	struct partitions *pp;

	buf = kmalloc(mmc->read_bl_len, 0); // size of a block
	if(buf == NULL){
		aml_mmc_dbg("malloc failed for buffer!");
        ret = -ENOMEM;
		goto exit_err;
	}
	memset(pt_fmt, 0, sizeof(struct mmc_partitions_fmt));
	memset(buf, 0, mmc->read_bl_len);

    pp = find_emmc_partitions_info(MMC_RESERVED_NAME);
    if (!pp) {
        ret = -1;
        goto exit_err;
    }
    start_blk = pp->offset/mmc->read_bl_len;
    size = sizeof(struct mmc_partitions_fmt);
    dst = (char *)pt_fmt;

    if (size >= mmc->read_bl_len) {
        blk_cnt = size / mmc->read_bl_len;
        printf(" line %d mmc read lba=%#x, blocks=%#x\n", __LINE__,start_blk, blk_cnt);
        ret = mmc->block_dev.block_read(mmc->block_dev.dev, start_blk, blk_cnt, dst);
        if (ret == 0) { // error
            ret = -1;
            goto exit_err;
        }

        start_blk += blk_cnt;
        dst += blk_cnt * mmc->read_bl_len;
        size -= blk_cnt * mmc->read_bl_len;
    }

    //printf(" %d ret = %d \n",__LINE__,ret);

    if (size > 0) { // the last block
        printf(" line %d mmc read lba=%#x, blocks=%#x\n",__LINE__, start_blk, blk_cnt);
        ret = mmc->block_dev.block_read(mmc->block_dev.dev, start_blk, 1, buf);
        if (ret == 0) { // error
            ret = -1;
            goto exit_err;
        }

        memcpy(dst, buf, size);
        // if ((buf[mmc->read_bl_len - 2] != 0x55) || (buf[mmc->read_bl_len - 1] != 0xaa)) { // error
            // ret = -1;
            // goto exit_err;
        // }
    }
    //printf(" %d ret = %d \n",__LINE__,ret);
	if ((strncmp(pt_fmt->magic, MMC_PARTITIONS_MAGIC, sizeof(pt_fmt->magic)) == 0) // the same
       && (pt_fmt->part_num > 0) && (pt_fmt->part_num <= MAX_MMC_PART_NUM)
       && (pt_fmt->checksum == mmc_partition_tbl_checksum_calc(pt_fmt->partitions, pt_fmt->part_num))) {
        ret = 0; // everything is OK now
	} else {
        ret = -1; // the partition infomation is invalid
    }

exit_err:
	if(buf){
		kfree(buf);
	}

	printf("%s: mmc read partition %s!\n", __FUNCTION__, (ret==0)? "OK": "ERROR");

    return ret;
}

int mmc_partition_verify (struct mmc_config * mmc_cfg, struct mmc_partitions_fmt *pt_fmt)
{
    int ret=0, i;
    struct partitions *pp1, *pp2;

    // printf("Partition table stored in eMMC/TSD: \n");
    // printf("magic: %s, version: %s, checksum=%#x\n",
            // pt_fmt->magic, pt_fmt->version, pt_fmt->checksum);
    // show_mmc_patition(pt_fmt->partitions, pt_fmt->part_num);

    // printf("Partition table get from SPL is : \n");
    // printf("version: %s\n", mmc_cfg->version);
    // show_mmc_patition(mmc_cfg->partitions, mmc_cfg->part_num);

    if ((strncmp(mmc_cfg->version, pt_fmt->version, sizeof(pt_fmt->version)) == 0x00)
            && (mmc_cfg->part_num == pt_fmt->part_num)) {  //pt_fmt_v->version, "01.00.00"
        pp1 = mmc_cfg->partitions;
        pp2 = pt_fmt->partitions;
        for (i = 0; i < pt_fmt->part_num; i++) {
            if ((pp1[i].size != pp2[i].size)
                    || (pp1[i].offset != pp2[i].offset)
                    || (strncmp(pp1[i].name, pp2[i].name, sizeof(pp1[i].name)) != 0x00)) {
                printf("%s: partition[%d] is different \n", __FUNCTION__, i);
                ret = -1;
                break;
            }
        }
    } else {
        printf("%s: version OR part_num is different!\n", __FUNCTION__);
        ret = -1;
    }

    return ret;
}

int mmc_device_init (struct mmc *mmc)
{
	int ret=0;
    struct mmc_partitions_fmt *pt_fmt;

	ret = mmc_get_partition_table(mmc);
    if (ret == 0) { // ok
        printf("Partition table get from SPL is : \n");
        show_mmc_patition(mmc_config_of->partitions, mmc_config_of->part_num);
    } else {
        printf("mmc get partition error!\n");
        return -1;
    }

    pt_fmt = kmalloc(sizeof(struct mmc_partitions_fmt), 0);
	if(pt_fmt == NULL){
		aml_mmc_dbg("malloc failed for struct mmc_partitions_fmt!");
		return -ENOMEM;
	}

    ret = mmc_read_partition_tbl(mmc, pt_fmt);
    if (ret == 0) { // ok
        ret = mmc_partition_verify(mmc_config_of, pt_fmt);
        if (ret == 0) { // ok
            printf("Partition table verified OK!\n");
        } else {
            printf("Partition table verified ERROR!\n"
                    "Following is the partition table stored in eMMC/TSD: \n");
            show_mmc_patition(pt_fmt->partitions, pt_fmt->part_num);
        }
    }

    if (ret != 0) { // error happen
        ret = mmc_write_partition_tbl(mmc, mmc_config_of, pt_fmt); // store mmc partition in tsd/emmc
    }

	if(pt_fmt){
		kfree(pt_fmt);
	}

	return ret;
}

int find_dev_num_by_partition_name (char *name)
{
    int port=-1, port_xc, dev_num;
    struct mmc *mmc;

    if (!strncmp(name, MMC_CARD_PARTITION_NAME, sizeof(MMC_CARD_PARTITION_NAME))) { // card
        port = SDIO_PORT_B;
        port_xc = SDIO_PORT_XC_B;
    } else { // eMMC OR TSD
        if (find_mmc_partition_by_name(name)) { // partition name is valid
            port = SDIO_PORT_C;
            port_xc = SDIO_PORT_XC_C;
        } // else port=-1
    }

    if (port > 0) {
        mmc = find_mmc_device_by_port(port);
        if (!mmc) { // not found yet
            mmc = find_mmc_device_by_port(port_xc);
        }

        if (!mmc) { // not found
            dev_num = -1;
        } else {
            dev_num = mmc->block_dev.dev;
        }
    } else { // partition name is invalid
        dev_num = -1;
    }
	// printf("[%s] dev_num = %d\n", __FUNCTION__, dev_num);

    return dev_num;
}


static int init_mmc_partion_tbl(struct mmc_partitions_fmt *pt_fmt_v)
{
    int i=0;
    strncpy(&pt_fmt_v->magic[0],"MPT",sizeof(pt_fmt_v->magic));
	strncpy(pt_fmt_v->version, "01.00.00", MAX_MMC_PART_NAME_LEN);
    pt_fmt_v->part_num = sizeof(partitions_emmc)/sizeof(partitions_emmc[i]);
    pt_fmt_v->checksum = 0x12;

    struct partitions *pt = &(pt_fmt_v->partitions[0]);

    //partitions_emmc[i]
    for(i = 0;i < pt_fmt_v->part_num;i++){
        strncpy(&(pt_fmt_v->partitions[i].name[0]),&(partitions_emmc[i].name[0]),MAX_PART_NAME_LEN);
        pt_fmt_v->partitions[i].offset = partitions_emmc[i].offset;
        pt_fmt_v->partitions[i].size= partitions_emmc[i].size;
        pt_fmt_v->partitions[i].mask_flags = 0x0;
    }
    return 0;
}

static int init_device_mmc_config_of(struct mmc_config * mmc_cfg)
{
    if(mmc_cfg == NULL)
        return 1;

    strncpy(mmc_cfg->version, "01.00.00", MAX_MMC_PART_NAME_LEN);
    mmc_cfg->part_num = sizeof(partitions_emmc)/sizeof(partitions_emmc[0]);
    memcpy(mmc_cfg->partitions,partitions_emmc, MAX_MMC_PART_NUM*sizeof(mmc_cfg->partitions[0]));

    return 0;
}


int mmc_device_partitions (struct mmc *mmc)
{
	int ret=0;
    struct mmc_partitions_fmt *pt_fmt;
    int part_num,i=0;

    pt_fmt = kmalloc(sizeof(struct mmc_partitions_fmt), 0);
	if(pt_fmt == NULL){
		aml_mmc_dbg("malloc failed for struct mmc_partitions_fmt!");
		return -ENOMEM;
	}

    mmc_config_of = kmalloc((sizeof(struct mmc_config)), 0);
	if(mmc_config_of == NULL){
		aml_mmc_dbg("malloc failed for mmc config!");
		ret = -1;
        goto error_exit1;
	}
	
    if(mmc->capacity < 0xe6000000) {
        part_num = sizeof(partitions_emmc)/sizeof(partitions_emmc[i]);

        for(i = 0;i < part_num;i++){
            if(strcmp(partitions_emmc[i].name,"data")==0)
				partitions_emmc[i].size=(uint64_t)1350*1024*1024;
        }
    }
	memset(mmc_config_of,0x0,(sizeof(struct mmc_config)));

    init_device_mmc_config_of(mmc_config_of);

    ret = mmc_read_partition_tbl(mmc, pt_fmt);
    if (ret == 0) { // ok
        ret = mmc_partition_verify(mmc_config_of, pt_fmt);
        if (ret == 0) { // ok
            printf("Partition table verified OK!\n");
			show_mmc_patition(pt_fmt->partitions, pt_fmt->part_num);
        } else {
            printf("Partition table verified ERROR!\n"
                    "Following is the partition table stored in eMMC/TSD: \n");
            show_mmc_patition(pt_fmt->partitions, pt_fmt->part_num);
        }
    }

    if (ret != 0) { // error happen
        init_mmc_partion_tbl(pt_fmt);
        ret = mmc_write_partition_tbl(mmc, mmc_config_of, pt_fmt); // store mmc partition in tsd/emmc
    }

error_exit1:

	if(pt_fmt){
		kfree(pt_fmt);
	}

	return ret;
}

