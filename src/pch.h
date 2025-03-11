#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

using namespace std::literals;
namespace logger = SKSE::log;
namespace fs = std::filesystem;

namespace stl {
using namespace SKSE::stl;

template <class T>
void write_thunk_call(std::uintptr_t a_src) {
  SKSE::AllocTrampoline(14);

  auto& trampoline = SKSE::GetTrampoline();
  T::func = trampoline.write_call<5>(a_src, T::thunk);
}

template <class F, class T>
void write_vfunc() {
  REL::Relocation vtbl{F::VTABLE[0]};
  T::func = vtbl.write_vfunc(T::idx, T::thunk);
}
}  // namespace stl

#ifdef SKYRIM_AE
#define OFFSET(se, ae) ae
#else
#define OFFSET(se, ae) se
#endif

#define EXTERN_C extern "C"

#define RETURN_IF_INITIALIZED()                        \
  if (static auto initialized = false; !initialized) { \
    initialized = true;                                \
  } else {                                             \
    return;                                            \
  }
