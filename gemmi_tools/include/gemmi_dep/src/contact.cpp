// Copyright 2018 Global Phasing Ltd.
//
// Searches for contacts -- neighbouring atoms.

#include <cstdio>
#include <cstdlib>    // for strtod
#include <algorithm>  // for min, max
#include <gemmi/contact.hpp>
#include <gemmi/subcells.hpp>
#include <gemmi/polyheur.hpp>  // for are_connected
#include <gemmi/elem.hpp>      // for is_hydrogen
#include <gemmi/gzread.hpp>
#include <gemmi/to_pdb.hpp>    // for padded_atom_name
#define GEMMI_PROG contact
#include "options.h"

using namespace gemmi;
using std::printf;

enum OptionIndex { Cov=4, CovMult, MaxDist, Occ, Any, NoH, NoSym, Count,
                   Twice };

static const option::Descriptor Usage[] = {
  { NoOp, 0, "", "", Arg::None,
    "Usage:\n " EXE_NAME " [options] INPUT[...]"
    "\nSearches for contacts in a model (PDB or mmCIF)."},
  CommonUsage[Help],
  CommonUsage[Version],
  CommonUsage[Verbose],
  { MaxDist, 0, "d", "maxdist", Arg::Float,
    "  -d, --maxdist=D  Maximal distance in A (default 3.0)" },
  { Cov, 0, "", "cov", Arg::Float,
    "  --cov=TOL  \tUse max distance = covalent radii sum + TOL [A]." },
  { CovMult, 0, "", "covmult", Arg::Float,
    "  --covmult=M  \tUse max distance = M * covalent radii sum + TOL [A]." },
  { Occ, 0, "", "minocc", Arg::Float,
    "  --minocc=MIN  \tIgnore atoms with occupancy < MIN." },
  { Any, 0, "", "any", Arg::None,
    "  --any  \tOutput any atom pair, even from the same residue." },
  { NoH, 0, "", "noh", Arg::None,
    "  --noh  \tIgnore hydrogen (and deuterium) atoms." },
  { NoSym, 0, "", "nosym", Arg::None,
    "  --nosym  \tIgnore contacts with symmetry mates." },
  { Count, 0, "", "count", Arg::None,
    "  --count  \tPrint only a count of atom pairs." },
  { Twice, 0, "", "twice", Arg::None,
    "  --twice  \tPrint each atom pair A-B twice (A-B and B-A)." },
  { 0, 0, 0, 0, 0, 0 }
};

struct Parameters {
  bool use_cov_radius;
  bool any;
  bool print_count;
  bool no_hydrogens;
  bool no_symmetry;
  bool twice;
  float cov_tol = 0.0f;
  float cov_mult = 1.0f;
  float max_dist = 3.0f;
  float min_occ = 0.0f;
  int verbose;
};

static void print_contacts(Structure& st, const Parameters& params) {
  float max_r = params.use_cov_radius ? 4.f + params.cov_tol : params.max_dist;
  SubCells sc(st.first_model(), st.cell, std::max(5.0f, max_r));
  sc.populate(/*include_h=*/!params.no_hydrogens);

  if (params.verbose > 0) {
    if (params.verbose > 1) {
      if (st.cell.explicit_matrices)
        printf(" Using fractionalization matrix from the file.\n");
      printf(" Each atom has %zu extra images.\n", st.cell.images.size());
    }
    printf(" Cell grid: %d x %d x %d\n", sc.grid.nu, sc.grid.nv, sc.grid.nw);
    size_t min_count = SIZE_MAX, max_count = 0, total_count = 0;
    for (const auto& el : sc.grid.data) {
      min_count = std::min(min_count, el.size());
      max_count = std::max(max_count, el.size());
      total_count += el.size();
    }
    printf(" Items per cell: from %zu to %zu, average: %.2g\n",
           min_count, max_count, double(total_count) / sc.grid.data.size());
  }

  // the code here is similar to LinkHunt::find_possible_links()
  int counter = 0;
  ContactSearch contacts(max_r);
  contacts.twice = params.twice;
  contacts.skip_intra_residue = !params.any;
  contacts.skip_adjacent_residue = !params.any;
  if (params.use_cov_radius)
    contacts.setup_atomic_radii(params.cov_mult, params.cov_tol);
  contacts.for_each_contact(sc, [&](const CRA& cra1, const CRA& cra2,
                                    int image_idx, float dist_sq) {
      ++counter;
      if (params.print_count)
        return;
      std::string sym1, sym2;
      if (!params.no_symmetry) {
        SymImage im = st.cell.find_nearest_pbc_image(cra1.atom->pos,
                                                     cra2.atom->pos, image_idx);
        sym1 = "1555";
        sym2 = im.pdb_symbol(false);
      }
      printf("            %-4s%c%3s%2s%5s   "
             "            %-4s%c%3s%2s%5s  %6s %6s %5.2f\n",
             padded_atom_name(*cra1.atom).c_str(),
             cra1.atom->altloc ? std::toupper(cra1.atom->altloc) : ' ',
             cra1.residue->name.c_str(),
             cra1.chain->name.c_str(),
             cra1.residue->seqid.str().c_str(),
             padded_atom_name(*cra2.atom).c_str(),
             cra2.atom->altloc ? std::toupper(cra2.atom->altloc) : ' ',
             cra2.residue->name.c_str(),
             cra2.chain->name.c_str(),
             cra2.residue->seqid.str().c_str(),
             sym1.c_str(), sym2.c_str(), std::sqrt(dist_sq));
  });
  if (params.print_count)
    printf("%s:%g\n", st.name.c_str(), 0.5 * counter);
}

int GEMMI_MAIN(int argc, char **argv) {
  OptParser p(EXE_NAME);
  p.simple_parse(argc, argv, Usage);
  p.require_input_files_as_args();
  Parameters params;
  params.verbose = p.options[Verbose].count();
  params.use_cov_radius = (p.options[Cov] || p.options[CovMult]);
  if (p.options[Cov])
    params.cov_tol = std::strtof(p.options[Cov].arg, nullptr);
  if (p.options[CovMult])
    params.cov_mult = std::strtof(p.options[CovMult].arg, nullptr);
  if (p.options[MaxDist])
    params.max_dist = std::strtof(p.options[MaxDist].arg, nullptr);
  if (p.options[Occ])
    params.min_occ = std::strtof(p.options[Occ].arg, nullptr);
  params.any = p.options[Any];
  params.print_count = p.options[Count];
  params.no_hydrogens = p.options[NoH];
  params.no_symmetry = p.options[NoSym];
  params.twice = p.options[Twice];
  try {
    for (int i = 0; i < p.nonOptionsCount(); ++i) {
      std::string input = p.coordinate_input_file(i);
      if (params.verbose > 0 ||
          (p.nonOptionsCount() > 1 && !params.print_count))
        std::printf("%sFile: %s\n", (i > 0 ? "\n" : ""), input.c_str());
      Structure st = read_structure_gz(input);
      if (params.no_symmetry)
        st.cell = UnitCell();
      print_contacts(st, params);
    }
  } catch (std::runtime_error& e) {
    std::fprintf(stderr, "ERROR: %s\n", e.what());
    return 1;
  }
  return 0;
}

// vim:sw=2:ts=2:et:path^=../include,../third_party
