#include "json.hpp"

using namespace nlohmann;
static RE::BGSFootstepSet* heels_sound = nullptr;

bool visit_children(RE::NiAVObject* parent,
                    const std::function<bool(RE::NiAVObject*)>& visitor) {
  if (!parent) {
    return false;
  }
  if (visitor(parent)) {
    return true;
  }
  if (auto* node = parent->AsNode()) {
    for (auto& child : node->children) {
      if (visit_children(child.get(), visitor)) {
        return true;
      }
    }
  }
  return false;
}

bool is_heel(const RE::NiAVObject* obj) {
  if (!obj) {
    return false;
  }
  if (obj->HasExtraData("HH_OFFSET")) {
    return true;
  }
  if (obj->HasExtraData("SDTA")) {
    const auto sdta = obj->GetExtraData<RE::NiStringExtraData>("SDTA");
    if (!sdta) {
      return false;
    }
    if (const auto json = json::parse(sdta->value); json.is_array()) {
      for (const auto& entry : json) {
        if (!entry.is_object()) {
          continue;
        }
        if (entry.contains("name") && entry["name"] == "NPC" &&
            entry.contains("pos")) {
          if (auto pos = entry["pos"]; pos.is_array() && pos.size() == 3) {
            const float y = pos[2];
            return y > 0.5f;
          }
        }
      }
    }
  }
  return false;
}

RE::TESObjectARMO* get_player_boots() {
  auto* player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    return nullptr;
  }
  return player->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
}

struct UpdatePlayerHook {
  static void thunk(RE::PlayerCharacter* player, float delta) {
    func(player, delta);
    auto* boots = get_player_boots();
    if (!boots) {
      return;
    }
    auto* addon = boots->GetArmorAddon(player->GetRace());
    if (!addon || addon->footstepSet->formID == heels_sound->formID) {
      return;
    }

    if (visit_children(player->Get3D(), &is_heel)) {
      addon->footstepSet = heels_sound;
    }
  }
  static inline REL::Relocation<decltype(thunk)> func;
  static inline auto idx = 0xAD;
};

void handle_message(SKSE::MessagingInterface::Message* msg) {
  switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
      auto* data_handler = RE::TESDataHandler::GetSingleton();
      logger::info("Registered inventory update sink");
      heels_sound = data_handler->LookupForm<RE::BGSFootstepSet>(
          0x004527, "Heels Sound.esm");
      logger::info("heels sound {}", heels_sound ? heels_sound->formID : -1);
      break;
  }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
  SKSE::Init(a_skse);
  stl::write_vfunc<RE::PlayerCharacter, UpdatePlayerHook>();
  logger::info("Registered update player hook");

  SKSE::GetMessagingInterface()->RegisterListener(handle_message);

  return true;
}
