
// Rossler chaotic oscillator example
// ==================================
// Warning: not validated, not optimized, not modular


// Method: Runge-Kutta 4
// Needs linking with math library (GCC option -lm)
// Copyright: (c) Petr Peringer, FIT TU Brno, 2018-2020

#include <stdio.h>
#include <math.h>               // sin(), M_PI

// System: Rossler
#define SIZE 3                  // state vector size of the system

// model parameters (can be changed):
const double a = 0.1;          // 
const double b = 0.1;          // 
const double c = 14.0;          // 

enum state_vector_index { ix, iy, iz };
// values of state variables
#define x (st[ix])
#define y (st[iy])
#define z (st[iz])

// Continuous model dynamics
// Input:  t = time of step start,
//         st = state vector = integrator outputs
// Output: yin = vector of derivatives = integrators input values
void f(double t, double *st, double *yin)
{
    yin[ix] = (-y) + (-z);
    yin[iy] = a*y + x;
    yin[iz] = b+(x-c)*z;
}

// Runge-Kutta-4 step
void RK4step(double t, double h, double *st);

// Experiment control
void Run()
{
    // parameters
    const double TEND = 150;
    const double stepsize = TEND/5000;   // RK4 step size and print interval
    printf("# Rossler\n");
    printf("# a = %g\n",a);
    printf("# b = %g\n",b);
    printf("# c = %g\n",c);
    printf("#\n");
    printf("# t       x       y      z\n");
    // initialization of model state
    double st[SIZE] = { 1, 1, 1};   // initial values (try another)
    // run simulation
    double t = 0;                   // start time
    while (t < TEND) {
        printf("%-8.3f %8f %8f %8f\n", t, x, y, z);
        RK4step(t, stepsize, st);   // make step
        t += stepsize;              // advance the simulation time
    }
    printf("\n");
}

int main()
{
    Run();
    return 0;
}

//////////////////////////////////////////////////////////////////
// Runge-Kutta 4-th order method step
// Parameters: t = start time,
//             h = step size,
//             st = state vector
// Output:  new state in st
// Warning: it does not update time
void RK4step(double t, double h, double *st)
{
    double yin[SIZE];           // integrator input = derivative
    double ystart[SIZE];        // initial state
    // 4 stages results:
    double k1[SIZE];
    double k2[SIZE];
    double k3[SIZE];
    double k4[SIZE];

    int i;
    for (i = 0; i < SIZE; i++)  // save initial value
        ystart[i] = st[i];

    f(t, st, yin);               // yin = f(t, st(t))

    for (i = 0; i < SIZE; i++) {
        k1[i] = h * yin[i];     // k1 = h * f(t, st(t))
        st[i] = ystart[i] + k1[i] / 2;
    }

    f(t + h / 2, st, yin);       // yin = f(t+h/2, st(t)+k1/2)

    for (i = 0; i < SIZE; i++) {
        k2[i] = h * yin[i];     // k2 = h * f(t+h/2, st(t)+k1/2)
        st[i] = ystart[i] + k2[i] / 2;
    }

    f(t + h / 2, st, yin);       // yin = f(t+h/2, st(t)+k2/2)

    for (i = 0; i < SIZE; i++) {
        k3[i] = h * yin[i];     // k3 = h * f(t+h/2, st(t)+k2/2)
        st[i] = ystart[i] + k3[i];
    }

    f(t + h, st, yin);           // yin = f(t+h, st(t)+k3)

    for (i = 0; i < SIZE; i++) {
        k4[i] = h * yin[i];     // k4 = h * f(t+h, st(t)+k3)
        // Result:
        // st(t+h) = st(t) + k1/6 + k2/3 + k3/3 + k4/6;
        st[i] = ystart[i] + k1[i] / 6 + k2[i] / 3 + k3[i] / 3 + k4[i] / 6;
    }
    // Return: st = new state at time t+h
}
