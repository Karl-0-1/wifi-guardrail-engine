#include <iostream>
#include <string>
#include <map>
#include <optional> // For ChangeRequest
#include <cmath>    // For std::abs
#include <cassert>  // For unit tests

// --- Configuration ---
const int CHANGE_BUDGET_MINUTES = 4 * 60; // 4 hours
const int HYSTERESIS_THRESHOLD_DB = 2;

// --- Data Structures ---

struct AccessPoint {
    std::string id;
    int channel;
    int power_db;
    // Default last_change_time to allow a change at time 0
    int last_change_time_minutes = -CHANGE_BUDGET_MINUTES - 1;
};

struct ChangeRequest {
    // Use std::optional to signify that a change is not requested
    std::optional<int> new_channel = std::nullopt;
    std::optional<int> new_power_db = std::nullopt;
    bool is_emergency = false;
};

// --- The Rules Engine ---

class SafeChangePlanner {
private:
    // The 'network_state' holds the ground truth for all APs
    std::map<std::string, AccessPoint> network_state;

public:
    void add_ap(const AccessPoint& ap) {
        network_state[ap.id] = ap;
        std::cout << "[State] Added AP: " << ap.id 
                  << " (Ch: " << ap.channel << ", Pwr: " << ap.power_db << "dB)\n";
    }

    AccessPoint get_ap_state(const std::string& ap_id) {
        return network_state.at(ap_id);
    }

    /**
     * Processes a change request against all guardrail rules.
     * @return True if the change is ACCEPTED and applied.
     * @return False if the change is REJECTED.
     */
    bool process_request(const std::string& ap_id, 
                         const ChangeRequest& request, 
                         int current_time_minutes, 
                         bool is_peak_hour) 
    {
        if (network_state.find(ap_id) == network_state.end()) {
            std::cout << "[Planner] REJECT: AP '" << ap_id << "' not found in network state.\n";
            return false;
        }

        // Get a reference to the AP in the map to modify it directly
        AccessPoint& ap = network_state.at(ap_id);
        
        std::cout << "\n--- Processing Request for " << ap_id 
                  << " at T=" << current_time_minutes << " ---\n";

        // --- Rule 1: Time Windows (Peak Hour Avoidance) ---
        if (is_peak_hour && !request.is_emergency) {
            std::cout << "[Planner] REJECT: Change blocked by Time Window (Peak Hour).\n";
            return false;
        }

        // --- Rule 2: Change Budgets (Rate Limiting) ---
        int time_since_last_change = current_time_minutes - ap.last_change_time_minutes;
        if (time_since_last_change < CHANGE_BUDGET_MINUTES) {
            std::cout << "[Planner] REJECT: Change blocked by Budget (Last change " 
                      << time_since_last_change << " min ago).\n";
            return false;
        }

        // --- Rule 3: Hysteresis (Preventing "flapping") ---
        if (request.new_power_db.has_value()) {
            int power_change_amount = std::abs(request.new_power_db.value() - ap.power_db);
            if (power_change_amount < HYSTERESIS_THRESHOLD_DB) {
                std::cout << "[Planner] REJECT: Change blocked by Hysteresis (Delta: " 
                          << power_change_amount << "dB).\n";
                return false;
            }
        }

        // --- All Rules Passed ---
        std::cout << "[Planner] ACCEPT: All guardrails passed.\n";

        bool changed = false;
        if (request.new_channel.has_value() && ap.channel != request.new_channel.value()) {
            ap.channel = request.new_channel.value();
            std::cout << "[State]   Applied channel -> " << ap.channel << "\n";
            changed = true;
        }
            
        if (request.new_power_db.has_value() && ap.power_db != request.new_power_db.value()) {
            ap.power_db = request.new_power_db.value();
            std::cout << "[State]   Applied power -> " << ap.power_db << "dB\n";
            changed = true;
        }

        if (changed) {
            ap.last_change_time_minutes = current_time_minutes;
        } else {
            std::cout << "[Planner] Info: Request accepted but no state change occurred.\n";
        }

        return true;
    }
};

// --- Unit Tests ---
int main() {
    std::cout << "======== Running Safe-Change Guardrail Tests ========\n";
    
    SafeChangePlanner planner;
    AccessPoint ap1 = {"AP-001", 6, 20, 0};
    planner.add_ap(ap1);

    // --- Test 1: Test_Reject_If_Change_Too_Soon ---
    ChangeRequest req1 = { .new_channel = 11 };
    bool res1 = planner.process_request("AP-001", req1, 100, false);
    assert(res1 == false);
    assert(planner.get_ap_state("AP-001").channel == 6);

    // --- Test 2: Test_Accept_If_Change_After_Budget ---
    ChangeRequest req2 = { .new_channel = 11 };
    bool res2 = planner.process_request("AP-001", req2, 250, false);
    assert(res2 == true);
    assert(planner.get_ap_state("AP-001").channel == 11);
    assert(planner.get_ap_state("AP-001").last_change_time_minutes == 250);

    // --- Test 3: Test_Reject_If_Hysteresis_Too_Small ---
    ChangeRequest req3 = { .new_power_db = 21 };
    bool res3 = planner.process_request("AP-001", req3, 500, false);
    assert(res3 == false);
    assert(planner.get_ap_state("AP-001").power_db == 20);

    // --- Test 4: Test_Accept_If_Hysteresis_Large_Enough ---
    ChangeRequest req4 = { .new_power_db = 22 };
    bool res4 = planner.process_request("AP-001", req4, 500, false);
    assert(res4 == true);
    assert(planner.get_ap_state("AP-001").power_db == 22);
    assert(planner.get_ap_state("AP-001").last_change_time_minutes == 500);

    // --- Test 5: Test_Reject_If_Peak_Hour_And_Not_Emergency ---
    ChangeRequest req5 = { .new_channel = 1 };
    bool res5 = planner.process_request("AP-001", req5, 800, true);
    assert(res5 == false);
    assert(planner.get_ap_state("AP-001").channel == 11);

    // --- Test 6: Test_Accept_If_Peak_Hour_And_Is_Emergency ---
    ChangeRequest req6 = { .new_channel = 1, .is_emergency = true };
    bool res6 = planner.process_request("AP-001", req6, 800, true);
    assert(res6 == true);
    assert(planner.get_ap_state("AP-001").channel == 1);
    assert(planner.get_ap_state("AP-001").last_change_time_minutes == 800);

    // --- Test 7: Test_Channel_Change_Only_Skips_Hysteresis ---
    ChangeRequest req7 = { .new_channel = 6 };
    bool res7 = planner.process_request("AP-001", req7, 1100, false);
    assert(res7 == true);
    assert(planner.get_ap_state("AP-001").channel == 6);
    assert(planner.get_ap_state("AP-001").last_change_time_minutes == 1100);

    std::cout << "\n======== All C++ Tests Passed Successfully! ========\n";
    return 0;
}
