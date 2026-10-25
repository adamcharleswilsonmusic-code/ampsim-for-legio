#pragma once
namespace legio_nam {
// Extend the quiet end only. Existing drive values and the maximum stay the
// same; no makeup gain or change to the captured model/effect controls.
constexpr float kMinimumDriveDb=-36.f;
constexpr float kMaximumDriveDb=24.f;
}
