#ifndef EH_SIM_CAPACITOR_HPP
#define EH_SIM_CAPACITOR_HPP

#include <cmath>
#include <cassert>
#include <stdexcept>

namespace ehsim {

/**
 * Calculate the energy stored in a capacitor.
 *
 * @param voltage The voltage across the capacitor in volts (V).
 * @param capacitance The capacitance in farads (F).
 *
 * @return The energy in nJ.
 */
inline double calculate_energy(double const voltage, double const capacitance)
{
  return 0.5 * capacitance * voltage * voltage * 1e9;
}

class capacitor {
public:
  /**
   * Create a capacitor.
   *
   * @param capacitance The capacitance in farads (F).
   * @param maximum_voltage The maximum voltage the capacitor can sustain.
   */
  capacitor(double const capacitance, double const maximum_voltage, double const maximum_current)
      : C(capacitance)
      , maxV(maximum_voltage)
      , maxI(maximum_current)
      , V(0)
      , maximum_energy(calculate_energy(maximum_voltage, C))
      , energy(0)
  {
    assert(maximum_energy > 0);
  }

  /**
   * @return The capacitance in farads (F).
   */
  double capacitance() const
  {
    return C;
  }

  /**
   * @return The maximum voltage in volts (V).
   */
  double max_voltage() const
  {
    return maxV;
  }

  /**
   * @return The maximum current in Ampheres (A).
   */
  double max_current() const
  {
    return maxI;
  }

  /**
   * @return The voltage in volts (V).
   */
  double voltage() const
  {
    return V;
  }

  /**
   * Updates the current voltage across capacitor based on current energy stored
   */
  void update_voltage()
  {
    V = sqrt(2 * energy * 1e-9 / C);
  }

  /**
   * @return The amount of stored energy in nJ.
   */
  double energy_stored() const
  {
    return energy;
  }

  /**
   * @return The maximum amount of energy (nJ) this capacitor can store.
   */
  double maximum_energy_stored() const
  {
    return maximum_energy;
  }

  /**
   * Consume energy from the capacitor.
   *
   * @param energy_to_consume The amount of energy to consume in nJ.
   */
  void consume_energy(double const energy_to_consume)
  {
    assert(energy_to_consume >= 0);
    assert(energy - energy_to_consume >= 0);

    energy -= energy_to_consume;
    update_voltage();
  }

  /**
   * Add energy to the capacitor.
   *
   * @param energy_harvested The amount of energy to harvest in nJ.
   *
   * @return The amount of energy that could be stored.
   */
  double harvest_energy(double const energy_harvested)
  {
    assert(energy_harvested >= 0);

    double can_harvest = energy_harvested;
    if(energy + energy_harvested > maximum_energy) {
      can_harvest = maximum_energy - energy;
    }

    // clamp to maximum energy, avoiding floating point precision errors
    energy = std::min(energy + can_harvest, maximum_energy);
    update_voltage();

    return can_harvest;
  }

private:
  // capacitance
  double const C;
  // maximum energy that can be stored in nJ
  double const maximum_energy;
  // maximum voltage
  double maxV;
  // maximum current
  double maxI;
  // voltage across the capacitor
  double V;
  // stored energy in nJ
  double energy;
};
}

#endif //EH_SIM_CAPACITOR_HPP
