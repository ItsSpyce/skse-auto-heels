#include "json.hpp"

using namespace nlohmann;
static RE::BGSFootstepSet* heels_sound = nullptr;
static RE::FormID update_piece = NULL;

void update_armor_with_heels(RE::TESObjectARMO* armor) {
  const auto addon =
      armor->GetArmorAddon(RE::PlayerCharacter::GetSingleton()->GetRace());
  if (!addon->footstepSet) {
    logger::info("Armor {} has no footstep set, skipping", armor->GetName());
    return;
  }
  logger::debug("Updating armor {} with heels", armor->GetName());
  addon->footstepSet = heels_sound;
}

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
    // JSON structure we're looking for is [{"name": "NPC", "pos": {
    // "x": 0.0, "y": 0.0, "z": 0.0 } }]
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

bool check_if_player_has_heel() {
  const auto* player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    return false;
  }
  if (!visit_children(player->Get3D(), &is_heel)) {
    return false;
  }
  return true;
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
    if (update_piece == NULL) {
      return;
    }
    auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(update_piece);
    update_piece = NULL;
    if (!armor) {
      logger::info("Armor {} not found", update_piece);
      return;
    }

    if (!check_if_player_has_heel()) {
      logger::info("No heels found");
      return;
    }
    update_armor_with_heels(armor);
  }
  static inline REL::Relocation<decltype(thunk)> func;
  static inline auto idx = 0xAD;
};

class InventoryUpdateSink final : public RE::BSTEventSink<RE::TESEquipEvent> {
 public:
  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESEquipEvent* event,
      RE::BSTEventSource<RE::TESEquipEvent>*) override {
    if (event->actor->IsPlayerRef()) {
      update_piece = event->baseObject;
    }
    return RE::BSEventNotifyControl::kContinue;
  }
};

void handle_message(SKSE::MessagingInterface::Message* msg) {
  auto* event_source_holder = RE::ScriptEventSourceHolder::GetSingleton();
  auto* data_handler = RE::TESDataHandler::GetSingleton();
  switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
      event_source_holder->AddEventSink(new InventoryUpdateSink());
      logger::info("Registered inventory update sink");
      heels_sound = data_handler->LookupForm<RE::BGSFootstepSet>(
          0x004527, "Heels Sound.esm");
      logger::info("heels sound {}", heels_sound ? heels_sound->formID : -1);
      break;
    case SKSE::MessagingInterface::kPostLoadGame:
      const auto* player = RE::PlayerCharacter::GetSingleton();
      if (!player) {
        return;
      }
      const auto *boots = get_player_boots();
      update_piece = boots->formID;
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
