/*
Note: the functions in this file were adapted from this wonderful repository:

https://github.com/cirosantilli/linux-kernel-module-cheat/blob/873737bd1fc6e5ee0378f21e9df1c52e3f61e3fb/userland/virt_to_phys_user.c
*/


#define _GNU_SOURCE

#include <fcntl.h> /* open */
#include <math.h> /* fabs */
#include <stdint.h> /* uint64_t  */
#include <stdlib.h> /* size_t */
#include <stdio.h> /* snprintf */
#include <sys/types.h>
#include <unistd.h> /* pread, sysconf */
#include <stdbool.h>

#ifndef ADDRESS_TRANSLATION_H
#define ADDRESS_TRANSLATION_H

/* Format documented at:
 * https://github.com/torvalds/linux/blob/v4.9/Documentation/vm/pagemap.txt
 */
typedef struct {
	uint64_t pfn : 54;
	unsigned int soft_dirty : 1;
	unsigned int file_page : 1;
	unsigned int swapped : 1;
	unsigned int present : 1;
} PagemapEntry;

/* Parse the pagemap entry for the given virtual address.
 *
 * @param[out] entry      the parsed entry
 * @param[in]  pagemap_fd file descriptor to an open /proc/pid/pagemap file
 * @param[in]  vaddr      virtual address to get entry for
 * @return                0 for success, 1 for failure
 */
int pagemap_get_entry(PagemapEntry *entry, int pagemap_fd, uintptr_t vaddr)
{
	size_t nread;
	ssize_t ret;
	uint64_t data;
	uintptr_t vpn;

	vpn = vaddr / sysconf(_SC_PAGE_SIZE);
	nread = 0;
	while (nread < sizeof(data)) {
		ret = pread(
			pagemap_fd,
			&data,
			sizeof(data) - nread,
			vpn * sizeof(data) + nread
		);
		nread += ret;
		if (ret <= 0) {
			return 1;
		}
	}
	entry->pfn = data & (((uint64_t)1 << 54) - 1);
	entry->soft_dirty = (data >> 54) & 1;
	entry->file_page = (data >> 61) & 1;
	entry->swapped = (data >> 62) & 1;
	entry->present = (data >> 63) & 1;
	return 0;
}

/* Convert the given virtual address to physical using /proc/PID/pagemap.
 *
 * @param[out] paddr physical address
 * @param[in]  pid   process to convert for
 * @param[in]  vaddr virtual address to get entry for
 * @return           0 for success, 1 for failure
 */
int virt_to_phys_user(uintptr_t *paddr, pid_t pid, uintptr_t vaddr)
{
	char pagemap_file[BUFSIZ];
	int pagemap_fd;

	snprintf(pagemap_file, sizeof(pagemap_file), "/proc/%ju/pagemap", (uintmax_t)pid);
	pagemap_fd = open(pagemap_file, O_RDONLY);
	if (pagemap_fd < 0) {
		return 1;
	}
	PagemapEntry entry;
	if (pagemap_get_entry(&entry, pagemap_fd, vaddr)) {
		return 1;
	}
	close(pagemap_fd);
	*paddr = (entry.pfn * sysconf(_SC_PAGE_SIZE)) + (vaddr % sysconf(_SC_PAGE_SIZE));
	return 0;
}

#endif
