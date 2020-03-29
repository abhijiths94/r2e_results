#ifndef EH_SIM_EH_MODEL_HPP
#define EH_SIM_EH_MODEL_HPP

#include "stats.hpp"

namespace ehsim {

enum class dead_cycles { best_case, worst_case, average_case };

inline double estimate_eh_progress(eh_model_parameters const &eh,
    dead_cycles case_D,
    double omega_R,
    double sigma_R,
    double A_R,
    double omega_B,
    double sigma_B,
    double A_B)
{
  double tau_D = 0.0;
  if(case_D == dead_cycles::best_case) {
    tau_D = 0.0;
  } else if(case_D == dead_cycles::average_case) {
    tau_D = eh.tau_B / 2.0;
  } else if(case_D == dead_cycles::worst_case) {
    tau_D = eh.tau_B;
  }

  // calculate dead energy - Equation 5
  double const e_D = (eh.epsilon - eh.epsilon_C) * tau_D;

  double e_R = 0.0;
  if(eh.do_restore) {
    // calculate restore energy - Equation 8
    e_R = (omega_R - eh.epsilon_C / sigma_R) * (A_R + eh.alpha_R * tau_D);
  }

  // numerator of Equation 9
  double const numerator = 1 - (e_D / eh.E) - (e_R / eh.E);

  // calculate backup energy = Equation 9
  double const e_B = (omega_B - eh.epsilon_C / sigma_B) * (A_B + eh.alpha_B * eh.tau_B);

  // denominator of Equation 9 (assume e_Theta is 0)
  double const denominator =
      (1 + e_B / ((eh.epsilon - eh.epsilon_C) * eh.tau_B)) * (1 - eh.epsilon_C / eh.epsilon);

  return numerator / denominator;
}
}

#endif //EH_SIM_EH_MODEL_HPP
