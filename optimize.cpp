#include "stdafx.h"
#include "c_core.h"
#include "parse.h"
#include "misc.h"

namespace c_compiler { namespace optimize {
  extern void redundant_to3ac(std::vector<tac*>&);
  namespace basic_block { extern void action(const fundef* fdef, std::vector<tac*>& v); }
} } // end of namespace optimize and c_compiler

void c_compiler::optimize::action(const fundef* fdef, std::vector<tac*>& v)
{
  redundant_to3ac(v);
  if ( cmdline::bb_optimize )
    basic_block::action(fdef, v);
}

void c_compiler::optimize::redundant_to3ac(std::vector<tac*>& v)
{
  using namespace std;
  typedef vector<tac*>::iterator IT;
  for ( IT p = v.begin() ; p != v.end() ; ){
    if ( (*p)->id == tac::TO ){
      to3ac* to = static_cast<to3ac*>(*p);
      if ( to->m_goto.empty() ){
        delete *p;
        p = v.erase(p);
      }
      else
        ++p;
    }
    else
      ++p;
  }
}

namespace c_compiler { namespace optimize { namespace basic_block {
  struct info;
  void create(std::vector<tac*>&, std::vector<info*>&);
  namespace dag {
    struct action_t {
      std::vector<tac*> conv;
      std::set<var*> addr;
      std::map<var*, std::vector<std::pair<tac*, int> > > loffs;
    };
    extern int action(info*, action_t*);
  } // end of namespace dag
} } } // end of namespace basic_block, optimize and c_compiler


namespace c_compiler { namespace optimize { namespace live_var {
  extern void analize(const std::vector<basic_block::info*>&);
} } } // end of namespace live_var, optimize and c_compiler

namespace c_compiler { namespace optimize { namespace symtab {
  extern int simplify(scope*, std::vector<tac*>*);
  namespace literal {
	  extern void simplify(const std::vector<tac*>&);
  } // end of namespace literal
} } } // end of namespace symtab, optimize and c_compiler

void c_compiler::optimize::basic_block::action(const fundef* fdef, std::vector<tac*>& v)
{
  using namespace std;
  if ( cmdline::output_medium && cmdline::output_optinfo ){
    cout << "Before optimization\n";
    function_definition::dump(fdef, v);
  }
  vector<info*> bbs;
  create(v,bbs);
  live_var::analize(bbs);
  if ( cmdline::dag_optimize ){
    dag::action_t seed;
    for_each(bbs.begin(),bbs.end(),bind2nd(ptr_fun(dag::action),&seed));
    const map<var*, vector<pair<tac*,int> > >& m = seed.loffs;
    typedef map<var*, vector<pair<tac*, int> > >::const_iterator IT;
    for ( IT p = m.begin() ; p != m.end() ; ++p ){
      const vector<pair<tac*, int> >& v = p->second;
      typedef vector<pair<tac*, int> >::const_iterator X;
      for ( X q = v.begin() ; q != v.end() ; ++q )
        delete q->first;
    }
    v = seed.conv;
  }
  for_each(bbs.begin(),bbs.end(),deleter<info>());
  {
    scope* p = fundef::current->m_param;
    vector<scope*>& children = p->m_children;
    assert(children.size() == 1);
    p = children.back();
    symtab::simplify(p, &v);
  }
  if ( !function_definition::Inline::resolve::flag )
    symtab::literal::simplify(v);
}

namespace c_compiler { namespace optimize { namespace basic_block {
  extern void devide(std::set<tac**>&, const std::vector<tac*>&);
  extern info* new_obj(tac**);
  extern void build(std::vector<info*>::iterator, std::vector<info*>::iterator, tac**);
} } } // end of namespace basic_block, optimize and c_compiler

void c_compiler::optimize::basic_block::create(std::vector<tac*>& v, std::vector<info*>& bbs)
{
  using namespace std;
  if ( v.empty() )
    return;
  set<tac**> leader;
  devide(leader,v);
  transform(leader.begin(),leader.end(),back_inserter(bbs),new_obj);
  tac** tmp = &v[0] + v.size();
  build(bbs.begin(),bbs.end(),tmp);
}

void c_compiler::optimize::basic_block::devide(std::set<tac**>& leader, const std::vector<tac*>& v)
{
  using namespace std;
  assert(!v.empty());
  typedef vector<tac*>::const_iterator IT;
  IT p = v.begin();
  leader.insert(const_cast<tac**>(&*p));
  while ( p != v.end() ){
    tac::id_t id = (*p)->id;
    switch ( id ){
    case tac::TO:
      leader.insert(const_cast<tac**>(&*p));
      ++p;
      break;
    case tac::GOTO:
      if ( ++p != v.end() )
        leader.insert(const_cast<tac**>(&*p));
      break;
    default:
      ++p;
      break;
    }
  }
}

namespace c_compiler { namespace optimize { namespace basic_block {
  struct info {
    tac** m_leader;
    int m_size;
    std::vector<info*> m_follow, m_preceed;
    info(tac** leader) : m_leader(leader), m_size(0) {}
  };
} } } // end of namespace basic_block, optimize and c_compiler

c_compiler::optimize::basic_block::info*
c_compiler::optimize::basic_block::new_obj(tac** leader)
{
  return new info(leader);
}

namespace c_compiler { namespace optimize { namespace basic_block {
  bool destination(info*, tac*);
} } } // end of namespace basic_block, optimize and c_compiler

void c_compiler::optimize::basic_block::build(std::vector<info*>::iterator begin,
                                              std::vector<info*>::iterator end,
                                              tac** end3ac)
{
  std::vector<tac*> vv;
  using namespace std;
  typedef vector<info*>::const_iterator IT;
  for ( IT p = begin ; p != end ; ){
    basic_block::info* curr = *p;
    ++p;
    basic_block::info* next = p != end ? *p : 0;
    curr->m_size = next ? next->m_leader - curr->m_leader : end3ac - curr->m_leader;
    tac* last = next ? *(next->m_leader - 1) : *(end3ac-1);
    if ( last->id == tac::GOTO ){
      goto3ac* go = static_cast<goto3ac*>(last);
      tac* to = go->m_to;
      IT q = find_if(begin,end,bind2nd(ptr_fun(destination),to));
      assert(q != end);
      basic_block::info* follow = *q;
      curr->m_follow.push_back(follow);
      follow->m_preceed.push_back(curr);
      if ( go->m_op && next )
        curr->m_follow.push_back(next), next->m_preceed.push_back(curr);
    }
    else if ( next )
      curr->m_follow.push_back(next), next->m_preceed.push_back(curr);
  }
}

bool c_compiler::optimize::basic_block::destination(info* B, tac* to)
{
  return *B->m_leader == to;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  struct info;
  struct mknode_t {
    std::map<var*, dag::info*>* node;
    std::map<dag::info*, var*>* result;
    basic_block::info* B;
    action_t* pa;
    bool ret;
    mknode_t(std::map<var*, dag::info*>* n, std::map<dag::info*, var*>* r, basic_block::info* b, action_t* a)
      : node(n), result(r), B(b), pa(a), ret(false) {}
  };
  int mknode(tac**, mknode_t*);
  namespace generate {
    struct action_t {
      mknode_t* mt;
      tac** pos;
      action_t(mknode_t* m, tac** p) : mt(m), pos(p) {}
    };
    void action(action_t*);
  } // end of namespace generate
} } } } // end of namespace dag, basic_block, optimize and c_compiler

int c_compiler::optimize::basic_block::dag::action(basic_block::info* B,
  action_t* pa)
{
  using namespace std;
  tac** begin = B->m_leader;
  tac** end = begin + B->m_size;
  map<var*, dag::info*> node;
  map<dag::info*, var*> result;
  mknode_t mt(&node,&result,B,pa);
  for ( tac** p = begin ; p != end ; ++p )
    mknode(p,&mt);
  generate::action_t act(&mt,end-1);
  generate::action(&act);
  return 0;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  struct info {
    tac* m_tac;
    std::vector<var*> m_vars;
    std::vector<info*> m_parents;
    info* m_left;
    info* m_right;
    static std::vector<info*> all;
    info() : m_tac(0), m_left(0), m_right(0) { all.push_back(this); }
  };
  std::vector<dag::info*> info::all;
  info* get(tac*, std::map<var*, dag::info*>*, bool*);
} } } } // end of namespace dag, basic_block, optimize and c_compiler

int c_compiler::optimize::basic_block::dag::mknode(tac** pp, mknode_t* mt)
{
  using namespace std;
  tac* ptr = *pp;
  tac::id_t id = ptr->id;
  if ( mt->ret ){
    if ( id != tac::TO && id != tac::GOTO ){
      delete ptr;
      return 0;
    }
    mt->ret = false;
  }
  if ( id == tac::RETURN )
    mt->ret = true;
  var* y = ptr->y;
  if ( !y ){
    // Special case. return3ac, goto3ac, to3ac or asm3ac.
    dag::info* p = new dag::info;
    p->m_tac = ptr;
    return 0;
  }
  map<var*, dag::info*>* node = mt->node;
  if ( node->find(y) == node->end() ){
    dag::info* leaf = new dag::info;
    leaf->m_vars.push_back(y);
    (*node)[y] = leaf;
  }
  if ( var* z = ptr->z ){
    if ( node->find(z) == node->end() ){
      dag::info* leaf = new dag::info;
      leaf->m_vars.push_back(z);
      (*node)[z] = leaf;
    }
  }
  bool found = false;
  dag::info* n = get(ptr,node,&found);
  if ( id == tac::CALL || id == tac::INVLADDR ){
    if ( var* x = ptr->x ){
      vector<var*>& v = n->m_vars;
      v.push_back(x);
      (*node)[x] = n;
    }
    generate::action_t act(mt,pp);
    generate::action(&act);
    node->clear();
    mt->result->clear();
    return 0;
  }
  var* x = ptr->x;
  if ( !x )
    return 0;
  auto_ptr<tac> sweper(found ? ptr : 0);
  vector<var*>& v = n->m_vars;
  v.push_back(x);
  (*node)[x] = n;
  if ( id ==  tac::ADDR )
    mt->pa->addr.insert(y);
  return 0;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  bool match(info*, std::pair<tac*, std::map<var*, dag::info*>*>);
  bool roff_match_loff(info*, std::pair<tac*, std::map<var*, dag::info*>*>);
} } } } // end of namespace dag, basic_block, optimize and c_compiler

c_compiler::optimize::basic_block::dag::info*
c_compiler::optimize::basic_block::dag::get(tac* ptr, std::map<var*, dag::info*>* node, bool* found)
{
  using namespace std;
  tac::id_t id = ptr->id;
  switch ( id ){
  case tac::ASSIGN:
    {
      var* y = ptr->y;
      map<var*, info*>::const_iterator p = node->find(y);
      assert(p != node->end());
      *found = true;
      return p->second;
    }
  case tac::CALL: case tac::VAARG:
    break;
  default:
    if ( ptr->x ){
      const vector<dag::info*>& v = dag::info::all;
      vector<dag::info*>::const_iterator p =
        find_if(v.begin(),v.end(),bind2nd(ptr_fun(match),make_pair(ptr,node)));
      if ( p != v.end() ){
        *found = true;
        return *p;
      }
      if ( id == tac::ROFF ){
        p = find_if(v.begin(),v.end(),bind2nd(ptr_fun(roff_match_loff),make_pair(ptr,node)));
        if ( p != v.end() ){
          *found = true;
          return (*p)->m_right;
        }
      }
    }
    break;
  }
  info* ret = new info;
  ret->m_tac = ptr;
  map<var*, info*>::const_iterator q = node->find(ptr->y);
  assert(q != node->end());
  info* i = q->second;
  i->m_parents.push_back(ret);
  ret->m_left = i;
  if ( var* z = ptr->z ){
    map<var*, info*>::const_iterator q = node->find(z);
    assert(q != node->end());
    info* i = q->second;
    i->m_parents.push_back(ret);
    ret->m_right = i;
  }
  return ret;
}

bool
c_compiler::optimize::basic_block::dag::match(info* xinfo, std::pair<tac*, std::map<var*, dag::info*>*> arg)
{
  using namespace std;
  tac* xtac = xinfo->m_tac;
  tac* ytac = arg.first;
  if ( !xtac )
    return false;
  if ( xtac->id != ytac->id )
    return false;
  map<var*, dag::info*>* node = arg.second;
  if ( dag::info* left = xinfo->m_left ){
    var* y = ytac->y;
    map<var*, dag::info*>::const_iterator p = node->find(ytac->y);
    assert(p != node->end());
    dag::info* i = p->second;
    if ( left != i )
      return false;
  }
  if ( dag::info* right = xinfo->m_right ){
    map<var*, dag::info*>::const_iterator p = node->find(ytac->z);
    assert(p != node->end());
    dag::info* i = p->second;
    if ( right != i )
      return false;
  }
  tac::id_t id = xtac->id;
  switch ( id ){
  case tac::ROFF:
  case tac::INVRADDR:
    {
      const type* x = xtac->x->m_type;
      const type* y = ytac->x->m_type;
      return x->compatible(y);
    }
  case tac::CAST:
    {
      cast3ac* x = static_cast<cast3ac*>(xtac);
      cast3ac* y = static_cast<cast3ac*>(ytac);
      return x->m_type->compatible(y->m_type);
    }
  case tac::LOFF:
  case tac::ALLOC:
    return xtac->x == ytac->x;
  default:
    return true;
  }
}

bool
c_compiler::optimize::basic_block::dag::roff_match_loff(info* xinfo, std::pair<tac*, std::map<var*, dag::info*>*> arg)
{
  using namespace std;
  tac* roff = arg.first;
  assert(roff->id == tac::ROFF);
  tac* loff = xinfo->m_tac;
  if ( !loff )
    return false;
  if ( loff->id != tac::LOFF )
    return false;
  map<var*, dag::info*>* node = arg.second;
  var* a = roff->y;
  map<var*, dag::info*>::const_iterator p = node->find(a);
  assert(p != node->end());
  if ( xinfo != p->second )
    return false;
  var* i = roff->z;
  map<var*, dag::info*>::const_iterator q = node->find(i);
  assert(q != node->end());
  if ( xinfo->m_left != q->second )
    return false;
  const type* x = roff->x->m_type;
  const type* y = loff->z->m_type;
  return x->compatible(y);
}

namespace c_compiler { namespace optimize { namespace live_var {
  std::map<basic_block::info*, std::set<var*> > in, out, def, use;
  int prepare(basic_block::info*);
  int add_global1(basic_block::info*);
  int calc(int, basic_block::info*);
  void msg(int);
} } } // end of namespace live_var, optimize and c_compiler

namespace c_compiler { namespace optimize { namespace basic_block {
  std::map<var*, std::vector<std::pair<tac**,bool> > > live;
} } } // end of namespace basic_block, optimize and c_compiler

namespace c_compiler {namespace names {
  std::string ref(var*);
  std::string refb(optimize::basic_block::info*);
} } // end of namespace names, dump and c_compiler

void c_compiler::optimize::live_var::analize(const std::vector<basic_block::info*>& bbs)
{
  using namespace std;
  in.clear(); out.clear(); def.clear(); use.clear();
  basic_block::live.clear();
  for_each(bbs.rbegin(),bbs.rend(),prepare);
  bool b = cmdline::output_medium && cmdline::output_optinfo;
  if ( b ){
    for_each(bbs.begin(),bbs.end(),names::refb);
    c_compiler::live_var::dump("def",def);
    c_compiler::live_var::dump("use",use);
  }
  for_each(bbs.begin(),bbs.end(),add_global1);
  int i = 0;
  if ( b ) msg(i);
  while ( accumulate(bbs.begin(),bbs.end(),0,calc) )
    if ( b ) msg(++i);
  if ( b )
    c_compiler::live_var::dump("out",out);
}

void c_compiler::optimize::live_var::msg(int i)
{
  using namespace std;
  cout << "step " << i << '\n';
}

namespace c_compiler { namespace optimize { namespace live_var {
  void setup(tac**, basic_block::info*);
} } } // end of namespace live_var, optimize and c_compiler

int c_compiler::optimize::live_var::prepare(basic_block::info* B)
{
  using namespace std;
  tac** rend = B->m_leader - 1;
  tac** rbegin = rend + B->m_size;
  for ( tac** p = rbegin ; p != rend ; --p )
    setup(p,B);
  return 0;
}

void c_compiler::optimize::live_var::setup(tac** pp, basic_block::info* B)
{
  using namespace std;
  tac* p = *pp;
  typedef map<basic_block::info*, set<var*> >::iterator IT;
  if ( var* x = p->x ){
    def[B].insert(x);
    IT q = use.find(B);
    if ( q != use.end() )
      q->second.erase(x);
    basic_block::live[x].push_back(make_pair(pp,false));
  }
  if ( var* y = p->y ){
    use[B].insert(y);
    IT q = def.find(B);
    if ( q != def.end() )
      q->second.erase(y);
    basic_block::live[y].push_back(make_pair(pp,true));
  }
  if ( var* z = p->z ){
    use[B].insert(z);
    IT q = def.find(B);
    if ( q != def.end() )
      q->second.erase(z);
    basic_block::live[z].push_back(make_pair(pp,true));
  }
}

namespace c_compiler { namespace optimize { namespace live_var {
  int setout(basic_block::info*, basic_block::info*);
} } } // end of namespace live_var, optimize and c_compiler

int c_compiler::optimize::live_var::calc(int n, basic_block::info* B)
{
  using namespace std;
  // out[B] = U in[F], where F follows B
  const vector<basic_block::info*>& v = B->m_follow;
  for_each(v.begin(),v.end(),bind1st(ptr_fun(setout),B));
  bool flag = cmdline::output_medium && cmdline::output_optinfo;
  if ( flag ){
    cout << "\tout[" << names::refb(B) << "] ";
    const set<var*>& o = out[B];
    transform(o.begin(),o.end(),ostream_iterator<string>(cout," "),names::ref);
  }

  // in[B] = ( out[B] - def[B] ) U use[B]
  set<var*>& i = in[B];
  int org = i.size();
  i = out[B];
  typedef set<var*>::const_iterator IT;
  const set<var*>& d = def[B];
  for ( IT p = d.begin() ; p != d.end() ; ++p )
    i.erase(*p);
  const set<var*>& u = use[B];
  copy(u.begin(),u.end(),inserter(i,i.begin()));
  if ( flag ){
    cout << ", in[" << names::refb(B) << "] ";
    transform(i.begin(),i.end(),ostream_iterator<string>(cout," "),names::ref);
    cout << '\n';
  }
  return org != i.size() ? n + 1 : n;
}

int c_compiler::optimize::live_var::setout(basic_block::info* B, basic_block::info* S)
{
  using namespace std;
  const set<var*>& a = in[S];
  set<var*>& b = out[B];
  copy(a.begin(),a.end(),inserter(b,b.begin()));
  return 0;
}

namespace c_compiler { namespace optimize { namespace live_var {
  void add_global2(const std::set<var*>&, basic_block::info*);
  bool local(var*);
} } } // end of namespace live_var, optimize and c_compiler

int c_compiler::optimize::live_var::add_global1(basic_block::info* B)
{
  using namespace std;
  typedef map<basic_block::info*, set<var*> >::const_iterator IT;
  {
    for ( IT p = def.begin() ; p != def.end() ; ++p )
      add_global2(p->second,B);
  }
  {
    for ( IT p = use.begin() ; p != use.end() ; ++p )
      add_global2(p->second,B);
  }
  return 0;
}

void c_compiler::optimize::live_var::add_global2(const std::set<var*>& i, basic_block::info* B)
{
  using namespace std;
  set<var*>& o = out[B];
  remove_copy_if(i.begin(),i.end(),inserter(o,o.begin()),local);
}

bool c_compiler::optimize::live_var::local(var* x)
{
  if ( !x->m_scope->m_parent )
    return false;
  usr* u = x->usr_cast();
  if ( !u )
    return true;
  usr::flag flag = u->m_flag;
  usr::flag mask = usr::flag(usr::EXTERN | usr::STATIC);
  return !(flag & mask);
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  var* inorder(info*, action_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::dag::generate::action(action_t* act)
{
  using namespace std;
  vector<info*>& v = info::all;
  for_each(v.begin(),v.end(),bind2nd(ptr_fun(inorder),act));
  for_each(v.begin(),v.end(),deleter<info>());
  v.clear();
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  var* assigns(dag::info*, action_t*);
  bool liveout(var*, basic_block::info*);
  bool use_after(var*, action_t*);
  bool resident(var*, std::pair<dag::info*, std::map<var*, dag::info*>*>);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

c_compiler::var*
c_compiler::optimize::basic_block::dag::generate::inorder(dag::info* d, action_t* act)
{
  using namespace std;
  map<dag::info*, var*>& result = *act->mt->result;
  tac* ptr = d->m_tac;
  if ( !ptr )
    return result[d] = assigns(d,act);
  tac::id_t id = ptr->id;
  dag::action_t* pa = act->mt->pa;
  map<var*, vector<pair<tac*, int> > >& loffs = pa->loffs;
  vector<tac*>& conv = pa->conv;
  switch ( id ){
  case tac::LOFF:
    {
      if ( d->m_parents.empty() )
        break;
      var* x = ptr->x;
      map<var*, vector<pair<tac*, int> > >::iterator p = loffs.find(x);
      if ( p != loffs.end() ){
        const vector<pair<tac*, int> >& v = p->second;
        typedef vector<pair<tac*, int> >::const_reverse_iterator IT;
        for ( IT q = v.rbegin() ; q != v.rend() ; ++q ){
          vector<tac*>::iterator r = conv.begin() + q->second;
          conv.insert(r,q->first);
        }
        loffs.erase(p);
      }
    }
    break;
  case tac::ALLOC:
    {
      if ( !d->m_parents.empty() ){
        dag::info* parent = d->m_parents[0];
        tac* ptr = parent->m_tac;
        tac::id_t id = ptr->id;
        if ( id == tac::DEALLOC )
          return result[d] = 0;
      }
    }
    break;
  case tac::DEALLOC:
    {
      map<dag::info*, var*>::const_iterator p = result.find(d->m_left);
      assert(p != result.end());
      if ( p->second == 0 ){
        delete d->m_left->m_tac;
        delete ptr;
        return result[d] = 0;
      }
    }
    break;
  }
  if ( dag::info* i = d->m_left ){
    map<dag::info*, var*>::const_iterator p = result.find(i);
    assert(p != result.end());
    ptr->y = p->second;
  }
  if ( dag::info* i = d->m_right ){
    map<dag::info*, var*>::const_iterator p = result.find(i);
    assert(p != result.end());
    ptr->z = p->second;
  }
  int n = conv.size();
  conv.push_back(ptr);
  var* x = ptr->x = assigns(d,act);
  if ( !x || !d->m_parents.empty() )
    return result[d] = x;
  basic_block::info* B = act->mt->B;
  if ( liveout(x,B) )
    return result[d] = x;
  bool b = use_after(x,act);
  if ( b && resident(x,make_pair(d,act->mt->node)) )
    return result[d] = x;
  const set<var*>& addr = pa->addr;
  if ( addr.find(x) != addr.end() )
    return result[d] = x;
  switch ( id ){
  case tac::CALL:
    if ( !b ){
      ptr->x = 0;
      return result[d] = 0;
    }
  case tac::ALLOC:
    return result[d] = x;
  }
  conv.erase(conv.begin()+n);
  if ( id == tac::LOFF ){
    loffs[x].push_back(make_pair(ptr,conv.size()));
    return result[d] = 0;
  }
  delete ptr;
  return result[d] = 0;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  var* special1(dag::info*, action_t*);
  void special2(var*, var*, mknode_t*);
  struct assign_t {
    action_t* act;
    var* y;
    var* x;
    info* d;
    var* addrrefed;
    assign_t(action_t* a, var* yy, info* dd) : act(a), y(yy), x(yy), d(dd), addrrefed(0) {}
  };
  int assign(var*, assign_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

c_compiler::var*
c_compiler::optimize::basic_block::dag::generate::assigns(dag::info* d, action_t* act)
{
  using namespace std;
  vector<var*>& v = d->m_vars;
  if ( v.empty() )
    return 0;
  if ( var* ret = special1(d,act) )
    return ret;
  mknode_t* mt = act->mt;
  basic_block::info* B = mt->B;
  vector<var*>::iterator p;
  if ( d->m_left ){
    p = find_if(v.begin(),v.end(),bind2nd(ptr_fun(resident),make_pair(d,mt->node)));
    if ( p != v.end() ){
      vector<var*>::iterator q =
        find_if(p,v.end(),bind2nd(ptr_fun(liveout),B));
      if ( q != v.end() ){
        if ( resident(*q,make_pair(d,mt->node)) )
          p = q;
      }
      else {
        q = find_if(p,v.end(),bind2nd(ptr_fun(use_after),act));
        if ( q != v.end() ){
          p = q;
          special2(*v.begin(),*p,mt);
        }
      }
    }
    if ( p != v.end() ){
      if ( p != v.begin() )
        special2(*v.begin(),*p,mt);
    }
    else
      p = v.begin();
  }
  else
    p = v.begin();
  var* y = *p;
  assign_t arg(act,y,d);
  for_each(p+1,v.end(),bind2nd(ptr_fun(assign),&arg));
  if ( var* ret = arg.addrrefed )
    return ret;
  return y;
}

c_compiler::var*
c_compiler::optimize::basic_block::dag::generate::special1(dag::info* d, action_t* act)
{
  using namespace std;
  mknode_t* mt = act->mt;
  vector<var*>& v = d->m_vars;
  vector<var*>::iterator p =
    find_if(v.begin(),v.end(),bind2nd(ptr_fun(resident),make_pair(d,mt->node)));
  if ( p == v.end() )
    return 0;
  if ( p == v.begin() )
    return 0;
  if ( d->m_parents.empty() )
    return 0;
  basic_block::info* B = mt->B;
  vector<var*>::iterator q =
    find_if(p,v.end(),bind2nd(ptr_fun(liveout),B));
  if ( q != v.end() && resident(*q,make_pair(d,mt->node)) )
    p = q;
  if ( !d->m_left ){
    mt->pa->conv.push_back(new assign3ac(*p,*v.begin()));
    assign_t arg(act,*p,d);
    for_each(p+1,v.end(),bind2nd(ptr_fun(assign),&arg));
  }
  return *p;
}

bool c_compiler::optimize::basic_block::dag::generate::resident(var* v,
  std::pair<dag::info*, std::map<var*, dag::info*>*> p)
{
  using namespace std;
  dag::info* d = p.first;
  map<var*, dag::info*>* node = p.second;
  map<var*, dag::info*>::const_iterator it = node->find(v);
  assert(it != node->end());
  return it->second == d;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  tac* replace_dst(tac*, var*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::dag::generate::special2(var* old, var* _new, mknode_t* mt)
{
  using namespace std;
  map<var*, vector<pair<tac*, int> > >& loffs = mt->pa->loffs;
  map<var*, vector<pair<tac*, int> > >::iterator p = loffs.find(old);
  if ( p == loffs.end() )
    return;
  const vector<pair<tac*, int> >& v = p->second;
  vector<tac*>& conv = mt->pa->conv;
  tac* tmp = conv.back();
  conv.pop_back();
  typedef vector<pair<tac*, int> >::const_reverse_iterator IT;
  for ( IT q = v.rbegin() ; q != v.rend() ; ++q ){
    vector<tac*>::iterator r = conv.begin() + q->second;
    conv.insert(r,replace_dst(q->first,_new));
  }
  conv.push_back(tmp);
  loffs.erase(p);
}

c_compiler::tac*
c_compiler::optimize::basic_block::dag::generate::replace_dst(tac* ptr, var* x)
{
  ptr->x = x;
  return ptr;
}

bool c_compiler::optimize::basic_block::dag::generate::liveout(var* x, basic_block::info* B)
{
  using namespace std;
  const set<var*>& o = live_var::out[B];
  return o.find(x) != o.end();
}

bool c_compiler::optimize::basic_block::dag::generate::use_after(var* x, action_t* act)
{
  using namespace std;
  tac** pos = act->pos;
  map<var*, vector<pair<tac**, bool> > >::const_iterator q =
    basic_block::live.find(x);
  assert(q != basic_block::live.end());
  const vector<pair<tac**, bool> >& v = q->second;
  vector<pair<tac**,bool> >::const_reverse_iterator r;
  for ( r = v.rbegin() ; r != v.rend() ; ++r ){
    if ( r->first > pos )
      break;
  }
  if ( r == v.rend() )
    return false;
  return r->second;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  bool addrrefs(var*, assign_t*);
  int gen_assign(var*, assign_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

int
c_compiler::optimize::basic_block::dag::generate::assign(var* x, assign_t* arg)
{
  using namespace std;
  var* y = arg->y;
  usr* uy = y->usr_cast();
  if ( addrrefs(x,arg) ){
    arg->addrrefed = x;
    if ( arg->d->m_left )
      return 0;
    return gen_assign(x,arg);
  }
  if ( !resident(x,make_pair(arg->d,arg->act->mt->node)) )
    return 0;
  basic_block::info* B = arg->act->mt->B;
  if ( liveout(x,B) )
    return gen_assign(x,arg);
  set<var*>& addr = arg->act->mt->pa->addr;
  if ( addr.find(x) != addr.end() )
    return gen_assign(x,arg);
  if ( use_after(x,arg->act) )
    return gen_assign(x,arg);
  return 0;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  bool addrref(var*, dag::info*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

bool
c_compiler::optimize::basic_block::dag::generate::addrrefs(var* x, assign_t* arg)
{
  using namespace std;
  dag::info* d = arg->d;
  vector<dag::info*> v = d->m_parents;
  return find_if(v.begin(),v.end(),bind1st(ptr_fun(addrref),x)) != v.end();
}

bool
c_compiler::optimize::basic_block::dag::generate::addrref(var* x, dag::info* parent)
{
  tac* ptr = parent->m_tac;
  if ( !ptr )
    return false;
  return ptr->id == tac::ADDR && ptr->y == x;
}

int
c_compiler::optimize::basic_block::dag::generate::gen_assign(var* x, assign_t* arg)
{
  using namespace std;
  vector<tac*>& conv = arg->act->mt->pa->conv;
  var* y = arg->y;
  conv.push_back(new assign3ac(x,y));
  arg->x = x;
  return 0;
}

namespace c_compiler { namespace optimize { namespace symtab {
  extern bool not_referenced(var*, const std::vector<tac*>*);
} } } // end of namespace symtab, optimize and c_compiler

int c_compiler::optimize::symtab::simplify(scope* ptr, std::vector<tac*>* res)
{
  using namespace std;
  map<string, vector<usr*> >& usrs = ptr->m_usrs;
  typedef map<string, vector<usr*> >::iterator X;
  for ( X x = usrs.begin() ; x != usrs.end() ; ){
    vector<usr*>& vec = x->second;
    typedef vector<usr*>::iterator IT;
    for ( IT p = vec.begin() ; p != vec.end() ; ){
		usr* u = *p;
		if (not_referenced(u, res)) {
			delete u;
			p = vec.erase(p);
		}
		else
			++p;
    }
    if ( vec.empty() ){
		X xx = x++;
		usrs.erase(xx);
    }
    else
      ++x;
  }
  if ( ptr->m_id == scope::BLOCK ){
    block* b = static_cast<block*>(ptr);
    vector<var*>& v = b->m_vars;
    typedef vector<var*>::iterator IT;
    for ( IT p = v.begin() ; p != v.end() ; ){
      if ( not_referenced(*p,res) ){
        delete *p;
        p = v.erase(p);
      }
      else
        ++p;
    }
  }
  vector<scope*>& c = ptr->m_children;
  for_each(c.begin(),c.end(),bind2nd(ptr_fun(simplify),res));
  return 0;
}

namespace c_compiler { namespace optimize { namespace symtab {
  extern bool refs(tac*, var*);
} } } // end of namespace symtab, optimize and c_compiler

bool c_compiler::optimize::symtab::not_referenced(var* v, const std::vector<tac*>* res)
{
  using namespace std;
  vector<tac*>::const_iterator p =
    find_if(res->begin(),res->end(),bind2nd(ptr_fun(refs),v));
  return p == res->end();
}

bool c_compiler::optimize::symtab::refs(tac* ptr, var* v)
{
  if ( var* u = ptr->x ){
    if ( u == v )
      return true;
  }
  if ( var* u = ptr->y ){
    if ( u == v )
      return true;
  }
  if ( var* u = ptr->z ){
    if ( u == v )
      return true;
  }
  return false;
}

namespace c_compiler { namespace optimize { namespace symtab { namespace literal {
	using namespace std;
	set<usr*> canbe_erased;
	void mark1(tac*);
	void mark2(scope*);
	void mark3(const pair<string, vector<usr*> >&);
	void mark4(usr*);
	void mark5(const pair<int, var*>&);
	void erase(usr*);
} } } } // end of namespace literal, symtab, optimize and c_compiler

void c_compiler::optimize::mark(usr* u)
{
	symtab::literal::canbe_erased.insert(u);
}

void c_compiler::optimize::symtab::literal::simplify(const std::vector<tac*>& vc)
{
	using namespace std;
	for_each(vc.begin(), vc.end(), mark1);
	mark2(&scope::root);
	for_each(canbe_erased.begin(), canbe_erased.end(), erase);
	canbe_erased.clear();
}

void c_compiler::optimize::symtab::literal::mark1(tac* tac)
{
  using namespace std;
  if (var* y = tac->y) {
	  if (usr* u = y->usr_cast()) {
		  if (u->isconstant() || is_string(u->m_name)) {
			  canbe_erased.erase(u);
		  }
	  }
  }
  if (var* z = tac->z) {
	  if (usr* u = z->usr_cast()) {
		  if (u->isconstant() || is_string(u->m_name)) {
			  canbe_erased.erase(u);
		  }
	  }
  }
}

void c_compiler::optimize::symtab::literal::mark2(scope* ptr)
{
	using namespace std;
	map<string, vector<usr*> >& usrs = ptr->m_usrs;
	for_each(usrs.begin(), usrs.end(), mark3);
	vector<scope*>& children = ptr->m_children;
	for_each(children.begin(), children.end(), mark2);
}

void c_compiler::optimize::symtab::literal::mark3(const std::pair<std::string, std::vector<usr*> >& x)
{
	using namespace std;
	const vector<usr*>& vec = x.second;
	for_each(vec.begin(), vec.end(), mark4);
}

void c_compiler::optimize::symtab::literal::mark4(usr* u)
{
  using namespace std;
  usr::flag flag = u->m_flag;
  if ( !(flag & usr::WITH_INI) )
    return;
  with_initial* wi = static_cast<with_initial*>(u);
  const map<int, var*>& value = wi->m_value;
  for_each(value.begin(), value.end(), mark5);
}

void c_compiler::optimize::symtab::literal::mark5(const std::pair<int, var*>& x)
{
	using namespace std;
	var* v = x.second;
	if (usr* u = v->usr_cast()) {
		assert(u->isconstant());
		canbe_erased.erase(u);
	}
	else {
		addrof* addr = v->addrof_cast();
		assert(addr);
		var* ref = addr->m_ref;
		if (usr* u = ref->usr_cast()) {
			if (is_string(u->m_name))
				canbe_erased.erase(u);
		}
	}
}

void c_compiler::optimize::symtab::literal::erase(usr* u)
{
	using namespace std;
	map<string, vector<usr*> >& usrs = scope::root.m_usrs;
	string name = u->m_name;
	map<string, vector<usr*> >::iterator it = usrs.find(name);
	assert(it != usrs.end());
	vector<usr*>& v = it->second;
	assert(v.size() == 1);
	assert(v[0] == u);
	usrs.erase(it);
	delete u;
}

void c_compiler::optimize::remember_action(const std::vector<tac*>& vc)
{
  using namespace std;
  using namespace symtab::literal;
  for_each(vc.begin(),vc.end(),mark1);
}