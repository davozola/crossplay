#pragma once

// MARKETS' quotes on the card and off the network, apart from the screens, so
// HOY can show and refresh the same three without a copy of how.

#include <cstddef>

#include "MarketsCore.h"

namespace marketsstore {

// The quote kept from the last fetch. False when there is none this build reads.
bool loadCached(markets::Asset asset, markets::Quote& out);
// Fetches all three, blocking, into `quotes` and onto the card. Wi-Fi must be
// up. Returns how many arrived; `report` gets a line on what each source said.
int fetchAll(markets::Quote quotes[markets::kAssets], char* report, size_t reportCap);

}  // namespace marketsstore
