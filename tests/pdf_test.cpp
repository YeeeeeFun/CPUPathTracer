// pdf_test.cpp
#include "pdf.h"

#include <cmath>
#include <iostream>

namespace {

bool close_enough(double left, double right, double tolerance = 1e-12) {
    return std::fabs(left - right) <= tolerance;
}

int fail(const char* message) {
    std::cerr << "PDF test failed: " << message << '\n';
    return 1;
}

} // 匿名命名空间

int main() {
    const double low_pdf = .2;
    const double high_pdf = .8;

    const double balance_low = balance_heuristic(low_pdf, high_pdf);
    const double balance_high = balance_heuristic(high_pdf, low_pdf);
    if (!close_enough(balance_low, .2)
        || !close_enough(balance_high, .8)
        || !close_enough(balance_low + balance_high, 1.0)) {
        return fail("balance heuristic weights are incorrect");
    }

    const double power_low = power_heuristic(low_pdf, high_pdf);
    const double power_high = power_heuristic(high_pdf, low_pdf);
    if (!close_enough(power_low, 1.0 / 17.0)
        || !close_enough(power_high, 16.0 / 17.0)
        || !close_enough(power_low + power_high, 1.0)) {
        return fail("power heuristic weights are incorrect");
    }

    if (!close_enough(power_heuristic(1.0, 0.0), 1.0)
        || !close_enough(power_heuristic(0.0, 1.0), 0.0)
        || !close_enough(power_heuristic(0.0, 0.0), 0.0)) {
        return fail("power heuristic zero-PDF handling is incorrect");
    }

    constexpr double selection_probability = .5;
    const double balance_estimator_factor = balance_heuristic(low_pdf, high_pdf)
        / (selection_probability * low_pdf);
    const double mixture_estimator_factor = 1.0
        / (selection_probability * low_pdf + selection_probability * high_pdf);
    if (!close_enough(balance_estimator_factor, mixture_estimator_factor))
        return fail("balance MIS no longer matches the original mixture estimator");

    if (!close_enough(
            mis_weight(mis_heuristic::balance, low_pdf, high_pdf),
            balance_low)
        || !close_enough(
            mis_weight(mis_heuristic::power, low_pdf, high_pdf),
            power_low)) {
        return fail("MIS heuristic dispatch is incorrect");
    }

    std::cout << "PDF and MIS tests passed.\n";
    return 0;
}
