#include "physmem.h"
#include "Util.h"


using namespace std;


struct wrap_efi_memmap g_EFI_memmap;

void * map_physical_memory(UINT64 address, UINT32 size) {

	#ifdef __linux__
		if (size > 0) {
			
			void *physmem_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_hDevice, address);
			if (physmem_addr) {
				debug_print("-> Successfully mapped physaddr %lx to %p\n", address, physmem_addr);								
			} else {
				debug_print("-> map_physical_memory error: failed to map %lx\n", address);	
			}

			return physmem_addr;
		}
		
	#elif _WIN32	
		
		NTSTATUS status = -1;
		DWORD bytesReturned = 0;
		
		MAP_PHYSICAL_MEM_CALL p = { 0 };		

		p.phys_address = address;

		void* user_addr = 0;
	
		status = DeviceIoControl(g_hDevice, IOCTL_MAP_PHYSICAL_MEM,
			&p, sizeof(MAP_PHYSICAL_MEM_CALL),
			&user_addr, sizeof(PVOID), &bytesReturned, NULL);		

		if (NT_SUCCESS(status)) {
			debug_print("-> Successfully mapped physaddr %llx to %p\n", address, user_addr);								
		} else {
			debug_print("-> map_physical_memory error: failed to map %llx\n", address);	
		}

		return (void *) user_addr;		
		

	#endif

	return NULL;
}

void unmap_physical_memory_unaligned(void *ptr, UINT32 size) {
	unmap_physical_memory((void *)((UINT64)ptr & PAGE_MASK), size);
}

void unmap_physical_memory(void *ptr, UINT32 size) {

	return;
	#ifdef __linux__
		munmap(ptr, size);		
	#elif _WIN32			

		NTSTATUS status = -1;
		DWORD bytesReturned = 0;
		
		UNMAP_PHYSICAL_MEM_CALL p = { 0 };		

		p.user_address = ptr;

		status = DeviceIoControl(g_hDevice, IOCTL_UNMAP_PHYSICAL_MEM,
			&p, sizeof(UNMAP_PHYSICAL_MEM_CALL),
			NULL, 0, &bytesReturned, NULL);		

		if (NT_SUCCESS(status)) {
			debug_print("-> Successfully unmapped %p\n", ptr);								
		}
		
	#endif
	
}


BOOL alloc_user_mem(UINT32 size, struct alloc_user_physmem *alloc_mem_arg) {
	#ifdef __linux__
		alloc_mem_arg->size = size;
		alloc_mem_arg->va = (UINT64) mmap(NULL, size, O_SYNC | O_RDWR,  MAP_SHARED|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);
		alloc_mem_arg->pa = 0;

		char pagemap[100] = {0};
		snprintf(pagemap, sizeof(pagemap), "/proc/%d/pagemap", getpid());

		UINT64 vpn_offset = (alloc_mem_arg->va >> 12) * 8;
		//cout << "looking for vpn-offset: " << std::hex << vpn_offset << "in pagemap: " << pagemap << endl;

		int fd = open(pagemap, O_RDONLY);    
		lseek(fd, vpn_offset, SEEK_SET);

		UINT64 val = 0;
		read(fd, &val, sizeof(UINT64));
		close(fd);

		if (!((val >> PAGE_PRESENT) & 1)) {
			printf("page of memory is not present?");
			return FALSE;
		}		
		
		if ((val >> PAGE_SWAPPED) & 1) {
			printf("page of memory has been swapped... lock it first?");
			return FALSE;
		}		
		
		alloc_mem_arg->pa = ((val & 0x3fffffffffffff) * PAGE_SIZE + ((uint64_t)alloc_mem_arg->va % PAGE_SIZE));  

		return TRUE;

	#elif _WIN32			

		NTSTATUS status = -1;
		DWORD bytesReturned = 0;

		struct alloc_user_physmem alloc_mem = {0};

		status = DeviceIoControl(g_hDevice, IOCTL_ALLOC_MEM,
			&size, sizeof(size),
			&alloc_mem, sizeof(alloc_mem), &bytesReturned, NULL);		

		if (NT_SUCCESS(status)) {
			debug_print("-> Successfully alloc %08x bytes of memory to va:[%016llx] - pa:[%016llx] \n",
				 size, alloc_mem.va, alloc_mem.pa);		

			if (alloc_mem_arg != NULL) {
				memcpy(alloc_mem_arg, &alloc_mem, sizeof(alloc_mem));
			}	

			return TRUE;					
		}

		return FALSE;
		
	#endif
}


void read_physical_memory(UINT64 address, UINT32 size, PVOID buffer, BOOL bPrint) {	


	#ifdef __linux__
		if (size > 0) {

			int i = 0;
			UINT32 auxSize = size;
			UINT64 cur_address;

			while (auxSize > PAGE_SIZE) {					
				cur_address = address + (i * PAGE_SIZE);
						
				void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, g_hDevice, cur_address);
				
				if (physmem_addr != MAP_FAILED) {
					debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);			
					if (buffer) {
						memcpy((char *)buffer + (i * PAGE_SIZE), physmem_addr, PAGE_SIZE);
					}
					if (bPrint) {
						print_memory(cur_address, (char *)physmem_addr, PAGE_SIZE);
					}
					munmap(physmem_addr, PAGE_SIZE);
				} 
				
				auxSize = auxSize - PAGE_SIZE;
				i++;
			}

			cur_address = address + (i * PAGE_SIZE);
			
			void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, g_hDevice, cur_address);

			if (physmem_addr != MAP_FAILED) {
				debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);			
				if (buffer) {
					memcpy((char *)buffer + (i * PAGE_SIZE), physmem_addr, auxSize);
				}

				if (bPrint) {
					print_memory(cur_address, (char *)physmem_addr, auxSize);
				}

				munmap(physmem_addr, PAGE_SIZE);
			} 
		}
		
	#elif _WIN32	

		NTSTATUS status = -1;
		DWORD bytesReturned = 0;
		if (size > 0) {
			READ_PHYSICAL_MEM_CALL p = { 0 };		

			PVOID mem = calloc(1, PAGE_SIZE);
			if (!mem) {
				debug_print("-> Memory allocation failed!\n");
				return;
			}

			int i = 0;
			UINT32 auxSize = size;
			while (auxSize > PAGE_SIZE) {					
				p.address = address + (i * PAGE_SIZE);
		
				status = DeviceIoControl(g_hDevice, IOCTL_READ_PHYSICAL_MEM,
					&p, sizeof(READ_PHYSICAL_MEM_CALL),
					mem, PAGE_SIZE, &bytesReturned, NULL);		

				if (NT_SUCCESS(status)) {
					debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, p.address);			
					if (buffer) {
						memcpy((char *)buffer + (i * PAGE_SIZE), mem, PAGE_SIZE);
					}
				}

				if (bPrint) {
					print_memory(p.address, (char *)mem, PAGE_SIZE);
				}
				auxSize = auxSize - PAGE_SIZE;
				i++;
			}
			
			p.address = address + (i * PAGE_SIZE);
			
			status = DeviceIoControl(g_hDevice, IOCTL_READ_PHYSICAL_MEM,
				&p, sizeof(READ_PHYSICAL_MEM_CALL),
				mem, auxSize, &bytesReturned, NULL);
	

			if (NT_SUCCESS(status)) {
				debug_print("-> %08x bytes read from physical Memory %p\n", auxSize, p.address);
				if (bPrint) {
					print_memory(p.address, (char *)mem, auxSize);
				}
				if (buffer) {
					memcpy((char *)buffer + (i * PAGE_SIZE), mem, auxSize);
				}
			}					

			free(mem);
		}
	#endif
}

void read_physical_memory_into_file(UINT64 address, UINT32 size, const char *filename) {

	FILE *f = fopen(filename, "wb");

	#ifdef __linux__
		if (size > 0) {

			int i = 0;
			UINT32 auxSize = size;
			UINT64 cur_address;

			while (auxSize > PAGE_SIZE) {					
				cur_address = address + (i * PAGE_SIZE);
						
				void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, g_hDevice, cur_address);
				
				if (physmem_addr != MAP_FAILED) {
					debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);			
					fwrite(physmem_addr, PAGE_SIZE, 1, f);
					munmap(physmem_addr, PAGE_SIZE);
				} 
				
				auxSize = auxSize - PAGE_SIZE;
				i++;				
			}

			cur_address = address + (i * PAGE_SIZE);
			
			void *physmem_addr = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, g_hDevice, cur_address);

			if (physmem_addr != MAP_FAILED) {
				debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, cur_address);			
				fwrite(physmem_addr, auxSize, 1, f);
				munmap(physmem_addr, PAGE_SIZE);
			} 

			
		}
		
	#elif _WIN32	

		DWORD bytesReturned = 0;
		NTSTATUS status = -1;

		if (size > 0) {
			READ_PHYSICAL_MEM_CALL p = { 0 };

			PVOID mem = calloc(1, PAGE_SIZE);
			if (!mem) {
				debug_print("-> Memory allocation failed!\n");
				return;
			}

			int i = 0;
			UINT32 auxSize = size;
			while (auxSize > PAGE_SIZE) {
				p.address = address + (i * PAGE_SIZE);
			
				status = DeviceIoControl(g_hDevice, IOCTL_READ_PHYSICAL_MEM,
					&p, sizeof(READ_PHYSICAL_MEM_CALL),
					mem, PAGE_SIZE, &bytesReturned, NULL);
			
				if (NT_SUCCESS(status)) {
					debug_print("-> %08x bytes read from physical memory %p\n", PAGE_SIZE, p.address);
					fwrite(mem, PAGE_SIZE, 1, f);
				}		
				auxSize = auxSize - PAGE_SIZE;
				i++;
			}

			p.address = address + (i * PAGE_SIZE);

			
			status = DeviceIoControl(g_hDevice, IOCTL_READ_PHYSICAL_MEM,
				&p, sizeof(READ_PHYSICAL_MEM_CALL),
				mem, auxSize, &bytesReturned, NULL);
	

			if (NT_SUCCESS(status)) {
				debug_print("-> %08x bytes read from physical Memory %p\n", auxSize, p.address);
				fwrite(mem, auxSize, 1, f);

				debug_print("-> Successfully written to %s\n", filename);
			}

			free(mem);
		}
	#endif

	fclose(f);
}

void write_byte_physical_memory(UINT64 address, UINT32 value) {
	WRITE_PHYSICAL_MEM_CALL p = { 0 };
	p.address = address;
	p.data[0] = value;
	p.size = 1;
	NTSTATUS status = -1;
	#ifdef __linux__
		printf("read_physical_memory unimplemented!\n");
		return;
	#elif _WIN32
		DWORD bytesReturned = 0;
		status = DeviceIoControl(g_hDevice, IOCTL_READ_PHYSICAL_MEM,
			&p, sizeof(READ_PHYSICAL_MEM_CALL),
			0, 0, &bytesReturned, NULL);
	#endif

	if (NT_SUCCESS(status)) {
		debug_print("-> 1 byte written to physical address %p\n", address);
	}
}





char efi_memory_type_name[][13] = {
	"Reserved",
	"Loader Code",
	"Loader Data",
	"Boot Code",
	"Boot Data",
	"Runtime Code",
	"Runtime Data",
	"Conventional",
	"Unusable",
	"ACPI Reclaim",
	"ACPI Mem NVS",
	"MMIO",
	"MMIO Port",
	"PAL Code",
	"Persistent",
};


int do_kernel_va_read(UINT64 va, UINT32 size, char *buff) {
	
	NTSTATUS status;

	#ifdef __linux__

		struct _kernel_read kread;

		if ((size & 7) != 0) {
			debug_print("- error: attempted to read kernel VA for an invalid size\n");
			return -1;
		}

		for (UINT32 offset = 0; offset < size; offset += sizeof(PVOID)) {
			kread.address = (void *) (va + offset);
			kread.value   = (void *) NULL;
			status = ioctl(g_hDevice, KERNETIX_ABR, &kread);
			if (status == 0) {
				// printf("Copying from: %016lx\n", kread.value);
				memcpy(buff + offset, &kread.value, sizeof(PVOID));
			} else {
				debug_print("- error: ioctl for reading kernel va failed\n");
			}
		}

	#elif _WIN32	
		READ_KMEM_IN kread = {0};
		kread.vaddr = va;
		kread.size  = size;
		DWORD bytesReturned = 0;
		status = DeviceIoControl(g_hDevice, IOCTL_READ_KMEM, &kread, sizeof(kread), buff, size, &bytesReturned, NULL);   

		if (status != STATUS_SUCCESS) {
			debug_print(" - error: ioctl for reading kernel va failed!\n");
			
		} 
	#endif

	return status;
}


int read_efi_memory_map() {

	NTSTATUS status = 0;

	#ifdef __linux__

		if (g_EFI_memmap.mm_desc_array != 0) {
			return 0;
		}

		memset(&g_EFI_memmap, 0x00, sizeof(g_EFI_memmap));
		
		status = -1;	
		status = ioctl(
			g_hDevice, 
			IOCTL_GET_EFI_MEMMAP_ADDRESS, 
			&g_EFI_memmap.kernel_vaddr
		);

		if (status == 0) {
			debug_print("-> EFI Memory Map at %016lx\n",
				 g_EFI_memmap.kernel_vaddr);
		} else {
			printf("- error retrieving EFI memmap kernel addr\n");
			return -1;
		}

		status = do_kernel_va_read(g_EFI_memmap.kernel_vaddr, 
			sizeof(struct efi_memory_map), (char *) &g_EFI_memmap.efi_memmap);

		if (status) {
			debug_print("- error reading efi_memory_map structure\n");
			return -1;
		}

		print_struct_efi_memory_map(&g_EFI_memmap.efi_memmap);
		

		UINT32 map_range = g_EFI_memmap.efi_memmap.map_end - g_EFI_memmap.efi_memmap.map;
		
		if( g_EFI_memmap.efi_memmap.desc_size != sizeof(efi_memory_desc_t)) {
			debug_print("error: sizeof of efi mem descriptor does not match\n");
			return -1;
		}
	
		if ((map_range / g_EFI_memmap.efi_memmap.desc_size) != g_EFI_memmap.efi_memmap.nr_map) {
			debug_print("error: nr_map does not match\n");
			return -1;
		}

		g_EFI_memmap.mm_desc_array = (efi_memory_desc_t *)
			calloc(g_EFI_memmap.efi_memmap.nr_map, sizeof(efi_memory_desc_t));
		
		efi_memory_desc_t *mm_desc  = g_EFI_memmap.mm_desc_array;
		UINT64         mem_desc_va  = g_EFI_memmap.efi_memmap.map;

		for (int i = 0; i < g_EFI_memmap.efi_memmap.nr_map; i++) {

			status = do_kernel_va_read(mem_desc_va, sizeof(efi_memory_desc_t), (char *) mm_desc);
			if (status) {				
				free(g_EFI_memmap.mm_desc_array);
				debug_print("- error reading efi_memory_desc_t structure\n");
				return -1;
			}

			mem_desc_va += g_EFI_memmap.efi_memmap.desc_size;
			mm_desc++;
		}

		return 0;
	#endif

	return -1;
}


#ifdef _WIN32
	#include "__win.h"
#elif __linux__
	#include "__linux.h"
#endif


void print_efi_memory_map() {
	

	#ifdef __linux__

		if (read_efi_memory_map() != 0) {
			printf("cannot retrieve efi memory map\n");
			return;
		}

		efi_memory_desc_t *mm_desc  = g_EFI_memmap.mm_desc_array;

		for (int i = 0; i < g_EFI_memmap.efi_memmap.nr_map; i++) {

			printf("mm va:[%016lx] - pa:[%016lx] - pages:[%d] - type:[%s]\n",
				mm_desc->virt_addr,
				mm_desc->phys_addr,
				mm_desc->num_pages,
				efi_memory_type_name[mm_desc->type]
			);

			mm_desc++;
		}

	#endif
}


void _map_pci_ecam() {

	// If ecam was mapped, do nothing
	if (g_pci_ecam.vaddr != 0) {
		return;
	}

	if (g_pci_ecam.physaddr == 0 || g_pci_ecam.size == 0) {
		printf("error when attempting to map PCI ECAM. PCIEXBAR / MMIOCfgBase NULL\n");
		return;
	}

	g_pci_ecam.vaddr = (DWORD64) map_physical_memory(g_pci_ecam.physaddr, g_pci_ecam.size);

}

void _unmap_pci_ecam() {

	if (g_pci_ecam.vaddr != 0) {
		unmap_physical_memory((void *) g_pci_ecam.vaddr, g_pci_ecam.size);
	}

	g_pci_ecam.vaddr = 0;
}




void * search_in_physical_memory(UINT64 start, UINT64 end, BYTE *needle, UINT needle_size) {
    if (end < start) {
        return NULL;
    }

    if ((start & 0xFFF) != 0  || (end & 0xFFF) != 0) {
        return NULL;
    }

    int n_pages = 0;

    if (start == end) {
        n_pages = 1;
    } else {
        n_pages = (end - start) >> 12;
    }

    for(int i = 0; i < n_pages; i++) {
        UINT64 addr = start + i * PAGE_SIZE;
        void *mapped_va = map_physical_memory(addr, PAGE_SIZE);

        debug_print("Searching for pattern at addr %016llx\n", addr);

        void *ptr = memmem(mapped_va, PAGE_SIZE, needle, needle_size);
        if (ptr != NULL) {
            debug_print("Found pattern at physical address: %016llx -> va: %p\n", addr, ptr);
            return ptr;
        }

        unmap_physical_memory(mapped_va, PAGE_SIZE);
    }

    return NULL;
}


#ifdef _WIN32

UINT32 get_highest_physical_range_below_4G() {
	/*
		This function returns the base address of the highest physical range 
		that is usable by the OS below the 4 Gigabytes boundary.
	*/

	NTSTATUS status = -1;
    DWORD dwReturnedBytes = 0;
    UINT32 requiredSize   = 0;
    status = DeviceIoControl(g_hDevice, IOCTL_GET_PHYSICAL_RANGES,
         NULL, 0, &requiredSize, sizeof(UINT32), &dwReturnedBytes, NULL);
    
    // if (NTSTATUS(status) != STATUS_SUCCESS) {
    //     printf("error in IOCTL_GET_PHYSICAL_RANGES: %08x - %08x\n", status, dwReturnedBytes);
    //     printf("requiredSize: %08x\n", requiredSize);
    //     return;
    // }

    PHYSICAL_MEMORY_RANGE *pRanges = (PHYSICAL_MEMORY_RANGE *) calloc(1, requiredSize * 2);

    status = DeviceIoControl(g_hDevice, IOCTL_GET_PHYSICAL_RANGES,
         NULL, 0, pRanges, requiredSize * 2, &dwReturnedBytes, NULL);

    UINT32 ranges_count = dwReturnedBytes / sizeof(PHYSICAL_MEMORY_RANGE);

	UINT32 addr_res = 0;

    for (int i = 0; i < ranges_count; i++) {
        //printf("BaseAddress: %016llx\n", pRanges[i].BaseAddress);
        //printf("  PageCount: %08x\n", (pRanges[i].NumberOfBytes >> 12) + 1);
		if (pRanges[i].BaseAddress < 0xFFFFFFFF && pRanges[i].BaseAddress > addr_res) {
			addr_res = pRanges[i].BaseAddress;
		}
    }

    free(pRanges);

	return addr_res;
}

#endif


UINT64 virt_to_phys(UINT64 va) {	

	UINT64 result = 0;
	#ifdef _WIN32
		NTSTATUS status = -1;

		UINT64 pa = 0;
		DWORD bytesReturned = 0;
		status = DeviceIoControl(
			g_hDevice, 
			IOCTL_GET_PHYSICAL_ADDRESS,
			&va,
			sizeof(va),
			&pa,
			sizeof(pa),
			&bytesReturned,
			NULL
		);

		if (NT_SUCCESS(status)) {
			result = pa;
		} else {
			debug_print("error obtaining physical address for %016llx\n", va);
		}

	
	#elif __linux__
		int status;
		struct virt_to_phys v2p = {
			.vaddr 		= va & PAGE_MASK,
			.physaddr 	= 0,
		};

		status = ioctl(g_hDevice, IOCTL_VIRT_TO_PHYS, &v2p); 

		if (status != 0) {
			debug_print("error obtaining physical address for %016llx\n", va);				
		} else {
			result = v2p.physaddr;
		}
	#endif

	return result;
}