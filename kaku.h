#ifndef KAKU_H
#define KAKU_H

#include <esphome/core.h>

namespace kaku {
  class KakuComponent : public esphome::Component {
  public:
    void setup() override;
    void loop() override;
  private:
    // Internal state or dependencies
  };
}

#endif // KAKU_H