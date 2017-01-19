  # labwork.S
  # Written 2015 by F Lundevall
  # Skeleton file for IS1200/IS1500 lab 1 2015-2016.
  # The macros PUSH and POP are in the public domain.
  
  
  

.macro	PUSH reg
	addi	$sp,$sp,-4
	sw	\reg,0($sp)
.endm

.macro	POP reg
	lw	\reg,0($sp)
	addi	$sp,$sp,4
.endm

  # You can add your own code here
  #
  
  .data 
  .align 2

  .global hexasc
  .global delay
  .global time2string
  .global enable_interrupt
  .text
  
enable_interrupt:
	PUSH $ra
	ei
	POP $ra
	jr $ra
	nop
hexasc:

	andi $a0, $a0, 0xf	# keep 4 lsb
	move $t0, $a0		# store parameter argument in $t0
	slti $t1, $t0, 0xa  	# set $t1 to 1 if input is a number 0-9
	bne $t1, 0x1, letter	# jump to letter if input > 9
	addi $v0, $t0, 0x30	# set return value
	jr $ra			# jumps to return adress
	nop			# delay slot filler (just in case)
letter:
	addi $v0, $t0, 0x37	# set return value	
	jr $ra			# jumps to return adress
	nop			# delay slot filler (just in case)
	
delay:
	move $t1, $a0		# t1 stores input
				
outerloop:
	beq $t1, $0, done
	move $t2, $0		# set inner loop index
	addi $t1, $t1, -1
	
innerloop:
	
	slti $t3, $t2, 17000		# sets $t3 to 0 if loop index >= imm value
	beq $t3, $0, outerloop		# jump out of inner loop if $t3 == 0
	
	addi $t2, $t2, 1		# increment index
	
	j innerloop			# keep looping
	nop
	
done:
	jr $ra
	nop

time2string:
	move $t8, $a0		# $t8 stores input target memory location
	
	PUSH $ra	# save on stack
	PUSH $s1	
	PUSH $s2
	
	move $s1, $a1		# $s1 stores input time info
	
	# First digit
	
	andi $a0, $s1, 0xf000	# Get the first 4 bits out of 16 lsbs
	srl $a0, $a0, 12 	# Shift to lsbs
	jal hexasc		# Convert to ASCII
	nop			# delay slot filler (just in case)
	move $s2, $v0		# Save value in $s2, future sw source register
	
	# Second digit
	
	andi $a0, $s1, 0x0f00
	srl $a0, $a0, 8
	jal hexasc
	nop
	sll $v0, $v0, 8		# Shift 1st LSB to 2nd LSB
	add $s2, $s2, $v0	# add onto $s2
	
	# Colon
	li $t2, 0x3a		# ASCII colon
	sll $t2, $t2, 16	
	add $s2, $s2, $t2
	
	# Third digit
	
	andi $a0, $s1, 0x00f0
	srl $a0, $a0, 4
	jal hexasc
	nop
	sll $v0, $v0, 24	
	add $s2, $s2, $v0	# add values 
	
	# Store 4 ASCII Char (32bits) in memory 
	
	sw $s2, 0($t8)		
	
	# Fourth digit 
	
	andi $a0, $s1, 0x000f
	jal hexasc
	nop
	andi $v0, $v0, 0x00ff	# add null byte
	
	sw $v0, 4($t8)
	
	POP $s2	# restore register values
	POP $s1
	POP $ra
	jr $ra
	nop
