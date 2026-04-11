#include "rf_profiles.h"

// TODO: Protocol channel plans (Phase 3)
// Every constant defined in this file must cite its source in
// docs/JJ_Protocol_Emulation_Reference_v2.md — no approximated values.
// Planned profiles: ELRS ISM2G4, ELRS FCC915, ELRS EU868, Crossfire 868/915,
// FrSky R9, mLRS, Ghost 2G4, FrSky D16/AFHDS2A footprint, generic FHSS.

const RfProfile *rfProfileByName(const char *name) {
    (void)name;
    return nullptr;
}
