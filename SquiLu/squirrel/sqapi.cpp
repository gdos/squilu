/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#include "sqvm.h"
#include "sqstring.h"
#include "sqtable.h"
#include "sqarray.h"
#include "sqfuncproto.h"
#include "sqclosure.h"
#include "squserdata.h"
#include "sqcompiler.h"
#include "sqfuncstate.h"
#include "sqclass.h"
#include <stdarg.h>

bool sq_aux_gettypedarg(HSQUIRRELVM v,SQInteger idx,SQObjectType type,SQObjectPtr **o)
{
	*o = &stack_get(v,idx);
	if(type(**o) != type){
		SQObjectPtr oval = v->PrintObjVal(**o);
		v->Raise_Error(_SC("wrong argument type, expected '%s' got '%.50s'"),IdType2Name(type),_stringval(oval));
		return false;
	}
	return true;
}

#define _GETSAFE_OBJ(v,idx,type,o) { if(!sq_aux_gettypedarg(v,idx,type,&o)) return SQ_ERROR; }

#define sq_aux_paramscheck(v,count) \
{ \
	if(sq_gettop(v) < count){ v->Raise_Error(_SC("not enough params in the stack")); return SQ_ERROR; }\
}


SQInteger sq_aux_invalidtype(HSQUIRRELVM v,SQObjectType type)
{
	return sq_throwerror(v, _SC("unexpected type %s"), IdType2Name(type));
}

HSQUIRRELVM sq_open(SQInteger initialstacksize)
{
	SQSharedState *ss;
	SQVM *v;
	sq_new(ss, SQSharedState);
	ss->Init();
	v = (SQVM *)SQ_MALLOC(sizeof(SQVM));
	new (v) SQVM(ss);
	ss->_root_vm = v;
	if(v->Init(NULL, initialstacksize)) {
		return v;
	} else {
		sq_delete(v, SQVM);
		return NULL;
	}
	return v;
}

HSQUIRRELVM sq_newthread(HSQUIRRELVM friendvm, SQInteger initialstacksize)
{
	SQSharedState *ss;
	SQVM *v;
	ss=_ss(friendvm);

	v= (SQVM *)SQ_MALLOC(sizeof(SQVM));
	new (v) SQVM(ss);

	if(v->Init(friendvm, initialstacksize)) {
		friendvm->Push(v);
		return v;
	} else {
		sq_delete(v, SQVM);
		return NULL;
	}
}

SQInteger sq_getvmstate(HSQUIRRELVM v)
{
	if(v->_suspended)
		return SQ_VMSTATE_SUSPENDED;
	else {
		if(v->_callsstacksize != 0) return SQ_VMSTATE_RUNNING;
		else return SQ_VMSTATE_IDLE;
	}
}

void sq_seterrorhandler(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v, -1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_errorhandler = o;
		v->Pop();
	}
}

SQRESULT sq_geterrorhandler(HSQUIRRELVM v)
{
    v->Push(v->_errorhandler);
	return 1;
}

void sq_setatexithandler(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v, -1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_atexithandler = o;
		v->Pop();
	}
}

SQRESULT sq_getatexithandler(HSQUIRRELVM v)
{
    v->Push(v->_atexithandler);
	return 1;
}

void sq_setnativedebughook(HSQUIRRELVM v,SQDEBUGHOOK hook)
{
	v->_debughook_native = hook;
	v->_debughook_closure.Null();
	v->_debughook = hook?true:false;
}

void sq_setdebughook(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v,-1);
	if(sq_isclosure(o) || sq_isnativeclosure(o) || sq_isnull(o)) {
		v->_debughook_closure = o;
		v->_debughook_native = NULL;
		v->_debughook = !sq_isnull(o);
		v->Pop();
	}
}

void sq_close(HSQUIRRELVM v)
{
	SQSharedState *ss = _ss(v);
	_thread(ss->_root_vm)->Finalize();
	sq_delete(ss, SQSharedState);
}

SQInteger sq_getversion()
{
	return SQUIRREL_VERSION_NUMBER;
}

SQRESULT sq_compile(HSQUIRRELVM v,SQLEXREADFUNC read,SQUserPointer p,const SQChar *sourcename
                    ,SQBool raiseerror, SQBool show_warnings)
{
	SQObjectPtr o;
#ifndef NO_COMPILER
	if(Compile(v, read, p, sourcename, o, raiseerror?true:false, _ss(v)->_debuginfo, show_warnings)) {
		v->Push(SQClosure::Create(_ss(v), _funcproto(o)));
		return SQ_OK;
	}
	return SQ_ERROR;
#else
	return sq_throwerror(v,_SC("this is a no compiler build"));
#endif
}

void sq_enabledebuginfo(HSQUIRRELVM v, SQBool enable)
{
	_ss(v)->_debuginfo = enable?true:false;
}

void sq_notifyallexceptions(HSQUIRRELVM v, SQBool enable)
{
	_ss(v)->_notifyallexceptions = enable?true:false;
}

void sq_addref(HSQUIRRELVM v,HSQOBJECT *po)
{
	if(!ISREFCOUNTED(type(*po))) return;
#ifdef NO_GARBAGE_COLLECTOR
	__AddRef(po->_type,po->_unVal);
#else
	_ss(v)->_refs_table.AddRef(*po);
#endif
}

SQUnsignedInteger sq_getrefcount(HSQUIRRELVM v,HSQOBJECT *po)
{
	if(!ISREFCOUNTED(type(*po))) return 0;
#ifdef NO_GARBAGE_COLLECTOR
   return po->_unVal.pRefCounted->_uiRef;
#else
   return _ss(v)->_refs_table.GetRefCount(*po);
#endif
}

SQBool sq_release(HSQUIRRELVM v,HSQOBJECT *po)
{
	if(!ISREFCOUNTED(type(*po))) return SQTrue;
#ifdef NO_GARBAGE_COLLECTOR
	bool ret = (po->_unVal.pRefCounted->_uiRef <= 1) ? SQTrue : SQFalse;
	__Release(po->_type,po->_unVal);
	return ret; //the ret val doesn't work(and cannot be fixed)
#else
	return _ss(v)->_refs_table.Release(*po);
#endif
}

const SQChar *sq_objtostring(const HSQOBJECT *o)
{
	if(sq_type(*o) == OT_STRING) {
		return _stringval(*o);
	}
	return NULL;
}

SQInteger sq_objtointeger(const HSQOBJECT *o)
{
	if(sq_isnumeric(*o)) {
		return tointeger(*o);
	}
	return 0;
}

SQFloat sq_objtofloat(const HSQOBJECT *o)
{
	if(sq_isnumeric(*o)) {
		return tofloat(*o);
	}
	return 0;
}

SQBool sq_objtobool(const HSQOBJECT *o)
{
	if(sq_isbool(*o)) {
		return _integer(*o);
	}
	return SQFalse;
}

SQUserPointer sq_objtouserpointer(const HSQOBJECT *o)
{
	if(sq_isuserpointer(*o)) {
		return _userpointer(*o);
	}
	return 0;
}

void sq_pushnull(HSQUIRRELVM v)
{
	v->PushNull();
}

void sq_pushstring(HSQUIRRELVM v,const SQChar *s,SQInteger len)
{
	if(s)
		v->Push(SQObjectPtr(SQString::Create(_ss(v), s, len)));
	else v->PushNull();
}

void sq_pushfstring(HSQUIRRELVM v,const SQChar *fmt, ...)
{
    if(fmt){
        SQChar str[1024];
        va_list vl;
        va_start(vl, fmt);
        SQInteger len = scvsnprintf(str, sizeof(str), fmt, vl);
        va_end(vl);
        v->Push(SQObjectPtr(SQString::Create(_ss(v), str, len)));
    }
	else v->PushNull();
}

void sq_pushinteger(HSQUIRRELVM v,SQInteger n)
{
	v->Push(n);
}

void sq_pushbool(HSQUIRRELVM v,SQBool b)
{
	v->Push(b?true:false);
}

void sq_pushfloat(HSQUIRRELVM v,SQFloat n)
{
	v->Push(n);
}

void sq_pushuserpointer(HSQUIRRELVM v,SQUserPointer p)
{
	v->Push(p);
}

SQUserPointer sq_newuserdata(HSQUIRRELVM v,SQUnsignedInteger size)
{
	SQUserData *ud = SQUserData::Create(_ss(v), size + SQ_ALIGNMENT);
	v->Push(ud);
	return (SQUserPointer)sq_aligning(ud + 1);
}

void sq_newtable(HSQUIRRELVM v)
{
	v->Push(SQTable::Create(_ss(v), 0));
}

void sq_newtableex(HSQUIRRELVM v,SQInteger initialcapacity)
{
	v->Push(SQTable::Create(_ss(v), initialcapacity));
}

void sq_newarray(HSQUIRRELVM v,SQInteger size)
{
	v->Push(SQArray::Create(_ss(v), size));
}

SQRESULT sq_newclass(HSQUIRRELVM v,SQBool hasbase)
{
	SQClass *baseclass = NULL;
	if(hasbase) {
		SQObjectPtr &base = stack_get(v,-1);
		if(type(base) != OT_CLASS)
			return sq_throwerror(v,_SC("invalid base type"));
		baseclass = _class(base);
	}
	SQClass *newclass = SQClass::Create(_ss(v), baseclass);
	if(baseclass) v->Pop();
	v->Push(newclass);
	return SQ_OK;
}

SQRESULT sq_pushnewclass(HSQUIRRELVM v, const SQChar *className,
                          const SQChar *parentName,
                          void *classTag, SQRegFunction *methods,
                          SQBool leaveOnTop){
    if(!className || !classTag)
        return sq_throwerror(v, _SC("Missing class name or class tag."));
    SQBool hasBase = parentName ? SQTrue : SQFalse;
    SQInteger top = sq_gettop(v);
    HSQOBJECT obj;

	sq_pushstring(v,className,-1);
	if(hasBase){
        sq_pushstring(v, parentName,-1);
        if (SQ_FAILED(sq_getonroottable(v)) || sq_gettype(v, -1) != OT_CLASS){
            sq_settop(v, top); //leave stack as we got it
            return sq_throwerror(v, _SC("Missing base class \"%s\" for \"%s\"."), parentName, className);
        }
	}
	sq_newclass(v, hasBase);
	sq_settypetag(v,-1,classTag);
	if(methods) sq_insert_reg_funcs(v, methods);

	if(leaveOnTop){
	    sq_resetobject(&obj);
	    sq_getstackobj(v, -1, &obj);
	}

	if(SQ_FAILED(sq_newslot(v, -3, SQFalse))){
            sq_settop(v, top); //leave stack as we got it
            return SQ_ERROR;
    }
    if(leaveOnTop) sq_pushobject(v, obj);
	return SQ_OK;
}

SQBool sq_instanceof(HSQUIRRELVM v)
{
	SQObjectPtr &inst = stack_get(v,-1);
	SQObjectPtr &cl = stack_get(v,-2);
	if(type(inst) != OT_INSTANCE || type(cl) != OT_CLASS)
		return sq_throwerror(v,_SC("invalid param type"));
	return _instance(inst)->InstanceOf(_class(cl))?SQTrue:SQFalse;
}

SQRESULT sq_arrayappend(HSQUIRRELVM v,SQInteger idx)
{
	sq_aux_paramscheck(v,2);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	_array(*arr)->Append(v->GetUp(-1));
	v->Pop();
	return SQ_OK;
}

SQRESULT sq_arraypop(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(_array(*arr)->Size() > 0) {
        if(pushval != 0){ v->Push(_array(*arr)->Top()); }
		_array(*arr)->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("empty array"));
}

SQRESULT sq_arrayresize(HSQUIRRELVM v,SQInteger idx,SQInteger newsize)
{
	sq_aux_paramscheck(v,1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	if(newsize >= 0) {
		_array(*arr)->Resize(newsize);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("negative size"));
}


SQRESULT sq_arrayreverse(HSQUIRRELVM v,SQInteger idx)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *o;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,o);
	SQArray *arr = _array(*o);
	if(arr->Size() > 0) {
		SQObjectPtr t;
		SQInteger size = arr->Size();
		SQInteger n = size >> 1; size -= 1;
		for(SQInteger i = 0; i < n; i++) {
			t = arr->_values[i];
			arr->_values[i] = arr->_values[size-i];
			arr->_values[size-i] = t;
		}
		return SQ_OK;
	}
	return SQ_OK;
}

SQRESULT sq_arrayremove(HSQUIRRELVM v,SQInteger idx,SQInteger itemidx)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	return _array(*arr)->Remove(itemidx) ? SQ_OK : sq_throwerror(v,_SC("index out of range"));
}

SQRESULT sq_arrayinsert(HSQUIRRELVM v,SQInteger idx,SQInteger destpos)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	SQRESULT ret = _array(*arr)->Insert(destpos, v->GetUp(-1)) ? SQ_OK : sq_throwerror(v,_SC("index out of range"));
	v->Pop();
	return ret;
}

SQRESULT sq_arrayset(HSQUIRRELVM v,SQInteger idx,SQInteger destpos)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	SQRESULT ret = _array(*arr)->Set(destpos, v->GetUp(-1)) ? SQ_OK : sq_throwerror(v,_SC("index out of range"));
	v->Pop();
	return ret;
}

SQRESULT sq_arrayget(HSQUIRRELVM v,SQInteger idx,SQInteger pos)
{
	sq_aux_paramscheck(v, 1);
	SQObjectPtr *arr;
	_GETSAFE_OBJ(v, idx, OT_ARRAY,arr);
	v->PushNull();
	if(!_array(*arr)->Get(pos, v->GetUp(-1)))
	{
        v->Pop();
        return sq_throwerror(v,_SC("index out of range"));
	}
	return SQ_OK;
}

void sq_newclosure(HSQUIRRELVM v,SQFUNCTION func,SQUnsignedInteger nfreevars)
{
	SQNativeClosure *nc = SQNativeClosure::Create(_ss(v), func,nfreevars);
	nc->_nparamscheck = 0;
	for(SQUnsignedInteger i = 0; i < nfreevars; i++) {
		nc->_outervalues[i] = v->Top();
		v->Pop();
	}
	v->Push(SQObjectPtr(nc));
}

SQRESULT sq_getclosureinfo(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger *nparams,SQUnsignedInteger *nfreevars)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(type(o) == OT_CLOSURE) {
		SQClosure *c = _closure(o);
		SQFunctionProto *proto = c->_function;
		*nparams = (SQUnsignedInteger)proto->_nparameters;
		*nfreevars = (SQUnsignedInteger)proto->_noutervalues;
		return SQ_OK;
	}
	else if(type(o) == OT_NATIVECLOSURE)
	{
		SQNativeClosure *c = _nativeclosure(o);
		*nparams = (SQUnsignedInteger)c->_nparamscheck;
		*nfreevars = c->_noutervalues;
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("the object is not a closure"));
}

SQRESULT sq_setnativeclosurename(HSQUIRRELVM v,SQInteger idx,const SQChar *name)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isnativeclosure(o)) {
		SQNativeClosure *nc = _nativeclosure(o);
		nc->_name = SQString::Create(_ss(v),name);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("the object is not a nativeclosure"));
}

SQRESULT sq_setparamscheck(HSQUIRRELVM v,SQInteger nparamscheck,const SQChar *typemask)
{
	SQObjectPtr &o = stack_get(v, -1);
	if(!sq_isnativeclosure(o))
		return sq_throwerror(v, _SC("native closure expected"));
	SQNativeClosure *nc = _nativeclosure(o);
	nc->_nparamscheck = nparamscheck;
	if(typemask) {
		SQIntVec res;
		if(!CompileTypemask(res, typemask))
			return sq_throwerror(v, _SC("invalid typemask"));
		nc->_typecheck.copy(res);
	}
	else {
		nc->_typecheck.resize(0);
	}
	if(nparamscheck == SQ_MATCHTYPEMASKSTRING) {
		nc->_nparamscheck = nc->_typecheck.size();
	}
	return SQ_OK;
}

SQRESULT sq_setfenv(HSQUIRRELVM v,SQInteger idx, SQBool cloning)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(!sq_isnativeclosure(o) &&
		!sq_isclosure(o))
		return sq_throwerror(v,_SC("the target is not a closure"));
    SQObjectPtr &env = stack_get(v,-1);
	if(!sq_istable(env) &&
		!sq_isarray(env) &&
		!sq_isclass(env) &&
		!sq_isinstance(env))
		return sq_throwerror(v,_SC("invalid environment"));
	SQWeakRef *w = _refcounted(env)->GetWeakRef(type(env));
	SQObjectPtr ret;
	if(sq_isclosure(o)) {
		SQClosure *c = cloning ? _closure(o)->Clone() : _closure(o);
		__ObjRelease(c->_env);
		c->_env = w;
		__ObjAddRef(c->_env);
		if(_closure(o)->_base) {
			c->_base = _closure(o)->_base;
			__ObjAddRef(c->_base);
		}
		ret = c;
	}
	else { //then must be a native closure
		SQNativeClosure *c = cloning ? _nativeclosure(o)->Clone() : _nativeclosure(o);
		__ObjRelease(c->_env);
		c->_env = w;
		__ObjAddRef(c->_env);
		ret = c;
	}
	v->Pop();
	if(cloning) v->Push(ret);
	return SQ_OK;
}

SQRESULT sq_getfenv(HSQUIRRELVM v,SQInteger idx, SQBool roottable_when_null)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(!sq_isnativeclosure(o) &&
		!sq_isclosure(o))
		return sq_throwerror(v,_SC("the source is not a closure"));
	if(sq_isclosure(o)) {
	    if(_closure(o)->_env) v->Push(_closure(o)->_env);
	    else {
	        if(roottable_when_null) sq_pushroottable(v);
	        else sq_pushnull(v);
	    }
	}
	else { //then must be a native closure
	    if(_nativeclosure(o)->_env) v->Push(_nativeclosure(o)->_env);
	    else {
	        if(roottable_when_null) sq_pushroottable(v);
	        else sq_pushnull(v);
	    }
	}
	return SQ_OK;
}

SQRESULT sq_bindenv(HSQUIRRELVM v,SQInteger idx)
{
    return sq_setfenv(v, idx, SQTrue);
}

SQRESULT sq_getclosurename(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);

	if(!sq_isnativeclosure(o) &&
		!sq_isclosure(o))
		return sq_throwerror(v,_SC("the target is not a closure"));
	if(sq_isnativeclosure(o))
	{
		v->Push(_nativeclosure(o)->_name);
	}
	else { //closure
		v->Push(_closure(o)->_function->_name);
	}
	return SQ_OK;
}

SQRESULT sq_clear(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o=stack_get(v,idx);
	switch(type(o)) {
		case OT_TABLE: _table(o)->Clear();	break;
		case OT_ARRAY: _array(o)->Resize(0); break;
		default:
			return sq_throwerror(v, _SC("clear only works on table and array"));
		break;

	}
	return SQ_OK;
}

void sq_pushroottable(HSQUIRRELVM v)
{
	v->Push(v->_roottable);
}

SQRESULT sq_getonroottable(HSQUIRRELVM v)
{
    SQObjectPtr &obj = v->GetUp(-1);
	if(_table(v->_roottable)->Get(obj,obj))
		return SQ_OK;
	v->Pop();
	return SQ_ERROR;
}

SQRESULT sq_setonroottable(HSQUIRRELVM v)
{
    SQObjectPtr &key = v->GetUp(-2);
	if(type(key) == OT_NULL) {
		v->Pop(2);
		return sq_throwerror(v, _SC("null key"));
	}
    _table(v->_roottable)->NewSlot(key, v->GetUp(-1));
    v->Pop(2);
    return SQ_OK;
}

void sq_pushregistrytable(HSQUIRRELVM v)
{
	v->Push(_ss(v)->_registry);
}

SQRESULT sq_getonregistrytable(HSQUIRRELVM v)
{
    SQObjectPtr &obj = v->GetUp(-1);
	if(_table(_ss(v)->_registry)->Get(obj,obj))
		return SQ_OK;
	v->Pop();
	return SQ_ERROR;
}

SQRESULT sq_setonregistrytable(HSQUIRRELVM v)
{
    SQObjectPtr &key = v->GetUp(-2);
	if(type(key) == OT_NULL) {
		v->Pop(2);
		return sq_throwerror(v, _SC("null key"));
	}
    _table(_ss(v)->_registry)->NewSlot(key, v->GetUp(-1));
    v->Pop(2);
    return SQ_OK;
}

void sq_pushconsttable(HSQUIRRELVM v)
{
	v->Push(_ss(v)->_consts);
}

SQRESULT sq_setroottable(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v, -1);
	if(sq_istable(o) || sq_isnull(o)) {
		v->_roottable = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("ivalid type"));
}

SQRESULT sq_setconsttable(HSQUIRRELVM v)
{
	SQObjectPtr &o = stack_get(v, -1);
	if(sq_istable(o)) {
		_ss(v)->_consts = o;
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v, _SC("ivalid type, expected table"));
}

void sq_setforeignptr(HSQUIRRELVM v,SQUserPointer p)
{
	v->_foreignptr = p;
}

SQUserPointer sq_getforeignptr(HSQUIRRELVM v)
{
	return v->_foreignptr;
}

void sq_push(HSQUIRRELVM v,SQInteger idx)
{
	v->Push(stack_get(v, idx));
}

SQObjectType sq_gettype(HSQUIRRELVM v,SQInteger idx)
{
	return type(stack_get(v, idx));
}

SQUIRREL_API const SQChar *sq_gettypename(HSQUIRRELVM v,SQInteger idx){
	SQObjectPtr &o = stack_get(v, idx);
    return GetTypeName(o);
}

SQRESULT sq_typeof(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	SQObjectPtr res;
	if(!v->TypeOf(o,res)) {
		return SQ_ERROR;
	}
	v->Push(res);
	return SQ_OK;
}

SQRESULT sq_tostring(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	SQObjectPtr res;
	if(!v->ToString(o,res)) {
		return SQ_ERROR;
	}
	v->Push(res);
	return SQ_OK;
}

SQRESULT sq_tobool(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr res;
	SQBool b;
	SQ_RETURN_IF_ERROR(sq_getbool(v,idx, &b));
	_integer(res) = b;
	v->Push(res);
	return SQ_OK;
}

SQRESULT sq_tointeger(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr res;
	SQInteger i;
	SQ_RETURN_IF_ERROR(sq_getinteger(v,idx, &i));
	_integer(res) = i;
	v->Push(res);
	return SQ_OK;
}

SQRESULT sq_tofloat(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr res;
	SQFloat f;
	SQ_RETURN_IF_ERROR(sq_getfloat(v,idx, &f));
	_float(res) = f;
	v->Push(res);
	return SQ_OK;
}

SQRESULT sq_getinteger(HSQUIRRELVM v,SQInteger idx,SQInteger *i)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*i = tointeger(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getfloat(HSQUIRRELVM v,SQInteger idx,SQFloat *f)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isnumeric(o)) {
		*f = tofloat(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getbool(HSQUIRRELVM v,SQInteger idx,SQBool *b)
{
	SQObjectPtr &o = stack_get(v, idx);
	if(sq_isbool(o)) {
		*b = _integer(o);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_getstring(HSQUIRRELVM v,SQInteger idx,const SQChar **c)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_STRING,o);
	*c = _stringval(*o);
	return SQ_OK;
}

SQRESULT sq_getstr_and_size(HSQUIRRELVM v,SQInteger idx,const SQChar **c, SQInteger *size)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_STRING,o);
	*c = _stringval(*o);
	*size = _string(*o)->_len;
	return SQ_OK;
}

SQRESULT sq_getthread(HSQUIRRELVM v,SQInteger idx,HSQUIRRELVM *thread)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_THREAD,o);
	*thread = _thread(*o);
	return SQ_OK;
}

SQRESULT sq_clone(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);
	v->PushNull();
	if(!v->Clone(o, stack_get(v, -1))){
		v->Pop();
		return SQ_ERROR;
	}
	return SQ_OK;
}

SQInteger sq_getsize(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	SQObjectType type = type(o);
	switch(type) {
	case OT_STRING:		return _string(o)->_len;
	case OT_TABLE:		return _table(o)->CountUsed();
	case OT_ARRAY:		return _array(o)->Size();
	case OT_USERDATA:	return _userdata(o)->_size;
	case OT_INSTANCE:	return _instance(o)->_class->_udsize;
	case OT_CLASS:		return _class(o)->_udsize;
	default:
		return sq_aux_invalidtype(v, type);
	}
}

SQHash sq_gethash(HSQUIRRELVM v, SQInteger idx)
{
	SQObjectPtr &o = stack_get(v, idx);
	return HashObj(o);
}

SQRESULT sq_getuserdata(HSQUIRRELVM v,SQInteger idx,SQUserPointer *p,SQUserPointer *typetag)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_USERDATA,o);
	(*p) = _userdataval(*o);
	if(typetag) *typetag = _userdata(*o)->_typetag;
	return SQ_OK;
}

SQRESULT sq_settypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	switch(type(o)) {
		case OT_USERDATA:	_userdata(o)->_typetag = typetag;	break;
		case OT_CLASS:		_class(o)->_typetag = typetag;		break;
		default:			return sq_throwerror(v,_SC("invalid object type"));
	}
	return SQ_OK;
}

SQRESULT sq_getobjtypetag(const HSQOBJECT *o,SQUserPointer * typetag)
{
  switch(type(*o)) {
    case OT_INSTANCE: *typetag = _instance(*o)->_class->_typetag; break;
    case OT_USERDATA: *typetag = _userdata(*o)->_typetag; break;
    case OT_CLASS:    *typetag = _class(*o)->_typetag; break;
    default: return SQ_ERROR;
  }
  return SQ_OK;
}

SQRESULT sq_gettypetag(HSQUIRRELVM v,SQInteger idx,SQUserPointer *typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(SQ_FAILED(sq_getobjtypetag(&o,typetag)))
		return sq_throwerror(v,_SC("invalid object type"));
	return SQ_OK;
}

SQRESULT sq_getuserpointer(HSQUIRRELVM v, SQInteger idx, SQUserPointer *p)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_USERPOINTER,o);
	(*p) = _userpointer(*o);
	return SQ_OK;
}

SQUserPointer sq_get_as_userpointer(HSQUIRRELVM v,SQInteger idx)
{
    SQObjectPtr &o = stack_get(v,idx);
    return _userpointer(o);
}

SQRESULT sq_setinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer p)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_INSTANCE) return sq_throwerror(v,_SC("the object is not a class instance"));
	_instance(o)->_userpointer = p;
	return SQ_OK;
}

SQRESULT sq_setclassudsize(HSQUIRRELVM v, SQInteger idx, SQInteger udsize)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_CLASS) return sq_throwerror(v,_SC("the object is not a class"));
	if(_class(o)->_locked) return sq_throwerror(v,_SC("the class is locked"));
	_class(o)->_udsize = udsize;
	return SQ_OK;
}


SQRESULT sq_getinstanceup(HSQUIRRELVM v, SQInteger idx, SQUserPointer *p,SQUserPointer typetag)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_INSTANCE) return sq_throwerror(v,_SC("the object is not a class instance"));
	(*p) = _instance(o)->_userpointer;
	if(typetag != 0) {
		SQClass *cl = _instance(o)->_class;
		do{
			if(cl->_typetag == typetag)
				return SQ_OK;
			cl = cl->_base;
		}while(cl != NULL);
		return sq_throwerror(v,_SC("invalid type tag"));
	}
	return SQ_OK;
}

SQInteger sq_getfulltop(HSQUIRRELVM v)
{
	return v->_top;
}

SQInteger sq_gettop(HSQUIRRELVM v)
{
	return (v->_top) - v->_stackbase;
}

void sq_settop(HSQUIRRELVM v, SQInteger newtop)
{
	SQInteger top = sq_gettop(v);
	if(top > newtop)
		sq_pop(v, top - newtop);
	else
		while(top++ < newtop) sq_pushnull(v);
}

void sq_pop(HSQUIRRELVM v, SQInteger nelemstopop)
{
	assert(v->_top >= nelemstopop);
	v->Pop(nelemstopop);
}

void sq_poptop(HSQUIRRELVM v)
{
	assert(v->_top >= 1);
    v->Pop();
}


void sq_remove(HSQUIRRELVM v, SQInteger idx)
{
	v->Remove(idx);
}

void sq_insert(HSQUIRRELVM v, SQInteger idx)
{
	v->Insert(idx);
}

void sq_replace(HSQUIRRELVM v, SQInteger idx)
{
	v->Replace(idx);
}

SQInteger sq_compare(HSQUIRRELVM v, SQInteger idx1, SQInteger idx2)
{
	SQInteger res;
	v->ObjCmp(stack_get(v, idx1), stack_get(v, idx2),res);
	return res;
}

SQInteger sq_cmp(HSQUIRRELVM v)
{
	SQInteger res;
	v->ObjCmp(stack_get(v, -1), stack_get(v, -2),res);
	return res;
}

SQRESULT sq_newslot(HSQUIRRELVM v, SQInteger idx, SQBool bstatic)
{
	sq_aux_paramscheck(v, 3);
	SQObjectPtr &self = stack_get(v, idx);
	if(type(self) == OT_TABLE || type(self) == OT_CLASS) {
		SQObjectPtr &key = v->GetUp(-2);
		if(type(key) == OT_NULL) return sq_throwerror(v, _SC("null is not a valid key"));
		v->NewSlot(self, key, v->GetUp(-1),bstatic?true:false);
		v->Pop(2);
	}
	return SQ_OK;
}

SQRESULT sq_deleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr &key = v->GetUp(-1);
	if(type(key) == OT_NULL) return sq_throwerror(v, _SC("null is not a valid key"));
	SQObjectPtr res;
	if(!v->DeleteSlot(*self, key, res)){
		v->Pop();
		return SQ_ERROR;
	}
	if(pushval)	v->GetUp(-1) = res;
	else v->Pop();
	return SQ_OK;
}

SQRESULT sq_set(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	if(v->Set(self, v->GetUp(-2), v->GetUp(-1),DONT_FALL_BACK)) {
		v->Pop(2);
		return SQ_OK;
	}
	return SQ_ERROR;
}

SQRESULT sq_rawset(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	SQObjectPtr &key = v->GetUp(-2);
	if(type(key) == OT_NULL) {
		v->Pop(2);
		return sq_throwerror(v, _SC("null key"));
	}
	switch(type(self)) {
	case OT_TABLE:
		_table(self)->NewSlot(key, v->GetUp(-1));
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_CLASS:
		_class(self)->NewSlot(_ss(v), key, v->GetUp(-1),false);
		v->Pop(2);
		return SQ_OK;
	break;
	case OT_INSTANCE:
		if(_instance(self)->Set(key, v->GetUp(-1))) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	case OT_ARRAY:
		if(v->Set(self, key, v->GetUp(-1),false)) {
			v->Pop(2);
			return SQ_OK;
		}
	break;
	default:
		v->Pop(2);
		return sq_throwerror(v, _SC("rawset works only on array/table/class and instance"));
	}
	v->Raise_IdxError(v->GetUp(-2));return SQ_ERROR;
}

SQRESULT sq_newmember(HSQUIRRELVM v,SQInteger idx,SQBool bstatic)
{
	SQObjectPtr &self = stack_get(v, idx);
	if(type(self) != OT_CLASS) return sq_throwerror(v, _SC("new member only works with classes"));
	SQObjectPtr &key = v->GetUp(-3);
	if(type(key) == OT_NULL) return sq_throwerror(v, _SC("null key"));
	if(!v->NewSlotA(self,key,v->GetUp(-2),v->GetUp(-1),bstatic?true:false,false))
		return SQ_ERROR;
	return SQ_OK;
}

SQRESULT sq_rawnewmember(HSQUIRRELVM v,SQInteger idx,SQBool bstatic)
{
	SQObjectPtr &self = stack_get(v, idx);
	if(type(self) != OT_CLASS) return sq_throwerror(v, _SC("new member only works with classes"));
	SQObjectPtr &key = v->GetUp(-3);
	if(type(key) == OT_NULL) return sq_throwerror(v, _SC("null key"));
	if(!v->NewSlotA(self,key,v->GetUp(-2),v->GetUp(-1),bstatic?true:false,true))
		return SQ_ERROR;
	return SQ_OK;
}

SQRESULT sq_setdelegate(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self = stack_get(v, idx);
	SQObjectPtr &mt = v->GetUp(-1);
	SQObjectType type = type(self);
	switch(type) {
	case OT_TABLE:
		if(type(mt) == OT_TABLE) {
			if(!_table(self)->SetDelegate(_table(mt))) return sq_throwerror(v, _SC("delagate cycle")); v->Pop();}
		else if(type(mt)==OT_NULL) {
			_table(self)->SetDelegate(NULL); v->Pop(); }
		else return sq_aux_invalidtype(v,type);
		break;
	case OT_USERDATA:
		if(type(mt)==OT_TABLE) {
			_userdata(self)->SetDelegate(_table(mt)); v->Pop(); }
		else if(type(mt)==OT_NULL) {
			_userdata(self)->SetDelegate(NULL); v->Pop(); }
		else return sq_aux_invalidtype(v, type);
		break;
	default:
			return sq_aux_invalidtype(v, type);
		break;
	}
	return SQ_OK;
}

SQRESULT sq_rawdeleteslot(HSQUIRRELVM v,SQInteger idx,SQBool pushval)
{
	sq_aux_paramscheck(v, 2);
	SQObjectPtr *self;
	_GETSAFE_OBJ(v, idx, OT_TABLE,self);
	SQObjectPtr &key = v->GetUp(-1);
	SQObjectPtr t;
	if(_table(*self)->Get(key,t)) {
		_table(*self)->Remove(key);
	}
	if(pushval != 0)
		v->GetUp(-1) = t;
	else
		v->Pop();
	return SQ_OK;
}

SQRESULT sq_getdelegate(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	switch(type(self)){
	case OT_TABLE:
	case OT_USERDATA:
		if(!_delegable(self)->_delegate){
			v->PushNull();
			break;
		}
		v->Push(SQObjectPtr(_delegable(self)->_delegate));
		break;
	default: return sq_throwerror(v,_SC("wrong type")); break;
	}
	return SQ_OK;

}

SQRESULT sq_get(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	SQObjectPtr &obj = v->GetUp(-1);
	if(v->Get(self,obj,obj,false,DONT_FALL_BACK))
		return SQ_OK;
	v->Pop();
	return SQ_ERROR;
}

SQRESULT sq_getbyname(HSQUIRRELVM v,SQInteger idx, const SQChar *key, SQInteger key_len)
{
	SQObjectPtr &self=stack_get(v,idx);
	sq_pushstring(v, key, key_len);
	SQObjectPtr &obj = v->GetUp(-1);
	switch(type(self)) {
	case OT_TABLE:
		if(_table(self)->Get(obj,obj))
			return SQ_OK;
		break;
	case OT_CLASS:
		if(_class(self)->Get(obj,obj))
			return SQ_OK;
		break;
	case OT_INSTANCE:
		if(_instance(self)->Get(obj,obj))
			return SQ_OK;
		break;
	default:
		sq_poptop(v);
		return sq_throwerror(v,_SC("sq_getbyname works only on table/instance and class"));
	}
	sq_poptop(v);
	return sq_throwerror(v,_SC("the index doesn't exist"));
}

SQRESULT sq_rawget(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	SQObjectPtr &obj = v->GetUp(-1);
	switch(type(self)) {
	case OT_TABLE:
		if(_table(self)->Get(obj,obj))
			return SQ_OK;
		break;
	case OT_CLASS:
		if(_class(self)->Get(obj,obj))
			return SQ_OK;
		break;
	case OT_INSTANCE:
		if(_instance(self)->Get(obj,obj))
			return SQ_OK;
		break;
	case OT_ARRAY:{
		if(sq_isnumeric(obj)){
			if(_array(self)->Get(tointeger(obj),obj)) {
				return SQ_OK;
			}
		}
		else {
			v->Pop();
			return sq_throwerror(v,_SC("invalid index type for an array"));
		}
				  }
		break;
	default:
		v->Pop();
		return sq_throwerror(v,_SC("rawget works only on array/table/instance and class"));
	}
	v->Pop();
	return sq_throwerror(v,_SC("the index doesn't exist"));
}

SQBool sq_rawexists(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &self=stack_get(v,idx);
	SQObjectPtr &obj = v->GetUp(-1);
	SQBool result = SQFalse;
	switch(type(self)) {
	case OT_TABLE:
		result = _table(self)->Exists(obj);
		break;
	case OT_CLASS:
		result = _class(self)->Exists(obj);
		break;
	case OT_INSTANCE:
		result = _instance(self)->Exists(obj);
		break;
	case OT_ARRAY:
		if(sq_isnumeric(obj)){
			result = _array(self)->Exists(tointeger(obj));
		}
		break;
	}
	v->Pop();
	return result;
}

SQRESULT sq_getstackobj(HSQUIRRELVM v,SQInteger idx,HSQOBJECT *po)
{
	*po=stack_get(v,idx);
	return SQ_OK;
}

const SQChar *sq_getlocal(HSQUIRRELVM v,SQUnsignedInteger level,SQUnsignedInteger idx)
{
	SQUnsignedInteger cstksize=v->_callsstacksize;
	SQUnsignedInteger lvl=(cstksize-level)-1;
	SQInteger stackbase=v->_stackbase;
	if(lvl<cstksize){
		for(SQUnsignedInteger i=0;i<level;i++){
			SQVM::CallInfo &ci=v->_callsstack[(cstksize-i)-1];
			stackbase-=ci._prevstkbase;
		}
		SQVM::CallInfo &ci=v->_callsstack[lvl];
		if(type(ci._closure)!=OT_CLOSURE)
			return NULL;
		SQClosure *c=_closure(ci._closure);
		SQFunctionProto *func=c->_function;
		if(func->_noutervalues > (SQInteger)idx) {
			v->Push(*_outer(c->_outervalues[idx])->_valptr);
			return _stringval(func->_outervalues[idx]._name);
		}
		idx -= func->_noutervalues;
		return func->GetLocal(v,stackbase,idx,(SQInteger)(ci._ip-func->_instructions)-1);
	}
	return NULL;
}

void sq_pushobject(HSQUIRRELVM v,HSQOBJECT obj)
{
	v->Push(SQObjectPtr(obj));
}

void sq_resetobject(HSQOBJECT *po)
{
	po->_unVal.pUserPointer=NULL;po->_type=OT_NULL;
}

SQRESULT sq_throwerror(HSQUIRRELVM v,const SQChar *fmt, ...)
{
    SQChar err[256];
    va_list vl;
    va_start(vl, fmt);
    scvsnprintf(err, sizeof(err), fmt, vl);
    va_end(vl);
	v->_lasterror=SQString::Create(_ss(v),err);
	return SQ_ERROR;
}

SQRESULT sq_throwobject(HSQUIRRELVM v)
{
	v->_lasterror = v->GetUp(-1);
	v->Pop();
	return SQ_ERROR;
}


void sq_reseterror(HSQUIRRELVM v)
{
	v->_lasterror.Null();
}

void sq_getlasterror(HSQUIRRELVM v)
{
	v->Push(v->_lasterror);
}

void sq_getlaststackinfo(HSQUIRRELVM v)
{
    sq_newtable(v);
    sq_pushliteral(v, _SC("line"));
    sq_pushinteger(v, v->_lasterror_stackinfo.line);
    sq_rawset(v, -3);
    sq_pushliteral(v, _SC("source"));
    sq_pushstring(v, v->_lasterror_stackinfo.source, -1);
    sq_rawset(v, -3);
    sq_pushliteral(v, _SC("funcname"));
    sq_pushstring(v, v->_lasterror_stackinfo.funcname, -1);
    sq_rawset(v, -3);
}

const SQChar *sq_getlasterror_str(HSQUIRRELVM v)
{
	return _stringval(v->_lasterror);
}

SQRESULT sq_reservestack(HSQUIRRELVM v,SQInteger nsize)
{
	if (((SQUnsignedInteger)v->_top + nsize) > v->_stack.size()) {
		if(v->_nmetamethodscall) {
			return sq_throwerror(v,_SC("cannot resize stack while in  a metamethod"));
		}
		v->_stack.resize(v->_stack.size() + ((v->_top + nsize) - v->_stack.size()));
	}
	return SQ_OK;
}

SQRESULT sq_resume(HSQUIRRELVM v,SQBool retval,SQBool raiseerror)
{
    SQObjectPtr &obj = v->GetUp(-1);
	if(type(obj)==OT_GENERATOR){
		v->PushNull(); //retval
		if(!v->Execute(v->GetUp(-2),0,v->_top,obj,raiseerror,SQVM::ET_RESUME_GENERATOR))
		{v->Raise_Error(v->_lasterror); return SQ_ERROR;}
		if(!retval)
			v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("only generators can be resumed"));
}

SQRESULT sq_call(HSQUIRRELVM v,SQInteger params,SQBool retval,SQBool raiseerror)
{
	SQObjectPtr res;
	if(v->Call(v->GetUp(-(params+1)),params,v->_top-params,res,raiseerror?true:false)){

		if(!v->_suspended) {
			v->Pop(params);//pop closure and args
			//collect garbage right after function call if any
			_ss(v)->CallDelayedReleaseHooks(v);
		}
		if(retval){
			v->Push(res); return SQ_OK;
		}
		return SQ_OK;
	}
	else {
		v->Pop(params);
		return SQ_ERROR;
	}
	if(!v->_suspended)
		v->Pop(params);
	return sq_throwerror(v,_SC("call failed"));
}

SQRESULT sq_suspendvm(HSQUIRRELVM v)
{
	return v->Suspend();
}

SQRESULT sq_wakeupvm(HSQUIRRELVM v,SQBool wakeupret,SQBool retval,SQBool raiseerror,SQBool throwerror)
{
	SQObjectPtr ret;
	if(!v->_suspended)
		return sq_throwerror(v,_SC("cannot resume a vm that is not running any code"));
	SQInteger target = v->_suspended_target;
	if(wakeupret) {
		if(target != -1) {
			v->GetAt(v->_stackbase+v->_suspended_target)=v->GetUp(-1); //retval
		}
		v->Pop();
	} else if(target != -1) { v->GetAt(v->_stackbase+v->_suspended_target).Null(); }
	SQObjectPtr dummy;
	if(!v->Execute(dummy,-1,-1,ret,raiseerror,throwerror?SQVM::ET_RESUME_THROW_VM : SQVM::ET_RESUME_VM)) {
		return SQ_ERROR;
	}
	if(retval)
		v->Push(ret);
	return SQ_OK;
}

void sq_setreleasehook(HSQUIRRELVM v,SQInteger idx,SQRELEASEHOOK hook)
{
	if(sq_gettop(v) >= 1){
		SQObjectPtr &ud=stack_get(v,idx);
		switch( type(ud) ) {
		case OT_USERDATA:	_userdata(ud)->_hook = hook;	break;
		case OT_INSTANCE:	_instance(ud)->_hook = hook;	break;
		case OT_CLASS:		_class(ud)->_hook = hook;		break;
		default: break; //shutup compiler
		}
	}
}

void sq_setcompilererrorhandler(HSQUIRRELVM v,SQCOMPILERERROR f)
{
	_ss(v)->_compilererrorhandler = f;
}

SQRESULT sq_writeclosure(HSQUIRRELVM v,SQWRITEFUNC w,SQUserPointer up)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, -1, OT_CLOSURE,o);
	unsigned short tag = SQ_BYTECODE_STREAM_TAG;
	if(_closure(*o)->_function->_noutervalues)
		return sq_throwerror(v,_SC("a closure with free valiables bound cannot be serialized"));
	if(w(up,&tag,2) != 2)
		return sq_throwerror(v,_SC("io error"));
	if(!_closure(*o)->Save(v,up,w))
		return SQ_ERROR;
	return SQ_OK;
}

SQRESULT sq_writeclosure_as_source(HSQUIRRELVM v,SQWRITEFUNC w,SQUserPointer up)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, -1, OT_CLOSURE,o);
	if(_closure(*o)->_function->_noutervalues)
		return sq_throwerror(v,_SC("a closure with free valiables bound cannot be serialized"));
    const SQChar decl[] = _SC("local bytecode = ");
	if(w(up, (void*)decl, scstrlen(decl)) != scstrlen(decl))
		return sq_throwerror(v,_SC("io error"));
	if(!_closure(*o)->SaveAsSource(v,up,w))
		return SQ_ERROR;
	return SQ_OK;
}

SQRESULT sq_readclosure(HSQUIRRELVM v,SQREADFUNC r,SQUserPointer up)
{
	SQObjectPtr closure;

	unsigned short tag;
	if(r(up,&tag,2) != 2)
		return sq_throwerror(v,_SC("io error"));
	if(tag != SQ_BYTECODE_STREAM_TAG)
		return sq_throwerror(v,_SC("invalid stream"));
	if(!SQClosure::Load(v,up,r,closure))
		return SQ_ERROR;
	v->Push(closure);
	return SQ_OK;
}

SQChar *sq_getscratchpad(HSQUIRRELVM v,SQInteger minsize)
{
	return _ss(v)->GetScratchPad(minsize);
}

SQRESULT sq_resurrectunreachable(HSQUIRRELVM v)
{
#ifndef NO_GARBAGE_COLLECTOR
	_ss(v)->ResurrectUnreachable(v);
	return SQ_OK;
#else
	return sq_throwerror(v,_SC("sq_resurrectunreachable requires a garbage collector build"));
#endif
}

SQInteger sq_collectgarbage(HSQUIRRELVM v)
{
#ifndef NO_GARBAGE_COLLECTOR
    _ss(v)->CallDelayedReleaseHooks(v);
	return _ss(v)->CollectGarbage(v);
#else
	return -1;
#endif
}

SQRESULT sq_getcallee(HSQUIRRELVM v)
{
	if(v->_callsstacksize > 1)
	{
		v->Push(v->_callsstack[v->_callsstacksize - 2]._closure);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("no closure in the calls stack"));
}

const SQChar *sq_getfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr &self=stack_get(v,idx);
	const SQChar *name = NULL;
	switch(type(self))
	{
	case OT_CLOSURE:{
		SQClosure *clo = _closure(self);
		SQFunctionProto *fp = clo->_function;
		if(((SQUnsignedInteger)fp->_noutervalues) > nval) {
			v->Push(*(_outer(clo->_outervalues[nval])->_valptr));
			SQOuterVar &ov = fp->_outervalues[nval];
			name = _stringval(ov._name);
		}
					}
		break;
	case OT_NATIVECLOSURE:{
		SQNativeClosure *clo = _nativeclosure(self);
		if(clo->_noutervalues > nval) {
			v->Push(clo->_outervalues[nval]);
			name = _SC("@NATIVE");
		}
						  }
		break;
	default: break; //shutup compiler
	}
	return name;
}

SQRESULT sq_setfreevariable(HSQUIRRELVM v,SQInteger idx,SQUnsignedInteger nval)
{
	SQObjectPtr &self=stack_get(v,idx);
	switch(type(self))
	{
	case OT_CLOSURE:{
		SQFunctionProto *fp = _closure(self)->_function;
		if(((SQUnsignedInteger)fp->_noutervalues) > nval){
			*(_outer(_closure(self)->_outervalues[nval])->_valptr) = stack_get(v,-1);
		}
		else return sq_throwerror(v,_SC("invalid free var index"));
					}
		break;
	case OT_NATIVECLOSURE:
		if(_nativeclosure(self)->_noutervalues > nval){
			_nativeclosure(self)->_outervalues[nval] = stack_get(v,-1);
		}
		else return sq_throwerror(v,_SC("invalid free var index"));
		break;
	default:
		return sq_aux_invalidtype(v,type(self));
	}
	v->Pop();
	return SQ_OK;
}

SQRESULT sq_setattributes(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr &key = stack_get(v,-2);
	SQObjectPtr &val = stack_get(v,-1);
	SQObjectPtr attrs;
	if(type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		_class(*o)->_attributes = val;
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}else if(_class(*o)->GetAttributes(key,attrs)) {
		_class(*o)->SetAttributes(key,val);
		v->Pop(2);
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("wrong index"));
}

SQRESULT sq_getattributes(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr &key = stack_get(v,-1);
	SQObjectPtr attrs;
	if(type(key) == OT_NULL) {
		attrs = _class(*o)->_attributes;
		v->Pop();
		v->Push(attrs);
		return SQ_OK;
	}
	else if(_class(*o)->GetAttributes(key,attrs)) {
		v->Pop();
		v->Push(attrs);
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("wrong index"));
}

SQRESULT sq_getmemberhandle(HSQUIRRELVM v,SQInteger idx,HSQMEMBERHANDLE *handle)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	SQObjectPtr &key = stack_get(v,-1);
	SQTable *m = _class(*o)->_members;
	SQObjectPtr val;
	if(m->Get(key,val)) {
		handle->_static = _isfield(val) ? SQFalse : SQTrue;
		handle->_index = _member_idx(val);
		v->Pop();
		return SQ_OK;
	}
	return sq_throwerror(v,_SC("wrong index"));
}

SQRESULT _getmemberbyhandle(HSQUIRRELVM v,SQObjectPtr &self,const HSQMEMBERHANDLE *handle,SQObjectPtr *&val)
{
	switch(type(self)) {
		case OT_INSTANCE: {
				SQInstance *i = _instance(self);
				if(handle->_static) {
					SQClass *c = i->_class;
					val = &c->_methods[handle->_index].val;
				}
				else {
					val = &i->_values[handle->_index];

				}
			}
			break;
		case OT_CLASS: {
				SQClass *c = _class(self);
				if(handle->_static) {
					val = &c->_methods[handle->_index].val;
				}
				else {
					val = &c->_defaultvalues[handle->_index].val;
				}
			}
			break;
		default:
			return sq_throwerror(v,_SC("wrong type(expected class or instance)"));
	}
	return SQ_OK;
}

SQRESULT sq_getbyhandle(HSQUIRRELVM v,SQInteger idx,const HSQMEMBERHANDLE *handle)
{
	SQObjectPtr &self = stack_get(v,idx);
	SQObjectPtr *val = NULL;
	if(SQ_FAILED(_getmemberbyhandle(v,self,handle,val))) {
		return SQ_ERROR;
	}
	v->Push(_realval(*val));
	return SQ_OK;
}

SQRESULT sq_setbyhandle(HSQUIRRELVM v,SQInteger idx,const HSQMEMBERHANDLE *handle)
{
	SQObjectPtr &self = stack_get(v,idx);
	SQObjectPtr &newval = stack_get(v,-1);
	SQObjectPtr *val = NULL;
	if(SQ_FAILED(_getmemberbyhandle(v,self,handle,val))) {
		return SQ_ERROR;
	}
	*val = newval;
	v->Pop();
	return SQ_OK;
}

SQRESULT sq_getbase(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	if(_class(*o)->_base)
		v->Push(SQObjectPtr(_class(*o)->_base));
	else
		v->PushNull();
	return SQ_OK;
}

SQRESULT sq_getclass(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_INSTANCE,o);
	v->Push(SQObjectPtr(_instance(*o)->_class));
	return SQ_OK;
}

SQRESULT sq_createinstance(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr *o = NULL;
	_GETSAFE_OBJ(v, idx, OT_CLASS,o);
	v->Push(_class(*o)->CreateInstance());
	return SQ_OK;
}

void sq_weakref(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o=stack_get(v,idx);
	if(ISREFCOUNTED(type(o))) {
		v->Push(_refcounted(o)->GetWeakRef(type(o)));
		return;
	}
	v->Push(o);
}

SQRESULT sq_getweakrefval(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o = stack_get(v,idx);
	if(type(o) != OT_WEAKREF) {
		return sq_throwerror(v,_SC("the object must be a weakref"));
	}
	v->Push(_weakref(o)->_obj);
	return SQ_OK;
}

SQRESULT sq_getdefaultdelegate(HSQUIRRELVM v,SQObjectType t)
{
	SQSharedState *ss = _ss(v);
	switch(t) {
	case OT_TABLE: v->Push(ss->_table_default_delegate); break;
	case OT_ARRAY: v->Push(ss->_array_default_delegate); break;
	case OT_STRING: v->Push(ss->_string_default_delegate); break;
	case OT_INTEGER: case OT_FLOAT: v->Push(ss->_number_default_delegate); break;
	case OT_GENERATOR: v->Push(ss->_generator_default_delegate); break;
	case OT_CLOSURE: case OT_NATIVECLOSURE: v->Push(ss->_closure_default_delegate); break;
	case OT_THREAD: v->Push(ss->_thread_default_delegate); break;
	case OT_CLASS: v->Push(ss->_class_default_delegate); break;
	case OT_INSTANCE: v->Push(ss->_instance_default_delegate); break;
	case OT_WEAKREF: v->Push(ss->_weakref_default_delegate); break;
	default: return sq_throwerror(v,_SC("the type doesn't have a default delegate"));
	}
	return SQ_OK;
}

SQRESULT sq_next(HSQUIRRELVM v,SQInteger idx)
{
	SQObjectPtr &o=stack_get(v,idx),&refpos = stack_get(v,-1),realkey,val;
	if(type(o) == OT_GENERATOR) {
		return sq_throwerror(v,_SC("cannot iterate a generator"));
	}
	int faketojump;
	if(!v->FOREACH_OP(o,realkey,val,refpos,0,666,faketojump))
		return SQ_ERROR;
	if(faketojump != 666) {
		v->Push(realkey);
		v->Push(val);
		return SQ_OK;
	}
	return SQ_ERROR;
}

struct BufState{
	const SQChar *buf;
	SQInteger ptr;
	SQInteger size;
};

SQInteger buf_lexfeed(SQUserPointer file)
{
	BufState *buf=(BufState*)file;
	if(buf->size<(buf->ptr+1))
		return 0;
	return buf->buf[buf->ptr++];
}

SQRESULT sq_compilebuffer(HSQUIRRELVM v,const SQChar *s,SQInteger size,const SQChar *sourcename,
                          SQBool raiseerror, SQBool show_warnings) {
	BufState buf;
	buf.buf = s;
	buf.size = size;
	buf.ptr = 0;
	return sq_compile(v, buf_lexfeed, &buf, sourcename, raiseerror, show_warnings);
}

void sq_move(HSQUIRRELVM dest,HSQUIRRELVM src,SQInteger idx)
{
	dest->Push(stack_get(src,idx));
}

void sq_setprintfunc(HSQUIRRELVM v, SQPRINTFUNCTION printfunc,SQPRINTFUNCTION errfunc)
{
	_ss(v)->_printfunc = printfunc;
	_ss(v)->_errorfunc = errfunc;
}

SQPRINTFUNCTION sq_getprintfunc(HSQUIRRELVM v)
{
	return _ss(v)->_printfunc;
}

SQPRINTFUNCTION sq_geterrorfunc(HSQUIRRELVM v)
{
	return _ss(v)->_errorfunc;
}

void *sq_malloc(SQUnsignedInteger size)
{
	return SQ_MALLOC(size);
}

void *sq_realloc(void* p,SQUnsignedInteger oldsize,SQUnsignedInteger newsize)
{
	return SQ_REALLOC(p,oldsize,newsize);
}

void sq_free(void *p,SQUnsignedInteger size)
{
	SQ_FREE(p,size);
}


SQRESULT sq_optinteger(HSQUIRRELVM sqvm, SQInteger idx, SQInteger *value, SQInteger default_value){
    if(sq_gettop(sqvm) >= idx){
        return sq_getinteger(sqvm, idx, value);
    }
    *value = default_value;
    return SQ_OK;
}

SQRESULT sq_optstr_and_size(HSQUIRRELVM sqvm, SQInteger idx, const SQChar **value, const SQChar *dflt, SQInteger *size){
    if(sq_gettop(sqvm) >= idx){
        return sq_getstr_and_size(sqvm, idx, value, size);
    }
    *value = dflt;
    *size = scstrlen(dflt);
    return SQ_OK;
}

void sq_insertfunc(HSQUIRRELVM sqvm, const SQChar *fname, SQFUNCTION func,
                        SQInteger nparamscheck, const SQChar *typemask, SQBool isStatic){
    sq_pushstring(sqvm,fname,-1);
    sq_newclosure(sqvm,func,0);
    sq_setparamscheck(sqvm,nparamscheck,typemask);
    sq_setnativeclosurename(sqvm,-1,fname);
    sq_newslot(sqvm,-3,isStatic);
}

void sq_insert_reg_funcs(HSQUIRRELVM sqvm, SQRegFunction *obj_funcs){
	SQInteger i = 0;
	while(obj_funcs[i].name != 0) {
		SQRegFunction &f = obj_funcs[i];
		sq_pushstring(sqvm,f.name,-1);
		sq_newclosure(sqvm,f.f,0);
		sq_setparamscheck(sqvm,f.nparamscheck,f.typemask);
		sq_setnativeclosurename(sqvm,-1,f.name);
		sq_newslot(sqvm,-3,f.isStatic);
		i++;
	}
}

#define DONE_AND_RETURN(x) {ret_val =x; goto done_and_return;}
#define CHECK_OK(v) if((ret_val = v) < 0) goto done_and_return;

#define DONE_AND_RETURN(x) {ret_val =x; goto done_and_return;}
#define CHECK_OK(v) if((ret_val = v) < 0) goto done_and_return;

SQRESULT sq_call_va_vl(HSQUIRRELVM v, SQBool reset_stack, SQInteger idx, const SQChar *func, SQInteger idx_this, const SQChar *sig, va_list vl)
{
    int narg; // nres;  /* number of arguments and results */
    SQRESULT ret_val = 0;
    SQObjectType toptype;
    SQInteger top = sq_gettop(v);

    sq_pushstring(v, func, -1);
    CHECK_OK(sq_get(v, idx > 0 ? idx : idx-1));
    toptype = sq_gettype(v, -1);
    if(!(toptype == OT_CLOSURE || toptype == OT_NATIVECLOSURE)) DONE_AND_RETURN(-100);

    if(idx_this == 0) sq_pushroottable(v);
    else sq_push(v, idx_this > 0 ? idx_this : idx_this-1);
    /* push arguments */
    narg = 1;
    while (*sig)    /* push arguments */
    {
        switch (*sig++)
        {
        case _SC('d'):  /* double argument */
            sq_pushfloat(v, va_arg(vl, double));
            break;

        case _SC('i'):  /* int argument */
            sq_pushinteger(v, va_arg(vl, int));
            break;

        case _SC('s'):  /* string argument */
            sq_pushstring(v, va_arg(vl, SQChar *), -1);
            break;

        case _SC('b'):  /* string argument */
            sq_pushbool(v, va_arg(vl, int));
            break;

        case _SC('n'):  /* string argument */
            sq_pushnull(v);
            break;

        case _SC('p'):  /* string argument */
            sq_pushuserpointer(v, va_arg(vl, void *));
            break;

        case _SC('>'):
            goto endwhile;

        default:
            DONE_AND_RETURN(-200);
        }
        narg++;
        //sq_checkstack(v, 1, "too many arguments");
    }
endwhile:

    /* do the call */
    //nres = strlen(sig);  /* number of expected results */
    if (sq_call(v, narg, *sig, SQTrue) != SQ_OK)  /* do the call */
        DONE_AND_RETURN(-300);

    /* retrieve results */
    //nres = -nres;  /* stack index of first result */
    if (*sig)    /* get results */
    {
        SQObjectPtr &o = stack_get(v,-1);
        switch (*sig)
        {

        case _SC('d'):  /* double result */
            if (!sq_isnumeric(o))  DONE_AND_RETURN(-1000);
            *va_arg(vl, double *) = tofloat(o);
            break;

        case _SC('i'):  /* int result */
            if (!sq_isnumeric(o))  DONE_AND_RETURN(-1100);
            *va_arg(vl, int *) = tointeger(o);
            break;
        //string do not work with stack reset
        case _SC('s'):  /* string result */
            if(reset_stack) DONE_AND_RETURN(-1250);
            if (!sq_isstring(o)) DONE_AND_RETURN(-1200);
            *va_arg(vl, const SQChar **) = _stringval(o);
            break;
        case _SC('b'):  /* bool result */
            if (!sq_isbool(o)) DONE_AND_RETURN(-1300);
            *va_arg(vl, int *) = tointeger(o);
            break;

        case _SC('p'):  /* user pointer result */
            if (!sq_isuserpointer(o)) DONE_AND_RETURN(-1400);
            *va_arg(vl, void **) = _userpointer(o);
            break;

        default:
            DONE_AND_RETURN(-500);
        }
        //nres++;
    }
done_and_return:
    if(reset_stack) sq_settop(v, top);
    return ret_val;
}

SQRESULT sq_call_va(HSQUIRRELVM v, SQBool reset_stack, SQInteger idx, const SQChar *func, SQInteger idx_this, const SQChar *sig, ...)
{
    va_list vl;
    va_start(vl, sig);
    SQRESULT ret_val = sq_call_va_vl(v, reset_stack, idx, func, idx_this, sig, vl);
    va_end(vl);
    return ret_val;
}

SQRESULT sq_checkoption (HSQUIRRELVM v, SQInteger narg, const SQChar *def,
                                 const SQChar *const lst[]) {
  const SQChar *name;
  if(def && sq_gettop(v) >= narg && sq_getstring(v, narg, &name) != SQ_OK) name = def;
  else if(sq_getstring(v, narg, &name) != SQ_OK) return SQ_ERROR;
  int i;
  for (i=0; lst[i]; i++)
    if (scstrcmp(lst[i], name) == 0)
      return i;
  return sq_throwerror(v, _SC("invalid option [%d] [%s]"), narg-1, name);
}
