#pragma once

// MARKETS' quotes on the card and off the network, apart from the screens, so
// HOY can show and refresh the same three without a copy of how. Also what
// the three cards are set to, and the portfolio.

#include <cstddef>
#include <cstdint>

#include "MarketsCore.h"

namespace marketsstore {

// The quote kept from the last fetch. False when there is none this build reads.
bool loadCached(markets::Asset asset, markets::Quote& out);
// Fetches the three cards, blocking, into `quotes` and onto the card, and with
// them whatever else the portfolio holds. Wi-Fi must be up. Returns how many
// of the three arrived; `report` says how many of everything did, and which
// did not.
int fetchAll(markets::Quote quotes[markets::kAssets], char* report, size_t reportCap);

// When the last round that brought something ended, Unix seconds by this
// reader's clock, kept on the card; 0 when never, or the clock was not set.
int64_t fetchedAt();

// What the cards show. loadPicks() reads the card's choice into MarketsCore
// and is safe to call again; every other function here calls it first.
void loadPicks();
void savePicks(const markets::Picks& picks);

void loadPortfolio(markets::Portfolio& out);
void savePortfolio(const markets::Portfolio& portfolio);
// Each holding's price in dollars from the quotes on the card, and the euro's
// (0 when it has not been fetched).
// `previous` is the close the period is measured from.
void pricePortfolio(const markets::Portfolio& portfolio, markets::Priced* priced, double& euro, markets::Period period);

// What was added by ticker or name, kept on the card. loadPicks() reads it.
void saveCustoms();
// Drops the quote kept for an instrument, when its slot is given to another.
void forget(int instrument);
// Asks CNBC's symbol lookup, blocking; Wi-Fi must be up. How many it found,
// or -1 when there was no usable answer (`status` says what came instead).
int search(const char* query, markets::Found* out, int cap, int& status);

}  // namespace marketsstore
