#pragma once

#include <juce_core/juce_core.h>

namespace kickforge::presets
{

// Migration d'un état sauvegardé v1 (kickforge_params_v1) vers le format v2 :
// - le tag racine devient kickforge_params_v2 ;
// - `punch` (transitoire v1) devient `atkLevel` (couche Attack, même plage) ;
// - les paramètres v2 absents prendront leurs défauts au replaceState
//   (crunchLevel 0 % => un projet v1 rechargé sonne quasi identique).
// No-op si l'état est déjà en v2.
inline void migrateStateToV2 (juce::XmlElement& state)
{
    if (! state.hasTagName ("kickforge_params_v1"))
        return;

    state.setTagName ("kickforge_params_v2");

    for (auto* param : state.getChildWithTagNameIterator ("PARAM"))
        if (param->getStringAttribute ("id") == "punch")
            param->setAttribute ("id", "atkLevel");
}

} // namespace kickforge::presets
