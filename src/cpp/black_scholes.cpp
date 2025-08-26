#include "black_scholes.hpp"
#include <math.h>

namespace order_flow{
namespace pricing{


double reference_cdf(double x){
    double CDF = 0.5 * (1.0 + erf(x/(sqrt(2.0))));
    return CDF;
}   

// aprox via taylor series
double BlackScholes::approx_CDF(double x, CDF_precompute CDF){
    double abs_x = fabs(x);
    double t = 1.0 /(1.0 + CDF.p * abs_x);
    double t2 = t*t;
    double t3 = t2 * t;
    double t4 = t3 * t;
    double t5 = t4 * t;

    //aproximate via a polynomial to fit the CDF
    double poly = CDF.a1 * t + CDF.a2 *t2 + CDF.a3 * t3 + CDF.a4 * t4 + CDF.a5 * t5;
    double exp_part = exp(-0.5 * abs_x * abs_x);
    double inv_sqrt_2pi = 1.0 / sqrt(2.0 * M_PI);

    double result = 1.0 - inv_sqrt_2pi * exp_part * poly;

    if (x < 0){
        result = 1.0 - result;
    }
    return result;
}

double BlackScholes::calculate_error(const std::vector<double>& x_values, CDF_precompute CDF){
    double total_error = 0.0;
    for (double x : x_values){
        double true_cdf = reference_cdf(x);
        double approx = approx_CDF(x, CDF);
        double error = true_cdf - approx;
        total_error += error * error;
    }
    return total_error;
}

void BlackScholes::gradient_descent(const std::vector<double>& x_values, CDF_precompute& CDF){
    double initial_learning_rate = 0.001;
    const double beta1 = 0.9;
    const double beta2 = 0.999;
    const double epsilon = 1e-8;
    const int max_iterations = 2000;

    double m_a1 = 0, m_a2 = 0, m_a3 = 0, m_a4 = 0, m_a5 = 0, m_p = 0;
    double v_a1 = 0, v_a2 = 0, v_a3 = 0, v_a4 = 0, v_a5 = 0, v_p = 0;

    double best_error = std::numeric_limits<double>::max();
    CDF_precompute best_CDF = CDF;

    for (int i = 0; i < max_iterations; i++) {
        double error = calculate_error(x_values, CDF);

        if (error < best_error) {
            best_error = error;
            best_CDF = CDF;
        }
        double grad_a1 = 0, grad_a2 = 0, grad_a3 = 0, grad_a4 = 0, grad_a5 = 0, grad_p = 0;
        const double h = 1e-5;

        // Compute gradients
        for (double x : x_values) {
            double true_cdf = reference_cdf(x);
            double approx = approx_CDF(x, CDF);
            double error = true_cdf - approx;
            CDF.a1 += h;
            double approx_a1 = approx_CDF(x, CDF);
            grad_a1 += (approx_a1 - approx) / h;
            CDF.a1 -= h;
            CDF.a2 += h;
            double approx_a2 = approx_CDF(x, CDF);
            grad_a2 += (approx_a2 - approx) / h;
            CDF.a2 -= h;

            CDF.a3 += h;
            double approx_a3 = approx_CDF(x, CDF);
            grad_a3 += (approx_a3 - approx) / h;
            CDF.a3 -= h;

            CDF.a4 += h;
            double approx_a4 = approx_CDF(x, CDF);
            grad_a4 += (approx_a4 - approx) / h;
            CDF.a4 -= h;

            CDF.a5 += h;
            double approx_a5 = approx_CDF(x, CDF);
            grad_a5 += (approx_a5 - approx) / h;
            CDF.a5 -= h;

            CDF.p += h;
            double approx_p = approx_CDF(x, CDF);
            grad_p += (approx_p - approx) / h;
            CDF.p -= h;

        }
        m_a1 = beta1 * m_a1 + (1 - beta1) * grad_a1;
        m_a2 = beta1 * m_a2 + (1 - beta1) * grad_a2;
        m_a3 = beta1 * m_a3 + (1 - beta1) * grad_a3;
        m_a4 = beta1 * m_a4 + (1 - beta1) * grad_a4;
        m_a5 = beta1 * m_a5 + (1 - beta1) * grad_a5;
        m_p = beta1 * m_p + (1 - beta1) * grad_p;

        v_a1 = beta2 * v_a1 + (1 - beta2) * grad_a1 * grad_a1;
        v_a2 = beta2 * v_a2 + (1 - beta2) * grad_a2 * grad_a2;
        v_a3 = beta2 * v_a3 + (1 - beta2) * grad_a3 * grad_a3;
        v_a4 = beta2 * v_a4 + (1 - beta2) * grad_a4 * grad_a4;
        v_a5 = beta2 * v_a5 + (1 - beta2) * grad_a5 * grad_a5;
        v_p = beta2 * v_p + (1 - beta2) * grad_p * grad_p;

        double m_a1_hat = m_a1 / (1 - pow(beta1, i + 1));
        double m_a2_hat = m_a2 / (1 - pow(beta1, i + 1));
        double m_a3_hat = m_a3 / (1 - pow(beta1, i + 1));
        double m_a4_hat = m_a4 / (1 - pow(beta1, i + 1));
        double m_a5_hat = m_a5 / (1 - pow(beta1, i + 1));
        double m_p_hat = m_p / (1 - pow(beta1, i + 1));

        double v_a1_hat = v_a1 / (1 - pow(beta2, i + 1));
        double v_a2_hat = v_a2 / (1 - pow(beta2, i + 1));
        double v_a3_hat = v_a3 / (1 - pow(beta2, i + 1));
        double v_a4_hat = v_a4 / (1 - pow(beta2, i + 1));
        double v_a5_hat = v_a5 / (1 - pow(beta2, i + 1));
        double v_p_hat = v_p / (1 - pow(beta2, i + 1));

        CDF.a1 -= initial_learning_rate * m_a1_hat / (sqrt(v_a1_hat) + epsilon);
        CDF.a2 -= initial_learning_rate * m_a2_hat / (sqrt(v_a2_hat) + epsilon);
        CDF.a3 -= initial_learning_rate * m_a3_hat / (sqrt(v_a3_hat) + epsilon);
        CDF.a4 -= initial_learning_rate * m_a4_hat / (sqrt(v_a4_hat) + epsilon);
        CDF.a5 -= initial_learning_rate * m_a5_hat / (sqrt(v_a5_hat) + epsilon);
        CDF.p -= initial_learning_rate * m_p_hat / (sqrt(v_p_hat) + epsilon);
    }
}

CDF_precompute BlackScholes::computeCoefficents(){
    //gives approx for CDF using method from Abramowitz and Stegun
    CDF_precompute CDF;
    std::vector<double> x_values;

    for (double x = -3.0; x < 3.0; x += 0.01){
        x_values.push_back(x);
    }
    for (double x = -8.0; x < -3.0; x += 0.1) {
        x_values.push_back(x);
    }
    for (double x = 3.0; x <= 8.0; x += 0.1) {
        x_values.push_back(x);
    }    

    //init guesses close to the actual solutions
    CDF.p = 0.2316419;
    CDF.a1 = 0.31938153;
    CDF.a2 = -0.356563782;
    CDF.a3 = 1.781477937;
    CDF.a4 = -1.821255978;
    CDF.a5 = 1.330274429;

    //gradient descent
    gradient_descent(x_values, CDF);
    
    return CDF;// Added return statement to match double return type
}



double BlackScholes::normalCDF(double x){
    return 0.0;
}

//ending namespace
    }
}