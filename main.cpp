#include "stdafx.h"
#include "c_core.h"
#include "parse.h"
#include "misc.h"
#include "yy.h"

int main(int argc, char** argv)
{
  using namespace std;
  using namespace c_compiler;
  cmdline::setup(argc,argv);
  if ( !cmdline::input.empty() ){
    c_compiler_in = fopen(cmdline::input.c_str(),"r");
    if ( !c_compiler_in ){
      error::cmdline::open(cmdline::input);
      exit(1);
    }
    parse::position = file_t(cmdline::input,1);
  }
  {
#ifdef USE_PROFILE
    simple_profiler::sentry mark("main");
#endif // USE_PROFILE
    generator::initialize();
    c_compiler_parse();
  }
#ifdef _DEBUG
  parse::delete_buffer();
#endif // _DEBUG
  static_inline::defer::last();

  if (parse::is_last_decl) {
    if (cmdline::output_medium)
      c_compiler::scope_impl::dump();
    if (!error::counter) {
      if (generator::generate) {
        generator::interface_t tmp = { &scope::root };
        generator::generate(&tmp);
      }
    }
  }
  
  if (!error::counter) {
    if (generator::last) {
      transform(funcs.begin(), funcs.end(), back_inserter(scope::root.m_children), get_pm);
      generator::last_interface_t tmp = {
        &scope::root,
        &funcs
      };
      generator::last(&tmp);
    }
  }

  generator::terminate();
#ifdef USE_PROFILE
  simple_profiler::sentry::output();
#endif // USE_PROFILE
  return error::counter;
}

void c_compiler::static_inline::defer::last()
{
  using namespace std;
  for (auto& p : refs) {
    const vector<ref_t>& v = p.second;
    assert(!v.empty());
    error::extdef::fundef::nodef(v[0]);
  }
}
