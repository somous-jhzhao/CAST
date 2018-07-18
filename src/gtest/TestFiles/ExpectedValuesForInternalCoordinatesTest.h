#ifdef GOOGLE_MOCK

#ifndef WRITE_OUTPUT_FOR_F_MATRIX_DERIVATIVES
#define WRITE_OUTPUT_FOR_F_MATRIX_DERIVATIVES

#include<string>
#include<vector>
#include"../../coords.h"

namespace ExpectedValuesForInternalCoordinates {

  coords::Representation_3D createSystemOfTwoMethanolMolecules(){
    return coords::Representation_3D{ coords::r3{ -6.053, -0.324, -0.108 },
      coords::r3{ -4.677, -0.093, -0.024 },
      coords::r3{ -6.262, -1.158, -0.813 },
      coords::r3{ -6.582, 0.600, -0.424 },
      coords::r3{ -6.431, -0.613, 0.894 },
      coords::r3{ -4.387, 0.166, -0.937 },
      coords::r3{ -6.146, 3.587, -0.024 },
      coords::r3{ -4.755, 3.671, -0.133 },
      coords::r3{ -6.427, 2.922, 0.821 },
      coords::r3{ -6.587, 3.223, -0.978 },
      coords::r3{ -6.552, 4.599, 0.179 },
      coords::r3{ -4.441, 2.753, -0.339 } };
  }

  std::vector<std::string> createSequenceOfSymbolsForTwoMethanolMolecules() {
    return { "C", "O", "H", "H", "H", "H", "C", "O", "H", "H", "H", "H" };
  }

  coords::Representation_3D createInitialMethanolForRotationSystem() {
    return coords::Representation_3D{ coords::r3{ -0.321, -0.087, 0.12733 },
      coords::r3{ 1.055, 0.144, 0.21133 },
      coords::r3{ -0.53, -0.921, -0.57767 },
      coords::r3{ -0.85, 0.837, -0.18867 },
      coords::r3{ -0.699, -0.376, 1.12933 },
      coords::r3{ 1.345, 0.403, -0.70167 } };
  }

  std::vector<std::string> createSequenceOfSymbolsForInitialMethanolForRotationSystem() {
    return  { "C", "O", "H", "H", "H", "H" };
  }

  coords::Representation_3D createRotatedMethanolForRotationSystem() {
    return coords::Representation_3D{ coords::r3{ -0.11699, -0.10048, 0.32100 },
      coords::r3{ 0.08440, -0.24140, -1.05500 },
      coords::r3{ -0.74011, 0.79636, 0.53000 },
      coords::r3{ 0.85731, -0.03439, 0.85000 },
      coords::r3{ -0.65548, -0.99354, 0.69900 },
      coords::r3{ 0.57087, 0.57345, -1.34500 } };
  }

  std::vector<std::string> createSequenceOfSymbolsForRotatedMethanolForRotationSystem() {
    return { "C", "O", "H", "H", "H", "H" };
  }

  coords::r3 constexpr firstBondAtomDerivative() {
    return coords::r3{ -0.98441712304088669, -0.16526188620817209,
             -0.060095231348426204 };
  }

  coords::r3 constexpr secondBondAtomDerivative() {
    return coords::r3{ 0.98441712304088669, 0.16526188620817209, 0.060095231348426204 };
  }

  coords::r3 constexpr leftAngleAtomDerivative() {
    return coords::r3{ -0.062056791850036874, 0.27963965489457271, 0.24754030125005314 };
  }

  coords::r3 constexpr middleAngleAtomDerivative() {
    return coords::r3{ -0.10143003133725584, -0.13768014867512546, -0.11073454633478358 };
  }

  coords::r3 constexpr rightAngleAtomDerivative() {
    return coords::r3{ 0.16348682318729271, -0.14195950621944725, -0.13680575491526956 };
  }

  coords::r3 constexpr leftLeftDihedralDerivative() {
    return coords::r3{ 0.047760930904702938, -0.49023624122103959, 0.56578012853796311 };
  }

  coords::r3 constexpr leftMiddleDihedralDerivative() {
    return coords::r3{ -0.056149623980491933, 0.17150456631353736, -0.11870115223680294 };
  }

  coords::r3 constexpr rightMiddleDihedralDerivative() {
    return coords::r3{ -0.084250009050912372, 0.23597149360212683, -0.14926917153430774 };
  }

  coords::r3 constexpr rightRightDihedralDerivative() {
    return coords::r3{ 0.092638702126701375, 0.082760181305375408, -0.29780980476685243 };
  }

  scon::mathmatrix<double> expectedValuesForF() {
    return scon::mathmatrix<double>{ { 6.84606631495125, -4.57230316634422,
                                       -13.0165563000258, 3.74086935468525 },
                                     { -4.57230316634422, 0.41134631529563,
                                       9.81533983441827, -19.2451004432376 },
                                     { -13.0165563000258, 9.81533983441827,
                                       -8.15722162019127, -10.0102206984924 },
                                     { 3.74086935468525, -19.2451004432376,
                                       -10.0102206984924, 0.899808989944389 } };
  }

  scon::mathmatrix<double> expectedEigenValuesForF() {
    return scon::mathmatrix<double>{ {
                                         -18.7538392250112,
                                     },
                                     {
                                         -18.0459463050214,
                                     },
                                     {
                                         6.10328376463374,
                                     },
                                     {
                                         30.6965017653989,
                                     } };
  }

  scon::mathmatrix<double> expectedEigenVectorsForF() {
    return scon::mathmatrix<double>{ { -0.199380447172903, 0.342091987934248,
                                       -0.811125403595648, 0.430460321885915 },
                                     { -0.549673535066269, -0.495167463863083,
                                       -0.371435099023426, -0.560984986245274 },
                                     { -0.401611672479942, 0.783069660011025,
                                       0.200535739330358, -0.430459509535228 },
                                     { -0.70485069813454, -0.156793373880449,
                                       0.404841900136933, 0.56098517550819 } };
  }

  auto constexpr * provideExpectedValuesForFMatrixDerivativesInFile() {

    return "  -0.606602086271963   0.000000000000000  -0.240625126641630  -0.164406172914831\n"
      "   0.000000000000000  -0.606602086271963  -0.164406172914831   0.240625126641630\n"
      "  -0.240625126641630  -0.164406172914831   0.606602086271963   0.000000000000000\n"
      "  -0.164406172914831   0.240625126641630   0.000000000000000   0.606602086271963\n"
      "  -0.164406172914831   0.240625126641630   0.000000000000000   0.606602086271963\n"
      "   0.240625126641630   0.164406172914831  -0.606602086271963   0.000000000000000\n"
      "   0.000000000000000  -0.606602086271963  -0.164406172914831   0.240625126641630\n"
      "   0.606602086271963   0.000000000000000   0.240625126641630   0.164406172914831\n"
      "   0.240625126641630   0.164406172914831  -0.606602086271963   0.000000000000000\n"
      "   0.164406172914831  -0.240625126641630   0.000000000000000  -0.606602086271963\n"
      "  -0.606602086271963   0.000000000000000  -0.240625126641630  -0.164406172914831\n"
      "   0.000000000000000  -0.606602086271963  -0.164406172914831   0.240625126641630\n"
      "   1.993661062358009   0.000000000000000  -0.399362121180088   0.272120562065927\n"
      "   0.000000000000000   1.993661062358009   0.272120562065927   0.399362121180088\n"
      "  -0.399362121180088   0.272120562065927  -1.993661062358009   0.000000000000000\n"
      "   0.272120562065927   0.399362121180088   0.000000000000000  -1.993661062358009\n"
      "   0.272120562065927   0.399362121180088   0.000000000000000  -1.993661062358009\n"
      "   0.399362121180088  -0.272120562065927   1.993661062358009   0.000000000000000\n"
      "   0.000000000000000   1.993661062358009   0.272120562065927   0.399362121180088\n"
      "  -1.993661062358009   0.000000000000000   0.399362121180088  -0.272120562065927\n"
      "   0.399362121180088  -0.272120562065927   1.993661062358009   0.000000000000000\n"
      "  -0.272120562065927  -0.399362121180088   0.000000000000000   1.993661062358009\n"
      "   1.993661062358009   0.000000000000000  -0.399362121180088   0.272120562065927\n"
      "   0.000000000000000   1.993661062358009   0.272120562065927   0.399362121180088\n"
      "  -1.001554846492649   0.000000000000000   1.091631791806139  -1.740437761546660\n"
      "   0.000000000000000  -1.001554846492649  -1.740437761546660  -1.091631791806139\n"
      "   1.091631791806139  -1.740437761546660   1.001554846492649   0.000000000000000\n"
      "  -1.740437761546660  -1.091631791806139   0.000000000000000   1.001554846492649\n"
      "  -1.740437761546660  -1.091631791806139   0.000000000000000   1.001554846492649\n"
      "  -1.091631791806139   1.740437761546660  -1.001554846492649   0.000000000000000\n"
      "   0.000000000000000  -1.001554846492649  -1.740437761546660  -1.091631791806139\n"
      "   1.001554846492649   0.000000000000000  -1.091631791806139   1.740437761546660\n"
      "  -1.091631791806139   1.740437761546660  -1.001554846492649   0.000000000000000\n"
      "   1.740437761546660   1.091631791806139   0.000000000000000  -1.001554846492649\n"
      "  -1.001554846492649   0.000000000000000   1.091631791806139  -1.740437761546660\n"
      "   0.000000000000000  -1.001554846492649  -1.740437761546660  -1.091631791806139\n"
      "  -1.606267206639154   0.000000000000000   0.356528329003044   1.581700767008202\n"
      "   0.000000000000000  -1.606267206639154   1.581700767008202  -0.356528329003044\n"
      "   0.356528329003044   1.581700767008202   1.606267206639154   0.000000000000000\n"
      "   1.581700767008202  -0.356528329003044   0.000000000000000   1.606267206639154\n"
      "   1.581700767008202  -0.356528329003044   0.000000000000000   1.606267206639154\n"
      "  -0.356528329003044  -1.581700767008202  -1.606267206639154   0.000000000000000\n"
      "   0.000000000000000  -1.606267206639154   1.581700767008202  -0.356528329003044\n"
      "   1.606267206639154   0.000000000000000  -0.356528329003044  -1.581700767008202\n"
      "  -0.356528329003044  -1.581700767008202  -1.606267206639154   0.000000000000000\n"
      "  -1.581700767008202   0.356528329003044   0.000000000000000  -1.606267206639154\n"
      "  -1.606267206639154   0.000000000000000   0.356528329003044   1.581700767008202\n"
      "   0.000000000000000  -1.606267206639154   1.581700767008202  -0.356528329003044\n"
      "  -1.320918561695022   0.000000000000000  -2.134130704350374  -0.710537023172143\n"
      "   0.000000000000000  -1.320918561695022  -0.710537023172143   2.134130704350374\n"
      "  -2.134130704350374  -0.710537023172143   1.320918561695022   0.000000000000000\n"
      "  -0.710537023172143   2.134130704350374   0.000000000000000   1.320918561695022\n"
      "  -0.710537023172143   2.134130704350374   0.000000000000000   1.320918561695022\n"
      "   2.134130704350374   0.710537023172143  -1.320918561695022   0.000000000000000\n"
      "   0.000000000000000  -1.320918561695022  -0.710537023172143   2.134130704350374\n"
      "   1.320918561695022   0.000000000000000   2.134130704350374   0.710537023172143\n"
      "   2.134130704350374   0.710537023172143  -1.320918561695022   0.000000000000000\n"
      "   0.710537023172143  -2.134130704350374   0.000000000000000  -1.320918561695022\n"
      "  -1.320918561695022   0.000000000000000  -2.134130704350374  -0.710537023172143\n"
      "   0.000000000000000  -1.320918561695022  -0.710537023172143   2.134130704350374\n"
      "   2.541681638740779   0.000000000000000   1.325957831362909   0.761559628559505\n"
      "   0.000000000000000   2.541681638740779   0.761559628559505  -1.325957831362909\n"
      "   1.325957831362909   0.761559628559505  -2.541681638740779   0.000000000000000\n"
      "   0.761559628559505  -1.325957831362909   0.000000000000000  -2.541681638740779\n"
      "   0.761559628559505  -1.325957831362909   0.000000000000000  -2.541681638740779\n"
      "  -1.325957831362909  -0.761559628559505   2.541681638740779   0.000000000000000\n"
      "   0.000000000000000   2.541681638740779   0.761559628559505  -1.325957831362909\n"
      "  -2.541681638740779   0.000000000000000  -1.325957831362909  -0.761559628559505\n"
      "  -1.325957831362909  -0.761559628559505   2.541681638740779   0.000000000000000\n"
      "  -0.761559628559505   1.325957831362909   0.000000000000000   2.541681638740779\n"
      "   2.541681638740779   0.000000000000000   1.325957831362909   0.761559628559505\n"
      "   0.000000000000000   2.541681638740779   0.761559628559505  -1.325957831362909\n";
  }
}

auto constexpr * provideExpectedValuesForQuaternionDerivatives() {
  return "-0.00317901715652067   0.0086232820215688 -0.00757183019995756  0.00525255623360916\n"
    " 0.00439880060444517  0.00331016117542616  0.00945966919445312  0.00719349875069121\n"
    " 0.00021825620288395  0.00417896782325132 0.000201367848122566  0.00416600719354294\n"
    "   0.0388082347226339  -0.0114816405830178  0.00854107259768903  -0.0347065272377706\n"
    " -0.0293462244452848  -0.0197305548884027  -0.0225092548277784  -0.0144843090151835\n"
    " 0.00981951104148851  0.00359621031649957  0.00583379283679878 0.000537842580671388\n"
    "  -0.0529884808663643  -0.0141427702569471   0.0150116576278747   0.0380357065806483\n"
    " -0.0198724880283105  -0.0106661866676083   0.0312573890184954   0.0285672207720257\n"
    " -0.0441353410310778 -0.00372287239445657  -0.0260032832703698   0.0101904244078065\n"
    "  -0.0199116998893643   0.0160013778522211  -0.0134259326178147   0.0209781106918321\n"
    "  0.0695507069534357   0.0431876244080595 -0.00832166399381712  -0.0165661244273064\n"
    "  0.0228969395740884  -0.0143916204665753   0.0132046971519158  -0.0218287756322355\n"
    "  0.0315142512563002    0.041627951759552  -0.0386401678708678  -0.0122035426308374\n"
    " -0.0103020681757879 -0.00461072694959947   0.0320568355868915   0.0278924531266778\n"
    "  0.0149090782030018   0.0324825635567004  0.00933019829290106   0.0282017014700561\n"
    "  0.0057567119333152   -0.040628200793377   0.0360852004630764  -0.0173563036374815\n"
    " -0.0144287269084976  -0.0114903170778752  -0.0419429749782445  -0.0326027392069049\n"
    "-0.00370844399038483  -0.0221432488354194 -0.00256677285936839  -0.0212672000198415\n";
}

#endif
#endif