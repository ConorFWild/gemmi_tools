// Copyright 2019 Global Phasing Ltd.
//
// Calculate structure factors from a molecular model.

#include <stdio.h>
#include <chrono>
#include <complex>
#include <gemmi/fourier.hpp>
#include <gemmi/gzread.hpp>
#include <gemmi/it92.hpp>
#include <gemmi/fileutil.hpp>  // for file_open
#include <gemmi/math.hpp>      // for sq
#include <gemmi/rhogrid.hpp>   // for put_model_density_on_grid
#include <gemmi/sfcalc.hpp>    // for calculate_structure_factor
#include <gemmi/smcif.hpp>     // for make_small_structure_from_block
#include <gemmi/mtz.hpp>       // for read_mtz_file
#include <gemmi/gz.hpp>        // for MaybeGzipped

#define GEMMI_PROG sfcalc
#include "options.h"

enum OptionIndex { Hkl=4, Dmin, Rate, Blur, RCut, Test, Check,
                   CifFp, Wavelength, Unknown, FLabel, PhiLabel, Scale };

static const option::Descriptor Usage[] = {
  { NoOp, 0, "", "", Arg::None,
    "Usage:\n  " EXE_NAME " [options] INPUT_FILE\n\n"
    "Calculates structure factors of a model (PDB, mmCIF or SMX CIF file).\n\n"
    "Uses FFT to calculate all reflections up to requested resolution for MX\n"
    "files. Otherwise, for SMX and --hkl, F's are calculated directly.\n"
    "This program can also compare F's calculated directly with values\n"
    "calculated through FFT or with values read from a reflection file.\n"
    "\nOptions:"},
  CommonUsage[Help],
  CommonUsage[Version],
  CommonUsage[Verbose],
  { Hkl, 0, "", "hkl", Arg::Int3,
    "  --hkl=H,K,L  \tCalculate structure factor F_hkl." },
  { Dmin, 0, "", "dmin", Arg::Float,
    "  --dmin=NUM  \tCalculate structure factors up to given resolution." },
  { CifFp, 0, "", "ciffp", Arg::None,
    "  --ciffp  \tRead f' from _atom_type_scat_dispersion_real in CIF." },
  { Wavelength, 0, "w", "wavelength", Arg::Float,
    "  --wavelength=NUM  \tWavelength [A] for calculation of f' "
    "(use --wavelength=0 or -w0 to ignore anomalous scattering)." },
  { Unknown, 0, "", "unknown", Arg::Required,
    "  --unknown=SYMBOL  \tUse form factor of SYMBOL for unknown atoms." },
  { NoOp, 0, "", "", Arg::None, "\nOptions for FFT-based calculations:" },
  { Rate, 0, "", "rate", Arg::Float,
    "  --rate=NUM  \tShannon rate used for grid spacing (default: 1.5)." },
  { Blur, 0, "", "blur", Arg::Float,
    "  --blur=NUM  \tB added for Gaussian blurring (default: auto)." },
  { RCut, 0, "", "rcut", Arg::Float,
    "  --rcut=Y  \tUse atomic radius r such that rho(r) < Y (default: 5e-5)." },
  { Test, 0, "", "test", Arg::Optional,
    "  --test[=CACHE]  \tCalculate exact values and report differences (slow)." },
  { NoOp, 0, "", "", Arg::None,
    "\nOptions for comparing calculated values with values from a file:" },
  { Check, 0, "", "check", Arg::Required,
    "  --check=FILE  \tRe-calculate Fcalc and report differences." },
  { FLabel, 0, "", "f", Arg::Required,
    "  --f=LABEL  \tMTZ column label (default: FC) or small molecule cif"
    " tag (default: F_calc or F_squared_calc)." },
  { PhiLabel, 0, "", "phi", Arg::Required,
    "  --phi=LABEL  \tMTZ column label (default: PHIC)" },
  { Scale, 0, "", "scale", Arg::Float,
    "  --scale=S  \tMultiply calculated F by sqrt(S) (default: 1)." },
  { 0, 0, 0, 0, 0, 0 }
};

static
void print_sf(std::complex<double> sf, const gemmi::Miller& hkl) {
  printf(" (%d %d %d)\t%8.12f\t%6.12f\n",
         hkl[0], hkl[1], hkl[2], std::abs(sf), gemmi::phase_in_angles(sf));
}

struct Comparator {
  double sum_sq_diff = 0.;
  double sum_sq1 = 0.;
  double sum_sq2 = 0.;
  double sum_abs = 0.;
  double max_abs_df = 0.;
  double sum_abs_diff = 0.;
  int count = 0;

  template<typename T> void add(T value, T exact) {
    double abs_df = std::abs(value - exact);
    sum_sq_diff += abs_df * abs_df;
    sum_sq1 += gemmi::sq(std::abs(value));
    sum_sq2 += gemmi::sq(std::abs(exact));
    sum_abs += std::abs(exact);
    sum_abs_diff += std::abs(std::abs(value) - std::abs(exact));
    if (abs_df > max_abs_df)
      max_abs_df = abs_df;
    ++count;
  }

  double rmse() const { return std::sqrt(sum_sq_diff / count); }
  double abs_avg() const { return sum_abs / count; }
  double weighted_rmse() const { return rmse() / abs_avg(); }
  double rfactor() const { return sum_abs_diff / sum_abs; }
  double scale() const { return std::sqrt(sum_sq1 / sum_sq2); }
};

static
void print_to_stderr(const Comparator& c) {
  fflush(stdout);
  fprintf(stderr, "RMSE=%#.5g  %#.4g%%  max|dF|=%#.4g  R=%.3f%%",
          c.rmse(), 100 * c.weighted_rmse(), c.max_abs_df, 100 * c.rfactor());
}

using namespace gemmi;

template<typename Table>
void print_structure_factors(const Structure& st,
                             DensityCalculator<Table,float>& dencalc,
                             bool verbose, char mode, const char* file_path,
                             const std::string& f_label,
                             const std::string& phi_label) {
  using Clock = std::chrono::steady_clock;
  if (verbose) {
    fprintf(stderr, "Preparing electron density on a grid...\n");
    fflush(stderr);
  }
  auto start = Clock::now();
  dencalc.set_grid_cell_and_spacegroup(st);
  dencalc.put_model_density_on_grid(st.models[0]);
  const Grid<float>& grid = dencalc.grid;
  if (verbose) {
    std::chrono::duration<double> elapsed = Clock::now() - start;
    fprintf(stderr, "...took %g s.\n", elapsed.count());
    fprintf(stderr, "FFT of grid %d x %d x %d\n", grid.nu, grid.nv, grid.nw);
    fflush(stderr);
    start = Clock::now();
  }
  FPhiGrid<float> sf = transform_map_to_f_phi(grid, /*half_l=*/true);
  StructureFactorCalculator<Table> calc(st.cell);
  if (verbose) {
    std::chrono::duration<double> elapsed = Clock::now() - start;
    fprintf(stderr, "...took %g s.\n", elapsed.count());
    fprintf(stderr, "Printing results...\n");
    fflush(stderr);
  }
  gemmi::fileptr_t cache(nullptr, nullptr);
  std::map<Miller, std::complex<double>> mtz_data;
  if (mode == 'T' && file_path) {
    cache = gemmi::file_open(file_path, "r");
  } else if (mode == 'C' && file_path) {
    Mtz mtz;
    mtz.read_input(gemmi::MaybeGzipped(file_path), true);
    Mtz::Column* f_col = mtz.column_with_label(f_label);
    if (!f_col)
      fail("MTZ file has no column with label: " + f_label);
    Mtz::Column* phi_col = mtz.column_with_label(phi_label);
    if (!phi_col)
      fail("MTZ file has no column with label: " + phi_label);
    gemmi::MtzDataProxy data_proxy{mtz};
    auto hkl_col = data_proxy.hkl_col();
    for (size_t i = 0; i < data_proxy.size(); i += data_proxy.stride()) {
      Miller hkl = data_proxy.get_hkl(i, hkl_col);
      double f_abs = data_proxy.get_num(i + f_col->idx);
      double f_deg = data_proxy.get_num(i + phi_col->idx);
      if (!std::isnan(f_abs) && !std::isnan(f_deg))
        mtz_data.emplace(hkl, std::polar(f_abs, gemmi::rad(f_deg)));
    }
  }

  Comparator comparator;
  double max_1_d = 1. / dencalc.d_min;
  gemmi::HklAsuChecker hkl_asu(dencalc.grid.spacegroup);
  int max_h = std::min(sf.nu / 2, int(max_1_d / st.cell.ar));
  int max_k = std::min(sf.nv / 2, int(max_1_d / st.cell.br));
  int max_l = std::min(sf.nw, int(max_1_d / st.cell.cr));
  for (int h = -max_h; h <= max_h; ++h)
    for (int k = -max_k; k <= max_k; ++k)
      for (int l = 0; l <= max_l; ++l) {
        if (!hkl_asu.is_in(h, k, l))
          continue;
        Miller hkl{{h, k, l}};
        double hkl_1_d2 = sf.unit_cell.calculate_1_d2(hkl);
        if (hkl_1_d2 < max_1_d * max_1_d) {
          int idx_h = h < 0 ? h + sf.nu : h;
          int idx_k = k < 0 ? k + sf.nv : k;
          std::complex<double> value = sf.get_value_q(idx_h, idx_k, l);
          value *= dencalc.reciprocal_space_multiplier(hkl_1_d2);
          if (mode != ' ') {
            std::complex<double> exact;
            if (file_path) {
              if (mode == 'T') {
                char cache_line[100];
                if (fgets(cache_line, 99, cache.get()) == nullptr)
                  gemmi::fail("cannot read line from file");
                int cache_h, cache_k, cache_l;
                double f_abs, f_deg;
                sscanf(cache_line, " (%d %d %d) %*f %lf %*f %lf",
                       &cache_h, &cache_k, &cache_l, &f_abs, &f_deg);
                if (cache_h != h || cache_k != k || cache_l != l)
                  gemmi::fail("Different h k l order than in cache file.");
                exact = std::polar(f_abs, gemmi::rad(f_deg));
              } else if (mode == 'C') {
                auto it = mtz_data.find(hkl);
                if (it == mtz_data.end())
                  continue;
                exact = it->second;
              }
            } else {
              exact = calc.calculate_sf_from_model(st.models[0], hkl);
            }
            comparator.add(value, exact);
            printf(" (%d %d %d)\t%7.2f\t%8.3f \t%6.2f\t%7.3f\td=%5.2f\n",
                   h, k, l, std::abs(value), std::abs(exact),
                   gemmi::phase_in_angles(value), gemmi::phase_in_angles(exact),
                   1. / std::sqrt(hkl_1_d2));
          } else {
            print_sf(value, hkl);
          }
        }
      }
  if (mode != ' ') {
    print_to_stderr(comparator);
    if (!verbose) {
      std::chrono::duration<double> elapsed = Clock::now() - start;
      fprintf(stderr, "   %#.5gs", elapsed.count());
    }
    fprintf(stderr, "\n");
  }
}

template<typename Table>
void print_structure_factors_sm(const SmallStructure& small,
                                StructureFactorCalculator<Table>& calc,
                                double d_min, bool verbose) {
  using Clock = std::chrono::steady_clock;
  auto start = Clock::now();
  int counter = 0;
  double max_1_d = 1. / d_min;
  int max_h = int(max_1_d / small.cell.ar);
  int max_k = int(max_1_d / small.cell.br);
  int max_l = int(max_1_d / small.cell.cr);
  const SpaceGroup* sg = find_spacegroup_by_name(small.spacegroup_hm,
                                           small.cell.alpha, small.cell.gamma);
  gemmi::HklAsuChecker hkl_asu(sg ? sg : &get_spacegroup_p1());
  for (int h = -max_h; h <= max_h; ++h)
    for (int k = -max_k; k <= max_k; ++k)
      for (int l = 0; l <= max_l; ++l) {
        if (!hkl_asu.is_in(h, k, l))
          continue;
        Miller hkl{{h, k, l}};
        double hkl_1_d2 = small.cell.calculate_1_d2(hkl);
        if (hkl_1_d2 < max_1_d * max_1_d) {
          auto value = calc.calculate_sf_from_small_structure(small, hkl);
          print_sf(value, hkl);
          ++counter;
        }
      }
  if (verbose) {
    std::chrono::duration<double> elapsed = Clock::now() - start;
    fflush(stdout);
    fprintf(stderr, "Calculated %d SFs in %g s.\n", counter, elapsed.count());
    fflush(stderr);
  }
}

static
double get_minimum_b_iso(const Model& model) {
  double b_min = 1000.;
  for (const Chain& chain : model.chains)
    for (const Residue& residue : chain.residues)
      for (const Atom& atom : residue.atoms)
        if (atom.b_iso < b_min)
          b_min = atom.b_iso;
  return b_min;
}

template<typename Table>
void compare_with_hkl(const SmallStructure& small,
                      StructureFactorCalculator<Table>& calc,
                      const std::string& label, double scale,
                      bool verbose, const char* path,
                      Comparator& comparator) {
  cif::Document hkl_doc = read_cif_gz(path);
  cif::Block& block = hkl_doc.blocks.at(0);
  std::vector<std::string> tags =
    {"index_h", "index_k", "index_l", "?F_calc", "?F_squared_calc"};
  if (!label.empty()) {
    tags.pop_back();
    tags.back().replace(1, std::string::npos, label);
  }
  cif::Table table = block.find("_refln_", tags);
  if (!table.ok())
    fail("_refln_index_ category not found in ", path);
  if (table.has_column(3)) {
    if (verbose)
      fprintf(stderr, "Checking _refln_%s from %s\n", tags[3].c_str()+1, path);
  } else if (tags.size() > 4 && table.has_column(4)) {
    if (verbose)
      fprintf(stderr, "Checking sqrt of _refln_%s from %s\n",
              tags[4].c_str()+1, path);
  } else {
    std::string msg;
    if (label.empty())
      msg = "Neither _refln_F_calc nor _refln_F_squared_calc";
    else
      msg = "_refln_" + label;
    fail(msg + " not found in: ", path);
  }
  Miller hkl;
  for (auto row : table) {
    double f_from_file = NAN;
    try {
      for (int i = 0; i != 3; ++i)
        hkl[i] = cif::as_int(row[i]);
      if (row.has(3))
        f_from_file = cif::as_number(row[3]);
      else if (row.has(4))
        f_from_file = std::sqrt(cif::as_number(row[4]));
    } catch(std::runtime_error& e) {
      fprintf(stderr, "Error in _refln_[] in %s: %s\n", path, e.what());
      continue;
    } catch(std::invalid_argument& e) {
      fprintf(stderr, "Error in _refln_[] in %s: %s\n", path, e.what());
      continue;
    }
    double f = std::abs(calc.calculate_sf_from_small_structure(small, hkl));
    f *= scale;
    comparator.add(f_from_file, f);
    if (verbose)
      printf(" (%d %d %d)\t%7.2f\t%8.3f \td=%5.2f\n",
             hkl[0], hkl[1], hkl[2], f_from_file, f,
             small.cell.calculate_d(hkl));
  }
}

template<typename Table>
void compare_with_mtz(const Model& model, const UnitCell& cell,
                      StructureFactorCalculator<Table>& calc,
                      const std::string& label, double scale, bool verbose,
                      const char* path, Comparator& comparator) {
  Mtz mtz;
  mtz.read_input(gemmi::MaybeGzipped(path), true);
  Mtz::Column* col = mtz.column_with_label(label);
  if (!col)
    fail("MTZ file has no column with label: " + label);
  gemmi::MtzDataProxy data_proxy{mtz};
  auto hkl_col = data_proxy.hkl_col();
  for (size_t i = 0; i < data_proxy.size(); i += data_proxy.stride()) {
    Miller hkl = data_proxy.get_hkl(i, hkl_col);
    double f_from_file = data_proxy.get_num(i + col->idx);
    double f = std::abs(calc.calculate_sf_from_model(model, hkl));
    f *= scale;
    comparator.add(f_from_file, f);
    if (verbose)
      printf(" (%d %d %d)\t%7.2f\t%8.3f \td=%5.2f\n",
             hkl[0], hkl[1], hkl[2], f_from_file, f, cell.calculate_d(hkl));
  }
}

void process(const std::string& input, const OptParser& p) {
  // read (Small)Structure
  gemmi::Structure st = gemmi::read_structure_gz(input);
  gemmi::SmallStructure small;
  bool use_st = !st.models.empty();
  if (!use_st) {
    if (giends_with(input, ".cif"))
      small = gemmi::make_small_structure_from_block(
                                gemmi::read_cif_gz(input).sole_block());
    if (small.sites.empty() ||
        // COD can have a row of nulls as a placeholder (e.g. 2211708)
        (small.sites.size() == 1 && small.sites[0].element == El::X))
      gemmi::fail("no atoms in the file");
    // SM CIF files specify full occupancy for atoms on special positions.
    // We need to adjust it for symmetry calculations.
    for (SmallStructure::Site& site : small.sites) {
      int n_mates = small.cell.is_special_position(site.fract, 0.4);
      if (n_mates != 0)
        site.occ /= (n_mates + 1);
    }
  }

  const UnitCell& cell = use_st ? st.cell : small.cell;
  using Table = IT92<double>;
  StructureFactorCalculator<Table> calc(cell);

  // assign f'
  if (p.options[CifFp]) {
    if (use_st) {
      // _atom_type.scat_dispersion_real is almost never used,
      // so for now we ignore it.
    } else { // small molecule
      if (p.options[Verbose])
        fprintf(stderr, "Using f' read from cif file (%u atom types)\n",
                (unsigned) small.atom_types.size());
      for (const SmallStructure::AtomType& atom_type : small.atom_types)
        calc.set_fprime(atom_type.element, atom_type.dispersion_real);
    }
  }
  double wavelength = 0;
  if (p.options[Wavelength]) {
    wavelength = std::atof(p.options[Wavelength].arg);
  } else {
    if (use_st) {
      // reading wavelength from PDB and mmCIF files needs to be revisited
      //if (!st.crystals.empty() && !st.crystals[0].diffractions.empty())
      //  wavelength_list = st.crystals[0].diffractions[0].wavelengths;
    } else {
      wavelength = small.wavelength;
    }
  }
  if (p.options[Unknown]) {
    El new_el = find_element(p.options[Unknown].arg);
    if (new_el == El::X)
      fail("--unknown must specify chemical element symbol.");
    if (use_st) {
      for (Chain& chain : st.models[0].chains)
        for (Residue& residue : chain.residues)
          for (Atom& atom : residue.atoms)
            if (atom.element == El::X)
              atom.element = new_el;
    } else {
      for (SmallStructure::Site& atom : small.sites)
        if (atom.element == El::X)
          atom.element = new_el;
    }
  }
  auto present_elems = use_st ? st.models[0].present_elements()
                              : small.present_elements();
  if (present_elems[(int)El::X])
    fail("unknown element. Add --unknown=O to treat unknown atoms as oxygen.");
  for (size_t i = 1; i != present_elems.size(); ++i)
    if (present_elems[i] && !Table::has((El)i))
      fail("Missing form factor for element ", element_name((El)i));
  if (wavelength > 0) {
    double energy = hc() / wavelength;
    for (int z = 1; z <= 92; ++z)
      if (present_elems[z]) {
        double fprime = cromer_libermann(z, energy, nullptr);
        calc.set_fprime_if_not_set((El)z, fprime);
      }
  }

  // handle option --hkl
  for (const option::Option* opt = p.options[Hkl]; opt; opt = opt->next()) {
    std::vector<int> hkl_ = parse_comma_separated_ints(opt->arg);
    gemmi::Miller hkl{{hkl_[0], hkl_[1], hkl_[2]}};
    if (p.options[Verbose])
      fprintf(stderr, "hkl=(%d %d %d) -> d=%g\n", hkl[0], hkl[1], hkl[2],
              cell.calculate_d(hkl));
    if (use_st)
      print_sf(calc.calculate_sf_from_model(st.models[0], hkl), hkl);
    else
      print_sf(calc.calculate_sf_from_small_structure(small, hkl), hkl);
  }

  std::string f_label, phi_label;
  if (p.options[FLabel])
   f_label = p.options[FLabel].arg;
  else if (use_st)
    f_label = "FC";
  if (p.options[PhiLabel])
   phi_label = p.options[PhiLabel].arg;
  else if (use_st)
    phi_label = "PHIC";

  // handle option --dmin
  if (p.options[Dmin]) {
    double d_min = std::strtod(p.options[Dmin].arg, nullptr);
    if (use_st) {
      DensityCalculator<Table, float> dencalc;
      dencalc.d_min = d_min;
      if (p.options[Rate])
        dencalc.rate = std::strtod(p.options[Rate].arg, nullptr);
      if (p.options[RCut])
        dencalc.r_cut = (float) std::strtod(p.options[RCut].arg, nullptr);
      for (auto& it : calc.fprimes())
        dencalc.fprimes[(int)it.first] = (float) it.second;
      if (p.options[Blur]) {
        dencalc.blur = std::strtod(p.options[Blur].arg, nullptr);
      } else if (dencalc.rate < 3) {
        // ITfC vol B section 1.3.4.4.5 has formula
        // B = log Q / (sigma * (sigma - 1) * d*_max ^2)
        // where Q is quality factor, sigma is the oversampling rate.
        // This value is not optimal.
        // The optimal value would depend on the distribution of B-factors
        // and on the atomic cutoff radius, and probably it would be too
        // hard to estimate.
        // Here we use a simple ad-hoc rule:
        double sqrtB = 4 * dencalc.d_min * (1./dencalc.rate - 0.2);
        double b_min = get_minimum_b_iso(st.models[0]);
        dencalc.blur = sqrtB * sqrtB - b_min;
        if (p.options[Verbose])
          fprintf(stderr, "B_min=%g, B_add=%g\n", b_min, dencalc.blur);
      }

      char mode = ' ';
      const char *file = nullptr;
      if (p.options[Test]) {
        mode = 'T';
        file = p.options[Test].arg;
      } else if (p.options[Check]) {
        mode = 'C';
        file = p.options[Check].arg;
      }
      print_structure_factors(st, dencalc, p.options[Verbose], mode, file,
                              f_label, phi_label);
    } else {
      if (p.options[Rate] || p.options[RCut] || p.options[Blur] ||
          p.options[Test])
        fail("Small molecule SFs are calculated directly. Do not use any\n"
             "of the FFT-related options: --rate, --blur, --rcut, --test.");
      print_structure_factors_sm(small, calc, d_min, p.options[Verbose]);
    }

  // handle option --check
  } else if (p.options[Check]) {
    double scale = 1.0;
    if (p.options[Scale])
      scale = std::strtod(p.options[Scale].arg, nullptr);
    const char* path = p.options[Check].arg;
    Comparator comparator;
    if (use_st)
      compare_with_mtz(st.models[0], st.cell, calc, f_label, scale,
                       p.options[Verbose], path, comparator);
    else
      compare_with_hkl(small, calc, f_label, scale,
                       p.options[Verbose], path, comparator);
    print_to_stderr(comparator);
    fprintf(stderr, "  sum(F^2)_ratio=%g\n", comparator.scale());
  }
}

int GEMMI_MAIN(int argc, char **argv) {
  OptParser p(EXE_NAME);
  p.simple_parse(argc, argv, Usage);
  p.require_input_files_as_args();
  try {
    for (int i = 0; i < p.nonOptionsCount(); ++i) {
      std::string input = p.coordinate_input_file(i);
      if (p.options[Verbose]) {
        fprintf(stderr, "Reading file %s ...\n", input.c_str());
        fflush(stderr);
      }
      process(input, p);
    }
  } catch (std::runtime_error& e) {
    std::fprintf(stderr, "ERROR: %s\n", e.what());
    return 1;
  }
  return 0;
}

// vim:sw=2:ts=2:et:path^=../include,../third_party
