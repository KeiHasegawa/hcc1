#include "stdafx.h"
#include "c_core.h"
#include "parse.h"
#include "misc.h"
#include "yy.h"
#include "c_y.h"

namespace c_compiler { namespace conversion {
    namespace arithmetic_impl {
      const type* match(const type*, var**, var**);
      const type* match(var**, var**);
    }
} } // end of namespace conversion and c_compiler

const c_compiler::type*
c_compiler::conversion::arithmetic_impl::match(const type* Tx, var** y, var** z)
{
  const type* Ty = (*y)->m_type;
  const type* Tz = (*z)->m_type;
  Tx = Tx->unqualified();
  Ty = Ty->unqualified();
  Tz = Tz->unqualified();
  if (Tx == Ty) {
    *z = (*z)->cast(Tx);
    return Tx;
  }
  if (Tx == Tz) {
    *y = (*y)->cast(Tx);
    return Tx;
  }
  return 0;
}

const c_compiler::type*
c_compiler::conversion::arithmetic_impl::match(var** y, var** z)
{
  const type* Ty = (*y)->m_type;
  const type* Tz = (*z)->m_type;
  Ty = Ty->unqualified();
  Tz = Tz->unqualified();
  const type* Ta = long_type::create();
  const type* Tb = uint_type::create();
  if (Ty->m_id == type::LONG && Tz->m_id == type::UINT) {
    if ( Ta->size() >= 2 * Tb->size() ){
      *z = (*z)->cast(Ta);
      return Ta;
    }
    else {
      const type* Tx = ulong_type::create();
      *y = (*y)->cast(Tx);
      *z = (*z)->cast(Tx);
      return Tx;
    }
  }
  if (Tz->m_id == type::LONG && Ty->m_id == type::UINT) {
    if ( Ta->size() >= 2 * Tb->size() ){
      *y = (*y)->cast(Ta);
      return Ta;
    }
    else {
      const type* Tx = ulong_type::create();
      *y = (*y)->cast(Tx);
      *z = (*z)->cast(Tx);
      return Tx;
    }
  }
  return 0;
}

const c_compiler::type* c_compiler::conversion::arithmetic(var** y, var** z)
{
  using namespace arithmetic_impl;
  const type* Ty = (*y)->m_type;
  const type* Tz = (*z)->m_type;
  if ( !Ty->arithmetic() || !Tz->arithmetic() )
    return 0;
  *y = (*y)->promotion();
  *z = (*z)->promotion();
  { const type* Tx = long_double_type::create(); if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = double_type::create();      if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = float_type::create();       if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = ulong_long_type::create();  if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = long_long_type::create();   if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = ulong_type::create();       if ( match(Tx,y,z) ) return Tx; }
  if ( const type* Tx = match(y,z) ) return Tx;
  { const type* Tx = long_type::create();        if ( match(Tx,y,z) ) return Tx; }
  { const type* Tx = uint_type::create();        if ( match(Tx,y,z) ) return Tx; }
  return int_type::create();
}

namespace c_compiler { namespace var_impl {
  var* mul(var*, var*);
  var* opt_mul(var*, var*);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::expr::binary(int op, var* a, var* b)
{
  decl::check(b);
  var* y = a->rvalue();
  var* z = b->rvalue();
  y = y->promotion();
  z = z->promotion();
  if (op != LSH_MK && op != RSH_MK) {
    const type* T = conversion::arithmetic(&y,&z);
    if (!T) {
      switch (op) {
      case '*': case '/': case '%': case '&': case '^': case '|':
        using namespace error::expr::binary;
        invalid(parse::position, op, y->m_type, z->m_type);
      }
    }
  }
  switch (op) {
  case '*': return y->mul(z);
  case '/': return y->div(z);
  case '%': return y->mod(z);
  case '+': return y->add(z);
  case '-': return y->sub(z);
  case LSH_MK: return y->lsh(z);
  case RSH_MK: return y->rsh(z);
  case '<': return y->lt(z);
  case '>': return y->gt(z);
  case LESSEQ_MK: return y->le(z);
  case GREATEREQ_MK: return y->ge(z);
  case EQUAL_MK: return y->eq(z);
  case NOTEQ_MK: return y->ne(z);
  case '&': return y->bit_and(z);
  case '^': return y->bit_xor(z);
  default:
    assert(op == '|');
    return y->bit_or(z);
  }
 }

c_compiler::var* c_compiler::var_impl::mul(var* y, var* z)
{
  const type* Ty = y->m_type;
  const type* Tz = z->m_type;
  const type* Tx = Ty->unqualified();
  if (!Ty->arithmetic() || !Tz->arithmetic())
    Tx = int_type::create();
  if ( var* x = opt_mul(y,z) )
    return x;
  if ( var* x = opt_mul(z,y) )
    return x;

  var* x = new var(Tx);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new mul3ac(x,y,z));
  return x;
}

namespace c_compiler { namespace var_impl {
    int log2(unsigned __int64);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::var_impl::opt_mul(var* y, var* z)
{
  if ( !z->isconstant() )
    return 0;
  if ( z->zero() )
    return z;
  const type* T = z->m_type;
  T = T->unqualified();
  if (!T->integer() || T->size() > sizeof(int))
    return 0;
  __int64 n = z->value();
  int m = log2(n);
  if ( m == -1 )
    return 0;
  var* x = new var(T);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  var* pos = integer::create(m);
  switch ( m ){
  case 0: code.push_back(new assign3ac(x,y)); break;
  case 1: code.push_back(new add3ac(x,y,y)); break;
  default: code.push_back(new lsh3ac(x,y,pos)); break;
  }
  return x;
}

int c_compiler::var_impl::log2(unsigned __int64 n)
{
  int i;
  for ( i = 0 ; n ; n >>= 1, ++i ){
    if ( n & 1 ){
      n &= ~1;
      break;
    }
  }
  return n ? -1 : i;
}

c_compiler::var* c_compiler::var::mul(var* z){ return var_impl::mul(this,z); }
c_compiler::var* c_compiler::var::mulr(constant<int>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<unsigned int>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<long int>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<unsigned long int>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<__int64>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<unsigned __int64>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<float>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<double>* y){ return var_impl::mul(y,this); }
c_compiler::var* c_compiler::var::mulr(constant<long double>* y){ return var_impl::mul(y,this); }

namespace c_compiler { namespace constant_impl {
  template<class V> var* mul(constant<V>* y, constant<V>* z)
  {
    usr::flag_t fy = y->m_flag;
    if (fy & usr::CONST_PTR)
      return var_impl::mul(y, z);
    usr::flag_t fz = z->m_flag;
    if (fz & usr::CONST_PTR)
      return var_impl::mul(y, z);
    usr* ret = integer::create(y->m_value * z->m_value);
    if (const type* T = SUB_CONST_LONG_impl::propagation(y, z))
      ret->m_type = T, ret->m_flag = usr::SUB_CONST_LONG;
    return ret; 
  }
  template<class A, class B> var* fop3(constant<A>* y, constant<B>* z,
                                       void (*pf)(unsigned char*, const unsigned char*))
  {
    using namespace std;
    int sz = long_double_type::create()->size();
    const type* Ty = y->m_type;
    const type* Tz = z->m_type;
    Ty = Ty->unqualified();
    Tz = Tz->unqualified();
    if (Ty->m_id == type::LONG_DOUBLE) {
      unsigned char* p = new unsigned char[sz];
      constant<long double>* yy = reinterpret_cast<constant<long double>*>(y);
      memcpy(p,yy->b,sz);
      if (Tz->m_id == type::LONG_DOUBLE) {
        constant<long double>* zz = reinterpret_cast<constant<long double>*>(z);
        (*pf)(p,zz->b);
        return floating::create(p);
      }
      auto_ptr<unsigned char> q = auto_ptr<unsigned char>(new unsigned char[sz]);
      (*generator::long_double->from_double)(q.get(),z->m_value);
      (*pf)(p,q.get());
      return floating::create(p);
    }
    if (Tz->m_id == type::LONG_DOUBLE) {
      unsigned char* p = new unsigned char[sz];
      (*generator::long_double->from_double)(p,y->m_value);
      constant<long double>* zz = reinterpret_cast<constant<long double>*>(z);
      (*pf)(p,zz->b);
      return floating::create(p);
    }
    return 0;
  }
} } // end of namespace constant_impl and c_compiler

namespace c_compiler {
  var* constant<int>::mulr(constant<int>* y)
  { return integer::create(y->m_value * m_value); }
  var* constant<unsigned int>::mulr(constant<unsigned int>* y)
  { return integer::create(y->m_value * m_value); }
  var* constant<long int>::mulr(constant<long int>* y)
  { return integer::create(y->m_value * m_value); }
  var* constant<unsigned long int>::mulr(constant<unsigned long int>* y)
  { return integer::create(y->m_value * m_value); }
  var* constant<__int64>::mulr(constant<__int64>* y)
  { return constant_impl::mul(y, this); }
  var* constant<unsigned __int64>::mulr(constant<unsigned __int64>* y)
  { return constant_impl::mul(y,this); }
  var* constant<float>::mulr(constant<float>* y)
  { return floating::create(y->m_value * m_value); }
  var* constant<double>::mulr(constant<double>* y)
  { return floating::create(y->m_value * m_value); }
  var* c_compiler::constant<long double>::mulr(constant<long double>* y)
  {
    constant<long double>* z = this;
    if ( generator::long_double ){
      using namespace constant_impl;
      if ( var* v = fop3(y, z, generator::long_double->mul) )
        return v;
    }
    return floating::create(y->m_value * z->m_value);
  }
} // end of namespace c_compiler


namespace c_compiler { namespace var_impl {
  var* div(var*, var*);
  var* opt_div(var*, var*);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::var_impl::div(var* y, var* z)
{
  const type* Ty = y->m_type;
  const type* Tz = z->m_type;
  const type* Tx = Ty->unqualified();
  if (!Ty->arithmetic() || !Tz->arithmetic())
    Tx = int_type::create();
  if ( var* x = opt_div(y,z) )
    return x;
  var* x = new var(Tx);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new div3ac(x,y,z));
  return x;
}

c_compiler::var* c_compiler::var_impl::opt_div(var* y, var* z)
{
  if ( !z->isconstant() )
    return 0;
  const type* T = z->m_type;
  if ( z->zero() ){
    if ( T->integer() ){
      using namespace warning;
      zero_division(parse::position);
    }
    return 0;
  }
  if ( !T->integer() || T->size() > sizeof(int) )
    return 0;
  int n = z->value();
  n = log2(n);
  if ( n == -1 )
    return 0;
  var* x = new var(T);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  var* pos = integer::create(n);
  switch ( n ){
  case 0: code.push_back(new assign3ac(x,y)); break;
  default: code.push_back(new rsh3ac(x,y,pos)); break;
  }
  return x;
}

c_compiler::var* c_compiler::var::div(var* z){ return var_impl::div(this,z); }
c_compiler::var* c_compiler::var::divr(constant<int>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<unsigned int>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<long int>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<unsigned long int>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<__int64>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<unsigned __int64>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<float>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<double>* y){ return var_impl::div(y,this); }
c_compiler::var* c_compiler::var::divr(constant<long double>* y){ return var_impl::div(y,this); }

namespace c_compiler { namespace constant_impl {
  template<class V> var* div(constant<V>* y, constant<V>* z)
  {
    usr::flag_t fy = y->m_flag;
    if (fy & usr::CONST_PTR)
      return var_impl::div(y,z);
    usr::flag_t fz = z->m_flag;
    if (fz & usr::CONST_PTR)
      return var_impl::div(y,z);
    if (!z->m_value)
      return var_impl::div(y, z);
    usr* ret = integer::create(y->m_value / z->m_value);
    if (const type* T = SUB_CONST_LONG_impl::propagation(y, z))
      ret->m_type = T, ret->m_flag = usr::SUB_CONST_LONG;
    return ret;
  }
  template<class V> var* fdiv(constant<V>* y, constant<V>* z)
  {
    if (!z->m_value)
      return var_impl::div(y, z);
    return floating::create(y->m_value / z->m_value);
  }
  template<> var* fdiv(constant<long double>* y, constant<long double>* z)
  {
    if ( generator::long_double ){
      if (!z->b)
        return var_impl::div(y, z);
      if ( var* v = fop3(y,z,generator::long_double->div) )
        return v;
    }
    if (!z->m_value)
      return var_impl::div(y, z);
    return floating::create(y->m_value / z->m_value);
  }
} } // end of namespace constant_impl and c_compiler

namespace c_compiler {
  var* constant<int>::divr(constant<int>* y)
  { return constant_impl::div(y,this); }
  var* constant<unsigned int>::divr(constant<unsigned int>* y)
  { return constant_impl::div(y,this); }
  var* constant<long int>::divr(constant<long int>* y)
  { return constant_impl::div(y,this); }
  var* constant<unsigned long int>::divr(constant<unsigned long int>* y)
  { return constant_impl::div(y,this); }
  var* constant<__int64>::divr(constant<__int64>* y)
  { return constant_impl::div(y,this); }
  var* constant<unsigned __int64>::divr(constant<unsigned __int64>* y)
  { return constant_impl::div(y,this); }
  var* constant<float>::divr(constant<float>* y)
  { return constant_impl::fdiv(y,this); }
  var* constant<double>::divr(constant<double>* y)
  { return constant_impl::fdiv(y,this); }
  var* constant<long double>::divr(constant<long double>* y)
  { return constant_impl::fdiv(y,this); }
} // end of namesapce c_compiler

namespace c_compiler { namespace var_impl {
  var* mod(var*, var*);
  var* opt_mod(var*, var*);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::var_impl::mod(var* y, var* z)
{
  const type* Ty = y->m_type;
  const type* Tz = z->m_type;
  const type* Tx = Ty->unqualified();
  if (Ty->arithmetic() && Tz->arithmetic()) {
    if (!Ty->integer() || !Tz->integer()) {
      using namespace error::expr::binary;
      invalid(parse::position,'%', Ty, Tz);
      Tx = int_type::create();
    }
  }
  else
    Tx = int_type::create();
  if ( var* x = opt_mod(y,z) )
    return x;
  var* x = new var(Tx);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new mod3ac(x,y,z));
  return x;
}

c_compiler::var* c_compiler::var_impl::opt_mod(var* y, var* z)
{
  if ( !z->isconstant() )
    return 0;
  const type* T = z->m_type;
  if ( z->zero() ){
    if ( T->integer() ){
      using namespace warning;
      zero_division(parse::position);
    }
    return 0;
  }
  if ( !T->integer() || T->size() > sizeof(int) )
    return 0;
  int n = z->value();
  n = log2(n);
  if ( n == -1 )
    return 0;
  var* x = new var(T);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  if ( n ){
    var* mask = integer::create((1 << n) - 1);
    code.push_back(new and3ac(x,y,mask));
  }
  else {
    var* zero = integer::create(0);
    code.push_back(new assign3ac(x,zero));
  }
  return x;
}

c_compiler::var* c_compiler::var::mod(var* z){ return var_impl::mod(this,z); }
c_compiler::var* c_compiler::var::modr(constant<int>* y){ return var_impl::mod(y,this); }
c_compiler::var* c_compiler::var::modr(constant<unsigned int>* y){ return var_impl::mod(y,this); }
c_compiler::var* c_compiler::var::modr(constant<long int>* y){ return var_impl::mod(y,this); }
c_compiler::var* c_compiler::var::modr(constant<unsigned long int>* y){ return var_impl::mod(y,this); }
c_compiler::var* c_compiler::var::modr(constant<__int64>* y){ return var_impl::mod(y,this); }
c_compiler::var* c_compiler::var::modr(constant<unsigned __int64>* y){ return var_impl::mod(y,this); }

namespace c_compiler { namespace constant_impl {
  template<class V> var* mod(constant<V>* y, constant<V>* z)
  {
    usr::flag_t fy = y->m_flag;
    if (fy & usr::CONST_PTR)
      return var_impl::mod(y,z);
    usr::flag_t fz = z->m_flag;
    if (fz & usr::CONST_PTR)
      return var_impl::mod(y,z);    
    if (!z->m_value)
      return var_impl::mod(y, z);
    usr* ret = integer::create(y->m_value % z->m_value);
    if (const type* T = SUB_CONST_LONG_impl::propagation(y, z))
      ret->m_type = T, ret->m_flag = usr::SUB_CONST_LONG;
    return ret;
  }
} } // end of namespace constant_impl and c_compiler

namespace c_compiler {
  var* constant<int>::modr(constant<int>* y)
  { return constant_impl::mod(y,this); }
  var* constant<unsigned int>::modr(constant<unsigned int>* y)
  { return constant_impl::mod(y,this); }
  var* constant<long int>::modr(constant<long int>* y)
  { return constant_impl::mod(y,this); }
  var* constant<unsigned long int>::modr(constant<unsigned long int>* y)
  { return constant_impl::mod(y,this); }
  var* constant<__int64>::modr(constant<__int64>* y)
  { return constant_impl::mod(y,this); }
  var* constant<unsigned __int64>::modr(constant<unsigned __int64>* y)
  { return constant_impl::mod(y,this); }
} // end of namespace c_compiler

namespace c_compiler { namespace var_impl {
  var* add(var*, var*);
  var* pointer_integer(int, var* ,var*);
  var* opt_add(var*, var*);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::var_impl::add(var* y, var* z)
{
  using namespace std;
  if ( var* r = pointer_integer('+',y,z) )
    return r;
  if ( var* r = pointer_integer('+',z,y) )
    return r;
  const type* Ty = y->m_type;
  const type* Tz = z->m_type;
  const type* Tx = Ty->unqualified();
  if (!Ty->arithmetic() || !Tz->arithmetic()) {
    using namespace error::expr::binary;
    invalid(parse::position,'+', Ty, Tz);
    Tx = int_type::create();
  }
  if ( var* x = opt_add(y,z) )
    return x;
  if ( var* x = opt_add(z,y) )
    return x;

  var* x = new var(Tx);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new add3ac(x,y,z));
  return x;
}

c_compiler::var* c_compiler::var_impl::pointer_integer(int op, var* y, var* z)
{
  using namespace std;
  const type* Ty = y->m_type;
  Ty = Ty->unqualified();
  typedef const pointer_type PT;
  PT* pt = Ty->m_id == type::POINTER ? static_cast<PT*>(Ty) : 0;
  if ( !pt )
    return 0;
  const type* Tz = z->m_type;
  if ( !Tz->integer() )
    return 0;
  const type* T = pt->referenced_type();
  T = T->complete_type();
  var* size = T->vsize();
  if ( !size ){
    int n = T->size();
    if ( !n ){
      using namespace error::expr::binary;
      invalid(parse::position,op,pt,Tz);
      n = 1;
    }
    size = integer::create(n);
  }
  z = expr::binary('*', z, size);
  if ( var* x = opt_add(y,z) )
    return x;
  var* x = new var(Ty);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  if ( op == '+' )
    code.push_back(new add3ac(x,y,z));
  else
    code.push_back(new sub3ac(x,y,z));
  return x;
}

c_compiler::var* c_compiler::var_impl::opt_add(var* y, var* z)
{
  if ( !z->isconstant() )
    return 0;
  if ( !z->zero() )
    return 0;
  const type* T = y->m_type;
  var* x = new var(T);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new assign3ac(x,y));
  return x;
}

c_compiler::var* c_compiler::var::add(var* z){ return var_impl::add(this,z); }
c_compiler::var* c_compiler::var::addr(constant<int>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<unsigned int>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<long int>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<unsigned long int>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<__int64>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<unsigned __int64>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<float>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<double>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<long double>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(constant<void*>* y){ return var_impl::add(y,this); }
c_compiler::var* c_compiler::var::addr(addrof* y){ return var_impl::add(y,this); }

namespace c_compiler { namespace constant_impl {
  template<class C> var* padd(constant<__int64>* y, constant<C>* z)
  {
    const type* T = y->m_type;
    assert(T->m_id == type::POINTER);
    typedef const pointer_type* PT;
    PT pt = static_cast<PT>(T);
    int psz = pt->size();
    assert(sizeof(void*) < psz);
    assert(psz == sizeof(__int64));
    T = pt->referenced_type();
    int size = T->size();
    if ( !size )
      return var_impl::add(y,z);
    __int64 v = y->m_value;
    int n = z->m_value;
    v += size * n;
    return pointer::create(pt, v);
  }
  template<class A, class B> var* add(constant<A>* y, constant<B>* z)
  {
    const type* Ty = y->m_type;
    const type* Tz = z->m_type;
    usr::flag_t fy = y->m_flag;
    usr::flag_t fz = z->m_flag;
    if (Ty->integer() && Tz->integer()) {
      usr* ret = integer::create(y->m_value + z->m_value);
      if (const type* T = SUB_CONST_LONG_impl::propagation(y, z))
        ret->m_type = T, ret->m_flag = usr::SUB_CONST_LONG;
      return ret;
    }
    if (fy & usr::CONST_PTR) {
      if (Tz->integer()) {
        constant<__int64>* yy = reinterpret_cast<constant<__int64>*>(y);
        return padd(yy, z);
      }
      else
        return var_impl::add(y,z);
    }
    else {
      assert(fz & usr::CONST_PTR);
      if (Ty->integer()) {
        constant<__int64>* zz = reinterpret_cast<constant<__int64>*>(z);
        return padd(zz, y);
      }
      else
        return var_impl::add(y,z);
    }
  }
  template<class A, class B> var* fadd3(constant<A>* y, constant<B>* z)
  {
    if ( generator::long_double ){
      if ( var* v = fop3(y,z,generator::long_double->add) )
        return v;
    }
    return floating::create(y->m_value + z->m_value); 
  }
  template<class C> var* padd(constant<void*>* y, constant<C>* z)
  {
    char* p = reinterpret_cast<char*>(y->m_value);
    int n = z->m_value;
    const type* T = y->m_type;
    typedef const pointer_type PT;
    assert(T->m_id == type::POINTER);
    PT* pt = static_cast<PT*>(T);
    T = pt->referenced_type();
    int size = T->size();
    if ( !size )
      return var_impl::add(y,z);
    p += size * n;
    if (sizeof(void*) >= pt->size())
      return pointer::create(pt,(void*)p);
    else
      return pointer::create(pt,(__int64)p);
  }
} } // end of namespace constant_impl and c_compiler

namespace c_compiler {
  var* constant<int>::addr(constant<int>* y)
  { return constant_impl::add(y,this); }
  var* constant<unsigned int>::addr(constant<unsigned int>* y)
  { return constant_impl::add(y,this); }
  var* constant<long int>::addr(constant<long int>* y)
  { return constant_impl::add(y,this); }
  var* constant<unsigned long int>::addr(constant<unsigned long int>* y)
  { return constant_impl::add(y,this); }
  var* constant<__int64>::addr(constant<__int64>* y)
  { return constant_impl::add(y,this); }
  var* constant<unsigned __int64>::addr(constant<unsigned __int64>* y)
  { return constant_impl::add(y,this); }
  var* constant<int>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<unsigned int>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<long int>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<unsigned long int>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<__int64>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<unsigned __int64>::addr(constant<void*>* y)
  { return constant_impl::padd(y,this); }
  var* constant<float>::addr(constant<float>* y)
  { return constant_impl::fadd3(y,this); }
  var* constant<double>::addr(constant<double>* y)
  { return constant_impl::fadd3(y,this); }
  var* constant<long double>::addr(constant<long double>* y)
  { return constant_impl::fadd3(y,this); }
  var* constant<void*>::addr(constant<int>* y)
  { return constant_impl::padd(this,y); }
  var* constant<void*>::addr(constant<unsigned int>* y)
  { return constant_impl::padd(this,y); }
  var* constant<void*>::addr(constant<long int>* y)
  { return constant_impl::padd(this,y); }
  var* constant<void*>::addr(constant<unsigned long int>* y)
  { return constant_impl::padd(this,y); }
  var* constant<void*>::addr(constant<__int64>* y)
  { return constant_impl::padd(this,y); }
  var* constant<void*>::addr(constant<unsigned __int64>* y)
  { return constant_impl::padd(this,y); }
} // end of namespace c_compiler

namespace c_compiler { namespace addrof_impl {
  template<class T> var* add(addrof* y, constant<T>* z)
  {
    typedef const pointer_type PT;
    PT* pt = static_cast<PT*>(y->m_type);
    const type* type = pt->referenced_type();
    int n = type->size();
    if ( !n )
      return var_impl::add(y,z);
    int offset = y->m_offset + n * z->m_value;
    if ( usr* u = y->m_ref->usr_cast() ){
      usr::flag_t& flag = u->m_flag;
      if ( flag & usr::REGISTER ){
        using namespace error::expr::address;
        implicit::specified_register(parse::position,u);
        flag = usr::flag_t(flag & ~usr::REGISTER);
      }
    }
    var* ret = new addrof(pt,y->m_ref,offset);
    garbage.push_back(ret);
    return ret;
  }
} } // end of namespace addrof_impl and c_compiler

namespace c_compiler {
  var* constant<int>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
  var* constant<unsigned int>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
  var* constant<long int>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
  var* constant<unsigned long int>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
  var* constant<__int64>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
  var* constant<unsigned __int64>::addr(addrof* y)
  { return addrof_impl::add(y,this); }
} // end of namespace c_compiler

c_compiler::var* c_compiler::addrof::addr(constant<int>* y){ return addrof_impl::add(this,y); }
c_compiler::var* c_compiler::addrof::addr(constant<unsigned int>* y){ return addrof_impl::add(this,y); }
c_compiler::var* c_compiler::addrof::addr(constant<long int>* y){ return addrof_impl::add(this,y); }
c_compiler::var* c_compiler::addrof::addr(constant<unsigned long int>* y){ return addrof_impl::add(this,y); }
c_compiler::var* c_compiler::addrof::addr(constant<__int64>* y){ return addrof_impl::add(this,y); }
c_compiler::var* c_compiler::addrof::addr(constant<unsigned __int64>* y){ return addrof_impl::add(this,y); }

namespace c_compiler { namespace var_impl {
  var* sub(var*, var*);
  var* pointer_pointer(var*, var*);
  var* opt_sub(var*, var*);
} } // end of namespace var_impl and c_compiler

c_compiler::var* c_compiler::var_impl::sub(var* y, var* z)
{
  if (var* r = pointer_pointer(y,z))
    return r;
  if (var* r = pointer_integer('-',y,z))
    return r;
  const type* Ty = y->m_type;
  const type* Tz = z->m_type;
  const type* Tx = Ty->unqualified();
  if (!Ty->arithmetic() || !Tz->arithmetic()) {
    using namespace error::expr::binary;
    invalid(parse::position,'-', Ty, Tz);
    Tx = int_type::create();
  }
  if ( var* x = opt_sub(y,z) )
    return x;

  var* x = new var(Tx);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new sub3ac(x,y,z));
  return x;
}

namespace c_compiler {
  namespace pointer_pointer_impl {
    const type* result_type()
    {
      switch (generator::ptrdiff_type) {
      case type::INT:
        return int_type::create();
      case type::LONG:
        return long_type::create();
      default:
        return long_long_type::create();
      }
    }
  } // end of namespace pointer_pointer_impl
} // end of namespace c_compiler

c_compiler::var* c_compiler::var_impl::pointer_pointer(var* y, var* z)
{
  using namespace std;
  typedef const pointer_type PT;
  const type* Ty = y->m_type;
  Ty = Ty->unqualified();
  PT* py = Ty->m_id == type::POINTER ? static_cast<PT*>(Ty) : 0;
  if ( !py )
    return 0;
  const type* Tz = z->m_type;
  Tz = Tz->unqualified();
  PT* pz = Tz->m_id == type::POINTER ? static_cast<PT*>(Tz) : 0;
  if ( !pz )
    return 0;
  Ty = py->referenced_type();
  Ty = Ty->unqualified();
  Tz = pz->referenced_type();
  Tz = Tz->unqualified();
  if (!compatible(Ty, Tz)) {
    using namespace error::expr::binary;
    invalid_pointer(parse::position, py, pz);
  }

  const type* T = pointer_pointer_impl::result_type();
  var* x = new var(T);
  var* t = new var(T);
  if (scope::current->m_id == scope::BLOCK) {
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x), b->m_vars.push_back(t);
  }
  else
    garbage.push_back(x), garbage.push_back(t);

  if ( var* s = Ty->vsize() ){
    code.push_back(new sub3ac(t,y,z));
    const type* Ts = s->m_type;
    Ts = Ts->unqualified();
    if (T->m_id == Ts->m_id)
      code.push_back(new div3ac(x,t,s));
    else {
      var* ss = new var(t->m_type);
      assert(scope::current->m_id == scope::BLOCK);
      block* b = static_cast<block*>(scope::current);
      b->m_vars.push_back(ss);
      code.push_back(new cast3ac(ss,s,t->m_type));
      code.push_back(new div3ac(x,t,ss));
    }
    return x;
  }
  int n = Ty->size();
  if ( !n ){
    using namespace error::expr::binary;
    invalid(parse::position,'-',py,pz);
    n = 1;
  }
  int m = log2(n);
  if ( !m ){
    code.push_back(new sub3ac(x,y,z));
    return x;
  }
  if ( m != -1 ){
    var* s = integer::create(m);
    code.push_back(new sub3ac(t,y,z));
    code.push_back(new rsh3ac(x,t,s));
    return x;
  }

  var* s = integer::create((long)n);
  code.push_back(new sub3ac(t,y,z));
  code.push_back(new div3ac(x,t,s));
  return x;
}

c_compiler::var* c_compiler::var_impl::opt_sub(var* y, var* z)
{
  if ( var* x = opt_add(y,z) )
    return x;
  if ( !y->isconstant() )
    return 0;
  if ( !y->zero() )
    return 0;
  const type* T = y->m_type;
  var* x = new var(T);
  if ( scope::current->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(scope::current);
    b->m_vars.push_back(x);
  }
  else
    garbage.push_back(x);
  code.push_back(new uminus3ac(x,z));
  return x;
}

c_compiler::var* c_compiler::var::sub(var* z){ return var_impl::sub(this,z); }
c_compiler::var* c_compiler::var::subr(constant<int>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<unsigned int>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<long int>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<unsigned long int>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<__int64>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<unsigned __int64>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<void*>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<float>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<double>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(constant<long double>* y){ return var_impl::sub(y,this); }
c_compiler::var* c_compiler::var::subr(addrof* y){ return var_impl::sub(y,this); }

c_compiler::var* c_compiler::addrof::subr(addrof* y)
{
  using namespace std;
  if ( y->m_ref == m_ref ){
    const type* T = m_type;
    typedef const pointer_type PT;
    PT* pt = static_cast<PT*>(T);
    T = pt->referenced_type();
    long int n = T->size();
    if ( !n ){
      using namespace error::expr::binary;
      invalid(parse::position,'-',T,T);
      n = 1;
    }
    n = (y->m_offset - m_offset)/n;
    return integer::create(n);
  }
  else
    return var_impl::sub(y,this);
}

namespace c_compiler { namespace addrof_impl {
  template<class T> var* sub(addrof* y, constant<T>* z)
  {
    typedef const pointer_type PT;
    PT* pt = static_cast<PT*>(y->m_type);
    const type* type = pt->referenced_type();
    int n = type->size();
    if ( !n )
      return var_impl::sub(y,z);
    int offset = y->m_offset - n * z->m_value;
    var* ret = new addrof(pt,y->m_ref,offset);
    garbage.push_back(ret);
    return ret;
  }
} } // end of namespace addrof_impl and c_compiler

namespace c_compiler {
  var* constant<int>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
  var* constant<unsigned int>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
  var* constant<long int>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
  var* constant<unsigned long int>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
  var* constant<__int64>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
  var* constant<unsigned __int64>::subr(addrof* y)
  { return addrof_impl::sub(y,this); }
} // end of namespace c_compiler

namespace c_compiler { namespace constant_impl {
  template<class C> var* psub(constant<__int64>* y, constant<C>* z)
  {
    const type* T = y->m_type;
    typedef const pointer_type PT;
    assert(T->m_id == type::POINTER);
    PT* pt = static_cast<PT*>(T);
    assert(sizeof(void*) < pt->size());
    T = pt->referenced_type();
    int size = T->size();
    if ( !size )
      return var_impl::sub(y,z);
    __int64 v = y->m_value;    
    int n = z->m_value;
    v -= size * n;
    return pointer::create(pt,v);
  }
  var* pointer_pointer(constant<__int64>* y, constant<__int64>* z);
  template<class A, class B> var* sub(constant<A>* y, constant<B>* z)
  {
    const type* Ty = y->m_type;
    const type* Tz = z->m_type;
    usr::flag_t fy = y->m_flag;
    usr::flag_t fz = z->m_flag;
    if (Ty->integer() && Tz->integer()) {
      usr* ret = integer::create(y->m_value - z->m_value);
      if (const type* T = SUB_CONST_LONG_impl::propagation(y, z))
        ret->m_type = T, ret->m_flag = usr::SUB_CONST_LONG;
      return ret;
    }
    if (fy & usr::CONST_PTR) {
      constant<__int64>* yy = reinterpret_cast<constant<__int64>*>(y);
      if (Tz->integer())
        return psub(yy, z);
      assert(fz & usr::CONST_PTR);
      constant<__int64>* zz = reinterpret_cast<constant<__int64>*>(z);
      return pointer_pointer(yy, zz);
    }
    else
      return var_impl::sub(y, z);
  }
  template<class A, class B> var* fsub3(constant<A>* y, constant<B>* z)
  {
    if ( generator::long_double ){
      if ( var* v = fop3(y,z,generator::long_double->sub) )
        return v;
    }
    return floating::create(y->m_value - z->m_value);
  }
  template<class C> var* psub(constant<void*>* y, constant<C>* z)
  {
    char* p = reinterpret_cast<char*>(y->m_value);
    int n = z->m_value;
    const type* T = y->m_type;
    typedef const pointer_type PT;
    PT* pt = static_cast<PT*>(T);
    T = pt->referenced_type();
    int size = T->size();
    if ( !size )
      return var_impl::sub(y,z);
    p -= size * n;
    if (sizeof(void*) >= pt->size())
      return pointer::create(pt,(void*)p);
    else
      return pointer::create(pt,(__int64)p);
  }
} } // end of namespace constant_impl and c_compiler

namespace c_compiler {
  var* constant<int>::subr(constant<int>* y)
  { return constant_impl::sub(y,this); }
  var* constant<int>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<unsigned int>::subr(constant<unsigned int>* y)
  { return constant_impl::sub(y,this); }
  var* constant<unsigned int>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<long int>::subr(constant<long int>* y)
  { return constant_impl::sub(y,this); }
  var* constant<long int>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<unsigned long int>::subr(constant<unsigned long int>* y)
  { return constant_impl::sub(y,this); }
  var* constant<unsigned long int>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<__int64>::subr(constant<__int64>* y)
  { return constant_impl::sub(y,this); }
  var* constant<__int64>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<unsigned  __int64>::subr(constant<unsigned __int64>* y)
  { return constant_impl::sub(y,this); }
  var* constant<unsigned  __int64>::subr(constant<void*>* y)
  { return constant_impl::psub(y,this); }

  var* constant<float>::subr(constant<float>* y)
  { return constant_impl::fsub3(y,this); }
  var* constant<double>::subr(constant<double>* y)
  { return constant_impl::fsub3(y,this); }
  var* constant<long double>::subr(constant<long double>* y)
  { return constant_impl::fsub3(y,this); }
} // end of namespace c_compiler

c_compiler::var* c_compiler::constant<void*>::subr(constant<void*>* that)
{
  const type* Ty = that->m_type;
  const type* Tz = this->m_type;
  Ty = Ty->unqualified();
  Tz = Tz->unqualified();
  if (Ty->m_id != Tz->m_id)
    return var_impl::sub(that,this);
  typedef const pointer_type PT;
  PT* Py = static_cast<PT*>(Ty);
  PT* Pz = static_cast<PT*>(Tz);
  const type* Ry = Py->referenced_type();
  const type* Rz = Pz->referenced_type();
  Ry = Ry->unqualified();
  Rz = Rz->unqualified();
  if (!compatible(Ry, Rz))
    return var_impl::sub(that,this);
  int size = Ry->size();
  if ( !size )
    return var_impl::sub(that,this);
  __int64 y = (__int64)that->m_value;
  __int64 z = (__int64)this->m_value;
  return integer::create((long int)((y - z)/size));
}

namespace c_compiler {
  namespace constant_impl {
    var* pointer_pointer(constant<__int64>* y, constant<__int64>* z)
    {
      const type* Ty = y->m_type;
      const type* Tz = z->m_type;
      Ty = Ty->unqualified();
      Tz = Tz->unqualified();
      assert(Ty->m_id == type::POINTER);
      assert(Tz->m_id == type::POINTER);
      typedef const pointer_type PT;
      PT* Py = static_cast<PT*>(Ty);
      PT* Pz = static_cast<PT*>(Tz);
      assert(sizeof(void*) < Py->size());
      Ty = Py->referenced_type();
      Tz = Pz->referenced_type();
      Ty = Ty->unqualified();
      Tz = Tz->unqualified();
      if (!compatible(Ty, Tz))
        return var_impl::sub(y, z);
      int size = Ty->size();
      if ( !size )
        return var_impl::sub(y, z);
      __int64 vy = y->m_value;
      __int64 vz = z->m_value;
      const type* T = pointer_pointer_impl::result_type();
      if (T->size() == sizeof(long))
        return integer::create((long int)((vy - vz)/size));
      assert(T->size() == 8 && sizeof(long) == 4);
      usr* ret = integer::create((vy - vz) / size);
      ret->m_type = const_type::create(T);
      ret->m_flag = usr::SUB_CONST_LONG;
      return ret;
    }
  } // end of namespace constant_impl
} // end of namespace c_compiler
