# Wi-Fi "Safe-Change Guardrail" Engine

This is a lightweight C++ rules engine designed to act as a "gatekeeper" for a Wi-Fi network controller. Its main purpose is to ensure **network stability** by validating change requests against a set of guardrail rules.

It prevents a "flapping" network (constant, disruptive changes) and avoids changes during critical peak hours.

## Features (The "Guardrails")

This engine checks all incoming change requests (like "change channel" or "change power") against three main rules:

1.  **Change Budgets:** An Access Point (AP) cannot be changed if it has already been changed within the last 4 hours.
2.  **Hysteresis:** A request to change an AP's power is rejected if the change is too small (less than 2 dB). This prevents tiny, pointless changes that still cause client disconnections.
3.  **Time Windows:** All non-emergency changes are automatically rejected if they are requested during a "peak hour" (e.g., 9 AM - 5 PM on a weekday).

## How to Build and Run

This project is a single, self-contained C++ file with no external dependencies (besides the C++20 standard library for `std::optional`).

### 1. Build (Compile)

You will need a C++ compiler that supports C++20.

```bash
# Using g++
g++ -std=c++20 -o planner planner.cpp
