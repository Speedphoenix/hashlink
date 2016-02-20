#include <hl.h>

hl_type hlt_array = { HARRAY };
hl_type hlt_bytes = { HBYTES };
hl_type hlt_dynobj = { HDYNOBJ };
hl_type hlt_dyn = { HDYN };
hl_type hlt_i32 = { HI32 };

static const uchar *TSTR[] = {
	USTR("void"), USTR("i8"), USTR("i16"), USTR("i32"), USTR("f32"), USTR("f64"),
	USTR("bool"), USTR("bytes"), USTR("dynamic"), NULL, NULL, 
	USTR("array"), USTR("type"), NULL, NULL, USTR("dynobj"), 
	NULL, NULL, NULL
};


int hl_type_size( hl_type *t ) {
	static int SIZES[] = {
		0, // VOID
		1, // I8
		2, // I16
		4, // I32
		4, // F32
		8, // F64
		1, // BOOL
		HL_WSIZE, // BYTES
		HL_WSIZE, // DYN
		HL_WSIZE, // FUN
		HL_WSIZE, // OBJ
		HL_WSIZE, // ARRAY
		HL_WSIZE, // TYPE
		HL_WSIZE, // REF
		HL_WSIZE, // VIRTUAL
		HL_WSIZE, // DYNOBJ
		HL_WSIZE, // ABSTRACT
		HL_WSIZE, // ENUM
		HL_WSIZE, // NULL
	};
	return SIZES[t->kind];
}

int hl_pad_size( int pos, hl_type *t ) {
	int sz = hl_type_size(t);
	int align;
	align = pos & (sz - 1);
	if( align && t->kind != HVOID )
		return sz - align;
	return 0;
}

bool hl_same_type( hl_type *a, hl_type *b ) {
	if( a == b )
		return true;
	if( a->kind != b->kind )
		return false;
	switch( a->kind ) {
	case HVOID:
	case HI8:
	case HI16:
	case HI32:
	case HF32:
	case HF64:
	case HBOOL:
	case HTYPE:
	case HBYTES:
	case HDYN:
	case HARRAY:
	case HDYNOBJ:
		return true;
	case HREF:
	case HNULL:
		return hl_same_type(a->tparam, b->tparam);
	case HFUN:
		{
			int i;
			if( a->fun->nargs != b->fun->nargs )
				return false;
			for(i=0;i<a->fun->nargs;i++)
				if( !hl_same_type(a->fun->args[i],b->fun->args[i]) )
					return false;
			return hl_same_type(a->fun->ret, b->fun->ret);
		}
	case HOBJ:
		return a->obj == b->obj;
	case HVIRTUAL:
		return a->virt == b->virt;
	case HABSTRACT:
		return a->abs_name == b->abs_name;
	case HENUM:
		return a->tenum == b->tenum;
	default:
		break;
	}
	return false;
}

bool hl_is_dynamic( hl_type *t ) {
	static bool T_IS_DYNAMIC[] = {
		false, // HVOID,
		false, // HI8
		false, // HI16
		false, // HI32
		false, // HF32
		false, // HF64
		false, // HBOOL
		false, // HBYTES
		true, // HDYN
		true, // HFUN
		true, // HOBJ
		true, // HARRAY
		false, // HTYPE
		false, // HREF
		true, // HVIRTUAL
		true, // HDYNOBJ
		false, // HABSTRACT
		false, // HENUM
		true, // HNULL
	};
	return T_IS_DYNAMIC[t->kind];
}

bool hl_safe_cast( hl_type *t, hl_type *to ) {
	if( t == to )
		return true;
	if( to->kind == HDYN && t->kind != HVIRTUAL )
		return hl_is_dynamic(t);
	if( t->kind != to->kind )
		return false;
	switch( t->kind ) {
	case HVIRTUAL:
		if( to->virt->nfields < t->virt->nfields ) {
			int i;
			for(i=0;i<to->virt->nfields;i++) {
				hl_obj_field *f1 = t->virt->fields + i;
				hl_obj_field *f2 = to->virt->fields + i;
				if( f1->hashed_name != f2->hashed_name || !hl_same_type(f1->t,f2->t) )
					break;
			}
			if( i == to->virt->nfields )
				return true;
		}
		break;
	case HOBJ:
		{
			hl_type_obj *o = t->obj;
			hl_type_obj *oto = to->obj;
			while( true ) {
				if( o == oto ) return true;
				if( o->super == NULL ) return false;
				o = o->super->obj;
			}
		}
	case HFUN:
		if( t->fun->nargs == to->fun->nargs ) {
			int i;
			if( !hl_safe_cast(t->fun->ret,to->fun->ret) )
				return false;
			for(i=0;i<t->fun->nargs;i++) {
				hl_type *t1 = t->fun->args[i];
				hl_type *t2 = to->fun->args[i];
				if( !hl_safe_cast(t1,t2) && (t1->kind != HDYN || !hl_is_dynamic(t2)) )
					return false;
			}
			return true;
		}
		break;
	}
	return hl_same_type(t,to);
}

static void hl_type_str_rec( hl_buffer *b, hl_type *t ) {
	const uchar *c = TSTR[t->kind];
	int i;
	if( c != NULL ) {
		hl_buffer_str(b,c);
		return;
	}
	switch( t->kind ) {
	case HFUN:
		hl_buffer_char(b,'(');
		hl_type_str_rec(b,t->fun->ret);
		hl_buffer_char(b,' ');
		hl_buffer_char(b,'(');
		for(i=0; i<t->fun->nargs; i++) {
			if( i ) hl_buffer_char(b,',');
			hl_type_str_rec(b,t->fun->args[i]);
		}
		hl_buffer_char(b,')');
		hl_buffer_char(b,')');
		break;
	case HOBJ:
		hl_buffer_char(b,'#');
		hl_buffer_str(b,t->obj->name);
		break;
	case HREF:
		hl_buffer_str(b,USTR("ref<"));
		hl_type_str_rec(b,t->tparam);
		hl_buffer_char(b,'>');
		break;
	case HVIRTUAL:
		hl_buffer_str(b,USTR("virtual<"));
		for(i=0; i<t->virt->nfields; i++) {
			hl_obj_field *f = t->virt->fields + i;
			if( i ) hl_buffer_char(b,',');
			hl_buffer_str(b,f->name);
			hl_buffer_char(b,':');
			hl_type_str_rec(b,f->t);
		}
		hl_buffer_char(b,'>');
		break;
	case HABSTRACT:
		hl_buffer_str(b,t->abs_name);
		break;
	case HENUM:
		hl_buffer_str(b,USTR("enum"));
		if( t->tenum->name ) {
			hl_buffer_char(b,'<');
			hl_buffer_str(b,t->tenum->name);
			hl_buffer_char(b,'>');
		}
		break;
	case HNULL:
		hl_buffer_str(b,USTR("null<"));
		hl_type_str_rec(b,t->tparam);
		hl_buffer_char(b,'>');
		break;
	default:
		hl_buffer_str(b,USTR("???"));
		break;
	}
}

const uchar *hl_type_str( hl_type *t ) {
	const uchar *c = TSTR[t->kind];
	hl_buffer *b;
	if( c != NULL )
		return c;
	b = hl_alloc_buffer();
	hl_type_str_rec(b,t);
	return hl_buffer_content(b,NULL);
}

HL_PRIM vbytes* hl_type_name( hl_type *t ) {
	switch( t->kind ) {
	case HOBJ:
		return (vbytes*)t->obj->name;
	case HENUM:
		return (vbytes*)t->tenum->name;
	case HABSTRACT:
		return (vbytes*)t->abs_name;
	default:
		break;
	}
	return NULL;
}

HL_PRIM varray* hl_type_enum_fields( hl_type *t ) {
	varray *a = (varray*)hl_gc_alloc_noptr(sizeof(varray) + t->tenum->nconstructs * sizeof(void*));
	int i;
	a->t = &hlt_array;
	a->at = &hlt_bytes;
	a->size = t->tenum->nconstructs;
	for( i=0; i<t->tenum->nconstructs;i++)
		((void**)(a+1))[i] = (vbytes*)t->tenum->constructs[i].name;
	return a;
}

HL_PRIM int hl_type_args_count( hl_type *t ) {
	if( t->kind == HFUN )
		return t->fun->nargs;
	return 0;
}

HL_PRIM varray *hl_type_instance_fields( hl_type *t ) {
	varray *a;
	int i;
	hl_type_obj *o;
	if( t->kind != HOBJ )
		return NULL;
	o = t->obj;
	a = hl_aalloc(&hlt_bytes,o->rt->nlookup);
	for(i=0;i<o->rt->nlookup;i++)
		((vbytes**)(a+1))[i] = (vbytes*)hl_field_name(o->rt->lookup[i].hashed_name);
	return a;
}

HL_PRIM vdynamic *hl_type_get_global( hl_type *t ) {
	switch( t->kind ) {
	case HOBJ:
		return *(vdynamic**)t->obj->global_value;
	case HENUM:
		hl_fatal("TODO");
		break;
	default:
		break;
	}
	return NULL;
}

DEFINE_PRIM(_BOOL, hl_type_check, _TYPE _DYN);
DEFINE_PRIM(_BYTES, hl_type_name, _TYPE);
DEFINE_PRIM(_ARR, hl_type_enum_fields, _TYPE);