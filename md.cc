#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <atomic>

#include "scon_vect.h"
#include "configuration.h"
#include "coords.h"
#include "coords_io.h"
#include "md.h"
#include "scon_chrono.h"

// Logging Functions

std::ostream& md::operator<<(std::ostream &strm, trace_data const &d)
{
  strm << std::right << std::setw(10) << d.i;
  strm << std::right << std::setw(20) << std::fixed << std::setprecision(3) << d.T;
  strm << std::right << std::setw(20) << std::fixed << std::setprecision(5) << d.P;
  strm << std::right << std::setw(20) << std::fixed << std::setprecision(5) << d.Ek;
  strm << std::right << std::setw(20) << std::fixed << std::setprecision(5) << d.Ep;
  strm << std::right << std::setw(20) << std::fixed << std::setprecision(5) << d.Ek + d.Ep;
  if (d.snapshot > 0u) strm << std::right << std::setw(20) << d.snapshot;
  else strm << std::right << std::setw(20) << '-';
  for (auto iae : d.Eia)
  {
    strm << std::right << std::setw(20) << std::fixed << std::setprecision(5) << iae;
  }
  strm << '\n';
  return strm;
}

void md::trace_writer::operator() (md::trace_data const & d)
{
  static std::atomic<std::size_t> x(0u);
  if (Config::get().general.verbosity > 4)
  {
    if (x % 100u == 0)
    {
      std::cout << std::right << std::setw(10u) << "It";
      std::cout << std::right << std::setw(20u) << "T";
      std::cout << std::right << std::setw(20u) << "P";
      std::cout << std::right << std::setw(20u) << "E_kin";
      std::cout << std::right << std::setw(20u) << "E_pot";
      std::cout << std::right << std::setw(20u) << "E_tot";
      std::cout << std::right << std::setw(20u) << "Snapsh.";
      if (d.Eia.size() > 1u)
      {
        for (auto i : scon::index_range(d.Eia))
        {
          std::cout << std::right << std::setw(20u) << "E_ia(# " << i << ')';
        }
      }
      std::cout << '\n';
    }
    std::cout << d;
    ++x;
  }
  *strm << d;
}

namespace
{
  inline std::size_t gap(std::size_t const st, std::size_t const sn)
  {
    if (sn == 0u) return 0u;
    return st / std::min(st, sn + 2u);
  }
}


md::Logger::Logger(coords::Coordinates &coords, std::size_t snap_offset) :
  snap_buffer(coords::make_buffered_cartesian_log(coords, "_MD_SNAP",
    Config::get().md.max_snap_buffer, snap_offset, Config::get().md.optimize_snapshots)),
  data_buffer(scon::offset_call_buffer<trace_data>(50u, Config::get().md.trace_offset, 
    trace_writer{ coords::output::filename("_MD_TRACE", ".txt").c_str() })),
  snapnum()
{
}

bool md::Logger::operator()(std::size_t const iter, coords::float_type const T,
  coords::float_type const P, coords::float_type const Ek, coords::float_type const Ep,
  std::vector<coords::float_type> const Eia, coords::Representation_3D const & x)
{
  return data_buffer(trace_data(Eia, T, Ek, Ep, P, iter, snap_buffer(x) ? ++snapnum : 0u));
}

// Serialization Helper Function

static inline void append_to_buffer(std::vector<char> & buffer, void const * data, std::size_t const bytes)
{
  if (bytes > 0 && data)
  {
    buffer.resize(buffer.size() + bytes);
    std::vector<char>::iterator ins_pos_it(buffer.end() - bytes);
    memcpy(&(*ins_pos_it), data, bytes);
  }
}

static inline std::istream::pos_type istream_size_left(std::istream & S)
{
  std::istream::pos_type const T(S.tellg());
  S.seekg(0, std::istream::end);
  std::istream::pos_type const R(S.tellg());
  S.seekg(T, std::istream::beg);
  return R;
}


// Traces 


md::simulation::simulation(coords::Coordinates &coord_object) :
  coordobj(coord_object), 
  logging(coord_object, gap(Config::get().md.num_steps, Config::get().md.num_snapShots)),
  P(coord_object.xyz()), P_old(coord_object.xyz()),
  F(coord_object.g_xyz()), F_old(coord_object.g_xyz()),
  V(coord_object.xyz().size()), M(coord_object.xyz().size()),
  M_total(0.0), E_kin(0.0), T(Config::get().md.T_init), temp(0.0), dt(Config::get().md.timeStep),
  freedom(0), snapGap(0), C_geo(), C_mass(),
  nht(), rattle_bonds(), window(), restarted(true)
{
  std::sort(Config::set().md.heat_steps.begin(), Config::set().md.heat_steps.end());
}





void md::simulation::run(bool const restart)
{
  // Print initial info if required due to verbosity
  if (Config::get().general.verbosity > 0U) print_init_info();
  // Get first energy & derivatives
  coordobj.g();
  restarted = restart;
  std::size_t iteration(0U);
  // (Re)initialize all values if simulation is to be fresh
  if (restarted)
  {
    nht = nose_hoover();
    T = Config::get().md.T_init;
    init();
    // use nvect3d function to eliminate translation and rotation
    //V.eliminate_trans_rot(coordobj.xyz(), M);
    // use built in md function (identical)
    tune_momentum();
  }

  // Read simulation data from resume file if required
  if (Config::get().md.resume)
  {
    if (Config::get().general.verbosity > 4) std::cout << "Resuming MD Simulation.\n";
    std::ifstream restart_stream(
      (Config::get().general.outputFilename + "_MD_restart.cbf").c_str(),
      std::ifstream::in | std::ifstream::binary
      );
    scon::binary_stream<std::istream> bs{ restart_stream };
    if (bs >> iteration) bs >> *this;
  }
  // start Simulation
  if (Config::get().general.verbosity > 29) std::cout << "Starting simulation at " << iteration << "\n";
  integrate(iteration);
}

void md::simulation::umbrella_run(bool const restart) {

  std::ofstream ofs;
  ofs.open("umbrella.txt", std::ofstream::out);
  std::size_t steps;
  steps = Config::get().md.num_steps;

  //General md config
  if (Config::get().general.verbosity > 0U)  print_init_info();
  coordobj.g();
  restarted = restart;
  if (restarted)
  {
    nht = nose_hoover();
    T = Config::get().md.T_init;
    init();
    // use nvect3d function to eliminate translation and rotation
    //V.eliminate_trans_rot(coordobj.xyz(), M);
    // use built in md function (identical)
    tune_momentum();
  }
  // Set kinetic Energy
  updateEkin();
  // general config end
  //run equilibration
  Config::set().md.num_steps = Config::get().md.usequil;
  integrate();
  // start Simulation
  udatacontainer.clear();
  // run production
  Config::set().md.num_steps = steps;
  integrate();
  //write output
  for (std::size_t i = 0; i < udatacontainer.size(); i++) {
    if (i% Config::get().md.usoffset == 0) {
      ofs << i << "   " << udatacontainer[i] << std::endl;
    }
  }
  ofs.close();
}


void md::simulation::integrate(std::size_t const k_init)
{
  switch (Config::get().md.integrator)
  {
    case config::md_conf::integrators::BEEMAN:
      { // Beeman integrator
        beemanintegrator(k_init);
        break;
      }
    default:
      { // Velocity verlet integrator
        //verletintegrator(k_init);
        velocity_verlet(k_init);
      }
  }
}


void md::simulation::print_init_info(void)
{
  double const psec(Config::get().md.timeStep*static_cast<double>(Config::get().md.num_steps));
  std::cout << "Molecular dynamics simulation, limited to " << Config::get().md.num_steps;
  std::cout << " steps (representing a time step of " << Config::get().md.timeStep;
  std::cout << " ps each) started, simulating ";
  if (psec >= 500000.0) std::cout << (psec / 1000000.0) << " microseconds." << lineend;
  else if (psec >= 500) std::cout << (psec / 1000.0) << " nanoseconds." << lineend;
  else std::cout << psec << " picoseconds." << lineend;
  if (Config::get().md.trace_offset > 1)
  {
    std::cout << "Traces will be written for every ";
    switch (Config::get().md.trace_offset)
    {
    case 2: std::cout << "2nd"; break;
    case 3: std::cout << "3rd"; break;
    default: std::cout << Config::get().md.trace_offset << "th";
    }
    std::cout << " step.\n";
  }
  if (!Config::get().md.heat_steps.empty())
  {
    std::cout << "Temperature changes: ";
    if (Config::get().md.heat_steps.empty() || Config::get().md.heat_steps.front().offset > 0U)
      std::cout << "[Step 0]:[" << Config::get().md.T_init << "K]";
    for (auto heatstep : Config::get().md.heat_steps)
    {
      std::cout << " -> [Step " << heatstep.offset << "]:[" << heatstep.raise << "K]";
    }
    std::cout << lineend;
  }
  std::cout << "Nose-Hoover thermostat is " << (Config::get().md.hooverHeatBath ? "active." : "inactive.") << lineend;
  if (Config::get().md.spherical.use)
  {
    std::cout << "Spherical boundaries will be applied." << lineend;
    std::cout << "Inner Sphere: (R: " << Config::get().md.spherical.r_inner << ", F: ";
    std::cout << Config::get().md.spherical.f1 << ", E: " << Config::get().md.spherical.e1 << ");";
    std::cout << "Outer Sphere: (R: " << Config::get().md.spherical.r_outer << ", F: ";
    std::cout << Config::get().md.spherical.f2 << ", E: " << Config::get().md.spherical.e2 << ")" << std::endl;
  }
  if (Config::get().md.rattle.use)
  {
    const std::size_t nr = Config::get().md.rattle.specified_rattle.size();
    if (Config::get().md.rattle.all) std::cout << "All covalent hydrogen bonds will be fixed" << lineend;
    else if (nr > 0)
    {
      std::cout << "The following covalent hydrogen bonds will be fixed: " << lineend;
      for (auto const & bond : Config::get().md.rattle.specified_rattle)
      {
        std::cout << "[" << bond.a << ":" << bond.b << "] ";
      }
      std::cout << std::endl;
    }
  }
  if (Config::get().md.resume)
  {
    std::cout << "The simulation state will be reverted to the state of the binary resume file '";
    std::cout << std::string(Config::get().general.outputFilename).append("_MD_restart.cbf") << "'." << std::endl;
  }
}

static inline double ldrand(void)
{
  return std::log(std::max(d_rand(), 1.0e-20));
}

void md::simulation::rattlesetup(void)
{
  config::md_conf::config_rattle::rattle_constraint_bond rctemp;
  //open rattle par file
  std::ifstream ifs;
  ifs.open(Config::get().md.rattle.ratpar.c_str());
  if (ifs.good()) std::cout << "Opened file!" << std::endl;
  if (!ifs.good()) {
    std::cout << "Couldn't open file for RATTLE parameters. Check your input" << std::endl;
    exit(1);
  }
  // temp vars;
  struct trat {
    std::size_t ia, ib;
    double ideal;
  };
  struct ratatoms {
    std::size_t ia, ga;
  };
  ratatoms RTA;
  trat TRAT;
  std::vector<trat> temprat;
  std::vector<ratatoms> ratoms;
  std::size_t tia = std::size_t(), tib = std::size_t();
  //double ideal;
  char buffer[150];
  std::string bufferc;
  std::size_t found;
  std::vector < std::string > tokens;
  while (!ifs.eof())
  {
    ifs.getline(buffer, 150);
    bufferc = buffer;
    found = bufferc.find("bond");
    if (found != std::string::npos)
    {
      tokens.clear();
      std::istringstream iss(bufferc);
      std::copy(std::istream_iterator <std::string>(iss), std::istream_iterator <std::string>(), std::back_inserter <std::vector <std::string >>(tokens));
      TRAT.ia = atoi(tokens[1].c_str());
      TRAT.ib = atoi(tokens[2].c_str());
      TRAT.ideal = atof(tokens[4].c_str());
      temprat.push_back(TRAT);
    }
  }// end of file check
  ifs.close();
  ifs.open(Config::get().md.rattle.ratpar.c_str());
  while (!ifs.eof())
  {
    ifs.getline(buffer, 150);
    bufferc = buffer;
    found = bufferc.find("atom");
    if (found != std::string::npos)
    {
      tokens.clear();
      std::istringstream iss(bufferc);
      std::copy(std::istream_iterator <std::string>(iss), std::istream_iterator <std::string>(), std::back_inserter <std::vector <std::string >>(tokens));
      RTA.ia = atoi(tokens[1].c_str());
      RTA.ga = atoi(tokens[2].c_str());
      ratoms.push_back(RTA);
    }
  }
  ifs.close();
  const std::size_t N = coordobj.size();
  //loop over all atoms
  for (std::size_t i = 0; i < N; i++) {
    //check if atom is hydrogen
    if (coordobj.atoms(i).number() == 1)
    {
      rctemp.a = i;
      rctemp.b = coordobj.atoms(i).bonds(0);
      //loop over param vector
      for (unsigned j = 0; j < ratoms.size(); j++)
      {
        if (ratoms[j].ia == coordobj.atoms(rctemp.a).energy_type()) tia = ratoms[j].ga;
        if (ratoms[j].ia == coordobj.atoms(rctemp.b).energy_type()) tib = ratoms[j].ga;
      }
      for (unsigned k = 0; k < temprat.size(); k++)
      {
        // found matching parameters -> get ideal bond distance
        if ((tia == temprat[k].ia || tia == temprat[k].ib) && (tib == temprat[k].ia || tib == temprat[k].ib))
        {
          rctemp.len = temprat[k].ideal;
        }
      }
      rattle_bonds.push_back(rctemp);
    }
  }
}


void md::simulation::init(void)
{
  using std::abs;
  std::size_t const N = coordobj.size();
  static double const twopi = 2.0*md::PI;
  double const twokbT = 2.0*md::kB*Config::get().md.T_init;
  M_total = 0;
  V.resize(N);
  F.resize(N);
  M.resize(N);
  if (Config::get().general.verbosity > 29U)
  {
    std::cout << "Initializing velocities at " << Config::get().md.T_init << "K.\n";
  }
  for (std::size_t i = 0; i<N; ++i)
  {
    // Get Atom Mass
    M[i] = coordobj.atoms(i).mass();
    // Sum total Mass
    M_total += M[i];
    // Set initial velocities zero if fixed
    if (coordobj.atoms(i).fixed() || !(abs(Config::get().md.T_init) > 0.0))
    {
      V[i] = coords::Cartesian_Point(0);
    }
    // initialize randomized otherwise
    else
    { //ratioRoot = sqrt ( 2*kB*T / m )
      double const ratio(twopi*std::sqrt(twokbT / M[i]));
      V[i].x() = (std::sqrt(std::fabs(-2.0*ldrand()))*std::cos(d_rand()*ratio));
      V[i].y() = (std::sqrt(std::fabs(-2.0*ldrand()))*std::cos(d_rand()*ratio));
      V[i].z() = (std::sqrt(std::fabs(-2.0*ldrand()))*std::cos(d_rand()*ratio));

      if (Config::get().general.verbosity > 149U) std::cout << "Initial Velocity of " << i << " is " << V[i] << " with rr: " << ratio << std::endl;
    }
    // sum position vectors for geometrical center
    C_geo += coordobj.xyz(i);
  }
  // Set up rattle vector for constraints
  if (Config::get().md.rattle.use == true && Config::get().md.rattle.all == true) rattlesetup();
  // get degrees of freedom
  freedom = 3U * N;
  // constraint degrees of freedom
  if (Config::get().md.rattle.use == true) freedom -= rattle_bonds.size();
  // periodics and isothermal cases
  if (Config::get().md.hooverHeatBath == true)
  {
    if (Config::get().energy.periodic == true) freedom -= 3;
    else freedom -= 6;
  }
  //if (Config::get().general.verbosity > 2U) std::cout << "Degrees of freedom: " << freedom << std::endl;


  // calc number of steps between snapshots (gap between snapshots)
  snapGap = gap(Config::get().md.num_steps, Config::get().md.num_snapShots);
  // scale geometrical center vector by number of atoms
  C_geo /= static_cast<double>(N);
  // call center of Mass method from coordinates object
  C_mass = coordobj.center_of_mass();



}

void md::simulation::fepinit(void)
{
  init();
  // center
  coordobj.move_all_by(-coordobj.center_of_geometry());
  double linear, dlin, linel, linvdw;
  double  increment, tempo, tempo2, diff;
  Config::set().md.fep = true;
  FEPsum = 0.0;
  // increment electrostatics
  linear = 1.0 - Config::get().fep.eleccouple;
  dlin = linear / Config::get().fep.dlambda;
  linel = 1.0 / dlin;
  // increment van der waals interactions
  linear = 1.0 - Config::get().fep.vdwcouple;
  dlin = linear / Config::get().fep.dlambda;
  linvdw = 1.0 / dlin;
  // fill vector with scaling increments
  increment = Config::get().fep.lambda / Config::get().fep.dlambda;
  std::cout << Config::get().fep.lambda << "   " << Config::get().fep.dlambda << std::endl;
  std::cout << "Number of FEP windows:  " << increment << std::endl;
  coordobj.fep.window.resize(std::size_t(increment));
  coordobj.fep.window[0].step = 0;
  // Electrostatics
  for (std::size_t i = 0; i< coordobj.fep.window.size(); i++) {

    tempo = i * Config::get().fep.dlambda;
    tempo2 = i *  Config::get().fep.dlambda + Config::get().fep.dlambda;
    if (tempo <= Config::get().fep.eleccouple)
    {
      coordobj.fep.window[i].ein = 0.0;
    }
    else
    {
      diff = std::abs(tempo - Config::get().fep.eleccouple);
      coordobj.fep.window[i].ein = (diff / Config::get().fep.dlambda) * linel;
      if (coordobj.fep.window[i].ein > 1.0) coordobj.fep.window[i].ein = 1.0;
    }
    if (tempo >= (1 - Config::get().fep.eleccouple))
    {
      coordobj.fep.window[i].eout = 0.0;
    }
    else {
      diff = tempo / Config::get().fep.dlambda;
      coordobj.fep.window[i].eout = 1.0 - (diff * linel);
    }
    if (tempo2 <= Config::get().fep.eleccouple) {
      coordobj.fep.window[i].dein = 0.0;
    }
    else {
      diff = std::abs(tempo2 - Config::get().fep.eleccouple);
      coordobj.fep.window[i].dein = (diff / Config::get().fep.dlambda) * linel;
      if (coordobj.fep.window[i].dein > 1.0) coordobj.fep.window[i].dein = 1.0;
    }
    if (tempo2 >= (1 - Config::get().fep.eleccouple)) {
      coordobj.fep.window[i].deout = 0.0;
    }
    else {
      diff = tempo2 / Config::get().fep.dlambda;
      coordobj.fep.window[i].deout = 1.0 - (diff * linel);
    }
    // van der Waals
    if (Config::get().fep.vdwcouple == 0) {
      coordobj.fep.window[i].vin = 1.0;
      coordobj.fep.window[i].dvin = 1.0;
      coordobj.fep.window[i].vout = 1.0;
      coordobj.fep.window[i].dvout = 1.0;
    }
    else if (Config::get().fep.vdwcouple == 1.0) {
      diff = tempo / Config::get().fep.dlambda;
      coordobj.fep.window[i].vout = 1.0 - (diff * Config::get().fep.dlambda);
      coordobj.fep.window[i].vin = diff * Config::get().fep.dlambda;
      diff = tempo2 / Config::get().fep.dlambda;;
      coordobj.fep.window[i].dvout = 1.0 - (diff * Config::get().fep.dlambda);
      coordobj.fep.window[i].dvin = diff * Config::get().fep.dlambda;
    }
    else {
      if (tempo <= (1 - Config::get().fep.vdwcouple)) {
        coordobj.fep.window[i].vout = 1.0;
        coordobj.fep.window[i].vin = 0.0;
      }
      else {
        diff = std::abs(tempo - Config::get().fep.vdwcouple);
        diff /= Config::get().fep.dlambda;
        coordobj.fep.window[i].vout = 1.0 - (diff * linvdw);
        coordobj.fep.window[i].vin = diff * linvdw;
      }
      if (tempo2 <= (1 - Config::get().fep.vdwcouple)) {
        coordobj.fep.window[i].dvout = 1.0;
        coordobj.fep.window[i].dvin = 0.0;
      }
      else {
        diff = std::abs(tempo2 - Config::get().fep.vdwcouple);
        diff /= Config::get().fep.dlambda;
        coordobj.fep.window[i].dvout = 1.0 - (diff * linvdw);
        coordobj.fep.window[i].dvin = diff * linvdw;
      }
    }
  }// end of loop

  std::ofstream fepclear;
  fepclear.open("alchemical.txt");
  fepclear.close();
  coordobj.fep.window[0].step = 0;
  std::cout << "FEP Coupling Parameters:" << std::endl;
  std::cout << std::endl;
  std::cout << "Van-der-Waals Coupling: " << std::endl;
  for (std::size_t i = 0; i < coordobj.fep.window.size(); i++)
  {
    std::cout << std::setw(8) << coordobj.fep.window[i].vout << std::setw(8) << coordobj.fep.window[i].dvout << std::setw(8) << coordobj.fep.window[i].vin << std::setw(8) << coordobj.fep.window[i].dvin << std::endl;
  }
  std::cout << std::endl;
  std::cout << "Electrostatic Coupling:" << std::endl;
  for (std::size_t i = 0; i < coordobj.fep.window.size(); i++)
  {
    std::cout << std::setw(8) << coordobj.fep.window[i].eout << std::setw(8) << coordobj.fep.window[i].deout << std::setw(8) << coordobj.fep.window[i].ein << std::setw(8) << coordobj.fep.window[i].dein << std::endl;
  }


  // Check for PME flag, if yes allocate neede memeory for the grids
  /*if (Config::get().energy.pme == true)
  {
    std::cout << "PME electrostatics for FEP calculation. Setting up memory" << std::endl;
    coordobj.getfepinfo();
    coordobj.pme.pmetemp.initingrid.Allocate(3, coordobj.pme.pmetemp.fepi.size());
    coordobj.pme.pmetemp.initoutgrid.Allocate(3, coordobj.pme.pmetemp.fepo.size());
    coordobj.pme.pmetemp.initallgrid.Allocate(3, coordobj.pme.pmetemp.fepa.size());
    coordobj.pme.pmetemp.parallelinpme.Allocate(coordobj.pme.pmetemp.fepi.size(), coordobj.pme.pmetemp.rgridtotal);
    coordobj.pme.pmetemp.paralleloutpme.Allocate(coordobj.pme.pmetemp.fepo.size(), coordobj.pme.pmetemp.rgridtotal);
    coordobj.pme.pmetemp.parallelallpme.Allocate(coordobj.pme.pmetemp.fepa.size(), coordobj.pme.pmetemp.rgridtotal);
  }*/




}
//Calculation of ensemble average and free energy change for each step
void md::simulation::freecalc()
{
  std::size_t iterator(0U), k(0U);
  double de_ensemble, temp_avg, boltz = 1.3806488E-23, avogad = 6.022E23, conv = 4184.0;
  for (std::size_t i = 0; i < coordobj.fep.fepdata.size(); i++) {
    iterator += 1;
    k = 0;
    de_ensemble = temp_avg = 0.0;
    coordobj.fep.fepdata[i].de_ens = exp(-1 / (boltz*coordobj.fep.fepdata[i].T)*conv*coordobj.fep.fepdata[i].dE / avogad);
    //std::cout << boltz << "   " << traces.T[i] << "   " << conv << "   " << coordobj.fep.fepdata[i].dE << std::endl;
    for (k = 0; k <= i; k++) {
      temp_avg += coordobj.fep.fepdata[k].T;
      de_ensemble += coordobj.fep.fepdata[k].de_ens;
    }
    de_ensemble = de_ensemble / iterator;
    temp_avg = temp_avg / iterator;
    coordobj.fep.fepdata[i].dG = -1 * std::log(de_ensemble)*temp_avg*boltz*avogad / conv;
  }// end of main loop
  this->FEPsum += coordobj.fep.fepdata[coordobj.fep.fepdata.size() - 1].dG;
}

// write the output for the equilibration steps
void md::simulation::freewrite(std::size_t i)
{

  std::ofstream fep("alchemical.txt", std::ios_base::app);
  std::ofstream res("FEP_Results.txt", std::ios_base::app);
  // standard forward output
  if (i*Config::get().fep.dlambda == 0 && this->prod == false && Config::get().fep.backward != 1)
  {
    res << std::fixed << std::right << std::setprecision(4) << std::setw(10) << "0" << std::setw(10) << "0" << std::endl;
  }
  // backward initial value for lambda = 1
  else if ((i * Config::get().fep.dlambda + Config::get().fep.dlambda) == 1 && this->prod == false && Config::get().fep.backward == 1)
  {
    res << std::fixed << std::right << std::setprecision(4) << std::setw(10) << "1" << std::setw(10) << "0" << std::endl;
  }
  if (this->prod == false) {
    fep << "Equilibration for Lambda =  " << i * Config::get().fep.dlambda << 
      "  and dLambda =  " << (i * Config::get().fep.dlambda) + Config::get().fep.dlambda << std::endl;
  }
  else if (this->prod == true) {
    fep << "Starting new data collection with values:  " << 
      i * Config::get().fep.dlambda << "   " << (i * Config::get().fep.dlambda) + Config::get().fep.dlambda << std::endl;
    //fep << (i * Config::get().fep.dlambda) + Config::get().fep.dlambda << "   ";
  }

  for (std::size_t k = 0; k < coordobj.fep.fepdata.size(); k++) {
    if (k%Config::get().fep.freq == 0) {
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].e_c_l1;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].e_c_l2;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].e_vdw_l1;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].e_vdw_l2;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].T;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].dE;
      fep << std::fixed << std::right << std::setprecision(4) << std::setw(15) << coordobj.fep.fepdata[k].dG;
      fep << std::endl;
    }
  }

  if (this->prod == true) {
    fep << "Free energy change for the current window:  ";
    fep << coordobj.fep.fepdata[coordobj.fep.fepdata.size() - 1].dG << std::endl;
    fep << "Total free energy change until current window:  " << FEPsum << std::endl;
    //fep <<  FEPsum << std::endl;
    fep << "End of collection. Increasing lambda value" << std::endl;
    if (Config::get().fep.backward != 1) {
      res << std::fixed << std::right << std::setprecision(4) << std::setw(10) << (i * Config::get().fep.dlambda) + Config::get().fep.dlambda << std::setw(10) << FEPsum << std::endl;
    }
    else res << std::fixed << std::right << std::setprecision(4) << std::setw(10) << (i * Config::get().fep.dlambda) << std::setw(10) << FEPsum << std::endl;
  }
}

void md::simulation::feprun()
{

  // Forward transformation
  if (Config::get().fep.backward == 0) {
    // main window loop
    for (std::size_t i(0U); i < coordobj.fep.window.size(); ++i)
    {
      std::cout << "Lambda:  " << i * Config::get().fep.dlambda << std::endl;
      coordobj.fep.window[0U].step = static_cast<int>(i);
      coordobj.fep.fepdata.clear();
      // equilibration run for window i

      Config::set().md.num_steps = Config::get().fep.equil;
      integrate();

      this->prod = false;
      freewrite(i);
      coordobj.fep.fepdata.clear();

      // production run for window i
      Config::set().md.num_steps = Config::get().fep.steps;
      integrate();
      this->prod = true;
      freecalc();
      freewrite(i);

    }// end of main window loop
  }
  // doing a backward transofrmation
  else if (Config::get().fep.backward == 1) {
    // main backward window loop
    for (std::size_t i = coordobj.fep.window.size() - 1; i < coordobj.fep.window.size(); --i)
    {

      std::cout << "Lambda:  " << i * Config::get().fep.dlambda << std::endl;
      coordobj.fep.window[0U].step = static_cast<int>(i);
      coordobj.fep.fepdata.clear();
      // equilibration run for window i<

      Config::set().md.num_steps = Config::get().fep.equil;
      integrate();

      this->prod = false;
      freewrite(i);
      coordobj.fep.fepdata.clear();

      // production run for window i
      Config::set().md.num_steps = Config::get().fep.steps;
      integrate();
      this->prod = true;
      freecalc();
      freewrite(i);
      if (i == 0) break;
    }// end of main window loop
  }
  else {
    throw std::runtime_error("Wrong value for FEPbackward (0 or 1). Check your input file");
  }

}

void md::simulation::tune_momentum(void)
{
  std::size_t const N = coordobj.size();
  coords::Cartesian_Point momentum_linear, momentum_angular, mass_vector, velocity_angular;
  // 3x3 Matrix (inertia tensor)
  scon::c3<scon::c3<coords::float_type>> InertiaTensor;
  //scon::vect3dmatrix<coords::float_type> InertiaTensor;
  // calculate system movement
  for (std::size_t i = 0; i < N; ++i)
  {
    mass_vector += coordobj.xyz(i)*M[i];
    momentum_linear += V[i] * M[i];
    momentum_angular += cross(coordobj.xyz(i), V[i])*M[i];
  }
  // scale by total mass
  momentum_linear /= M_total;
  mass_vector /= M_total;
  // MVxLM * M
  momentum_angular -= cross(mass_vector, momentum_linear)*M_total;
  // momentum of inertia
  double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
  for (std::size_t i = 0; i < N; ++i)
  {
    coords::Cartesian_Point r(coordobj.xyz(i) - mass_vector);
    xx += r.x()*r.x()*M[i];
    xy += r.x()*r.y()*M[i];
    xz += r.x()*r.z()*M[i];
    yy += r.y()*r.y()*M[i];
    yz += r.y()*r.z()*M[i];
    zz += r.z()*r.z()*M[i];
  }
  // Set up Inertia Tensor
  InertiaTensor.x() = scon::c3<coords::float_type>(yy + zz, -xy, -xz);
  InertiaTensor.y() = scon::c3<coords::float_type>(-xy, xx + zz, -yz);
  InertiaTensor.z() = scon::c3<coords::float_type>(-xz, -yz, xx + yy);
  // Invert inertia tensor
  scon::invert(InertiaTensor);
  //InertiaTensor.invert();
  // get angular velocity
  velocity_angular.x() = (dot(InertiaTensor.x(), momentum_angular));
  velocity_angular.y() = (dot(InertiaTensor.y(), momentum_angular));
  velocity_angular.z() = (dot(InertiaTensor.z(), momentum_angular));
  // remove angular and linear momentum
  for (std::size_t i = 0; i < N; ++i)
  {
    V[i] -= momentum_linear;
    coords::Cartesian_Point r(coordobj.xyz(i) - mass_vector);
    V[i] -= cross(velocity_angular, r);
  }
  if (Config::get().general.verbosity > 99U) std::cout << "Tuned momentum " << lineend;
}

void md::simulation::boundary_adjustments()
{
  if (Config::get().md.spherical.use)
  {
    if (Config::get().general.verbosity > 99U) std::cout << "Adjusting boundary conditions." << lineend;
    spherical_adjust();
  }
}

void md::simulation::spherical_adjust()
{
  std::size_t const N = coordobj.size();
  for (std::size_t i(0U); i < N; ++i)
  {
    // distance from geometrical center
    coords::Cartesian_Point r = coordobj.xyz(i) - C_geo;
    double const L = geometric_length(r);
    // if distance is greater than inner radius of spherical boundary -> adjust atom
    if (L > Config::get().md.spherical.r_inner)
    {
      // E = f1*d^e1 -> deriv: e1*f1*d^e1-1 [/L: normalization]
      coordobj.move_atom_by(i, r*(Config::get().md.spherical.e1 * 
        Config::get().md.spherical.f1 * 
        std::pow(L - Config::get().md.spherical.r_inner, 
          Config::get().md.spherical.e1 - 1) / L), true);
    }
  }
}

void md::simulation::updateEkin(void)
{
  using TenVal = coords::Tensor::value_type;
  TenVal z = {0.0,0.0,0.0};
  E_kin_tensor.fill(z);
  auto const N = V.size();
  for (std::size_t i = 0; i < N; ++i)
  {
    auto const fact = 0.5 * M[i] / convert;
    E_kin_tensor[0][0] += fact * V[i].x() * V[i].x();
    E_kin_tensor[1][0] += fact * V[i].x() * V[i].y();
    E_kin_tensor[2][0] += fact * V[i].x() * V[i].z();
    E_kin_tensor[0][1] += fact * V[i].y() * V[i].x();
    E_kin_tensor[1][1] += fact * V[i].y() * V[i].y();
    E_kin_tensor[2][1] += fact * V[i].y() * V[i].z();
    E_kin_tensor[0][2] += fact * V[i].z() * V[i].x();
    E_kin_tensor[1][2] += fact * V[i].z() * V[i].y();
    E_kin_tensor[2][2] += fact * V[i].z() * V[i].z();
  }
  E_kin = E_kin_tensor[0][0] + E_kin_tensor[1][1] + E_kin_tensor[2][2];
  if (Config::get().general.verbosity > 149U)
  {
    std::cout << "Updating kinetic Energy from " << E_kin_tensor[0][0] << ", "
      << E_kin_tensor[1][1] << ", " << E_kin_tensor[2][2] << " to " << E_kin << lineend;
  }
}

void md::simulation::berendsen(double const & time)
{

  const std::size_t N = coordobj.size();
  double fac, scale, volume;
  double ptensor[3][3], aniso[3][3], anisobox[3][3], dimtemp[3][3];
  //Pressure calculation (only for isotropic boxes!)
  volume = Config::get().energy.pb_box.x() *  Config::get().energy.pb_box.y() *  Config::get().energy.pb_box.z();
  fac = presc / volume;
  // ISOTROPIC BOX
  if (Config::get().energy.isotropic == true) {
    ptensor[0][0] = fac * (2.0 * E_kin_tensor[0][0] - coordobj.virial()[0][0]);
    ptensor[1][1] = fac * (2.0 * E_kin_tensor[1][1] - coordobj.virial()[1][1]);
    ptensor[2][2] = fac * (2.0 * E_kin_tensor[2][2] - coordobj.virial()[2][2]);
    press = (ptensor[0][0] + ptensor[1][1] + ptensor[2][2]) / 3.0;
    // Berendsen scaling for isotpropic boxes
    scale = std::pow((1.0 + (time*Config::get().md.pcompress / Config::get().md.pdelay)*(press - Config::get().md.ptarget)), 0.3333333333333);
    // Adjust box dimensions
    Config::set().energy.pb_box.x() = Config::get().energy.pb_box.x() * scale;
    Config::set().energy.pb_box.y() = Config::get().energy.pb_box.y() * scale;
    Config::set().energy.pb_box.z() = Config::get().energy.pb_box.z() * scale;
    // scale atomic coordinates
    for (size_t i = 0; i < N; ++i)
    {
      coordobj.scale_atom_by(i, scale);
    }
  }
  // ANISOTROPIC BOX
  else {
    // get pressure tensor for anisotropic systems
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        ptensor[j][i] = fac * (2.0*E_kin_tensor[j][i] - coordobj.virial()[j][i]);
        //std::cout << "Ptensor:" << ptensor[j][i] << std::endl;
      }
    }
    // get isotropic pressure value
    press = (ptensor[0][0] + ptensor[1][1] + ptensor[2][2]) / 3.0;
    // Anisotropic scaling factors
    scale = time * Config::get().md.pcompress / (3.0*Config::get().md.pdelay);
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        if (j == i) aniso[j][i] = 1.0 + scale * (ptensor[i][i] - Config::get().md.ptarget);
        else aniso[j][i] = scale * ptensor[j][i];
      }
    }
    // modify anisotropic box dimensions (only for cube or octahedron)
    dimtemp[0][0] = Config::get().energy.pb_box.x();
    dimtemp[1][0] = 0.0;
    dimtemp[2][0] = 0.0;
    dimtemp[0][1] = Config::get().energy.pb_box.y() * 1.0;
    dimtemp[1][1] = Config::get().energy.pb_box.y() * 0.0;
    dimtemp[2][1] = 0.0;
    dimtemp[0][2] = Config::get().energy.pb_box.z() * 0.0;
    dimtemp[1][2] = Config::get().energy.pb_box.z() * (1.0 - 1.0 * 1.0) / 1.0;
    dimtemp[2][2] = Config::get().energy.pb_box.z() * sqrt(1.0 * 1.0 - 0.0 - 0.0);
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        anisobox[j][i] = 0.0;
        for (int k = 0; k < 3; k++) {
          anisobox[j][i] = anisobox[j][i] + aniso[j][k] * dimtemp[k][i];
        }
      }
    }
    // scale box dimensions for anisotropic case
    Config::set().energy.pb_box.x() = std::sqrt((anisobox[0][0] * anisobox[0][0] + anisobox[1][0] * anisobox[1][0] + anisobox[2][0] * anisobox[2][0]));
    Config::set().energy.pb_box.y() = std::sqrt((anisobox[0][1] * anisobox[0][1] + anisobox[1][1] * anisobox[1][1] + anisobox[2][1] * anisobox[2][1]));
    Config::set().energy.pb_box.z() = std::sqrt((anisobox[0][2] * anisobox[0][2] + anisobox[1][2] * anisobox[1][2] + anisobox[2][2] * anisobox[2][2]));
    // scale atomic coordinates for anisotropic case
    for (size_t i = 0; i < N; ++i)
    {
      coordobj.set_atom_aniso(i, aniso, true);
    }
  }//end of isotropic else
}

bool md::simulation::heat(std::size_t const step)
{
  config::md_conf::config_heat last;
  last.raise = Config::get().md.T_init;
  for (auto const & heatstep : Config::get().md.heat_steps)
  {
    if (heatstep.offset > step)
    {
      double const delta((heatstep.raise - last.raise) / static_cast<double>(heatstep.offset - last.offset));
      T += delta;
      return std::fabs(delta) > 0.0 ? true : false;
    }
    last = heatstep;
  }
  if (T < Config::get().md.T_final || T > Config::get().md.T_final)
  {
    double const delta((Config::get().md.T_final - last.raise) /
      static_cast<double>(Config::get().md.num_steps - last.offset));
    T += delta;
    return std::fabs(delta) > 0.0 ? true : false;
  }
  return false;
}

void md::simulation::nose_hoover_thermostat(void)
{
  double tempscale(0.0);
  std::size_t const N(coordobj.size());
  double const delt(Config::get().md.timeStep),
    d2(delt / 2.0), d4(d2 / 2.0), d8(d4 / 2.0),
    TR(T*md::R), fTR(freedom*TR);
  nht.G2 = (nht.Q1*nht.v1*nht.v1 - TR) / nht.Q2;
  nht.v2 += nht.G2*d4;
  nht.v1 *= exp(-nht.v2*d8);
  nht.G1 = (2.0*E_kin - fTR) / nht.Q1;
  nht.v1 += nht.G1*d4;
  nht.v1 *= exp(-nht.v2*d8);
  nht.x1 += nht.v1*d2;
  nht.x2 += nht.v2*d2;
  tempscale = exp(-nht.v1*d2);
  for (std::size_t i(0U); i < N; ++i) V[i] *= tempscale;
  E_kin *= tempscale*tempscale;
  if (Config::get().general.verbosity > 149U)
  {
    std::cout << "Nose-Hoover-Adjustment; Scaling factor: " << tempscale << lineend;
  }
  nht.v1 *= exp(-nht.v2*d8);
  nht.G1 = (2.0*E_kin - fTR) / nht.Q1;
  nht.v1 += nht.G1*d4;
  nht.v1 *= exp(-nht.v2*d8);
  nht.G2 = (nht.Q1*nht.v1*nht.v1 - TR) / nht.Q2;
  nht.v2 += nht.G2*d4;
}


void md::simulation::velocity_verlet(std::size_t k_init)
{

  scon::chrono::high_resolution_timer integration_timer;

  config::molecular_dynamics const & CONFIG(Config::get().md);

  // prepare tracking
  std::size_t const VERBOSE(Config::get().general.verbosity);

  std::size_t const N = this->coordobj.size();
  // coords::Cartesian_Point acceleration, velocity;
  double p_average(0.0);
  // constant values
  double const
    //velofactor(-0.5*dt*md::convert),
    dt_2(0.5*dt),
    tempfactor(2.0 / (freedom*md::R));


  if (VERBOSE > 0U)
  {
    std::cout << "Saving " << std::size_t(snapGap > 0 ? (CONFIG.num_steps - k_init) / snapGap : 0);
    std::cout << " snapshots (" << Config::get().md.num_snapShots << " in config)" << lineend;
  }
  // Main MD Loop
  auto split = std::max(std::size_t{ CONFIG.num_steps / 100u }, std::size_t{ 100u });
  for (std::size_t k(k_init); k < CONFIG.num_steps; ++k)
  {
    bool const HEATED(heat(k));
    if (VERBOSE < 4U && k % split == 0 && k > 1)
    {
      std::cout << k << " of " << CONFIG.num_steps << " steps completed" << '\n';
    }

    // apply half step temperature corrections
    if (CONFIG.hooverHeatBath)
    {
      updateEkin();
      nose_hoover_thermostat();
      temp = E_kin * tempfactor;
    }
    else if (HEATED)
    {
      updateEkin();
      temp = E_kin * tempfactor;
      double const factor(std::sqrt(T / temp));
      // scale velocities
      for (size_t i(0U); i < N; ++i) V[i] *= factor;
    }

    // save old coordinates
    P_old = coordobj.xyz();
    // Main simulation Progress Cycle
    for (std::size_t i(0U); i < N; ++i)
    {
      // calc acceleration
      coords::Cartesian_Point const acceleration(coordobj.g_xyz(i)*md::negconvert / M[i]);

      if (Config::get().general.verbosity > 149)
      {
        std::cout << "Move " << i << " by " << (V[i] * dt + (acceleration*dt)) 
          << " with g " << coordobj.g_xyz(i) << ", V: " << V[i] << std::endl;
      }

      // update veloctiy
      V[i] += acceleration*dt_2;
      // update coordinates
      coordobj.move_atom_by(i, V[i] * dt);
    }
    // first rattle if Hydrogens are fixed
    if (CONFIG.rattle.use) rattle_pre();

    // new energy & gradients
    coordobj.g();
    // Apply umbrella potential if umbrella sampling is used
    if (CONFIG.umbrella == true)
    {
      coordobj.ubias(udatacontainer);
      /* if (coordobj.potentials().dihedrals().size() != 0)
       {
         for (auto && dihedral : coordobj.potentials().dihedrals())
         {
           udatacontainer.push_back(dihedral.value.degrees());
         }
       }
       else if (coordobj.potentials().distances().size() != 0)
       {
         for (auto const & dist : coordobj.potentials().distances())
         {
           udatacontainer.push_back(dist.value);
         }
       }  */
    }
    // refine nonbondeds if refinement is required due to configuration
    if (CONFIG.refine_offset != 0 && (k + 1U) % CONFIG.refine_offset == 0)
    {
      if (VERBOSE > 29U) std::cout << "Refining structure/nonbondeds in step " << k << "." << lineend;
      coordobj.energy_update(true);
    }
    // If spherical boundaries are used apply boundary potential
    boundary_adjustments();
    // add new acceleration
    for (std::size_t i(0U); i < N; ++i)
    {
      coords::Cartesian_Point const acceleration(coordobj.g_xyz(i)*md::negconvert / M[i]);
      V[i] += acceleration*dt_2;
    }
    // Apply full step RATTLE constraints
    if (CONFIG.rattle.use) rattle_post();
    // Apply full step temperature adjustments
    if (CONFIG.hooverHeatBath)
    {
      updateEkin();
      nose_hoover_thermostat();
      updateEkin();
      temp = E_kin * tempfactor;
    }
    else if (HEATED)
    {
      updateEkin();
      temp = E_kin*tempfactor;
      for (std::size_t i(0U); i < N; ++i) V[i] *= std::sqrt(T / temp);
      updateEkin();
      temp = E_kin * tempfactor;
    }


    // Apply pressure adjustments
    if (CONFIG.pressure)
    {
      berendsen(dt);
    }
    // save temperature for FEP
    if (Config::get().md.fep)
    {
      coordobj.fep.fepdata.back().T = temp;
    }

    if (Config::get().md.veloScale) tune_momentum();

    // Logging / Traces

    if (CONFIG.track)
    {
      std::vector<coords::float_type> iae;
      if (coordobj.interactions().size() > 1)
      {
        iae.reserve(coordobj.interactions().size());
        for (auto const & ia : coordobj.interactions()) iae.push_back(ia.energy);
      }
      logging(k, temp, press, E_kin, coordobj.pes().energy, iae, coordobj.xyz());
    }

    // Serialize to binary file if required.
    if (k > 0 && Config::get().md.restart_offset > 0 && k % Config::get().md.restart_offset == 0)
    {
      write_restartfile(k);
    }

    // if veloscale is true, eliminate rotation and translation
    
    p_average += press;
  }
  p_average /= CONFIG.num_steps;
  if (VERBOSE > 2U)
  {
    std::cout << "Average pressure: " << p_average << std::endl;
    //auto integration_time = integration_timer();
    std::cout << "Velocity-Verlet integration took " << integration_timer << lineend;
  }
}

void md::simulation::verletintegrator(std::size_t const k_init)
{

  scon::chrono::high_resolution_timer integration_timer;

  config::molecular_dynamics const & CONFIG(Config::get().md);

  // prepare tracking
  std::size_t const VERBOSE(Config::get().general.verbosity);

  std::size_t const N = this->coordobj.size();
  // coords::Cartesian_Point acceleration, velocity;
  double p_average(0.0);
  // constant values
  double const
    //velofactor(-0.5*dt*md::convert),
    dt_2(0.5*dt),
    tempfactor(2.0 / (freedom*md::R));


  if (VERBOSE > 0U)
  {
    std::cout << "Saving " << std::size_t(snapGap > 0 ? (CONFIG.num_steps - k_init) / snapGap : 0);
    std::cout << " snapshots (" << Config::get().md.num_snapShots << " in config)" << lineend;
  }
  auto split = std::max(std::size_t{ CONFIG.num_steps / 100u }, std::size_t{ 100u });
  // Main MD Loop
  for (std::size_t k(k_init); k < CONFIG.num_steps; ++k)
  {
    bool const HEATED(heat(k));
    if (VERBOSE < 4U && k % split == 0 && k > 1)
    {
      std::cout << k << " steps completed" << std::endl;
    }

    // apply half step temperature corrections
    if (!CONFIG.hooverHeatBath)
    {
      updateEkin();
      temp = E_kin * tempfactor;
    }
    else if (CONFIG.hooverHeatBath && !HEATED)
    {
      updateEkin();
      nose_hoover_thermostat();
      temp = E_kin * tempfactor;
    }
    // Thermostat scaling factor update if heating is going on
    else if (CONFIG.hooverHeatBath && HEATED)
    {
      updateEkin();
      temp = E_kin * tempfactor;
      double const factor(std::sqrt(T / temp));
      // scale velocities
      for (size_t i(0U); i < N; ++i) V[i] *= factor;
    }
    // save old coordinates
    P_old = coordobj.xyz();
    // Main simulation Progress Cycle
    for (std::size_t i(0U); i < N; ++i)
    {
      // calc acceleration
      coords::Cartesian_Point const acceleration(coordobj.g_xyz(i)*md::negconvert / M[i]);

      if (Config::get().general.verbosity > 149)
      {
        std::cout << "Move " << i << " by " << (V[i] * dt + (acceleration*dt)) << " with g " << coordobj.g_xyz(i) << ", V: " << V[i] << std::endl;
      }

      // update veloctiy
      V[i] += acceleration*dt_2;
      // update coordinates
      coordobj.move_atom_by(i, V[i] * dt);
    }
    // first rattle if Hydrogens are fixed
    if (CONFIG.rattle.use) rattle_pre();

    // new energy & gradients
    coordobj.g();
    // Apply umbrella potential if umbrella sampling is used
    if (CONFIG.umbrella == true)
    {
      coordobj.ubias(udatacontainer);
      /* if (coordobj.potentials().dihedrals().size() != 0)
      {
      for (auto && dihedral : coordobj.potentials().dihedrals())
      {
      udatacontainer.push_back(dihedral.value.degrees());
      }
      }
      else if (coordobj.potentials().distances().size() != 0)
      {
      for (auto const & dist : coordobj.potentials().distances())
      {
      udatacontainer.push_back(dist.value);
      }
      }  */
    }
    // refine nonbondeds if refinement is required due to configuration
    if (CONFIG.refine_offset != 0 && (k + 1U) % CONFIG.refine_offset == 0)
    {
      if (VERBOSE > 99U) std::cout << "Refining structure/nonbondeds." << lineend;
      coordobj.energy_update(true);
    }
    // If spherical boundaries are used apply boundary potential
    boundary_adjustments();
    // add new acceleration
    for (std::size_t i(0U); i < N; ++i)
    {
      coords::Cartesian_Point const acceleration(coordobj.g_xyz(i)*md::negconvert / M[i]);
      V[i] += acceleration*dt_2;
    }
    // Apply full step RATTLE constraints
    if (CONFIG.rattle.use) rattle_post();
    // Apply full step temperature adjustments
    if (!CONFIG.hooverHeatBath)
    {
      updateEkin();
      temp = E_kin * tempfactor;
    }
    else if (CONFIG.hooverHeatBath && !HEATED)
    {
      updateEkin();
      nose_hoover_thermostat();
      updateEkin();
      temp = E_kin * tempfactor;
    }
    else if (HEATED)
    { // if final temperature has not been reached adjust
      updateEkin();
      temp = E_kin*tempfactor;
      if (CONFIG.hooverHeatBath)
      {
        for (std::size_t i(0U); i < N; ++i) V[i] *= std::sqrt(T / temp);
      }
      updateEkin();
      temp = E_kin * tempfactor;
    }

    // Apply pressure adjustments
    if (CONFIG.pressure)
    {
      berendsen(dt);
    }
    // save temperature for FEP
    if (Config::get().md.fep)
    {
      coordobj.fep.fepdata.back().T = temp;
    }

    if (Config::get().md.veloScale) tune_momentum();

    // Logging / Traces

    if (CONFIG.track)
    {
      std::vector<coords::float_type> iae;
      if (coordobj.interactions().size() > 1)
      {
        iae.reserve(coordobj.interactions().size());
        for (auto const & ia : coordobj.interactions()) iae.push_back(ia.energy);
      }
      logging(k, temp, press, E_kin, coordobj.pes().energy, iae, coordobj.xyz());
    }

    // Serialize to binary file if required.
    if (k > 0 && Config::get().md.restart_offset > 0 && k % Config::get().md.restart_offset == 0)
    {
      write_restartfile(k);
    }

    // if veloscale is true, eliminate rotation and translation

    p_average += press;
  }
  p_average /= CONFIG.num_steps;
  if (VERBOSE > 2U)
  {
    std::cout << "Average pressure: " << p_average << std::endl;
    //auto integration_time = integration_timer();
    std::cout << "Velocity-Verlet integration took " << integration_timer << lineend;
  }
}


void md::simulation::beemanintegrator(std::size_t const k_init)
{
  // const values

  config::molecular_dynamics const & CONFIG(Config::get().md);
  std::size_t const
    N = coordobj.size();
  double const
    deltt(Config::get().md.timeStep),
    d_8(deltt*0.125),
    five_d_8(d_8*5.0),
    mconvert(-md::convert),
    tempfactor(2.0 / (freedom*md::R));
  coords::Cartesian_Point force, temporary;
  std::ofstream ob_stream(
    std::string(std::string(Config::get().general.outputFilename).append("_MD_restart.cbf")).c_str(),
    std::ofstream::out | std::ofstream::binary
    );
  F_old.resize(N);

  for (std::size_t i(0U); i < N; ++i)
  {
    F[i] = (coordobj.g_xyz(i) / M[i])*mconvert;
    F_old[i] = F[i];
  }

  updateEkin();

  for (std::size_t k(k_init); k < Config::get().md.num_steps; ++k)
  {

    bool const heated(heat(k));

    if (Config::get().md.hooverHeatBath && !heated)
    {
      nose_hoover_thermostat();
    }

    // first beeman step
    for (std::size_t i(0U); i < N; ++i)
    {
      coords::Cartesian_Point const tmp((F[i] - F_old[i])*five_d_8);
      coordobj.move_atom_by(i, (V[i] + tmp)*deltt);
      V[i] += tmp;
    }

    // new energy & gradients
    coordobj.g();

    // refine nonbondeds if refinement is required due to configuration
    if (Config::get().md.refine_offset == 0 || (k + 1U) % Config::get().md.refine_offset == 0) coordobj.energy_update(true);

    // adjust boundary influence
    boundary_adjustments();

    // full step velocities, beeman recursion
    for (std::size_t i(0U); i < N; ++i)
    {
      F_old[i] = F[i];
      F[i] = (coordobj.g_xyz(i) / M[i])*mconvert;
      V[i] += (F[i] * 3.0 + F_old[i])*d_8;
    }

    if (Config::get().md.hooverHeatBath && !heated)
    {
      temp = E_kin*tempfactor;
      nose_hoover_thermostat();
    }
    else
    {
      updateEkin();
      temp = E_kin*tempfactor;
    }
    // trace Energy and Temperature
    if (CONFIG.track)
    {
      std::vector<coords::float_type> iae;
      if (coordobj.interactions().size() > 1)
      {
        iae.reserve(coordobj.interactions().size());
        for (auto const & ia : coordobj.interactions()) iae.push_back(ia.energy);
      }
      logging(k, temp, press, E_kin, coordobj.pes().energy, iae, coordobj.xyz());
    }
    // Serialize to binary file if required.
    if (Config::get().md.restart_offset > 0 && k%Config::get().md.restart_offset == 0)
    {
      write_restartfile(k);
    }
    // if veloscale is true, eliminate rotation and translation
    if (Config::get().md.veloScale) tune_momentum();

  }

}

void md::simulation::rattle_pre(void)
{
  std::size_t niter(0U);
  bool done = false;
  do
  {
    niter += 1;
    done = true;
    for (auto const & rattlebond : rattle_bonds)
    {
      // get bond distance
      coords::Cartesian_Point const d(coordobj.xyz(rattlebond.b) - coordobj.xyz(rattlebond.a));
      // difference of square to parameter length square
      double const delta = rattlebond.len*rattlebond.len - dot(d, d);
      // if delta is greater than tolerance -> rattle
      if (std::abs(delta) > Config::get().md.rattle.tolerance)
      {
        done = false;
        coords::Cartesian_Point const d_old(P_old[rattlebond.b] - P_old[rattlebond.a]);
        double inv_ma = 1.0 / M[rattlebond.a];
        double inv_mb = 1.0 / M[rattlebond.b];
        //calculate lagrange multiplier
        coords::Cartesian_Point rattle(1.25*delta / (2.0 * (inv_ma + inv_mb) * dot(d, d_old)));
        rattle *= d_old;
        // update half step positions
        coordobj.move_atom_by(rattlebond.a, -(rattle*inv_ma));
        coordobj.move_atom_by(rattlebond.b, rattle*inv_mb);
        inv_ma /= Config::get().md.timeStep;
        inv_mb /= Config::get().md.timeStep;
        // update half step velocities
        V[rattlebond.a] -= rattle*inv_ma;
        V[rattlebond.b] += rattle*inv_mb;
      }
    }
  } while (niter < Config::get().md.rattle.num_iter && done == false);
}

void md::simulation::rattle_post(void)
{
  // initialize some local variables
  std::size_t niter(0U);
  bool done = false;
  double tfact = 2.0 / (dt*1.0e-3*convert);
  double xvir, yvir, zvir;
  double vxx, vyx, vzx, vyy, vzy, vzz;
  std::array<std::array<double, 3>, 3> part_v;
  part_v[0][0] = part_v[0][1] = part_v[0][2] = part_v[1][0] = part_v[1][1] = part_v[1][2] = part_v[2][0] = part_v[2][1] = part_v[2][2] = 0.0;
  do
  {
    niter += 1;
    done = true;
    for (auto const & rattlebond : rattle_bonds)
    {
      // get bond distance
      coords::Cartesian_Point const d(coordobj.xyz(rattlebond.b) - coordobj.xyz(rattlebond.a));
      double inv_ma = 1.0 / M[rattlebond.a];
      double inv_mb = 1.0 / M[rattlebond.b];
      // calculate lagrange multiplier
      double const lagrange = -dot(V[rattlebond.b] - V[rattlebond.a], d) / (rattlebond.len*rattlebond.len*(inv_ma + inv_mb));
      if (std::fabs(lagrange) > Config::get().md.rattle.tolerance)
      {
        done = false;
        coords::Cartesian_Point rattle(d*lagrange*1.25);
        // update full step velocities
        V[rattlebond.a] -= rattle*inv_ma;
        V[rattlebond.b] += rattle*inv_mb;
        // compute RATTLE contributions to the internal virial tensor
        xvir = rattle.x() * tfact;
        yvir = rattle.y() * tfact;
        zvir = rattle.z() * tfact;
        vxx = d.x() * xvir;
        vyx = d.y() * xvir;
        vzx = d.z() * xvir;
        vyy = d.y() * yvir;
        vzy = d.z() * yvir;
        vzz = d.z() * zvir;
        part_v[0][0] -= vxx;
        part_v[1][0] -= vyx;
        part_v[2][0] -= vzx;
        part_v[0][1] -= vyx;
        part_v[1][1] -= vyy;
        part_v[2][1] -= vzy;
        part_v[0][2] -= vzx;
        part_v[1][2] -= vzy;
        part_v[2][2] -= vzz;
      }
    }
  } while (niter < Config::get().md.rattle.num_iter && done == false);
  // Add RATTLE contributions to the internal virial tensor
  coordobj.add_to_virial(part_v);
}

void md::simulation::write_restartfile(std::size_t const k)
{
  // Create binary buffer and copy data into vector char
  scon::binary_stream<std::vector<char>> buffer;
  buffer.v.reserve(scon::binary_size(k, *this));
  buffer << k << *this;
  // Create ofstream and insert buffer into stream
  std::ofstream restart_stream(
    (Config::get().general.outputFilename + "_MD_restart.cbf").c_str(),
    std::ofstream::out | std::ofstream::binary
    );
  restart_stream.write(buffer.v.data(), buffer.v.size());
}


//md::traces::traces(coords::Coordinates &coords, std::size_t const max_snap_buffer, std::size_t const) :
//  counter(0u), snap_counter(0U),
//  m_snap_buffer(max_snap_buffer, coords::Representation_3D(coords.size())), snapstream(),
//  tracestream(std::string(Config::get().general.outputFilename).append("_MD_LOG.txt").c_str(), Config::get().md.resume ? std::ios_base::app : std::ios_base::out),
//  m_snap_offset(0U), m_max_buffer(max_snap_buffer), cref(coords)
//{
//  if (Config::get().md.num_snapShots > 0)
//  {
//    snapstream.open(std::string(Config::get().general.outputFilename).append("_MD_SNAPS.xyz").c_str(), Config::get().md.resume ? std::ios_base::app : std::ios_base::out);
//    if (!snapstream) throw std::runtime_error("Failed to open MD snapshot stream.");
//  }
//  if (!tracestream) throw std::runtime_error("Failed to open MD trace stream.");
//  tracestream << std::right << std::setw(10) << "N";
//  tracestream << std::right << std::setw(20) << "T";
//  tracestream << std::right << std::setw(20) << "P";
//  tracestream << std::right << std::setw(20) << "E_kin";
//  tracestream << std::right << std::setw(20) << "E_int";
//  tracestream << std::right << std::setw(20) << "E_tot";
//  tracestream << std::right << std::setw(20) << "Snapshot";
//  std::size_t const N(cref.interactions().size());
//  if (N > 1)
//  {
//    for (std::size_t i(0U); i < N; ++i)
//    {
//      std::stringstream iass;
//      iass << "IA:" << i + 1;
//      tracestream << std::right << std::setw(20) << iass.str();
//    }
//  }
//  tracestream << std::endl;
//  if (Config::get().general.verbosity > 3U)
//  {
//    std::cout << "Snapshot buffer for " << max_snap_buffer << " structures created." << lineend;
//  }
//}
//
//void md::traces::add(std::size_t const iter, double const T, double const E_kin, double const E_pot,
//  std::vector<double> const ia, double const P,
//  coords::Representation_3D const * const snap_ptr)
//{
//  if (m_snap_offset + 1 == m_max_buffer) write(true);
//  else if (m_T.size() >= 50) write(false);
//  if (snap_ptr && snap_ptr->size() == cref.size())
//  {
//    if (Config::get().md.optimize_snapshots)
//    { // if optimization is required: opt snapshot
//      // Save current state
//      coords::Representation_3D const tmp(cref.xyz());
//      // plug snapshot into coords
//      cref.set_xyz(*snap_ptr);
//      // optimize snapshot
//      cref.o();
//      // copy snapshot to buffer
//      m_snap_buffer[m_snap_offset++] = cref.xyz();
//      // reset state
//      cref.set_xyz(tmp);
//    }
//    else // otherwise copy snapshot to buffer
//      m_snap_buffer[m_snap_offset++] = *snap_ptr;
//    m_snapshots.push_back(true);
//  }
//  else
//    m_snapshots.push_back(false);
//  // copy data to buffer
//  m_T.push_back(T);
//  m_Ek.push_back(E_kin);
//  m_Ep.push_back(E_pot);
//  m_P.push_back(P);
//  m_ia.push_back(ia);
//  // output if verbose
//  if (Config::get().general.verbosity > 3U)
//  {
//    std::cout << std::right << std::setw(10) << iter;
//    std::cout << std::fixed << std::right << std::setw(20) << std::setprecision(5) << T;
//    std::cout << std::fixed << std::right << std::setw(20) << std::setprecision(5) << P;
//    std::cout << std::fixed << std::right << std::setw(20) << std::setprecision(5) << E_kin;
//    std::cout << std::fixed << std::right << std::setw(20) << std::setprecision(5) << E_pot;
//    std::cout << std::fixed << std::right << std::setw(20) << std::setprecision(5) << E_pot + E_kin;
//    std::cout << lineend;
//  }
//}
//
//void md::traces::write(bool const snaps)
//{
//  std::size_t const N(m_T.size());
//  if (Config::get().general.verbosity > 2U)
//  {
//    std::cout << "Writing " << N << " tracelines and " << (snaps ? m_snap_offset : std::size_t(0U)) << " snapshots." << lineend;
//  }
//  for (std::size_t i(0U); i < N; ++i)
//  {
//    tracestream << std::right << std::setw(10) << ++counter;
//    tracestream << std::right << std::setw(20) << m_T[i];
//    tracestream << std::right << std::setw(20) << m_P[i];
//    tracestream << std::right << std::setw(20) << m_Ek[i];
//    tracestream << std::right << std::setw(20) << m_Ep[i];
//    tracestream << std::right << std::setw(20) << m_Ek[i] + m_Ep[i];
//    if (m_snapshots[i]) tracestream << std::right << std::setw(20) << ++snap_counter;
//    else tracestream << std::right << std::setw(20) << "-";
//    for (auto iae : m_ia[i]) tracestream << std::right << std::setw(20) << iae;
//    tracestream << "\n";
//  }
//  m_T.clear();
//  m_Ek.clear();
//  m_Ep.clear();
//  m_P.clear();
//  m_ia.clear();
//  m_snapshots.clear();
//  if (snaps)
//  {
//    coords::Representation_3D const xyz_tmp(cref.xyz());
//    for (std::size_t i(0U); i < m_snap_offset; ++i)
//    {
//      cref.set_xyz(m_snap_buffer[i]);
//      snapstream << cref;
//    }
//    cref.set_xyz(xyz_tmp);
//    m_snap_offset = 0U;
//  }
//}


//void md::nose_hoover::copy_to_buffer(std::vector<char> &buffer) const
//{
//  append_to_buffer(buffer, &v1, sizeof(double));
//  append_to_buffer(buffer, &v2, sizeof(double));
//  append_to_buffer(buffer, &x1, sizeof(double));
//  append_to_buffer(buffer, &x2, sizeof(double));
//  append_to_buffer(buffer, &Q1, sizeof(double));
//  append_to_buffer(buffer, &Q2, sizeof(double));
//  append_to_buffer(buffer, &G1, sizeof(double));
//  append_to_buffer(buffer, &G2, sizeof(double));
//}
//void md::nose_hoover::deserialize_from_stream(std::istream &ib_stream)
//{
//  ib_stream.read(reinterpret_cast<char*>(&v1), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&v2), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&G1), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&G2), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&Q1), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&Q2), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&x1), sizeof(double));
//  ib_stream.read(reinterpret_cast<char*>(&x2), sizeof(double));
//}

//std::size_t md::traces::bytesize(void) const
//{
//  // Number of snapshots, traces and interactions
//  std::size_t M(3 * sizeof(std::size_t));
//  // Vectors
//  M += 4 * m_T.size()*sizeof(double);
//  for (auto const & iaenergies : m_ia)
//  {
//    M += iaenergies.size()*sizeof(double);
//  }
//  // Snapshots
//  if (m_snap_offset > 0U)
//  {
//    M += m_snap_offset*m_snap_buffer.front().size()*sizeof(coords::Representation_3D::value_type);
//  }
//  if (Config::get().general.verbosity > 99U) std::cout << "Trace bytesize is " << M << " bytes." << lineend;
//  return M;
//}

//void md::traces::copy_to_buffer(std::vector<char> &buffer) const
//{
//  std::size_t const num_traces(m_T.size()), num_ia(cref.interactions().size() > 1 ? cref.interactions().size() : 0);
//  append_to_buffer(buffer, &m_snap_offset, sizeof(std::size_t));
//  append_to_buffer(buffer, &num_traces, sizeof(std::size_t));
//  append_to_buffer(buffer, &num_ia, sizeof(std::size_t));
//  append_to_buffer(buffer, m_T.data(), m_T.size()*sizeof(double));
//  append_to_buffer(buffer, m_Ek.data(), m_Ek.size()*sizeof(double));
//  append_to_buffer(buffer, m_Ep.data(), m_Ep.size()*sizeof(double));
//  append_to_buffer(buffer, m_P.data(), m_P.size()*sizeof(double));
//  for (auto const & iaenergies : m_ia)
//  {
//    if (iaenergies.size() > 0)
//      append_to_buffer(buffer, iaenergies.data(), num_ia*sizeof(double));
//  }
//  if (m_snap_offset > 0U)
//  {
//    for (std::size_t i(0U); i < m_snap_offset; ++i)
//    {
//      append_to_buffer(buffer, m_snap_buffer[i].data(), m_snap_buffer[i].size()*sizeof(coords::Representation_3D::value_type));
//    }
//  }
//}
//
//void md::traces::deserialize_from_stream(std::istream &ib_stream)
//{
//  std::size_t num_traces(0U), num_ia(0U);
//  // read sizes from stream
//  if (!ib_stream.read(reinterpret_cast<char*>(&m_snap_offset), sizeof(std::size_t)) ||
//    !ib_stream.read(reinterpret_cast<char*>(&num_traces), sizeof(std::size_t)) ||
//    !ib_stream.read(reinterpret_cast<char*>(&num_ia), sizeof(std::size_t))
//    )
//  {
//    throw std::runtime_error("Reading MD traces from binary stream failed.");
//  }
//  // Check whether we have enough space for snapshots from stream
//  if (m_snap_offset > m_max_buffer)
//    throw std::logic_error("Number of snapshots from the stream larger than buffer.");
//  // Check whether the system matches
//  if (num_ia != (cref.interactions().size() > 1 ? cref.interactions().size() : 0))
//    throw std::logic_error("Number of stream interaction energies does not match system.");
//  // check whether the stream has enough data left
//  std::size_t const REQ_BYTES(4 * num_traces*sizeof(double)
//    + num_traces*num_ia*sizeof(double)
//    + m_snap_offset*cref.size()*sizeof(coords::Representation_3D::value_type));
//  if (REQ_BYTES > std::size_t(istream_size_left(ib_stream)))
//    throw std::logic_error("Traces data missing.");
//  // resize and read vectors
//  m_T.resize(num_traces);
//  ib_stream.read(reinterpret_cast<char*>(m_T.data()), m_T.size()*sizeof(double));
//  m_Ek.resize(num_traces);
//  ib_stream.read(reinterpret_cast<char*>(m_Ek.data()), m_Ek.size()*sizeof(double));
//  m_Ep.resize(num_traces);
//  ib_stream.read(reinterpret_cast<char*>(m_Ep.data()), m_Ep.size()*sizeof(double));
//  m_P.resize(num_traces);
//  ib_stream.read(reinterpret_cast<char*>(m_P.data()), m_P.size()*sizeof(double));
//  m_ia.resize(num_traces);
//  for (auto & ia_element : m_ia)
//  {
//    ia_element.resize(num_ia);
//    ib_stream.read(reinterpret_cast<char*>(ia_element.data()), ia_element.size()*sizeof(double));
//  }
//  for (std::size_t i(0U); i < m_snap_offset && i < m_snap_buffer.size(); ++i)
//  {
//    m_snap_buffer[i].resize(cref.size());
//    ib_stream.read(reinterpret_cast<char*>(m_snap_buffer[i].data()), m_snap_buffer[i].size()*sizeof(coords::Representation_3D::value_type));
//  }
//
//}


//std::size_t md::simulation::data_bytesize(void) const
//{
//  // Number of vector elements
//  std::size_t const VS(coordobj.xyz().size());
//  // E_kin_tens bytes
//
//  std::size_t M(3 * 3 * sizeof(double));
//  // E_kin, T, temp, press, presstemp, v1, v2, G1, G2, Q1, Q2, x1, x2
//  sizeof(thermostat::nose_hoover);
//  M += 13 * sizeof(double);
//  // Vector size 
//  M += sizeof(std::size_t);
//  // coords
//  M += sizeof(coords::Representation_3D::value_type)*VS;
//  // P_old, F, F_old, V
//  M += 4U * sizeof(coords::Representation_3D::value_type)*VS;
//  M += trace.bytesize();
//  return M;
//}
//
//void md::simulation::serialize_to_stream(std::ostream &strm, std::size_t const & current_step) const
//{
//  if (strm)
//  {
//    if (Config::get().general.verbosity > 99U) std::cout << "Serializing to Stream." << lineend;
//    scon::chrono::high_resolution_timer buffer_timer, serialization_timer;
//    // return to the start of the stream
//    strm.seekp(0, std::ios_base::beg);
//    std::size_t const SIZES[5] = { coordobj.xyz().size(), P_old.size(), F.size(), F_old.size(), V.size() };
//    if (SIZES[0] != SIZES[1] || SIZES[0] != SIZES[2] || SIZES[0] != SIZES[2] ||
//      SIZES[0] != SIZES[3] || SIZES[0] != SIZES[4])
//    {
//      throw std::logic_error("MD serialization failed due to incorrect vector sizes.");
//    }
//    std::size_t const N_bytes(data_bytesize() + sizeof(std::size_t));
//    // Buffer 
//    std::vector<char> buffer;
//    // Reserve memory to avoid reallocation
//    buffer.reserve(N_bytes);
//    // Current Step
//    append_to_buffer(buffer, &current_step, sizeof(std::size_t));
//    // push data into the buffer
//    append_to_buffer(buffer, &E_kin_tensor, 3 * 3 * sizeof(double));
//    append_to_buffer(buffer, &E_kin, sizeof(double));
//    append_to_buffer(buffer, &T, sizeof(double));
//    append_to_buffer(buffer, &temp, sizeof(double));
//    append_to_buffer(buffer, &press, sizeof(double));
//    append_to_buffer(buffer, &presstemp, sizeof(double));
//    nht.copy_to_buffer(buffer);
//    append_to_buffer(buffer, &SIZES[0], sizeof(std::size_t));
//    append_to_buffer(buffer, coordobj.xyz().data(), sizeof(coords::Representation_3D::value_type)*SIZES[0]);
//    append_to_buffer(buffer, P_old.data(), sizeof(coords::Representation_3D::value_type)*SIZES[0]);
//    append_to_buffer(buffer, F.data(), sizeof(coords::Representation_3D::value_type)*SIZES[0]);
//    append_to_buffer(buffer, F_old.data(), sizeof(coords::Representation_3D::value_type)*SIZES[0]);
//    append_to_buffer(buffer, V.data(), sizeof(coords::Representation_3D::value_type)*SIZES[0]);
//    // copy traces to buffer
//    trace.copy_to_buffer(buffer);
//    auto bt = buffer_timer();
//    // write buffer to stream at once
//    strm.write(buffer.data(), buffer.size());
//    strm.flush();
//    if (Config::get().general.verbosity > 99U)
//    {
//      std::cout << "Serialization and write took " << serialization_timer
//        << "; (Buffering: " << scon::chrono::to_seconds<double>(bt) << ")." << lineend;
//    }
//  }
//  else
//    throw std::runtime_error("Serialization stream bad.");
//}
//
//
//std::size_t md::simulation::deserialize_from_stream(std::istream &ib_stream)
//{
//  std::size_t step(0);
//  if (ib_stream)
//  {
//    scon::chrono::high_resolution_timer deserialization_timer;
//    std::size_t const LEN(data_bytesize() + sizeof(std::size_t));
//    if (static_cast<std::size_t>(istream_size_left(ib_stream)) < LEN) throw std::runtime_error(std::string("Size of restartfile too small.").c_str());
//    ib_stream.read(reinterpret_cast<char*>(&step), sizeof(std::size_t));
//    ib_stream.read(reinterpret_cast<char*>(&E_kin_tensor), 3 * 3 * sizeof(double));
//    ib_stream.read(reinterpret_cast<char*>(&E_kin), sizeof(double));
//    ib_stream.read(reinterpret_cast<char*>(&T), sizeof(double));
//    ib_stream.read(reinterpret_cast<char*>(&temp), sizeof(double));
//    ib_stream.read(reinterpret_cast<char*>(&press), sizeof(double));
//    ib_stream.read(reinterpret_cast<char*>(&presstemp), sizeof(double));
//    nht.deserialize_from_stream(ib_stream);
//    std::size_t v_size;
//    ib_stream.read(reinterpret_cast<char*>(&v_size), sizeof(std::size_t));
//    if (coordobj.xyz().size() != v_size) throw std::runtime_error(std::string("Vector size in the restartfile does not match.").c_str());
//    coords::Representation_3D pos_buffer(v_size);
//    scon::read(ib_stream, pos_buffer);
//    scon::read(ib_stream, P_old);
//    scon::read(ib_stream, F);
//    scon::read(ib_stream, F_old);
//    scon::read(ib_stream, V);
//    trace.deserialize_from_stream(ib_stream);
//    coordobj.set_xyz(pos_buffer);
//    if (Config::get().general.verbosity > 99U)
//    {
//      std::cout << "Reading and deserializing data took " << scon::chrono::to_seconds<double>(deserialization_timer()) << "s." << lineend;
//    }
//  }
//  return step;
//}


//std::size_t md::simulation::read_restartfile(void)
//{
//  std::string const filename(std::string(Config::get().general.outputFilename).append("_MD_restart.cbf"));
//  std::ifstream ib_stream(filename.c_str(), std::ifstream::in | std::ifstream::binary);
//  if (!ib_stream) throw std::runtime_error(std::string("Failed to open restartfile '").append(filename).append("' for reading.").c_str());
//  return deserialize_from_stream(ib_stream);
//}



double md::barostat::berendsen::operator()(double const time, 
  coords::Representation_3D & p,
  coords::Tensor const & Ek_T, 
  coords::Tensor const & Vir_T, 
  coords::Cartesian_Point & box)
{

  double const volume = std::abs(box.x()) *  std::abs(box.y()) *  std::abs(box.z());
  
  double const fac = presc / volume;

  double press = 0.0;

  // ISOTROPIC BOX
  if (isotropic == true) 
  {
    press = fac * ( (2.0 * Ek_T[0][0] - Vir_T[0][0]) + 
                    (2.0 * Ek_T[1][1] - Vir_T[1][1]) + 
                    (2.0 * Ek_T[2][2] - Vir_T[2][2])   ) / 3.0;
    // Berendsen scaling for isotpropic boxes
    double const scale = std::cbrt(1.0 + (time * compress / delay) * (press - target));
    // Adjust box dimensions
    box *= scale;
    // scale atomic coordinates
    p *= scale;
  }
  // ANISOTROPIC BOX
  else 
  {
    coords::Tensor ptensor, aniso, anisobox, dimtemp;
    // get pressure tensor for anisotropic systems
    for (int i = 0; i < 3; i++) 
    {
      for (int j = 0; j < 3; j++) 
      {
        ptensor[j][i] = fac * (2.0*Ek_T[j][i] - Vir_T[j][i]);
        //std::cout << "Ptensor:" << ptensor[j][i] << std::endl;
      }
    }
    // get isotropic pressure value
    press = (ptensor[0][0] + ptensor[1][1] + ptensor[2][2]) / 3.0;
    // Anisotropic scaling factors
    double const scale = time * compress / (3.0 * delay);
    for (int i = 0; i < 3; i++) 
    {
      for (int j = 0; j < 3; j++) 
      {
        if (j == i)
        {
          aniso[i][i] = 1.0 + scale * (ptensor[i][i] - target);
        }
        else
        {
          aniso[j][i] = scale * ptensor[j][i];
        }
      }
    }
    // modify anisotropic box dimensions (only for cube or octahedron)
    dimtemp[0][0] = box.x();
    dimtemp[1][0] = 0.0;
    dimtemp[2][0] = 0.0;
    dimtemp[0][1] = box.y();
    dimtemp[1][1] = 0.0;
    dimtemp[2][1] = 0.0;
    dimtemp[0][2] = 0.0;
    dimtemp[1][2] = 0.0;
    dimtemp[2][2] = box.z();

    for (int i = 0; i < 3; i++) 
    {
      for (int j = 0; j < 3; j++) 
      {
        anisobox[j][i] = 0.0;
        for (int k = 0; k < 3; k++) 
        {
          anisobox[j][i] = anisobox[j][i] + aniso[j][k] * dimtemp[k][i];
        }
      }
    }

    // scale box dimensions for anisotropic case

    box.x() = std::sqrt((anisobox[0][0] * anisobox[0][0] + 
      anisobox[1][0] * anisobox[1][0] + 
      anisobox[2][0] * anisobox[2][0]));

    box.y() = std::sqrt((anisobox[0][1] * anisobox[0][1] + 
      anisobox[1][1] * anisobox[1][1] + 
      anisobox[2][1] * anisobox[2][1]));

    box.z() = std::sqrt((anisobox[0][2] * anisobox[0][2] + 
      anisobox[1][2] * anisobox[1][2] + 
      anisobox[2][2] * anisobox[2][2]));

    // scale atomic coordinates for anisotropic case
    for (auto & pos : p)
    {
      auto px = aniso[0][0] * pos.x() + aniso[0][1] * pos.y() + aniso[0][2] * pos.z();
      //pos.x() = px;
      auto py = aniso[1][0] * pos.x() + aniso[1][1] * pos.y() + aniso[1][2] * pos.z();
      //pos.y() = py;
      auto pz = aniso[2][0] * pos.x() + aniso[2][1] * pos.y() + aniso[2][2] * pos.z();
      pos.x() = px;
      pos.y() = py;
      pos.z() = pz;
    }

  }//end of isotropic else
  return press;
}
