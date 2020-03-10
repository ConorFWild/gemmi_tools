
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdlib>  // for rand
#include <climits>  // for INT_MIN, INT_MAX
#include <gemmi/atox.hpp>
#include <gemmi/math.hpp>
#include <linalg.h>

static double draw() { return 10.0 * std::rand() / RAND_MAX - 5; }

static gemmi::Transform random_transform() {
  gemmi::Transform a;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      a.mat[i][j] = draw();
    a.vec.at(i) = draw();
  }
  return a;
}

TEST_CASE("Transform::inverse") {
  std::srand(12345);
  gemmi::Transform tr = random_transform();
  linalg::mat<double,4,4> m44 = linalg::identity;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      m44[i][j] = tr.mat[i][j];
    m44[i][3] = tr.vec.at(i);
  }
  linalg::mat<double,4,4> inv_m44 = linalg::inverse(m44);
  gemmi::Transform inv_tr = tr.inverse();
  CHECK_EQ(inv_m44[3][3], doctest::Approx(1.0));
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      CHECK_EQ(inv_tr.mat[i][j], doctest::Approx(inv_m44[i][j]));
    CHECK_EQ(inv_tr.vec.at(i), doctest::Approx(inv_m44[i][3]));
    CHECK_EQ(inv_m44[3][i], 0);
  }
}

TEST_CASE("Transform::combine") {
  std::srand(12345);
  gemmi::Transform a = random_transform();
  gemmi::Transform b = random_transform();
  gemmi::Vec3 v;
  for (int i = 0; i < 3; ++i)
    v.at(i) = draw();
  gemmi::Vec3 result1 = a.combine(b).apply(v);
  gemmi::Vec3 result2 = a.apply(b.apply(v));
  for (int i = 0; i < 3; ++i)
    CHECK_EQ(result1.at(i), doctest::Approx(result2.at(i)));
}

TEST_CASE("Mat33::smallest_eigenvalue") {
  auto ev = gemmi::Mat33(3, 2, 4, 2, 0, 2, 4, 2, 3).calculate_eigenvalues();
  CHECK_EQ(ev[0], doctest::Approx(8));
  CHECK_EQ(ev[1], doctest::Approx(-1));
  CHECK_EQ(ev[2], doctest::Approx(-1));
  gemmi::Mat33 m2(3, 1, -1, 1, 3, -1, -1, -1, 5);
  auto ev2 = m2.calculate_eigenvalues();
  CHECK_EQ(ev2[0], doctest::Approx(6));
  CHECK_EQ(ev2[1], doctest::Approx(3));
  CHECK_EQ(ev2[2], doctest::Approx(2));
  gemmi::Vec3 evec0 = m2.calculate_eigenvector(ev2[0]);
  CHECK_EQ(evec0.x, doctest::Approx(-std::sqrt(1./6)));
  CHECK_EQ(evec0.y, doctest::Approx(-std::sqrt(1./6)));
  CHECK_EQ(evec0.z, doctest::Approx(std::sqrt(4./6)));
  gemmi::Vec3 evec2 = m2.calculate_eigenvector(ev2[2]);
  CHECK_EQ(evec2.length_sq(), doctest::Approx(1.0));
  CHECK_EQ(evec2.y, doctest::Approx(-evec2.x));
  CHECK_EQ(evec2.z, doctest::Approx(0));
}

TEST_CASE("Variance") {
  gemmi::Variance v;
  for (double x : {0.14, 0.08, 0.16, 0.12, 0.04})
    v.add_point(x);
  CHECK_EQ(v.for_sample(), 0.00232);
  CHECK_EQ(v.n, 5);
  CHECK_EQ(v.mean_x, 0.108);
}

TEST_CASE("Covariance") {
  gemmi::Covariance cov;
  cov.add_point(2.1, 8);
  cov.add_point(2.5, 12);
  cov.add_point(4.0, 14);
  cov.add_point(3.6, 10);
  CHECK_EQ(cov.n, 4);
  CHECK_EQ(cov.mean_x, 3.05);
  CHECK_EQ(cov.mean_y, 11);
  CHECK_EQ(cov.for_population(), doctest::Approx(1.15));
  CHECK_EQ(cov.for_sample(), doctest::Approx(1.53333));
}

TEST_CASE("Correlation") {
  gemmi::Correlation cor;
  cor.add_point(2.1, 8);
  cor.add_point(2.5, 12);
  CHECK_EQ(cor.n, 2);
  CHECK_EQ(cor.coefficient(), 1.0);
  cor.add_point(4.0, 14);
  cor.add_point(3.6, 10);
  CHECK_EQ(cor.n, 4);
  CHECK_EQ(cor.mean_x, 3.05);
  CHECK_EQ(cor.mean_y, 11);
  CHECK_EQ(cor.coefficient(), doctest::Approx(0.66257388));
  CHECK_EQ(cor.covariance(), doctest::Approx(1.15));
  CHECK_EQ(cor.x_variance(), doctest::Approx(0.6025));
  CHECK_EQ(cor.y_variance(), doctest::Approx(5));
  // scipy.stats.linregress([2.1, 2.5, 4.0, 3.6], [8, 12, 14, 10])
  CHECK_EQ(cor.slope(), doctest::Approx(1.9087136929460577));
  CHECK_EQ(cor.intercept(), doctest::Approx(5.178423236514524));
}

TEST_CASE("string_to_int") {
  CHECK_EQ(gemmi::string_to_int(std::to_string(INT_MAX), true), INT_MAX);
  CHECK_EQ(gemmi::string_to_int(std::to_string(INT_MIN), true), INT_MIN);
  CHECK_EQ(gemmi::string_to_int("", false), 0);
}
