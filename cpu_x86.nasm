          BITS 32
          GLOBAL _Mod_PointInLeaf
          GLOBAL _SV_HullPointContents
          SECTION .text
_Mod_PointInLeaf

;{

        mov     eax, dword [esp+12-4] ; model
	sub	esp, 4
	push	ebx
	push	esi

;       mnode_t         *node;
;       node = model->nodes;

        mov     esi, dword [eax+200] ; model->nodes

;       if (node->contents < 0)

        cmp     dword [esi], 0 ; node->contents
        jge     .firstvalid

;               return (mleaf_t *)node;

	mov	eax, esi
	pop	esi
	pop	ebx
	add	esp, 4
	ret	0
.firstvalid
        mov     edx, dword [esp+8+8] ; p
.loop

;       while (1)

        xor     ecx, ecx
        mov     eax, dword [esi+76] ; node->plane
        mov     cl, byte [eax+16] ; node->plane->type

;       {
;               node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct (p,node->plane->normal)) < node->plane->dist];

	cmp	cl, 3
        jb      .axisplane
        fld     dword [eax+4] ; node->plane->normal[1]
        fmul    dword [edx+4] ; p[1]
        fld     dword [eax+8] ; node->plane->normal[2]
        fmul    dword [edx+8] ; p[2]
        fld     dword [eax]   ; node->plane->normal[0]
        fmul    dword [edx]   ; p[0]
        faddp   st1, st0
        faddp   st1, st0
        fld     dword [eax+12] ; node->plane->dist
        fcompp
	fnstsw	ax
	test	ah, 65					; 00000041H
	sete	cl
        mov     esi, dword [esi+ecx*4+80] ; node = node->children[condition]

;               if (node->contents < 0)

        cmp     dword [esi], 0
        jge     .loop

;                       return (mleaf_t *)node;

	mov	eax, esi

;       }
;       return NULL;    // never reached
;}

	pop	esi
	pop	ebx
	add	esp, 4
	ret	0
.axisplane:
        fld     dword [edx+ecx*4]
        fld     dword [eax+12]
        fcompp
        fnstsw  ax
	test	ah, 65					; 00000041H
	sete	cl
        mov     esi, dword [esi+ecx*4+80] ; node = node->children[condition]

;               if (node->contents < 0)

        cmp     dword [esi], 0
        jge     .loop

;                       return (mleaf_t *)node;

	mov	eax, esi

;       }
;       return NULL;    // never reached
;}

	pop	esi
	pop	ebx
	add	esp, 4
	ret	0


_SV_HullPointContents

;{
        mov     ecx, [esp+12-4] ; num
	sub	esp, 4
	test	ecx, ecx
        nop                     ; padding
	push	ebx
	push	esi
	push	edi
	push	ebp

;       while (num >= 0)

        jge     .firstvalid
;       return num;
	mov	eax, ecx
	pop	ebp
;}
	pop	edi
	pop	esi
	pop	ebx
	add	esp, 4
	ret	0
.firstvalid
        mov     eax, [esp+8+16] ; hull
        mov     edx, [esp+16+16] ; p
        mov     esi, [eax]
        mov     edi, [eax+4]
.loop
        mov     eax, [esi+ecx*8]
        lea     ebx, [eax+eax*2]
        xor     eax, eax
        mov     al, [edi+ebx*8+16]
        lea     ebp, [edi+ebx*8]

;               num = hull->clipnodes[num].children[(hull->planes[hull->clipnodes[num].planenum].type < 3 ? p[hull->planes[hull->clipnodes[num].planenum].type] : DotProduct (hull->planes[hull->clipnodes[num].planenum].normal, p)) < hull->planes[hull->clipnodes[num].planenum].dist];

	cmp	al, 3
        jb      .axisplane
        fld     dword [edx+8]
        fmul    dword [ebp+8]
        fld     dword [edx+4]
        fmul    dword [ebp+4]
        fld     dword [edx]
        fmul    dword [ebp]
        faddp   st1, st0
        faddp   st1, st0
        fstp    dword [esp-4+20]

        fld     dword [ebp+12]
        fcomp   dword [esp-4+20]
	xor	ebx, ebx
	fnstsw	ax
	test	ah, 65					; 00000041H
	sete	bl
        lea     eax, [ebx+ecx*4]
        movsx   ecx, word [esi+eax*2+4]
	test	ecx, ecx
        jge     .loop
;       return num;
	mov	eax, ecx
	pop	ebp
;}
	pop	edi
	pop	esi
	pop	ebx
	add	esp, 4
	ret	0

.axisplane
        mov     eax, [edx+eax*4]
        mov     [esp-4+20], eax

        fld     dword [ebp+12]
        fcomp   dword [esp-4+20]
	xor	ebx, ebx
	fnstsw	ax
	test	ah, 65					; 00000041H
	sete	bl
        lea     eax, [ebx+ecx*4]
        movsx   ecx, word [esi+eax*2+4]
	test	ecx, ecx
        jge     .loop
;       return num;
	mov	eax, ecx
	pop	ebp
;}
	pop	edi
	pop	esi
	pop	ebx
	add	esp, 4
	ret	0
