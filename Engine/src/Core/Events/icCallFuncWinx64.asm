.CODE             ;Indicates the start of a code segment.

icCallFunc PROC x:QWORD, y:QWORD
	mov		rax, 0		; get return value in case callee doesn't
	call	rdx 
	ret 
icCallFunc ENDP 
END 
