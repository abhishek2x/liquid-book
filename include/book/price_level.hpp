#pragma once

#include <cstdint>

namespace liquidbook
{

  struct PriceLevel
  {
    double price;
    double aggregate_qty;
    int32_t order_count;
  };

} // namespace liquidbook
