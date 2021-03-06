#include "stdafx.h"
#include "c_core.h"
#include "parse.h"
#include "misc.h"

namespace c_compiler {
  namespace optimize {
    using namespace std;
    void goto_to(vector<tac*>&);
    void empty_to3ac(vector<tac*>&);
    namespace basic_block {
      void action(const fundef* fdef, vector<tac*>& v);
    } // end of namespace basic_block
  } // end of namespace optimize
} // end of namespace c_compiler

void c_compiler::optimize::action(const fundef* fdef, std::vector<tac*>& v)
{
  goto_to(v);
  empty_to3ac(v);
  if (cmdline::bb_optimize)
    basic_block::action(fdef, v);
}

void c_compiler::optimize::goto_to(std::vector<tac*>& vt)
{
  using namespace std;
  typedef vector<tac*>::iterator IT;
  for ( IT p = vt.begin() ; p != vt.end() ; ){
    if ((*p)->m_id != tac::GOTO) {
      ++p;
      continue;
    }
    goto3ac* go = static_cast<goto3ac*>(*p);
    IT q = p + 1;
    if (q == vt.end())
      break;
    if ((*q)->m_id != tac::TO) {
      p = q;
      continue;
    }
    to3ac* to = static_cast<to3ac*>(*q);
    if (go->m_to != to) {
      p = q + 1;
      continue;
    }
    delete go;
    vector<goto3ac*>& vg = to->m_goto;
    vector<goto3ac*>::iterator r = find(vg.begin(),vg.end(),go);
    assert(r != vg.end());
    vg.erase(r);
    if (vg.empty()) {
      delete to;
      vt.erase(q);
    }
    p = vt.erase(p);
  }
}

void c_compiler::optimize::empty_to3ac(std::vector<tac*>& v)
{
  using namespace std;
  typedef vector<tac*>::iterator IT;
  for ( IT p = v.begin() ; p != v.end() ; ){
    if ((*p)->m_id == tac::TO) {
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
  using namespace std;    
  namespace dag {
    struct action_t {
      vector<tac*> conv;
      set<var*> addr;
      bool m_opt_goto {false};
    };
    extern void action(info_t*, action_t*);
  } // end of namespace dag
    inline bool not_reach(info_t* info)
    {
      return info->m_preceed.empty();
    }
    inline void erase_to(tac* ptr)
    {
      tac::id_t id = ptr->m_id;
      if (id != tac::GOTO)
	return;
      auto go = static_cast<goto3ac*>(ptr);
      auto to = go->m_to;
      auto& v = to->m_goto;
      auto p = find(begin(v), end(v), go);
      v.erase(p);
    }
    inline void del_code(info_t* info, vector<tac*>& v)
    {
      auto leader = info->m_leader;
      auto b = find(begin(v), end(v), *leader);
      assert(b != end(v));
      auto e = b + info->m_size;
      for_each(b, e, erase_to);
      for_each(b, e, [](tac* ptr) { delete ptr; });
      v.erase(b,e);
    }
    inline bool erase_if(vector<info_t*>& bbs, vector<tac*>& v)
    {
      if (bbs.empty())
	return false;
      bool erased = false;
      for (auto p = rbegin(bbs) ; p != rend(bbs) - 1 ; ) {
	if (not_reach(*p)) {
	  del_code(*p, v);
	  auto q = p.base()-1;
	  q = bbs.erase(q);
	  p = make_reverse_iterator(q);
	  erased = true;
	}
	else
	  ++p;
      }
      return erased;
    }
} } } // end of namespace basic_block, optimize and c_compiler


namespace c_compiler { namespace optimize { namespace live_var {
  void analize(const std::vector<basic_block::info_t*>&);
} } } // end of namespace live_var, optimize and c_compiler

namespace c_compiler {
  namespace optimize {
    using namespace std;
    namespace symtab {
      void simplify(scope*, vector<tac*>*);
      namespace literal {
        void simplify(const vector<tac*>&);
      } // end of namespace literal
    } // end of namespace symtab
  } // end of namespace optimize
} // end of namespace c_compiler

void
c_compiler::optimize::basic_block::action(const fundef* fdef,
					  std::vector<tac*>& v)
{
  using namespace std;
  if (cmdline::output_medium && cmdline::output_optinfo) {
    cout << "Before optimization\n";
    function_definition::dump(fdef, v);
  }
  int N = cmdline::optimize_level;
  for (int i = 0 ; i != N ; ++i) {
    pvector<info_t> bbs;
    create(v,bbs);
    if (erase_if(bbs,v)) {
      --i;
      goto_to(v);
      empty_to3ac(v);
      continue;
    }
    live_var::analize(bbs);
    if (cmdline::dag_optimize) {
      dag::action_t seed;
      for_each(bbs.begin(),bbs.end(),bind2nd(ptr_fun(dag::action),&seed));
      int n = v.size();
      v = seed.conv;
      if (n == v.size())
	break;
      if (seed.m_opt_goto) {
	goto_to(v);
	empty_to3ac(v);
      }
    }
  }
  {
    scope* ptr = fdef->m_param;
    vector<scope*>& children = ptr->m_children;
    assert(children.size() == 1);
    ptr = children.back();
    assert(ptr->m_id == scope::BLOCK);
    symtab::simplify(ptr, &v);
  }
  symtab::literal::simplify(v);
}

namespace c_compiler { namespace optimize { namespace basic_block {
  using namespace std;
  void devide(set<tac**>&, const vector<tac*>&);
  void build(vector<info_t*>::iterator, vector<info_t*>::iterator, tac* const*);
} } } // end of namespace basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::create(const std::vector<tac*>& v, std::vector<info_t*>& bbs)
{
  using namespace std;
  if ( v.empty() )
    return;
  set<tac**> leader;
  devide(leader,v);
  transform(leader.begin(),leader.end(),back_inserter(bbs),
            [](tac** ptr){ return new info_t(ptr); } );
  tac* const* tmp = &v[0] + v.size();
  build(bbs.begin(),bbs.end(),tmp);
}

void c_compiler::optimize::basic_block::devide(std::set<tac**>& leader, const std::vector<tac*>& v)
{
  using namespace std;
  assert(!v.empty());
  typedef vector<tac*>::const_iterator IT;
  IT p = v.begin();
  leader.insert(const_cast<tac**>(&*p));
  while (p != v.end()) {
    tac::id_t id = (*p)->m_id;
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
  bool destination(info_t*, tac*);
} } } // end of namespace basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::build(std::vector<info_t*>::iterator begin,
                                         std::vector<info_t*>::iterator end,
                                         tac* const* end3ac)
{
  std::vector<tac*> vv;
  using namespace std;
  typedef vector<info_t*>::const_iterator IT;
  for ( IT p = begin ; p != end ; ){
    basic_block::info_t* curr = *p;
    ++p;
    basic_block::info_t* next = p != end ? *p : 0;
    curr->m_size = next ? next->m_leader - curr->m_leader : end3ac - curr->m_leader;
    tac* last = next ? *(next->m_leader - 1) : *(end3ac-1);
    if (last->m_id == tac::GOTO) {
      goto3ac* go = static_cast<goto3ac*>(last);
      tac* to = go->m_to;
      IT q = find_if(begin,end,bind2nd(ptr_fun(destination),to));
      assert(q != end);
      basic_block::info_t* follow = *q;
      curr->m_follow.push_back(follow);
      follow->m_preceed.push_back(curr);
      if ( go->m_op && next )
        curr->m_follow.push_back(next), next->m_preceed.push_back(curr);
    }
    else if ( next )
      curr->m_follow.push_back(next), next->m_preceed.push_back(curr);
  }
}

bool c_compiler::optimize::basic_block::destination(info_t* B, tac* to)
{
  return *B->m_leader == to;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  struct info_t;
  struct mknode_t {
    std::map<var*, dag::info_t*>* node;
    std::map<dag::info_t*, var*>* result;
    basic_block::info_t* B;
    action_t* pa;
    bool ret;
    mknode_t(std::map<var*, dag::info_t*>* n, std::map<dag::info_t*, var*>* r, basic_block::info_t* b, action_t* a)
      : node(n), result(r), B(b), pa(a), ret(false) {}
  };
  void mknode(tac**, mknode_t*);
  namespace generate {
    struct action_t {
      mknode_t* mt;
      tac** pos;
      action_t(mknode_t* m, tac** p) : mt(m), pos(p) {}
    };
    void action(action_t*);
  } // end of namespace generate
} } } } // end of namespace dag, basic_block, optimize and c_compiler

void c_compiler::optimize::basic_block::dag::action(basic_block::info_t* B,
                                                    action_t* pa)
{
  using namespace std;
  tac** begin = B->m_leader;
  tac** end = begin + B->m_size;
  map<var*, dag::info_t*> node;
  map<dag::info_t*, var*> result;
  mknode_t mt(&node,&result,B,pa);
  for ( tac** p = begin ; p != end ; ++p )
    mknode(p,&mt);
  generate::action_t act(&mt,end-1);
  generate::action(&act);
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  using namespace std;
  struct info_t {
    tac* m_tac;
    vector<var*> m_vars;
    vector<info_t*> m_parents;
    info_t* m_left;
    info_t* m_right;
    info_t* m_extra;
    static vector<info_t*> all;
    info_t()
      : m_tac(0), m_left(0), m_right(0), m_extra(0) { all.push_back(this); }
  };
  vector<dag::info_t*> info_t::all;
  info_t* get(tac*, map<var*, dag::info_t*>*, bool*);
} } } } // end of namespace dag, basic_block, optimize and c_compiler

void c_compiler::optimize::basic_block::dag::mknode(tac** pp, mknode_t* mt)
{
  using namespace std;
  tac* ptr = *pp;
  tac::id_t id = ptr->m_id;
  if ( mt->ret ){
    if (id != tac::TO && id != tac::GOTO) {
      delete ptr;
      return;
    }
    mt->ret = false;
  }
  if ( id == tac::RETURN )
    mt->ret = true;
  var* y = ptr->y;
  if (!y) {
    // Special case. return3ac, goto3ac, to3ac or asm3ac.
    dag::info_t* p = new dag::info_t;
    p->m_tac = ptr;
    return;
  }
  map<var*, dag::info_t*>* node = mt->node;
  if (node->find(y) == node->end()) {
    dag::info_t* leaf = new dag::info_t;
    leaf->m_vars.push_back(y);
    (*node)[y] = leaf;
  }
  if (var* z = ptr->z) {
    if (node->find(z) == node->end()) {
      dag::info_t* leaf = new dag::info_t;
      leaf->m_vars.push_back(z);
      (*node)[z] = leaf;
    }
  }
  bool found = false;
  dag::info_t* n = get(ptr,node,&found);
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
    return;
  }
  var* x = ptr->x;
  if (!x)
    return;
  auto_ptr<tac> sweper(found ? ptr : 0);
  vector<var*>& v = n->m_vars;
  v.push_back(x);
  (*node)[x] = n;
  if (id == tac::ADDR)
    mt->pa->addr.insert(y);
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag {
  bool match(info_t*, std::pair<tac*, std::map<var*, dag::info_t*>*>);
  bool roff_match_loff(info_t*, std::pair<tac*, std::map<var*, dag::info_t*>*>);
} } } } // end of namespace dag, basic_block, optimize and c_compiler

c_compiler::optimize::basic_block::dag::info_t*
c_compiler::optimize::basic_block::
dag::get(tac* ptr, std::map<var*, dag::info_t*>* node, bool* found)
{
  using namespace std;
  tac::id_t id = ptr->m_id;
  switch (id) {
  case tac::ASSIGN:
    {
      var* y = ptr->y;
      map<var*, info_t*>::const_iterator p = node->find(y);
      assert(p != node->end());
      *found = true;
      info_t* info = p->second;
      return info;
    }
  case tac::CALL: case tac::VAARG:
    break;
  default:
    if (ptr->x) {
      const vector<dag::info_t*>& v = dag::info_t::all;
      vector<dag::info_t*>::const_iterator p =
        find_if(v.begin(),v.end(),bind2nd(ptr_fun(match),make_pair(ptr,node)));
      if (p != v.end()) {
        *found = true;
        return *p;
      }
      if (id == tac::ROFF) {
        p = find_if(v.begin(),v.end(),
                    bind2nd(ptr_fun(roff_match_loff),make_pair(ptr,node)));
        if ( p != v.end() ){
          *found = true;
          return (*p)->m_right;
        }
      }
    }
    break;
  }
  info_t* ret = new info_t;
  ret->m_tac = ptr;
  map<var*, info_t*>::const_iterator q = node->find(ptr->y);
  assert(q != node->end());
  info_t* i = q->second;
  i->m_parents.push_back(ret);
  ret->m_left = i;
  if (var* z = ptr->z) {
    map<var*, info_t*>::const_iterator q = node->find(z);
    assert(q != node->end());
    info_t* i = q->second;
    i->m_parents.push_back(ret);
    ret->m_right = i;
  }
  if (ptr->m_id == tac::LOFF) {
    var* x = ptr->x;
    map<var*, info_t*>::const_iterator q = node->find(x);
    if (q != node->end()) {
      info_t* i = q->second;
      i->m_parents.push_back(ret);
      ret->m_extra = i;
    }
  }
  return ret;
}

bool
c_compiler::optimize::basic_block::
dag::match(info_t* xinfo, std::pair<tac*, std::map<var*, dag::info_t*>*> arg)
{
  using namespace std;
  tac* xtac = xinfo->m_tac;
  tac* ytac = arg.first;
  if ( !xtac )
    return false;
  if (xtac->m_id != ytac->m_id)
    return false;
  map<var*, dag::info_t*>* node = arg.second;

  if ( dag::info_t* left = xinfo->m_left ){
    var* y = ytac->y;
    map<var*, dag::info_t*>::const_iterator p = node->find(ytac->y);
    assert(p != node->end());
    dag::info_t* i = p->second;
    if ( left != i )
      return false;
  }
  if ( dag::info_t* right = xinfo->m_right ){
    map<var*, dag::info_t*>::const_iterator p = node->find(ytac->z);
    assert(p != node->end());
    dag::info_t* i = p->second;
    if ( right != i )
      return false;
  }
  tac::id_t id = xtac->m_id;
  if (id == tac::LOFF) {
    if ( dag::info_t* extra = xinfo->m_extra ){
      map<var*, dag::info_t*>::const_iterator p = node->find(ytac->x);
      if (p != node->end()) {
        dag::info_t* i = p->second;
        if ( extra != i )
          return false;
      }
    }
  }

  switch (id) {
  case tac::ROFF:
  case tac::INVRADDR:
    {
      const type* x = xtac->x->m_type;
      const type* y = ytac->x->m_type;
      x = x->unqualified();
      y = y->unqualified();
      return compatible(x, y);
    }
  case tac::CAST:
    {
      cast3ac* x = static_cast<cast3ac*>(xtac);
      cast3ac* y = static_cast<cast3ac*>(ytac);
      const type* Tx = x->m_type;
      const type* Ty = y->m_type;
      Tx = Tx->unqualified();
      Ty = Ty->unqualified();
      return compatible(Tx, Ty);
    }
  case tac::LOFF:
  case tac::ALLOCA:
    return xtac->x == ytac->x;
  default:
    return true;
  }
}

bool
c_compiler::optimize::basic_block::
dag::roff_match_loff(info_t* xinfo,
                     std::pair<tac*, std::map<var*, dag::info_t*>*> arg)
{
  using namespace std;
  tac* roff = arg.first;
  assert(roff->m_id == tac::ROFF);
  tac* loff = xinfo->m_tac;
  if ( !loff )
    return false;
  if (loff->m_id != tac::LOFF)
    return false;
  map<var*, dag::info_t*>* node = arg.second;
  var* a = roff->y;
  map<var*, dag::info_t*>::const_iterator p = node->find(a);
  assert(p != node->end());
  if ( xinfo != p->second )
    return false;
  var* i = roff->z;
  map<var*, dag::info_t*>::const_iterator q = node->find(i);
  assert(q != node->end());
  if ( xinfo->m_left != q->second )
    return false;
  const type* x = roff->x->m_type;
  const type* y = loff->z->m_type;
  x = x->unqualified();
  y = y->unqualified();
  return compatible(x, y);
}

namespace c_compiler { namespace optimize { namespace live_var {
  std::map<basic_block::info_t*, std::set<var*> > in, out, def, use;
  int prepare(basic_block::info_t*);
  void add_global1(basic_block::info_t*);
  int calc(int, basic_block::info_t*);
  void msg(int);
} } } // end of namespace live_var, optimize and c_compiler

namespace c_compiler { namespace optimize { namespace basic_block {
  std::map<var*, std::vector<std::pair<tac**,bool> > > live;
} } } // end of namespace basic_block, optimize and c_compiler

namespace c_compiler {namespace names {
  std::string ref(var*);
  std::string refb(optimize::basic_block::info_t*);
} } // end of namespace names, dump and c_compiler

void c_compiler::optimize::live_var::analize(const std::vector<basic_block::info_t*>& bbs)
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
  void setup(tac**, basic_block::info_t*);
} } } // end of namespace live_var, optimize and c_compiler

int c_compiler::optimize::live_var::prepare(basic_block::info_t* B)
{
  using namespace std;
  tac** rend = B->m_leader - 1;
  tac** rbegin = rend + B->m_size;
  for ( tac** p = rbegin ; p != rend ; --p )
    setup(p,B);
  return 0;
}

void c_compiler::optimize::live_var::setup(tac** pp, basic_block::info_t* B)
{
  using namespace std;
  tac* p = *pp;
  typedef map<basic_block::info_t*, set<var*> >::iterator IT;
  if ( var* x = p->x ){
    def[B].insert(x);
    IT q = use.find(B);
    if ( q != use.end() )
      q->second.erase(x);
    tac::id_t id = p->m_id;
    if (id == tac::LOFF)
      basic_block::live[x].push_back(make_pair(pp,true));
    else
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
  int setout(basic_block::info_t*, basic_block::info_t*);
} } } // end of namespace live_var, optimize and c_compiler

int c_compiler::optimize::live_var::calc(int n, basic_block::info_t* B)
{
  using namespace std;
  // out[B] = U in[F], where F follows B
  const vector<basic_block::info_t*>& v = B->m_follow;
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

int c_compiler::optimize::live_var::setout(basic_block::info_t* B, basic_block::info_t* S)
{
  using namespace std;
  const set<var*>& a = in[S];
  set<var*>& b = out[B];
  copy(a.begin(),a.end(),inserter(b,b.begin()));
  return 0;
}

namespace c_compiler { namespace optimize { namespace live_var {
  void add_global2(const std::set<var*>&, basic_block::info_t*);
  bool local(var*);
} } } // end of namespace live_var, optimize and c_compiler

void c_compiler::optimize::live_var::add_global1(basic_block::info_t* B)
{
  for (auto& p : def)
    add_global2(p.second,B);
  for (auto& p : use)
    add_global2(p.second,B);
}

void c_compiler::optimize::live_var::add_global2(const std::set<var*>& i, basic_block::info_t* B)
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
  usr::flag_t flag = u->m_flag;
  usr::flag_t mask = usr::flag_t(usr::EXTERN | usr::STATIC);
  return !(flag & mask);
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  var* inorder(info_t*, action_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::dag::generate::action(action_t* act)
{
  using namespace std;
  vector<info_t*>& v = info_t::all;
  for (auto p : v) inorder(p, act);
  for (auto p : v) delete p;
  v.clear();
}

namespace c_compiler {
  namespace optimize {
    namespace basic_block {
      namespace dag {
	namespace generate {
	  using namespace std;
	  var* assigns(dag::info_t*, action_t*);
	  bool liveout(var*, basic_block::info_t*);
	  bool use_after(var*, action_t*);
	  bool resident(var*, pair<dag::info_t*, map<var*, dag::info_t*>*>);
	} // end of namespace generate
      } // end of namespace dag
    } // end of namespace basic_block
    namespace constant_conv {
      typedef var* (var::*BIN)(var*);
      struct bin_tbl_t : map<tac::id_t, BIN> {
	bin_tbl_t()
	{
	  (*this)[tac::ADD] = &var::add;
	  (*this)[tac::SUB] = &var::sub;
	  (*this)[tac::MUL] = &var::mul;
	  (*this)[tac::DIV] = &var::div;
	  (*this)[tac::MOD] = &var::mod;
	  (*this)[tac::LSH] = &var::lsh;
	  (*this)[tac::RSH] = &var::rsh;
	  (*this)[tac::AND] = &var::bit_and;
	  (*this)[tac:: OR] = &var::bit_or;
	  (*this)[tac::XOR] = &var::bit_xor;
	}
      } bin_tbl;
      typedef var* (var::*UNA)();
      struct una_tbl_t : map<tac::id_t, UNA> {
	una_tbl_t()
	{
	  (*this)[tac::UMINUS] = &var::minus;
	  (*this)[tac::TILDE]  = &var::tilde;
	}
      } una_tbl;
      inline var* op(tac* ptr, var* y, var* z)
      {
	tac::id_t id = ptr->m_id;
	auto p = bin_tbl.find(id);
	if (p != bin_tbl.end()) {
	  auto v = p->second;
	  return (y->*v)(z);
	}
	auto q = una_tbl.find(id);
	if (q != una_tbl.end()) {
	  auto v = q->second;
	  return (y->*v)();
	}
	if (id != tac::CAST)
	  return 0;
	cast3ac* cast = static_cast<cast3ac*>(ptr);
	const type* T = cast->m_type;
	return y->cast(T);
      }
      struct go_tbl_t : map<goto3ac::op, BIN> {
	go_tbl_t()
	{
	  (*this)[goto3ac::EQ] = &var::eq;
	  (*this)[goto3ac::NE] = &var::ne;
	  (*this)[goto3ac::LE] = &var::le;
	  (*this)[goto3ac::GE] = &var::ge;
	  (*this)[goto3ac::LT] = &var::lt;
	  (*this)[goto3ac::GT] = &var::gt;
	}
      } go_tbl;
      inline tac* ifgoto_opt(tac* ptr, var* y, var* z, bool* opt_goto)
      {
	tac::id_t id = ptr->m_id;
	if (id != tac::GOTO)
	  return ptr;
	auto org = static_cast<goto3ac*>(ptr);
	auto op = org->m_op;
	if (op == goto3ac::NONE)
	  return ptr;
	*opt_goto = true;
	auto p = go_tbl.find(op);
	assert(p != go_tbl.end());
	auto v = p->second;
	var* res = (y->*v)(z);
	assert(res->isconstant());
	int n = res->value();
	auto to = org->m_to;
	auto& goes = to->m_goto;
	auto q = find(begin(goes), end(goes), org);
	assert(q != end(goes));
	delete org;
	if (n) {
	  auto ret = new goto3ac;
	  ret->m_to = to;
	  *q = ret;
	  return ret;
	}
	goes.erase(q);
	return 0;
      }
      tac* action(tac* ptr, bool* opt_goto)
      {
	var* y = ptr->y;
	if (y) {
	  if (!y->isconstant())
	    return ptr;
	}
	var* z = ptr->z;
	if (z) {
	  if (!z->isconstant())
	    return ptr;
	  const type* T = y->m_type;
	  z = z->cast(T);
	}
	var* x = op(ptr, y, z);
	if (!x)
	  return ifgoto_opt(ptr, y, z, opt_goto);
	if (!x->isconstant()) {
	  tac::id_t id = ptr->m_id;
	  assert(id == tac::DIV || id == tac::MOD);
	  assert(z->zero());
	  return ptr;
	}
	delete ptr;
	return new assign3ac(ptr->x, x);
      }
    } // end of namespace constant_conv
  } // end of namespace optimize
} // end of namespace c_compiler

c_compiler::var*
c_compiler::optimize::basic_block::
dag::generate::inorder(dag::info_t* d, action_t* act)
{
  using namespace std;
  map<dag::info_t*, var*>& result = *act->mt->result;
  tac* ptr = d->m_tac;
  if (!ptr)
    return result[d] = assigns(d,act);
  if (dag::info_t* left = d->m_left) {
    map<dag::info_t*, var*>::const_iterator p = result.find(left);
    assert(p != result.end());
    ptr->y = p->second;
  }
  if (dag::info_t* right = d->m_right) {
    map<dag::info_t*, var*>::const_iterator p = result.find(right);
    assert(p != result.end());
    ptr->z = p->second;
  }
  dag::action_t* pa = act->mt->pa;
  vector<tac*>& conv = pa->conv;
  ptr = constant_conv::action(ptr, &pa->m_opt_goto);
  if (!ptr)
    return result[d];
  int n = conv.size();
  conv.push_back(ptr);
  var* x = ptr->x = assigns(d,act);
  if (!x || !d->m_parents.empty())
    return result[d] = x;
  basic_block::info_t* B = act->mt->B;
  if (liveout(x,B))
    return result[d] = x;
  bool b = use_after(x,act);
  if (b && resident(x,make_pair(d,act->mt->node)))
    return result[d] = x;
  const set<var*>& addr = pa->addr;
  if (addr.find(x) != addr.end())
    return result[d] = x;
  tac::id_t id = ptr->m_id;
  switch (id) {
  case tac::CALL:
    if (!b) {
      ptr->x = 0;
      return result[d] = 0;
    }
  case tac::ALLOCA:
    return result[d] = x;
  case tac::LOFF:
    {
      const vector<var*>& v = d->m_vars;
      typedef vector<var*>::const_iterator IT;
      IT p = find_if(begin(v), end(v), bind2nd(ptr_fun(liveout), B));
      if (p != end(v))
        return result[d] = x;
      p = find_if(begin(v), end(v), bind2nd(ptr_fun(use_after), act));
      if (p != end(v))
        return result[d] = x;
    }
  }
  conv.erase(conv.begin()+n);
  delete ptr;
  return result[d] = 0;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  var* special_case(dag::info_t*, action_t*);
  struct assign_t {
    action_t* act;
    var* y;
    var* x;
    info_t* d;
    var* addrrefed;
    assign_t(action_t* a, var* yy, info_t* dd)
      : act(a), y(yy), x(yy), d(dd), addrrefed(0) {}
  };
  void assign(var*, assign_t*);
  typedef vector<var*>::iterator IT;
  IT chose(vector<var*>& v, dag::info_t* d, action_t* act)
  {
    if (!d->m_left) {
      assert(!d->m_right);
      assert(!d->m_extra);
      return v.begin();
    }
    if (tac* ptr = d->m_tac) {
      tac::id_t id = ptr->m_id;
      if (id == tac::LOFF)
        return v.begin();
    }
    mknode_t* mt = act->mt;
    basic_block::info_t* B = mt->B;
    IT p = find_if(v.begin(),v.end(),bind2nd(ptr_fun(liveout),B));
    if (p != v.end())
      return p;
    p = find_if(v.begin(),v.end(),bind2nd(ptr_fun(use_after),act));
    if (p != v.end())
      return p;
    p = find_if(v.begin(),v.end(),bind2nd(ptr_fun(resident),
                                          make_pair(d,mt->node)));
    if (p != v.end())
      return p;
    p = find_if(v.begin(),v.end(),
                [mt](var* v){
                  set<var*>& addr = mt->pa->addr;
                  return addr.find(v) != addr.end();
                });
    if (p != v.end())
      return p;
    return v.begin();
  }
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

c_compiler::var*
c_compiler::optimize::basic_block::
dag::generate::assigns(dag::info_t* d, action_t* act)
{
  using namespace std;
  vector<var*>& v = d->m_vars;
  if (v.empty())
    return 0;
  if (var* ret = special_case(d,act))
    return ret;
  vector<var*>::iterator p = chose(v, d, act);
  assert(p != v.end());
  var* y = *p;
  assign_t arg(act,y,d);
  for_each(p+1,v.end(),bind2nd(ptr_fun(assign),&arg));
  if (var* ret = arg.addrrefed)
    return ret;
  return y;
}

c_compiler::var*
c_compiler::optimize::basic_block::
dag::generate::special_case(dag::info_t* d, action_t* act)
{
  using namespace std;
  mknode_t* mt = act->mt;
  vector<var*>& v = d->m_vars;
  vector<var*>::iterator p =
    find_if(v.begin(),v.end(),
            bind2nd(ptr_fun(resident),make_pair(d,mt->node)));
  if ( p == v.end() )
    return 0;
  if ( p == v.begin() )
    return 0;
  if ( d->m_parents.empty() )
    return 0;
  basic_block::info_t* B = mt->B;
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
  std::pair<dag::info_t*, std::map<var*, dag::info_t*>*> p)
{
  using namespace std;
  dag::info_t* d = p.first;
  map<var*, dag::info_t*>* node = p.second;
  map<var*, dag::info_t*>::const_iterator it = node->find(v);
  assert(it != node->end());
  return it->second == d;
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  tac* replace_dst(tac*, var*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

c_compiler::tac*
c_compiler::optimize::basic_block::dag::generate::replace_dst(tac* ptr, var* x)
{
  ptr->x = x;
  return ptr;
}

bool c_compiler::optimize::basic_block::dag::generate::liveout(var* x, basic_block::info_t* B)
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
  void gen_assign(var*, assign_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

void
c_compiler::optimize::basic_block::
dag::generate::assign(var* x, assign_t* arg)
{
  using namespace std;
  if (tac* ptr = arg->d->m_tac) {
    tac::id_t id = ptr->m_id;
    if (id == tac::LOFF)
      return gen_assign(x,arg);
  }
  if ( addrrefs(x,arg) ){
    arg->addrrefed = x;
    if ( arg->d->m_left )
      return;
    return gen_assign(x,arg);
  }
  const vector<dag::info_t*>& parents = arg->d->m_parents;
  typedef vector<dag::info_t*>::const_iterator IT;
  IT p = find_if(begin(parents), end(parents), [x](dag::info_t* parent)
                 {
                   if (parent->m_tac->m_id != tac::LOFF)
                     return false;
                   dag::info_t* extra = parent->m_extra;
                   if (!extra)
                     return false;
                   const vector<var*>& v = extra->m_vars;
                   return find(begin(v), end(v), x) != end(v);
                 });
  if (p != end(parents))
    return gen_assign(x, arg);
  if ( !resident(x,make_pair(arg->d,arg->act->mt->node)) )
    return;
  basic_block::info_t* B = arg->act->mt->B;
  if (liveout(x,B))
    return gen_assign(x,arg);
  set<var*>& addr = arg->act->mt->pa->addr;
  if ( addr.find(x) != addr.end() )
    return gen_assign(x,arg);
  if ( use_after(x,arg->act) )
    return gen_assign(x,arg);
}

namespace c_compiler { namespace optimize { namespace basic_block { namespace dag { namespace generate {
  bool addrref(var*, dag::info_t*);
} } } } } // end of namespace generate, dag, basic_block, optimize and c_compiler

bool
c_compiler::optimize::basic_block::dag::generate::addrrefs(var* x, assign_t* arg)
{
  using namespace std;
  dag::info_t* d = arg->d;
  vector<dag::info_t*> v = d->m_parents;
  return find_if(v.begin(),v.end(),bind1st(ptr_fun(addrref),x)) != v.end();
}

bool
c_compiler::optimize::basic_block::dag::generate::addrref(var* x, dag::info_t* parent)
{
  tac* ptr = parent->m_tac;
  if ( !ptr )
    return false;
  return ptr->m_id == tac::ADDR && ptr->y == x;
}

void
c_compiler::optimize::basic_block::dag::generate::gen_assign(var* x, assign_t* arg)
{
  using namespace std;
  vector<tac*>& conv = arg->act->mt->pa->conv;
  var* y = arg->y;
  conv.push_back(new assign3ac(x,y));
  arg->x = x;
}

namespace c_compiler { namespace optimize { namespace symtab {
  extern bool not_referenced(var*, const std::vector<tac*>*);
} } } // end of namespace symtab, optimize and c_compiler

void
c_compiler::optimize::symtab::simplify(scope* ptr, std::vector<tac*>* res)
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
  usr::flag_t flag = u->m_flag;
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
