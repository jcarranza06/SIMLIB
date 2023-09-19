////////////////////////////////////////////////////////////////////////////
// Model: 2x mass on spring, vertical axis only

#include "simlib.h"

Constant g=9.81;        // gravity acceleration

// system:   y'' = ( F - D * y' - k * y ) / M
//  --------
//     <
//      >
//     <
//    (m1)
//     <
//      >
//     <
//    (m2)

// mass block: input=force, output=position
struct Mass /*: public aContiBlock1*/ {
  const double mass;
  Integrator speed;
  Integrator position;
  Mass(Input force, double m, double pos0):
    /*aContiBlock1( force ),*/
    mass( m ),
    speed( -g + force/mass ),
    position( speed, pos0 ) {}
  /*double Value() { return position.Value(); }*/
};

// spring block: input1=position1, input2=position2, output=force
struct Spring : aContiBlock2 {
  const double K;
  const double L0;
  Spring(Input p1, Input p2, double k, double l0) :
    aContiBlock2(p1,p2),
    K(k),
    L0(l0) {}
  double Value() {
    double len = Input1Value() - Input2Value();
    if(len <= L0/4) Error("Spring too compressed");
    return K*(len-L0);
  }
};


const double y0_0 = 5;          // = spring1 upper position
const double K1 = 10;           // spring1 stiffness
const double L1_0 = 1;          // zero-force spring1 length
const double y1_0 = y0_0 - 3;   // = mass1 initial position
const double K2 = 30;           // spring2 stiffness
const double L2_0 = 1;          // zero-force spring2 length
const double y2_0 = y0_0 - 5;   // = mass2 initial position

// 2 masses + 2 springs
struct System2 {
  Spring f1;
  Mass m1;
  Spring f2;
  Mass m2;
//
  System2() :
    m1(f1-f2, 2.0, y1_0),
    m2(f2, 1.0, y2_0),
    f1(y0_0, m1.position, K1, L1_0),
    f2(m1.position, m2.position, K2, L2_0)
    {}
};

// model instance

System2 s;

// model state sampling
void Sample() {
  Print("%g  %g  %g\n", T.Value(),
             s.m1.position.Value(), s.m2.position.Value());
}
Sampler S(Sample, 0.01);

// experiment control
int main() {
  SetOutput("test.dat");
  _Print("# model: 2x mass+spring\n");
  Init(0,50);                     // simulation time span
  SetStep(1e-4,0.01);             // integration stepsize range
  SetAccuracy(1e-5,0.001);
  Run();                          // simulation
}

