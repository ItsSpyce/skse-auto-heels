#include "heel_update_manager.hpp"

static auto heels_update_manager = new HeelUpdateManager();

struct UpdatePlayerHook {
  static void thunk(RE::PlayerCharacter* player, float delta) {
    func(player, delta);
    if (heels_update_manager->needs_update()) {
      if (heels_update_manager->reset_non_feet_armors(player)) {
        logger::info("Reset non-feet armors");
      }
      if (heels_update_manager->update(player)) {
        logger::info("Updated heels sound");
      }
    }
  }
  static inline REL::Relocation<decltype(thunk)> func;
  static inline auto idx = 0xAD;
};


SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
  SKSE::Init(a_skse);
  stl::write_vfunc<RE::PlayerCharacter, UpdatePlayerHook>();
  logger::info("Registered update player hook");
  heels_update_manager->install();
  logger::info("Installed heel update manager");

  return true;
}
