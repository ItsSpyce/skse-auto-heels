#pragma once

#include "json.hpp"

using namespace nlohmann;

class HeelUpdateManager final : RE::BSTEventSink<RE::TESEquipEvent>,
                                RE::BSTEventSink<RE::TESLoadGameEvent> {
 public:
  void install() {
    if (auto* event_dispatcher = RE::ScriptEventSourceHolder::GetSingleton()) {
      event_dispatcher->AddEventSink<RE::TESEquipEvent>(this);
      event_dispatcher->AddEventSink<RE::TESLoadGameEvent>(this);
    }
  }

  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESEquipEvent* event,
      RE::BSTEventSource<RE::TESEquipEvent>*) override {
    if (!event->actor->IsPlayerRef()) {
      return RE::BSEventNotifyControl::kContinue;
    }
    if (std::ranges::find(already_updated_cache_, event->baseObject) !=
        already_updated_cache_.end()) {
      return RE::BSEventNotifyControl::kContinue;
    }
    if (!needs_update_) {
      needs_update_ = true;
      already_updated_cache_.emplace_back(event->baseObject);
    }
    return RE::BSEventNotifyControl::kContinue;
  }

  RE::BSEventNotifyControl ProcessEvent(
      const RE::TESLoadGameEvent*,
      RE::BSTEventSource<RE::TESLoadGameEvent>*) override {
    logger::info("Received load game event");
    if (try_lookup_heel_sound()) {
      logger::info("Heels sound loaded");
      needs_update_ = true;
    } else {
      logger::error("Failed to load heels sound");
    }
    return RE::BSEventNotifyControl::kContinue;
  }

  [[nodiscard]] bool needs_update() const { return needs_update_; }

  bool reset_non_feet_armors(RE::PlayerCharacter* player) const {
    if (!player) {
      return false;
    }

    bool did_update = false;
    for (uint32_t i = 1; i < 30; i++) {
      const auto slot =
          static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1 << i);
      if (slot == RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) {
        continue;
      }
      auto* armor = player->GetWornArmor(slot);
      if (!armor) {
        continue;
      }
      if (std::ranges::find(already_updated_cache_, armor->formID) !=
          already_updated_cache_.end()) {
        continue;
      }
      already_updated_cache_.push_back(armor->formID);
      auto* addon = armor->GetArmorAddon(player->GetRace());
      if (!addon) {
        continue;
      }
      if (addon->footstepSet &&
          addon->footstepSet->formID == heels_sound_->formID) {
        addon->footstepSet = nullptr;
        did_update = true;
      }
    }
    return did_update;
  }

  bool update(RE::PlayerCharacter* player) {
    needs_update_ = false;
    if (visit_children(player->Get3D(), &is_heel)) {
      update_footstep_sound_for_slot(
          player, RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, heels_sound_);
      return true;
    }
    logger::info("No heels found");
    return false;
  }

 private:
  static bool try_lookup_heel_sound() {
    if (heels_sound_) {
      return true;
    }
    auto* data_handler = RE::TESDataHandler::GetSingleton();
    if (!data_handler) {
      return false;
    }
    heels_sound_ = data_handler->LookupForm<RE::BGSFootstepSet>(
        0x004527, "Heels Sound.esm");
    return heels_sound_ != nullptr;
  }

  static bool visit_children(
      RE::NiAVObject* parent,
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

  static bool is_heel(const RE::NiAVObject* obj) {
    if (!obj) {
      return false;
    }
    const auto name = obj->name.c_str();
    if (is_heel_cache_.contains(name)) {
      return is_heel_cache_[name];
    }
    if (obj->HasExtraData("HH_OFFSET")) {
      is_heel_cache_[name] = true;
      return true;
    }
    if (obj->HasExtraData("SDTA")) {
      const auto sdta = obj->GetExtraData<RE::NiStringExtraData>("SDTA");
      if (!sdta || !sdta->value) {
        is_heel_cache_[name] = false;
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
              const auto is_heel = y > 0.5f;
              is_heel_cache_[name] = is_heel;
              return is_heel;
            }
          }
        }
      }
    }
    return false;
  }

  static void update_footstep_sound_for_slot(
      RE::PlayerCharacter* player,
      const RE::BGSBipedObjectForm::BipedObjectSlot slot,
      RE::BGSFootstepSet* footstep_set) {
    if (!player) {
      return;
    }
    auto* armor = player->GetWornArmor(slot);
    if (!armor) {
      return;
    }
    auto* addon = armor->GetArmorAddon(player->GetRace());
    if (!addon) {
      return;
    }
    addon->footstepSet = footstep_set;
  }

  bool needs_update_ = false;
  static inline RE::BGSFootstepSet* heels_sound_ = nullptr;
  static inline std::unordered_map<const char*, bool> is_heel_cache_{};
  static inline std::vector<RE::FormID> already_updated_cache_{};
};